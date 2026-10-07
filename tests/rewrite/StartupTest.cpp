#include "Runtime.h"
#include "Request.h"

#include <aiRecord.h>
#include <aoRecord.h>
#include <dbAccess.h>
#include <dbLock.h>
#include <initHooks.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace {
const unsigned RecordCount=4, ContextCount=2;
const uint64_t CompletionBudgetUs=10000000;
struct Observer {
    dbCommon* records[RecordCount]{};
    snmp3::RecordContext* contexts[ContextCount]{};
    bool failure=false, initialized=false, beforeFree=false;
};
Observer& observer() { static auto* value=new Observer; return *value; }
void emit(const std::string& text)
{ std::printf("%s\n",text.c_str()); std::fflush(stdout); }

void snapshot(const char* phase, int restart=-1)
{
    auto& test=observer();
    const auto state=snmp3::Runtime::instance().snapshot();
    const auto requests=snmp3::Requests::instance().snapshot();
    std::ostringstream out;
    out<<std::setprecision(17)<<"{\"event\":\"startup_phase\",\"phase\":\""<<phase
       <<"\",\"at_us\":"<<snmp3::monotonicUs()<<",\"before_free\":"<<test.beforeFree
       <<",\"restart\":"<<restart<<",\"state\":\""<<snmp3::Runtime::stateName(state.state)
       <<"\",\"admission\":"<<state.admission<<",\"activation\":"<<state.activation
       <<",\"created\":"<<state.created<<",\"exited\":"<<state.exited<<",\"joined\":"<<state.joined
       <<",\"inventory\":"<<requests.contexts<<",\"active\":"<<requests.active
       <<",\"pending\":"<<requests.pending<<",\"queued\":"<<requests.queued
       <<",\"entered\":"<<requests.entered<<",\"completions\":"<<requests.completions
       <<",\"entry_open\":"<<requests.entryOpen<<",\"detach_allowed\":"<<requests.detachAllowed
       <<",\"drain_failed\":"<<requests.drainFailed<<",\"records\":[";
    for(unsigned i=0;i<RecordCount;++i) {
        if(i)out<<',';
        auto* record=test.records[i];
        if(!record) { out<<"null"; continue; }
        dbScanLock(record);
        const double value=i%2==0 ? reinterpret_cast<aiRecord*>(record)->val :
                                   reinterpret_cast<aoRecord*>(record)->val;
        out<<"{\"dpvt_null\":"<<(record->dpvt==nullptr)
           <<",\"attached\":"<<(i<ContextCount && record->dpvt==test.contexts[i] && record->dpvt)
           <<",\"pact\":"<<unsigned(record->pact)<<",\"stat\":"<<record->stat
           <<",\"sevr\":"<<record->sevr<<",\"value\":"<<value<<"}";
        dbScanUnlock(record);
    }
    out<<"],\"contexts\":[";
    // During failure no producer exists; normal observations follow settled record work.
    // Early-release controls are detected by inventory before saved-pointer access.
    if(test.initialized && requests.contexts==ContextCount && !requests.active && !requests.entered) {
        for(unsigned i=0;i<ContextCount;++i) {
            if(i)out<<',';
            const auto* context=test.contexts[i];
            if(!context) { out<<"null"; continue; }
            out<<"{\"record_null\":"<<(context->record==nullptr)
               <<",\"attached\":"<<(context->record==test.records[i])
               <<",\"handle\":"<<context->handle<<",\"generation\":"<<context->identity.generation
               <<",\"admission\":"<<context->identity.admission
               <<",\"published\":"<<context->published<<",\"native_success\":"<<context->nativeSuccess
               <<",\"terminal\":"<<bool(context->terminal.result)<<"}";
        }
    }
    out<<"],\"reservations\":[";
    auto owner=snmp3::Runtime::instance().schedulerOwner();
    bool first=true;
    if(owner)for(auto address:owner->addresses()) {
        const auto q=owner->snapshot(address);
        if(!first)out<<',';
        first=false;
        out<<"{\"count\":"<<q.count<<",\"bytes\":"<<q.bytes
           <<",\"undelivered\":"<<q.undelivered<<",\"retirement_pending\":"<<q.retirementPending<<"}";
    }
    out<<"]}";
    emit(out.str());
}

