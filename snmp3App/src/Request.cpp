#include "Request.h"
#include "Runtime.h"
#include <algorithm>
#include <alarm.h>
#include <chrono>
#include <dbCommon.h>
#include <dbLock.h>
#include <dbScan.h>
#include <recGbl.h>
#include <recSup.h>
#include <epicsThread.h>
#include "Peer.h"

namespace snmp3 {
namespace {
const unsigned ServiceVisits=128;
const uint64_t DrainBudgetUs=2000000;
const uint64_t DrainWaitUs=50000;
const uint64_t ContextStorageLimit=8192;
void require(bool condition)
{ if(!condition)throw std::runtime_error("record request rejected"); }
}
// The requests of one subscription. A cycle is one request for all its records: it is admitted when the period
// has passed and the cycle before it has ended, its results are given to the records together by one scan
// request, and it ends when Base has processed the records of every priority.
struct Cycle {
    enum class State { Idle, Admitted, Delivered };
    std::string name;
    unsigned periodMs=0;
    IOSCANPVT scan=nullptr;
    std::vector<RecordContext*> members;
    State state=State::Idle;
    // listed: records on the I/O Intr list; a subscription without one sends no request. pending: priorities
    // whose records Base still processes.
    size_t listed=0;
    unsigned pending=0;
    uint64_t dueUs=0;
};
RecordContext::RecordContext(RecordBinding spec,dbCommon* rec,std::string address,
                            bool (*valid)(RecordContext&),
                            void (*stage)(RecordContext&,const ipc::Result&))
    : definition(std::move(spec)),link(std::move(address)),dtype(rec->dtyp),
      dset(rec->dset),rset(rec->rset),record(rec),validate(valid),prepare(stage)
{}
Requests::Requests() : cursor(contexts.end()) {}
Requests& Requests::instance() { static Requests* value=new Requests; return *value; }
bool Requests::attachmentAllowed()
{ std::lock_guard<std::mutex> guard(mutex); return initializing && !drainFailed; }
RecordContext& Requests::attach(RecordBinding binding,dbCommon* record,const std::string& link,
                               bool (*validate)(RecordContext&),
                               void (*prepare)(RecordContext&,const ipc::Result&),
                               const std::string& subscription,unsigned periodMs)
{
    require(record && !record->dpvt && validate && prepare);
    auto context=std::unique_ptr<RecordContext>(new RecordContext(std::move(binding),record,link,validate,prepare));
    const auto& spec=context->definition.binding->definition();
    const auto fixed=ipc::add(sizeof(RecordContext)+128,context->link.capacity()+1);
    require(fixed<=ContextStorageLimit);
    const auto captureBytes=spec.operation==Operation::Set ?
        ipc::add(256,ipc::multiply(2,std::min<size_t>(spec.capacity,context->definition.storageCapacity))):0;
    const auto retained=ipc::add(fixed,captureBytes);
    // Charge simultaneous decode, checked conversion and text/capture staging at admission.
    const auto content=spec.valueType==ValueType::ObjectId ? ipc::multiply(spec.capacity,4):spec.capacity;
    const auto textBytes=spec.valueType==ValueType::ObjectId ? ipc::multiply(spec.capacity,11):
                         spec.valueType==ValueType::IpAddress ? 16:content;
    const auto scratch=ipc::add(512,ipc::multiply(4,ipc::add(content,textBytes)));
    require(attachmentAllowed());
    std::list<std::unique_ptr<RecordContext>> pending;
    pending.push_back(std::move(context));
    auto& result=*pending.back();
    result.handle=Runtime::instance().bind(result.definition.binding,retained,scratch);
    result.owner=Runtime::instance().schedulerOwner();
    callbackSetCallback(&Requests::callback,&result.callback);
    callbackSetPriority(record->prio,&result.callback);
    callbackSetUser(&result,&result.callback);
    std::lock_guard<std::mutex> guard(mutex);
    require(initializing && !drainFailed);
    if(!subscription.empty()) {
        auto& cycle=cycles[subscription];
        if(!cycle) {
            cycle.reset(new Cycle); cycle->name=subscription;
            scanIoInit(&cycle->scan); scanIoSetComplete(cycle->scan,&Requests::cycleProcessed,cycle.get());
        }
        cycle->periodMs=periodMs; cycle->members.push_back(&result); result.subscription=cycle.get();
    }
    contexts.splice(contexts.end(),pending);
    record->dpvt=&result;
    return result;
}
bool Requests::interrupt(RecordContext& context,bool added,IOSCANPVT* scan)
{
    {
        std::lock_guard<std::mutex> guard(mutex);
        if(!context.subscription)return false;
        *scan=context.subscription->scan;
        // The first request of a subscription leaves at once when a record joins a list that held none; no
        // request leaves while the list holds none.
        auto& cycle=*context.subscription;
        if(added) { if(!cycle.listed++)cycle.dueUs=0; }
        else if(cycle.listed)--cycle.listed;
    }
    Runtime::instance().poke();
    return true;
}
void Requests::scanning(bool open)
{
    { std::lock_guard<std::mutex> guard(mutex); scanOpen=open; }
    Runtime::instance().poke();
}
bool Requests::takeOutcome(RecordContext& context)
{
    std::lock_guard<std::mutex> guard(mutex);
    if(!context.ready || !entryOpen || context.callbackState==CallbackState::Running)return false;
    context.ready=false; context.callbackState=CallbackState::Running; ++entered;
    return true;
}
void Requests::outcomeTaken(RecordContext& context)
{
    std::lock_guard<std::mutex> guard(mutex);
    finish(context); --entered;
}
// One request for the records of a subscription. A record of an endpoint whose unit shows another engine ID is
// not asked for, except system information while the responder may still authenticate or has done so; it gets
// the alarm of that state as its outcome. A refused admission gives its records a read alarm with a message.
void Requests::startCycle(Cycle& cycle,uint64_t nowUs)
{
    const uint64_t period=ipc::multiply(cycle.periodMs,1000);
    cycle.dueUs=cycle.dueUs && ipc::add(cycle.dueUs,period)>nowUs ? ipc::add(cycle.dueUs,period):ipc::add(nowUs,period);
    std::vector<uint64_t> handles; std::vector<RecordContext*> asked; unsigned budgetMs=0;
    for(auto* member:cycle.members) {
        if(!member->record || member->active || member->ready || member->callbackState!=CallbackState::Idle ||
           member->owner!=activation)continue;
        const auto& binding=*member->definition.binding;
        const auto state=Peers::instance().state(binding.endpoint().id);
        if(state!=PeerState::Normal && !(binding.definition().systemInfo && state!=PeerState::Unverified)) {
            member->syntheticAlarm=COMM_ALARM; member->syntheticMessage=Peers::message(state); member->ready=true;
            continue;
        }
        handles.push_back(member->handle); asked.push_back(member);
        budgetMs=budgetMs ? std::min(budgetMs,member->definition.budgetMs):member->definition.budgetMs;
    }
    if(!handles.empty()) {
        try {
            const auto identities=Runtime::instance().admit(handles,std::vector<Value>(),budgetMs);
            for(size_t index=0;index<asked.size();++index) {
                asked[index]->identity=identities[index]; asked[index]->active=true; asked[index]->terminal=TerminalView();
            }
            cycle.state=Cycle::State::Admitted; return;
        } catch(const std::exception&) {
            // The queue of the address did not take the request, for its count or its bytes.
            for(auto* member:asked) {
                member->syntheticAlarm=READ_ALARM; member->syntheticMessage="request not admitted"; member->ready=true;
            }
        }
    }
    deliver(cycle);
}
// Gives the outcomes of a cycle to its records: Base processes every record of the I/O Intr list, in the order
// of PHAS among the records of one priority.
void Requests::deliver(Cycle& cycle)
{
    if(stopping || !scanOpen) { settle(cycle); return; }
    cycle.state=Cycle::State::Delivered; cycle.pending=0;
    const unsigned queued=scanIoRequest(cycle.scan);
    for(unsigned priority=0;priority<NUM_CALLBACK_PRIORITIES;++priority)if(queued & (1u<<priority))++cycle.pending;
    if(!cycle.pending)settle(cycle);
}
// Ends a cycle: an outcome that no processing took is dropped, so the next cycle starts from nothing.
void Requests::settle(Cycle& cycle)
{
    for(auto* member:cycle.members) {
        if(member->callbackState==CallbackState::Running)continue;
        if(member->active || member->ready) {
            if(member->active && !member->terminal.result)continue;
            member->ready=false; member->syntheticAlarm=0; member->syntheticMessage=nullptr;
            finish(*member);
        }
    }
    cycle.state=Cycle::State::Idle; cycle.pending=0;
}
void Requests::cycleProcessed(void* user,IOSCANPVT,int)
{
    auto& self=instance(); auto& cycle=*static_cast<Cycle*>(user);
    {
        std::lock_guard<std::mutex> guard(self.mutex);
        if(cycle.state!=Cycle::State::Delivered || !cycle.pending || --cycle.pending)return;
        self.settle(cycle);
        ++self.changes; self.changed.notify_all();
    }
    Runtime::instance().poke();
}
uint64_t Requests::serviceCycles(uint64_t nowUs)
{
    uint64_t timer=UINT64_MAX;
    for(auto& entry:cycles) {
        auto& cycle=*entry.second;
        if(cycle.members.empty())continue;
        if(cycle.state==Cycle::State::Admitted) {
            bool complete=true;
            for(auto* member:cycle.members) {
                if(!member->active || member->ready)continue;
                if(!member->terminal.result) {
                    member->terminal=member->owner->take(member->identity);
                    if(!member->terminal.result) { complete=false; continue; }
                    require(member->terminal.id==member->identity);
                }
                member->ready=true;
            }
            if(complete)deliver(cycle);
        }
        // A stop does not wait for records that Base no longer processes.
        if(cycle.state==Cycle::State::Delivered && stopping)settle(cycle);
        if(cycle.state==Cycle::State::Idle && cycle.listed && scanOpen && !stopping) {
            if(nowUs>=cycle.dueUs)startCycle(cycle,nowUs);
            if(cycle.state==Cycle::State::Idle)timer=std::min(timer,cycle.dueUs);
        }
    }
    return timer;
}
bool Requests::detach(RecordContext& context)
{
    std::lock_guard<std::mutex> guard(mutex);
    if(!detachAllowed)return false;
    if(context.record && context.record->dpvt==&context)context.record->dpvt=nullptr;
    context.record=nullptr;
    return true;
}
void Requests::start(std::shared_ptr<Scheduler> owner)
{
    std::lock_guard<std::mutex> guard(mutex);
    require(!drainFailed && !entered);
    for(const auto& context:contexts)require(!context->active && context->owner==owner && context->record);
    activation=std::move(owner);
    initializing=false; detachAllowed=false; entryOpen=true; producers=true; stopping=false;
    for(auto& cycle:cycles) { cycle.second->state=Cycle::State::Idle; cycle.second->pending=0; cycle.second->dueUs=0; }
    cursor=contexts.begin(); visited=0;
}
void Requests::admit(RecordContext& context,const std::vector<Value>& payload)
{
    std::lock_guard<std::mutex> guard(mutex);
    require(producers && entryOpen && context.owner==activation && !context.active &&
            context.callbackState==CallbackState::Idle && context.record);
    const auto identities=Runtime::instance().admit({context.handle},payload,context.definition.budgetMs);
    context.identity=identities.front(); context.active=true; context.published=false;
    context.alarm=0; context.message=nullptr; context.valueWithAlarm=false;
    context.record->pact=TRUE;
}
bool Requests::completing(RecordContext& context)
{
    std::lock_guard<std::mutex> guard(mutex);
    return context.active && context.callbackState==CallbackState::Running &&
           context.terminal.result && context.identity==context.terminal.id &&
           context.owner==activation;
}
ServiceOutcome Requests::service()
{
    std::lock_guard<std::mutex> guard(mutex);
    ServiceOutcome outcome;
    if(!producers || contexts.empty()) { visited=0; refused=false; return outcome; }
    // The retry flag covers every call of one cycle: a refusal in an early call is not forgotten when the
    // later calls of the cycle find nothing to refuse.
    if(visited==0)refused=false;
    const size_t count=std::min<size_t>(contexts.size(),ServiceVisits);
    for(size_t visit=0;visit<count;++visit) {
        if(cursor==contexts.end())cursor=contexts.begin();
        auto& context=**cursor++;
        // The records of a subscription are served by their cycle.
        if(context.subscription)continue;
        if(!context.active || context.callbackState==CallbackState::Queued ||
           context.callbackState==CallbackState::Running || context.callbackState==CallbackState::Inert)continue;
        if(!context.terminal.result) {
            context.terminal=context.owner->take(context.identity);
            if(!context.terminal.result)continue;
            require(context.terminal.id==context.identity);
            context.callbackState=CallbackState::Pending;
        }
        // Publish queue ownership before Base can execute the accepted entry.
        context.callbackState=CallbackState::Queued;
        if(callbackRequest(&context.callback)) {
            context.callbackState=CallbackState::Pending;
            ++enqueueFailures; refused=true;
        }
    }
    // A pass over every context completes before the caller waits again.
    visited+=count;
    if(visited>=contexts.size())visited=0;
    else outcome.more=true;
    outcome.retry=refused;
    outcome.timerUs=serviceCycles(monotonicUs());
    return outcome;
}
void Requests::finish(RecordContext& context)
{
    if(context.terminal.result)context.owner->release(context.terminal.id);
    context.terminal=TerminalView();
    context.staged.reset(); std::string().swap(context.text);
    context.active=false; context.callbackState=CallbackState::Idle;
    ++completions; ++changes; changed.notify_all();
}
void Requests::callback(epicsCallback* callback)
{
    auto& context=*static_cast<RecordContext*>(callback->user);
    auto& self=instance(); dbCommon* record=nullptr;
    {
        std::lock_guard<std::mutex> guard(self.mutex);
        if(!self.entryOpen) {
            context.callbackState=CallbackState::Inert;
            ++self.changes; self.changed.notify_all(); return;
        }
        if(context.callbackState!=CallbackState::Queued || !context.active ||
           !context.terminal.result || !(context.identity==context.terminal.id))return;
        ++self.entered; context.callbackState=CallbackState::Running;
        record=context.record;
    }
    if(record) {
        dbScanLock(record);
        if(record->dpvt==&context && record->rset==context.rset && record->pact) {
            if(!context.validate(context))context.alarm=LINK_ALARM;
            else {
                try { context.prepare(context,*context.terminal.result); }
                catch(const std::exception&) {
                    context.alarm=context.definition.binding->definition().operation==Operation::Get ? READ_ALARM:WRITE_ALARM;
                }
            }
            // Base keeps NAMSG when it equals AMSG, so a message raised twice in a row would stay on the record
            // after the request succeeds; a completion without a message starts from an empty NAMSG.
            if(!context.message)record->namsg[0]='\0';
            if(context.alarm) {
                if(context.message)recGblSetSevrMsg(record,context.alarm,INVALID_ALARM,"%s",context.message);
                else recGblSetSevr(record,context.alarm,INVALID_ALARM);
            }
            // Base selects the input source and owns monitors, FLNK, RPRO and PACT clearing.
            record->rset->process(record);
        }
        {
            std::lock_guard<std::mutex> guard(self.mutex);
            self.finish(context);
        }
        dbScanUnlock(record);
    } else {
        std::lock_guard<std::mutex> guard(self.mutex); self.finish(context);
    }
    {
        std::lock_guard<std::mutex> guard(self.mutex);
        --self.entered; ++self.changes; self.changed.notify_all();
    }
}
bool Requests::drain()
{
    const auto deadline=ipc::add(monotonicUs(),DrainBudgetUs);
    bool drained=false;
    { std::lock_guard<std::mutex> guard(mutex); stopping=true; }
    do {
        uint64_t seen=0;
        { std::lock_guard<std::mutex> guard(mutex); seen=changes; }
        ServiceOutcome outcome;
        do { outcome=service(); } while(outcome.more);
        std::unique_lock<std::mutex> wait(mutex);
        drained=!entered && std::none_of(contexts.begin(),contexts.end(),
                                       [](const std::unique_ptr<RecordContext>& c){ return c->active; });
        if(drained)break;
        // finish() and the callback exit notify this condition; the bound covers a failed enqueue.
        // A change that arrived since `seen` (even during the service pass above) ends the wait at once.
        changed.wait_for(wait,std::chrono::microseconds(outcome.retry ? ServiceRetryUs:DrainWaitUs),[&]{ return changes!=seen; });
    } while(monotonicUs()<deadline);
    std::unique_lock<std::mutex> guard(mutex);
    producers=false; entryOpen=false;
    if(!drained)drainFailed=true;
    // The safety wait includes callbacks blocked on dbScanLock or downstream processing.
    changed.wait(guard,[&]{return entered==0;});
    return drained && !drainFailed;
}
void Requests::permitDetach()
{ std::lock_guard<std::mutex> guard(mutex); require(!entryOpen && !entered); detachAllowed=true; }
void Requests::queuesDestroyed()
{
    std::lock_guard<std::mutex> guard(mutex);
    require(!entryOpen && !entered);
    for(const auto& context:contexts) {
        require(!context->record);
        if(context->active)finish(*context);
    }
    // Base frees its I/O Intr lists with the database, so the cycles go with the contexts and a database that
    // is built again gets new ones.
    cycles.clear();
    contexts.clear(); cursor=contexts.end(); visited=0; activation.reset();
    initializing=!drainFailed; detachAllowed=false;
}
RecordSnapshot Requests::snapshot()
{
    std::lock_guard<std::mutex> guard(mutex);
    RecordSnapshot result;
    result.contexts=contexts.size(); result.entered=entered; result.enqueueFailures=enqueueFailures;
    result.completions=completions; result.entryOpen=entryOpen;
    result.drainFailed=drainFailed; result.detachAllowed=detachAllowed;
    for(const auto& context:contexts) {
        if(context->active)++result.active;
        switch(context->callbackState) {
        case CallbackState::Pending: ++result.pending; break;
        case CallbackState::Queued: ++result.queued; break;
        case CallbackState::Running: ++result.running; break;
        case CallbackState::Inert: ++result.inert; break;
        default: break;
        }
    }
    return result;
}
}
