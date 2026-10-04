#include "Runtime.h"
#include "Config.h"
#include <cstdio>
#include <functional>
#include <set>
#include <thread>
#include <csignal>
#include <unistd.h>
#include <dbAccess.h>
#include <dbBase.h>
#include <dbUnitTest.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsUnitTest.h>
#include <initHooks.h>
#include <iocsh.h>

using namespace snmp3;
extern "C" int snmp3RuntimeTest_registerRecordDeviceDriver(dbBase*);
namespace {
unsigned checks=0,hooks=0;
std::map<uint64_t,pid_t> children;
std::map<uint64_t,uint64_t> sends;
void check(bool condition) { ++checks; if(!condition)throw std::runtime_error("IOC runtime assertion failed"); }
void rejects(const std::function<void()>& f)
{ bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; } check(rejected); }
std::string quoted(const std::string& text)
{ check(text.find_first_of("\"\\\n\r")==std::string::npos); return "\""+text+"\""; }
void command(const std::string& text,bool accepted=true)
{ check((iocshCmd(text.c_str())==0)==accepted); }
void hook(initHookState state)
{
    if(state!=initHookAfterInitialProcess)return;
    const auto current=Runtime::instance().snapshot();
    check(current.state==State::Running && current.admission); ++hooks;
    std::printf("{\"event\":\"ioc_initial_process_hook\",\"activation\":%lu,\"running\":true}\n",current.activation);
}
void events(Runtime& runtime)
{
    for(const auto& e:runtime.takeSupervisionEvents()) {
        if(e.code==2)children[e.address]=e.pid;
        if(e.code==14)++sends[e.address];
        std::printf("{\"event\":\"supervision\",\"code\":%u,\"at\":%llu,\"address\":%llu,\"epoch\":%llu,\"batch\":%llu,\"pid\":%lld,\"detail\":%lld}\n",
                    e.code,(unsigned long long)e.at,(unsigned long long)e.address,(unsigned long long)e.epoch,
                    (unsigned long long)e.batch,(long long)e.pid,(long long)e.detail);
    }
}
void consume(Runtime& runtime,const std::vector<ipc::Identity>& identities)
{
    auto scheduler=runtime.schedulerOwner(); std::vector<ipc::Identity> pending(identities.begin(),identities.end());
    const auto end=ipc::add(monotonicUs(),6000000);
    while(!pending.empty() && monotonicUs()<end) {
        events(runtime);
        for(auto it=pending.begin();it!=pending.end();) {
            const auto view=scheduler->take(*it); if(!view.result) { ++it; continue; }
            check(view.result->outcome==ipc::Outcome::Complete && view.result->nativeOutcome==1);
            scheduler->release(view.id); it=pending.erase(it);
        }
        epicsThreadSleep(0.001);
    }
    check(pending.empty());
    while(!scheduler->settled() && monotonicUs()<end) { events(runtime); epicsThreadSleep(0.001); }
    check(scheduler->settled());
}
}
extern "C" void snmp3RuntimeTestRegistrar() { initHookRegister(hook); }
epicsExportRegistrar(snmp3RuntimeTestRegistrar);

