#include "Scheduler.h"
#include <cstdio>
#include <algorithm>
#include <functional>
#include <thread>
#include <utility>
#include <vector>
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
void consume(Scheduler& s, const ipc::Identity& id, ipc::Outcome expected)
{ auto terminal=s.take(id); check(terminal.result && terminal.result->outcome==expected); s.release(terminal.id); rejects([&]{s.release(terminal.id);}); }
void jointRelease()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"); s.limits("127.0.0.1",1,4608);
    const auto first=s.admit({a},{},1,1000).front(); auto d=s.dispatch(1,1000); check(d.commands.size()==1); s.transmitted(1,d.batch);
    check(s.expire(1,2000)); consume(s,first,ipc::Outcome::Deadline);
    auto held=s.snapshot(1); check(held.count==1 && held.bytes==4608 && held.retirementPending==1);
    rejects([&]{s.admit({a},{},100,2001);});
    s.limits("::ffff:127.0.0.1",1,1); check(s.snapshot(1).bytes==4608);
    check(s.complete(1,d.batch,results(d),2001)); auto ids=identities(d); auto bad=ids; ++bad[0].generation;
    check(!s.retired(1,d.batch,bad)); check(s.snapshot(1).count==1);
    check(s.retired(1,d.batch,ids)); check(s.settled());
    check(!s.retired(1,d.batch,ids)); s.limits("127.0.0.1",1,4608);
    const auto second=s.admit({a},{},100,3000).front(); d=s.dispatch(1,3000); check(s.complete(1,d.batch,results(d),3001));
    check(s.retired(1,d.batch,identities(d))); check(s.snapshot(1).count==1 && s.snapshot(1).undelivered==1);
    consume(s,second,ipc::Outcome::Complete); check(s.settled());
}
void fifo()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"),peer=handle(s,"Read2");
    auto ab=s.admit({a,b},{},100,1000); auto d=s.dispatch(1,1000); check(d.commands.size()==2 && d.commands[0].id.binding==a && d.commands[1].id.binding==b);
    check(s.dispatch(1,1000).commands.empty()); s.transmitted(1,d.batch);
    auto response=results(d); auto bad=response; ++bad[1].id.generation; check(!s.complete(1,d.batch,std::move(bad),1001)); check(s.snapshot(1).undelivered==0);
    auto malformed=response; malformed[1].value.push_back(0); rejects([&]{s.complete(1,d.batch,std::move(malformed),1001);}); check(s.snapshot(1).undelivered==0);
    check(s.complete(1,d.batch,std::move(response),1001)); check(s.retired(1,d.batch,identities(d))); consume(s,ab[0],ipc::Outcome::Complete); consume(s,ab[1],ipc::Outcome::Complete);
    const auto a2=s.admit({a},{},100,2000).front(),b2=s.admit({b},{},100,2001).front(),p2=s.admit({peer},{},100,2000).front();
    d=s.dispatch(1,2001); check(d.commands.size()==1 && d.commands[0].id.binding==a); check(s.dispatch(2,2001).commands.size()==1);
    s.stop(); s.reaped(1); s.reaped(2); consume(s,a2,ipc::Outcome::Stopping); consume(s,b2,ipc::Outcome::Stopping); consume(s,p2,ipc::Outcome::Stopping); check(s.settled());
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
    check(!s.expire(1,100001)); consume(s,ids[0],ipc::Outcome::Deadline); consume(s,ids[1],ipc::Outcome::Deadline); check(s.settled());
    rejects([&]{s.admit({a},{},0,1);}); rejects([&]{s.admit({a},{},600001,1);}); rejects([&]{s.admit({a},{},1,UINT64_MAX);});
}
void recovery()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"); const auto ids=s.admit({a,b},{},100,1000);
    auto first=s.dispatch(1,1000); s.workerLost(1,ipc::Outcome::WorkerFailure); check(s.snapshot(1).undelivered==0);
    s.reaped(1); auto recovered=s.dispatch(1,1001); check(identities(first)==identities(recovered) && recovered.commands[0].deadline==101000);
    s.transmitted(1,recovered.batch); s.workerLost(1,ipc::Outcome::ChannelFailure);
    consume(s,ids[0],ipc::Outcome::ChannelFailure); check(s.snapshot(1).count==2); s.reaped(1); check(s.snapshot(1).count==1);
    consume(s,ids[1],ipc::Outcome::ChannelFailure); check(s.settled() && s.dispatch(1,1002).commands.empty());
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
    const auto ids=s.admit({a,b},{payload,payload},100,1000);
    auto first=s.dispatch(1,1000); check(first.commands.size()==1);
    check(ipc::encodeCommands(first.commands).size()==1048640 && s.snapshot(1).count==2);
    s.workerLost(1,ipc::Outcome::ChannelFailure); s.reaped(1);
    s.stop(); consume(s,ids[0],ipc::Outcome::Stopping); consume(s,ids[1],ipc::Outcome::Stopping); check(s.settled());
}
void aliasesAndRace()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read0"); check(a!=b);
    s.limits("127.0.0.1",2,9216); const auto ids=s.admit({a,b},{},100,1000); auto d=s.dispatch(1,1000); check(d.commands.size()==2 && d.commands[0].definition==d.commands[1].definition);
    check(s.complete(1,d.batch,results(d),1001)); check(s.retired(1,d.batch,identities(d))); consume(s,ids[0],ipc::Outcome::Complete); consume(s,ids[1],ipc::Outcome::Complete);
    std::thread setter([&]{for(unsigned i=0;i<100;++i)s.limits("127.0.0.1",2,9216);});
    std::thread admission([&]{for(unsigned i=0;i<100;++i) {const auto id=s.admit({a},{},1,1000).front(); s.expire(1,2000); auto v=s.take(id); if(!v.result)throw std::runtime_error("missing terminal"); s.release(v.id);}});
    setter.join(); admission.join(); check(s.settled());
}
// Count and byte limits leave headroom so only the per-handle rule decides admission: a handle whose
// previous generation is consumed and awaits only retirement admits one new generation, which queues
// behind that retirement; an unconsumed or borrowed predecessor and a third generation are rejected.
void admissionBehindRetirement()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"); s.limits("127.0.0.1",8,65536);
    const auto initial=s.admit({a},{},100,1000).front(); auto d=s.dispatch(1,1000); s.transmitted(1,d.batch); check(s.complete(1,d.batch,results(d),1001));
    consume(s,initial,ipc::Outcome::Complete);
    const auto first=s.snapshot(1); check(first.count==1 && first.retirementPending==1);
    bool admitted=true; ipc::Identity next; try { next=s.admit({a},{},100,1002).front(); } catch(const std::exception&) { admitted=false; }
    std::printf("{\"event\":\"admission_behind_retirement\",\"admitted\":%s}\n",admitted?"true":"false");
    check(admitted);
    const auto both=s.snapshot(1);
    check(both.count==2 && both.bytes==2*first.bytes && both.retirementPending==1 && both.queued==1 &&
          both.behindRetirement==1 && both.behindAdmitted==1);
    check(s.take(initial).result==nullptr && s.take(next).result==nullptr);
    check(s.dispatch(1,1002).commands.empty());
    rejects([&]{s.admit({a},{},100,1003);});
    check(s.retired(1,d.batch,identities(d)));
    d=s.dispatch(1,1004); check(d.commands.size()==1 && d.commands[0].id.binding==a && d.commands[0].id.generation==2);
    s.transmitted(1,d.batch); check(s.complete(1,d.batch,results(d),1005)); check(s.retired(1,d.batch,identities(d)));
    consume(s,next,ipc::Outcome::Complete); check(s.settled());
    const auto other=s.admit({b},{},100,2000).front(); d=s.dispatch(1,2000); s.transmitted(1,d.batch); check(s.complete(1,d.batch,results(d),2001));
    rejects([&]{s.admit({b},{},100,2002);});
    auto borrowed=s.take(other); check(borrowed.result!=nullptr && borrowed.sent); rejects([&]{s.admit({b},{},100,2003);});
    s.release(borrowed.id); check(s.retired(1,d.batch,identities(d))); check(s.settled());
}
// A member answered before its deadline is not contained until its Retired frame is still missing
// one grace interval after the deadline; an unanswered member is contained at the deadline.
void containmentGrace()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0");
    const auto id=s.admit({a},{},100,1000).front(); auto d=s.dispatch(1,1000); s.transmitted(1,d.batch);
    check(s.complete(1,d.batch,results(d),100999));
    check(!s.expire(1,101000) && !s.expire(1,1100999) && s.expire(1,1101000));
    check(s.retired(1,d.batch,identities(d))); consume(s,id,ipc::Outcome::Complete); check(s.settled());
    const auto late=s.admit({a},{},100,2000000).front(); d=s.dispatch(1,2000000); s.transmitted(1,d.batch);
    check(s.expire(1,2100000)); check(s.complete(1,d.batch,results(d),2100001));
    check(s.retired(1,d.batch,identities(d))); consume(s,late,ipc::Outcome::Deadline); check(s.settled());
}
// With a consumed, retirement-pending predecessor and a queued successor on one handle, expire, stop
// and reaped must each act on the right generation: the successor expires or stops as never-sent work
// while the predecessor still waits for retirement, and a reap retires only the predecessor.
struct TwoGenerations {
    Scheduler s{configuration(),1,1}; uint64_t a=0; ipc::Identity first,second; Dispatch d;
    TwoGenerations(unsigned successorBudgetMs) {
        a=handle(s,"Read0"); s.limits("127.0.0.1",8,65536);
        first=s.admit({a},{},100,1000).front(); d=s.dispatch(1,1000); s.transmitted(1,d.batch);
        check(s.complete(1,d.batch,results(d),1001)); consume(s,first,ipc::Outcome::Complete);
        second=s.admit({a},{},successorBudgetMs,1002).front();
        const auto q=s.snapshot(1); check(q.count==2 && q.retirementPending==1 && q.queued==1);
    }
};
void twoGenerationLifecycle()
{
    {
        TwoGenerations t(1);
        check(!t.s.expire(1,2002));
        auto q=t.s.snapshot(1); check(q.retirementPending==1 && q.undelivered==1 && q.queued==0 && q.behindNeverSent==1);
        auto view=t.s.take(t.second); check(view.result && view.result->outcome==ipc::Outcome::Deadline && !view.sent);
        t.s.release(view.id); check(t.s.snapshot(1).count==1 && t.s.dispatch(1,2003).commands.empty());
        check(t.s.retired(1,t.d.batch,identities(t.d))); check(t.s.settled());
    }
    {
        TwoGenerations t(5000);
        t.s.stop();
        auto view=t.s.take(t.second); check(view.result && view.result->outcome==ipc::Outcome::Stopping && !view.sent);
        t.s.release(view.id); check(t.s.snapshot(1).count==1 && t.s.snapshot(1).retirementPending==1);
        t.s.reaped(1); check(t.s.settled());
    }
    {
        TwoGenerations t(5000);
        t.s.reaped(1);
        auto q=t.s.snapshot(1); check(q.count==1 && q.queued==1 && q.retirementPending==0);
        auto d=t.s.dispatch(1,1003); check(d.commands.size()==1 && d.commands[0].id==t.second);
        t.s.transmitted(1,d.batch); check(t.s.complete(1,d.batch,results(d),1004)); check(t.s.retired(1,d.batch,identities(d)));
        consume(t.s,t.second,ipc::Outcome::Complete); check(t.s.settled());
    }
}
// A Retired frame carrying the queued successor's identity, presented while the predecessor's batch
// is still active, must be rejected: validation is membership in the active batch, not presence in
// generation storage. The real frame then retires only the predecessor.
void forgedRetirement()
{
    TwoGenerations t(5000);
    check(!t.s.retired(1,t.d.batch,{t.second}));
    auto q=t.s.snapshot(1); check(q.count==2 && q.retirementPending==1 && q.queued==1 && t.s.dispatch(1,1003).commands.empty());
    check(t.s.retired(1,t.d.batch,identities(t.d)));
    auto d=t.s.dispatch(1,1004); check(d.commands.size()==1 && d.commands[0].id==t.second);
    t.s.transmitted(1,d.batch); check(t.s.complete(1,d.batch,results(d),1005)); check(t.s.retired(1,d.batch,identities(d)));
    consume(t.s,t.second,ipc::Outcome::Complete); check(t.s.settled());
}
// Containment grace applies to a member answered Complete or NativeFailure before its deadline; a result that
// arrives after the deadline is selected Deadline and its worker is contained without any grace.
void graceOutcomes()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0");
    const auto answered=s.admit({a},{},100,1000).front(); auto d=s.dispatch(1,1000); s.transmitted(1,d.batch);
    auto failure=results(d); failure[0].outcome=ipc::Outcome::NativeFailure; failure[0].nativeOutcome=2; failure[0].value.clear();
    check(s.complete(1,d.batch,failure,100999));
    check(!s.expire(1,101000) && !s.expire(1,1100999) && s.expire(1,1101000));
    check(s.retired(1,d.batch,identities(d))); consume(s,answered,ipc::Outcome::NativeFailure); check(s.settled());
    const auto late=s.admit({a},{},100,2000000).front(); d=s.dispatch(1,2000000); s.transmitted(1,d.batch);
    check(s.complete(1,d.batch,results(d),2100000) && s.expire(1,2100000));
    check(s.retired(1,d.batch,identities(d))); consume(s,late,ipc::Outcome::Deadline); check(s.settled());
}
// The never-sent counters follow only generations admitted behind a retirement: a first-generation request
// that expires in the queue is not counted, and the same expiry behind a predecessor is.
void behindClassification()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0"),b=handle(s,"Read1"); s.limits("127.0.0.1",8,65536);
    const auto plain=s.admit({b},{},1,1000).front();
    auto q=s.snapshot(1); check(q.behindAdmitted==0 && q.behindRetirement==0 && q.behindNeverSent==0);
    check(!s.expire(1,2000));
    q=s.snapshot(1); check(q.behindAdmitted==0 && q.behindRetirement==0 && q.behindNeverSent==0);
    consume(s,plain,ipc::Outcome::Deadline);
    const auto first=s.admit({a},{},100,3000).front(); auto d=s.dispatch(1,3000); s.transmitted(1,d.batch);
    check(s.complete(1,d.batch,results(d),3001)); consume(s,first,ipc::Outcome::Complete);
    const auto second=s.admit({a},{},1,3002).front();
    q=s.snapshot(1); check(q.behindAdmitted==1 && q.behindRetirement==1 && q.behindNeverSent==0);
    check(!s.expire(1,4002));
    q=s.snapshot(1); check(q.behindAdmitted==1 && q.behindNeverSent==1);
    check(s.retired(1,d.batch,identities(d))); consume(s,second,ipc::Outcome::Deadline); check(s.settled());
}
// A terminal is borrowed only by the exact identity that was issued, including its admission number.
void takeIdentity()
{
    Scheduler s(configuration(),1,1); auto a=handle(s,"Read0");
    const auto id=s.admit({a},{},100,1000).front(); auto d=s.dispatch(1,1000); s.transmitted(1,d.batch);
    check(s.complete(1,d.batch,results(d),1001));
    auto foreign=id; foreign.admission+=100; check(s.take(foreign).result==nullptr);
    auto next=id; next.generation+=1; check(s.take(next).result==nullptr);
    consume(s,id,ipc::Outcome::Complete); check(s.retired(1,d.batch,identities(d))); check(s.settled());
}
}

