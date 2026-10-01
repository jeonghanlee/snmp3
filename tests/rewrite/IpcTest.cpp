#include "Ipc.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>

using namespace snmp3;
namespace {
unsigned checks=0;
void check(bool v) { ++checks; if(!v)throw std::runtime_error("IPC assertion failed"); }
void rejects(const std::function<void()>& f)
{ bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; } check(rejected); }
void value(const Value& v, size_t capacity)
{
    BindingDefinition b; b.valueType=v.type(); b.capacity=capacity; b.oid={1,3,6,1,4,1,53864,1,0};
    auto encoded=ipc::encodeValue(v); ipc::Reader r(encoded); auto decoded=ipc::decodeValue(r,b,false); r.end();
    check(ipc::encodeValue(decoded)==encoded);
    for(size_t n=0;n<encoded.size();++n) {
        ipc::Reader truncated(encoded.data(),n); rejects([&]{ipc::decodeValue(truncated,b,false);});
    }
    auto bad=encoded; bad[2]=1; ipc::Reader reserved(bad); rejects([&]{ipc::decodeValue(reserved,b,false);});
}
void headers()
{
    ipc::Header h; h.kind=ipc::Kind::Batch; h.length=56; h.activation=1; h.epoch=2; h.address=3; h.revision=4; h.batch=5; h.sequence=6;
    auto b=ipc::encodeHeader(h); auto got=ipc::decodeHeader(b);
    check(got.length==56 && got.activation==1 && got.epoch==2 && got.address==3 && got.revision==4 && got.batch==5 && got.sequence==6);
    for(unsigned i=0;i<4;++i) { auto bad=b; bad[i]^=1; rejects([&]{ipc::decodeHeader(bad);}); }
    for(unsigned i:{4u,5u,12u,13u,14u,15u}) { auto bad=b; bad[i]=255; rejects([&]{ipc::decodeHeader(bad);}); }
    h.length=ipc::DataBytes+1; rejects([&]{ipc::encodeHeader(h);});
    h.kind=ipc::Kind::Close; h.length=0; h.batch=0; check(ipc::decodeHeader(ipc::encodeHeader(h)).length==0);
    h.length=1; rejects([&]{ipc::encodeHeader(h);});
    h.kind=ipc::Kind(99); rejects([&]{ipc::encodeHeader(h);});
}
void accounting()
{
    BindingDefinition b; b.oid={1,3,6,1,4,1,53864,1,0};
    check(ipc::charge(b)==4608); check(ipc::responseBytes(b)==56); check(ipc::DefaultBytes/ipc::charge(b)==227);
    b.operation=Operation::Set; check(ipc::charge(b)==4672);
    b.oid.resize(128,1); b.valueType=ValueType::Octets; b.capacity=1048576;
    check(ipc::charge(b)==8397056); b.operation=Operation::Get; check(ipc::charge(b)==4202688);
    rejects([]{ipc::add(UINT64_MAX,1);}); rejects([]{ipc::multiply(UINT64_MAX,2);});
    check(ipc::addressKey("127.0.0.1")==ipc::addressKey("::ffff:127.0.0.1"));
    check(ipc::addressKey("::1")==ipc::addressKey("0:0:0:0:0:0:0:1"));
    check(ipc::addressKey("::1")!=ipc::addressKey("127.0.0.1"));
    rejects([]{ipc::addressKey("localhost");}); rejects([]{ipc::addressKey("fe80::1%1");});
    rejects([]{ipc::addressKey(std::string("127.0.0.1\0suffix",16));});
}
void bodies()
{
    std::vector<ipc::Command> commands(1024);
    for(size_t i=0;i<commands.size();++i) { auto& c=commands[i]; c.definition=1; c.id.binding=i+1; c.id.generation=1; c.id.admission=i+1; c.deadline=1000; }
    auto b=ipc::encodeCommands(commands); check(b.size()==49160); auto got=ipc::decodeCommands(b); check(got.size()==1024 && got.back().id==commands.back().id);
    auto extra=b; extra.push_back(0); rejects([&]{ipc::decodeCommands(extra);});
    for(size_t n:{size_t(0),size_t(7),b.size()-1}) { std::vector<uint8_t> trunc(b.begin(),b.begin()+n); rejects([&]{ipc::decodeCommands(trunc);}); }
    std::vector<ipc::Identity> retired; std::vector<ipc::Result> results;
    for(const auto& c:commands) { retired.push_back(c.id); ipc::Result r; r.id=c.id; r.outcome=ipc::Outcome::Deadline; results.push_back(std::move(r)); }
    auto r=ipc::encodeRetired(retired); check(r.size()==24584 && ipc::decodeRetired(r).back()==retired.back());
    auto out=ipc::encodeResults(results); check(out.size()==49160 && ipc::decodeResults(out).size()==1024);
    commands.push_back(commands.front()); rejects([&]{ipc::encodeCommands(commands);});
}
void bootstrap()
{
    Configuration c; c.capabilities.nativeVersion="fixture"; Profile p; p.id="Default"; p.community={'a','b'}; c.profiles[p.id]=p;
    Endpoint e; e.id="Device"; e.address="127.0.0.1"; e.profile=p.id; c.endpoints[e.id]=e;
    BindingDefinition b; b.id="Read"; b.endpoint=e.id; b.oid={1,3,6,1,4,1,53864,1,0}; c.bindings[b.id]=b;
    auto data=ipc::encodeBootstrap(c); auto got=ipc::decodeBootstrap(data);
    auto invalid=c; invalid.profiles.at(p.id).community={0}; rejects([&]{ipc::encodeBootstrap(invalid);});
    invalid=c; invalid.profiles.at(p.id).community.resize(256,'a'); rejects([&]{ipc::encodeBootstrap(invalid);});
    std::vector<uint8_t> text={0,0,0,2,0xc0,0x80}; ipc::Reader textReader(text); rejects([&]{textReader.text(255);});
    check(got->profiles.at(p.id).community==p.community && got->bindings.at(b.id).oid==b.oid);
    for(size_t n=0;n<data.size();++n) { std::vector<uint8_t> trunc(data.begin(),data.begin()+n); rejects([&]{ipc::decodeBootstrap(trunc);}); }
    p.id="Zoo"; c.profiles[p.id]=p; e.id="Peer"; e.profile=p.id; e.address="127.0.0.2"; c.endpoints[e.id]=e;
    b.id="ZooRead"; b.endpoint=e.id; c.bindings[b.id]=b;
    std::map<uint64_t,std::string> definitions;
    got=ipc::decodeBootstrap(ipc::encodeBootstrap(c,ipc::addressKey("127.0.0.2")),&definitions);
    check(got->profiles.size()==1 && got->profiles.count("Zoo") && got->endpoints.size()==1 && definitions.at(2)=="ZooRead");
    ipc::Ready ready; ready.capabilities=c.capabilities; ready.executable.fill(7); ready.libraries["/fixture/library.so"].fill(8);
    auto observed=ipc::decodeReady(ipc::encodeReady(ready));
    check(ipc::sameCapabilities(observed.capabilities,ready.capabilities) && observed.executable==ready.executable && observed.libraries==ready.libraries);
}
void channel()
{
    int sockets[2]; check(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,sockets)==0);
    ipc::Channel sender(sockets[0]),receiver(sockets[1]);
    ipc::Header h; h.kind=ipc::Kind::Batch; h.activation=h.epoch=h.address=h.revision=h.batch=h.sequence=1;
    ipc::Command c; c.definition=c.deadline=c.id.binding=c.id.generation=c.id.admission=1;
    auto bytes=ipc::encodeCommands({c}); sender.queue(h,bytes);
    bool header=false,complete=false; std::vector<uint8_t> body;
    for(unsigned i=0;i<256 && !complete;++i) {
        size_t write=1; sender.write(write); size_t read=1;
        if(!header) { header=receiver.readHeader(read,i+1,h); if(header)receiver.acceptBody(true); }
        if(header)complete=receiver.readBody(read,body);
    }
    check(complete && body==bytes && sender.bytesSent()==64+bytes.size());
    sender.close(); size_t budget=64; rejects([&]{receiver.readHeader(budget,1,h);});
    check(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,sockets)==0);
    ipc::Channel blocked(sockets[0]); h.kind=ipc::Kind::Bootstrap; h.batch=0;
    blocked.queue(h,std::vector<uint8_t>(ipc::BootstrapBytes,1)); size_t quota=ipc::BootstrapBytes+64;
    check(!blocked.write(quota) && blocked.bytesSent()>0 && quota>0); ::close(sockets[1]);
    quota=64; rejects([&]{blocked.write(quota);});
}
void streaming()
{
    auto config=std::make_shared<Configuration>(); Profile p; p.id="Default"; p.maxVarbinds=2; p.community={1};
    config->profiles[p.id]=p; Endpoint e; e.id="Device"; e.address="127.0.0.1"; e.profile=p.id; config->endpoints[e.id]=e;
    BindingDefinition spec; spec.id="Write"; spec.endpoint=e.id; spec.oid={1,3,6,1,4,1,53864,1,0};
    spec.operation=Operation::Set; spec.valueType=ValueType::Octets; spec.capacity=4; config->bindings[spec.id]=spec;
    ipc::BatchReader::Definitions definitions; definitions[1]=std::make_shared<Binding>(config,spec);
    ipc::Command c; c.definition=c.deadline=c.id.binding=c.id.generation=c.id.admission=1;
    c.operation=Operation::Set; c.value=ipc::encodeValue(Value::octets({0,1,0,255}));
    auto bytes=ipc::encodeCommands({c});
    auto run=[&](const std::vector<uint8_t>& wire,bool valid) {
        int sockets[2]; check(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,sockets)==0);
        ipc::Channel sender(sockets[0]),receiver(sockets[1]); ipc::BatchReader reader(definitions);
        ipc::Header h; h.kind=ipc::Kind::Batch; h.activation=h.epoch=h.address=h.revision=h.batch=h.sequence=1;
        sender.queue(h,wire); bool header=false,complete=false; std::vector<ipc::Command> commands;
        auto receive=[&] {
            for(unsigned i=0;i<1024 && !complete;++i) {
                size_t write=1; sender.write(write); size_t read=1;
                if(!header) { header=receiver.readHeader(read,1,h); if(header)reader.begin(receiver,h.length); }
                if(header)complete=reader.read(receiver,read,commands);
            }
            check(complete && commands.size()==1 && commands[0].id==c.id && commands[0].value==c.value);
        };
        if(valid)receive(); else rejects(receive);
    };
    run(bytes,true);
    auto bad=bytes; bad[55]=13; run(bad,false);
    bad=bytes; bad[48]=0; bad[49]=1; run(bad,false);
    bad=bytes; bad.push_back(0); run(bad,false);
    ipc::Result result; result.id=c.id; result.outcome=ipc::Outcome::Complete;
    ipc::Writer content(ipc::DataBytes); content.oid(spec.oid); content.raw(c.value.data(),c.value.size()); result.value=content.finish();
    std::vector<ipc::ResultReservation> expected={{c.id,&spec}};
    check(ipc::decodeResults(ipc::encodeResults({result}),expected).size()==1);
    result.id.generation=2; rejects([&]{ipc::decodeResults(ipc::encodeResults({result}),expected);});
    result.id=c.id; result.value.resize(ipc::responseBytes(spec)+1); rejects([&]{ipc::decodeResults(ipc::encodeResults({result}),expected);});
}
}
int main(int argc,char** argv)
{
    try {
        if(argc==3 && std::string(argv[1])=="--digest") {
            auto sha=ipc::digest(argv[2]); for(auto byte:sha)std::printf("%02x",byte); std::printf("\n"); return 0;
        }
        check(argc==1);
        headers(); accounting(); bodies(); bootstrap(); channel(); streaming();
        value(Value::integer(INT64_MIN),1); value(Value::integer(INT64_MAX),1);
        for(auto t:{ValueType::Unsigned32,ValueType::Counter32,ValueType::Gauge32,ValueType::TimeTicks}) value(Value::unsigned32(t,UINT32_MAX),1);
        value(Value::counter64(UINT64_MAX),1); value(Value::octets({0,1,0,255}),4);
        value(Value::objectId({1,3,6,1,4,1,53864}),7); value(Value::ipAddress({{127,0,0,1}}),1);
        value(Value::opaqueFloat(-1.25f),1); value(Value::opaqueDouble(1.25),1);
        for(auto t:{ValueType::NoSuchObject,ValueType::NoSuchInstance,ValueType::EndOfMibView}) value(Value::exception(t),1);
        std::vector<uint8_t> large(1048576,255); auto wire=ipc::encodeValue(Value::octets(std::move(large))); check(wire.size()==1048584 && wire.capacity()==wire.size());
        std::printf("IPC checks: %u\n",checks); return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"IPC failure after %u checks: %s\n",checks,e.what()); return 1; }
}