int main(int argc,char** argv)
{
    setvbuf(stdout,nullptr,_IOLBF,0);
    if(argc!=7)return 2;
    try {
        auto& runtime=Runtime::instance(); testPlan(4);
        for(unsigned cycle=0;cycle<2;++cycle) {
            testdbPrepare(); testdbReadDatabase(argv[5],nullptr,nullptr);
            check(snmp3RuntimeTest_registerRecordDeviceDriver(pdbbase)==0);
            if(cycle==0) {
                command("snmp3Load("+quoted(argv[1])+","+quoted(argv[2])+")");
                command("snmp3WorkerPath("+quoted(argv[3])+")");
                Qualification mode; mode.stderrPath=argv[4];
#ifdef __SANITIZE_ADDRESS__
                mode.sanitizers=true;
#endif
                runtime.setQualification(mode);
            }
            const auto config=Config::instance().snapshot();
            std::map<uint64_t,std::vector<uint64_t>> groups; std::vector<uint64_t> all;
            for(const auto& definition:config.configuration->bindings) {
                for(unsigned alias=0;alias<2;++alias) {
                    const auto handle=runtime.bind(definition.first); all.push_back(handle);
                    auto owner=runtime.schedulerOwner(); const auto& endpoint=config.configuration->endpoints.at(definition.second.endpoint);
                    for(const auto address:owner->addresses())if(owner->key(address)==ipc::addressKey(endpoint.address))groups[address].push_back(handle);
                }
            }
            command("snmp3WorkerPath("+quoted(argv[3])+")");
            command("snmp3WorkerPath(\"/invalid/different-worker\")",false);
            rejects([&]{runtime.admit({all.front()},{},1000);});
            const auto& first=config.configuration->endpoints.begin()->second;
            const auto limit="snmp3QueueLimit("+quoted(first.address)+",2,9344)";
            command(limit);
            testdbReadDatabase(argv[6],nullptr,"P=Runtime_"); testIocInitOk();
            testdbGetFieldEqual("Runtime_PiniProbe.VAL",DBR_LONG,42);
            testdbGetFieldEqual("Runtime_PiniProbe.PACT",DBR_LONG,0);
            check(hooks==cycle+1 && runtime.snapshot().activation==cycle+1);
            std::vector<ipc::Identity> admitted;
            for(const auto& group:groups)for(const auto& id:runtime.admit(group.second,{},5000))admitted.push_back(id);
            consume(runtime,admitted);
            auto owner=runtime.schedulerOwner(); const auto original=owner->snapshot(1);
            for(const auto& bad:{"4294967298,9344","0,9344","2,18446744073709551616","-1,9344"})
                command("snmp3QueueLimit("+quoted(first.address)+","+bad+")",false);
            check(owner->snapshot(1).countLimit==original.countLimit && owner->snapshot(1).byteLimit==original.byteLimit);
            command("snmp3QueueLimit("+quoted(first.address)+",1,1)");
            rejects([&]{runtime.admit(groups.at(1),{},5000);}); check(owner->snapshot(1).count==0);
            command(limit); consume(runtime,runtime.admit(groups.at(1),{},5000));
            if(cycle==0) {
                const auto held=runtime.admit({all.front()},{},5000).front();
                const auto end=ipc::add(monotonicUs(),6000000);
                while(owner->snapshot(1).undelivered!=1 && monotonicUs()<end) { events(runtime); epicsThreadSleep(0.001); }
                check(owner->snapshot(1).undelivered==1);
                runtime.stop(); events(runtime); const auto incomplete=runtime.snapshot();
                check(incomplete.state==State::IncompleteStopped && !incomplete.admission && incomplete.joined==1);
                std::printf("{\"event\":\"incomplete_stop\",\"count\":%llu,\"bytes\":%llu,\"joined\":%lu}\n",
                            (unsigned long long)owner->snapshot(1).count,(unsigned long long)owner->snapshot(1).bytes,incomplete.joined);
                check(!runtime.start() && runtime.snapshot().created==incomplete.created);
                runtime.stop(); check(runtime.snapshot().state==State::IncompleteStopped);
                const auto view=owner->take(held); check(view.result && view.result->outcome==ipc::Outcome::Complete);
                owner->release(view.id); runtime.stop(); check(runtime.snapshot().state==State::Stopped && owner->settled());
                std::printf("{\"event\":\"reconciled_stop\",\"joined\":%lu}\n",runtime.snapshot().joined);
            } else {
                events(runtime); const auto child=children.at(1); const auto sent=sends[1];
                check(child>0 && ::kill(child,SIGSTOP)==0);
                const auto stopped=runtime.admit({groups.at(1).front()},{},5000).front();
                const auto end=ipc::add(monotonicUs(),6000000);
                while(sends[1]==sent && monotonicUs()<end) { events(runtime); epicsThreadSleep(0.001); }
                check(sends[1]>sent);
                std::thread firstStop([&]{runtime.stop();});
                std::thread secondStop([&]{runtime.stop();});
                bool consumed=false,retained=false;
                while(!consumed && monotonicUs()<end) {
                    const auto view=owner->take(stopped);
                    if(view.result) {
                        consumed=view.result->outcome==ipc::Outcome::Stopping;
                        owner->release(view.id);
                        const auto queue=owner->snapshot(1);
                        retained=queue.count==1 && queue.bytes==4672 && queue.retirementPending==1 && ::kill(child,0)==0;
                    }
                    epicsThreadSleep(0.001);
                }
                firstStop.join(); secondStop.join(); events(runtime);
                check(consumed && retained && runtime.snapshot().joined==2 && owner->settled());
                std::printf("{\"event\":\"concurrent_stop\",\"joined\":2,\"consumed_retained\":true,\"pid\":%lld}\n",(long long)child);
            }
            testIocShutdownOk(); events(runtime);
            const auto stopped=runtime.snapshot(); check(stopped.state==State::Stopped && stopped.created==cycle+1 && stopped.exited==cycle+1 && stopped.joined==cycle+1);
            testdbCleanup();
        }
        check(testDone()==0);
        std::printf("{\"event\":\"ioc_runtime_summary\",\"checks\":%u,\"activations\":2,\"joined\":2}\n",checks);
        epicsExit(0);
    } catch(const std::exception& error) {
        Runtime::instance().stop(); std::fprintf(stderr,"IOC runtime failure after %u checks: %s\n",checks,error.what()); epicsExit(1);
    }
    return 1;
}
