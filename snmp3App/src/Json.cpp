#include "Json.h"
#include "Text.h"

#include <memory>
#include <stdexcept>
#include <yajl_parse.h>

namespace snmp3 {
namespace {
const size_t MaxDepth = 32;
const size_t MaxInput = 1024 * 1024;
const size_t MaxElements = 4096;


bool validUnicodeEscapes(const std::string& input)
{
    size_t position = 0;
    auto codeUnit = [&]() -> int {
        if (input.size() - position < 4) return -1;
        int value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const char c = input[position++];
            const int digit = c >= '0' && c <= '9' ? c - '0' :
                c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) return -1;
            value = value * 16 + digit;
        }
        return value;
    };
    while (position < input.size()) {
        if (input[position++] != '\\') continue;
        if (position == input.size()) return false;
        if (input[position++] != 'u') continue;
        const int first = codeUnit();
        if (first < 0 || (first >= 0xdc00 && first <= 0xdfff)) return false;
        if (first < 0xd800 || first > 0xdbff) continue;
        if (input.size() - position < 6 || input[position] != '\\' || input[position + 1] != 'u')
            return false;
        position += 2;
        const int second = codeUnit();
        if (second < 0xdc00 || second > 0xdfff) return false;
    }
    return true;
}

struct Parser {
    Json root;
    bool hasRoot = false;
    struct Frame { Json* node; std::string key; bool hasKey = false; };
    std::vector<Frame> stack;

    Json* append(Json value)
    {
        if (stack.empty()) {
            if (hasRoot) throw std::runtime_error("JSON rejected");
            hasRoot = true;
            root = std::move(value);
            return &root;
        }
        Frame& frame = stack.back();
        if (frame.node->kind == Json::Kind::Array) {
            if (frame.node->array.size() >= MaxElements)
                throw std::runtime_error("JSON limit exceeded");
            frame.node->array.push_back(std::move(value));
            return &frame.node->array.back();
        }
        if (!frame.hasKey || frame.node->object.size() >= MaxElements)
            throw std::runtime_error("JSON rejected");
        auto result = frame.node->object.emplace(frame.key, std::move(value));
        frame.hasKey = false;
        if (!result.second) throw std::runtime_error("duplicate JSON key");
        return &result.first->second;
    }
};

template<class F> int guarded(F operation) noexcept
{
    try { operation(); return 1; } catch (...) { return 0; }
}
int scalar(void* context, Json value)
{
    return guarded([&] { static_cast<Parser*>(context)->append(std::move(value)); });
}
int nullValue(void* context) { return scalar(context, Json()); }
int booleanValue(void* context, int value)
{
    Json node; node.kind = Json::Kind::Boolean; node.boolean = value != 0;
    return scalar(context, std::move(node));
}
int numberValue(void* context, const char* bytes, size_t size)
{
    return guarded([&] {
        Json node; node.kind = Json::Kind::Number; node.text.assign(bytes, size);
        static_cast<Parser*>(context)->append(std::move(node));
    });
}
int stringValue(void* context, const unsigned char* bytes, size_t size)
{
    return guarded([&] {
        if (!validUtf8(bytes, size)) throw std::runtime_error("JSON rejected");
        Json node; node.kind = Json::Kind::String;
        node.text.assign(reinterpret_cast<const char*>(bytes), size);
        static_cast<Parser*>(context)->append(std::move(node));
    });
}
int start(void* context, Json::Kind kind)
{
    return guarded([&] {
        Parser& parser = *static_cast<Parser*>(context);
        if (parser.stack.size() >= MaxDepth) throw std::runtime_error("JSON depth exceeded");
        Json node; node.kind = kind;
        Json* owned = parser.append(std::move(node));
        Parser::Frame frame; frame.node = owned;
        parser.stack.push_back(std::move(frame));
    });
}
int startMap(void* context) { return start(context, Json::Kind::Object); }
int startArray(void* context) { return start(context, Json::Kind::Array); }
int mapKey(void* context, const unsigned char* bytes, size_t size)
{
    return guarded([&] {
        if (!validUtf8(bytes, size)) throw std::runtime_error("JSON rejected");
        Parser::Frame& frame = static_cast<Parser*>(context)->stack.back();
        frame.key.assign(reinterpret_cast<const char*>(bytes), size);
        frame.hasKey = true;
        if (frame.node->object.count(frame.key)) throw std::runtime_error("duplicate JSON key");
    });
}
int end(void* context)
{
    return guarded([&] { static_cast<Parser*>(context)->stack.pop_back(); });
}
}

Json Json::parse(const std::string& input)
{
    if (input.size() > MaxInput) throw std::runtime_error("JSON size exceeded");
    if (!validUtf8(reinterpret_cast<const unsigned char*>(input.data()), input.size()) ||
        !validUnicodeEscapes(input))
        throw std::runtime_error("JSON rejected");
    Parser parser;
    const yajl_callbacks callbacks = {
        nullValue, booleanValue, NULL, NULL, numberValue, stringValue,
        startMap, mapKey, end, startArray, end
    };
    std::unique_ptr<yajl_handle_t, decltype(&yajl_free)> handle(
        yajl_alloc(&callbacks, NULL, &parser), yajl_free);
    if (!handle) throw std::runtime_error("JSON allocation failed");
    // The selected Base enables JSON5 at allocation; both language flags are explicit.
    if (!yajl_config(handle.get(), yajl_allow_json5, 0) ||
        !yajl_config(handle.get(), yajl_allow_comments, 0) ||
        !yajl_config(handle.get(), yajl_dont_validate_strings, 0))
        throw std::runtime_error("JSON configuration failed");
    if (yajl_parse(handle.get(), reinterpret_cast<const unsigned char*>(input.data()),
                   input.size()) != yajl_status_ok ||
        yajl_complete_parse(handle.get()) != yajl_status_ok || !parser.hasRoot)
        throw std::runtime_error("JSON rejected");
    return std::move(parser.root);
}
}
