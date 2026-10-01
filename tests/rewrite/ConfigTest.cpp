#include "Config.h"
#include "Json.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>

namespace {
unsigned checks = 0;
void expect(bool condition, const char* label)
{
    if (!condition) throw std::runtime_error(label);
    ++checks;
}
template<class F> bool rejects(F operation)
{
    try { operation(); return false; } catch (const std::exception&) { return true; }
}
void values()
{
    using namespace snmp3;
    expect(Value::integer(INT64_MIN).integer() == INT64_MIN, "signed minimum");
    expect(Value::integer(INT64_MAX).integer() == INT64_MAX, "signed maximum");
    for (ValueType tag : {ValueType::Unsigned32, ValueType::Counter32, ValueType::Gauge32, ValueType::TimeTicks}) {
        expect(Value::unsigned32(tag, UINT32_MAX).unsigned32() == UINT32_MAX, "unsigned maximum");
        expect(Value::unsigned32(tag, 0).type() == tag, "unsigned native tag");
    }
    expect(Value::counter64(UINT64_MAX).counter64() == UINT64_MAX, "counter64 maximum");
    expect(Value::counter64(0).counter64() == 0, "counter64 zero");
    std::vector<uint8_t> source = {1, 0, 255};
    Value bytes = Value::octets(source); source[0] = 9; source.clear();
    expect(bytes.octets() == std::vector<uint8_t>({1, 0, 255}), "binary owned lifetime");
    std::vector<uint32_t> arcs = {2, UINT32_MAX, 0};
    Value oid = Value::objectId(arcs); arcs.clear();
    expect(oid.objectId() == std::vector<uint32_t>({2, UINT32_MAX, 0}), "OID owned lifetime");
    expect(Value::ipAddress({{127, 0, 0, 1}}).ipAddress()[3] == 1, "IPv4 value");
    const float f = -0.0f; const double d = -0.0;
    const float ownedF = Value::opaqueFloat(f).opaqueFloat();
    const double ownedD = Value::opaqueDouble(d).opaqueDouble();
    expect(std::memcmp(&f, &ownedF, sizeof(f)) == 0, "float signed-zero bits");
    expect(std::memcmp(&d, &ownedD, sizeof(d)) == 0, "double signed-zero bits");
    expect(Value::opaqueFloat(1.0f).type() != Value::opaqueDouble(1.0).type(), "distinct floating tags");
    expect(std::isnan(Value::opaqueFloat(std::numeric_limits<float>::quiet_NaN()).opaqueFloat()), "float NaN retained");
    expect(std::isinf(Value::opaqueFloat(std::numeric_limits<float>::infinity()).opaqueFloat()), "float infinity retained");
    expect(std::isnan(Value::opaqueDouble(std::numeric_limits<double>::quiet_NaN()).opaqueDouble()), "double NaN retained");
    expect(std::isinf(Value::opaqueDouble(std::numeric_limits<double>::infinity()).opaqueDouble()), "double infinity retained");
    const uint32_t floatBits = 0x7fc12345;
    const uint64_t doubleBits = 0x7ff8123456789abcULL;
    float payloadFloat; double payloadDouble;
    std::memcpy(&payloadFloat, &floatBits, sizeof(payloadFloat));
    std::memcpy(&payloadDouble, &doubleBits, sizeof(payloadDouble));
    const float actualFloat = Value::opaqueFloat(payloadFloat).opaqueFloat();
    const double actualDouble = Value::opaqueDouble(payloadDouble).opaqueDouble();
    expect(std::memcmp(&payloadFloat, &actualFloat, sizeof(float)) == 0, "float NaN payload bits");
    expect(std::memcmp(&payloadDouble, &actualDouble, sizeof(double)) == 0, "double NaN payload bits");
    for (ValueType tag : {ValueType::NoSuchObject, ValueType::NoSuchInstance, ValueType::EndOfMibView})
        expect(Value::exception(tag).type() == tag, "exception tag retained");
    expect(rejects([&] { bytes.integer(); }), "wrong tag rejected");
    expect(rejects([&] { bytes.unsigned32(); }), "wrong unsigned tag rejected");
    expect(rejects([&] { bytes.counter64(); }), "wrong counter64 tag rejected");
    expect(rejects([&] { bytes.objectId(); }), "wrong OID tag rejected");
    expect(rejects([&] { bytes.ipAddress(); }), "wrong IP tag rejected");
    expect(rejects([&] { bytes.opaqueFloat(); }), "wrong float tag rejected");
    expect(rejects([&] { bytes.opaqueDouble(); }), "wrong double tag rejected");
    expect(rejects([&] { oid.octets(); }), "wrong octets tag rejected");
    expect(rejects([] { Value::unsigned32(ValueType::Integer, 1); }), "invalid unsigned tag rejected");
    expect(rejects([] { Value::exception(ValueType::Octets); }), "invalid exception tag rejected");
}
void jsonLimits()
{
    using snmp3::Json;
    const std::string depth32 = std::string(32, '[') + "0" + std::string(32, ']');
    expect(Json::parse(depth32).kind == Json::Kind::Array, "depth 32 accepted");
    expect(rejects([&] { Json::parse("[" + depth32 + "]"); }), "depth 33 rejected");
    std::string elements = "[0";
    for (unsigned i = 1; i < 4096; ++i) elements += ",0";
    expect(Json::parse(elements + "]").array.size() == 4096, "4096 array elements accepted");
    expect(rejects([&] { Json::parse(elements + ",0]"); }), "4097 array elements rejected");
    const std::string exact = "\"" + std::string(1024 * 1024 - 2, 'a') + "\"";
    expect(Json::parse(exact).text.size() == 1024 * 1024 - 2, "1 MiB JSON accepted");
    expect(rejects([&] { Json::parse(exact + " "); }), "oversized JSON rejected");
    const std::string pair = "\xf0\x9d\x84\x9e";
    expect(Json::parse("\"\\ud834\\udd1e\"").text == pair, "valid decoded surrogate pair retained");
    expect(Json::parse("{\"\\ud834\\udd1e\":\"\\u0080\"}").object.at(pair).text == "\xc2\x80",
           "valid decoded Unicode key and value retained");
    expect(rejects([] { Json::parse("\"\\udc00\""); }), "decoded lone surrogate value rejected");
    expect(rejects([] { Json::parse("{\"\\udc00\":0}"); }), "decoded lone surrogate key rejected");
    expect(rejects([] { Json::parse("\"\\ud800\""); }), "lone high surrogate rejected before substitution");
    expect(Json::parse("\"\\\\ud800\"").text == "\\ud800", "escaped literal Unicode escape retained");
}
void positive(const char* file, const char* probe, const char* replacement)
{
    using namespace snmp3;
    Config config;
    config.load(file, probe);
    const ConfigState original = config.snapshot();
    const auto& first = *original.configuration;
    expect(original.revision == 1 && !original.frozen, "initial publication");
    expect(first.profiles.size() == 6 && first.endpoints.size() == 4 && first.bindings.size() == 3, "complete definitions");
    const Profile& defaults = first.profiles.at("V2");
    expect(defaults.timeoutMs == 1000 && defaults.retries == 3 && defaults.maxVarbinds == 32 && defaults.checkRanges, "native snapshot defaults");
    expect(first.profiles.at("V1").version == Version::V1 && defaults.version == Version::V2c, "community protocols");
    expect(first.profiles.at("Plain").securityLevel == SecurityLevel::NoAuthNoPriv, "noAuthNoPriv");
    expect(first.profiles.at("Auth").securityLevel == SecurityLevel::AuthNoPriv, "authNoPriv");
    expect(first.profiles.at("Private").securityLevel == SecurityLevel::AuthPriv, "authPriv");
    expect(first.profiles.at("Plain").securityEngineId.empty() && first.profiles.at("Plain").contextEngineId.empty(), "discovery and inheritance declarations");
    expect(first.profiles.at("Private").securityEngineId.size() == 5 && first.profiles.at("Other").securityEngineId.size() == 32, "engine byte boundaries");
    expect(first.profiles.at("Private").securityEngineId != first.profiles.at("Private").contextEngineId, "independent engine IDs");
    expect(first.profiles.at("Private").contextName != first.profiles.at("Other").contextName, "distinct contexts");
    expect(first.endpoints.at("IPv6").address == "::1" && first.endpoints.at("IPv6").port == 1161, "normalized numeric endpoint");
    expect(!first.capabilities.authentication.empty() && !first.capabilities.privacy.empty(), "actual native capabilities");
    expect(rejects([&] { config.bind("absent"); }) && !config.snapshot().frozen, "invalid binding does not freeze");
    config.load(replacement, probe);
    auto second = config.snapshot();
    expect(second.revision == 2 && second.configuration != original.configuration, "whole publication replaced");
    expect(first.profiles.at("V2").timeoutMs == 1000, "old snapshot still owned");
    auto binding = config.bind("Read");
    expect(config.snapshot().frozen && binding->profile().timeoutMs == 1500 && !binding->profile().checkRanges, "first actual API binding freezes replacement");
    expect(binding->definition().operation == Operation::Get && binding->endpoint().address == "127.0.0.1", "binding owns typed identity");
    expect(rejects([&] { config.load(file, probe); }), "frozen load rejected");
    expect(config.snapshot().revision == 2 && binding->profile().timeoutMs == 1500, "frozen state retained");
    config.freeze(); config.freeze();
    expect(config.snapshot().configuration == second.configuration, "repeated freeze preserves snapshot");
    values();

    Config concurrent; concurrent.load(file, probe);
    std::atomic<bool> valid(true);
    std::vector<std::thread> callers;
    for (int i = 0; i < 4; ++i) callers.emplace_back([&] {
        for (int j = 0; j < 100; ++j) {
            const auto state = concurrent.snapshot();
            if (state.revision < 1 || state.configuration->profiles.size() != 6 ||
                state.configuration->bindings.size() != 3) valid = false;
        }
    });
    callers.emplace_back([&] {
        try { concurrent.load(replacement, probe); }
        catch (const std::exception& error) { if (std::strcmp(error.what(), "configuration frozen")) valid = false; }
    });
    auto held = concurrent.bind("Read");
    concurrent.freeze();
    for (auto& caller : callers) caller.join();
    expect(valid && concurrent.snapshot().frozen && held->definition().id == "Read", "concurrent snapshot/load/bind/freeze");
}
void negative(const char* good, const char* probe, const char* bad, const char* badProbe)
{
    snmp3::Config config; config.load(good, probe);
    const auto before = config.snapshot();
    expect(rejects([&] { config.load(bad, badProbe); }), "candidate rejected");
    const auto after = config.snapshot();
    expect(after.revision == before.revision && after.configuration == before.configuration && !after.frozen,
           "entire previous snapshot retained");
    auto binding = config.bind("Read");
    expect(binding->profile().timeoutMs == 1000 && binding->definition().oid.back() == 0,
           "retained candidate remains bindable");
}
void accepted(const char* file, const char* probe)
{
    snmp3::Config config;
    config.load(file, probe);
    const auto state = config.snapshot();
    expect(state.revision == 1 && !state.frozen, "accepted whole candidate");
    for (const auto& item : state.configuration->bindings) {
        auto binding = config.bind(item.first);
        expect(binding->definition().id == item.first, "accepted binding identity");
        expect(binding->capabilities().executable == probe, "selected native product retained");
    }
}
void publicationRace(const char* file, const char* probe, const char* gate,
                     const char* marker, const char* release, bool bind)
{
    snmp3::Config config;
    config.load(file, probe);
    const auto before = config.snapshot();
    std::atomic<bool> frozenRejection(false);
    std::thread loading([&] {
        try { config.load(file, gate); }
        catch (const std::exception& error) {
            frozenRejection = !std::strcmp(error.what(), "configuration frozen");
        }
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool ready = false;
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream input(marker);
        if (input.good()) { ready = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (bind) config.bind("Read");
    else config.freeze();
    { std::ofstream output(release); output << "release\n"; }
    loading.join();
    const auto after = config.snapshot();
    expect(ready && frozenRejection, "concurrent freeze rejects completed real candidate");
    expect(after.frozen && after.revision == before.revision &&
           after.configuration == before.configuration, "publication race retains exact snapshot");
}
}
int main(int argc, char** argv)
{
    try {
        if (argc == 5 && std::strcmp(argv[1], "positive") == 0) positive(argv[2], argv[3], argv[4]);
        else if (argc == 6 && std::strcmp(argv[1], "negative") == 0) negative(argv[2], argv[3], argv[4], argv[5]);
        else if (argc == 4 && std::strcmp(argv[1], "accept") == 0) accepted(argv[2], argv[3]);
        else if (argc == 2 && !std::strcmp(argv[1], "json-limits")) jsonLimits();
        else if (argc == 7 && (!std::strcmp(argv[1], "race-freeze") || !std::strcmp(argv[1], "race-bind")))
            publicationRace(argv[2], argv[3], argv[4], argv[5], argv[6], !std::strcmp(argv[1], "race-bind"));
        else throw std::runtime_error("configuration test arguments rejected");
        std::printf("{\"status\":\"PASS\",\"checks\":%u}\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "configuration test failed: %s\n", error.what());
        return 1;
    }
}
