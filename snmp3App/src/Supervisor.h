#ifndef SNMP3_SUPERVISOR_H
#define SNMP3_SUPERVISOR_H
#include "Scheduler.h"
#include <atomic>
#include <sys/types.h>

namespace snmp3 {
struct WorkerSnapshot {
    uint64_t address=0,epoch=0,batch=0,spawnAttempts=0,reaps=0,forced=0;
    pid_t pid=0;
    bool ready=false,closing=false;
};
struct SupervisionEvent {
    uint64_t at=0,address=0,epoch=0,batch=0;
    int64_t pid=0,detail=0;
    uint32_t code=0;
};
struct Qualification {
    std::string stderrPath;
    bool sanitizers=false;
    uint64_t transportAddress=0;
    int transportParent=-1,transportChild=-1;
};
class Supervisor {
public:
    Supervisor(std::shared_ptr<Scheduler> scheduler,const std::string& executable,
               const Qualification& qualification=Qualification());
    ~Supervisor();
    Supervisor(const Supervisor&)=delete;
    Supervisor& operator=(const Supervisor&)=delete;
    void step(uint64_t nowUs);
    void requestStop();
    bool finished(uint64_t nowUs) const;
    bool reconcile();
    std::vector<WorkerSnapshot> snapshots() const;
    std::vector<SupervisionEvent> takeEvents();
    uint64_t droppedEvents() const;
    uint64_t nextTimer() const;
private:
    struct Address {
        WorkerSnapshot status;
        std::unique_ptr<ipc::Channel> channel;
        std::vector<uint8_t> bootstrap;
        std::vector<ipc::ResultReservation> reservations;
        std::vector<ipc::Identity> members;
        ipc::Header incoming;
        uint64_t sent=0,received=0,spawned=0,origin=0,restartAt=0,resultMaximum=0,failures=0;
        std::deque<uint64_t> attempts;
        ipc::Kind outgoing=ipc::Kind::Fault;
        bool reading=false,keep=false,term=false,kill=false,closed=false,batchSent=false;
    };
    void launch(Address& address,uint64_t nowUs);
    void contain(Address& address,uint64_t nowUs,ipc::Outcome outcome);
    void input(Address& address,uint64_t nowUs);
    void output(Address& address,uint64_t nowUs);
    bool reap(Address& address,uint64_t nowUs);
    void queue(Address& address,ipc::Kind kind,uint64_t batch,std::vector<uint8_t> bytes={});
    void event(Address& address,uint64_t nowUs,uint32_t code,int64_t detail=0);
    std::shared_ptr<Scheduler> scheduler;
    std::string executable,libraryPath;
    ipc::Ready selected;
    Qualification qualification;
    int qualificationFd=-1;
    int transportParent=-1,transportChild=-1;
    std::vector<Address> addresses;
    size_t roundRobin=0;
    uint64_t stopOrigin=0;
    std::atomic<bool> stopping{false};
    mutable std::mutex reportMutex;
    std::vector<WorkerSnapshot> published;
    std::vector<SupervisionEvent> events;
    uint64_t dropped=0;
};
uint64_t monotonicUs();
}
#endif
