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
    std::vector<NativeEvent> takeEvents();
    uint64_t droppedEvents() const;
private:
    std::unique_ptr<detail::Service> state;
};
}
#endif
