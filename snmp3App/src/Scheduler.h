#ifndef SNMP3_SCHEDULER_H
#define SNMP3_SCHEDULER_H

#include "Ipc.h"
#include <deque>
#include <map>
#include <mutex>
#include <utility>

namespace snmp3 {
struct QueueSnapshot {
    uint64_t address=0, count=0, bytes=0, countLimit=ipc::DefaultCount, byteLimit=ipc::DefaultBytes;
    uint64_t queued=0, active=0, undelivered=0, retirementPending=0;
    uint64_t behindRetirement=0, behindAdmitted=0, behindNeverSent=0;
    // Admission statistics per address: a burst is a run of admission calls each less than 5 ms after
    // the previous one; the span is the first to the last call of a burst, the gap the largest distance
    // between two consecutive calls inside a burst. Values are microseconds of the admission clock.
    uint64_t admissions=0, bursts=0, burstSpanMaxUs=0, burstGapMaxUs=0;
    bool accepting=false;
};
struct Dispatch {
    uint64_t address=0, batch=0;
    std::vector<ipc::Command> commands;
};
struct TerminalView {
    ipc::Identity id;
    const ipc::Result* result=nullptr;
    bool sent=false;
};
class Scheduler {
public:
    Scheduler(std::shared_ptr<const Configuration> config, uint64_t revision, uint64_t activation);
    uint64_t registerBinding(std::shared_ptr<const Binding> binding,
                             uint64_t retainedBytes=0, uint64_t generationBytes=0);
    std::vector<ipc::Identity> admit(const std::vector<uint64_t>& handles,
                                   const std::vector<Value>& payload, unsigned budgetMs, uint64_t nowUs);
    void limits(const std::string& address, uint64_t count, uint64_t bytes);
    QueueSnapshot snapshot(uint64_t address) const;
    std::vector<uint64_t> addresses() const;
    const std::string& key(uint64_t address) const;
    std::shared_ptr<const Configuration> configuration() const { return config; }
    uint64_t configurationRevision() const { return revision; }
    uint64_t activationId() const { return activation; }
    Dispatch dispatch(uint64_t address, uint64_t nowUs);
    void transmitted(uint64_t address, uint64_t batch);
    bool complete(uint64_t address, uint64_t batch, std::vector<ipc::Result> results, uint64_t nowUs);
    bool retired(uint64_t address, uint64_t batch, const std::vector<ipc::Identity>& members);
    bool expire(uint64_t address, uint64_t nowUs);
    void workerLost(uint64_t address, ipc::Outcome reason);
    void reaped(uint64_t address);
    void stop();
    TerminalView take(const ipc::Identity& terminal);
    void release(const ipc::Identity& terminal);
    bool settled() const;
    uint64_t earliestDeadline(uint64_t address) const;
    // Time at which the held queue front of an address may leave: zero when nothing is held (an ungrouped
    // binding, a group of one, a whole group, a member put back after a worker loss). The time can already
    // be past; dispatch() holds only while it lies in the future.
    uint64_t holdRelease(uint64_t address) const;
    // Earliest time at which expire() acts: a queued or unanswered deadline and, when grace is set, the
    // retirement grace of an answered member. Once the worker is contained the grace has acted and is
    // no longer offered, so a past time never keeps the loop from waiting.
    uint64_t earliestAction(uint64_t address, bool grace) const;
    // Called without the scheduler lock from release() once admission has closed, so a stopping
    // servicing loop learns that a terminal was consumed.
    void setNotifier(void (*function)(void*), void* context);
    static uint64_t fixedGenerationBytes();
    void reserveConfiguration(uint64_t supervisorBytes);
    void closeAdmission(uint64_t address);
private:
    // A handle holds at most one consumed, retirement-pending generation plus one unconsumed
    // generation; every structure is keyed by (binding, generation).
    using Key=std::pair<uint64_t,uint64_t>;
    static Key key(const ipc::Identity& id) { return Key(id.binding,id.generation); }
    struct Generation {
        ipc::Command command;
        ipc::Result result;
        uint64_t q=0, admittedUs=0;
        // recovered: put back at the queue front after a worker loss, so it leaves without a new hold.
        bool selected=false, borrowed=false, consumed=false, retired=false, sent=false, active=false, behind=false,
             recovered=false;
    };
    struct Handle {
        std::shared_ptr<const Binding> binding;
        uint64_t address=0, definition=0, generation=0;
        uint64_t generationBytes=0;
    };
    struct Address {
        std::string key;
        uint64_t countLimit=ipc::DefaultCount, byteLimit=ipc::DefaultBytes;
        uint64_t count=0, bytes=0, nextAdmission=1, nextBatch=1, batch=0;
        uint64_t behindAdmitted=0, behindNeverSent=0;
        uint64_t admissions=0, bursts=0, burstSpanMaxUs=0, burstGapMaxUs=0, burstStartUs=0, lastAdmissionUs=0;
        bool accepting=true;
        std::deque<Key> queue;
        std::vector<Key> active;
        std::map<Key,std::unique_ptr<Generation>> generations;
    };
    void select(Generation& g, ipc::Outcome outcome);
    void collect(Address& a, const Key& id);
    bool matches(const Address& a, uint64_t batch, const std::vector<ipc::Identity>& ids) const;
    bool compatible(uint64_t first, uint64_t next, const Generation& a, const Generation& b) const;
    const std::string& groupOf(uint64_t handle) const { return handles.at(handle).binding->definition().group; }
    uint64_t holdLocked(const Address& a) const;
    std::shared_ptr<const Configuration> config;
    const uint64_t revision, activation;
    mutable std::mutex mutex;
    std::map<uint64_t,Address> queues;
    std::map<std::string,uint64_t> keys,definitions;
    std::map<std::string,size_t> declared;
    std::map<uint64_t,Handle> handles;
    uint64_t nextHandle=1;
    uint64_t configurationBase=0,configurationBytes=0,registrationBytes=0;
    bool accepting=true;
    void (*notifier)(void*)=nullptr;
    void* notifierContext=nullptr;
};
}
#endif
