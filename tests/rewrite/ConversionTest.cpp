#include "Conversion.h"
#include "Request.h"
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>

using namespace snmp3;
namespace {
unsigned checks = 0;
void check(bool valid)
{ ++checks; if (!valid) throw std::runtime_error("conversion assertion failed"); }
void rejects(const std::function<void()>& action)
{
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    check(rejected);
}
double binary64(uint64_t bits)
{ double value; std::memcpy(&value, &bits, sizeof(value)); return value; }
uint32_t binary32(float value)
{ uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }

void rounding()
{
    struct Pair { uint64_t input; uint32_t expected; };
    const Pair cases[] = {
        {0x0000000000000000ULL, 0x00000000U},
        {0x8000000000000000ULL, 0x80000000U},
        {0x3ff000000fffffffULL, 0x3f800000U},
        {0x3ff0000010000000ULL, 0x3f800000U},
        {0x3ff0000010000001ULL, 0x3f800001U},
        {0x3ff0000030000000ULL, 0x3f800002U},
        {0xbff0000010000000ULL, 0xbf800000U},
        {0xbff0000030000000ULL, 0xbf800002U},
        {0x3690000000000000ULL, 0x00000000U},
        {0x3690000000000001ULL, 0x00000001U},
        {0xb690000000000000ULL, 0x80000000U},
        {0x36a0000000000000ULL, 0x00000001U},
        {0x36a8000000000000ULL, 0x00000002U},
        {0x3810000000000000ULL, 0x00800000U},
        {0x47efffffe0000000ULL, 0x7f7fffffU},
        {0xc7efffffe0000000ULL, 0xff7fffffU}
    };
    const int saved = std::fegetround();
    for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        check(std::fesetround(mode) == 0);
        std::feclearexcept(FE_ALL_EXCEPT);
        std::feraiseexcept(FE_DIVBYZERO);
        const int flags = std::fetestexcept(FE_ALL_EXCEPT);
        for (const auto& test : cases) {
            const auto value = convertNumeric(Value::opaqueDouble(binary64(test.input)), ValueType::OpaqueFloat, true);
            check(binary32(value.opaqueFloat()) == test.expected);
            check(std::fegetround() == mode && std::fetestexcept(FE_ALL_EXCEPT) == flags);
        }
        rejects([] { roundBinary32(binary64(0x47effffff0000000ULL)); });
        rejects([] { roundBinary32(binary64(0x7ff0000000000000ULL)); });
        rejects([] { roundBinary32(binary64(0x7ff8000000000001ULL)); });
        check(std::fegetround() == mode && std::fetestexcept(FE_ALL_EXCEPT) == flags);
    }
    check(std::fesetround(FE_TONEAREST) == 0);
    uint64_t generator = 0x62b53a9187ULL;
    for (unsigned i = 0; i < 50000; ++i) {
        generator ^= generator << 13;
        generator ^= generator >> 7;
        generator ^= generator << 17;
        if (((generator >> 52) & 0x7ff) == 0x7ff) continue;
        volatile double input = binary64(generator);
        volatile float hardware = static_cast<float>(input);
        if (!std::isfinite(hardware)) rejects([&] { roundBinary32(input); });
        else check(binary32(roundBinary32(input)) == binary32(hardware));
    }
    check(std::fesetround(saved) == 0);
    std::feclearexcept(FE_ALL_EXCEPT);
}

