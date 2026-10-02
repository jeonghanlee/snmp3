#include "Conversion.h"
#include "Request.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace snmp3 {
namespace {
void require(bool valid)
{ if (!valid) throw std::runtime_error("record conversion rejected"); }

uint64_t magnitude(int64_t value)
{ return value < 0 ? uint64_t(-(value + 1)) + 1 : uint64_t(value); }

bool exactInteger(uint64_t value, unsigned digits)
{
    unsigned bits = 0;
    for (uint64_t remaining = value; remaining; remaining >>= 1) ++bits;
    return bits <= digits || (value & ((uint64_t(1) << (bits - digits)) - 1)) == 0;
}

uint64_t unsignedValue(const Value& value)
{
    if (value.type() == ValueType::Counter64) return value.counter64();
    if (value.type() != ValueType::Integer && integerType(value.type())) return value.unsigned32();
    if (value.type() == ValueType::Integer) {
        require(value.integer() >= 0);
        return uint64_t(value.integer());
    }
    const double number = value.type() == ValueType::OpaqueFloat ? value.opaqueFloat() : value.opaqueDouble();
    require(std::isfinite(number) && number >= 0 && number < std::ldexp(1.0, 64) && std::trunc(number) == number);
    return static_cast<uint64_t>(number);
}

int64_t signedValue(const Value& value)
{
    if (value.type() == ValueType::Integer) return value.integer();
    if (integerType(value.type())) {
        const uint64_t number = unsignedValue(value);
        require(number <= uint64_t(INT64_MAX));
        return static_cast<int64_t>(number);
    }
    const double number = value.type() == ValueType::OpaqueFloat ? value.opaqueFloat() : value.opaqueDouble();
    require(std::isfinite(number) && number >= -std::ldexp(1.0, 63) &&
            number < std::ldexp(1.0, 63) && std::trunc(number) == number);
    return static_cast<int64_t>(number);
}

double floatingValue(const Value& value, unsigned digits)
{
    if (value.type() == ValueType::Integer) {
        require(exactInteger(magnitude(value.integer()), digits));
        return static_cast<double>(value.integer());
    }
    if (integerType(value.type())) {
        const auto number = unsignedValue(value);
        require(exactInteger(number, digits));
        return static_cast<double>(number);
    }
    const double number = value.type() == ValueType::OpaqueFloat ? value.opaqueFloat() : value.opaqueDouble();
    require(std::isfinite(number));
    return number;
}

uint64_t roundedShift(uint64_t significand, unsigned shift)
{
    if (shift > 63) return 0;
    if (!shift) return significand;
    const uint64_t quotient = significand >> shift;
    const uint64_t remainder = significand & ((uint64_t(1) << shift) - 1);
    const uint64_t halfway = uint64_t(1) << (shift - 1);
    return quotient + (remainder > halfway || (remainder == halfway && (quotient & 1)));
}
}

bool integerType(ValueType type)
{ return type >= ValueType::Integer && type <= ValueType::Counter64; }
bool numericType(ValueType type)
{ return integerType(type) || type == ValueType::OpaqueFloat || type == ValueType::OpaqueDouble; }
bool textualType(ValueType type)
{ return type == ValueType::Octets || type == ValueType::ObjectId || type == ValueType::IpAddress; }

float roundBinary32(double value)
{
    static_assert(sizeof(double) == 8 && sizeof(float) == 4 &&
                  std::numeric_limits<double>::is_iec559 && std::numeric_limits<float>::is_iec559,
                  "IEEE binary64 and binary32 required");
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    const unsigned exponent = unsigned((bits >> 52) & 0x7ff);
    require(exponent != 0x7ff);
    const uint32_t sign = uint32_t(bits >> 63) << 31;
    const uint64_t mantissa = (bits & ((uint64_t(1) << 52) - 1)) |
                              (exponent ? uint64_t(1) << 52 : 0);
    int power = exponent ? int(exponent) - 1023 : -1022;
    uint32_t result = sign;
    if (power >= -126) {
        uint64_t rounded = roundedShift(mantissa, 29);
        if (rounded == (uint64_t(1) << 24)) { rounded >>= 1; ++power; }
        require(power <= 127);
        result |= uint32_t(power + 127) << 23;
        result |= uint32_t(rounded) & 0x7fffff;
    } else {
        // The rounded subnormal may become the smallest normal value.
        result |= uint32_t(roundedShift(mantissa, unsigned(-power - 97)));
    }
    float converted;
    std::memcpy(&converted, &result, sizeof(converted));
    return converted;
}

