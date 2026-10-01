#include "Config.h"
#include "Supervisor.h"
#include <cstdio>
#include <poll.h>
#include <set>

using namespace snmp3;
namespace {
unsigned checks=0;
void check(bool value) { ++checks; if(!value)throw std::runtime_error("supervisor assertion failed"); }
void events(Supervisor& owner)
{
    for(const auto& event:owner.takeEvents())
        std::printf("{\"event\":\"supervision\",\"code\":%u,\"at\":%llu,\"address\":%llu,\"epoch\":%llu,\"batch\":%llu,\"pid\":%lld,\"detail\":%lld}\n",
                    event.code,(unsigned long long)event.at,(unsigned long long)event.address,
                    (unsigned long long)event.epoch,(unsigned long long)event.batch,(long long)event.pid,(long long)event.detail);
}
bool stop(Supervisor& owner)
{
    owner.requestStop(); const auto end=ipc::add(monotonicUs(),2000000);
    do { owner.step(monotonicUs()); events(owner); if(owner.finished(monotonicUs()))break; poll(nullptr,0,1); } while(monotonicUs()<end);
    return owner.reconcile();
}
}
int main(int argc,char** argv)
{
    setvbuf(stdout,nullptr,_IOLBF,0);
    if(argc!=5)return 2;
    std::unique_ptr<Supervisor> owner;
    try {
        Config config; config.load(argv[1],argv[2]);
        const auto frozen=config.snapshot(); auto scheduler=std::make_shared<Scheduler>(frozen.configuration,frozen.revision,1);
        std::map<uint64_t,std::vector<uint64_t>> groups; std::map<uint64_t,std::string> specs;
        for(const auto& definition:frozen.configuration->bindings) {
            check(definition.second.operation==Operation::Get && definition.second.valueType==ValueType::Integer);
            const auto binding=config.bind(definition.first);
            uint64_t address=0;
            for(const auto id:scheduler->addresses())if(scheduler->key(id)==ipc::addressKey(binding->endpoint().address))address=id;
            check(address!=0);
            for(unsigned alias=0;alias<2;++alias) { const auto handle=scheduler->registerBinding(binding); groups[address].push_back(handle); specs[handle]=definition.first; }
        }
        Qualification qualification; qualification.stderrPath=argv[4];
#ifdef __SANITIZE_ADDRESS__
        qualification.sanitizers=true;
#endif
        owner.reset(new Supervisor(scheduler,argv[3],qualification));
        std::set<uint64_t> pending;
        for(const auto& group:groups) {
            scheduler->admit(group.second,{},5000,monotonicUs()); pending.insert(group.second.begin(),group.second.end());
            check(scheduler->snapshot(group.first).count==group.second.size());
        }
        const auto end=ipc::add(monotonicUs(),6000000);
        while(!pending.empty() && monotonicUs()<end) {
            owner->step(monotonicUs()); events(*owner);
            for(auto it=pending.begin();it!=pending.end();) {
                const auto terminal=scheduler->take(*it); if(!terminal.result) { ++it; continue; }
                check(terminal.result->outcome==ipc::Outcome::Complete && terminal.result->nativeOutcome==1);
                {
                    const auto& definition=frozen.configuration->bindings.at(specs.at(*it));
                    ipc::Reader reader(terminal.result->value); check(reader.oid()==definition.oid);
                    const auto value=ipc::decodeValue(reader,definition,false); reader.end(); check(value.integer()==-123);
                    std::printf("{\"event\":\"consumed\",\"binding\":%llu,\"generation\":%llu,\"admission\":%llu,\"value\":%lld}\n",
                                (unsigned long long)terminal.id.binding,(unsigned long long)terminal.id.generation,
                                (unsigned long long)terminal.id.admission,(long long)value.integer());
                }
                scheduler->release(terminal.id); it=pending.erase(it);
            }
            poll(nullptr,0,1);
        }
        check(pending.empty()); check(stop(*owner));
        for(const auto& status:owner->snapshots())check(!status.pid);
        check(scheduler->settled() && owner->droppedEvents()==0);
        std::printf("{\"event\":\"summary\",\"checks\":%u,\"addresses\":%zu,\"consumed\":%zu,\"settled\":true}\n",checks,groups.size(),specs.size()); return 0;
    } catch(const std::exception& error) {
        if(owner) { try { stop(*owner); } catch(...) {} }
        std::fprintf(stderr,"supervisor failure after %u checks: %s\n",checks,error.what()); return 1;
    }
}
