#ifndef SNMP3_NATIVE_WORKER_H
#define SNMP3_NATIVE_WORKER_H

#include "Native.h"

namespace snmp3 {
namespace detail { struct Service; }
struct NativeEvent {
    std::string event;
    uint64_t session = 0, token = 0;
    int operation = 0;
    long detail = 0;
    int64_t monotonicUs = 0;
};
class Worker {
public:
    Worker();
    explicit Worker(bool bounded);
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    std::shared_ptr<Native> open(const Profile& profile, const Endpoint& endpoint,
                                 const Capabilities& expected);
    void service(unsigned maximumWaitMs);
    // As service(), and also returns when the channel descriptor is readable, or writable while
    // channelWrite is set. A maximumWaitUs of UINT64_MAX waits for readiness or a library timer only.
    void service(uint64_t maximumWaitUs, int channel, bool channelWrite);
    // Drops what the library remembers of the boots and time of an engine, so that the next request to it
    // synchronizes from the start. A unit that returns with its time behind the remembered one is refused
    // by the library until then.
    void forgetEngineTime(const std::vector<uint8_t>& engine);
    // Asks the unit of an SNMPv3 endpoint for its engine ID without authentication. Throws a NativeError with
    // the outcome Timeout when the unit does not answer.
    std::vector<uint8_t> discoverEngine(const Profile& profile, const Endpoint& endpoint);
    std::vector<NativeEvent> takeEvents();
    uint64_t droppedEvents() const;
private:
    std::unique_ptr<detail::Service> state;
};
}
#endif
