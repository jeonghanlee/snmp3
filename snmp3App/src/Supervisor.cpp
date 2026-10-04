#include "Supervisor.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <set>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <system_error>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace snmp3 {
namespace {
constexpr uint64_t StopGrace=1000000,StopTerm=1500000,StopBound=2000000;
constexpr size_t EventSlots=8192;
void require(bool value) { if(!value)throw std::runtime_error("worker supervision rejected"); }
void systemCheck(int error) { if(error)throw std::system_error(error,std::generic_category(),"worker spawn failed"); }
struct Descriptor {
    int value;
    explicit Descriptor(int value=-1) : value(value) {}
    ~Descriptor() { if(value>=0)::close(value); }
    int release() { int result=value; value=-1; return result; }
};
struct Actions {
    posix_spawn_file_actions_t value;
    Actions() { systemCheck(posix_spawn_file_actions_init(&value)); }
    ~Actions() { posix_spawn_file_actions_destroy(&value); }
    void duplicate(int from,int to) { systemCheck(posix_spawn_file_actions_adddup2(&value,from,to)); }
    void null(int fd) { systemCheck(posix_spawn_file_actions_addopen(&value,fd,"/dev/null",fd==0 ? O_RDONLY:O_WRONLY,0)); }
    void closeFrom(int fd) { systemCheck(posix_spawn_file_actions_addclosefrom_np(&value,fd)); }
};
std::string absolute(const std::string& path)
{
    require(!path.empty() && path.front()=='/' && path.size()<=4096 && path.find('\0')==std::string::npos);
    char* resolved=realpath(path.c_str(),nullptr); require(resolved); std::string result(resolved); free(resolved); return result;
}
std::map<std::string,std::array<uint8_t,32>> libraries(const std::string& executable)
{
    int pipes[2]; if(pipe2(pipes,O_CLOEXEC|O_NONBLOCK)!=0)systemCheck(errno);
    Descriptor input(pipes[0]),output(pipes[1]); Actions actions;
    actions.null(0); actions.duplicate(output.value,1); actions.null(2); actions.closeFrom(3);
    std::string command="/usr/bin/ldd"; char* argv[]={&command[0],const_cast<char*>(executable.c_str()),nullptr};
    char path[]="PATH=/usr/bin:/bin",lang[]="LANG=C",locale[]="LC_ALL=C"; char* env[]={path,lang,locale,nullptr};
    pid_t pid=0; systemCheck(posix_spawn(&pid,command.c_str(),&actions.value,nullptr,argv,env));
    ::close(output.release()); std::string text; text.reserve(65536);
    bool eof=false,reaped=false; int status=0; const auto deadline=ipc::add(monotonicUs(),5000000);
    try {
        while(!eof || !reaped) {
            char bytes[4096]; const auto n=read(input.value,bytes,sizeof(bytes));
            if(n>0) { require(size_t(n)<=65536-text.size()); text.append(bytes,n); }
            else if(n==0)eof=true;
            else require(errno==EAGAIN || errno==EINTR);
            if(!reaped) { const auto result=waitpid(pid,&status,WNOHANG); if(result==pid)reaped=true; else if(result<0)require(errno==EINTR); }
            require(monotonicUs()<deadline);
            if(!eof || !reaped) { pollfd fd{input.value,POLLIN,0}; poll(&fd,1,10); }
        }
    } catch(...) {
        if(!reaped) { ::kill(pid,SIGKILL); while(waitpid(pid,&status,0)<0 && errno==EINTR) {} }
        throw;
    }
    require(WIFEXITED(status) && WEXITSTATUS(status)==0);
    std::map<std::string,std::array<uint8_t,32>> result; std::istringstream lines(text); std::string line;
    uint64_t owned=0;
    while(std::getline(lines,line)) {
        if(line.find("linux-vdso.so.")!=std::string::npos)continue;
        const auto begin=line.find('/'); require(begin!=std::string::npos); const auto end=line.find(" (",begin); require(end!=std::string::npos);
        const auto file=absolute(line.substr(begin,end-begin)); owned=ipc::add(owned,192+2*(file.size()+1)); require(owned<=131072);
        require(result.emplace(file,ipc::digest(file)).second);
    }
    require(!result.empty()); return result;
}
}
uint64_t monotonicUs()
{
    timespec value{}; require(clock_gettime(CLOCK_MONOTONIC,&value)==0 && value.tv_sec>=0);
    return ipc::add(ipc::multiply(uint64_t(value.tv_sec),1000000),uint64_t(value.tv_nsec)/1000);
}
Supervisor::Supervisor(std::shared_ptr<Scheduler> owner,const std::string& path,const Qualification& mode)
    : scheduler(std::move(owner)),executable(absolute(path)),qualification(mode)
{
    require(bool(scheduler)); scheduler->reserveConfiguration(16777216+65536);
    selected.capabilities=scheduler->configuration()->capabilities;
    selected.executable=ipc::digest(executable); selected.libraries=libraries(executable);
    std::set<std::string> dirs; for(const auto& lib:selected.libraries)dirs.insert(lib.first.substr(0,lib.first.find_last_of('/')));
    for(const auto& dir:dirs) { if(!libraryPath.empty())libraryPath+=':'; libraryPath+=dir; }
    require(libraryPath.size()<=4096);
    auto ids=scheduler->addresses(); uint64_t storage=16777216+65536;
    for(const auto id:ids) {
        require(ipc::bootstrapOwnedBytes(*scheduler->configuration(),scheduler->key(id))<=33554432);
        storage=ipc::add(storage,ipc::add(65536,ipc::bootstrapBytes(*scheduler->configuration(),scheduler->key(id))));
    }
    scheduler->reserveConfiguration(storage);
    addresses.reserve(ids.size()); published.resize(ids.size());
    for(const auto id:ids) { Address address; address.status.address=id; address.bootstrap=ipc::encodeBootstrap(*scheduler->configuration(),scheduler->key(id)); addresses.push_back(std::move(address)); }
    static_assert(2*EventSlots*sizeof(SupervisionEvent)<=1048576,"diagnostic storage exceeds inventory"); events.reserve(EventSlots);
    Descriptor sink,sinkCopy,parentCopy,childCopy;
    if(!qualification.stderrPath.empty()) {
        require(qualification.stderrPath.front()=='/');
        const auto parent=qualification.stderrPath.substr(0,qualification.stderrPath.find_last_of('/'));
        struct stat status{}; require(stat(parent.c_str(),&status)==0 && S_ISDIR(status.st_mode) && status.st_uid==geteuid() && (status.st_mode&0777)==0700);
        sink.value=open(qualification.stderrPath.c_str(),O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600); require(sink.value>=0);
        require(fstat(sink.value,&status)==0 && S_ISREG(status.st_mode) && status.st_uid==geteuid() && (status.st_mode&0777)==0600);
        sinkCopy.value=fcntl(sink.value,F_DUPFD_CLOEXEC,4); require(sinkCopy.value>=4);
    }
    require(!qualification.sanitizers || sinkCopy.value>=0);
    if(qualification.transportAddress) {
        require(sinkCopy.value>=0 && std::find(ids.begin(),ids.end(),qualification.transportAddress)!=ids.end());
        for(const auto descriptor:{qualification.transportParent,qualification.transportChild}) {
            int domain=0,type=0; socklen_t length=sizeof(int);
            require(descriptor>=0 && getsockopt(descriptor,SOL_SOCKET,SO_DOMAIN,&domain,&length)==0 && domain==AF_UNIX);
            length=sizeof(int); require(getsockopt(descriptor,SOL_SOCKET,SO_TYPE,&type,&length)==0 && type==SOCK_STREAM);
        }
        parentCopy.value=fcntl(qualification.transportParent,F_DUPFD_CLOEXEC,4); require(parentCopy.value>=4);
        childCopy.value=fcntl(qualification.transportChild,F_DUPFD_CLOEXEC,4); require(childCopy.value>=4);
    } else require(qualification.transportParent==-1 && qualification.transportChild==-1);
    qualificationFd=sinkCopy.release(); transportParent=parentCopy.release(); transportChild=childCopy.release();
}
Supervisor::~Supervisor()
{
    for(const auto fd:{qualificationFd,transportParent,transportChild})if(fd>=0)::close(fd);
}
void Supervisor::event(Address& a,uint64_t now,uint32_t code,int64_t detail)
{
    std::lock_guard<std::mutex> guard(reportMutex);
    if(events.size()>=EventSlots) { if(dropped!=UINT64_MAX)++dropped; return; }
    SupervisionEvent e; e.at=now; e.address=a.status.address; e.epoch=a.status.epoch; e.batch=a.status.batch; e.pid=a.status.pid; e.code=code; e.detail=detail; events.push_back(e);
}
void Supervisor::queue(Address& a,ipc::Kind kind,uint64_t batch,std::vector<uint8_t> bytes)
{
    ipc::Header h; h.kind=kind; h.activation=scheduler->activationId(); h.epoch=a.status.epoch; h.address=a.status.address;
    h.revision=scheduler->configurationRevision(); h.batch=batch; a.sent=ipc::add(a.sent,1); h.sequence=a.sent;
    a.channel->queue(h,std::move(bytes)); a.outgoing=kind;
}
void Supervisor::launch(Address& a,uint64_t now)
{
    require(!a.status.pid && !a.channel && !stopping);
    if(a.status.epoch==UINT64_MAX || a.status.spawnAttempts==UINT64_MAX) {
        scheduler->closeAdmission(a.status.address);
        throw std::runtime_error("worker identity exhausted");
    }
    a.status.epoch=ipc::add(a.status.epoch,1); a.status.spawnAttempts=ipc::add(a.status.spawnAttempts,1);
    a.attempts.push_back(now); event(a,now,1);
    Descriptor parent,child;
    if(qualification.transportAddress==a.status.address) {
        require(transportParent>=0 && transportChild>=0);
        parent.value=transportParent; child.value=transportChild; transportParent=transportChild=-1;
    } else {
        int sockets[2]; if(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,sockets)!=0)systemCheck(errno);
        parent.value=sockets[0]; child.value=sockets[1];
    }
    Descriptor safe(fcntl(child.value,F_DUPFD_CLOEXEC,4)); require(safe.value>=4);
    Actions actions; if(qualificationFd>=0)actions.duplicate(qualificationFd,2); else actions.null(2);
    actions.null(0); actions.null(1); actions.duplicate(safe.value,3); actions.closeFrom(4);
    std::string argument="--ipc-fd=3"; char* argv[]={&executable[0],&argument[0],nullptr};
    std::vector<std::string> environment={"PATH=/usr/bin:/bin","LANG=C","LC_ALL=C","LD_LIBRARY_PATH="+libraryPath};
    if(qualification.sanitizers) { environment.push_back("ASAN_OPTIONS=detect_leaks=0:abort_on_error=1"); environment.push_back("UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"); }
    uint64_t envBytes=0; std::vector<char*> env; for(auto& entry:environment) { envBytes=ipc::add(envBytes,entry.size()+1); env.push_back(&entry[0]); } env.push_back(nullptr); require(envBytes<=8192);
    pid_t pid=0; systemCheck(posix_spawn(&pid,executable.c_str(),&actions.value,nullptr,argv,env.data()));
    a.status.pid=pid; a.spawned=now; a.sent=a.received=a.origin=0; a.term=a.kill=a.closed=a.reading=a.keep=a.batchSent=false;
    a.channel.reset(new ipc::Channel(parent.release()));
    if(a.bootstrap.empty())a.bootstrap=ipc::encodeBootstrap(*scheduler->configuration(),scheduler->key(a.status.address));
    queue(a,ipc::Kind::Bootstrap,0,std::move(a.bootstrap)); event(a,now,2); event(a,now,17,a.channel->descriptor());
}
void Supervisor::contain(Address& a,uint64_t now,ipc::Outcome outcome)
{
    if(!a.origin) { a.origin=now; a.status.closing=true; a.status.ready=false; event(a,now,6,int64_t(outcome)); }
    scheduler->workerLost(a.status.address,outcome);
    if(a.channel && a.channel->writing() && a.outgoing!=ipc::Kind::Close) { a.channel.reset(); a.reading=false; }
}
bool Supervisor::reap(Address& a,uint64_t now)
{
    if(!a.status.pid)return true;
    int status=0; const auto pid=waitpid(a.status.pid,&status,WNOHANG);
    if(pid==0 || (pid<0 && errno==EINTR))return false;
    if(pid<0) { event(a,now,11,errno); return false; }
    event(a,now,7,status); a.channel.reset(); a.reservations.clear(); a.members.clear(); a.status.pid=0;
    a.status.ready=a.status.closing=a.reading=false; a.status.batch=0; ++a.status.reaps; scheduler->reaped(a.status.address);
    ++a.failures; const auto delay=uint64_t(250000)<<std::min<uint64_t>(a.failures-1,4);
    a.restartAt=ipc::add(now,delay); a.origin=0; return true;
}
void Supervisor::input(Address& a,uint64_t now)
{
    size_t quota=ipc::IoBytes; unsigned frames=0;
    while(a.channel && quota && frames<ipc::IoFrames) {
        if(!a.reading) {
            if(!a.channel->readHeader(quota,now,a.incoming))return;
            const auto& h=a.incoming; if(h.sequence>a.received)require(h.sequence==ipc::add(a.received,1));
            a.keep=h.sequence>a.received; if(a.keep)a.received=h.sequence;
            a.keep=a.keep && h.activation==scheduler->activationId() && h.epoch==a.status.epoch && h.address==a.status.address && h.revision==scheduler->configurationRevision();
            if(h.kind==ipc::Kind::Result || h.kind==ipc::Kind::Retired)a.keep=a.keep && h.batch==a.status.batch && h.batch;
            if(a.keep) {
                require(h.kind==ipc::Kind::Ready || h.kind==ipc::Kind::Result || h.kind==ipc::Kind::Retired || h.kind==ipc::Kind::Closed || h.kind==ipc::Kind::Fault);
                if(h.kind==ipc::Kind::Result)require(h.length<=a.resultMaximum);
                if(h.kind==ipc::Kind::Retired)require(h.length==8+24*a.members.size());
            }
            a.channel->acceptBody(a.keep); a.reading=true;
        }
        std::vector<uint8_t> bytes; if(!a.channel->readBody(quota,bytes))return;
        a.reading=false; ++frames; if(!a.keep)continue;
        const auto& h=a.incoming;
        if(h.kind==ipc::Kind::Ready) {
            require(!a.status.ready && !a.status.batch && !a.origin);
            const auto ready=ipc::decodeReady(bytes); require(ipc::sameCapabilities(ready.capabilities,selected.capabilities) && ready.executable==selected.executable && ready.libraries==selected.libraries);
            a.status.ready=true; event(a,now,3);
        } else if(h.kind==ipc::Kind::Result) {
            try {
                auto results=ipc::decodeResults(bytes,a.reservations);
                if(scheduler->complete(a.status.address,h.batch,std::move(results),monotonicUs()))event(a,now,4);
            } catch(const ipc::StaleIdentity&) { event(a,now,16); }
        } else if(h.kind==ipc::Kind::Retired) {
            const auto members=ipc::decodeRetired(bytes);
            if(scheduler->retired(a.status.address,h.batch,members)) { event(a,now,5); a.reservations.clear(); a.members.clear(); a.status.batch=0; a.failures=0; }
        } else if(h.kind==ipc::Kind::Closed) { require(a.origin); a.closed=true; event(a,now,10); }
        else { ipc::Reader fault(bytes); const auto category=fault.u32(),detail=fault.u32(); fault.end(); require(category>=1 && category<=5); event(a,now,12,detail); contain(a,now,ipc::Outcome::WorkerFailure); }
    }
}
void Supervisor::output(Address& a,uint64_t now)
{
    if(!a.channel)return;
    if(!a.channel->writing()) {
        if(a.origin && a.outgoing!=ipc::Kind::Close)queue(a,ipc::Kind::Close,0);
        else if(a.status.ready && !a.origin && !stopping && !a.status.batch) {
            auto dispatch=scheduler->dispatch(a.status.address,now);
            if(dispatch.batch) {
                a.status.batch=dispatch.batch; a.resultMaximum=8; a.reservations.clear(); a.members.clear(); a.batchSent=false;
                a.reservations.reserve(dispatch.commands.size()); a.members.reserve(dispatch.commands.size());
                for(const auto& command:dispatch.commands) {
                    require(command.definition>=1 && command.definition<=scheduler->configuration()->bindings.size());
                    auto definition=scheduler->configuration()->bindings.begin(); std::advance(definition,command.definition-1);
                    require(definition!=scheduler->configuration()->bindings.end());
                    a.reservations.push_back({command.id,&definition->second}); a.members.push_back(command.id);
                    a.resultMaximum=ipc::add(a.resultMaximum,ipc::add(48,ipc::responseBytes(definition->second)));
                }
                queue(a,ipc::Kind::Batch,dispatch.batch,ipc::encodeCommands(dispatch.commands)); event(a,now,13);
            }
        }
    }
    if(stopping && a.outgoing==ipc::Kind::Batch) { contain(a,now,ipc::Outcome::Stopping); return; }
    if(a.outgoing==ipc::Kind::Batch && !a.batchSent && scheduler->expire(a.status.address,monotonicUs())) { contain(a,now,ipc::Outcome::Deadline); return; }
    size_t quota=ipc::IoBytes;
    try { a.channel->write(quota); }
    catch(...) { if(a.outgoing==ipc::Kind::Batch && a.channel->bytesSent())scheduler->transmitted(a.status.address,a.status.batch); throw; }
    if(a.outgoing==ipc::Kind::Batch && !a.batchSent && a.channel->bytesSent()) { scheduler->transmitted(a.status.address,a.status.batch); a.batchSent=true; event(a,now,14); }
}
void Supervisor::requestStop() { scheduler->stop(); stopping=true; }
void Supervisor::step(uint64_t now)
{
    if(stopping && !stopOrigin)stopOrigin=now;
    for(auto& a:addresses) {
        if(scheduler->expire(a.status.address,now))contain(a,now,ipc::Outcome::Deadline);
        if(stopping)contain(a,stopOrigin,ipc::Outcome::Stopping);
    }
    bool spawned=false;
    for(size_t index=0;index<addresses.size();++index) {
        auto& a=addresses[(roundRobin+index)%addresses.size()];
        if(a.status.pid && !reap(a,now)) {
            if(a.origin) {
                if(now>=ipc::add(a.origin,StopGrace) && !a.term) { ::kill(a.status.pid,SIGTERM); a.term=true; ++a.status.forced; event(a,now,8); }
                if(now>=ipc::add(a.origin,StopTerm) && !a.kill) { ::kill(a.status.pid,SIGKILL); a.kill=true; event(a,now,9); }
            }
            try {
                if(!a.origin && !a.status.ready && now-a.spawned>=5000000)contain(a,now,ipc::Outcome::WorkerFailure);
                if(a.channel && a.channel->partial() && now-a.channel->partialOrigin()>=(a.status.ready ? 1000000u:5000000u))contain(a,now,ipc::Outcome::ChannelFailure);
                if(a.channel)input(a,now);
                if(a.channel)output(a,now);
            } catch(...) { contain(a,now,ipc::Outcome::ChannelFailure); a.channel.reset(); }
        }
        while(!a.attempts.empty() && now-a.attempts.front()>=60000000)a.attempts.pop_front();
        if(!a.status.pid && !stopping && !spawned && now>=a.restartAt && a.attempts.size()<4) {
            spawned=true;
            try { launch(a,now); }
            catch(...) { if(a.status.pid)contain(a,now,ipc::Outcome::WorkerFailure); else { ++a.failures; a.restartAt=ipc::add(now,uint64_t(250000)<<std::min<uint64_t>(a.failures-1,4)); event(a,now,15,errno); } }
        }
    }
    if(!addresses.empty())roundRobin=(roundRobin+1)%addresses.size();
    { std::lock_guard<std::mutex> guard(reportMutex); for(size_t i=0;i<addresses.size();++i)published[i]=addresses[i].status; }
}
bool Supervisor::finished(uint64_t now) const
{
    if(!stopOrigin)return false;
    return (scheduler->settled() && std::all_of(addresses.begin(),addresses.end(),[](const Address& a){return !a.status.pid;})) || now>=ipc::add(stopOrigin,StopBound);
}
bool Supervisor::reconcile()
{
    require(stopping); const auto now=monotonicUs(); bool all=true;
    for(auto& a:addresses)if(!reap(a,now))all=false;
    { std::lock_guard<std::mutex> guard(reportMutex); for(size_t i=0;i<addresses.size();++i)published[i]=addresses[i].status; }
    return all && scheduler->settled();
}
std::vector<WorkerSnapshot> Supervisor::snapshots() const { std::lock_guard<std::mutex> guard(reportMutex); return published; }
std::vector<SupervisionEvent> Supervisor::takeEvents()
{
    std::lock_guard<std::mutex> guard(reportMutex); auto result=std::move(events); events.clear(); events.reserve(EventSlots); return result;
}
uint64_t Supervisor::droppedEvents() const { std::lock_guard<std::mutex> guard(reportMutex); return dropped; }
uint64_t Supervisor::nextTimer() const
{
    uint64_t timer=stopOrigin ? ipc::add(stopOrigin,StopBound):UINT64_MAX;
    for(const auto& a:addresses) {
        timer=std::min(timer,scheduler->earliestDeadline(a.status.address));
        if(a.status.pid && a.origin && !a.kill)timer=std::min(timer,ipc::add(a.origin,!a.term ? StopGrace:StopTerm));
        else if(a.status.pid && !a.origin && !a.status.ready)timer=std::min(timer,ipc::add(a.spawned,5000000));
        else if(!a.status.pid && !stopping)timer=std::min(timer,a.attempts.size()<4 ? a.restartAt:ipc::add(a.attempts.front(),60000000));
    }
    return timer;
}
}
