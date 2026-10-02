#include "Runtime.h"
#include "Config.h"
#include "Request.h"

#include <cstdio>
#include <cantProceed.h>
#include <epicsExit.h>
#include <algorithm>
#include <climits>
#include <set>

namespace snmp3 {
namespace {
epicsThreadOnceId once = EPICS_THREAD_ONCE_INIT;
Runtime* owner = NULL;
}

Runtime::Runtime()
    : thread(NULL), current{State::Cold, false, 0, 0, 0, 0}
{}

void Runtime::initialize(void*)
{
    // Process storage survives Base isolated-database cleanup and hook reuse.
    owner = new Runtime;
    if (epicsAtExit(&Runtime::fallback, owner) != 0)
        cantProceed("snmp3: cannot register process fallback\n");
}

Runtime& Runtime::instance()
{
    epicsThreadOnce(&once, &Runtime::initialize, NULL);
    return *owner;
}

const char* Runtime::stateName(State state)
{
    switch (state) {
    case State::Cold: return "Cold";
    case State::Starting: return "Starting";
    case State::Running: return "Running";
    case State::Stopping: return "Stopping";
    case State::Stopped: return "Stopped";
    case State::IncompleteStopped: return "IncompleteStopped";
    case State::Failed: return "Failed";
    }
    return "Unknown";
}

void Runtime::trace(const char* event)
{
    // Caller holds the state lock; each line describes one observed transition.
    std::printf("snmp3 lifecycle: %s activation=%lu\n", event, current.activation);
}

bool Runtime::start()
{
    epicsGuard<epicsMutex> lifecycle(lifecycleMutex);
    epicsGuard<epicsMutex> operation(operationMutex);
    Config::instance().freeze();
    {
        epicsGuard<epicsMutex> state(stateMutex);
        if(current.state==State::Running)return true;
        if(current.state==State::Failed)return false;
    }
    if(Requests::instance().snapshot().drainFailed)return false;
    if(supervisor && !thread && !supervisor->reconcile()) {
        epicsGuard<epicsMutex> state(stateMutex); current.state=State::IncompleteStopped; return false;
    }
    try {
        const auto config=Config::instance().snapshot();
        prepare(config);
        supervisor.reset();
        if(!config.configuration->endpoints.empty())supervisor.reset(new Supervisor(scheduler,config.workerPath,qualification));
        Requests::instance().start(schedulerOwner());
    } catch(const std::exception&) {
        if(scheduler)scheduler->stop();
        epicsGuard<epicsMutex> state(stateMutex); current.state=State::Failed; current.admission=false; trace("preflight-failed"); return false;
    }
    {
        epicsGuard<epicsMutex> state(stateMutex);
        if (current.state == State::Running)
            return true;
        if (current.state == State::Failed)
            return false;
        ready.tryWait();
        wake.tryWait();
        current.state = State::Starting;
        current.admission = false;
        if(current.activation==ULONG_MAX) { current.state=State::Failed; return false; }
        ++current.activation;
        trace("starting");
        epicsThreadOpts options = EPICS_THREAD_OPTS_INIT;
        options.joinable = 1;
        thread = epicsThreadCreateOpt("snmp3Runtime", &Runtime::run, this, &options);
        if (!thread) {
            if(scheduler)scheduler->stop();
            current.state = State::Failed;
            trace("create-failed");
            return false;
        }
        ++current.created;
        trace("created");
    }
    ready.wait();
    return snapshot().state == State::Running;
}

void Runtime::run(void* argument)
{
    Runtime& self = *static_cast<Runtime*>(argument);
    {
        epicsGuard<epicsMutex> state(self.stateMutex);
        self.current.state = State::Running;
        self.current.admission = true;
        self.trace("ready");
        self.ready.trigger();
    }
    if(self.supervisor) {
        try {
            for(;;) {
                const auto now=monotonicUs(); self.supervisor->step(now);
                Requests::instance().service();
                if(self.supervisor->finished(monotonicUs()))break;
                const auto timer=self.supervisor->nextTimer(); const auto origin=monotonicUs();
                self.wake.wait(timer<=origin ? 0.0:std::min(0.01,double(timer-origin)/1000000.0));
            }
        } catch(...) {
            {
                epicsGuard<epicsMutex> state(self.stateMutex);
                self.current.admission=false;
                self.current.state=State::Failed;
                self.trace("service-failed");
            }
            self.supervisor->requestStop();
            const auto end=ipc::add(monotonicUs(),2000000);
            while(monotonicUs()<end) {
                try {
                    self.supervisor->step(monotonicUs()); Requests::instance().service();
                    if(self.supervisor->finished(monotonicUs()))break;
                } catch(...) {}
                self.wake.wait(0.01);
            }
        }
    } else self.wake.wait();
    {
        epicsGuard<epicsMutex> state(self.stateMutex);
        ++self.current.exited;
        self.trace("worker-exit");
    }
}

void Runtime::stop()
{
    // Completion callbacks need no lifecycle lock; waits hold neither operation nor record locks.
    epicsGuard<epicsMutex> lifecycle(lifecycleMutex);
    Config::instance().freeze();
    epicsThreadId target;
    const bool recordDrainFailed=Requests::instance().snapshot().drainFailed;
    {
        epicsGuard<epicsMutex> state(stateMutex);
        current.admission = false;
        target = thread;
        if(target) {
            current.state = State::Stopping;
            trace("stop-requested");
        }
    }
    if(!target) {
        const bool drained=Requests::instance().drain();
        bool reconciled=true;
        if(supervisor) { supervisor->requestStop(); reconciled=supervisor->reconcile(); }
        epicsGuard<epicsMutex> state(stateMutex);
        if(reconciled && drained && !recordDrainFailed && current.state==State::IncompleteStopped)
            current.state=State::Stopped;
        return;
    }
    if(supervisor)supervisor->requestStop();
    else if(scheduler)scheduler->stop();
    wake.trigger();
    epicsThreadMustJoin(target);
    const bool drained=Requests::instance().drain();
    {
        epicsGuard<epicsMutex> state(stateMutex);
        thread = NULL;
        ++current.joined;
        current.state = drained && (!supervisor || supervisor->reconcile()) ? State::Stopped:State::IncompleteStopped;
        trace("joined");
    }
}

Snapshot Runtime::snapshot()
{
    epicsGuard<epicsMutex> state(stateMutex);
    return current;
}

void Runtime::report()
{
    epicsGuard<epicsMutex> operation(operationMutex);
    const Snapshot value = snapshot();
    std::printf("snmp3 runtime: state=%s admission=%u activation=%lu created=%lu exited=%lu joined=%lu\n",
                stateName(value.state), value.admission ? 1u : 0u,
                value.activation, value.created, value.exited, value.joined);
    const auto records=Requests::instance().snapshot();
    std::printf("snmp3 records: contexts=%llu active=%llu pending=%llu queued=%llu running=%llu inert=%llu entered=%llu enqueueFailures=%llu completions=%llu entryOpen=%u drainFailed=%u detachAllowed=%u\n",
                (unsigned long long)records.contexts,(unsigned long long)records.active,
                (unsigned long long)records.pending,(unsigned long long)records.queued,
                (unsigned long long)records.running,(unsigned long long)records.inert,
                (unsigned long long)records.entered,(unsigned long long)records.enqueueFailures,
                (unsigned long long)records.completions,records.entryOpen?1u:0u,
                records.drainFailed?1u:0u,records.detachAllowed?1u:0u);
    auto owner=schedulerOwner();
    if(owner)for(const auto id:owner->addresses()) {
        const auto queue=owner->snapshot(id);
        std::printf("snmp3 queue: address=%llu count=%llu bytes=%llu countLimit=%llu byteLimit=%llu queued=%llu active=%llu undelivered=%llu retirementPending=%llu\n",
                    (unsigned long long)id,(unsigned long long)queue.count,(unsigned long long)queue.bytes,
                    (unsigned long long)queue.countLimit,(unsigned long long)queue.byteLimit,
                    (unsigned long long)queue.queued,(unsigned long long)queue.active,
                    (unsigned long long)queue.undelivered,(unsigned long long)queue.retirementPending);
    }
    if(supervisor)for(const auto& worker:supervisor->snapshots())
        std::printf("snmp3 worker: address=%llu epoch=%llu pid=%lld ready=%u closing=%u batch=%llu launches=%llu reaps=%llu forced=%llu\n",
                    (unsigned long long)worker.address,(unsigned long long)worker.epoch,(long long)worker.pid,
                    worker.ready ? 1u:0u,worker.closing ? 1u:0u,(unsigned long long)worker.batch,
                    (unsigned long long)worker.spawnAttempts,(unsigned long long)worker.reaps,(unsigned long long)worker.forced);
}

void Runtime::prepare(const ConfigState& config)
{
    epicsGuard<epicsMutex> state(stateMutex);
    if(config.configuration->endpoints.empty()) { scheduler.reset(); return; }
    if(current.activation==ULONG_MAX)throw std::runtime_error("activation exhausted");
    const auto activation=current.state==State::Running ? current.activation:current.activation+1;
    if(scheduler && scheduler->activationId()==activation && scheduler->configurationRevision()==config.revision)return;
    auto owner=std::make_shared<Scheduler>(config.configuration,config.revision,activation);
    for(const auto& endpoint:config.configuration->endpoints) {
        const auto limits=prestartLimits.find(ipc::addressKey(endpoint.second.address));
        if(limits!=prestartLimits.end())owner->limits(endpoint.second.address,limits->second.first,limits->second.second);
    }
    scheduler=std::move(owner);
}
uint64_t Runtime::bind(const std::string& definition)
{
    return bind(Config::instance().bind(definition),0,0);
}
uint64_t Runtime::bind(std::shared_ptr<const Binding> binding,uint64_t retainedBytes,uint64_t generationBytes)
{
    epicsGuard<epicsMutex> operation(operationMutex);
    const auto state=snapshot().state;
    if(state==State::IncompleteStopped || state==State::Failed || state==State::Stopping)throw std::runtime_error("binding lifecycle rejected");
    prepare(Config::instance().snapshot());
    return schedulerOwner()->registerBinding(std::move(binding),retainedBytes,generationBytes);
}
std::shared_ptr<Scheduler> Runtime::schedulerOwner()
{ epicsGuard<epicsMutex> state(stateMutex); return scheduler; }
std::vector<ipc::Identity> Runtime::admit(const std::vector<uint64_t>& handles,const std::vector<Value>& payload,unsigned budgetMs)
{
    std::shared_ptr<Scheduler> owner;
    { epicsGuard<epicsMutex> state(stateMutex); if(current.state!=State::Running || !current.admission)throw std::runtime_error("runtime admission closed"); owner=scheduler; }
    if(!owner)throw std::runtime_error("runtime has no configured address");
    auto admitted=owner->admit(handles,payload,budgetMs,monotonicUs()); wake.trigger(); return admitted;
}
void Runtime::queueLimit(const std::string& address,uint64_t count,uint64_t bytes)
{
    if(count<1 || count>ipc::MaxCount || bytes<1 || bytes>ipc::MaxBytes)throw std::runtime_error("queue limits rejected");
    const auto key=ipc::addressKey(address); epicsGuard<epicsMutex> operation(operationMutex);
    const auto config=Config::instance().snapshot(); std::set<std::string> configured;
    for(const auto& endpoint:config.configuration->endpoints)configured.insert(ipc::addressKey(endpoint.second.address));
    if(!configured.count(key) || configured.size()>64)throw std::runtime_error("queue address rejected");
    auto next=prestartLimits;
    for(auto it=next.begin();it!=next.end();) { if(!configured.count(it->first))it=next.erase(it); else ++it; }
    next[key]={count,bytes}; auto owner=schedulerOwner(); if(owner)owner->limits(address,count,bytes);
    prestartLimits.swap(next); wake.trigger();
}
void Runtime::setQualification(const Qualification& mode)
{
    epicsGuard<epicsMutex> operation(operationMutex);
    if(mode.transportAddress || mode.transportParent!=-1 || mode.transportChild!=-1)
        throw std::runtime_error("runtime transport selector rejected");
    if(mode.stderrPath==qualification.stderrPath && mode.sanitizers==qualification.sanitizers)return;
    const auto state=snapshot().state;
    if((state!=State::Cold && state!=State::Stopped) || Config::instance().snapshot().frozen)
        throw std::runtime_error("qualification selector frozen");
    qualification=mode;
}
std::vector<SupervisionEvent> Runtime::takeSupervisionEvents()
{
    epicsGuard<epicsMutex> operation(operationMutex);
    return supervisor ? supervisor->takeEvents():std::vector<SupervisionEvent>();
}

void Runtime::fallback(void* argument)
{
    std::printf("snmp3 lifecycle: fallback\n");
    Runtime& self = *static_cast<Runtime*>(argument);
    self.stop();
    self.report();
}
}