void processRecords()
{
    auto& test=observer();
    for(unsigned i=0;i<ContextCount;++i) {
        auto* record=test.records[i];
        if(!record)throw std::runtime_error("startup record missing");
        dbScanLock(record);
        const long status=dbProcess(record);
        dbScanUnlock(record);
        std::ostringstream out;
        out<<"{\"event\":\"startup_process\",\"slot\":"<<i<<",\"status\":"<<status<<"}";
        emit(out.str());
    }
}

void initialize()
{
    auto& test=observer();
    const char* names[]={"Startup_Input","Startup_Output","Startup_RejectedInput","Startup_RejectedOutput"};
    for(unsigned i=0;i<RecordCount;++i) {
        DBADDR address{};
        if(dbNameToAddr(names[i],&address))throw std::runtime_error("startup record missing");
        test.records[i]=address.precord;
        if(i<ContextCount)test.contexts[i]=static_cast<snmp3::RecordContext*>(address.precord->dpvt);
    }
    test.initialized=true;
}

void hook(initHookState state)
{
    try {
        switch(state) {
        case initHookAfterInitDatabase: initialize(); snapshot("initialized"); break;
        case initHookAfterFinishDevSup: snapshot("started"); break;
        case initHookAfterInitialProcess:
            if(observer().failure) {
                processRecords(); snapshot("attempted");
                snmp3::Runtime::instance().stop(); snapshot("stop1");
                snmp3::Runtime::instance().stop(); snapshot("stop2");
            }
            break;
        case initHookAtShutdown: snapshot("AtShutdown"); break;
        case initHookAfterCloseLinks: snapshot("AfterCloseLinks"); break;
        case initHookAfterStopCallback: snapshot("AfterStopCallback"); break;
        case initHookBeforeFree: observer().beforeFree=true; snapshot("BeforeFree"); break;
        case initHookAfterShutdown:
            snapshot("AfterShutdown");
            if(observer().failure) {
                const bool started=snmp3::Runtime::instance().start();
                snapshot("restart",started ? 1:0);
            }
            break;
        default: break;
        }
    } catch(...) { emit("{\"event\":\"startup_observer_error\"}"); }
}

void setup(const iocshArgBuf* args)
{
    try {
        observer().failure=args[0].ival!=0;
        snmp3::Qualification mode;
        mode.stderrPath=args[1].sval ? args[1].sval:"";
        mode.sanitizers=args[2].ival!=0;
        snmp3::Runtime::instance().setQualification(mode);
        if(initHookRegister(hook))throw std::runtime_error("startup hook registration failed");
    } catch(...) { iocshSetError(1); }
}
void exercise(const iocshArgBuf*)
{
    try {
        if(observer().failure || !observer().initialized)throw std::runtime_error("startup exercise mode rejected");
        processRecords();
        const auto deadline=snmp3::monotonicUs()+CompletionBudgetUs;
        for(;;) {
            const auto q=snmp3::Requests::instance().snapshot();
            const auto owner=snmp3::Runtime::instance().schedulerOwner();
            if(q.completions==ContextCount && !q.active && !q.entered && owner && owner->settled())break;
            if(snmp3::monotonicUs()>=deadline)throw std::runtime_error("startup completion timeout");
            epicsThreadSleep(0.001);
        }
        snapshot("completed");
    } catch(...) { emit("{\"event\":\"startup_observer_error\"}"); iocshSetError(1); }
}
const iocshArg failureArg={"failure",iocshArgInt}, stderrArg={"workerStderr",iocshArgString},
                  sanitizerArg={"sanitizers",iocshArgInt};
const iocshArg* const setupArgs[]={&failureArg,&stderrArg,&sanitizerArg};
const iocshFuncDef setupDef={"snmp3StartupSetup",3,setupArgs,"Observe startup after product registration and before iocInit."};
const iocshFuncDef exerciseDef={"snmp3StartupExercise",0,nullptr,"Process valid records and observe settled native completions."};
}
extern "C" void snmp3StartupTestRegistrar()
{
    iocshRegister(&setupDef,setup);
    iocshRegister(&exerciseDef,exercise);
}
epicsExportRegistrar(snmp3StartupTestRegistrar);
