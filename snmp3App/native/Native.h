#ifndef SNMP3_NATIVE_H
#define SNMP3_NATIVE_H

#include "Binding.h"
#include <functional>

namespace snmp3 {
namespace detail { struct Session; }
class Worker;
enum class NativeOutcome {
    Complete, Timeout, OpenFailure, SendFailure, SecurityFailure, ProtocolFailure,
    SecurityConflict, CapabilityMismatch, ResponseRejected, Cancelled, InvalidRequest
};
class NativeError : public std::runtime_error {
public:
    NativeError(NativeOutcome value, const char* message) : std::runtime_error(message), value(value) {}
    NativeOutcome outcome() const { return value; }
private:
    NativeOutcome value;
};
struct NativeVariable {
    std::vector<uint32_t> oid;
    Value value;
};
struct NativeResult {
    uint64_t token = 0;
    NativeOutcome outcome = NativeOutcome::Complete;
    long errorStatus = 0, errorIndex = 0;
    std::vector<NativeVariable> variables;
};
struct NativeIdentity {
    uint64_t session = 0;
    Endpoint endpoint;
    Version version;
    SecurityLevel securityLevel;
    std::string profile, user, contextName;
    Algorithm authentication, privacy;
    std::vector<uint8_t> securityEngineId, contextEngineId;
};
class Native {
public:
    using Completion = std::function<void(NativeResult)>;
    ~Native();
    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;
    uint64_t submit(const std::vector<std::shared_ptr<const Binding>>& bindings,
                    const std::vector<Value>& payload, Completion completion);
    void close();
    const NativeIdentity& identity() const;
private:
    friend class Worker;
    explicit Native(std::shared_ptr<detail::Session> state) : state(std::move(state)) {}
    std::shared_ptr<detail::Session> state;
};
}
#endif
