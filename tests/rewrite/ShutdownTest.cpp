#include "Runtime.h"
#include "Request.h"

#include <aiRecord.h>
#include <aoRecord.h>
#include <calcRecord.h>
#include <dbAccess.h>
#include <dbLock.h>
#include <initHooks.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <atomic>
#include <cstdio>
#include <iomanip>
#include <set>
#include <sstream>

namespace {
const double ReadySeconds=10.0, WatchdogSeconds=30.0;
const int QueueSize=128;
struct Observer {
    epicsCallback blocker{};
    epicsEvent entered, release;
    std::atomic<bool> watchdog{false};
    std::mutex observationMutex;
    dbCommon* records[3]{};
    snmp3::RecordContext* contexts[2]{};
    std::set<uint64_t> results, retired;
    bool prepared=false, queued=false, beforeFree=false;
};
// Callback storage survives the real Base join and process-exit callbacks.
Observer& observer() { static auto* value=new Observer; return *value; }

void emit(const std::string& text)
{ std::printf("%s\n",text.c_str()); std::fflush(stdout); }

void collectTransport()
{
    auto& test=observer();
    for(const auto& event:snmp3::Runtime::instance().takeSupervisionEvents()) {
        if(event.code==4)test.results.insert(event.batch);
        if(event.code==5)test.retired.insert(event.batch);
        std::ostringstream out;
        out<<"{\"event\":\"shutdown_transport\",\"at_us\":"<<event.at
           <<",\"code\":"<<event.code<<",\"address\":"<<event.address
           <<",\"epoch\":"<<event.epoch<<",\"batch\":"<<event.batch
           <<",\"pid\":"<<event.pid<<",\"detail\":"<<event.detail<<"}";
        emit(out.str());
    }
}

void snapshot(const char* phase, bool ready=true, int restart=-1)
{
    auto& test=observer();
    std::lock_guard<std::mutex> observation(test.observationMutex);
    collectTransport();
    const auto state=snmp3::Runtime::instance().snapshot();
    const auto requests=snmp3::Requests::instance().snapshot();
    const auto owner=snmp3::Runtime::instance().schedulerOwner();
    callbackQueueStats stats{};
    const int status=callbackQueueStatus(0,&stats);
    std::ostringstream out;
    out<<std::setprecision(17)<<"{\"event\":\"shutdown_phase\",\"case\":\"retained\",\"phase\":\""<<phase
       <<"\",\"at_us\":"<<snmp3::monotonicUs()<<",\"ready\":"<<ready
       <<",\"watchdog\":"<<test.watchdog.load()<<",\"before_free\":"<<test.beforeFree
       <<",\"restart\":"<<restart<<",\"runtime\":{\"state\":\""<<snmp3::Runtime::stateName(state.state)
       <<"\",\"admission\":"<<state.admission<<",\"activation\":"<<state.activation
       <<",\"created\":"<<state.created<<",\"exited\":"<<state.exited<<",\"joined\":"<<state.joined<<"}"
       <<",\"requests\":{\"contexts\":"<<requests.contexts<<",\"active\":"<<requests.active
       <<",\"pending\":"<<requests.pending<<",\"queued\":"<<requests.queued
       <<",\"running\":"<<requests.running<<",\"inert\":"<<requests.inert
       <<",\"entered\":"<<requests.entered<<",\"enqueueFailures\":"<<requests.enqueueFailures
       <<",\"completions\":"<<requests.completions<<",\"entryOpen\":"<<requests.entryOpen
       <<",\"drainFailed\":"<<requests.drainFailed<<",\"detachAllowed\":"<<requests.detachAllowed<<"}"
       <<",\"queue_status\":"<<status<<",\"queue_size\":"<<stats.size
       <<",\"queue_used\":"<<stats.numUsed[priorityLow]<<",\"records\":[";
    for(unsigned i=0;i<3;++i) {
        if(i)out<<',';
        auto* record=test.records[i];
        if(!record) { out<<"null"; continue; }
        dbScanLock(record);
        const bool detached=record->dpvt==nullptr;
        const bool attached=i<2 && record->dpvt==test.contexts[i];
        const unsigned pact=record->pact;
        const double value=i==0 ? reinterpret_cast<aiRecord*>(record)->val :
                           i==1 ? reinterpret_cast<aoRecord*>(record)->val :
                                  reinterpret_cast<calcRecord*>(record)->val;
        dbScanUnlock(record);
        out<<"{\"dpvt_null\":"<<detached<<",\"attached\":"<<attached
           <<",\"pact\":"<<pact<<",\"value\":"<<value<<"}";
    }
    out<<"],\"contexts\":[";
    // Inventory is checked before saved-pointer access. The caller holds the
    // consumer, or the module has stopped and Base has joined that consumer.
    if(requests.contexts==2 && test.prepared && !test.watchdog.load() && !requests.running && !requests.entered) {
        for(unsigned i=0;i<2;++i) {
            if(i)out<<',';
            const auto* context=test.contexts[i];
            if(!context) { out<<"null"; continue; }
            out<<"{\"record_null\":"<<(context->record==nullptr)
               <<",\"attached\":"<<(context->record==test.records[i])
               <<",\"published\":"<<context->published<<",\"native_success\":"<<context->nativeSuccess
               <<",\"revision\":"<<context->owner->configurationRevision()
               <<",\"activation\":"<<context->owner->activationId()
               <<",\"binding\":"<<context->identity.binding<<",\"generation\":"<<context->identity.generation
               <<",\"admission\":"<<context->identity.admission
               <<",\"terminal\":"<<(context->terminal.result!=nullptr)
               <<",\"sent\":"<<context->terminal.sent
               <<",\"identity_match\":"<<(context->identity==context->terminal.id)
               <<",\"outcome\":"<<(context->terminal.result ? int(context->terminal.result->outcome):0)<<"}";
        }
    }
    out<<"],\"reservations\":[";
    bool first=true;
    if(owner)for(auto address:owner->addresses()) {
        const auto queue=owner->snapshot(address);
        if(!first)out<<',';
        first=false;
        out<<"{\"address\":"<<address<<",\"count\":"<<queue.count<<",\"bytes\":"<<queue.bytes
           <<",\"undelivered\":"<<queue.undelivered<<",\"retirement_pending\":"<<queue.retirementPending<<"}";
    }
    out<<"],\"result_batches\":"<<test.results.size()<<",\"retired_batches\":"<<test.retired.size()<<"}";
    emit(out.str());
}

void blocker(epicsCallback*)
{
    auto& test=observer();
    test.entered.signal();
    if(!test.release.wait(WatchdogSeconds)) {
        std::lock_guard<std::mutex> observation(test.observationMutex);
        test.watchdog=true;
    }
}

void hook(initHookState state)
{
    const char* phase=nullptr;
    switch(state) {
    case initHookAtShutdown: phase="AtShutdown"; break;
    case initHookAfterCloseLinks: phase="AfterCloseLinks"; break;
    case initHookAfterStopCallback: phase="AfterStopCallback"; break;
    case initHookBeforeFree: observer().beforeFree=true; phase="BeforeFree"; break;
    case initHookAfterShutdown: phase="AfterShutdown"; break;
    default: return;
    }
    try {
        snapshot(phase);
        if(state==initHookAfterShutdown) {
            const bool started=snmp3::Runtime::instance().start();
            snapshot("restart",true,started ? 1:0);
        }
    } catch(...) {
        emit("{\"event\":\"shutdown_observer_error\"}");
    }
    // Observation failures never prevent real Base shutdown or release a
    // production-owned context. Only the external callback hold is released.
    if(state==initHookAfterCloseLinks)observer().release.signal();
}

void setup(const iocshArgBuf* args)
{
    try {
        if(callbackParallelThreads(1,"low") || callbackSetQueueSize(QueueSize))
            throw std::runtime_error("callback setup failed");
        snmp3::Qualification mode;
        mode.stderrPath=args[0].sval ? args[0].sval:"";
        mode.sanitizers=args[1].ival!=0;
        snmp3::Runtime::instance().setQualification(mode);
        if(initHookRegister(hook))throw std::runtime_error("observer registration failed");
    } catch(...) { iocshSetError(1); }
}

void prepare(const iocshArgBuf*)
{
    try {
        auto& test=observer();
        const char* names[]={"Shutdown_Input","Shutdown_Output","Shutdown_Completed"};
        for(unsigned i=0;i<3;++i) {
            DBADDR address{};
            if(dbNameToAddr(names[i],&address))throw std::runtime_error("record missing");
            test.records[i]=address.precord;
            if(i<2) {
                dbScanLock(address.precord);
                test.contexts[i]=static_cast<snmp3::RecordContext*>(address.precord->dpvt);
                dbScanUnlock(address.precord);
            }
        }
        test.blocker.callback=blocker;
        test.blocker.priority=priorityLow;
        if(callbackRequest(&test.blocker) || !test.entered.wait(ReadySeconds))
            throw std::runtime_error("callback blocker not entered");
        test.prepared=true;
        snapshot("prepared");
    } catch(...) { observer().release.signal(); iocshSetError(1); }
}

void awaitQueued(const iocshArgBuf*)
{
    try {
        auto& test=observer();
        if(!test.prepared || test.queued)throw std::runtime_error("invalid observation sequence");
        const auto end=snmp3::monotonicUs()+uint64_t(ReadySeconds*1000000);
        bool ready=false;
        do {
            collectTransport();
            const auto counts=snmp3::Requests::instance().snapshot();
            ready=counts.contexts==2 && counts.queued==2 && counts.active==2 &&
                  !counts.entered && !counts.completions && test.results.size()==2 &&
                  test.retired==test.results;
            if(ready)break;
            epicsThreadSleep(0.001);
        } while(snmp3::monotonicUs()<end && !test.watchdog.load());
        test.queued=true;
        snapshot("queued",ready);
    } catch(...) { emit("{\"event\":\"shutdown_observer_error\"}"); }
}

const iocshArg stderrArg={"workerStderr",iocshArgString}, sanitizerArg={"sanitizers",iocshArgInt};
const iocshArg* const setupArgs[]={&stderrArg,&sanitizerArg};
const iocshFuncDef setupDef={"snmp3ShutdownSetup",2,setupArgs,"Register observer after the product registrar, before iocInit."};
const iocshFuncDef prepareDef={"snmp3ShutdownPrepare",0,nullptr,"Hold the actual low-priority callback consumer after iocInit."};
const iocshFuncDef awaitDef={"snmp3ShutdownAwait",0,nullptr,"Observe two real retired native results held in the Base queue."};
}

extern "C" void snmp3ShutdownTestRegistrar()
{
    iocshRegister(&setupDef,setup);
    iocshRegister(&prepareDef,prepare);
    iocshRegister(&awaitDef,awaitQueued);
}
epicsExportRegistrar(snmp3ShutdownTestRegistrar);
