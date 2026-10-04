#include "Scheduler.h"
#include <cstdio>
#include <functional>
#include <thread>
#include <string>

using namespace snmp3;
namespace {
unsigned checks=0;
void check(bool value) { ++checks; if(!value)throw std::runtime_error("scheduler assertion failed"); }
void rejects(const std::function<void()>& f) { bool failed=false; try { f(); } catch(const std::exception&) { failed=true; } check(failed); }
std::shared_ptr<const Configuration> configuration()
{
    auto c=std::make_shared<Configuration>(); Profile p; p.id="Default"; p.community={'f'}; p.maxVarbinds=1024; c->profiles[p.id]=p;
    Endpoint e; e.id="Main"; e.profile=p.id; e.address="127.0.0.1"; c->endpoints[e.id]=e;
    e.id="Peer"; e.address="127.0.0.2"; c->endpoints[e.id]=e;
    for(unsigned i=0;i<3;++i) {
        BindingDefinition b; b.id="Read"+std::to_string(i); b.endpoint=i==2 ? "Peer":"Main"; b.oid={1,3,6,1,4,1,53864,1,0}; c->bindings[b.id]=b;
    }
    return c;
}
uint64_t handle(Scheduler& s, const std::string& id)
{ auto c=s.configuration(); return s.registerBinding(std::make_shared<const Binding>(c,c->bindings.at(id))); }
std::vector<ipc::Identity> identities(const Dispatch& d)
{ std::vector<ipc::Identity> ids; for(const auto& c:d.commands)ids.push_back(c.id); return ids; }
std::vector<ipc::Result> results(const Dispatch& d)
{
    std::vector<ipc::Result> v;
    for(const auto& c:d.commands) {
        ipc::Result r; r.id=c.id; r.outcome=ipc::Outcome::Complete; r.nativeOutcome=1;
        ipc::Writer w(ipc::DataBytes); w.oid({1,3,6,1,4,1,53864,1,0}); auto value=ipc::encodeValue(Value::integer(123)); w.raw(value.data(),value.size()); r.value=w.finish(); v.push_back(std::move(r));
    }
    return v;
}
void consume(Scheduler& s, uint64_t id, ipc::Outcome expected)
{ auto terminal=s.take(id); check(terminal.result && terminal.result->outcome==expected); s.release(terminal.id); rejects([&]{s.release(terminal.id);}); }
void jointRelease()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"); s.limits("127.0.0.1",1,4608);
    s.admit({a},{},1,1000); auto d=s.dispatch(1,1000); check(d.commands.size()==1); s.transmitted(1,d.batch);
    check(s.expire(1,2000)); consume(s,a,ipc::Outcome::Deadline);
    auto held=s.snapshot(1); check(held.count==1 && held.bytes==4608 && held.retirementPending==1);
    rejects([&]{s.admit({a},{},100,2001);});
    s.limits("::ffff:127.0.0.1",1,1); check(s.snapshot(1).bytes==4608);
    check(s.complete(1,d.batch,results(d),2001)); auto ids=identities(d); auto bad=ids; ++bad[0].generation;
    check(!s.retired(1,d.batch,bad)); check(s.snapshot(1).count==1);
    check(s.retired(1,d.batch,ids)); check(s.settled());
    check(!s.retired(1,d.batch,ids)); s.limits("127.0.0.1",1,4608);
    s.admit({a},{},100,3000); d=s.dispatch(1,3000); check(s.complete(1,d.batch,results(d),3001));
    check(s.retired(1,d.batch,identities(d))); check(s.snapshot(1).count==1 && s.snapshot(1).undelivered==1);
    consume(s,a,ipc::Outcome::Complete); check(s.settled());
}
void fifo()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"),peer=handle(s,"Read2");
    s.admit({a,b},{},100,1000); auto d=s.dispatch(1,1000); check(d.commands.size()==2 && d.commands[0].id.binding==a && d.commands[1].id.binding==b);
    check(s.dispatch(1,1000).commands.empty()); s.transmitted(1,d.batch);
    auto response=results(d); auto bad=response; ++bad[1].id.generation; check(!s.complete(1,d.batch,std::move(bad),1001)); check(s.snapshot(1).undelivered==0);
    auto malformed=response; malformed[1].value.push_back(0); rejects([&]{s.complete(1,d.batch,std::move(malformed),1001);}); check(s.snapshot(1).undelivered==0);
    check(s.complete(1,d.batch,std::move(response),1001)); check(s.retired(1,d.batch,identities(d))); consume(s,a,ipc::Outcome::Complete); consume(s,b,ipc::Outcome::Complete);
    s.admit({a},{},100,2000); s.admit({b},{},100,2001); s.admit({peer},{},100,2000);
    d=s.dispatch(1,2001); check(d.commands.size()==1 && d.commands[0].id.binding==a); check(s.dispatch(2,2001).commands.size()==1);
    s.stop(); s.reaped(1); s.reaped(2); consume(s,a,ipc::Outcome::Stopping); consume(s,b,ipc::Outcome::Stopping); consume(s,peer,ipc::Outcome::Stopping); check(s.settled());
}
void bounds()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"),peer=handle(s,"Read2");
    rejects([&]{s.admit({a,peer},{},100,1);}); rejects([&]{s.admit({a,a},{},100,1);}); check(s.snapshot(1).count==0);
    s.limits("127.0.0.1",2,9215); rejects([&]{s.admit({a,b},{},100,1);}); check(s.snapshot(1).count==0 && s.snapshot(1).bytes==0);
    s.limits("127.0.0.1",2,9216); auto ids=s.admit({a,b},{},100,1); check(ids.size()==2 && s.snapshot(1).bytes==9216);
    rejects([&]{s.admit({a},{},1,1);}); rejects([&]{s.limits("127.0.0.1",0,1);}); rejects([&]{s.limits("127.0.0.1",16385,1);});
    rejects([&]{s.limits("127.0.0.1",1,67108865);}); rejects([&]{s.limits("127.0.0.99",1,1);});
    check(s.snapshot(1).countLimit==2 && s.snapshot(1).byteLimit==9216);
    s.limits("127.0.0.1",1,1); check(s.snapshot(1).count==2);
    check(!s.expire(1,100001)); consume(s,a,ipc::Outcome::Deadline); consume(s,b,ipc::Outcome::Deadline); check(s.settled());
    rejects([&]{s.admit({a},{},0,1);}); rejects([&]{s.admit({a},{},600001,1);}); rejects([&]{s.admit({a},{},1,UINT64_MAX);});
}
void recovery()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"); s.admit({a,b},{},100,1000);
    auto first=s.dispatch(1,1000); s.workerLost(1,ipc::Outcome::WorkerFailure); check(s.snapshot(1).undelivered==0);
    s.reaped(1); auto recovered=s.dispatch(1,1001); check(identities(first)==identities(recovered) && recovered.commands[0].deadline==101000);
    s.transmitted(1,recovered.batch); s.workerLost(1,ipc::Outcome::ChannelFailure);
    consume(s,a,ipc::Outcome::ChannelFailure); check(s.snapshot(1).count==2); s.reaped(1); check(s.snapshot(1).count==1);
    consume(s,b,ipc::Outcome::ChannelFailure); check(s.settled() && s.dispatch(1,1002).commands.empty());
}
void frameBounds()
{
    auto config=std::make_shared<Configuration>(*configuration());
    auto& definition=config->bindings.at("Read0"); definition.operation=Operation::Set;
    definition.valueType=ValueType::Octets; definition.capacity=1048576;
    Scheduler s(config,1,1); const auto a=handle(s,"Read0"),b=handle(s,"Read0");
    s.limits("127.0.0.1",2,ipc::MaxBytes);
    rejects([&]{s.admit({a},{Value::octets(std::vector<uint8_t>(1048577,255))},100,1000);});
    check(s.snapshot(1).count==0 && s.snapshot(1).bytes==0);
    auto payload=Value::octets(std::vector<uint8_t>(1048576,255));
    s.admit({a,b},{payload,payload},100,1000);
    auto first=s.dispatch(1,1000); check(first.commands.size()==1);
    check(ipc::encodeCommands(first.commands).size()==1048640 && s.snapshot(1).count==2);
    s.workerLost(1,ipc::Outcome::ChannelFailure); s.reaped(1);
    s.stop(); consume(s,a,ipc::Outcome::Stopping); consume(s,b,ipc::Outcome::Stopping); check(s.settled());
}
void aliasesAndRace()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read0"); check(a!=b);
    s.limits("127.0.0.1",2,9216); s.admit({a,b},{},100,1000); auto d=s.dispatch(1,1000); check(d.commands.size()==2 && d.commands[0].definition==d.commands[1].definition);
    check(s.complete(1,d.batch,results(d),1001)); check(s.retired(1,d.batch,identities(d))); consume(s,a,ipc::Outcome::Complete); consume(s,b,ipc::Outcome::Complete);
    std::thread setter([&]{for(unsigned i=0;i<100;++i)s.limits("127.0.0.1",2,9216);});
    std::thread admission([&]{for(unsigned i=0;i<100;++i) {s.admit({a},{},1,1000); s.expire(1,2000); auto v=s.take(a); if(!v.result)throw std::runtime_error("missing terminal"); s.release(v.id);}});
    setter.join(); admission.join(); check(s.settled());
}
// Count and byte limits leave headroom so only the per-handle rule decides admission: a handle whose
// previous generation is consumed and awaits only retirement admits one new generation, which queues
// behind that retirement; an unconsumed or borrowed predecessor and a third generation are rejected.
void admissionBehindRetirement()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"); s.limits("127.0.0.1",8,65536);
    s.admit({a},{},100,1000); auto d=s.dispatch(1,1000); s.transmitted(1,d.batch); check(s.complete(1,d.batch,results(d),1001));
    consume(s,a,ipc::Outcome::Complete);
    const auto first=s.snapshot(1); check(first.count==1 && first.retirementPending==1);
    bool admitted=true; try { s.admit({a},{},100,1002); } catch(const std::exception&) { admitted=false; }
    std::printf("{\"event\":\"admission_behind_retirement\",\"admitted\":%s}\n",admitted?"true":"false");
    check(admitted);
    const auto both=s.snapshot(1);
    check(both.count==2 && both.bytes==2*first.bytes && both.retirementPending==1 && both.queued==1);
    check(s.dispatch(1,1002).commands.empty());
    rejects([&]{s.admit({a},{},100,1003);});
    check(s.retired(1,d.batch,identities(d)));
    d=s.dispatch(1,1004); check(d.commands.size()==1 && d.commands[0].id.binding==a && d.commands[0].id.generation==2);
    s.transmitted(1,d.batch); check(s.complete(1,d.batch,results(d),1005)); check(s.retired(1,d.batch,identities(d)));
    consume(s,a,ipc::Outcome::Complete); check(s.settled());
    s.admit({b},{},100,2000); d=s.dispatch(1,2000); s.transmitted(1,d.batch); check(s.complete(1,d.batch,results(d),2001));
    rejects([&]{s.admit({b},{},100,2002);});
    auto borrowed=s.take(b); check(borrowed.result!=nullptr); rejects([&]{s.admit({b},{},100,2003);});
    s.release(borrowed.id); check(s.retired(1,d.batch,identities(d))); check(s.settled());
}
}

int main(int argc,char** argv)
{
    try {
        if(argc==2 && std::string(argv[1])=="admission-behind-retirement") { admissionBehindRetirement(); std::printf("Scheduler checks: %u\n",checks); return 0; }
        jointRelease(); fifo(); bounds(); recovery(); frameBounds(); aliasesAndRace(); admissionBehindRetirement();
        std::printf("Scheduler checks: %u\n",checks); return 0;
    }
    catch(const std::exception& e) { std::fprintf(stderr,"Scheduler failure after %u checks: %s\n",checks,e.what()); return 1; }
}
