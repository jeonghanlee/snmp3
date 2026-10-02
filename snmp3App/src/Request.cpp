#include "Request.h"
#include "Runtime.h"
#include <algorithm>
#include <alarm.h>
#include <dbCommon.h>
#include <dbLock.h>
#include <dbScan.h>
#include <recGbl.h>
#include <recSup.h>
#include <epicsThread.h>

namespace snmp3 {
namespace {
const unsigned ServiceVisits=128;
const uint64_t DrainBudgetUs=2000000;
const uint64_t ContextStorageLimit=8192;
void require(bool condition)
{ if(!condition)throw std::runtime_error("record request rejected"); }
}
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
                               void (*prepare)(RecordContext&,const ipc::Result&))
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
    contexts.splice(contexts.end(),pending);
    record->dpvt=&result;
    return result;
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
    initializing=false; detachAllowed=false; entryOpen=true; producers=true;
    cursor=contexts.begin();
}
void Requests::admit(RecordContext& context,const std::vector<Value>& payload)
{
    std::lock_guard<std::mutex> guard(mutex);
    require(producers && entryOpen && context.owner==activation && !context.active &&
            context.callbackState==CallbackState::Idle && context.record);
    const auto identities=Runtime::instance().admit({context.handle},payload,context.definition.budgetMs);
    context.identity=identities.front(); context.active=true; context.published=false;
    context.alarm=0;
    context.record->pact=TRUE;
}
bool Requests::completing(RecordContext& context)
{
    std::lock_guard<std::mutex> guard(mutex);
    return context.active && context.callbackState==CallbackState::Running &&
           context.terminal.result && context.identity==context.terminal.id &&
           context.owner==activation;
}
void Requests::service()
{
    std::lock_guard<std::mutex> guard(mutex);
    if(!producers || contexts.empty())return;
    for(size_t count=0;count<std::min<size_t>(contexts.size(),ServiceVisits);++count) {
        if(cursor==contexts.end())cursor=contexts.begin();
        auto& context=**cursor++;
        if(!context.active || context.callbackState==CallbackState::Queued ||
           context.callbackState==CallbackState::Running || context.callbackState==CallbackState::Inert)continue;
        if(!context.terminal.result) {
            context.terminal=context.owner->take(context.handle);
            if(!context.terminal.result)continue;
            require(context.terminal.id==context.identity);
            context.callbackState=CallbackState::Pending;
        }
        // Publish queue ownership before Base can execute the accepted entry.
        context.callbackState=CallbackState::Queued;
        if(callbackRequest(&context.callback)) {
            context.callbackState=CallbackState::Pending;
            ++enqueueFailures;
        }
    }
}
void Requests::finish(RecordContext& context)
{
    if(context.terminal.result)context.owner->release(context.terminal.id);
    context.terminal=TerminalView();
    context.staged.reset(); std::string().swap(context.text);
    context.active=false; context.callbackState=CallbackState::Idle;
    ++completions; changed.notify_all();
}
void Requests::callback(epicsCallback* callback)
{
    auto& context=*static_cast<RecordContext*>(callback->user);
    auto& self=instance(); dbCommon* record=nullptr;
    {
        std::lock_guard<std::mutex> guard(self.mutex);
        if(!self.entryOpen) {
            context.callbackState=CallbackState::Inert;
            self.changed.notify_all(); return;
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
            if(context.alarm)recGblSetSevr(record,context.alarm,INVALID_ALARM);
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
        --self.entered; self.changed.notify_all();
    }
}
bool Requests::drain()
{
    const auto deadline=ipc::add(monotonicUs(),DrainBudgetUs);
    bool drained=false;
    do {
        service();
        {
            std::lock_guard<std::mutex> guard(mutex);
            drained=!entered && std::none_of(contexts.begin(),contexts.end(),
                                           [](const std::unique_ptr<RecordContext>& c){ return c->active; });
        }
        if(drained)break;
        epicsThreadSleep(0.001);
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
    contexts.clear(); cursor=contexts.end(); activation.reset();
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