int main(int argc,char** argv)
{
    // One ordered list drives both modes: with no argument every cell runs in order; with exactly one
    // argument only that named cell runs, so a defective-code control can name the cell it must fail.
    const std::vector<std::pair<std::string,void(*)()>> cells={
        {"joint-release",jointRelease},{"fifo",fifo},{"bounds",bounds},{"recovery",recovery},
        {"frame-bounds",frameBounds},{"aliases-and-race",aliasesAndRace},
        {"admission-behind-retirement",admissionBehindRetirement},{"containment-grace",containmentGrace},
        {"two-generation-lifecycle",twoGenerationLifecycle},{"forged-retirement",forgedRetirement},
        {"grace-outcomes",graceOutcomes},{"behind-classification",behindClassification},{"take-identity",takeIdentity}};
    if(argc>2) { std::fprintf(stderr,"Usage: snmp3SchedulerTest [cell]\n"); return 2; }
    try {
        if(argc==2) {
            const auto cell=std::find_if(cells.begin(),cells.end(),[&](const std::pair<std::string,void(*)()>& c){return c.first==argv[1];});
            if(cell==cells.end()) { std::fprintf(stderr,"Unknown Scheduler cell: %s\n",argv[1]); return 2; }
            cell->second();
        } else {
            for(const auto& cell:cells)cell.second();
        }
        std::printf("Scheduler checks: %u\n",checks); return 0;
    }
    catch(const std::exception& e) { std::fprintf(stderr,"Scheduler failure after %u checks: %s\n",checks,e.what()); return 1; }
}
