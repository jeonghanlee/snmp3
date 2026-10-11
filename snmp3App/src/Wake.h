#ifndef SNMP3_WAKE_H
#define SNMP3_WAKE_H

#include <cerrno>
#include <stdexcept>
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace snmp3 {
// A readiness source for the servicing loop: any thread signals it, the loop polls its descriptor
// together with the worker channels and clears it before it acts on the state it announces.
class Wake {
public:
    Wake() : fd(eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC))
    { if(fd<0)throw std::runtime_error("wake descriptor creation failed"); }
    ~Wake() { ::close(fd); }
    Wake(const Wake&)=delete;
    Wake& operator=(const Wake&)=delete;
    int descriptor() const { return fd; }
    void signal()
    {
        const uint64_t one=1;
        // A full counter already reports readiness, so a failed write loses nothing.
        while(::write(fd,&one,sizeof(one))<0 && errno==EINTR) {}
    }
    void clear()
    {
        uint64_t value;
        while(::read(fd,&value,sizeof(value))<0 && errno==EINTR) {}
    }
private:
    int fd;
};
}
#endif
