#include "Config.h"
#include "Supervisor.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <poll.h>
#include <set>
#include <thread>
#include <sys/resource.h>
#include <unistd.h>

using namespace snmp3;
namespace {
unsigned checks=0;
void check(bool value,const char* name)
{ ++checks; if(!value)throw std::runtime_error(name); }
void rejects(const std::function<void()>& call)
{ bool failed=false; try { call(); } catch(const std::exception&) { failed=true; } check(failed,"rejection absent"); }
// A timer that came due during the last 1 ms wait is acted on by the next step; one that is older than
// that stays in the past because it cannot act, which makes the servicing loop spin.
bool notStale(uint64_t timer) { return timer==UINT64_MAX || timer+5000>monotonicUs(); }
volatile sig_atomic_t interrupts=0;
void interrupt(int) { ++interrupts; }
struct Fixture {
    Config config;
    std::shared_ptr<Scheduler> scheduler;
    std::unique_ptr<Supervisor> owner;
    std::map<std::string,std::vector<uint64_t>> handles;
    std::map<uint64_t,const BindingDefinition*> specs;
    std::vector<SupervisionEvent> observed;
    std::mutex admittedMutex;
    std::map<uint64_t,std::deque<ipc::Identity>> admitted;
    // Records each admitted identity per handle so terminals are taken by exact identity in FIFO order.
    std::vector<ipc::Identity> admit(const std::vector<uint64_t>& ids,const std::vector<Value>& values,unsigned budgetMs,uint64_t nowUs) {
        auto result=scheduler->admit(ids,values,budgetMs,nowUs);
        std::lock_guard<std::mutex> guard(admittedMutex);
        for(const auto& id:result)admitted[id.binding].push_back(id);
        return result;
    }
    TerminalView take(uint64_t handle) {
        std::lock_guard<std::mutex> guard(admittedMutex);
        auto it=admitted.find(handle); if(it==admitted.end() || it->second.empty())return TerminalView();
        const auto view=scheduler->take(it->second.front()); if(view.result)it->second.pop_front();
        return view;
    }
    Fixture(char** argv) {
        config.load(argv[1],argv[2]); const auto frozen=config.snapshot();
        scheduler=std::make_shared<Scheduler>(frozen.configuration,frozen.revision,1);
        for(const auto& definition:frozen.configuration->bindings) {
            const auto binding=config.bind(definition.first);
            for(unsigned i=0;i<4;++i) {
                const auto id=scheduler->registerBinding(binding); handles[definition.first].push_back(id);
                specs[id]=&definition.second;
            }
        }
        if(std::string(argv[5])=="immutable") {
            std::printf("{\"event\":\"immutable_snapshot\"}\n");
            const auto end=ipc::add(monotonicUs(),5000000);
            while(access(argv[6],F_OK)!=0 && monotonicUs()<end)poll(nullptr,0,1);
            check(access(argv[6],F_OK)==0,"immutable file coordination absent");
        }
        Qualification mode; mode.stderrPath=argv[4];
        #ifdef __SANITIZE_ADDRESS__
        mode.sanitizers=true;
        #endif
        if(std::string(argv[5]).find("ipc-")==0) {
            mode.transportAddress=1; mode.transportParent=std::stoi(argv[6]); mode.transportChild=std::stoi(argv[7]);
        }
        owner.reset(new Supervisor(scheduler,argv[3],mode));
        if(mode.transportAddress) { close(mode.transportParent); close(mode.transportChild); }
    }
    ~Fixture() {
        if(!owner)return;
        try {
            owner->requestStop(); const auto end=ipc::add(monotonicUs(),2500000);
            do {
                tick();
                for(const auto& entry:specs) { const auto view=take(entry.first); if(view.result)scheduler->release(view.id); }
                if(owner->reconcile())return;
            } while(monotonicUs()<end);
        } catch(...) {}
    }
    void tick() {
        owner->step(monotonicUs());
        for(const auto& e:owner->takeEvents()) {
            check(observed.size()<32768,"test event capacity exceeded"); observed.push_back(e);
            std::printf("{\"event\":\"supervision\",\"code\":%u,\"at\":%llu,\"address\":%llu,\"epoch\":%llu,\"batch\":%llu,\"pid\":%lld,\"detail\":%lld}\n",
                        e.code,(unsigned long long)e.at,(unsigned long long)e.address,(unsigned long long)e.epoch,
                        (unsigned long long)e.batch,(long long)e.pid,(long long)e.detail);
        }
        poll(nullptr,0,1);
    }
    void until(const std::function<bool()>& predicate,unsigned milliseconds=6000) {
        const auto end=ipc::add(monotonicUs(),ipc::multiply(milliseconds,1000));
        bool met=predicate();
        while(!met && monotonicUs()<end) { tick(); met=predicate(); }
        check(met,"actual progress deadline exceeded");
    }
    WorkerSnapshot worker(uint64_t address) {
        for(const auto& status:owner->snapshots())if(status.address==address)return status;
        throw std::runtime_error("worker address absent");
    }
    void ready() {
        until([&]{const auto all=owner->snapshots(); return std::all_of(all.begin(),all.end(),[](const WorkerSnapshot& s){return s.ready;});});
    }
    void signal(uint64_t address,int number) {
        const auto status=worker(address); check(status.pid>0,"signal requires owned worker");
        check(::kill(status.pid,number)==0,"owned worker signal failed");
        std::printf("{\"event\":\"signal\",\"pid\":%lld,\"number\":%d,\"at\":%llu}\n",
                    (long long)status.pid,number,(unsigned long long)monotonicUs());
    }
    Value read(uint64_t handle,ipc::Outcome outcome=ipc::Outcome::Complete,int64_t integer=INT64_MIN,uint16_t native=0) {
        TerminalView view;
        until([&]{view=take(handle); return view.result;});
        check(view.result->outcome==outcome || (outcome==ipc::Outcome::ChannelFailure && view.result->outcome==ipc::Outcome::WorkerFailure),"unexpected terminal outcome");
        if(native)check(view.result->nativeOutcome==native,"unexpected native outcome");
        auto value=Value::integer(0);
        if(outcome==ipc::Outcome::Complete) {
            const auto& definition=*specs.at(handle); ipc::Reader reader(view.result->value);
            check(reader.oid()==definition.oid,"response OID mismatch");
            value=ipc::decodeValue(reader,definition,false); reader.end();
            if(integer!=INT64_MIN)check(value.integer()==integer,"real agent integer mismatch");
        }
        std::printf("{\"event\":\"consumed\",\"binding\":%llu,\"generation\":%llu,\"admission\":%llu,\"outcome\":%u}\n",
                    (unsigned long long)view.id.binding,(unsigned long long)view.id.generation,
                    (unsigned long long)view.id.admission,unsigned(view.result->outcome));
        scheduler->release(view.id); return value;
    }
    void settle() { until([&]{return scheduler->settled();}); }
    void stop() {
        owner->requestStop(); until([&]{return owner->finished(monotonicUs());},2500);
        check(owner->reconcile(),"stop ownership incomplete");
        check(scheduler->settled() && !owner->droppedEvents(),"stop lost ownership or events");
    }
};
Value expected(const BindingDefinition& definition,const Profile& profile)
{
    const auto index=definition.oid.at(8);
    const unsigned context=profile.contextName=="alpha" ? 1:profile.contextName=="beta" ? 2:0;
    if(index==13)return Value::exception(ValueType::NoSuchObject);
    if(index==14)return Value::exception(ValueType::NoSuchInstance);
    if(index==15)return Value::exception(ValueType::EndOfMibView);
    switch(definition.valueType) {
    case ValueType::Integer:return Value::integer(-123-int64_t(context));
    case ValueType::Unsigned32: case ValueType::Counter32: case ValueType::Gauge32: case ValueType::TimeTicks:
        return Value::unsigned32(definition.valueType,UINT32_MAX);
    case ValueType::Counter64:return Value::counter64(UINT64_MAX);
    case ValueType::Octets:return Value::octets({'A',0,'B',255,uint8_t(context)});
    case ValueType::ObjectId:return Value::objectId({1,3,6,1,4,1,53864});
    case ValueType::IpAddress:return Value::ipAddress({{127,0,0,1}});
    case ValueType::OpaqueFloat:return Value::opaqueFloat(-2.25f);
    case ValueType::OpaqueDouble:return Value::opaqueDouble(1.0/3.0);
    default:throw std::runtime_error("unexpected fixture type");
    }
}
void typed(Fixture& f)
{
    std::vector<uint64_t> reads,sets; std::vector<Value> values;
    const auto config=f.scheduler->configuration();
    for(const auto& definition:config->bindings) {
        const auto handle=f.handles.at(definition.first).front();
        const auto& profile=config->profiles.at(config->endpoints.at(definition.second.endpoint).profile);
        if(definition.second.operation==Operation::Get)reads.push_back(handle);
        else { sets.push_back(handle); values.push_back(expected(definition.second,profile)); }
    }
    f.admit(reads,{},10000,monotonicUs());
    for(const auto handle:reads) {
        const auto& definition=*f.specs.at(handle); const auto& profile=config->profiles.at(config->endpoints.at(definition.endpoint).profile);
        const auto value=f.read(handle);
        check(ipc::encodeValue(value)==ipc::encodeValue(expected(definition,profile)),"typed owned value mismatch");
    }
    f.settle();
    f.admit(sets,values,10000,monotonicUs());
    for(auto& value:values)value=Value::integer(987);
    for(const auto handle:sets) {
        const auto& definition=*f.specs.at(handle); const auto& profile=config->profiles.at(config->endpoints.at(definition.endpoint).profile);
        const auto value=f.read(handle); check(ipc::encodeValue(value)==ipc::encodeValue(expected(definition,profile)),"caller mutation reached SET");
    }
    f.settle(); f.admit(reads,{},10000,monotonicUs());
    for(const auto handle:reads) {
        const auto& definition=*f.specs.at(handle); const auto& profile=config->profiles.at(config->endpoints.at(definition.endpoint).profile);
        check(ipc::encodeValue(f.read(handle))==ipc::encodeValue(expected(definition,profile)),"real SET readback mismatch");
    }
    f.settle();
}
void fifo(Fixture& f)
{
    const auto ids=f.handles.at("Address4Read"); const auto origin=monotonicUs();
    f.admit({ids[0]},{},5000,origin);
    f.admit({ids[1]},{},5000,ipc::add(origin,1));
    f.admit({ids[2],ids[3]},{},5000,ipc::add(origin,2));
    for(auto handle:ids)f.read(handle,ipc::Outcome::Complete,-123);
    f.settle();
    check(std::count_if(f.observed.begin(),f.observed.end(),[](const SupervisionEvent& e){return e.address==1 && e.code==13;})==3,"unequal deadlines batched");
}
void deadline(Fixture& f)
{
    const auto a=f.handles.at("Address4Read"),b=f.handles.at("Address6Read"); f.signal(1,SIGSTOP);
    f.admit({a[0],a[1]},{},100,monotonicUs());
    f.admit({b[0],b[1]},{},5000,monotonicUs());
    for(unsigned i=0;i<2;++i)f.read(a[i],ipc::Outcome::Deadline);
    auto pending=f.scheduler->snapshot(1); check(pending.count==2 && pending.retirementPending==2,"early consumption released live Q");
    f.scheduler->limits("127.0.0.1",1,1);
    rejects([&]{f.admit({a[2]},{},5000,monotonicUs());});
    check(f.scheduler->snapshot(1).bytes==pending.bytes,"limit decrease released live Q");
    for(unsigned i=0;i<2;++i)f.read(b[i],ipc::Outcome::Complete,-123);
    check(f.worker(1).pid>0 && f.worker(2).ready,"blocked address starved peer or reaped early");
    f.until([&]{return f.scheduler->snapshot(1).count==0;},2500);
    check(std::any_of(f.observed.begin(),f.observed.end(),[](const SupervisionEvent& e){return e.address==1 && e.code==9;}),"blocked worker KILL absent");
    f.scheduler->limits("127.0.0.1",2,9344); f.admit({a[0]},{},5000,monotonicUs());
    f.read(a[0],ipc::Outcome::Complete,-123); f.settle();
}
void unsent(Fixture& f)
{
    const auto ids=f.handles.at("Address4Read"); const auto accepted=f.admit({ids[0]},{},5000,monotonicUs());
    // Launch queues Bootstrap only. Killing before another step preserves zero Batch bytes.
    f.tick(); f.signal(1,SIGKILL);
    f.read(ids[0],ipc::Outcome::Complete,-123); f.settle();
    check(f.worker(1).epoch==2 && accepted[0].generation==1 && accepted[0].admission==1,"unsent recovery identity changed");
}
void ambiguous(Fixture& f,const char* marker)
{
    const auto id=f.handles.at("Address4Set").front(); const auto read=f.handles.at("Address4Read").front();
    f.admit({id},{Value::integer(777)},5000,monotonicUs());
    f.until([&]{return std::any_of(f.observed.begin(),f.observed.end(),[](const SupervisionEvent& e){return e.address==1 && e.code==14;});});
    std::printf("{\"event\":\"await_loss\",\"pid\":%lld}\n",(long long)f.worker(1).pid);
    const auto end=ipc::add(monotonicUs(),5000000);
    while(access(marker,F_OK)!=0 && monotonicUs()<end) { f.tick(); }
    check(access(marker,F_OK)==0,"actual SET loss coordination absent");
    const auto view=f.take(id);
    if(!view.result)f.read(id,ipc::Outcome::WorkerFailure);
    else { check(view.result->outcome==ipc::Outcome::WorkerFailure,"ambiguous SET wrong outcome"); f.scheduler->release(view.id); }
    f.admit({read},{},5000,monotonicUs()); f.read(read,ipc::Outcome::Complete,777); f.settle();
}
void crashes(Fixture& f)
{
    for(unsigned i=0;i<4;++i) {
        f.until([&]{return f.worker(1).pid>0;}); f.signal(1,SIGKILL);
        f.until([&]{return f.worker(1).reaps==i+1;});
    }
    const auto id=f.handles.at("Address4Read").front(); f.admit({id},{},100,monotonicUs());
    f.read(id,ipc::Outcome::Deadline); f.settle();
    check(f.worker(1).spawnAttempts==4 && f.worker(1).pid==0,"launch rate exhausted incorrectly");
    const auto peer=f.handles.at("Address6Read").front(); f.admit({peer},{},5000,monotonicUs());
    f.read(peer,ipc::Outcome::Complete,-123); f.settle();
    std::vector<uint64_t> launches,reaps;
    for(const auto& e:f.observed)if(e.address==1) { if(e.code==2)launches.push_back(e.at); if(e.code==7)reaps.push_back(e.at); }
    check(launches.size()==4 && reaps.size()==4,"rate evidence absent");
    for(size_t i=1;i<4;++i)check(launches[i]>=ipc::add(reaps[i-1],uint64_t(250000)<<(i-1)),"restart backoff too short");
}
void nativeDeadline(Fixture& f)
{
    const auto a=f.handles.at("Address4Read"),b=f.handles.at("Address6Read");
    f.admit({a[0],a[1]},{},100,monotonicUs());
    f.admit({a[2]},{},50,monotonicUs());
    f.admit({b[0],b[1]},{},5000,monotonicUs());
    f.read(a[2],ipc::Outcome::Deadline);
    f.until([&]{return f.scheduler->snapshot(1).undelivered==2;});
    check(f.scheduler->snapshot(1).count==2,"unconsumed deadline released Q");
    f.read(a[0],ipc::Outcome::Deadline); f.read(a[1],ipc::Outcome::Deadline);
    const auto config=f.scheduler->configuration();
    if(config->profiles.at(config->endpoints.at("Address4").profile).version==Version::V3)
        check(f.scheduler->snapshot(1).count==2 && f.scheduler->snapshot(1).retirementPending==2 && f.worker(1).pid>0,"blocked discovery released consumed Q");
    f.read(b[0],ipc::Outcome::Complete,-123); f.read(b[1],ipc::Outcome::Complete,-123); f.settle();
    check(std::count_if(f.observed.begin(),f.observed.end(),[](const SupervisionEvent& e){return e.code==13 && e.address==1;})==1,"expired queued batch dispatched");
}
void ipcFault(Fixture& f,const std::string& mode)
{
    const auto a=f.handles.at(f.handles.count("Address4Set") ? "Address4Set":"Address4Read").front(),b=f.handles.at("Address6Read").front();
    f.admit({a},f.handles.count("Address4Set") ? std::vector<Value>{Value::integer(777)}:std::vector<Value>{},5000,monotonicUs());
    f.admit({b},{},5000,monotonicUs());
    if(mode=="ipc-partial-timeout") {
        // A frame that stops after one byte arms the partial-frame limit of a ready worker (1 s), well
        // before the 5 s request deadline, and the supervisor learns it without any further event.
        const auto contained=[&]{ return std::any_of(f.observed.begin(),f.observed.end(),
                                      [](const SupervisionEvent& e){return e.address==1 && e.code==6;}); };
        f.until([&]{ const auto now=monotonicUs(); return contained() || f.owner->nextTimer()<now+1100000; },3000);
        check(!contained(),"partial-frame limit was not an event timer before the containment");
        const auto now=monotonicUs(),timer=f.owner->nextTimer();
        check(timer>now && timer<=now+1000000,"partial-frame timer outside the ready-worker limit");
        // The plan's number: the partial-frame containment follows its timer within 50 ms.
        f.until([&]{return contained();},3000);
        uint64_t at=0;
        for(const auto& e:f.observed)if(e.address==1 && e.code==6) { at=e.at; break; }
        check(at>=timer && at<=timer+50000,"partial-frame limit acted later than 50 ms after its timer");
        std::printf("{\"event\":\"timer_lateness\",\"case\":\"partial-frame\",\"late_us\":%llu}\n",
                    (unsigned long long)(at-timer));
    }
    f.read(a,ipc::Outcome::ChannelFailure); f.read(b,ipc::Outcome::Complete,-123); f.settle();
}
void fullChannel(Fixture& f)
{
    const auto a=f.handles.at("Address4Set").front(),b=f.handles.at("Address6Read").front();
    f.scheduler->limits("127.0.0.1",2,ipc::MaxBytes);
    f.admit({a},{Value::octets(std::vector<uint8_t>(1048576,255))},100,monotonicUs());
    f.admit({b},{},5000,monotonicUs());
    // The reader has stopped, so the channel keeps unsent bytes and the loop must wait for write readiness.
    f.until([&]{ for(const auto& d:f.owner->descriptors())if(d.write)return true; return false; },3000);
    f.read(a,ipc::Outcome::Deadline);
    check(f.scheduler->snapshot(1).count==1 && f.scheduler->snapshot(1).retirementPending==1,"full IPC released live Q");
    f.read(b,ipc::Outcome::Complete,-123); f.settle();
}
void bootstrapFault(Fixture& f)
{
    const auto a=f.handles.at("Address4Read").front(),b=f.handles.at("Address6Read").front();
    f.admit({a},{},500,monotonicUs()); f.admit({b},{},5000,monotonicUs());
    f.read(a,ipc::Outcome::Deadline); f.read(b,ipc::Outcome::Complete,-123); f.settle();
    check(f.worker(1).reaps==1,"rejected bootstrap worker not reaped");
    check(std::none_of(f.observed.begin(),f.observed.end(),[](const SupervisionEvent& e){return e.address==1 && (e.code==3 || e.code==13);}),"mismatched product dispatched application work");
}
void securityConflict(Fixture& f)
{
    const auto good=f.handles.at("GoodRead").front(),bad=f.handles.at("BadRead").front();
    f.admit({good},{},5000,monotonicUs()); f.read(good,ipc::Outcome::Complete,-123); f.settle();
    f.admit({bad},{},5000,monotonicUs()); f.read(bad,ipc::Outcome::NativeFailure,INT64_MIN,7); f.settle();
    f.admit({good},{},5000,monotonicUs()); f.read(good,ipc::Outcome::Complete,-123); f.settle();
    check(f.worker(1).epoch==1 && f.worker(1).spawnAttempts==1,"security conflict restarted healthy worker");
}
void liveRace(Fixture& f)
{
    const std::vector<uint64_t> handles={f.handles.at("Address4Read").front(),f.handles.at("Address6Read").front()};
    std::atomic<bool> halt(false); std::atomic<unsigned> accepted(0),rejected(0),finished(0);
    std::vector<std::thread> threads;
    struct Join {
        std::atomic<bool>& halt; std::vector<std::thread>& threads;
        ~Join() { halt=true; for(auto& thread:threads)if(thread.joinable())thread.join(); }
    } join{halt,threads};
    for(const auto handle:handles)threads.emplace_back([&,handle]{
        unsigned count=0;
        while(count<32 && !halt) {
            try { f.admit({handle},{},5000,monotonicUs()); ++count; ++accepted; }
            catch(const std::runtime_error&) { ++rejected; }
            poll(nullptr,0,1);
        }
        ++finished;
    });
    threads.emplace_back([&]{
        while(!halt && finished<2) {
            f.scheduler->limits("127.0.0.1",1,1); poll(nullptr,0,1);
            f.scheduler->limits("127.0.0.1",2,9344); poll(nullptr,0,1);
        }
        f.scheduler->limits("127.0.0.1",2,9344);
    });
    std::set<std::pair<uint64_t,uint64_t>> consumed;
    const auto end=ipc::add(monotonicUs(),6000000);
    while(consumed.size()<64 && monotonicUs()<end) {
        f.tick();
        for(const auto handle:handles) {
            const auto view=f.take(handle); if(!view.result)continue;
            check(view.result->outcome==ipc::Outcome::Complete,"concurrent traffic terminal failed");
            check(consumed.insert({view.id.binding,view.id.generation}).second,"duplicate concurrent consumption");
            ipc::Reader reader(view.result->value); check(reader.oid()==f.specs.at(handle)->oid,"concurrent response OID mismatch");
            check(ipc::decodeValue(reader,*f.specs.at(handle),false).integer()==-123,"concurrent native value mismatch"); reader.end();
            f.scheduler->release(view.id);
        }
    }
    halt=true; for(auto& thread:threads)thread.join();
    check(accepted==64 && finished==2 && consumed.size()==64 && rejected>0,"live race evidence incomplete");
    f.settle();
    std::printf("{\"event\":\"live_race\",\"accepted\":%u,\"consumed\":%zu,\"rejected\":%u}\n",accepted.load(),consumed.size(),rejected.load());
}

uint64_t eventAt(const std::vector<SupervisionEvent>& events,uint32_t code,uint64_t after,uint64_t epoch=0);
// The worker never receives its Bootstrap: its own 5 s limit ends it, with no help from the supervisor.
void withheldBootstrap(Fixture& f)
{
    f.until([&]{ for(const auto& s:f.owner->snapshots())if(s.address==1 && s.reaps>=1)return true; return false; },9000);
}
// A real worker with a 400 ms request deadline against an agent that never answers, and a proxy that
// withholds every Close frame: the worker's own deadline timer ends the request.
void heldClose(Fixture& f)
{
    const auto a=f.handles.at("Address4Read").front();
    f.admit({a},{},400,monotonicUs());
    f.read(a,ipc::Outcome::Deadline);
    f.settle();
}
// The library retransmits an unanswered request when its own timer is due (timeoutMs 300, one retry):
// the request fails after both attempts, and the outer proxy records when each attempt arrived.
void retransmit(Fixture& f)
{
    const auto a=f.handles.at("Address4Read").front();
    f.admit({a},{},5000,monotonicUs());
    f.read(a,ipc::Outcome::NativeFailure);
    f.settle();
}
// Two endpoints of one address in one worker: the first one's request is pending (its agent drops every
// answer) while the second session is idle and opened later. The library's retransmit timer of the first
// session must still wake the worker, so the request ends by its own timeout and not by the deadline.
void multisession(Fixture& f)
{
    const auto drop=f.handles.at("DropRead").front(),ok=f.handles.at("OkRead").front();
    f.admit({drop},{},8000,monotonicUs());
    f.read(drop,ipc::Outcome::NativeFailure);
    f.settle();
    f.admit({ok},{},3000,monotonicUs());
    f.read(ok,ipc::Outcome::Complete,-123);
    f.settle();
    const auto started=monotonicUs();
    f.admit({drop},{},6000,started);
    f.read(drop,ipc::Outcome::NativeFailure);
    check(monotonicUs()-started<5000000,"request ended by its deadline and not by the library timeout");
    f.settle();
}
// 100 SET members and then 100 GET members of one octet string in one batch each: the Batch and the
// Result frame are each larger than one 64 KiB read, and the socket buffers are small and read slowly.
void bulk(Fixture& f)
{
    constexpr unsigned Count=100;
    f.scheduler->limits("127.0.0.1",1024,ipc::MaxBytes);
    const auto config=f.scheduler->configuration();
    const auto setBinding=f.config.bind("BulkSet"),readBinding=f.config.bind("BulkRead");
    std::vector<uint64_t> sets,reads;
    for(unsigned i=0;i<Count;++i) {
        const auto s=f.scheduler->registerBinding(setBinding),r=f.scheduler->registerBinding(readBinding);
        f.specs[s]=&config->bindings.at("BulkSet"); f.specs[r]=&config->bindings.at("BulkRead");
        sets.push_back(s); reads.push_back(r);
    }
    std::vector<uint8_t> pattern(600); for(size_t i=0;i<pattern.size();++i)pattern[i]=uint8_t(i*7+3);
    std::vector<Value> values(Count,Value::octets(pattern));
    f.admit(sets,values,30000,monotonicUs());
    for(const auto handle:sets)f.read(handle,ipc::Outcome::Complete);
    f.settle();
    f.admit(reads,{},30000,monotonicUs());
    for(const auto handle:reads) {
        const auto value=f.read(handle,ipc::Outcome::Complete);
        check(value.octets()==pattern,"bulk octet string mismatch");
    }
    f.settle();
}
// Idle, contained and hung-up workers: which timers and wait descriptors the supervisor reports.
void idleTimers(Fixture& f)
{
    const auto idle=[&]{
        const auto now=monotonicUs();
        check(f.owner->nextTimer()==UINT64_MAX,"idle supervisor holds an event timer");
        check(f.owner->nextPoll(now)==now+100000,"idle worker is not polled at the backstop interval");
        check(f.owner->nextWake(now)==now+100000,"loop wake time does not include the backstop poll");
        const auto set=f.owner->descriptors();
        check(set.size()==2 && !set[0].write && !set[1].write,"idle channels are not waited on for reading only");
    };
    idle();
    const auto a=f.handles.at("Address4Read");
    f.signal(1,SIGSTOP);
    f.admit({a[0]},{},100,monotonicUs()); f.read(a[0],ipc::Outcome::Deadline);
    const auto contained=monotonicUs();
    check(f.worker(1).pid>0 && f.owner->nextPoll(contained)==contained+10000,"contained worker is not polled for its exit soon");
    check(f.owner->nextWake(contained)<=contained+10000,"loop wake time does not include the exit poll");
    f.until([&]{return f.worker(1).reaps==1;},4000);
    f.ready(); f.settle(); idle();
    // A worker killed from outside hangs its channel up; the dead channel leaves the wait set.
    f.signal(2,SIGKILL);
    f.until([&]{return f.owner->descriptors().size()==1;},3000);
    f.until([&]{return f.worker(2).reaps==1;},4000);
    f.ready(); f.settle(); idle();
}
// An answered, consumed member whose Retired frame never arrives is contained when the retire grace
// after its deadline ends; that time is the supervisor's next timer.
void graceTimer(Fixture& f)
{
    const auto a=f.handles.at("Address4Read").front(); const auto origin=monotonicUs();
    const uint64_t deadline=origin+400000;
    f.admit({a},{},400,origin);
    f.read(a,ipc::Outcome::Complete,-123);
    check(f.scheduler->snapshot(1).retirementPending==1,"answered member is not retirement-pending");
    check(f.owner->nextTimer()==deadline+1000000,"retire grace is not the supervisor's next timer");
    check(f.owner->nextWake(monotonicUs())<=monotonicUs()+100000,"idle backstop is not part of the loop's wake time");
    // The worker is stopped, so once it is contained it neither exits nor closes its channel until the
    // supervisor kills it: the grace has acted and no timer in the past may remain.
    // The worker has sent its Retired frame by now and the proxy has withheld it.
    const auto settled=monotonicUs()+200000;
    f.until([&]{return monotonicUs()>=settled;},1000);
    f.signal(1,SIGSTOP);
    f.until([&]{return eventAt(f.observed,6,origin)!=0;},4000);
    const auto contained=eventAt(f.observed,6,origin);
    check(contained>=deadline+1000000,"worker contained before the retire grace ended");
    // The plan's number: the containment follows the end of the grace within 50 ms.
    check(contained<=deadline+1000000+50000,"worker contained later than 50 ms after the retire grace ended");
    std::printf("{\"event\":\"timer_lateness\",\"case\":\"retire-grace\",\"late_us\":%llu}\n",
                (unsigned long long)(contained-(deadline+1000000)));
    unsigned ticks=0;
    f.until([&]{ ++ticks; if(f.worker(1).reaps>0)return true;
                 check(notStale(f.owner->nextTimer()),"a past event timer remained after containment"); return false; },4000);
    check(ticks>50,"containment window was not observed");
    f.settle();
}

uint64_t eventAt(const std::vector<SupervisionEvent>& events,uint32_t code,uint64_t after,uint64_t epoch)
{
    for(const auto& e:events)if(e.address==1 && e.code==code && e.at>=after && (!epoch || e.epoch==epoch))return e.at;
    return 0;
}
// A consumed Deadline generation whose worker is stopped stays retirement-pending; a new generation of
// the same handle is admitted behind it and dispatched only after reap, relaunch and Ready. A second,
// short-budget successor expires in the queue unsent. The second containment relaunches after the
// first backoff step because the successful batch in between reset the failure count.
void behindRetirement(Fixture& f)
{
    const auto a=f.handles.at("Address4Read");
    const auto phaseOne=monotonicUs();
    f.signal(1,SIGSTOP);
    f.admit({a[0]},{},100,monotonicUs()); f.read(a[0],ipc::Outcome::Deadline);
    const auto queued=f.admit({a[0]},{},5000,monotonicUs()).front(); const auto admittedAt=monotonicUs();
    const auto held=f.scheduler->snapshot(1);
    check(queued.generation==2 && held.count==2 && held.retirementPending==1 && held.queued==1 && held.behindRetirement==1,
          "successor not held behind retirement");
    f.read(a[0],ipc::Outcome::Complete,-123);
    const auto reapAt=eventAt(f.observed,7,admittedAt),launchAt=eventAt(f.observed,2,reapAt),
               epoch=f.worker(1).epoch,readyAt=eventAt(f.observed,3,launchAt,epoch),dispatchAt=eventAt(f.observed,13,readyAt,epoch);
    check(reapAt && launchAt && readyAt && dispatchAt && epoch==2,"successor dispatched before reap, relaunch and Ready");
    f.until([&]{const auto q=f.scheduler->snapshot(1); return q.count==0 && f.worker(1).ready;});
    check(eventAt(f.observed,5,dispatchAt,epoch)!=0,"successor batch was not retired before the second phase");
    const auto phaseTwo=monotonicUs();
    f.signal(1,SIGSTOP);
    f.admit({a[1]},{},100,monotonicUs()); f.read(a[1],ipc::Outcome::Deadline);
    const auto shortQueued=f.admit({a[1]},{},200,monotonicUs()).front();
    TerminalView view; f.until([&]{view=f.take(a[1]); return view.result!=nullptr;},3000);
    check(view.id==shortQueued && view.result->outcome==ipc::Outcome::Deadline && !view.sent,"short-budget successor was sent or not expired");
    f.scheduler->release(view.id);
    const auto counters=f.scheduler->snapshot(1);
    std::printf("{\"event\":\"behind_counters\",\"admitted\":%llu,\"never_sent\":%llu}\n",
                (unsigned long long)counters.behindAdmitted,(unsigned long long)counters.behindNeverSent);
    check(counters.behindNeverSent==1 && counters.behindAdmitted==2,"behind-retirement counters mismatch");
    f.until([&]{const auto reap=eventAt(f.observed,7,phaseTwo); return reap && eventAt(f.observed,2,reap);},4000);
    const auto secondReap=eventAt(f.observed,7,phaseTwo),secondLaunch=eventAt(f.observed,2,secondReap);
    const auto firstReap=eventAt(f.observed,7,phaseOne),firstLaunch=eventAt(f.observed,2,firstReap);
    f.ready(); f.settle();
    std::printf("{\"event\":\"behind_retirement\",\"admitted_at\":%llu,\"reap_at\":%llu,\"launch_at\":%llu,\"ready_at\":%llu,"
                "\"dispatch_at\":%llu,\"epoch\":%llu,\"short_sent\":%s,\"first_backoff_us\":%llu,\"second_backoff_us\":%llu}\n",
                (unsigned long long)admittedAt,(unsigned long long)reapAt,(unsigned long long)launchAt,(unsigned long long)readyAt,
                (unsigned long long)dispatchAt,(unsigned long long)epoch,view.sent?"true":"false",
                (unsigned long long)(firstLaunch-firstReap),(unsigned long long)(secondLaunch-secondReap));
}
// Repeated same-handle generations under the stale-frame IPC fault: each successor is admitted as soon
// as its predecessor is consumed, so a forged Result or Retired carrying the successor's identity can
// arrive while that successor is really queued. Every generation must still complete exactly once.
void staleBehind(Fixture& f)
{
    const auto a=f.handles.at("Address4Read").front();
    constexpr unsigned Trials=5;
    f.admit({a},{},5000,monotonicUs());
    for(unsigned trial=0;trial<Trials;++trial) {
        f.read(a,ipc::Outcome::Complete,-123);
        const auto next=f.admit({a},{},5000,monotonicUs()).front();
        const auto queuedNow=f.scheduler->snapshot(1).behindRetirement;
        std::printf("{\"event\":\"stale_behind_admitted\",\"generation\":%llu,\"at_ns\":%llu,\"behind\":%llu}\n",
                    (unsigned long long)next.generation,(unsigned long long)monotonicUs()*1000ULL,(unsigned long long)queuedNow);
    }
    f.read(a,ipc::Outcome::Complete,-123); f.settle();
}
}
int main(int argc,char** argv)
{
    setvbuf(stdout,nullptr,_IOLBF,0); if(argc<6 || argc>8)return 2;
    std::vector<int> descriptors;
    try {
        const std::string mode=argv[5];
        if(mode=="high-fd") {
            rlimit limit{}; check(getrlimit(RLIMIT_NOFILE,&limit)==0 && limit.rlim_max>=2048,"high FD hard limit unavailable");
            limit.rlim_cur=std::max<rlim_t>(limit.rlim_cur,2048); check(setrlimit(RLIMIT_NOFILE,&limit)==0,"high FD soft limit failed");
            int descriptor=0;
            while(descriptor<1100) { descriptor=open("/dev/null",O_RDONLY|O_CLOEXEC); check(descriptor>=0,"high FD fixture failed"); descriptors.push_back(descriptor); }
            struct sigaction action{}; action.sa_handler=interrupt; sigemptyset(&action.sa_mask);
            check(sigaction(SIGUSR1,&action,nullptr)==0,"interrupt handler failed");
            check(raise(SIGUSR1)==0 && interrupts==1,"real signal absent");
            std::printf("{\"event\":\"high_fd\",\"highest_fixture_fd\":%d,\"soft_limit\":%llu}\n",descriptor,(unsigned long long)limit.rlim_cur);
        }
        {
            Fixture f(argv);
            if(mode=="unsent")unsent(f);
            else if(mode=="ipc-bootstrap-mismatch" || mode=="ipc-bootstrap-secret" || mode=="ipc-ready-executable" || mode=="ipc-ready-library")bootstrapFault(f);
            else if(mode=="ipc-withhold-bootstrap")withheldBootstrap(f);
            else {
                f.ready();
                if(mode=="typed" || mode=="immutable")typed(f);
                else if(mode=="security-conflict")securityConflict(f);
                else if(mode=="live-race")liveRace(f);
                else if(mode=="fifo" || mode=="high-fd" || mode=="ipc-partial" || mode=="ipc-coalesced" || mode=="ipc-stale")fifo(f);
                else if(mode=="deadline")deadline(f);
                else if(mode=="behind-retirement")behindRetirement(f);
                else if(mode=="ipc-stale-behind")staleBehind(f);
                else if(mode=="native-expiry" || mode=="discovery-expiry")nativeDeadline(f);
                else if(mode=="ipc-full-channel")fullChannel(f);
                else if(mode=="ipc-malformed" || mode=="ipc-truncated" || mode=="ipc-partial-timeout" || mode=="ipc-bad-set" || mode=="ipc-oversize" || mode=="ipc-stall-batch")ipcFault(f,mode);
                else if(mode=="native-retransmit")retransmit(f);
                else if(mode=="native-multisession")multisession(f);
                else if(mode=="ipc-forward")fifo(f);
                else if(mode=="ipc-hold-close")heldClose(f);
                else if(mode=="ipc-bulk")bulk(f);
                else if(mode=="timers")idleTimers(f);
                else if(mode=="ipc-hold-retired")graceTimer(f);
                else if(mode=="crashes")crashes(f);
                else if(mode=="ambiguous" && argc==7)ambiguous(f,argv[6]);
                else throw std::runtime_error("qualification mode rejected");
            }
            f.stop();
            std::printf("{\"event\":\"qualification_summary\",\"mode\":\"%s\",\"checks\":%u,\"settled\":true}\n",mode.c_str(),checks);
        }
        for(const auto descriptor:descriptors)close(descriptor);
        return 0;
    } catch(const std::exception& error) {
        for(const auto descriptor:descriptors)close(descriptor);
        std::fprintf(stderr,"qualification failure after %u checks: %s\n",checks,error.what()); return 1;
    }
}
