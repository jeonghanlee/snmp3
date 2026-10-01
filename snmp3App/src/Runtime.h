#ifndef SNMP3_RUNTIME_H
#define SNMP3_RUNTIME_H

#include "BaseVersion.h"
#include "Supervisor.h"
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsThread.h>

namespace snmp3 {
enum class State { Cold, Starting, Running, Stopping, Stopped, IncompleteStopped, Failed };
struct ConfigState;
struct Snapshot {
    State state;
    bool admission;
    unsigned long activation, created, exited, joined;
};

class Runtime {
public:
    static Runtime& instance();
    bool start();
    void stop();
    Snapshot snapshot();
    void report();
    static const char* stateName(State state);
    uint64_t bind(const std::string& definition);
    std::vector<ipc::Identity> admit(const std::vector<uint64_t>& handles,
                                   const std::vector<Value>& payload,unsigned budgetMs);
    void queueLimit(const std::string& address,uint64_t count,uint64_t bytes);
    std::shared_ptr<Scheduler> schedulerOwner();
    void setQualification(const Qualification& mode);
    std::vector<SupervisionEvent> takeSupervisionEvents();
private:
    Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    static void initialize(void*);
    static void fallback(void*);
    static void run(void*);
    void trace(const char* event);
    void prepare(const ConfigState& config);
    epicsMutex operationMutex, stateMutex;
    epicsEvent ready, wake;
    epicsThreadId thread;
    Snapshot current;
    std::shared_ptr<Scheduler> scheduler;
    std::unique_ptr<Supervisor> supervisor;
    Qualification qualification;
    std::map<std::string,std::pair<uint64_t,uint64_t>> prestartLimits;
};
}
#endif