void numeric()
{
    check(convertScalar(Value::counter64(INT64_MAX), ScalarType::Signed64).integer() == INT64_MAX);
    check(convertScalar(Value::counter64(UINT64_MAX), ScalarType::Unsigned64).counter64() == UINT64_MAX);
    rejects([] { convertScalar(Value::counter64(uint64_t(INT64_MAX) + 1), ScalarType::Signed64); });
    rejects([] { convertScalar(Value::integer(-1), ScalarType::Unsigned64); });
    check(convertNumeric(Value::integer(INT32_MIN), ValueType::Integer).integer() == INT32_MIN);
    rejects([] { convertNumeric(Value::integer(int64_t(INT32_MAX) + 1), ValueType::Integer); });
    check(convertNumeric(Value::counter64(UINT32_MAX), ValueType::Counter32).unsigned32() == UINT32_MAX);
    rejects([] { convertNumeric(Value::counter64(uint64_t(UINT32_MAX) + 1), ValueType::Counter32); });
    check(convertScalar(Value::counter64(uint64_t(1) << 53), ScalarType::Float64).opaqueDouble() == 9007199254740992.0);
    rejects([] { convertScalar(Value::counter64((uint64_t(1) << 53) + 1), ScalarType::Float64); });
    check(convertScalar(Value::integer(16777216), ScalarType::Float32).opaqueFloat() == 16777216.0f);
    rejects([] { convertScalar(Value::integer(16777217), ScalarType::Float32); });
    rejects([] { convertScalar(Value::opaqueDouble(0.1), ScalarType::Float32); });
    rejects([] { convertNumeric(Value::opaqueDouble(1.5), ValueType::Counter64); });
    rejects([] { convertScalar(Value::opaqueDouble(std::ldexp(1.0, 63)), ScalarType::Signed64); });
    check(convertScalar(Value::opaqueDouble(-std::ldexp(1.0, 63)), ScalarType::Signed64).integer() == INT64_MIN);
    rejects([] { convertScalar(Value::opaqueDouble(std::ldexp(1.0, 64)), ScalarType::Unsigned64); });
    rejects([] { convertScalar(Value::exception(ValueType::NoSuchObject), ScalarType::Signed32); });
    const double precise = 1.0 / 3.0;
    check(convertNumeric(Value::opaqueDouble(precise), ValueType::OpaqueDouble).opaqueDouble() == precise);
}

void stringsAndGrammar()
{
    check(convertText(Value::octets({}), 0).empty());
    check(convertText(Value::octets({'A', 0xff}), 2).size() == 2);
    rejects([] { convertText(Value::octets({'A', 0, 'B'}), 3); });
    rejects([] { convertText(Value::octets({'A', 'B'}), 1); });
    check(convertText(Value::ipAddress({{127, 0, 0, 1}}), 9) == "127.0.0.1");
    check(convertText(Value::objectId({1, 3, 6}), 5) == "1.3.6");
    rejects([] { convertText(Value::objectId({1, 3, 6}), 4); });
    check(captureText("", 1, 0, 1).octets().empty());
    check(captureText("abc", 4, 3, 4).octets().size() == 3);
    rejects([] { captureText("abc", 3, 3); });
    rejects([] { captureText("abc", 4, 2, 4); });
    rejects([] { captureText("abc", 4, 3, 3); });
    std::string maximum(32766, 'x');
    check(captureText(maximum.c_str(), 32767, 32766, 32767).octets().size() == 32766);
    rejects([&] { captureText(maximum.c_str(), 32766, 32766); });
    check(parseRecordLink("@binding=Read.1 deadline_ms=1").definition == "Read.1");
    check(parseRecordLink("@deadline_ms=600000 binding=Read.1").budgetMs == 600000);
    for (const std::string text : {"binding=x deadline_ms=1", "@binding=x", "@binding=x deadline_ms=0",
         "@binding=x deadline_ms=600001", "@binding=x deadline_ms=-1", "@binding=x deadline_ms=1.0",
         "@binding=x deadline_ms=1 trailing", "@binding=x binding=y deadline_ms=1",
         "@binding=x deadline_ms=1 deadline_ms=2", "@binding=x deadline_ms=1 unknown=2"})
        rejects([&] { parseRecordLink(text); });
}
}

int main()
{
    try {
        rounding(); numeric(); stringsAndGrammar();
        std::printf("snmp3 conversion checks=%u\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "snmp3 conversion failure: %s checks=%u\n", error.what(), checks);
        return 1;
    }
}
