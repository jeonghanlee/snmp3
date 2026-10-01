#ifndef SNMP3_NATIVE_INTERNAL_H
#define SNMP3_NATIVE_INTERNAL_H

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "Worker.h"
#include <deque>
#include <thread>

namespace snmp3 { namespace detail {
struct Service;
struct Request;
struct Session {
    Service* owner = nullptr;
    void* handle = nullptr;
    bool closing = false;
    uint64_t reservation = 0;
    Profile profile;
    NativeIdentity identity;
    std::map<uint64_t, std::unique_ptr<Request>> requests;
    ~Session();
};
struct Request {
    Session* session;
    uint64_t token;
    bool terminal = false;
    Operation operation;
    std::vector<std::shared_ptr<const Binding>> bindings;
    std::vector<Value> payload;
    Native::Completion completion;
};
struct SecurityMaterial {
    SecurityLevel level;
    Algorithm authentication, privacy;
    std::vector<uint8_t> authSecret, privSecret, authKey, privKey;
};
using SecurityTuple = std::pair<std::vector<uint8_t>, std::string>;
struct Service {
    const std::thread::id thread = std::this_thread::get_id();
    bool accepting = true, delivering = false;
    bool bounded = false;
    uint64_t nativeBytes = 0, pinnedCount = 0, liveCount = 0, droppedEvents = 0;
    unsigned depth = 0;
    uint64_t nextSession = 1, nextToken = 1;
    std::vector<std::weak_ptr<Session>> sessions;
    std::map<SecurityTuple, SecurityMaterial> security;
    std::deque<std::pair<Native::Completion, NativeResult>> completions;
    std::vector<NativeEvent> events;
    Service();
    explicit Service(bool bounded);
    ~Service();
    void check() const;
    void enter();
    void leave();
    void event(const char* name, uint64_t session = 0, uint64_t token = 0,
               int operation = 0, long detail = 0);
    void finish(Request& request, NativeResult result);
    void close(Session& session);
    void poll(unsigned maximumWaitMs);
    void reserve(uint64_t bytes);
    void release(uint64_t bytes);
};
std::shared_ptr<Session> open(Service& owner, const Profile& profile, const Endpoint& endpoint,
                              const Capabilities& expected);
uint64_t submit(Session& session, const std::vector<std::shared_ptr<const Binding>>& bindings,
                const std::vector<Value>& payload, Native::Completion completion);
}}
#endif
