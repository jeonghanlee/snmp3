#include "Ipc.h"
#include "Text.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <system_error>
#include <set>
#include <sys/socket.h>
#include <unistd.h>

namespace snmp3 { namespace ipc {
namespace {
void require(bool condition) { if (!condition) throw std::runtime_error("invalid IPC data"); }
uint64_t contentBytes(const BindingDefinition& b)
{
    switch (b.valueType) {
    case ValueType::Integer: case ValueType::Counter64: case ValueType::OpaqueDouble: return 8;
    case ValueType::Unsigned32: case ValueType::Counter32: case ValueType::Gauge32:
    case ValueType::TimeTicks: case ValueType::IpAddress: case ValueType::OpaqueFloat: return 4;
    case ValueType::Octets: require(b.capacity > 0 && b.capacity <= DefaultBytes); return b.capacity;
    case ValueType::ObjectId: require(b.capacity >= 1 && b.capacity <= 128); return add(4, multiply(4, b.capacity));
    case ValueType::NoSuchObject: case ValueType::NoSuchInstance: case ValueType::EndOfMibView: return 0;
    }
    throw std::runtime_error("invalid Value type");
}
void identity(Writer& w, const Identity& v)
{
    require(v.binding && v.generation && v.admission);
    w.u64(v.binding); w.u64(v.generation); w.u64(v.admission);
}
Identity identity(Reader& r)
{
    Identity v; v.binding=r.u64(); v.generation=r.u64(); v.admission=r.u64();
    require(v.binding && v.generation && v.admission); return v;
}
void members(Writer& w, size_t n) { require(n && n <= MaxMembers); w.u32(n); w.u32(0); }
uint32_t members(Reader& r) { auto n=r.u32(); require(n && n<=MaxMembers && r.u32()==0); return n; }
bool exception(ValueType t)
{
    return t==ValueType::NoSuchObject || t==ValueType::NoSuchInstance || t==ValueType::EndOfMibView;
}
void algorithms(Writer& w, const std::map<std::string, Algorithm>& v)
{
    w.u32(v.size());
    for (const auto& entry : v) {
        w.text(entry.first); w.u32(entry.second.nativeType); w.oid(entry.second.oid);
    }
}
std::map<std::string, Algorithm> algorithms(Reader& r)
{
    std::map<std::string, Algorithm> v; auto n=r.u32(); require(n<=4096);
    r.reserveOwned(multiply(n,sizeof(Algorithm)+128));
    while (n--) {
        Algorithm a; a.name=r.text(128); auto code=r.u32(); require(code>=1 && code<=INT32_MAX);
        a.nativeType=code; a.oid=r.oid();
        r.reserveOwned(add(multiply(2,a.name.capacity()+1),multiply(a.oid.capacity(),4)));
        require(!a.name.empty() && v.emplace(a.name,a).second);
    }
    return v;
}
bool validId(const std::string& id)
{
    return !id.empty() && id.size()<=64 && std::all_of(id.begin(),id.end(),[](unsigned char c) {
        return (c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.';
    });
}
void validateProfile(const Profile& p)
{
    require(validId(p.id) && p.timeoutMs>=1 && p.timeoutMs<=60000 && p.retries<=10 &&
            p.maxVarbinds>=1 && p.maxVarbinds<=1024 && p.user.size()<=32 && p.contextName.size()<=255);
    auto engine=[](const std::vector<uint8_t>& v) {
        return v.empty() || (v.size()>=5 && v.size()<=32 &&
            !std::all_of(v.begin(),v.end(),[](uint8_t b){return b==0;}) &&
            !std::all_of(v.begin(),v.end(),[](uint8_t b){return b==255;}));
    };
    auto secret=[](const std::vector<uint8_t>& v,size_t minimum,size_t maximum) {
        return v.size()>=minimum && v.size()<=maximum &&
            std::none_of(v.begin(),v.end(),[](uint8_t b){return b<32 || b==127;});
    };
    require(engine(p.securityEngineId) && engine(p.contextEngineId));
    if (p.version!=Version::V3) {
        require(secret(p.community,1,255) && p.user.empty() &&
                p.contextName.empty() && p.authSecret.empty() && p.privSecret.empty() &&
                p.authentication.name.empty() && p.privacy.name.empty() &&
                p.securityEngineId.empty() && p.contextEngineId.empty() && p.securityLevel==SecurityLevel::NoAuthNoPriv);
    } else {
        require(!p.user.empty() && p.community.empty());
        const bool auth=p.securityLevel!=SecurityLevel::NoAuthNoPriv;
        const bool priv=p.securityLevel==SecurityLevel::AuthPriv;
        require(auth ? !p.authentication.name.empty() && secret(p.authSecret,8,1024)
                     : p.authentication.name.empty() && p.authSecret.empty());
        require(priv ? !p.privacy.name.empty() && secret(p.privSecret,8,1024)
                     : p.privacy.name.empty() && p.privSecret.empty());
    }
}
ssize_t receive(int fd, void* data, size_t length)
{
    auto n=recv(fd,data,length,MSG_DONTWAIT);
    if (n==0) throw std::runtime_error("IPC peer closed");
    if (n<0 && errno!=EINTR && errno!=EAGAIN && errno!=EWOULDBLOCK) throw std::runtime_error("IPC receive failed");
    return n;
}
}
bool Identity::operator==(const Identity& v) const
{ return binding==v.binding && generation==v.generation && admission==v.admission; }
uint64_t add(uint64_t a, uint64_t b)
{ if (b>UINT64_MAX-a) throw std::overflow_error("IPC size overflow"); return a+b; }
uint64_t multiply(uint64_t a, uint64_t b)
{ if (b && a>UINT64_MAX/b) throw std::overflow_error("IPC size overflow"); return a*b; }
size_t size(uint64_t v) { require(v<=SIZE_MAX); return static_cast<size_t>(v); }
uint32_t ceiling(Kind k)
{
    switch (k) {
    case Kind::Bootstrap: return BootstrapBytes;
    case Kind::Ready: return ReadyBytes;
    case Kind::Batch: case Kind::Result: return DataBytes;
    case Kind::Retired: return RetiredBytes;
    case Kind::Close: case Kind::Closed: return 0;
    case Kind::Fault: return 8;
    }
    throw std::runtime_error("unknown IPC kind");
}
uint16_t tag(ValueType t)
{
    switch (t) {
    case ValueType::Integer:return 1; case ValueType::Unsigned32:return 2;
    case ValueType::Counter32:return 3; case ValueType::Gauge32:return 4;
    case ValueType::TimeTicks:return 5; case ValueType::Counter64:return 6;
    case ValueType::Octets:return 7; case ValueType::ObjectId:return 8;
    case ValueType::IpAddress:return 9; case ValueType::OpaqueFloat:return 10;
    case ValueType::OpaqueDouble:return 11; case ValueType::NoSuchObject:return 12;
    case ValueType::NoSuchInstance:return 13; case ValueType::EndOfMibView:return 14;
    }
    throw std::runtime_error("unknown Value type");
}
bool sameCapabilities(const Capabilities& a, const Capabilities& b)
{
    auto same=[](const std::map<std::string,Algorithm>& x,const std::map<std::string,Algorithm>& y) {
        if(x.size()!=y.size())return false;
        for(const auto& e:x) { auto it=y.find(e.first); if(it==y.end() || it->second.nativeType!=e.second.nativeType || it->second.oid!=e.second.oid)return false; }
        return true;
    };
    return a.nativeVersion==b.nativeVersion && same(a.authentication,b.authentication) && same(a.privacy,b.privacy);
}
std::vector<uint8_t> encodeReady(const Ready& v)
{
    Writer w(ReadyBytes); w.text(v.capabilities.nativeVersion); algorithms(w,v.capabilities.authentication); algorithms(w,v.capabilities.privacy);
    w.raw(v.executable.data(),v.executable.size()); w.u32(v.libraries.size());
    for(const auto& l:v.libraries) { w.text(l.first); w.raw(l.second.data(),l.second.size()); }
    return w.finish();
}
Ready decodeReady(const std::vector<uint8_t>& b)
{
    require(b.size()<=ReadyBytes); Reader r(b.data(),b.size(),131072); r.reserveOwned(sizeof(Ready)+128); Ready v; v.capabilities.nativeVersion=r.text(128);
    v.capabilities.authentication=algorithms(r); v.capabilities.privacy=algorithms(r);
    auto p=r.raw(32); std::copy(p,p+32,v.executable.begin()); auto n=r.u32(); require(n<=r.left()/36);
    r.reserveOwned(multiply(n,128));
    while(n--) { auto path=r.text(4096); require(!path.empty() && path.front()=='/'); std::array<uint8_t,32> sha{}; p=r.raw(32); std::copy(p,p+32,sha.begin()); r.reserveOwned(path.capacity()+1); require(v.libraries.emplace(path,sha).second); }
    r.end(); return v;
}
ValueType type(uint16_t v)
{
    switch (v) {
    case 1:return ValueType::Integer; case 2:return ValueType::Unsigned32;
    case 3:return ValueType::Counter32; case 4:return ValueType::Gauge32;
    case 5:return ValueType::TimeTicks; case 6:return ValueType::Counter64;
    case 7:return ValueType::Octets; case 8:return ValueType::ObjectId;
    case 9:return ValueType::IpAddress; case 10:return ValueType::OpaqueFloat;
    case 11:return ValueType::OpaqueDouble; case 12:return ValueType::NoSuchObject;
    case 13:return ValueType::NoSuchInstance; case 14:return ValueType::EndOfMibView;
    }
    throw std::runtime_error("unknown Value tag");
}
uint64_t responseBytes(const BindingDefinition& b)
{ require(b.oid.size()>=2 && b.oid.size()<=128); return add(add(4,multiply(4,b.oid.size())),add(8,contentBytes(b))); }
uint64_t charge(const BindingDefinition& b)
{
    auto v=add(8,contentBytes(b)); auto total=add(4096+144,multiply(4,add(multiply(4,b.oid.size()),
                                add(b.operation==Operation::Set ? v : 0,responseBytes(b)))));
    return (add(total,63)/64)*64;
}
std::string addressKey(const std::string& address)
{
    require(!address.empty() && address.size()<INET6_ADDRSTRLEN && address.find('\0')==std::string::npos);
    std::array<uint8_t,16> bytes{};
    if (inet_pton(AF_INET,address.c_str(),bytes.data())==1)
        return std::string(1,char(4))+std::string(reinterpret_cast<char*>(bytes.data()),4);
    require(inet_pton(AF_INET6,address.c_str(),bytes.data())==1);
    const bool mapped=std::all_of(bytes.begin(),bytes.begin()+10,[](uint8_t b){return b==0;}) && bytes[10]==255 && bytes[11]==255;
    return mapped ? std::string(1,char(4))+std::string(reinterpret_cast<char*>(bytes.data()+12),4)
                  : std::string(1,char(6))+std::string(reinterpret_cast<char*>(bytes.data()),16);
}
void Writer::raw(const void* p, size_t n)
{
    require(n<=maximum-position); position+=n; if(measuring)return;
    if (n) {
        const size_t required=data.size()+n;
        if(required>data.capacity()) { data.reserve(required); require(data.capacity()==required); }
        const auto* b=static_cast<const uint8_t*>(p); data.insert(data.end(),b,b+n);
    }
    require(data.capacity()<=maximum);
}
void Writer::u16(uint16_t v) { uint8_t b[]={uint8_t(v>>8),uint8_t(v)}; raw(b,2); }
void Writer::u32(uint32_t v) { u16(v>>16); u16(v); }
void Writer::u64(uint64_t v) { u32(v>>32); u32(v); }
void Writer::bytes(const std::vector<uint8_t>& v) { require(v.size()<=UINT32_MAX); u32(v.size()); raw(v.data(),v.size()); }
void Writer::text(const std::string& v) { require(v.size()<=UINT32_MAX); u32(v.size()); raw(v.data(),v.size()); }
void Writer::oid(const std::vector<uint32_t>& v)
{ require(v.size()>=2 && v.size()<=128 && v[0]<=2 && (v[0]==2 || v[1]<=39)); u32(v.size()); for(auto a:v)u32(a); }
const uint8_t* Reader::raw(size_t n) { require(n<=remaining); auto p=data; if(n)data+=n; remaining-=n; return p; }
uint16_t Reader::u16() { auto p=raw(2); return (uint16_t(p[0])<<8)|p[1]; }
uint32_t Reader::u32() { auto hi=u16(); return (uint32_t(hi)<<16)|u16(); }
uint64_t Reader::u64() { auto hi=u32(); return (uint64_t(hi)<<32)|u32(); }
std::vector<uint8_t> Reader::bytes(size_t maximum)
{ auto n=u32(); require(n<=maximum); auto p=raw(n); reserveOwned(n); return n ? std::vector<uint8_t>(p,p+n):std::vector<uint8_t>(); }
std::string Reader::text(size_t maximum)
{ auto n=u32(); require(n<=maximum); auto p=raw(n); reserveOwned(std::max<uint64_t>(n,15)+1); require(validUtf8(p,n) && std::none_of(p,p+n,[](uint8_t b){return b<32 || b==127;})); return n ? std::string(reinterpret_cast<const char*>(p),n):std::string(); }
std::vector<uint32_t> Reader::oid()
{
    auto n=u32(); require(n>=2 && n<=128 && multiply(n,4)<=left());
    reserveOwned(multiply(n,4));
    std::vector<uint32_t> v; v.reserve(n); while(n--)v.push_back(u32());
    require(v[0]<=2 && (v[0]==2 || v[1]<=39)); return v;
}
void Reader::end() const { require(remaining==0); }
void Reader::reserveOwned(uint64_t bytes) { owned=add(owned,bytes); require(owned<=ownedLimit); }
std::array<uint8_t,HeaderBytes> encodeHeader(const Header& v)
{
    const bool control=v.kind==Kind::Bootstrap || v.kind==Kind::Ready || v.kind==Kind::Close || v.kind==Kind::Closed || v.kind==Kind::Fault;
    require(v.activation && v.epoch && v.address && v.revision && v.sequence &&
            (control ? v.batch==0 : v.batch!=0) && v.length<=ceiling(v.kind) &&
            ((v.kind==Kind::Close || v.kind==Kind::Closed) ? v.length==0 : v.length!=0));
    Writer w(HeaderBytes,HeaderBytes); w.u32(0x46534e35); w.u16(1); w.u16(uint16_t(v.kind)); w.u32(v.length); w.u32(0);
    w.u64(v.activation); w.u64(v.epoch); w.u64(v.address); w.u64(v.revision); w.u64(v.batch); w.u64(v.sequence);
    auto b=w.finish(); std::array<uint8_t,HeaderBytes> r{}; std::copy(b.begin(),b.end(),r.begin()); return r;
}
Header decodeHeader(const std::array<uint8_t,HeaderBytes>& b)
{
    Reader r(b.data(),b.size()); require(r.u32()==0x46534e35 && r.u16()==1); Header v;
    v.kind=Kind(r.u16()); v.length=r.u32(); require(r.u32()==0);
    v.activation=r.u64(); v.epoch=r.u64(); v.address=r.u64(); v.revision=r.u64(); v.batch=r.u64(); v.sequence=r.u64();
    encodeHeader(v); return v;
}
std::vector<uint8_t> encodeValue(const Value& v)
{
    Writer content(DataBytes);
    switch(v.type()) {
    case ValueType::Integer: { auto n=v.integer(); uint64_t bits; std::memcpy(&bits,&n,8); content.u64(bits); break; }
    case ValueType::Unsigned32: case ValueType::Counter32: case ValueType::Gauge32: case ValueType::TimeTicks: content.u32(v.unsigned32()); break;
    case ValueType::Counter64: content.u64(v.counter64()); break;
    case ValueType::Octets: content.raw(v.octets().data(),v.octets().size()); break;
    case ValueType::ObjectId: content.oid(v.objectId()); break;
    case ValueType::IpAddress: { auto bytes=v.ipAddress(); content.raw(bytes.data(),4); break; }
    case ValueType::OpaqueFloat: { static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"binary32 required"); auto n=v.opaqueFloat(); uint32_t bits; std::memcpy(&bits,&n,4); content.u32(bits); break; }
    case ValueType::OpaqueDouble: { static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,"binary64 required"); auto n=v.opaqueDouble(); uint64_t bits; std::memcpy(&bits,&n,8); content.u64(bits); break; }
    case ValueType::NoSuchObject: case ValueType::NoSuchInstance: case ValueType::EndOfMibView: break;
    }
    auto b=content.finish(); Writer w(DataBytes); w.u16(tag(v.type())); w.u16(0); w.bytes(b); return w.finish();
}
Value decodeValue(Reader& r, const BindingDefinition& b, bool set)
{
    auto t=type(r.u16()); require(r.u16()==0); auto n=r.u32();
    require((t==b.valueType || (!set && exception(t))) && n<=contentBytes(b));
    Reader v(r.raw(n),n);
    Value result=Value::integer(0);
    switch(t) {
    case ValueType::Integer: { auto bits=v.u64(); int64_t data; std::memcpy(&data,&bits,8); result=Value::integer(data); break; }
    case ValueType::Unsigned32: case ValueType::Counter32: case ValueType::Gauge32: case ValueType::TimeTicks: result=Value::unsigned32(t,v.u32()); break;
    case ValueType::Counter64: result=Value::counter64(v.u64()); break;
    case ValueType::Octets: { auto p=v.raw(n); result=Value::octets(std::vector<uint8_t>(p,p+n)); break; }
    case ValueType::ObjectId: { auto data=v.oid(); require(data.size()<=b.capacity); result=Value::objectId(std::move(data)); break; }
    case ValueType::IpAddress: { std::array<uint8_t,4> data{}; auto p=v.raw(4); std::copy(p,p+4,data.begin()); result=Value::ipAddress(data); break; }
    case ValueType::OpaqueFloat: { auto bits=v.u32(); float data; std::memcpy(&data,&bits,4); result=Value::opaqueFloat(data); break; }
    case ValueType::OpaqueDouble: { auto bits=v.u64(); double data; std::memcpy(&data,&bits,8); result=Value::opaqueDouble(data); break; }
    case ValueType::NoSuchObject: case ValueType::NoSuchInstance: case ValueType::EndOfMibView: require(!set); result=Value::exception(t); break;
    }
    v.end(); return result;
}
std::vector<uint8_t> encodeCommands(const std::vector<Command>& v)
{
    uint64_t length=8; for(const auto& c:v)length=add(length,add(48,c.value.size()));
    Writer w(DataBytes,size(length)); members(w,v.size());
    for(const auto& c:v) {
        require(c.definition && c.deadline && (c.operation==Operation::Set || c.value.empty()));
        w.u64(c.definition); identity(w,c.id); w.u64(c.deadline); w.u16(c.operation==Operation::Get ? 1:2); w.u16(0); w.bytes(c.value);
    }
    return w.finish();
}
std::vector<Command> decodeCommands(const std::vector<uint8_t>& b)
{
    require(b.size()<=DataBytes); Reader r(b); auto n=members(r); require(n<=r.left()/48); std::vector<Command> v; v.reserve(n);
    while(n--) { Command c; c.definition=r.u64(); c.id=identity(r); c.deadline=r.u64(); auto op=r.u16(); require(op==1 || op==2); c.operation=op==1 ? Operation::Get:Operation::Set; require(r.u16()==0); c.value=r.bytes(DataBytes); require(c.definition && c.deadline && (op==2 || c.value.empty())); v.push_back(std::move(c)); }
    r.end(); return v;
}
std::vector<uint8_t> encodeResults(const std::vector<Result>& v)
{
    uint64_t length=8; for(const auto& c:v)length=add(length,add(48,c.value.size()));
    Writer w(DataBytes,size(length)); members(w,v.size());
    for(const auto& c:v) {
        require(uint16_t(c.outcome)>=1 && uint16_t(c.outcome)<=6 && c.nativeOutcome<=11 && (c.outcome==Outcome::Complete || c.value.empty()));
        identity(w,c.id); w.u16(uint16_t(c.outcome)); w.u16(c.nativeOutcome); w.u32(c.value.size());
        uint64_t bits; std::memcpy(&bits,&c.errorStatus,8); w.u64(bits); std::memcpy(&bits,&c.errorIndex,8); w.u64(bits); w.raw(c.value.data(),c.value.size());
    }
    return w.finish();
}
std::vector<Result> decodeResults(const std::vector<uint8_t>& b)
{
    require(b.size()<=DataBytes); Reader r(b); auto n=members(r); require(n<=r.left()/48); std::vector<Result> v; v.reserve(n);
    while(n--) {
        Result c; c.id=identity(r); auto op=r.u16(); require(op>=1 && op<=6); c.outcome=Outcome(op); c.nativeOutcome=r.u16(); require(c.nativeOutcome<=11);
        auto len=r.u32(); auto bits=r.u64(); std::memcpy(&c.errorStatus,&bits,8); bits=r.u64(); std::memcpy(&c.errorIndex,&bits,8);
        require(len<=DataBytes && (c.outcome==Outcome::Complete || len==0)); auto p=r.raw(len); c.value.assign(p,p+len); v.push_back(std::move(c));
    }
    r.end(); return v;
}
std::vector<uint8_t> encodeRetired(const std::vector<Identity>& v)
{ Writer w(RetiredBytes,size(add(8,multiply(v.size(),24)))); members(w,v.size()); for(const auto& id:v)identity(w,id); return w.finish(); }
std::vector<Result> decodeResults(const std::vector<uint8_t>& b,
                                  const std::vector<ResultReservation>& expected)
{
    require(b.size()<=DataBytes); Reader r(b); const auto count=members(r);
    require(count==expected.size());
    uint64_t maximum=8;
    for(const auto& e:expected) { require(e.definition); maximum=add(maximum,add(48,responseBytes(*e.definition))); }
    require(b.size()<=maximum);
    for(const auto& e:expected) {
        if(!(identity(r)==e.id))throw StaleIdentity();
        const auto outcome=r.u16(); require(outcome>=1 && outcome<=6);
        require(r.u16()<=11); const auto length=r.u32(); r.u64(); r.u64();
        require(length<=responseBytes(*e.definition) && (outcome==1 || length==0));
        r.raw(length);
    }
    r.end(); return decodeResults(b);
}
std::vector<Identity> decodeRetired(const std::vector<uint8_t>& b)
{ require(b.size()<=RetiredBytes); Reader r(b); auto n=members(r); require(n<=r.left()/24); std::vector<Identity> v; v.reserve(n); while(n--)v.push_back(identity(r)); r.end(); return v; }
namespace {
void bootstrap(Writer& w,const Configuration& c,const std::string& address)
{
    require(multiply(add(c.profiles.size(),c.endpoints.size()),640)<=BootstrapBytes);
    w.u32(1); w.text(c.capabilities.nativeVersion); algorithms(w,c.capabilities.authentication); algorithms(w,c.capabilities.privacy);
    std::map<std::string,uint64_t> profiles,endpoints; std::set<std::string> selectedEndpoints,selectedProfiles;
    uint64_t id=0;
    for(const auto& p:c.profiles)profiles[p.first]=++id;
    id=0;
    for(const auto& e:c.endpoints) {
        endpoints[e.first]=++id;
        if(address.empty() || addressKey(e.second.address)==address) { selectedEndpoints.insert(e.first); selectedProfiles.insert(e.second.profile); }
    }
    w.u32(selectedProfiles.size());
    for(const auto& e:c.profiles) {
        if(!selectedProfiles.count(e.first))continue;
        const auto& p=e.second; validateProfile(p); w.u64(profiles.at(e.first)); w.text(e.first);
        w.u16(p.version==Version::V1 ? 1:p.version==Version::V2c ? 2:3); w.u16(p.securityLevel==SecurityLevel::NoAuthNoPriv ? 1:p.securityLevel==SecurityLevel::AuthNoPriv ? 2:3);
        w.u32(p.timeoutMs); w.u32(p.retries); w.u32(p.maxVarbinds); w.u32(p.checkRanges ? 1:0); w.text(p.user); w.text(p.contextName); w.text(p.authentication.name); w.text(p.privacy.name);
        w.bytes(p.community); w.bytes(p.authSecret); w.bytes(p.privSecret); w.bytes(p.securityEngineId); w.bytes(p.contextEngineId);
    }
    w.u32(selectedEndpoints.size());
    for(const auto& e:c.endpoints) { if(!selectedEndpoints.count(e.first))continue; w.u64(endpoints.at(e.first)); w.text(e.first); w.text(e.second.address); w.u32(e.second.port); w.u64(profiles.at(e.second.profile)); }
    uint32_t selectedBindings=0;
    for(const auto& b:c.bindings)if(selectedEndpoints.count(b.second.endpoint))++selectedBindings;
    id=0; w.u32(selectedBindings);
    for(const auto& e:c.bindings) { ++id; const auto& b=e.second; if(!selectedEndpoints.count(b.endpoint))continue; w.u64(id); w.text(e.first); w.u64(endpoints.at(b.endpoint)); w.oid(b.oid); w.u16(b.operation==Operation::Get ? 1:2); w.u16(tag(b.valueType)); require(b.capacity<=UINT32_MAX); w.u32(b.capacity); }
}
}
size_t bootstrapBytes(const Configuration& c,const std::string& address)
{ Writer w(BootstrapBytes,0,true); bootstrap(w,c,address); return w.length(); }
uint64_t bootstrapOwnedBytes(const Configuration& c,const std::string& address)
{
    require(multiply(add(c.profiles.size(),c.endpoints.size()),640)<=BootstrapBytes);
    auto text=[](const std::string& s) { return add(std::max<uint64_t>(15,s.size()),1); };
    uint64_t total=add(sizeof(Configuration)+128,text(c.capabilities.nativeVersion));
    for(const auto* catalogue:{&c.capabilities.authentication,&c.capabilities.privacy})
        for(const auto& entry:*catalogue)total=add(total,add(sizeof(Algorithm)+128,add(multiply(text(entry.first),3),multiply(entry.second.oid.size(),8))));
    std::set<std::string> endpoints,profiles;
    for(const auto& e:c.endpoints)if(addressKey(e.second.address)==address) { endpoints.insert(e.first); profiles.insert(e.second.profile); }
    for(const auto& name:profiles) {
        const auto& p=c.profiles.at(name); uint64_t bytes=sizeof(Profile)+384+192;
        for(const auto* s:{&p.id,&p.user,&p.contextName,&p.authentication.name,&p.privacy.name})bytes=add(bytes,text(*s));
        for(const auto* a:{&p.authentication,&p.privacy})if(!a->name.empty())bytes=add(bytes,add(text(a->name),multiply(a->oid.size(),4)));
        for(const auto* secret:{&p.community,&p.authSecret,&p.privSecret,&p.securityEngineId,&p.contextEngineId})bytes=add(bytes,secret->size());
        total=add(total,bytes);
    }
    for(const auto& name:endpoints) { const auto& e=c.endpoints.at(name); total=add(total,add(sizeof(Endpoint)+384+192,add(text(e.id),text(e.address)))); }
    for(const auto& entry:c.bindings)if(endpoints.count(entry.second.endpoint))
        total=add(total,add(sizeof(BindingDefinition)+sizeof(Binding)+512+256,add(text(entry.second.id),multiply(entry.second.oid.size(),4))));
    return total;
}
std::vector<uint8_t> encodeBootstrap(const Configuration& c,const std::string& address)
{
    Writer w(BootstrapBytes,bootstrapBytes(c,address)); bootstrap(w,c,address); return w.finish();
}
std::shared_ptr<const Configuration> decodeBootstrap(const std::vector<uint8_t>& bytes, std::map<uint64_t,std::string>* definitions)
{
    require(bytes.size()<=BootstrapBytes); Reader r(bytes.data(),bytes.size(),33554432); r.reserveOwned(sizeof(Configuration)+128); require(r.u32()==1); auto c=std::make_shared<Configuration>();
    c->capabilities.nativeVersion=r.text(128); c->capabilities.authentication=algorithms(r); c->capabilities.privacy=algorithms(r);
    std::map<uint64_t,std::string> profiles,endpoints;
    auto count=r.u32(); require(count<=4096 && count<=r.left()/64);
    r.reserveOwned(multiply(count,sizeof(Profile)+384+192));
    uint64_t previous=0;
    for(uint64_t index=0;index<count;++index) {
        Profile p; const auto id=r.u64(); require(id>previous); previous=id; p.id=r.text(64); auto v=r.u16(); require(v>=1 && v<=3); p.version=v==1 ? Version::V1:v==2 ? Version::V2c:Version::V3;
        auto level=r.u16(); require(level>=1 && level<=3); p.securityLevel=level==1 ? SecurityLevel::NoAuthNoPriv:level==2 ? SecurityLevel::AuthNoPriv:SecurityLevel::AuthPriv;
        p.timeoutMs=r.u32(); p.retries=r.u32(); p.maxVarbinds=r.u32(); auto ranges=r.u32(); require(ranges<=1); p.checkRanges=ranges;
        p.user=r.text(255); p.contextName=r.text(255); auto auth=r.text(128),priv=r.text(128);
        if(!auth.empty()) { const auto& a=c->capabilities.authentication.at(auth); r.reserveOwned(a.name.capacity()+1+multiply(a.oid.capacity(),4)); p.authentication=a; }
        if(!priv.empty()) { const auto& a=c->capabilities.privacy.at(priv); r.reserveOwned(a.name.capacity()+1+multiply(a.oid.capacity(),4)); p.privacy=a; }
        p.community=r.bytes(1024); p.authSecret=r.bytes(1024); p.privSecret=r.bytes(1024); p.securityEngineId=r.bytes(32); p.contextEngineId=r.bytes(32); validateProfile(p);
        profiles[id]=p.id; const auto name=p.id; require(c->profiles.emplace(name,std::move(p)).second);
    }
    count=r.u32(); require(count<=4096 && count<=r.left()/28);
    r.reserveOwned(multiply(count,sizeof(Endpoint)+384+192));
    previous=0;
    for(uint64_t index=0;index<count;++index) { Endpoint e; const auto id=r.u64(); require(id>previous); previous=id; e.id=r.text(64); e.address=r.text(INET6_ADDRSTRLEN); require(e.address.find('\0')==std::string::npos); addressKey(e.address); e.port=r.u32(); e.profile=profiles.at(r.u64()); require(validId(e.id) && e.port && e.port<=65535); endpoints[id]=e.id; const auto name=e.id; require(c->endpoints.emplace(name,std::move(e)).second); }
    count=r.u32(); require(count<=4096 && count<=r.left()/36);
    r.reserveOwned(multiply(count,sizeof(BindingDefinition)+sizeof(Binding)+512+256));
    std::map<uint64_t,std::string> decodedDefinitions; previous=0;
    for(uint64_t index=0;index<count;++index) { BindingDefinition b; const auto id=r.u64(); require(id>previous); previous=id; b.id=r.text(64); b.endpoint=endpoints.at(r.u64()); b.oid=r.oid(); auto op=r.u16(); require(op==1 || op==2); b.operation=op==1 ? Operation::Get:Operation::Set; b.valueType=type(r.u16()); b.capacity=r.u32(); require(validId(b.id) && !exception(b.valueType)); if(b.valueType!=ValueType::Octets && b.valueType!=ValueType::ObjectId)require(b.capacity==1); contentBytes(b); decodedDefinitions[id]=b.id; const auto name=b.id; require(c->bindings.emplace(name,std::move(b)).second); }
    r.end(); if(definitions)*definitions=std::move(decodedDefinitions); return c;
}
Channel::Channel(int value) : fd(value)
{
    require(fd>=0); auto flags=fcntl(fd,F_GETFL); if(flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0) { ::close(fd); fd=-1; throw std::runtime_error("IPC nonblocking failed"); }
}
Channel::~Channel() { close(); }
void Channel::close()
{
    if(fd>=0)::close(fd);
    fd=-1; std::vector<uint8_t>().swap(tx); std::vector<uint8_t>().swap(rx);
    txHeaderOffset=HeaderBytes; txOffset=rxHeaderOffset=rxOffset=0; bodyAccepted=false; rxOrigin=0;
}
void Channel::queue(Header h, std::vector<uint8_t> body)
{ require(!writing() && body.size()<=ceiling(h.kind)); h.length=body.size(); txHeader=encodeHeader(h); tx=std::move(body); txHeaderOffset=txOffset=0; sent=0; }
bool Channel::write(size_t& budget)
{
    while(budget && writing()) {
        bool header=txHeaderOffset<HeaderBytes; auto length=header ? HeaderBytes-txHeaderOffset:tx.size()-txOffset;
        auto p=header ? txHeader.data()+txHeaderOffset:tx.data()+txOffset;
        auto n=send(fd,p,std::min(budget,length),MSG_DONTWAIT|MSG_NOSIGNAL);
        if(n<0) { if(errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)return false; throw std::system_error(errno,std::generic_category(),"IPC send failed"); }
        require(n>0); budget-=n; sent=add(sent,n); if(header)txHeaderOffset+=n; else txOffset+=n;
    }
    if(!writing()) { std::vector<uint8_t>().swap(tx); return true; } return false;
}
bool Channel::readHeader(size_t& budget, uint64_t now, Header& header)
{
    require(!bodyAccepted);
    while(budget && rxHeaderOffset<HeaderBytes) {
        auto n=receive(fd,rxHeader.data()+rxHeaderOffset,std::min(budget,HeaderBytes-rxHeaderOffset)); if(n<0)return false;
        if(!rxHeaderOffset)rxOrigin=now;
        rxHeaderOffset+=n; budget-=n;
    }
    if(rxHeaderOffset!=HeaderBytes)return false;
    header=decodeHeader(rxHeader); rxLength=header.length; return true;
}
void Channel::acceptBody(bool retain)
{ require(rxHeaderOffset==HeaderBytes && !bodyAccepted); retaining=retain; if(retain)rx.resize(rxLength); rxOffset=0; bodyAccepted=true; }
bool Channel::readBody(size_t& budget, std::vector<uint8_t>& body)
{
    require(bodyAccepted);
    while(budget && rxOffset<rxLength) {
        auto length=std::min(budget,size_t(rxLength-rxOffset)); if(!retaining)length=std::min(length,discard.size());
        auto n=receive(fd,retaining ? rx.data()+rxOffset:discard.data(),length); if(n<0)return false; rxOffset+=n; budget-=n;
    }
    if(rxOffset!=rxLength)return false;
    if(retaining)body=std::move(rx); else body.clear(); std::vector<uint8_t>().swap(rx);
    rxHeaderOffset=0; rxOffset=0; rxOrigin=0; bodyAccepted=false; return true;
}
size_t Channel::pull(size_t& budget, uint8_t* target, size_t capacity)
{
    require(bodyAccepted && !retaining && capacity<=rxLength-rxOffset);
    const auto length=std::min(budget,capacity); if(!length)return 0;
    const auto n=receive(fd,target,length); if(n<0)return 0;
    budget-=n; rxOffset+=n; return size_t(n);
}
void Channel::finishBody()
{
    require(bodyAccepted && !retaining && rxOffset==rxLength);
    rxHeaderOffset=rxOffset=0; rxOrigin=0; bodyAccepted=false;
}
void BatchReader::begin(Channel& channel, uint32_t length, uint64_t floor)
{
    require(!started && length>=56 && length<=DataBytes && pending.empty());
    channel.acceptBody(false); remaining=length; count=valueLength=0;
    prefixUsed=valueUsed=0; haveCount=haveMember=false; started=true;
    admissionFloor=floor;
}
void BatchReader::discard()
{ std::vector<Command>().swap(pending); started=false; }
bool BatchReader::read(Channel& channel, size_t& budget, std::vector<Command>& commands)
{
    require(started);
    while(budget) {
        if(!haveCount || !haveMember) {
            const size_t needed=haveCount ? 48:8;
            require(needed-prefixUsed<=remaining);
            const auto n=channel.pull(budget,prefix.data()+prefixUsed,needed-prefixUsed);
            prefixUsed+=n; remaining-=n; if(prefixUsed!=needed)return false;
            Reader r(prefix.data(),needed); prefixUsed=0;
            if(!haveCount) {
                count=members(r); require(count<=remaining/48);
                pending.reserve(count); haveCount=true; continue;
            }
            Command c; c.definition=r.u64(); c.id=identity(r); c.deadline=r.u64();
            if(c.id.admission<=admissionFloor)throw StaleIdentity();
            const auto operation=r.u16(); require(operation==1 || operation==2); require(r.u16()==0);
            c.operation=operation==1 ? Operation::Get:Operation::Set; valueLength=r.u32();
            const auto& binding=*definitions.at(c.definition); const auto& spec=binding.definition();
            const auto maximum=responseBytes(spec)-4-multiply(spec.oid.size(),4);
            require(c.deadline && count<=binding.profile().maxVarbinds && c.operation==spec.operation);
            require(valueLength<=remaining && valueLength<=maximum && (operation==2 || valueLength==0));
            require(add(8,multiply(count,48))<=DataBytes);
            if(!pending.empty()) {
                const auto& first=pending.front(); const auto& prior=pending.back();
                require(c.deadline==first.deadline && c.operation==first.operation && c.id.admission>prior.id.admission);
                require(spec.endpoint==definitions.at(first.definition)->definition().endpoint);
                for(const auto& existing:pending)require(existing.id.binding!=c.id.binding);
            }
            c.value.resize(valueLength); require(c.value.capacity()==valueLength);
            pending.push_back(std::move(c)); valueUsed=0; haveMember=true;
        }
        auto& c=pending.back();
        const auto n=valueLength==valueUsed ? 0:channel.pull(budget,c.value.data()+valueUsed,valueLength-valueUsed);
        valueUsed+=n; remaining-=n; if(valueUsed!=valueLength)return false;
        if(c.operation==Operation::Set) { Reader r(c.value); decodeValue(r,definitions.at(c.definition)->definition(),true); r.end(); }
        haveMember=false;
        if(pending.size()==count) {
            require(remaining==0);
            uint64_t response=8;
            for(const auto& member:pending)response=add(response,add(48,responseBytes(definitions.at(member.definition)->definition())));
            require(response<=DataBytes); channel.finishBody(); commands=std::move(pending); started=false; return true;
        }
    }
    return false;
}
}}