Value convertNumeric(const Value& value, ValueType target, bool permitRounding)
{
    require(numericType(value.type()) && numericType(target));
    if (target == ValueType::Integer) {
        const auto number = signedValue(value);
        require(number >= INT32_MIN && number <= INT32_MAX);
        return Value::integer(number);
    }
    if (target == ValueType::Counter64) return Value::counter64(unsignedValue(value));
    if (integerType(target)) {
        const auto number = unsignedValue(value);
        require(number <= UINT32_MAX);
        return Value::unsigned32(target, uint32_t(number));
    }
    if (target == ValueType::OpaqueDouble) return Value::opaqueDouble(floatingValue(value, 53));
    // Integer precision is checked before any intermediate floating representation.
    const double number = floatingValue(value, permitRounding ? 53 : 24);
    const float rounded = roundBinary32(number);
    if (!permitRounding) require(static_cast<double>(rounded) == number);
    return Value::opaqueFloat(rounded);
}

Value convertScalar(const Value& value, ScalarType target)
{
    require(numericType(value.type()));
    switch (target) {
    case ScalarType::Signed32: return convertNumeric(value, ValueType::Integer);
    case ScalarType::Unsigned32: return convertNumeric(value, ValueType::Unsigned32);
    case ScalarType::Signed64: return Value::integer(signedValue(value));
    case ScalarType::Unsigned64: return Value::counter64(unsignedValue(value));
    case ScalarType::Float32: return convertNumeric(value, ValueType::OpaqueFloat);
    case ScalarType::Float64: return convertNumeric(value, ValueType::OpaqueDouble);
    }
    throw std::runtime_error("record scalar type rejected");
}

std::string convertText(const Value& value, size_t dataCapacity)
{
    require(textualType(value.type()));
    std::string result;
    if (value.type() == ValueType::Octets) {
        const auto& bytes = value.octets();
        require(bytes.size() <= dataCapacity);
        for (const auto byte : bytes) require(byte != 0);
        if (!bytes.empty()) result.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    } else {
        auto append = [&](uint32_t part) {
            const std::string text = (result.empty() ? "" : ".") + std::to_string(part);
            require(result.size() <= dataCapacity && text.size() <= dataCapacity - result.size());
            result += text;
        };
        if (value.type() == ValueType::ObjectId) for (auto part : value.objectId()) append(part);
        else for (auto part : value.ipAddress()) append(part);
    }
    return result;
}

Value captureText(const char* buffer, size_t storageBytes, size_t bindingCapacity, size_t lengthIncludingNul)
{
    require(buffer && storageBytes);
    const auto end = static_cast<const char*>(std::memchr(buffer, 0, storageBytes));
    require(end != nullptr);
    const size_t length = size_t(end - buffer);
    require(length <= bindingCapacity && (!lengthIncludingNul || lengthIncludingNul == length + 1));
    return Value::octets(std::vector<uint8_t>(buffer, buffer + length));
}

RecordLink parseRecordLink(const std::string& text)
{
    require(!text.empty() && text[0] == '@');
    std::istringstream input(text.substr(1));
    RecordLink link;
    bool haveDefinition = false, haveBudget = false;
    std::string token;
    while (input >> token) {
        const auto equal = token.find('=');
        require(equal != std::string::npos && equal + 1 < token.size());
        const auto key = token.substr(0, equal), value = token.substr(equal + 1);
        if (key == "binding") {
            require(!haveDefinition && value.size() <= 64);
            for (const unsigned char byte : value)
                require((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
                        (byte >= '0' && byte <= '9') || byte == '_' || byte == '-' || byte == '.');
            link.definition = value;
            haveDefinition = true;
        } else if (key == "deadline_ms") {
            require(!haveBudget);
            unsigned budget = 0;
            for (const unsigned char byte : value) {
                require(byte >= '0' && byte <= '9' && budget <= 600000 / 10);
                budget = budget * 10 + unsigned(byte - '0');
                require(budget <= 600000);
            }
            require(budget != 0);
            link.budgetMs = budget;
            haveBudget = true;
        } else require(false);
    }
    require(haveDefinition && haveBudget);
    return link;
}
}
