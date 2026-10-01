#include "Config.h"
#include "Capabilities.h"
#include "Worker.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/resource.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <thread>
#include <csignal>
#include <sys/time.h>
#include <limits>

namespace {
using namespace snmp3;
constexpr unsigned DeadlineMs = 5000;
constexpr size_t HighFdCount = 1100;
void interrupted(int) {}
void value(const Value& data)
{
    std::printf("{\"type\":%d,\"data\":", static_cast<int>(data.type()));
    switch (data.type()) {
    case ValueType::Integer: std::printf("%lld", static_cast<long long>(data.integer())); break;
    case ValueType::Unsigned32:
    case ValueType::Counter32:
    case ValueType::Gauge32:
    case ValueType::TimeTicks: std::printf("%u", data.unsigned32()); break;
    case ValueType::Counter64: std::printf("%llu", static_cast<unsigned long long>(data.counter64())); break;
    case ValueType::Octets: {
        std::printf("[");
        for (size_t i = 0; i < data.octets().size(); ++i) std::printf("%s%u", i ? "," : "", data.octets()[i]);
        std::printf("]"); break;
    }
    case ValueType::ObjectId: {
        std::printf("[");
        for (size_t i = 0; i < data.objectId().size(); ++i) std::printf("%s%u", i ? "," : "", data.objectId()[i]);
        std::printf("]"); break;
    }
    case ValueType::IpAddress: {
        const auto ip = data.ipAddress();
        std::printf("[%u,%u,%u,%u]", ip[0], ip[1], ip[2], ip[3]); break;
    }
    case ValueType::OpaqueFloat: {
        uint32_t bits; const float native = data.opaqueFloat();
        std::memcpy(&bits, &native, sizeof(bits)); std::printf("%u", bits); break;
    }
    case ValueType::OpaqueDouble: {
        uint64_t bits; const double native = data.opaqueDouble();
        std::memcpy(&bits, &native, sizeof(bits));
        std::printf("%llu", static_cast<unsigned long long>(bits)); break;
    }
    default: std::printf("null");
    }
    std::printf("}");
}
void result(const NativeResult& data, const char* event = "result")
{
    std::printf("{\"event\":\"%s\",\"token\":%llu,\"outcome\":%d,\"status\":%ld,\"index\":%ld,\"values\":[",
        event, static_cast<unsigned long long>(data.token), static_cast<int>(data.outcome), data.errorStatus, data.errorIndex);
    for (size_t i = 0; i < data.variables.size(); ++i) {
        if (i) std::printf(",");
        value(data.variables[i].value);
    }
    std::puts("]}");
}
void trace(Worker& worker)
{
    for (const auto& event : worker.takeEvents())
        std::printf("{\"event\":\"%s\",\"session\":%llu,\"token\":%llu,\"operation\":%d,\"detail\":%ld,\"monotonic_us\":%lld}\n",
            event.event.c_str(), static_cast<unsigned long long>(event.session),
            static_cast<unsigned long long>(event.token), event.operation, event.detail,
            static_cast<long long>(event.monotonicUs));
}
void wait(Worker& worker, const std::vector<NativeResult>& completed, size_t count)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(DeadlineMs);
    while (completed.size() < count && std::chrono::steady_clock::now() < end) worker.service(50);
    trace(worker);
    if (completed.size() < count) throw std::runtime_error("adapter test deadline expired");
}
std::shared_ptr<Native> open(Worker& worker, const Configuration& config, const std::string& endpoint)
{
    const auto& target = config.endpoints.at(endpoint);
    std::shared_ptr<Native> session;
    try { session = worker.open(config.profiles.at(target.profile), target, config.capabilities); }
    catch (...) { trace(worker); throw; }
    const auto& identity = session->identity();
    std::printf("{\"event\":\"identity\",\"session\":%llu,\"security_engine\":[",
                static_cast<unsigned long long>(identity.session));
    for (size_t i = 0; i < identity.securityEngineId.size(); ++i)
        std::printf("%s%u", i ? "," : "", identity.securityEngineId[i]);
    std::printf("],\"context_engine\":[");
    for (size_t i = 0; i < identity.contextEngineId.size(); ++i)
        std::printf("%s%u", i ? "," : "", identity.contextEngineId[i]);
    std::printf("],\"context_length\":%zu}\n", identity.contextName.size());
    return session;
}
std::vector<Value> initial(unsigned context)
{
    return {Value::integer(-123L - static_cast<long>(context)),
        Value::unsigned32(ValueType::Counter32, UINT32_MAX),
        Value::unsigned32(ValueType::Gauge32, UINT32_MAX),
        Value::unsigned32(ValueType::TimeTicks, UINT32_MAX), Value::counter64(UINT64_MAX),
        Value::octets({'A', 0, 'B', 255, static_cast<uint8_t>(context)}),
        Value::objectId({1, 3, 6, 1, 4, 1, 53864}), Value::ipAddress({{127, 0, 0, 1}}),
        Value::opaqueFloat(-2.25f), Value::opaqueDouble(1.0 / 3.0)};
}
}
int main(int argc, char** argv)
{
    if (argc < 5 || argc > 6) {
        std::fprintf(stderr, "Usage: snmp3NativeTest CASE CONFIG HELPER ENDPOINT [SECOND_ENDPOINT]\n");
        return 2;
    }
    setvbuf(stdout, nullptr, _IOLBF, 0);
    try {
        const std::string mode = argv[1], endpoint = argv[4];
        Config config;
        config.load(argv[2], argv[3]);
        const auto snapshot = config.snapshot().configuration;
        std::vector<int> held;
        if (mode == "high-fd") {
            rlimit limit;
            if (getrlimit(RLIMIT_NOFILE, &limit) || limit.rlim_max < HighFdCount + FD_SETSIZE)
                throw std::runtime_error("adapter high descriptor limit unavailable");
            limit.rlim_cur = std::max(limit.rlim_cur, static_cast<rlim_t>(HighFdCount + FD_SETSIZE));
            if (setrlimit(RLIMIT_NOFILE, &limit)) throw std::runtime_error("adapter descriptor limit rejected");
            for (size_t i = 0; i < HighFdCount; ++i) {
                const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
                if (fd < 0) throw std::runtime_error("adapter descriptor reservation failed");
                held.push_back(fd);
            }
        }
        std::vector<NativeResult> completed;
        Worker worker;
        const auto binding = config.bind(endpoint + "_G1");
        if (mode == "open-failure") {
            nativeCapabilities();
            {
                auto primed = open(worker, *snapshot, endpoint);
                try { primed->submit({}, {}, [](NativeResult) {}); }
                catch (const NativeError& error) {
                    if (error.outcome() != NativeOutcome::InvalidRequest) throw;
                    std::puts("{\"event\":\"native_error_primed\"}");
                }
                primed->close();
            }
            std::puts("{\"event\":\"native_lifetime_primed\"}");
            rlimit limit;
            if (getrlimit(RLIMIT_NOFILE, &limit)) throw std::runtime_error("process limit unavailable");
            limit.rlim_cur = std::min<rlim_t>(limit.rlim_cur, 128);
            if (setrlimit(RLIMIT_NOFILE, &limit)) throw std::runtime_error("process limit rejected");
            while (true) {
                const int fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
                if (fd < 0) break;
                held.push_back(fd);
            }
        }
        std::shared_ptr<Native> session;
        try { session = open(worker, *snapshot, endpoint); }
        catch (...) {
            for (int fd : held) ::close(fd);
            held.clear();
            throw;
        }
        auto collect = [&](NativeResult data) { result(data); completed.push_back(std::move(data)); };
        if (mode == "interrupted") {
            struct sigaction action = {};
            action.sa_handler = interrupted;
            sigemptyset(&action.sa_mask);
            sigaction(SIGALRM, &action, nullptr);
            itimerval timer = {};
            timer.it_value.tv_usec = timer.it_interval.tv_usec = 10000;
            setitimer(ITIMER_REAL, &timer, nullptr);
        }
        if (mode == "mixed-deadlines") {
            if (argc != 6) throw std::runtime_error("second endpoint absent");
            auto second = open(worker, *snapshot, argv[5]);
            bool expired = false, busy = false;
            session->submit({binding}, {}, [&](NativeResult data) {
                expired = true; collect(std::move(data));
            });
            const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(DeadlineMs);
            while ((!expired || busy) && std::chrono::steady_clock::now() < end) {
                if (!expired && !busy) {
                    busy = true;
                    second->submit({config.bind(std::string(argv[5]) + "_G1")}, {}, [&](NativeResult data) {
                        busy = false; collect(std::move(data));
                    });
                }
                worker.service(5);
            }
            if (!expired || busy) {
                session->close();
                second->close();
                throw std::runtime_error("mixed deadline test expired");
            }
            second->close();
        } else if (mode == "conflict" || mode == "compatible") {
            if (argc != 6) throw std::runtime_error("second endpoint absent");
            session->submit({binding}, {}, collect);
            std::shared_ptr<Native> second;
            try { second = open(worker, *snapshot, argv[5]); }
            catch (const NativeError& error) {
                std::printf("{\"event\":\"open_error\",\"outcome\":%d}\n", static_cast<int>(error.outcome()));
            }
            if (second) second->submit({config.bind(std::string(argv[5]) + "_G1")}, {}, collect);
            wait(worker, completed, second ? 2 : 1);
            if (mode == "compatible" && second) {
                second->submit({config.bind(std::string(argv[5]) + "_G1")}, {}, collect);
                session->close();
                session = open(worker, *snapshot, endpoint);
                session->submit({binding}, {}, collect);
                wait(worker, completed, 4);
            }
            session->close();
            if (second) second->close();
            if (mode == "conflict") {
                try { second = open(worker, *snapshot, argv[5]); }
                catch (const NativeError& error) {
                    std::printf("{\"event\":\"reopen_error\",\"outcome\":%d}\n", static_cast<int>(error.outcome()));
                }
            } else if (second) {
                session = open(worker, *snapshot, endpoint);
                session->submit({binding}, {}, collect);
                wait(worker, completed, 5);
            }
        } else if (mode == "restart") {
            if (argc != 6) throw std::runtime_error("restart release marker absent");
            session->submit({binding}, {}, collect);
            wait(worker, completed, 1);
            std::puts("{\"event\":\"restart_ready\"}");
            const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(DeadlineMs);
            bool released = false;
            while (std::chrono::steady_clock::now() < end) {
                std::ifstream marker(argv[5]);
                if (marker.good()) { released = true; break; }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!released) throw std::runtime_error("agent restart not released");
            session->submit({binding}, {}, collect);
            wait(worker, completed, 2);
        } else if (mode == "errors" || mode == "exceptions" || mode == "integer-boundaries") {
            const unsigned first = mode == "errors" ? 11 : mode == "exceptions" ? 13 : 16;
            const unsigned last = mode == "errors" ? 12 : mode == "exceptions" ? 15 : 18;
            for (unsigned i = first; i <= last; ++i) {
                session->submit({config.bind(endpoint + "_G" + std::to_string(i))}, {}, collect);
                wait(worker, completed, i - first + 1);
            }
        } else if (mode == "reentry") {
            session->submit({binding}, {}, [&](NativeResult data) {
                collect(std::move(data));
                session->submit({binding}, {}, [&](NativeResult next) {
                    collect(std::move(next)); session->close();
                });
            });
            wait(worker, completed, 2);
        } else if (mode == "send-failure") {
            session->submit({config.bind(endpoint + "_S6")},
                {Value::octets(std::vector<uint8_t>(70000, 0))}, collect);
            wait(worker, completed, 1);
        } else if (mode == "value-boundaries") {
            std::vector<std::shared_ptr<const Binding>> get, set;
            for (unsigned i = 1; i <= 10; ++i) {
                get.push_back(config.bind(endpoint + "_G" + std::to_string(i)));
                set.push_back(config.bind(endpoint + "_S" + std::to_string(i)));
            }
            auto zero = initial(0);
            zero[0] = Value::integer(INT32_MIN);
            for (size_t i = 1; i <= 3; ++i) zero[i] = Value::unsigned32(zero[i].type(), 0);
            zero[4] = Value::counter64(0); zero[5] = Value::octets({});
            zero[6] = Value::objectId({0, 0}); zero[7] = Value::ipAddress({{0, 0, 0, 0}});
            zero[8] = Value::opaqueFloat(-0.0f); zero[9] = Value::opaqueDouble(-0.0);
            session->submit(set, zero, collect);
            zero.clear();
            wait(worker, completed, 1);
            session->submit(get, {}, collect); wait(worker, completed, 2);
            uint32_t floatBits = 0x7fc12345;
            uint64_t doubleBits = 0x7ff8123456789abcULL;
            float floating; double precise;
            std::memcpy(&floating, &floatBits, sizeof(floating));
            std::memcpy(&precise, &doubleBits, sizeof(precise));
            auto high = initial(0);
            high[0] = Value::integer(INT32_MAX);
            high[6] = Value::objectId({2, UINT32_MAX - 80, UINT32_MAX});
            high[8] = Value::opaqueFloat(floating); high[9] = Value::opaqueDouble(precise);
            session->submit(set, high, collect); wait(worker, completed, 3);
            session->submit(get, {}, collect); wait(worker, completed, 4);
            high[8] = Value::opaqueFloat(std::numeric_limits<float>::infinity());
            high[9] = Value::opaqueDouble(std::numeric_limits<double>::infinity());
            session->submit(set, high, collect); wait(worker, completed, 5);
            session->submit(get, {}, collect); wait(worker, completed, 6);
            session->submit(set, initial(0), collect); wait(worker, completed, 7);
            result(completed[1], "retained_result");
            bool rejected = false;
            try {
                session->submit({set[6]}, {Value::objectId({2, UINT32_MAX, 0})}, collect);
            } catch (const NativeError& error) { rejected = error.outcome() == NativeOutcome::InvalidRequest; }
            std::printf("{\"event\":\"native_oid_limit\",\"rejected_before_dispatch\":%s}\n",
                        rejected ? "true" : "false");
        } else if (mode == "unsigned-alias") {
            const auto get = config.bind(endpoint + "_G3");
            session->submit({get}, {}, collect);
            wait(worker, completed, 1);
            session->submit({config.bind(endpoint + "_S3")},
                            {Value::unsigned32(ValueType::Unsigned32, UINT32_MAX)}, collect);
            wait(worker, completed, 2);
            session->submit({get}, {}, collect);
            wait(worker, completed, 3);
        } else if (mode == "typed") {
            std::vector<std::shared_ptr<const Binding>> get, set;
            for (unsigned i = 1; i <= 10; ++i) {
                get.push_back(config.bind(endpoint + "_G" + std::to_string(i)));
                set.push_back(config.bind(endpoint + "_S" + std::to_string(i)));
            }
            session->submit(get, {}, collect);
            wait(worker, completed, 1);
            const auto& profile = snapshot->profiles.at(snapshot->endpoints.at(endpoint).profile);
            const unsigned context = profile.contextName == "alpha" ? 1 : profile.contextName == "beta" ? 2 : 0;
            auto payload = initial(context);
            session->submit(set, payload, collect);
            payload.clear();
            payload.push_back(Value::integer(999));
            wait(worker, completed, 2);
            session->submit(get, {}, collect);
            wait(worker, completed, 3);
            result(completed.front(), "retained_result");
        } else if (mode == "invalid") {
            const auto limit = binding->profile().maxVarbinds;
            for (auto batch : {std::vector<std::shared_ptr<const Binding>>{},
                               std::vector<std::shared_ptr<const Binding>>(limit + 1, binding)}) {
                bool rejected = false;
                try { session->submit(batch, {}, collect); }
                catch (const NativeError& error) { rejected = error.outcome() == NativeOutcome::InvalidRequest; }
                std::printf("{\"event\":\"batch_rejected\",\"size\":%zu,\"rejected\":%s}\n",
                            batch.size(), rejected ? "true" : "false");
            }
            session->submit(std::vector<std::shared_ptr<const Binding>>(limit, binding), {}, collect);
            wait(worker, completed, 1);
        } else {
            const auto batch = mode == "batch" ? std::vector<std::shared_ptr<const Binding>>{
                binding, config.bind(endpoint + "_G2")} : mode == "duplicate-oid" ?
                std::vector<std::shared_ptr<const Binding>>{binding, binding} :
                std::vector<std::shared_ptr<const Binding>>{binding};
            if (mode == "set-once") session->submit({config.bind(endpoint + "_S1")}, {Value::integer(-123)}, collect);
            else session->submit(batch, {}, collect);
            if (mode == "pending-close") { session->close(); session->close(); }
            wait(worker, completed, 1);
            if (mode == "settle") {
                for (unsigned i = 0; i < 8; ++i) worker.service(50);
            }
        }
        session->close();
        trace(worker);
        if (mode == "interrupted") {
            itimerval timer = {};
            setitimer(ITIMER_REAL, &timer, nullptr);
        }
        for (int fd : held) ::close(fd);
        std::printf("{\"event\":\"summary\",\"completions\":%zu}\n", completed.size());
        return 0;
    } catch (const NativeError& error) {
        std::printf("{\"event\":\"driver_error\",\"outcome\":%d}\n", static_cast<int>(error.outcome()));
        return 1;
    } catch (...) {
        std::fprintf(stderr, "native adapter test failed\n");
        return 1;
    }
}
