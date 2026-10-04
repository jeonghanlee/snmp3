#include "Scheduler.h"
#include <algorithm>
#include <set>
#include <type_traits>

namespace snmp3 {
uint64_t Scheduler::fixedGenerationBytes()
{
    using GenerationEntry=std::pair<const Key,std::unique_ptr<Generation>>;
    static_assert(std::is_same<decltype(Address::generations)::value_type,GenerationEntry>::value,
                  "charged generation node must match the generation container");
    using GenerationNode=std::_Rb_tree_node<GenerationEntry>;
    using HandleNode=std::_Rb_tree_node<std::pair<const uint64_t,Handle>>;
    return sizeof(Generation)+sizeof(GenerationNode)+sizeof(HandleNode)+3*sizeof(ipc::Command)+
           2*sizeof(ipc::Result)+2*sizeof(ipc::Identity)+sizeof(ipc::ResultReservation)+
           2*sizeof(Value)+4*sizeof(std::shared_ptr<const Binding>)+1024+256;
}
namespace {
// A member whose request already succeeded or failed natively is contained only when its Retired
// frame is still missing this long after the deadline.
constexpr uint64_t RetireGraceUs=1000000;
void require(bool v) { if(!v)throw std::runtime_error("scheduler request rejected"); }
uint64_t next(uint64_t& value)
{
    require(value && value<UINT64_MAX); return value++;
}
}
Scheduler::Scheduler(std::shared_ptr<const Configuration> value, uint64_t rev, uint64_t act)
    : config(std::move(value)), revision(rev), activation(act)
{
    require(config && rev && act);
    configurationBase=ipc::add(65536,ipc::multiply(config->bindings.size(),256));
    configurationBytes=configurationBase; require(configurationBytes<=268435456);
    // The charged fixed inventory includes both process owners, container nodes and closures.
    static_assert(2*sizeof(Generation)+2*sizeof(Handle)+512<=4096,"generation fixed inventory exceeds M");
    static_assert(sizeof(Handle)+128<=256,"registration fixed inventory exceeds bound");
    std::set<std::string> unique;
    for(const auto& e:config->endpoints) {
        auto key=ipc::addressKey(e.second.address);
        require(unique.count(key) || unique.size()<64); unique.insert(std::move(key));
    }
    require(unique.size()<=64);
    uint64_t id=0;
    for(const auto& k:unique) { keys[k]=++id; queues[id].key=k; }
    id=0; for(const auto& b:config->bindings)definitions[b.first]=++id;
}
uint64_t Scheduler::registerBinding(std::shared_ptr<const Binding> b,
                                    uint64_t retainedBytes, uint64_t generationBytes)
{
    std::lock_guard<std::mutex> guard(mutex);
    require(accepting && b && &b->capabilities()==&config->capabilities);
    const auto& spec=b->definition(); const auto& expected=config->bindings.at(spec.id);
    require(spec.endpoint==expected.endpoint && spec.oid==expected.oid && spec.operation==expected.operation &&
            spec.valueType==expected.valueType && spec.capacity==expected.capacity);
    uint64_t address=keys.at(ipc::addressKey(b->endpoint().address)),count=0;
    for(const auto& h:handles)if(h.second.address==address)++count;
    const auto retained=ipc::add(retainedBytes,ipc::add(256,ipc::add(ipc::multiply(spec.oid.capacity(),4),ipc::add(spec.id.capacity()+1,spec.endpoint.capacity()+1))));
    require(count<ipc::MaxCount && ipc::add(configurationBytes,ipc::add(registrationBytes,retained))<=268435456);
    auto id=next(nextHandle); Handle h; h.binding=std::move(b); h.address=address; h.definition=definitions.at(spec.id);
    h.generationBytes=generationBytes;
    handles.emplace(id,std::move(h)); registrationBytes+=retained; return id;
}
void Scheduler::reserveConfiguration(uint64_t bytes)
{
    std::lock_guard<std::mutex> guard(mutex);
    const auto total=ipc::add(configurationBase,bytes);
    require(ipc::add(total,registrationBytes)<=268435456); configurationBytes=total;
}
std::vector<ipc::Identity> Scheduler::admit(const std::vector<uint64_t>& ids, const std::vector<Value>& values,
                                         unsigned budgetMs, uint64_t nowUs)
{
    require(!ids.empty() && ids.size()<=ipc::MaxCount && budgetMs>=1 && budgetMs<=600000);
    const auto deadline=ipc::add(nowUs,ipc::multiply(budgetMs,1000));
    require(deadline);
    std::lock_guard<std::mutex> guard(mutex); require(accepting);
    const auto address=handles.at(ids.front()).address; auto& a=queues.at(address);
    require(a.accepting);
    const auto operation=handles.at(ids.front()).binding->definition().operation;
    require(operation==Operation::Get ? values.empty():values.size()==ids.size());
    std::set<uint64_t> unique; std::vector<bool> behind; uint64_t total=0;
    require(ipc::add(a.count,ids.size())<=a.countLimit);
    if(ids.size()>UINT64_MAX-a.nextAdmission) { a.accepting=false; throw std::runtime_error("admission identity exhausted"); }
    for(size_t index=0;index<ids.size();++index) {
        const auto id=ids[index]; const auto& h=handles.at(id); const auto& b=h.binding->definition();
        require(h.address==address && b.operation==operation && unique.insert(id).second);
        // A new generation is admitted only when every existing generation of the handle is consumed
        // and at most one exists; it then queues behind that generation's native retirement.
        unsigned existing=0; bool consumed=true;
        for(auto it=a.generations.lower_bound(Key(id,0));it!=a.generations.end() && it->first.first==id;++it) {
            ++existing; consumed=consumed && it->second->consumed;
        }
        require(existing<=1 && consumed); behind.push_back(existing==1);
        if(h.generation==UINT64_MAX) { a.accepting=false; throw std::runtime_error("generation identity exhausted"); }
        total=ipc::add(total,ipc::add(ipc::charge(b),h.generationBytes)); require(ipc::add(a.bytes,total)<=a.byteLimit);
        require(ipc::add(8+48,ipc::responseBytes(b))<=ipc::DataBytes);
        if(operation==Operation::Set) {
            require(values[index].type()==b.valueType);
            if(b.valueType==ValueType::Octets)require(values[index].octets().size()<=b.capacity);
            if(b.valueType==ValueType::ObjectId)require(values[index].objectId().size()<=b.capacity);
        }
    }
    std::map<Key,std::unique_ptr<Generation>> candidates;
    std::vector<ipc::Identity> admitted; admitted.reserve(ids.size());
    auto admission=a.nextAdmission;
    for(size_t index=0;index<ids.size();++index) {
        auto id=ids[index]; auto& h=handles.at(id); const auto& b=h.binding->definition();
        require(h.generation<UINT64_MAX && admission && admission<UINT64_MAX);
        auto g=std::unique_ptr<Generation>(new Generation);
        g->command.definition=h.definition; g->command.id.binding=id; g->command.id.generation=h.generation+1;
        g->command.id.admission=admission++; g->command.deadline=deadline; g->command.operation=operation;
        g->q=ipc::add(ipc::charge(b),h.generationBytes); g->behind=behind[index]; require(ipc::add(8+48,ipc::responseBytes(b))<=ipc::DataBytes);
        if(operation==Operation::Set) {
            auto encoded=ipc::encodeValue(values[index]); ipc::Reader check(encoded);
            ipc::decodeValue(check,b,true); check.end();
            require(ipc::add(8+48,encoded.size())<=ipc::DataBytes); g->command.value=std::move(encoded);
        }
        admitted.push_back(g->command.id); candidates.emplace(key(g->command.id),std::move(g));
    }
    require(ipc::add(a.count,ids.size())<=a.countLimit && ipc::add(a.bytes,total)<=a.byteLimit);
    // Allocate queue publication before changing any visible generation or counter.
    auto queue=a.queue; for(const auto& id:admitted)queue.push_back(key(id));
    std::vector<Key> inserted; inserted.reserve(ids.size());
    try {
        for(auto& entry:candidates) { a.generations.emplace(entry.first,std::move(entry.second)); inserted.push_back(entry.first); }
    } catch(...) { for(auto id:inserted)a.generations.erase(id); throw; }
    a.queue.swap(queue); a.count+=ids.size(); a.bytes+=total; a.nextAdmission=admission;
    a.behindAdmitted+=std::count(behind.begin(),behind.end(),true);
    for(auto id:ids)++handles.at(id).generation;
    return admitted;
}
void Scheduler::limits(const std::string& address, uint64_t count, uint64_t bytes)
{
    require(count>=1 && count<=ipc::MaxCount && bytes>=1 && bytes<=ipc::MaxBytes);
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(keys.at(ipc::addressKey(address)));
    a.countLimit=count; a.byteLimit=bytes;
}
QueueSnapshot Scheduler::snapshot(uint64_t address) const
{
    std::lock_guard<std::mutex> guard(mutex); const auto& a=queues.at(address); QueueSnapshot r;
    r.address=address; r.count=a.count; r.bytes=a.bytes; r.countLimit=a.countLimit; r.byteLimit=a.byteLimit; r.accepting=accepting && a.accepting;
    for(const auto& e:a.generations) {
        const auto& g=*e.second;
        if(g.selected) { if(!g.consumed)++r.undelivered; else if(!g.retired)++r.retirementPending; }
        else if(g.active)++r.active; else ++r.queued;
        if(!g.selected && !g.active && e.first.second>1) {
            const auto previous=a.generations.find(Key(e.first.first,e.first.second-1));
            if(previous!=a.generations.end() && previous->second->consumed && !previous->second->retired)++r.behindRetirement;
        }
    }
    r.behindAdmitted=a.behindAdmitted; r.behindNeverSent=a.behindNeverSent;
    return r;
}
std::vector<uint64_t> Scheduler::addresses() const
{ std::vector<uint64_t> ids; for(const auto& a:queues)ids.push_back(a.first); return ids; }
const std::string& Scheduler::key(uint64_t address) const { return queues.at(address).key; }
bool Scheduler::compatible(uint64_t first, uint64_t following, const Generation& a, const Generation& b) const
{
    const auto& x=*handles.at(first).binding; const auto& y=*handles.at(following).binding;
    return a.command.deadline==b.command.deadline && a.command.operation==b.command.operation &&
           x.endpoint().id==y.endpoint().id && x.profile().id==y.profile().id;
}
Dispatch Scheduler::dispatch(uint64_t address, uint64_t nowUs)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(address); Dispatch d; d.address=address;
    if(!accepting || a.batch || a.queue.empty())return d;
    if(a.nextBatch==UINT64_MAX) { a.accepting=false; return d; }
    const auto first=a.queue.front(); const auto& initial=*a.generations.at(first);
    if(initial.command.deadline<=nowUs)return d;
    const auto maximum=std::min<unsigned>(handles.at(first.first).binding->profile().maxVarbinds,ipc::MaxMembers);
    uint64_t commandBytes=8,resultBytes=8;
    for(auto id:a.queue) {
        const auto& g=*a.generations.at(id);
        if(g.command.deadline<=nowUs || !compatible(first.first,id.first,initial,g) || d.commands.size()>=maximum)break;
        const auto request=ipc::add(commandBytes,ipc::add(48,g.command.value.size()));
        const auto response=ipc::add(resultBytes,ipc::add(48,ipc::responseBytes(handles.at(id.first).binding->definition())));
        if(request>ipc::DataBytes || response>ipc::DataBytes)break;
        d.commands.push_back(g.command); commandBytes=request; resultBytes=response;
    }
    require(!d.commands.empty()); std::vector<Key> active; active.reserve(d.commands.size());
    for(const auto& c:d.commands)active.push_back(key(c.id));
    d.batch=next(a.nextBatch); a.batch=d.batch; a.active=std::move(active);
    for(auto id:a.active) { a.generations.at(id)->active=true; require(a.queue.front()==id); a.queue.pop_front(); }
    return d;
}
void Scheduler::transmitted(uint64_t address, uint64_t batch)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(address); require(a.batch==batch && batch);
    for(auto id:a.active)a.generations.at(id)->sent=true;
}
void Scheduler::select(Generation& g, ipc::Outcome outcome)
{
    if(g.selected)return;
    g.result.id=g.command.id; g.result.outcome=outcome; g.selected=true;
}
void Scheduler::collect(Address& a, const Key& id)
{
    auto it=a.generations.find(id); if(it==a.generations.end())return;
    const auto& g=*it->second;
    if(g.consumed && g.retired) { require(a.count && a.bytes>=g.q); --a.count; a.bytes-=g.q; a.generations.erase(it); }
}
bool Scheduler::matches(const Address& a, uint64_t batch, const std::vector<ipc::Identity>& ids) const
{
    if(!batch || batch!=a.batch || ids.size()!=a.active.size())return false;
    for(size_t i=0;i<ids.size();++i)if(!(a.generations.at(a.active[i])->command.id==ids[i]))return false;
    return true;
}
bool Scheduler::complete(uint64_t address, uint64_t batch, std::vector<ipc::Result> results, uint64_t nowUs)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(address); std::vector<ipc::Identity> ids;
    for(const auto& r:results)ids.push_back(r.id);
    if(!matches(a,batch,ids))return false;
    // Validate the entire frame before committing any terminal ownership.
    for(const auto& r:results) {
        const auto& b=handles.at(r.id.binding).binding->definition();
        require(uint16_t(r.outcome)>=1 && uint16_t(r.outcome)<=6 && r.nativeOutcome<=11);
        if(r.outcome==ipc::Outcome::Complete) {
            require(r.nativeOutcome==1 && r.value.size()<=ipc::responseBytes(b)); ipc::Reader reader(r.value);
            require(reader.oid()==b.oid); ipc::decodeValue(reader,b,false); reader.end();
        } else require(r.value.empty());
    }
    for(auto& r:results) {
        auto& g=*a.generations.at(key(r.id)); if(g.selected)continue;
        if(nowUs>=g.command.deadline)select(g,ipc::Outcome::Deadline);
        else { g.result=std::move(r); g.selected=true; }
    }
    return true;
}
bool Scheduler::retired(uint64_t address, uint64_t batch, const std::vector<ipc::Identity>& ids)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(address); if(!matches(a,batch,ids))return false;
    for(auto id:a.active)require(a.generations.at(id)->selected);
    for(auto id:a.active) { auto& g=*a.generations.at(id); std::vector<uint8_t>().swap(g.command.value); g.retired=true; g.active=false; collect(a,id); }
    a.active.clear(); a.batch=0; return true;
}
bool Scheduler::expire(uint64_t address, uint64_t nowUs)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(address); bool contain=false;
    for(auto it=a.queue.begin();it!=a.queue.end();) {
        auto id=*it; auto& g=*a.generations.at(id);
        if(g.command.deadline>nowUs) { ++it; continue; }
        if(!g.selected && g.behind)++a.behindNeverSent;
        select(g,ipc::Outcome::Deadline); std::vector<uint8_t>().swap(g.command.value); g.retired=true; it=a.queue.erase(it); collect(a,id);
    }
    for(auto id:a.active) {
        auto& g=*a.generations.at(id);
        const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure);
        if(answered) { if(nowUs>=ipc::add(g.command.deadline,RetireGraceUs))contain=true; continue; }
        if(nowUs>=g.command.deadline) { select(g,ipc::Outcome::Deadline); contain=true; }
    }
    return contain;
}
void Scheduler::workerLost(uint64_t address, ipc::Outcome reason)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(address);
    for(auto id:a.active) { auto& g=*a.generations.at(id); if(g.sent || !accepting)select(g,reason); }
}
void Scheduler::reaped(uint64_t address)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(address);
    std::deque<Key> recovered;
    for(auto id:a.active) {
        auto& g=*a.generations.at(id);
        if(!g.selected && !g.sent && accepting) { g.active=false; recovered.push_back(id); }
        else { select(g,ipc::Outcome::WorkerFailure); std::vector<uint8_t>().swap(g.command.value); g.active=false; g.retired=true; collect(a,id); }
    }
    // Recover only zero-byte commands at their original FIFO positions.
    recovered.insert(recovered.end(),a.queue.begin(),a.queue.end()); a.queue.swap(recovered); a.active.clear(); a.batch=0;
}
void Scheduler::stop()
{
    std::lock_guard<std::mutex> guard(mutex); accepting=false;
    for(auto& item:queues) {
        auto& a=item.second;
        for(auto id:a.queue) { auto& g=*a.generations.at(id); select(g,ipc::Outcome::Stopping); std::vector<uint8_t>().swap(g.command.value); g.retired=true; collect(a,id); }
        a.queue.clear();
        for(auto id:a.active)select(*a.generations.at(id),ipc::Outcome::Stopping);
    }
}
void Scheduler::closeAdmission(uint64_t address)
{ std::lock_guard<std::mutex> guard(mutex); queues.at(address).accepting=false; }
TerminalView Scheduler::take(const ipc::Identity& terminal)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(handles.at(terminal.binding).address);
    auto it=a.generations.find(key(terminal)); TerminalView v;
    if(it==a.generations.end() || !(it->second->command.id==terminal) || !it->second->selected ||
       it->second->borrowed || it->second->consumed)return v;
    auto& g=*it->second; g.borrowed=true; v.id=g.command.id; v.result=&g.result; v.sent=g.sent; return v;
}
void Scheduler::release(const ipc::Identity& terminal)
{
    std::lock_guard<std::mutex> guard(mutex); auto& a=queues.at(handles.at(terminal.binding).address);
    auto it=a.generations.find(key(terminal)); require(it!=a.generations.end()); auto& g=*it->second;
    require(g.command.id==terminal && g.selected && g.borrowed && !g.consumed);
    std::vector<uint8_t>().swap(g.result.value); g.consumed=true; g.borrowed=false; collect(a,key(terminal));
}
bool Scheduler::settled() const
{ std::lock_guard<std::mutex> guard(mutex); return std::all_of(queues.begin(),queues.end(),[](const std::pair<const uint64_t,Address>& a) { return a.second.count==0 && a.second.batch==0; }); }
uint64_t Scheduler::earliestDeadline(uint64_t address) const
{
    std::lock_guard<std::mutex> guard(mutex); uint64_t deadline=UINT64_MAX;
    for(const auto& entry:queues.at(address).generations)if(!entry.second->selected)deadline=std::min(deadline,entry.second->command.deadline);
    return deadline;
}
}
