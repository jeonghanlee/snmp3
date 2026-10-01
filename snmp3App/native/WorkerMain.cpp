#include "Worker.h"
#include "Capabilities.h"
#include "Ipc.h"
#include <cstdio>
#include <climits>
#include <poll.h>
#include <system_error>
#include <time.h>
#include <unistd.h>

namespace {
using namespace snmp3;
uint64_t nowUs()
{
    timespec ts{}; if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0 || ts.tv_sec<0)throw std::runtime_error("clock failed");
    return ipc::add(ipc::multiply(uint64_t(ts.tv_sec),1000000),uint64_t(ts.tv_nsec)/1000);
}
void require(bool value) { if(!value)throw std::runtime_error("worker frame rejected"); }
uint16_t nativeCode(NativeOutcome outcome)
{
    switch(outcome) {
    case NativeOutcome::Complete:return 1; case NativeOutcome::Timeout:return 2;
    case NativeOutcome::OpenFailure:return 3; case NativeOutcome::SendFailure:return 4;
    case NativeOutcome::SecurityFailure:return 5; case NativeOutcome::ProtocolFailure:return 6;
    case NativeOutcome::SecurityConflict:return 7; case NativeOutcome::CapabilityMismatch:return 8;
    case NativeOutcome::ResponseRejected:return 9; case NativeOutcome::Cancelled:return 10;
    case NativeOutcome::InvalidRequest:return 11;
    }
    throw std::runtime_error("unknown native outcome");
}
class AddressWorker {
public:
    explicit AddressWorker(int descriptor) : channel(descriptor) {}
    int run();
private:
    void input(uint64_t now);
    void batch();
    void completion(NativeResult result);
    void local(ipc::Outcome outcome,uint16_t native=0,int64_t status=0,int64_t index=0);
    void output();
    void closeNative();
    void events();
    void queue(ipc::Kind kind,uint64_t batch,std::vector<uint8_t> body={});
    ipc::Channel channel;
    ipc::Header identity, incoming;
    uint64_t received=0, sent=0, lastBatch=0, lastAdmission=0;
    uint64_t activeBatch=0, deadline=0;
    uint32_t faultCategory=2;
    bool initialized=false, reading=false, keep=false, streaming=false;
    bool closing=false, resultReady=false, resultSent=false, retiredSent=false, closedSent=false;
    std::shared_ptr<const Configuration> config;
    ipc::BatchReader::Definitions definitions;
    std::unique_ptr<ipc::BatchReader> decoder;
    std::unique_ptr<Worker> native;
    std::map<std::string,std::shared_ptr<Native>> sessions;
    std::vector<ipc::Command> commands;
    std::vector<uint8_t> resultFrame;
    std::vector<ipc::Identity> retired;
};
void AddressWorker::queue(ipc::Kind kind,uint64_t batch,std::vector<uint8_t> body)
{
    auto header=identity; header.kind=kind; header.batch=batch; sent=ipc::add(sent,1); header.sequence=sent;
    channel.queue(header,std::move(body));
}
void AddressWorker::events()
{
    // Production redirects stderr to /dev/null; qualification retains these owned numeric events.
    for(const auto& event:native->takeEvents())
        std::fprintf(stderr,"native_event %s %llu %llu %d %ld %lld %llu %llu %lld\n",event.event.c_str(),
                     (unsigned long long)event.session,(unsigned long long)event.token,
                     event.operation,event.detail,(long long)event.monotonicUs,
                     (unsigned long long)identity.address,(unsigned long long)identity.epoch,(long long)getpid());
    if(native->droppedEvents())std::fprintf(stderr,"native_dropped %llu\n",(unsigned long long)native->droppedEvents());
}
void AddressWorker::closeNative()
{
    for(auto& session:sessions)session.second->close();
    sessions.clear(); events();
}
void AddressWorker::local(ipc::Outcome outcome,uint16_t code,int64_t status,int64_t index)
{
    require(activeBatch && !resultReady);
    std::vector<ipc::Result> results; results.reserve(commands.size());
    for(auto& command:commands) {
        std::vector<uint8_t>().swap(command.value);
        ipc::Result result; result.id=command.id; result.outcome=outcome; result.nativeOutcome=code;
        result.errorStatus=status; result.errorIndex=index;
        results.push_back(std::move(result));
    }
    resultFrame=ipc::encodeResults(results); resultReady=true;
}
void AddressWorker::completion(NativeResult result)
{
    require(activeBatch && !resultReady);
    const auto code=nativeCode(result.outcome);
    if(result.outcome!=NativeOutcome::Complete || closing || nowUs()>=deadline) {
        local(closing ? ipc::Outcome::Stopping:nowUs()>=deadline ? ipc::Outcome::Deadline:ipc::Outcome::NativeFailure,code,result.errorStatus,result.errorIndex);
        return;
    }
    require(result.variables.size()==commands.size());
    std::vector<ipc::Result> results; results.reserve(commands.size());
    for(size_t i=0;i<commands.size();++i) {
        const auto& spec=definitions.at(commands[i].definition)->definition();
        ipc::Result value; value.id=commands[i].id; value.outcome=ipc::Outcome::Complete; value.nativeOutcome=code;
        value.errorStatus=result.errorStatus; value.errorIndex=result.errorIndex;
        auto bytes=ipc::encodeValue(result.variables[i].value);
        ipc::Writer encoded(ipc::size(ipc::responseBytes(spec)),ipc::size(ipc::add(4,ipc::add(ipc::multiply(result.variables[i].oid.size(),4),bytes.size()))));
        encoded.oid(result.variables[i].oid); encoded.raw(bytes.data(),bytes.size());
        value.value=encoded.finish(); results.push_back(std::move(value));
    }
    resultFrame=ipc::encodeResults(results); resultReady=true;
}
void AddressWorker::batch()
{
    require(activeBatch && !commands.empty()); deadline=commands.front().deadline;
    if(closing || nowUs()>=deadline) { local(closing ? ipc::Outcome::Stopping:ipc::Outcome::Deadline); return; }
    const auto binding=definitions.at(commands.front().definition);
    std::shared_ptr<Native> session;
    try {
        auto found=sessions.find(binding->endpoint().id);
        if(found==sessions.end()) {
            session=native->open(binding->profile(),binding->endpoint(),config->capabilities);
            sessions.emplace(binding->endpoint().id,session);
        } else session=found->second;
        events();
        // Open may block in discovery. Service actual queued Close before any submit.
        input(nowUs());
        if(closing || nowUs()>=deadline) { if(!resultReady)local(closing ? ipc::Outcome::Stopping:ipc::Outcome::Deadline); return; }
        std::vector<std::shared_ptr<const Binding>> bindings; bindings.reserve(commands.size());
        std::vector<Value> payload; if(binding->definition().operation==Operation::Set)payload.reserve(commands.size());
        for(auto& command:commands) {
            const auto current=definitions.at(command.definition); bindings.push_back(current);
            if(command.operation==Operation::Set) { ipc::Reader reader(command.value); payload.push_back(ipc::decodeValue(reader,current->definition(),true)); reader.end(); }
            std::vector<uint8_t>().swap(command.value);
        }
        session->submit(bindings,payload,[this](NativeResult result){completion(std::move(result));});
        events();
    } catch(const NativeError& error) {
        if(!resultReady)local(ipc::Outcome::NativeFailure,nativeCode(error.outcome()));
    }
}
void AddressWorker::input(uint64_t now)
{
    size_t quota=ipc::IoBytes; unsigned frames=0;
    while(quota && frames<ipc::IoFrames) {
        if(!reading) {
            if(!channel.readHeader(quota,now,incoming))return;
            if(incoming.sequence>received)require(incoming.sequence==ipc::add(received,1));
            keep=incoming.sequence>received;
            if(keep)received=incoming.sequence;
            if(initialized)keep=keep && incoming.activation==identity.activation && incoming.epoch==identity.epoch &&
                                      incoming.address==identity.address && incoming.revision==identity.revision;
            else require(keep && incoming.kind==ipc::Kind::Bootstrap);
            if(initialized && incoming.kind==ipc::Kind::Batch && incoming.batch<=lastBatch)keep=false;
            streaming=keep && incoming.kind==ipc::Kind::Batch;
            if(keep) {
                if(!initialized)require(incoming.kind==ipc::Kind::Bootstrap);
                else require(incoming.kind==ipc::Kind::Batch || incoming.kind==ipc::Kind::Close);
            }
            if(streaming) { require(!activeBatch && !closing && incoming.batch>lastBatch); decoder->begin(channel,incoming.length,lastAdmission); }
            else channel.acceptBody(keep);
            reading=true;
        }
        if(streaming) {
            try { if(!decoder->read(channel,quota,commands))return; }
            catch(const ipc::StaleIdentity&) { decoder->discard(); streaming=false; keep=false; continue; }
            if(commands.front().id.admission<=lastAdmission) { std::vector<ipc::Command>().swap(commands); reading=false; ++frames; continue; }
            lastAdmission=commands.back().id.admission; activeBatch=lastBatch=incoming.batch;
            retired.clear(); retired.reserve(commands.size()); for(const auto& command:commands)retired.push_back(command.id);
            reading=false; ++frames; batch();
        } else {
            std::vector<uint8_t> body; if(!channel.readBody(quota,body))return;
            reading=false; ++frames; if(!keep)continue;
            if(incoming.kind==ipc::Kind::Bootstrap) {
                faultCategory=1;
                identity=incoming; std::map<uint64_t,std::string> ids; config=ipc::decodeBootstrap(body,&ids);
                std::vector<uint8_t>().swap(body); require(!config->endpoints.empty());
                const auto key=ipc::addressKey(config->endpoints.begin()->second.address);
                for(const auto& e:config->endpoints)require(ipc::addressKey(e.second.address)==key);
                const auto capabilities=nativeCapabilities(); require(ipc::sameCapabilities(config->capabilities,capabilities));
                for(const auto& id:ids)definitions.emplace(id.first,std::make_shared<Binding>(config,config->bindings.at(id.second)));
                decoder.reset(new ipc::BatchReader(definitions)); faultCategory=3; native.reset(new Worker(true)); events();
                faultCategory=5;
                ipc::Ready ready; ready.capabilities=capabilities; ready.executable=ipc::digest("/proc/self/exe"); ready.libraries=ipc::loadedLibraries();
                queue(ipc::Kind::Ready,0,ipc::encodeReady(ready)); initialized=true; faultCategory=2;
            } else {
                closing=true; closeNative();
            }
        }
    }
}
void AddressWorker::output()
{
    if(!channel.writing()) {
        if(resultReady && !resultSent) { queue(ipc::Kind::Result,activeBatch,std::move(resultFrame)); resultSent=true; }
        else if(resultSent && !retiredSent) {
            std::vector<ipc::Command>().swap(commands);
            queue(ipc::Kind::Retired,activeBatch,ipc::encodeRetired(retired)); retiredSent=true;
        }
        if(closing && !activeBatch && !channel.writing() && !closedSent) { queue(ipc::Kind::Closed,0); closedSent=true; }
    }
    size_t quota=ipc::IoBytes; channel.write(quota);
    if(retiredSent && !channel.writing()) {
        std::vector<ipc::Identity>().swap(retired); activeBatch=deadline=0;
        resultReady=resultSent=retiredSent=false;
    }
}
int AddressWorker::run()
{
    const auto origin=nowUs();
    try {
        for(;;) {
            const auto now=nowUs();
            require(initialized || now-origin<5000000);
            if(channel.partial())require(now-channel.partialOrigin()<(initialized ? 1000000u:5000000u));
            input(now);
            if(native) {
                if(activeBatch && !resultReady && (closing || nowUs()>=deadline)) {
                    for(auto& session:sessions)session.second->close();
                    if(!resultReady)local(closing ? ipc::Outcome::Stopping:ipc::Outcome::Deadline);
                }
                native->service(0); events();
            }
            output();
            if(closedSent && !channel.writing())return 0;
            pollfd fd{channel.descriptor(),short(POLLIN|(channel.writing() ? POLLOUT:0)),0};
            poll(&fd,1,10);
        }
    } catch(...) {
        uint32_t detail=0;
        try { throw; }
        catch(const std::bad_alloc&) { faultCategory=4; }
        catch(const std::system_error& error) { if(error.code().value()>0)detail=uint32_t(error.code().value()); }
        catch(...) {}
        if(native) { closing=true; closeNative(); }
        if(identity.activation && !channel.writing()) {
            ipc::Writer fault(8,8); fault.u32(faultCategory); fault.u32(detail);
            try { queue(ipc::Kind::Fault,0,fault.finish()); size_t quota=ipc::IoBytes; channel.write(quota); } catch(...) {}
        }
        return 1;
    }
}
}
int main(int argc,char** argv)
{
    if(argc!=2 || std::string(argv[1])!="--ipc-fd=3")return 2;
    try { return AddressWorker(3).run(); } catch(...) { return 1; }
}
