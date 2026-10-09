#include "Request.h"
#include "Runtime.h"
#include "Config.h"
#include <alarm.h>
#include <dbAccess.h>
#include <dbChannel.h>
#include <dbEvent.h>
#include <db_field_log.h>
#include <dbBase.h>
#include <dbCommon.h>
#include <dbLock.h>
#include <dbLink.h>
#include <devSup.h>
#include <initHooks.h>
#include <dbUnitTest.h>
#include <epicsExit.h>
#include <epicsUnitTest.h>
#include <iocsh.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <calcRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <int64inRecord.h>
#include <int64outRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <lsiRecord.h>
#include <lsoRecord.h>
#include <waveformRecord.h>
#include <menuFtype.h>
#include <menuIvoa.h>
#include <menuOmsl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cfenv>
#include <limits>
#include <atomic>
#include <thread>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <signal.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace snmp3;
extern "C" int snmp3RecordTest_registerRecordDeviceDriver(dbBase*);
namespace {
unsigned checks=0;
bool edges=false;
bool alarms=false;
bool active=false;
bool policy=false;
bool numeric=false;
bool activeUnforced=false;
bool deadlineQueue=false;
bool nearDeadline=false;
bool accounting=false;
bool stopQueued=false;
bool rebuild=false;
bool stopInflight=false;
bool stopEnqueueFailed=false;
bool stopDownstream=false;
bool liveDetach=false;
bool repeatDetach=false;
bool liveDtype=false;
bool supportTransition=false;
bool scanProc=false;
bool alarmFlow=false;
bool chainFlow=false;
bool queuedSimm=false;
bool boundaries=false;
bool retirementFlow=false;
bool contract=false;
bool staleRecord=false;
bool finalClauses=false;
const unsigned BoundaryRows[][6]={{15,16,16,6,7,3},{16,16,15,7,6,4},{39,41,40,8,8,5},
    {40,40,39,6,8,4},{255,256,256,8,6,5},{256,257,255,7,7,4},{32767,32767,32767,8,8,4}};
const size_t BoundaryLengths[]={0,1,14,15,16,17,38,39,40,41,254,255,256,257,32765,32766,32767};
uint64_t boundaryHandles[3]={};
unsigned boundaryStimuli=0,boundaryGets=0,boundarySets=0,boundaryRejected=0;
int64_t firstActivationWorker=0;
struct RecordLock {
    dbCommon* value;
    explicit RecordLock(dbCommon* record) : value(record) { if(value)dbScanLock(value); }
    ~RecordLock() { if(value)dbScanUnlock(value); }
    void unlock() { dbScanUnlock(value); value=nullptr; }
};
void check(bool valid,const char* message)
{ ++checks; if(!valid)throw std::runtime_error(message); }
dbCommon* record(const char* name)
{
    DBADDR address{};
    check(dbNameToAddr(name,&address)==0,"record lookup failed");
    return address.precord;
}
template<typename Record> Record& typed(const char* name)
{ return *reinterpret_cast<Record*>(record(name)); }
std::string quoted(const char* value)
{
    const std::string text=value;
    check(text.find_first_of("\"\\\n\r")==std::string::npos,"test argument quoting rejected");
    return "\""+text+"\"";
}
void command(const std::string& value)
{ check(iocshCmd(value.c_str())==0,"IOC command failed"); }
void process(const char* name,bool successful=true,bool cascade=false,uint16_t expectedNative=0,bool expired=false,bool observeTerminal=false)
{
    auto* rec=record(name);
    RecordLock first(rec);
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    check(context!=nullptr,"record has no SNMP binding");
    const auto count=Requests::instance().snapshot().completions;
    dbProcess(rec);
    if(policy && context->definition.kind==RecordKind::Ao) {
        const auto& value=*reinterpret_cast<aoRecord*>(rec);
        std::printf("{\"event\":\"policy_first_pass\",\"record\":\"%s\",\"pact\":%u,\"val\":%.17g,\"oval\":%.17g,\"ivoa\":%u,\"hihi\":%.17g,\"hhsv\":%u,\"nsev\":%u}\n",
                    name,value.pact,value.val,value.oval,value.ivoa,value.hihi,value.hhsv,value.nsev);
    }
    const bool admitted=rec->pact;
    if(expectedNative || expired || observeTerminal) {
        const auto terminalEnd=ipc::add(monotonicUs(),8000000);
        while(!Requests::instance().snapshot().entered && monotonicUs()<terminalEnd)epicsThreadSleep(0.001);
        check(Requests::instance().snapshot().entered==1,"native terminal callback did not acquire record lease");
        check(context->terminal.result && (observeTerminal || (
              context->terminal.result->outcome==(expired?ipc::Outcome::Deadline:ipc::Outcome::NativeFailure) &&
              (expired || context->terminal.result->nativeOutcome==expectedNative))),"unexpected native terminal classification");
        const auto& terminal=*context->terminal.result;
        std::printf("{\"event\":\"native_terminal\",\"record\":\"%s\",\"outcome\":%u,\"native_code\":%u,\"error_status\":%lld,\"error_index\":%lld}\n",
                    name,unsigned(terminal.outcome),terminal.nativeOutcome,
                    (long long)terminal.errorStatus,(long long)terminal.errorIndex);
    }
    first.unlock();
    const auto end=ipc::add(monotonicUs(),8000000);
    bool complete=false;
    do {
        dbScanLock(rec);
        complete=!rec->pact;
        dbScanUnlock(rec);
        if(complete)break;
        epicsThreadSleep(0.001);
    } while(monotonicUs()<end);
    check(complete,"record completion timeout");
    RecordLock last(rec);
    check((rec->sevr==NO_ALARM)==successful,(std::string("record alarm outcome mismatch: ")+name).c_str());
    if(admitted) {
        const auto completed=Requests::instance().snapshot().completions;
        check(completed==count+1 || (cascade && completed==count+2),"completion was not finalized once");
        check(!context->active && !context->terminal.result,"terminal remained borrowed after completion");
    }
    std::printf("{\"event\":\"record_completed\",\"record\":\"%s\",\"at\":%llu,\"handle\":%llu,\"generation\":%llu,\"admission\":%llu,\"activation\":%llu,\"revision\":%llu,\"alarm\":%u,\"severity\":%u,\"udf\":%u,\"published\":%s}\n",
                name,(unsigned long long)monotonicUs(),(unsigned long long)context->handle,
                (unsigned long long)context->identity.generation,(unsigned long long)context->identity.admission,
                (unsigned long long)context->owner->activationId(),(unsigned long long)context->owner->configurationRevision(),
                rec->stat,rec->sevr,rec->udf,context->published?"true":"false");
    last.unlock();
    auto owner=context->owner;
    while(!owner->settled() && monotonicUs()<end)epicsThreadSleep(0.001);
    check(owner->settled(),(std::string("native retirement did not settle: ")+name).c_str());
}
void put(const char* name,short type,const void* data,long count=1)
{
    DBADDR address{};
    check(dbNameToAddr(name,&address)==0,"field lookup failed");
    check(dbPutField(&address,type,data,count)==0,"field put failed");
}
void outputs()
{
    // First-pass capture uses Base-prepared values; explicit processing drives each actual SET.
    auto& ao=typed<aoRecord>("Records_Ao");
    dbScanLock(reinterpret_cast<dbCommon*>(&ao)); ao.val=1.0+std::ldexp(1.0,-24); ao.udf=FALSE; dbScanUnlock(reinterpret_cast<dbCommon*>(&ao));
    process("Records_Ao");
    auto* lo=record("Records_Longout");
    DBADDR addr{}; check(dbNameToAddr("Records_Longout.VAL",&addr)==0,"longout address absent");
    dbScanLock(lo); *static_cast<epicsInt32*>(addr.pfield)=17; lo->udf=FALSE; dbScanUnlock(lo);
    process("Records_Longout");
    auto& wide=typed<int64outRecord>("Records_Int64out");
    dbScanLock(reinterpret_cast<dbCommon*>(&wide)); wide.val=INT64_C(9007199254740993); wide.udf=FALSE; dbScanUnlock(reinterpret_cast<dbCommon*>(&wide));
    process("Records_Int64out");
    auto* shortrec=record("Records_Stringout");
    check(dbNameToAddr("Records_Stringout.VAL",&addr)==0,"stringout address absent");
    dbScanLock(shortrec); std::strcpy(static_cast<char*>(addr.pfield),"short"); shortrec->udf=FALSE; dbScanUnlock(shortrec);
    process("Records_Stringout");
    auto& longrec=typed<lsoRecord>("Records_Lso");
    dbScanLock(reinterpret_cast<dbCommon*>(&longrec));
    std::memset(longrec.val,'X',200); longrec.val[200]=0; longrec.len=201; longrec.udf=FALSE;
    dbScanUnlock(reinterpret_cast<dbCommon*>(&longrec));
    process("Records_Lso");
}
void baseline()
{
    std::printf("{\"event\":\"record_contexts\",\"count\":%zu}\n",Requests::instance().snapshot().contexts);
    check(Requests::instance().snapshot().contexts==(edges?33u:alarms?24u:(active || activeUnforced || accounting)?19u:policy?24u:numeric?160u:12u),"eleven DSET kinds and aliases were not initialized");
    check(!typed<waveformRecord>("Records_Waveform").busy,"initial waveform BUSY was not normalized");
    process("Records_Ai"); check(typed<aiRecord>("Records_Ai").val==-123,"ai input mismatch");
    process("Records_Longin"); check(typed<longinRecord>("Records_Longin").val==-123,"longin input mismatch");
    double completions=0; DBADDR addr{}; long count=1;
    check(dbNameToAddr("Records_Completed.VAL",&addr)==0 &&
          dbGetField(&addr,DBR_DOUBLE,&completions,nullptr,&count,nullptr)==0 && completions==1,
          "Base FLNK did not execute once");
    process("Records_Int64in"); check(typed<int64inRecord>("Records_Int64in").val==UINT32_MAX,"int64in Counter32 lost precision");
    process("Records_Stringin");
    check(dbNameToAddr("Records_Stringin.VAL",&addr)==0 &&
          std::strcmp(static_cast<const char*>(addr.pfield),"1.3.6.1.4.1.53864")==0,"OID text mismatch");
    process("Records_Lsi",false);
    check(typed<lsiRecord>("Records_Lsi").udf,"initial failed lsi read cleared UDF");
    process("Records_Waveform");
    auto& wf=typed<waveformRecord>("Records_Waveform");
    check(*static_cast<epicsUInt64*>(wf.bptr)==UINT64_MAX && wf.nord==1 && !wf.busy,"Counter64 waveform lost precision");
    outputs();
    process("Records_FloatRead"); check(typed<aiRecord>("Records_FloatRead").val==1.0,"ao binary32 wire tie did not select even neighbor");
    process("Records_Ai"); check(typed<aiRecord>("Records_Ai").val==17,"integer SET was not observable through GET");
    process("Records_Waveform");
    check(*static_cast<epicsUInt64*>(wf.bptr)==UINT64_C(9007199254740993),"int64out SET used a double intermediate");
    process("Records_Lsi");
    auto& lsi=typed<lsiRecord>("Records_Lsi");
    check(lsi.len==201 && std::strlen(lsi.val)==200 && !lsi.udf,"long string SET/GET length mismatch");
    check(std::string(lsi.val)==std::string(200,'X'),"long string SET/GET payload mismatch");
    auto* rec=record("Records_Ai");
    const epicsUInt16 dtype=rec->dtyp, changed=0;
    put("Records_Ai.DTYP",DBR_USHORT,&changed);
    process("Records_Ai",false);
    check(rec->stat==LINK_ALARM,"direct DTYP write did not surface frozen identity failure");
    put("Records_Ai.DTYP",DBR_USHORT,&dtype);
    process("Records_Ai");
    const auto handle=static_cast<RecordContext*>(rec->dpvt)->handle;
    const char replacement[]="@binding=Counter32Read deadline_ms=5000";
    check(dbNameToAddr("Records_Ai.INP",&addr)==0,"input field absent");
    check(dbPutField(&addr,DBR_STRING,replacement,1)!=0,"live binding replacement was accepted");
    check(static_cast<RecordContext*>(rec->dpvt)->handle==handle,"refused link replacement detached context");
    process("Records_Ai");
}
struct QueueBlocker {
    epicsEvent entered,release,done;
    bool queued=false;
    epicsCallback callback{};
    std::atomic<unsigned> noops{0};
    static void wait(epicsCallback* callback)
    {
        auto& self=*static_cast<QueueBlocker*>(callback->user);
        self.entered.trigger(); self.release.wait(); self.done.trigger();
    }
    static void count(epicsCallback* callback)
    { ++static_cast<QueueBlocker*>(callback->user)->noops; }
    QueueBlocker()
    {
        callbackSetCallback(&QueueBlocker::wait,&callback);
        callbackSetUser(this,&callback);
        callbackSetPriority(priorityLow,&callback);
    }
    ~QueueBlocker() { if(queued) { release.trigger(); done.wait(); } }
};
template<typename Predicate> bool until(Predicate predicate,uint64_t budget=8000000)
{
    const auto end=ipc::add(monotonicUs(),budget);
    do { if(predicate())return true; epicsThreadSleep(0.001); } while(monotonicUs()<end);
    return predicate();
}
void inputEdges()
{
    for(const char* name:{"Records_UnknownBinding","Records_BadGrammar","Records_BadOopt","Records_BadFtvl"})
        check(!record(name)->dpvt,"invalid startup record acquired a usable context");
    check(typed<lsiRecord>("Records_SmallLsi").sizv==16 && typed<lsiRecord>("Records_LargeLsi").sizv==32767,
          "Base lsi clamping was not frozen at effective size");
    check(typed<lsoRecord>("Records_SmallLso").sizv==16 && typed<lsoRecord>("Records_LargeLso").sizv==32767,
          "Base lso clamping was not frozen at effective size");
    for(const char* name:{"Records_WideDouble","Records_UnsignedLong","Records_WideSigned"}) {
        auto* rec=record(name);
        { RecordLock lock(rec); rec->udf=TRUE; }
        process(name,false);
    }
    check(typed<aiRecord>("Records_WideDouble").val==42 && typed<aiRecord>("Records_WideDouble").udf,
          "lossy Counter64 ai input replaced prior value or cleared UDF");
    check(typed<longinRecord>("Records_UnsignedLong").val==42 && typed<longinRecord>("Records_UnsignedLong").udf,
          "overflowing longin input replaced prior value or cleared UDF");
    check(typed<int64inRecord>("Records_WideSigned").val==42 && typed<int64inRecord>("Records_WideSigned").udf,
          "overflowing int64in input replaced prior value or cleared UDF");
    for(const char* name:{"Records_NarrowFloat","Records_NarrowDouble","Records_ShortOctets","Records_ShortOid"}) {
        auto& wf=typed<waveformRecord>(name);
        const size_t bytes=wf.nelm*(wf.ftvl==menuFtypeUCHAR?1:4);
        { RecordLock lock(reinterpret_cast<dbCommon*>(&wf)); std::memset(wf.bptr,0x5a,bytes); wf.nord=1; }
        process(name,false);
        const auto* data=static_cast<unsigned char*>(wf.bptr);
        check(wf.nord==1 && !wf.udf && !wf.busy &&
              std::all_of(data,data+bytes,[](unsigned char byte){return byte==0x5a;}),
              "failed waveform input changed BPTR/NORD or contradicted Base UDF semantics");
        check(!static_cast<RecordContext*>(wf.dpvt)->nativeSuccess,"failed waveform input counted as native success");
    }
    process("Records_Octets");
    auto& octets=typed<waveformRecord>("Records_Octets");
    const unsigned char binary[]={65,0,66,255,0};
    check(octets.nord==5 && std::memcmp(octets.bptr,binary,sizeof(binary))==0,"binary waveform input lost bytes");
    process("Records_Oid");
    auto& oid=typed<waveformRecord>("Records_Oid");
    const epicsUInt32 arcs[]={1,3,6,1,4,1,53864};
    check(oid.nord==7 && std::memcmp(oid.bptr,arcs,sizeof(arcs))==0,"OID waveform input lost arcs");
    process("Records_Ip");
    auto& ip=typed<waveformRecord>("Records_Ip");
    const unsigned char address[]={127,0,0,1};
    check(ip.nord==4 && std::memcmp(ip.bptr,address,sizeof(address))==0,"IPv4 waveform input lost bytes");
    process("Records_Text",false);
    check(std::strcmp(typed<stringinRecord>("Records_Text").val,"retained")==0,
          "embedded-NUL string input published a prefix");
    std::printf("{\"event\":\"input_edges\",\"precision_rejected\":true,\"overflow_rejected\":true,\"binary_exact\":true,\"base_clamps\":true}\n");
}
void capacityAndOrder()
{
    process("Records_Text",false);
    check(std::strcmp(typed<stringinRecord>("Records_Text").val,"retained")==0,
          "over-capacity stringin published a prefix");
    auto& small=typed<lsiRecord>("Records_SmallLsi");
    { RecordLock lock(reinterpret_cast<dbCommon*>(&small)); std::strcpy(small.val,"retained"); small.len=9; }
    process("Records_SmallLsi",false);
    check(small.len==9 && std::strcmp(small.val,"retained")==0 && small.udf,
          "over-capacity lsi replaced VAL/LEN/UDF");
    process("Records_OrderA",true,true);
    check(until([]{ return !typed<lsiRecord>("Records_OrderB").pact &&
                           Requests::instance().snapshot().active==0; }),"FLNK target did not complete");
    DBADDR addr{}; double phase=0,completed=0; long count=1;
    check(dbNameToAddr("Records_OrderPhase.VAL",&addr)==0 &&
          dbGetField(&addr,DBR_DOUBLE,&phase,nullptr,&count,nullptr)==0 && phase==1,
          "Base FLNK did not execute before source PACT cleared");
    count=1;
    check(dbNameToAddr("Records_OrderCount.VAL",&addr)==0 &&
          dbGetField(&addr,DBR_DOUBLE,&completed,nullptr,&count,nullptr)==0 && completed==1,
          "FLNK target completed more than once");
    auto& next=typed<lsiRecord>("Records_OrderB");
    check(next.len==201 && std::string(next.val)==std::string(200,'X'),"FLNK target saw incomplete long string");
    std::printf("{\"event\":\"base_order\",\"flnk_source_pact\":1,\"target_completions\":1,\"complete_length\":201}\n");
}
void simulation()
{
    const epicsUInt16 yes=1,no=0;
    for(const char* name:{"Records_Ai","Records_Longin","Records_Int64in","Records_Text","Records_SmallLsi","Records_Waveform"}) {
        const std::string root=name;
        const bool text=root=="Records_Text" || root=="Records_SmallLsi";
        const char* source=text?"Records_SimString.VAL NPP":"Records_SimNumber.VAL NPP";
        put((root+".SIOL").c_str(),DBR_STRING,source);
        put((root+".SIMM").c_str(),DBR_USHORT,&yes);
        auto* rec=record(name);
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        const auto count=Requests::instance().snapshot().completions;
        const auto identity=context->identity;
        { RecordLock lock(rec); dbProcess(rec); check(!rec->pact,"synchronous simulation left PACT set"); }
        check(context->identity==identity && Requests::instance().snapshot().completions==count,
              "simulation before admission sent native work");
        put((root+".SIMM").c_str(),DBR_USHORT,&no);
        QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
        check(blocker.queued && blocker.entered.wait(3.0),"simulation queue blocker did not enter");
        { RecordLock lock(rec); dbProcess(rec); check(rec->pact,"simulation switch request did not admit"); }
        check(until([]{return Requests::instance().snapshot().queued==1;}),"native completion did not enter actual Base queue");
        put((root+".SIMM").c_str(),DBR_USHORT,&yes);
        if(root=="Records_Waveform") { RecordLock lock(rec); typed<waveformRecord>(name).busy=TRUE; }
        blocker.release.trigger();
        check(until([&]{RecordLock lock(rec); return !rec->pact;}),"simulation bypass lost completion");
        check(Requests::instance().snapshot().completions==count+1 && !context->active && !context->published,
              "simulation bypass published native staging or lost terminal release");
        if(root=="Records_Ai")check(typed<aiRecord>(name).val==85,"ai bypass chose native input");
        else if(root=="Records_Longin")check(typed<longinRecord>(name).val==85,"longin bypass chose native input");
        else if(root=="Records_Int64in")check(typed<int64inRecord>(name).val==85,"int64in bypass chose native input");
        else if(root=="Records_Text")check(std::strcmp(typed<stringinRecord>(name).val,"simulated")==0 &&
                                           rec->stat==READ_ALARM && !context->nativeSuccess,
                                           "string simulation hid failure or counted native success");
        else if(root=="Records_SmallLsi")check(std::strcmp(typed<lsiRecord>(name).val,"simulated")==0 &&
                                               rec->stat==READ_ALARM && !context->nativeSuccess,
                                               "lsi simulation hid failure or counted native success");
        else check(*static_cast<epicsUInt64*>(typed<waveformRecord>(name).bptr)==85 &&
                   !typed<waveformRecord>(name).busy,"waveform bypass retained BUSY or chose native input");
        const double delay=0.01;
        put((root+".SDLY").c_str(),DBR_DOUBLE,&delay);
        const auto delayedIdentity=context->identity;
        const auto delayedCount=Requests::instance().snapshot().completions;
        { RecordLock lock(rec); dbProcess(rec); check(rec->pact,"Base SDLY did not start asynchronous simulation"); }
        check(until([&]{RecordLock lock(rec); return !rec->pact;}),"Base delayed simulation did not complete");
        check(context->identity==delayedIdentity && Requests::instance().snapshot().completions==delayedCount,
              "Base SDLY callback admitted or consumed a module generation");
        const double synchronous=-1;
        put((root+".SDLY").c_str(),DBR_DOUBLE,&synchronous);
        put((root+".SIMM").c_str(),DBR_USHORT,&no);
        // Normal mode resumes native publication; text fixtures retain their native rejection and SIOL value.
        process(name,!text);
        if(root=="Records_Ai")check(typed<aiRecord>(name).val==17,"ai normal mode did not publish native input");
        else if(root=="Records_Longin")check(typed<longinRecord>(name).val==17,"longin normal mode did not publish native input");
        else if(root=="Records_Int64in")check(typed<int64inRecord>(name).val==UINT32_MAX,"int64in normal mode did not publish native input");
        else if(root=="Records_Text")check(std::strcmp(typed<stringinRecord>(name).val,"simulated")==0 &&
                                           rec->stat==READ_ALARM,"stringin normal-mode rejection replaced SIOL value");
        else if(root=="Records_SmallLsi")check(std::strcmp(typed<lsiRecord>(name).val,"simulated")==0 &&
                                               rec->stat==READ_ALARM,"lsi normal-mode rejection replaced SIOL value");
        else check(*static_cast<epicsUInt64*>(typed<waveformRecord>(name).bptr)==UINT64_C(9007199254740993) &&
                   typed<waveformRecord>(name).nord==1,"waveform normal mode did not publish native input");
        check(context->published==!text && context->nativeSuccess==!text,"normal mode native publication state mismatch");
        std::printf("{\"event\":\"simulation_bypass\",\"record\":\"%s\",\"published\":false,\"terminal_released\":true,\"normal_native_published\":%s}\n",
                    name,text?"false":"true");
    }
    const epicsUInt16 raw=2;
    put("Records_Ai.SIMM",DBR_USHORT,&raw);
    process("Records_Ai");
    check(typed<aiRecord>("Records_Ai").val==85 && typed<aiRecord>("Records_Ai").rval==85,
          "Base RAW simulation did not select its raw input path");
    put("Records_Ai.SIMM",DBR_USHORT,&no);
}
void contractTrial()
{
    const char* kinds[]={"Ai","Longin","Int64in","Stringin","Lsi","Waveform","Ao","Longout","Int64out","Stringout","Lso"};
    unsigned rejected=0;
    const auto reject=[&](const std::string& suffix) {
        const std::string name="Records_"+suffix;
        check(!record(name.c_str())->dpvt,"invalid contract record acquired a context"); ++rejected;
        std::printf("{\"event\":\"contract_rejected\",\"record\":\"%s\"}\n",name.c_str());
    };
    for(unsigned i=0;i<13;++i)reject("RejectGrammar"+std::to_string(i));
    for(const char* kind:kinds) { reject(std::string("RejectOperation")+kind); reject(std::string("RejectType")+kind); }
    for(unsigned i=0;i<9;++i)reject("RejectFtvl"+std::to_string(i));
    for(const char* name:{"RejectLinrai","RejectLinrao","RejectScan"})reject(name);
    check(rejected==47 && Requests::instance().snapshot().contexts==17 &&
          Requests::instance().snapshot().completions==0,"contract startup inventory mismatch");
    std::vector<uint64_t> handles;
    for(unsigned i=0;i<11;++i) {
        auto* context=static_cast<RecordContext*>(record((std::string("Records_")+kinds[i]).c_str())->dpvt);
        check(context && unsigned(context->definition.kind)==i && context->handle && !context->active &&
              !context->identity.generation,"contract DSET initialization mismatch");
        handles.push_back(context->handle);
    }
    std::sort(handles.begin(),handles.end());
    check(std::adjacent_find(handles.begin(),handles.end())==handles.end(),"duplicate definition did not get independent handles");
    for(const char* name:{"Records_DefaultLsi","Records_DefaultLso"}) {
        auto* rec=record(name); auto* context=static_cast<RecordContext*>(rec->dpvt);
        const unsigned sizv=name==std::string("Records_DefaultLsi")?reinterpret_cast<lsiRecord*>(rec)->sizv:reinterpret_cast<lsoRecord*>(rec)->sizv;
        check(context && sizv==41 && context->definition.storageCapacity==41 && context->storage,"default SIZV not frozen");
        DBADDR addr{}; const epicsUInt16 requested=42;
        check(dbNameToAddr((std::string(name)+".SIZV").c_str(),&addr)==0 && dbPutField(&addr,DBR_USHORT,&requested,1)!=0,
              "Base allowed live SIZV replacement");
        check(context->definition.storageCapacity==41,"refused SIZV write changed frozen capacity");
    }
    check(static_cast<RecordContext*>(record("Records_BudgetMin")->dpvt)->definition.budgetMs==1 &&
          static_cast<RecordContext*>(record("Records_BudgetMax")->dpvt)->definition.budgetMs==600000,"valid budget boundaries rejected");
    check(!typed<waveformRecord>("Records_DefaultBusy").busy && !typed<waveformRecord>("Records_Waveform").busy,
          "default/nonzero initial waveform BUSY mismatch");
    std::printf("{\"event\":\"contract_init\",\"at\":%llu,\"rejected\":%u,\"contexts\":17,\"distinct_handles\":11,\"default_sizv\":41}\n",
                (unsigned long long)monotonicUs(),rejected);
    auto& output=typed<lsoRecord>("Records_DefaultLso");
    { RecordLock lock(reinterpret_cast<dbCommon*>(&output)); std::strcpy(output.val,"default"); output.len=8; output.udf=FALSE; }
    process("Records_DefaultLso"); process("Records_DefaultLsi"); process("Records_DefaultBusy");
    check(typed<lsiRecord>("Records_DefaultLsi").len==18 &&
          std::string(typed<lsiRecord>("Records_DefaultLsi").val)=="1.3.6.1.4.1.53864","default lsi native GET mismatch");
    outputs();
    const epicsUInt16 yes=1,no=0;
    for(unsigned i=0;i<6;++i) {
        const std::string name=std::string("Records_")+kinds[i];
        auto* rec=record(name.c_str()); auto* context=static_cast<RecordContext*>(rec->dpvt);
        const bool text=i==3 || i==4;
        put((name+".SIOL").c_str(),DBR_STRING,text?"Records_ContractString.VAL NPP":"Records_ContractNumber.VAL NPP");
        const auto before=Requests::instance().snapshot().completions,identity=context->identity.generation;
        const auto links=typed<calcRecord>("Records_Completed").val;
        check(!context->nativeSuccess,"simulation trial has prior native-success history");
        put((name+".SIMM").c_str(),DBR_USHORT,&yes);
        { RecordLock lock(rec); dbProcess(rec); check(!rec->pact,"contract synchronous simulation pending"); }
        check(context->identity.generation==identity && Requests::instance().snapshot().completions==before &&
              !context->nativeSuccess,"pre-admission simulation created native work");
        put((name+".SIMM").c_str(),DBR_USHORT,&no);
        QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
        check(blocker.queued && blocker.entered.wait(3.0),"contract callback blocker absent");
        { RecordLock lock(rec); dbProcess(rec); check(rec->pact,"contract GET not admitted"); }
        check(until([]{return Requests::instance().snapshot().queued==1;}),"contract successful terminal not queued");
        { RecordLock lock(rec); check(context->terminal.result && context->terminal.result->outcome==ipc::Outcome::Complete &&
              context->terminal.result->nativeOutcome==1 && context->terminal.id==context->identity,"contract terminal was not native success"); }
        put((name+".SIMM").c_str(),DBR_USHORT,&yes);
        if(i==5) { RecordLock lock(rec); reinterpret_cast<waveformRecord*>(rec)->busy=TRUE; }
        blocker.release.trigger();
        check(until([&]{RecordLock lock(rec); return !rec->pact && !context->active;}),"contract simulation completion missing");
        check(until([&]{return context->owner->settled();}),"contract simulated terminal retained");
        { RecordLock lock(rec);
          check(!context->published && !context->nativeSuccess && !context->terminal.result && !rec->udf &&
                rec->sevr==NO_ALARM && context->identity.generation==identity+1 &&
                Requests::instance().snapshot().completions==before+1,"simulation counted native publication or lost ownership");
          if(i==0)check(reinterpret_cast<aiRecord*>(rec)->val==85,"contract ai SIOL mismatch");
          else if(i==1)check(reinterpret_cast<longinRecord*>(rec)->val==85,"contract longin SIOL mismatch");
          else if(i==2)check(reinterpret_cast<int64inRecord*>(rec)->val==85,"contract int64in SIOL mismatch");
          else if(i==3)check(std::string(reinterpret_cast<stringinRecord*>(rec)->val)=="simulated","contract stringin SIOL mismatch");
          else if(i==4)check(std::string(reinterpret_cast<lsiRecord*>(rec)->val)=="simulated" &&
                            reinterpret_cast<lsiRecord*>(rec)->len==10,"contract lsi SIOL/LEN mismatch");
          else check(*static_cast<epicsUInt64*>(reinterpret_cast<waveformRecord*>(rec)->bptr)==85 &&
                     reinterpret_cast<waveformRecord*>(rec)->nord==1 && !reinterpret_cast<waveformRecord*>(rec)->busy,"contract waveform SIOL mismatch"); }
        put((name+".SIMM").c_str(),DBR_USHORT,&no); process(name.c_str());
        check(context->published && context->nativeSuccess && context->identity.generation==identity+2 &&
              Requests::instance().snapshot().completions==before+2 && typed<calcRecord>("Records_Completed").val==links+3,
              "contract normal return did not publish exactly once");
        if(i==0)check(typed<aiRecord>(name.c_str()).val==17,"contract ai native return mismatch");
        else if(i==1)check(typed<longinRecord>(name.c_str()).val==17,"contract longin native return mismatch");
        else if(i==2)check(typed<int64inRecord>(name.c_str()).val==UINT32_MAX,"contract int64in native return mismatch");
        else if(i==3)check(std::string(typed<stringinRecord>(name.c_str()).val)=="1.3.6.1.4.1.53864","contract stringin native return mismatch");
        else if(i==4)check(std::string(typed<lsiRecord>(name.c_str()).val)==std::string(200,'X') &&
                          typed<lsiRecord>(name.c_str()).len==201,"contract lsi native return mismatch");
        else check(*static_cast<epicsUInt64*>(typed<waveformRecord>(name.c_str()).bptr)==UINT64_C(9007199254740993),"contract waveform native return mismatch");
        std::printf("{\"event\":\"contract_simm\",\"record\":\"%s\",\"terminal\":1,\"native\":1,\"generations\":2,\"completions\":2,\"flnk\":3,\"simulation_published\":false,\"native_recovered\":true}\n",name.c_str());
    }
    for(const char* kind:{"Ai","Ao"}) {
        const std::string name=std::string("Records_")+kind; auto* rec=record(name.c_str());
        auto* context=static_cast<RecordContext*>(rec->dpvt); const auto original=context->identity;
        const auto before=Requests::instance().snapshot().completions;
        DBADDR linr{}; check(dbNameToAddr((name+".LINR").c_str(),&linr)==0,"LINR field absent");
        const auto restore=[&] {
            check(dbPutField(&linr,DBR_USHORT,&no,1)==0,"LINR restoration put failed");
            check(until([&]{RecordLock lock(rec); return !rec->pact && !rec->rpro && !context->active;}),
                  "LINR restoration completion absent");
            check(until([&]{return context->owner->settled();}),"LINR restoration retained ownership");
            check(rec->sevr==NO_ALARM,"LINR restoration failed");
        };
        check(dbPutField(&linr,DBR_USHORT,&yes,1)==-1 && *static_cast<epicsEnum16*>(linr.pfield)==yes,
              "idle LINR put did not retain the value and report local process rejection");
        check(rec->stat==LINK_ALARM && context->identity==original && Requests::instance().snapshot().completions==before,
              "live idle LINR change admitted native work");
        restore();
        {
            RecordLock lock(rec); dbProcess(rec); check(rec->pact,"live LINR active request not admitted");
            check(until([&]{return Requests::instance().snapshot().entered==1 && context->terminal.result;}),"live LINR terminal absent");
            check(context->terminal.result->outcome==ipc::Outcome::Complete && context->terminal.id==context->identity,
                  "live LINR terminal identity mismatch");
            check(dbPutField(&linr,DBR_USHORT,&yes,1)==0 && rec->rpro,"active LINR put did not request Base reprocessing");
        }
        check(until([&]{RecordLock lock(rec); return !rec->pact && !rec->rpro && !context->active;}),"live LINR completion lost");
        check(until([&]{return context->owner->settled();}),"live LINR terminal retained");
        check(rec->stat==LINK_ALARM && rec->sevr==INVALID_ALARM && !context->published && !context->terminal.result &&
              Requests::instance().snapshot().completions==before+2,"live LINR failure lost alarm or ownership");
        restore();
        check(context->identity.generation==original.generation+3 && Requests::instance().snapshot().completions==before+3,
              "live LINR restoration did not recover");
        std::printf("{\"event\":\"contract_linr\",\"record\":\"%s\",\"idle_admitted\":false,\"active_completed\":true,\"recovered\":true,\"generations\":3}\n",name.c_str());
    }
    check(Requests::instance().snapshot().completions==26,"contract native completion count mismatch");
}
void outputEdges()
{
    auto& ao=typed<aoRecord>("Records_Ao");
    struct Rounding { double value; uint32_t bits; };
    const Rounding values[]={
        {1.0+std::ldexp(1.0,-24),0x3f800000}, {1.0+3*std::ldexp(1.0,-24),0x3f800002},
        {-1.0-std::ldexp(1.0,-24),0xbf800000}, {-1.0-3*std::ldexp(1.0,-24),0xbf800002},
        {0.1,0x3dcccccd}, {std::ldexp(1.0,-150),0}, {3*std::ldexp(1.0,-150),2},
        {std::ldexp(1.0,-126)-std::ldexp(1.0,-150),0x00800000}, {-0.0,0x80000000}
    };
    const int original=std::fegetround();
    for(const int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
        check(std::fesetround(mode)==0,"ambient rounding mode setup failed");
        for(const auto& value:values) {
            { RecordLock lock(reinterpret_cast<dbCommon*>(&ao)); ao.val=value.value; ao.udf=FALSE; }
            process("Records_Ao");
            check(std::fegetround()==mode,"actual ao path changed ambient rounding mode");
            process("Records_FloatRead");
            float expected; std::memcpy(&expected,&value.bits,sizeof(expected));
            const double observed=typed<aiRecord>("Records_FloatRead").val;
            check(observed==double(expected) && std::signbit(observed)==std::signbit(expected),
                  "actual binary32 SET/GET disagreed with specified IEEE bits");
            check(ao.val==value.value && std::signbit(ao.val)==std::signbit(value.value),
                  "rounded SET replaced requested ao value");
        }
    }
    check(std::fesetround(original)==0,"ambient rounding mode restore failed");
    for(const double invalid:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::max()}) {
        auto* context=static_cast<RecordContext*>(ao.dpvt);
        const auto identity=context->identity;
        { RecordLock lock(reinterpret_cast<dbCommon*>(&ao)); ao.val=invalid; ao.udf=FALSE; }
        process("Records_Ao",false);
        check(context->identity==identity && !ao.pact,"unusable ao value admitted a SET");
    }
    auto& wide=typed<int64outRecord>("Records_Int64out");
    auto* context=static_cast<RecordContext*>(wide.dpvt);
    const auto identity=context->identity;
    { RecordLock lock(reinterpret_cast<dbCommon*>(&wide)); wide.val=-1; wide.udf=FALSE; }
    process("Records_Int64out",false);
    check(context->identity==identity && !wide.pact,"negative Counter64 SET was admitted");
    auto& large=typed<lsoRecord>("Records_LargeLso");
    context=static_cast<RecordContext*>(large.dpvt);
    const auto before=context->identity;
    { RecordLock lock(reinterpret_cast<dbCommon*>(&large));
      std::memset(large.val,'Y',1025); large.val[1025]=0; large.len=1026; large.udf=FALSE; }
    process("Records_LargeLso",false);
    check(context->identity==before && !large.pact,"over-capacity lso SET was admitted");
    auto& text=typed<lsoRecord>("Records_Lso");
    { RecordLock lock(reinterpret_cast<dbCommon*>(&text)); text.val[0]=0; text.len=1; text.udf=FALSE; }
    process("Records_Lso"); process("Records_Lsi");
    check(typed<lsiRecord>("Records_Lsi").len==1 && !typed<lsiRecord>("Records_Lsi").val[0],
          "empty long string SET/GET included a native NUL byte");
    std::printf("{\"event\":\"output_edges\",\"rounding_modes\":4,\"wire_bit_fixtures\":36,\"invalid_set_rejected\":true,\"empty_len\":1}\n");
}
void maxPayloadQueue()
{
    auto& output=typed<lsoRecord>("Records_MaxLso");
    { RecordLock lock(reinterpret_cast<dbCommon*>(&output));
      std::memset(output.val,'M',32766); output.val[32766]=0; output.len=32767; output.udf=FALSE; }
    process("Records_MaxLso");
    QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
    check(blocker.queued && blocker.entered.wait(3.0),"max-payload external callback did not enter");
    const auto before=Requests::instance().snapshot().completions;
    for(const char* name:{"Records_MaxLsi1","Records_MaxLsi2"}) {
        auto* rec=record(name); RecordLock lock(rec); dbProcess(rec);
        check(rec->pact,"maximum GET did not admit");
    }
    check(until([]{return Requests::instance().snapshot().queued==2;}),"maximum GET terminals did not enter Base queue");
    auto scheduler=Runtime::instance().schedulerOwner();
    const auto held=scheduler->snapshot(1);
    auto* third=record("Records_MaxLsi3");
    auto* context=static_cast<RecordContext*>(third->dpvt);
    const auto identity=context->identity;
    { RecordLock lock(third); dbProcess(third);
      check(!third->pact && third->sevr==INVALID_ALARM && third->stat==READ_ALARM && context->identity==identity,
            "byte-exhausted record blocked or admitted native work"); }
    Runtime::instance().queueLimit("127.0.0.1",1,held.byteLimit/2);
    const auto lowered=scheduler->snapshot(1);
    check(lowered.count==2 && lowered.bytes==held.bytes && lowered.bytes>lowered.byteLimit,
          "live limit decrease discarded retained maximum results");
    Runtime::instance().queueLimit("127.0.0.1",8,4194304);
    { RecordLock lock(third); dbProcess(third); check(third->pact,"raised limit did not permit explicit subsequent GET"); }
    check(until([]{return Requests::instance().snapshot().queued==3;}),"third maximum GET did not complete native work");
    const auto three=scheduler->snapshot(1);
    check(held.count==2 && held.undelivered==2 && held.bytes>0 && three.count==3 && three.bytes>held.bytes,
          "maximum borrowed payloads lost count or byte reservation");
    blocker.release.trigger();
    check(until([]{return Requests::instance().snapshot().active==0;}),"maximum payload callbacks did not finish");
    check(until([&]{return scheduler->settled();}),"maximum payload native retirement retained reservations");
    for(const char* name:{"Records_MaxLsi1","Records_MaxLsi2","Records_MaxLsi3"}) {
        auto& input=typed<lsiRecord>(name); RecordLock lock(reinterpret_cast<dbCommon*>(&input));
        check(!input.pact && !input.udf && input.len==32767 && std::string(input.val)==std::string(32766,'M'),
              "maximum payload did not publish a complete VAL/LEN");
    }
    check(Requests::instance().snapshot().completions==before+3,"maximum payload callbacks completed more than once");
    Runtime::instance().queueLimit("127.0.0.1",ipc::DefaultCount,ipc::DefaultBytes);
    std::printf("{\"event\":\"max_payload_queue\",\"data_bytes\":32766,\"rejected_without_admission\":true,\"retained_before_release\":true,\"held_bytes\":%llu}\n",
                (unsigned long long)held.bytes);
}
void pressure(const char* name)
{
    QueueBlocker blocker;
    epicsCallback fillers[8]{};
    blocker.queued=callbackRequest(&blocker.callback)==0;
    check(blocker.queued,"external queue blocker admission failed");
    check(blocker.entered.wait(3.0),"actual Base callback did not enter external blocker");
    for(auto& filler:fillers) {
        callbackSetCallback(&QueueBlocker::count,&filler);
        callbackSetUser(&blocker,&filler); callbackSetPriority(priorityLow,&filler);
        check(callbackRequest(&filler)==0,"actual callback queue fill failed");
    }
    const auto before=Requests::instance().snapshot();
    auto* rec=record(name);
    {
        RecordLock lock(rec); dbProcess(rec); check(rec->pact,"pressure request did not admit");
    }
    const bool retry=until([&]{ const auto value=Requests::instance().snapshot();
                               return value.pending==1 && value.enqueueFailures>before.enqueueFailures; });
    const auto queue=Runtime::instance().schedulerOwner()->snapshot(1);
    const auto held=Requests::instance().snapshot();
    blocker.release.trigger();
    const bool completed=until([&]{ RecordLock lock(rec); return !rec->pact; });
    const bool fillersDone=until([&]{ return blocker.noops==8; });
    check(retry && held.completions==before.completions,"full callback queue lost or prematurely consumed terminal");
    check(queue.count==1 && queue.bytes>0 && queue.undelivered==1,"borrowed terminal lost full reservation");
    check(completed && fillersDone && Requests::instance().snapshot().completions==before.completions+1,
          "callback retry did not complete exactly once");
    check(until([&]{return Runtime::instance().schedulerOwner()->settled();}),"callback retry retained native generation");
    std::printf("{\"event\":\"callback_pressure\",\"record\":\"%s\",\"enqueue_failures\":%llu,\"full_reservation\":true,\"completed_once\":true}\n",name,
                (unsigned long long)(Requests::instance().snapshot().enqueueFailures-before.enqueueFailures));
}
void callbackPhases(const char* name)
{
    QueueBlocker first,second;
    epicsCallback fillers[7]{};
    first.queued=callbackRequest(&first.callback)==0;
    check(first.queued && first.entered.wait(3.0),"first pressure boundary did not enter");
    second.queued=callbackRequest(&second.callback)==0;
    check(second.queued,"second pressure boundary did not queue");
    for(auto& filler:fillers) {
        callbackSetCallback(&QueueBlocker::count,&filler);
        callbackSetUser(&first,&filler); callbackSetPriority(priorityLow,&filler);
        check(callbackRequest(&filler)==0,"pressure phase queue fill failed");
    }
    auto* rec=record(name); RecordLock lock(rec);
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    const auto before=Requests::instance().snapshot();
    const auto links=typed<calcRecord>("Records_Completed").val;
    const auto admittedBefore=monotonicUs();
    dbProcess(rec);
    const auto admittedAfter=monotonicUs();
    const auto identity=context->identity;
    auto owner=Runtime::instance().schedulerOwner();
    const auto deadline=owner->earliestDeadline(1);
    check(rec->pact && deadline>=admittedBefore+1000000 && deadline<=admittedAfter+1000000,
          "pressure did not observe original admission deadline");
    check(until([&]{const auto s=Requests::instance().snapshot();
        return s.pending==1 && s.enqueueFailures>before.enqueueFailures;}),"pressure did not retry failed enqueue");
    check(context->terminal.result && context->terminal.result->outcome==ipc::Outcome::Complete,
          "pressure did not select successful native terminal");
    const auto pending=Requests::instance().snapshot();
    const auto charge=owner->snapshot(1);
    check(until([&]{return monotonicUs()>deadline+20000;}),"pressure did not cross original deadline");
    const auto afterDeadline=Requests::instance().snapshot();
    check(afterDeadline.pending==1 && !afterDeadline.queued && !afterDeadline.running &&
          afterDeadline.completions==before.completions && context->identity==identity &&
          context->terminal.result->outcome==ipc::Outcome::Complete && rec->pact,
          "callback delay replaced selected result or original request");
    first.release.trigger();
    check(second.entered.wait(3.0),"second pressure boundary did not enter");
    check(until([]{return Requests::instance().snapshot().queued==1;}),"retried callback did not queue");
    const auto queued=Requests::instance().snapshot();
    check(!queued.pending && !queued.running && queued.completions==before.completions &&
          context->identity==identity,"queued callback ownership mismatch");
    second.release.trigger();
    check(until([]{const auto s=Requests::instance().snapshot();return s.running==1 && s.entered==1;}),
          "callback did not enter while actual record lock was held");
    const auto running=Requests::instance().snapshot();
    const auto held=owner->snapshot(1);
    check(!running.pending && !running.queued && running.completions==before.completions &&
          context->identity==identity && context->terminal.result->outcome==ipc::Outcome::Complete &&
          charge.count==1 && charge.bytes>0 && charge.undelivered==1 &&
          held.count==1 && held.bytes==charge.bytes && held.undelivered==1,
          "running callback lost terminal or charged ownership");
    const auto releasedAt=monotonicUs();
    lock.unlock();
    check(until([&]{RecordLock done(rec);return !rec->pact;}),"pressure phase record did not finish");
    check(until([&]{return Requests::instance().snapshot().entered==0 && first.noops==7 && owner->settled();}),
          "pressure phase callbacks or native ownership did not settle");
    const auto finished=Requests::instance().snapshot();
    check(finished.completions==before.completions+1 && !finished.active && !finished.pending &&
          !finished.queued && !finished.running && rec->stat==NO_ALARM && rec->sevr==NO_ALARM &&
          context->identity==identity && typed<calcRecord>("Records_Completed").val==links+1,
          "pressure phase completion was not successful exactly once");
    std::printf("{\"event\":\"callback_phases\",\"record\":\"%s\",\"pending\":%llu,\"queued\":%llu,\"running\":%llu,"
                "\"enqueue_failures\":%llu,\"deadline_us\":%llu,\"released_us\":%llu,\"bytes\":%llu,"
                "\"generation\":%llu,\"admission\":%llu,\"completions_delta\":%llu}\n",name,
                (unsigned long long)pending.pending,(unsigned long long)queued.queued,(unsigned long long)running.running,
                (unsigned long long)(afterDeadline.enqueueFailures-before.enqueueFailures),
                (unsigned long long)deadline,(unsigned long long)releasedAt,(unsigned long long)held.bytes,
                (unsigned long long)identity.generation,(unsigned long long)identity.admission,
                (unsigned long long)(finished.completions-before.completions));
}
void finalClauseTrial()
{
    check(Requests::instance().snapshot().contexts==16 && !Requests::instance().snapshot().completions,
          "final-clause initialization inventory mismatch");
    typed<aoRecord>("Records_PressureSet").udf=FALSE;
    callbackPhases("Records_PressureGet"); callbackPhases("Records_PressureSet");
    auto& drive=typed<int64outRecord>("Records_Drive64");
    const epicsInt64 lower=9007199254740993LL,upper=9007199254740995LL;
    check(drive.drvl==lower && drive.drvh==upper,"integer drive limits were not initialized exactly");
    const epicsInt64 requested[]={INT64_MIN,lower+1,INT64_MAX};
    const epicsInt64 expected[]={lower,lower+1,upper};
    DBADDR address{};
    check(dbNameToAddr("Records_Drive64.VAL",&address)==0,"integer drive VAL absent");
    for(unsigned i=0;i<3;++i) {
        { RecordLock lock(reinterpret_cast<dbCommon*>(&drive));
          check(dbPut(&address,DBR_INT64,&requested[i],1)==0,"integer drive DBR put failed"); }
        process("Records_Drive64"); process("Records_DriveRead");
        const auto observed=typed<int64inRecord>("Records_DriveRead").val;
        check(drive.val==expected[i] && observed==expected[i],"Base integer drive clipping disagreed with native GET");
        std::printf("{\"event\":\"integer_drive\",\"requested\":\"%lld\",\"value\":\"%lld\",\"readback\":\"%lld\"}\n",
                    (long long)requested[i],(long long)drive.val,(long long)observed);
    }
    struct Adjacent { uint64_t source; uint32_t expected; };
    const Adjacent values[]={{0x3ff000000fffffffULL,0x3f800000},{0x3ff0000010000001ULL,0x3f800001},
                             {0xbff000000fffffffULL,0xbf800000},{0xbff0000010000001ULL,0xbf800001}};
    auto& output=typed<aoRecord>("Records_Ao");
    const int original=std::fegetround();
    for(const int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
        check(std::fesetround(mode)==0,"adjacent rounding setup failed");
        for(const auto& value:values) {
            double requestedValue; std::memcpy(&requestedValue,&value.source,sizeof(requestedValue));
            { RecordLock lock(reinterpret_cast<dbCommon*>(&output)); output.val=requestedValue;output.udf=FALSE; }
            process("Records_Ao");
            check(std::fegetround()==mode,"adjacent SET changed ambient rounding mode");
            process("Records_FloatRead");
            float expectedValue;std::memcpy(&expectedValue,&value.expected,sizeof(expectedValue));
            const double observed=typed<aiRecord>("Records_FloatRead").val;
            check(observed==double(expectedValue) && output.val==requestedValue,
                  "adjacent binary64 SET rounded to incorrect binary32 value");
            std::printf("{\"event\":\"adjacent_rounding\",\"source_hex\":\"%016llx\",\"expected_hex\":\"%08x\",\"mode\":%d}\n",
                        (unsigned long long)value.source,value.expected,mode);
        }
    }
    check(std::fesetround(original)==0,"adjacent rounding restore failed");
    check(Requests::instance().snapshot().completions==40,"final-clause native completion count mismatch");
}
void queuedShutdown()
{
    QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
    check(blocker.queued && blocker.entered.wait(3.0),"shutdown external queue blocker did not enter");
    const auto before=Requests::instance().snapshot().completions;
    auto* rec=record("Records_Longin");
    { RecordLock lock(rec); dbProcess(rec); check(rec->pact,"queued shutdown request did not admit"); }
    check(until([]{return Requests::instance().snapshot().queued==1;}),"shutdown completion was not queued in Base");
    std::atomic<bool> finished{false};
    std::thread shutdown([&]{testIocShutdownOk(); finished=true;});
    const bool retained=until([]{const auto value=Requests::instance().snapshot();
                                 return value.drainFailed && !value.entryOpen && value.detachAllowed &&
                                        value.active==1 && value.queued==1 && value.entered==0;});
    const bool waiting=!finished;
    blocker.release.trigger(); shutdown.join();
    const auto after=Requests::instance().snapshot();
    check(retained && waiting,"queued callback storage was freed before actual queue cleanup");
    check(after.contexts==0 && after.completions==before+1 && after.drainFailed,
          "isolated queue cleanup did not finalize abandoned ownership exactly once");
    check(Runtime::instance().snapshot().state==State::IncompleteStopped && !Runtime::instance().start(),
          "abandoned callback drain permitted successful restart");
    std::printf("{\"event\":\"queued_shutdown\",\"storage_retained_until_cleanup\":true,\"abandoned_finalized_once\":true,\"restart_rejected\":true}\n");
}
void blockedShutdown()
{
    auto* rec=record("Records_Longin");
    RecordLock lock(rec);
    dbProcess(rec); check(rec->pact,"shutdown request did not admit");
    const bool entered=until([]{ return Requests::instance().snapshot().entered==1; });
    check(entered,"callback did not acquire lease before blocking on actual record lock");
    std::atomic<bool> finished{false};
    std::thread shutdown([&]{ testIocShutdownOk(); finished=true; });
    const bool closed=until([]{ const auto value=Requests::instance().snapshot();
                                return value.drainFailed && !value.entryOpen && value.entered==1; });
    const bool waiting=!finished;
    lock.unlock(); shutdown.join();
    const auto drained=Requests::instance().snapshot();
    const auto stopped=Runtime::instance().snapshot();
    check(closed && waiting,"shutdown closed links before entered record processing completed");
    check(drained.contexts==0 && drained.entered==0 && drained.drainFailed,"failed drain or isolated cleanup accounting lost");
    check(stopped.state==State::IncompleteStopped && !stopped.admission,"expired record drain reported successful stop");
    check(!Runtime::instance().start() && Runtime::instance().snapshot().created==stopped.created,
          "expired drain allowed a replacement activation");
    std::printf("{\"event\":\"blocked_shutdown\",\"closed_gate_before_release\":true,\"waited_for_entered\":true,\"drain_failed\":true,\"restart_rejected\":true}\n");
}
void responseMode(const char* mode)
{
    const char* path=std::getenv("SNMP3_RECORD_FAULT_CONTROL");
    check(path!=nullptr,"response control path absent");
    const std::string temporary=std::string(path)+".next";
    { std::ofstream stream(temporary); stream<<mode<<"\n"; stream.close(); check(bool(stream),"response control write failed"); }
    check(std::rename(temporary.c_str(),path)==0,"response control rename failed");
}
struct AlarmMonitor {
    dbEventCtx events=nullptr;
    dbChannel* channel=nullptr;
    dbEventSubscription subscription=nullptr;
    std::atomic<unsigned> count{0},alarm{0},emptyComm{0},neverSent{0};
    static void changed(void* raw,dbChannel*,int,db_field_log* value)
    {
        auto& self=*static_cast<AlarmMonitor*>(raw);
        if(value && (value->mask&DBE_ALARM)) {
            self.alarm=unsigned(value->stat)*10+value->sevr;
            if(value->stat==COMM_ALARM && value->sevr==INVALID_ALARM) {
                if(!value->amsg[0])++self.emptyComm;
                if(std::strcmp(value->amsg,"deadline before send")==0)++self.neverSent;
            }
            ++self.count;
        }
    }
    explicit AlarmMonitor(const std::string& name)
    {
        events=db_init_events(); check(events!=nullptr,"monitor context absent");
        check(db_start_events(events,"recordAlarmMonitor",nullptr,nullptr,epicsThreadPriorityLow)==0,"monitor thread failed");
        channel=dbChannelCreate((name+".VAL").c_str());
        check(channel && dbChannelOpen(channel)==0,"monitor channel failed");
        subscription=db_add_event(events,channel,&AlarmMonitor::changed,this,DBE_ALARM);
        check(subscription!=nullptr,"alarm subscription failed"); db_event_enable(subscription);
    }
    ~AlarmMonitor()
    {
        if(subscription)db_cancel_event(subscription);
        if(channel)dbChannelDelete(channel);
        if(events)db_close_events(events);
    }
};
std::vector<unsigned char> inputImage(dbCommon* rec)
{
    const void* data=nullptr; size_t size=0;
    switch(static_cast<RecordContext*>(rec->dpvt)->definition.kind) {
    case RecordKind::Ai: data=&reinterpret_cast<aiRecord*>(rec)->val; size=sizeof(epicsFloat64); break;
    case RecordKind::Longin: data=&reinterpret_cast<longinRecord*>(rec)->val; size=sizeof(epicsInt32); break;
    case RecordKind::Int64in: data=&reinterpret_cast<int64inRecord*>(rec)->val; size=sizeof(epicsInt64); break;
    case RecordKind::Stringin: data=reinterpret_cast<stringinRecord*>(rec)->val; size=MAX_STRING_SIZE; break;
    case RecordKind::Lsi: { const auto& value=*reinterpret_cast<lsiRecord*>(rec); data=value.val; size=value.sizv; break; }
    case RecordKind::Waveform: { const auto& value=*reinterpret_cast<waveformRecord*>(rec); data=value.bptr; size=value.nelm*sizeof(epicsUInt64); break; }
    default: check(false,"unsupported input snapshot");
    }
    const auto* bytes=static_cast<const unsigned char*>(data);
    return std::vector<unsigned char>(bytes,bytes+size);
}
void alarmAfterSuccess()
{
    responseMode("pass"); outputs();
    for(const char* kind:{"Ai","Longin","Int64in","Stringin","Lsi","Waveform"}) {
        const std::string name=std::string("Records_")+kind;
        auto* rec=record(name.c_str()); auto* context=static_cast<RecordContext*>(rec->dpvt);
        put((name+".FLNK").c_str(),DBR_STRING,"Records_Completed");
        AlarmMonitor monitor(name);
        for(const char* mode:{"error-status","too-big","bad-error-index","no-such-object",
                              "no-such-instance","end-of-mib","wrong-type","drop-all"}) {
            responseMode("pass"); process(name.c_str());
            check(context->nativeSuccess && context->published && !rec->udf,"successful GET did not establish valid input");
            std::vector<unsigned char> image;
            { RecordLock lock(rec); image=inputImage(rec); }
            const auto length=context->definition.kind==RecordKind::Lsi?reinterpret_cast<lsiRecord*>(rec)->len:
                              context->definition.kind==RecordKind::Waveform?reinterpret_cast<waveformRecord*>(rec)->nord:0;
            const auto count=monitor.count.load();
            const auto links=typed<calcRecord>("Records_Completed").val;
            responseMode(mode); process(name.c_str(),false,false,0,false,true);
            const unsigned expected=std::strcmp(mode,"drop-all")==0?COMM_ALARM:READ_ALARM;
            check(until([&]{return monitor.count>count && monitor.alarm==expected*10+INVALID_ALARM;}),"failure alarm monitor absent");
            RecordLock lock(rec);
            check(rec->stat==expected && rec->sevr==INVALID_ALARM && !rec->udf,"native failure alarm or UDF mismatch");
            check(inputImage(rec)==image && !context->published && context->nativeSuccess,"native failure changed prior valid input");
            if(context->definition.kind==RecordKind::Lsi)check(reinterpret_cast<lsiRecord*>(rec)->len==length,"failed GET changed LEN");
            if(context->definition.kind==RecordKind::Waveform)check(reinterpret_cast<waveformRecord*>(rec)->nord==length && !reinterpret_cast<waveformRecord*>(rec)->busy,"failed GET changed NORD/BUSY");
            check(typed<calcRecord>("Records_Completed").val==links+1,"failed GET lost or duplicated FLNK");
            std::printf("{\"event\":\"alarm_after_good\",\"record\":\"%s\",\"mode\":\"%s\",\"generation\":%llu,\"alarm\":%u,\"severity\":%u,\"udf\":%u,\"length\":%u,\"preserved\":true,\"native_success\":true,\"published\":false,\"monitor\":%u}\n",
                        name.c_str(),mode,(unsigned long long)context->identity.generation,rec->stat,rec->sevr,rec->udf,unsigned(length),monitor.alarm.load());
        }
    }
    responseMode("pass");
    auto& ai=typed<aiRecord>("Records_Ai"); auto* rec=reinterpret_cast<dbCommon*>(&ai);
    { RecordLock lock(rec); ai.hihi=-1000; ai.hhsv=MAJOR_ALARM; }
    process("Records_Ai",false); check(ai.stat==HIHI_ALARM && ai.sevr==MAJOR_ALARM,"competing limit alarm absent");
    responseMode("error-status"); process("Records_Ai",false,false,0,false,true);
    check(ai.stat==READ_ALARM && ai.sevr==INVALID_ALARM,"limit alarm hid the native error");
    { RecordLock lock(rec); ai.hhsv=NO_ALARM; ai.udf=TRUE; }
    process("Records_Ai",false,false,0,false,true);
    check(ai.stat==READ_ALARM && ai.sevr==INVALID_ALARM && ai.udf,"UDF alarm hid the native error");
    responseMode("pass"); process("Records_Ai");
    std::printf("{\"event\":\"competing_alarm\",\"limit_observed\":true,\"native_error_wins\":true,\"udf_preserved\":true,\"recovered\":true}\n");
}
void chainAndFanout()
{
    const char* selected=std::getenv("SNMP3_CHAIN_KIND");
    check(selected!=nullptr,"chain source selection absent");
    responseMode("pass"); outputs();
    auto* next=record("Records_ChainB");
    auto* nextContext=static_cast<RecordContext*>(next->dpvt);
    auto& phase=typed<calcRecord>("Records_ChainPhase");
    auto& value=typed<lsiRecord>("Records_ChainValue");
    auto& count=typed<calcRecord>("Records_ChainCount");
    for(const char* kind:{"Ai","Longin","Int64in","Stringin","Lsi","Waveform",
                         "Ao","Longout","Int64out","Stringout","Lso"}) {
        if(std::strcmp(kind,selected)!=0)continue;
        const std::string name=std::string("Records_")+kind;
        auto* rec=record(name.c_str()); auto* context=static_cast<RecordContext*>(rec->dpvt);
        responseMode("pass"); process(name.c_str());
        put((name+".FLNK").c_str(),DBR_STRING,"Records_ChainPhase");
        const char* fields[]={"PACT","STAT","SEVR","UDF"};
        const char* links[]={"INPA","INPB","INPC","INPD"};
        for(unsigned i=0;i<4;++i)
            put((std::string("Records_ChainPhase.")+links[i]).c_str(),DBR_STRING,(name+"."+fields[i]+" NPP NMS").c_str());
        const bool longText=context->definition.kind==RecordKind::Lsi || context->definition.kind==RecordKind::Lso;
        put("Records_ChainValue.INP",DBR_STRING,longText?(name+".VAL$ NPP NMS").c_str():"unchanged");
        for(const char* mode:{"pass","error-status","duplicate","drop-all"}) {
            responseMode(mode);
            const bool good=std::strcmp(mode,"pass")==0 || std::strcmp(mode,"duplicate")==0;
            const bool deadline=std::strcmp(mode,"drop-all")==0;
            const auto before=Requests::instance().snapshot().completions;
            const auto linksBefore=count.val;
            const auto previous=context->identity.generation;
            const auto targetPrevious=nextContext->identity.generation;
            unsigned terminal=0,native=0;
            uint64_t terminalAt=0;
            {
                RecordLock lock(rec);
                dbProcess(rec);
                check(rec->pact && context->active,"chain source did not admit");
                if(context->definition.kind==RecordKind::Waveform)reinterpret_cast<waveformRecord*>(rec)->busy=TRUE;
                check(until([]{return Requests::instance().snapshot().entered==1;}),"chain source terminal did not enter");
                check(context->terminal.result!=nullptr,"chain source has no terminal");
                terminal=unsigned(context->terminal.result->outcome); native=context->terminal.result->nativeOutcome;
                terminalAt=monotonicUs();
                check(count.val==linksBefore && nextContext->identity.generation==targetPrevious,
                      "FLNK target started before source terminal processing");
            }
            check(until([&]{RecordLock lock(next); return !next->pact && count.val==linksBefore+1;}),"chain target did not complete once");
            check(until([&]{return context->owner->settled();}),"chain retirement did not settle");
            {
                RecordLock lock(rec);
                const unsigned expected=good?NO_ALARM:deadline?COMM_ALARM:
                    context->definition.kind<=RecordKind::Waveform?READ_ALARM:WRITE_ALARM;
                check(!rec->pact && !rec->rpro && !context->active && !context->terminal.result,
                      "chain source retained generation");
                check(rec->stat==expected && rec->sevr==(good?NO_ALARM:INVALID_ALARM),"chain source alarm mismatch");
                check(phase.a==1 && phase.b==rec->stat && phase.c==rec->sevr && phase.d==rec->udf,
                      "FLNK did not observe source PACT and final alarm/UDF");
                if(longText)check(value.len==201 && std::string(value.val)==std::string(200,'X'),
                                  "FLNK did not observe complete long VAL/LEN");
                if(context->definition.kind==RecordKind::Waveform)
                    check(!reinterpret_cast<waveformRecord*>(rec)->busy,"waveform completion retained BUSY");
                check(context->identity.generation==previous+1 && nextContext->identity.generation==targetPrevious+1 &&
                      Requests::instance().snapshot().completions==before+2,"chain duplicated generation completion");
                std::printf("{\"event\":\"chain_flow\",\"record\":\"%s\",\"mode\":\"%s\",\"terminal\":%u,\"native\":%u,\"terminal_at\":%llu,\"finished_at\":%llu,\"source_handle\":%llu,\"source_generation\":%llu,\"target_handle\":%llu,\"target_generation\":%llu,\"source_pact_at_flnk\":%.0f,\"alarm\":%u,\"severity\":%u,\"value_length\":%u,\"completions\":2,\"flnk\":1}\n",
                            name.c_str(),mode,terminal,native,(unsigned long long)terminalAt,(unsigned long long)monotonicUs(),
                            (unsigned long long)context->handle,(unsigned long long)context->identity.generation,
                            (unsigned long long)nextContext->handle,(unsigned long long)nextContext->identity.generation,
                            phase.a,rec->stat,rec->sevr,unsigned(longText?value.len:0));
            }
            { RecordLock lock(next); check(next->stat==NO_ALARM && !nextContext->active && !nextContext->terminal.result,
                                          "chain target did not release successful GET"); }
        }
        put((name+".FLNK").c_str(),DBR_STRING,"");
    }
    responseMode("pass");
    for(unsigned trial=0;trial<2;++trial) {
        const char* first=trial?"Records_Fast4":"Records_Slow4";
        const char* second=trial?"Records_Slow6":"Records_Fast6";
        auto* slow=record(trial?second:first); auto* fast=record(trial?first:second);
        auto* slowContext=static_cast<RecordContext*>(slow->dpvt);
        auto* fastContext=static_cast<RecordContext*>(fast->dpvt);
        put("Records_Fanout.LNK0",DBR_STRING,first); put("Records_Fanout.LNK1",DBR_STRING,second);
        const auto before=Requests::instance().snapshot().completions;
        const auto started=monotonicUs();
        {
            RecordLock lock(record("Records_Fanout")); dbProcess(record("Records_Fanout"));
            check(slow->pact && fast->pact && typed<calcRecord>("Records_FanoutIssued").val==trial+1,
                  "fanout acted as a completion barrier");
            check(slowContext->active && fastContext->active,"fanout did not admit both independent targets");
        }
        check(until([&]{RecordLock lock(fast); return !fast->pact;}),"fast fanout target did not finish");
        const auto fastAt=monotonicUs();
        { RecordLock lock(slow); check(slow->pact && slowContext->active,"slow target finished before independent fast target"); }
        check(until([&]{RecordLock lock(slow); return !slow->pact;}),"slow fanout target did not finish");
        const auto slowAt=monotonicUs();
        check(until([&]{return slowContext->owner->settled();}),"fanout retirement did not settle");
        check(Requests::instance().snapshot().completions==before+2,"fanout completion count mismatch");
        for(auto* rec:{slow,fast}) {
            RecordLock lock(rec);
            check(rec->stat==NO_ALARM && typed<calcRecord>((std::string(rec->name)+"Done").c_str()).val==1,
                  "fanout target alarm or FLNK count mismatch");
        }
        std::printf("{\"event\":\"fanout_flow\",\"trial\":%u,\"first\":\"%s\",\"second\":\"%s\",\"started\":%llu,\"fast_at\":%llu,\"slow_at\":%llu,\"no_barrier\":true,\"completions\":2}\n",
                    trial,first,second,(unsigned long long)started,(unsigned long long)fastAt,(unsigned long long)slowAt);
    }
}
void nativeTimeouts()
{
    for(const char* kind:{"Ai","Longin","Int64in","Stringin","Lsi","Waveform",
                         "Ao","Longout","Int64out","Stringout","Lso"}) {
        const std::string name=std::string("Records_Timeout")+kind;
        auto* rec=record(name.c_str());
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        const bool input=context->definition.kind<=RecordKind::Waveform;
        {
            RecordLock lock(rec); rec->udf=input;
            if(context->definition.kind==RecordKind::Lso) {
                auto& value=*reinterpret_cast<lsoRecord*>(rec);
                std::strcpy(value.val,"timeout"); value.len=8;
            }
        }
        process(name.c_str(),false,false,2);
        check(rec->stat==COMM_ALARM && rec->sevr==INVALID_ALARM,
              (std::string("native timeout communication alarm mismatch: ")+name).c_str());
        check(!context->nativeSuccess && !context->published,"timeout published native input");
        if(input)check(bool(rec->udf)==(context->definition.kind!=RecordKind::Waveform),
                       "timeout did not preserve record-specific UDF behavior");
        if(context->definition.kind==RecordKind::Waveform)
            check(!typed<waveformRecord>(name.c_str()).busy,"timeout left waveform BUSY set");
        std::printf("{\"event\":\"native_timeout\",\"record\":\"%s\",\"communication_alarm\":true}\n",name.c_str());
    }
    process("Records_Deadline",false,false,0,true);
    check(record("Records_Deadline")->stat==COMM_ALARM && record("Records_Deadline")->sevr==INVALID_ALARM,
          "record deadline communication alarm mismatch");
    std::printf("{\"event\":\"record_deadline\",\"communication_alarm\":true}\n");
}
struct OutputFixture {
    RecordKind kind;
    const char* name;
    const char* target;
    const char* readback;
};
const OutputFixture PolicyOutputs[]={
    {RecordKind::Ao,"Records_PolicyAo","Records_SimAo","Records_FloatRead"},
    {RecordKind::Longout,"Records_PolicyLongout","Records_SimLongout","Records_Ai"},
    {RecordKind::Int64out,"Records_PolicyInt64out","Records_SimInt64out","Records_Waveform"},
    {RecordKind::Stringout,"Records_PolicyStringout","Records_SimStringout","Records_Lsi"},
    {RecordKind::Lso,"Records_PolicyLso","Records_SimLso","Records_Lsi"}
};
double outputDouble(unsigned value) { return value==3?2.5:30.5+value; }
epicsInt32 outputInteger(unsigned value) { return value==3?2:30+value; }
epicsInt64 outputWide(unsigned value) { return INT64_C(9007199254740991)+2*value; }
std::string outputText(RecordKind kind,unsigned value)
{
    if(value==3)return "ivov";
    return kind==RecordKind::Lso?std::string(200,value==1?'F':'L'):
           (value==1?"first-short":"latest-short");
}
void prepareOutput(const OutputFixture& fixture,unsigned value)
{
    auto* rec=record(fixture.name); RecordLock lock(rec); rec->udf=FALSE;
    switch(fixture.kind) {
    case RecordKind::Ao: reinterpret_cast<aoRecord*>(rec)->val=outputDouble(value); break;
    case RecordKind::Longout: reinterpret_cast<longoutRecord*>(rec)->val=outputInteger(value); break;
    case RecordKind::Int64out: reinterpret_cast<int64outRecord*>(rec)->val=outputWide(value); break;
    case RecordKind::Stringout: std::strcpy(reinterpret_cast<stringoutRecord*>(rec)->val,outputText(fixture.kind,value).c_str()); break;
    case RecordKind::Lso: {
        auto& output=*reinterpret_cast<lsoRecord*>(rec); const auto text=outputText(fixture.kind,value);
        std::strcpy(output.val,text.c_str()); output.len=text.size()+1; break;
    }
    default: check(false,"unexpected policy output kind");
    }
}
void checkOutput(const OutputFixture& fixture,unsigned value,bool simulation=false,bool readback=false)
{
    const char* name=simulation?fixture.target:readback?fixture.readback:fixture.name;
    const bool input=simulation || readback;
    switch(fixture.kind) {
    case RecordKind::Ao:
        check((input?typed<aiRecord>(name).val:typed<aoRecord>(name).val)==outputDouble(value),"policy floating value mismatch"); break;
    case RecordKind::Longout:
        check((readback?typed<aiRecord>(name).val:simulation?typed<longinRecord>(name).val:typed<longoutRecord>(name).val)==outputInteger(value),"policy integer value mismatch"); break;
    case RecordKind::Int64out:
        if(readback) {
            const auto& waveform=typed<waveformRecord>(name);
            check(waveform.nord==1 && *static_cast<epicsUInt64*>(waveform.bptr)==epicsUInt64(outputWide(value)),"policy wide wire value lost precision");
        } else check((simulation?typed<int64inRecord>(name).val:typed<int64outRecord>(name).val)==outputWide(value),"policy wide value lost precision");
        break;
    case RecordKind::Stringout:
        check(std::string(readback?typed<lsiRecord>(name).val:simulation?typed<stringinRecord>(name).val:typed<stringoutRecord>(name).val)==outputText(fixture.kind,value),"policy short string mismatch"); break;
    case RecordKind::Lso: {
        const auto text=outputText(fixture.kind,value);
        check(std::string(input?typed<lsiRecord>(name).val:typed<lsoRecord>(name).val)==text &&
              (input?typed<lsiRecord>(name).len:typed<lsoRecord>(name).len)==text.size()+1,"policy long string value or LEN mismatch"); break;
    }
    default: check(false,"unexpected policy value kind");
    }
}
void policyCompletion(const OutputFixture& fixture,epicsEnum16 ivoa,bool simulated=false,epicsEnum16 oopt=0)
{
    epicsEnum16 no=0;
    put((std::string(fixture.name)+".IVOA").c_str(),DBR_USHORT,&no);
    put((std::string(fixture.name)+".SIMM").c_str(),DBR_USHORT,&no);
    prepareOutput(fixture,1);
    auto* rec=record(fixture.name); auto* context=static_cast<RecordContext*>(rec->dpvt);
    const auto before=Requests::instance().snapshot().completions;
    const auto links=typed<calcRecord>("Records_PolicyCompleted").val;
    QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
    check(blocker.queued && blocker.entered.wait(3.0),"policy callback blocker did not enter");
    { RecordLock lock(rec); dbProcess(rec); check(rec->pact && context->active,"policy SET did not admit"); }
    check(until([]{return Requests::instance().snapshot().queued==1;}),"policy terminal did not enter Base queue");
    {
        RecordLock lock(rec);
        check(context->terminal.result && context->terminal.result->outcome==ipc::Outcome::NativeFailure &&
              context->terminal.result->nativeOutcome==2,"policy fixture did not produce actual native timeout");
    }
    put((std::string(fixture.name)+".IVOA").c_str(),DBR_USHORT,&ivoa);
    if(simulated) { epicsEnum16 yes=1; put((std::string(fixture.name)+".SIMM").c_str(),DBR_USHORT,&yes); }
    if(oopt)put((std::string(fixture.name)+".OOPT").c_str(),DBR_USHORT,&oopt);
    prepareOutput(fixture,2);
    blocker.release.trigger();
    check(until([&]{RecordLock lock(rec); return !rec->pact && !context->active;}),"output policy bypass lost completion");
    {
        RecordLock lock(rec);
        check(rec->sevr==INVALID_ALARM && rec->stat==(oopt?LINK_ALARM:COMM_ALARM),"output policy bypass suppressed terminal error");
        check(!rec->rpro && !context->terminal.result && Requests::instance().snapshot().completions==before+1,
              "output policy bypass duplicated or retained terminal");
        checkOutput(fixture,2);
    }
    if(simulated)checkOutput(fixture,fixture.kind==RecordKind::Ao?1:2,true);
    check(typed<calcRecord>("Records_PolicyCompleted").val==links+1,"policy bypass lost or duplicated Base FLNK");
    check(until([&]{return context->owner->settled();}),"policy bypass retained native ownership");
    std::printf("{\"event\":\"output_policy\",\"record\":\"%s\",\"ivoa\":%u,\"simulation\":%s,\"oopt\":%u,\"terminal_released_once\":true,\"latest_requested_preserved\":true}\n",
                fixture.name,ivoa,simulated?"true":"false",oopt);
    if(oopt)put((std::string(fixture.name)+".OOPT").c_str(),DBR_USHORT,&no);
}
void outputPolicies()
{
    for(unsigned choice=1;choice<=5;++choice) {
        const auto name="Records_BadOopt"+std::to_string(choice);
        check(record(name.c_str())->dpvt==nullptr,"unsupported initial OOPT created a usable binding");
    }
    for(const auto& fixture:PolicyOutputs) {
        for(epicsEnum16 mode=0;mode<=2;++mode)policyCompletion(fixture,mode);
        policyCompletion(fixture,menuIvoaContinue_normally,true);
        const auto root=std::string(fixture.name);
        auto* rec=record(fixture.name); auto* context=static_cast<RecordContext*>(rec->dpvt);
        epicsEnum16 zero=0,yes=1;
        put((root+".IVOA").c_str(),DBR_USHORT,&zero);
        put((root+".SIMM").c_str(),DBR_USHORT,&yes);
        for(double delay:{-1.0,0.01}) {
            put((root+".SDLY").c_str(),DBR_DOUBLE,&delay);
            prepareOutput(fixture,1);
            const auto identity=context->identity;
            const auto before=Requests::instance().snapshot().completions;
            { RecordLock lock(rec); dbProcess(rec); }
            check(until([&]{RecordLock lock(rec); return !rec->pact;}),"Base delayed output simulation did not finish");
            check(!context->active && context->identity==identity && Requests::instance().snapshot().completions==before,
                  "simulation-only output admitted a native generation");
            checkOutput(fixture,1,true);
            std::printf("{\"event\":\"output_simulation\",\"record\":\"%s\",\"delay\":%.3f,\"native_admission\":false}\n",fixture.name,delay);
        }
        const double synchronous=-1;
        put((root+".SDLY").c_str(),DBR_DOUBLE,&synchronous);
        put((root+".SIMM").c_str(),DBR_USHORT,&zero);
        epicsEnum16 substitute=menuIvoaSet_output_to_IVOV;
        put((root+".IVOA").c_str(),DBR_USHORT,&substitute);
        if(fixture.kind==RecordKind::Ao) {
            const double value=outputDouble(3);
            put((root+".IVOV").c_str(),DBR_DOUBLE,&value);
            RecordLock lock(rec); auto& output=*reinterpret_cast<aoRecord*>(rec);
            output.hihi=0; output.hhsv=INVALID_ALARM;
        } else if(fixture.kind==RecordKind::Longout) {
            const auto value=outputInteger(3); put((root+".IVOV").c_str(),DBR_LONG,&value);
        } else if(fixture.kind==RecordKind::Int64out) {
            const auto value=outputWide(3); put((root+".IVOV").c_str(),DBR_INT64,&value);
        } else put((root+".IVOV").c_str(),DBR_STRING,"ivov");
        prepareOutput(fixture,1);
        if(fixture.kind!=RecordKind::Ao) { RecordLock lock(rec); rec->udf=TRUE; }
        process(fixture.name,false,false,2);
        checkOutput(fixture,3);
        process(fixture.readback);
        if(fixture.kind==RecordKind::Ao)
            std::printf("{\"event\":\"ivov_ao_state\",\"val\":%.17g,\"oval\":%.17g,\"ivov\":%.17g,\"get\":%.17g}\n",
                        typed<aoRecord>(fixture.name).val,typed<aoRecord>(fixture.name).oval,
                        typed<aoRecord>(fixture.name).ivov,typed<aiRecord>(fixture.readback).val);
        checkOutput(fixture,3,false,true);
        std::printf("{\"event\":\"output_ivov\",\"record\":\"%s\",\"wire_readback_exact\":true}\n",fixture.name);
        for(epicsEnum16 mode:{epicsEnum16(menuIvoaDon_t_drive_outputs),epicsEnum16(menuIvoaContinue_normally)}) {
            put((root+".IVOA").c_str(),DBR_USHORT,&mode);
            prepareOutput(fixture,1);
            if(fixture.kind!=RecordKind::Ao) { RecordLock lock(rec); rec->udf=TRUE; }
            const auto identity=context->identity;
            const auto before=Requests::instance().snapshot().completions;
            process(fixture.name,false,false,mode==menuIvoaContinue_normally?2:0);
            if(mode==menuIvoaDon_t_drive_outputs)
                check(context->identity==identity && Requests::instance().snapshot().completions==before,
                      "first-pass Don't drive outputs admitted SET");
            else { process(fixture.readback); checkOutput(fixture,1,false,true); }
            checkOutput(fixture,1);
            std::printf("{\"event\":\"output_first_policy\",\"record\":\"%s\",\"ivoa\":%u,\"native_admission\":%s}\n",
                        fixture.name,mode,mode==menuIvoaContinue_normally?"true":"false");
        }
        if(fixture.kind==RecordKind::Ao) { RecordLock lock(rec); reinterpret_cast<aoRecord*>(rec)->hhsv=NO_ALARM; }
        put((root+".IVOA").c_str(),DBR_USHORT,&zero);
    }
    for(epicsEnum16 choice=1;choice<=5;++choice)policyCompletion(PolicyOutputs[1],0,false,choice);
    auto& drive=typed<aoRecord>("Records_DriveAo");
    { RecordLock lock(reinterpret_cast<dbCommon*>(&drive)); drive.val=10; drive.udf=FALSE; }
    process("Records_DriveAo"); process("Records_FloatRead");
    check(drive.val==5 && drive.oval==1 && typed<aiRecord>("Records_FloatRead").val==1,"ao Base drive limit or OVAL capture bypassed");
    process("Records_DriveAo"); process("Records_FloatRead");
    check(drive.val==5 && drive.oval==2 && typed<aiRecord>("Records_FloatRead").val==2,"ao Base output rate limit bypassed");
    std::printf("{\"event\":\"output_drive\",\"drive_clipped\":true,\"oval_wire_exact\":true}\n");
    const auto& fixture=PolicyOutputs[4];
    auto& source=typed<lsiRecord>("Records_DolSource");
    auto& output=typed<lsoRecord>(fixture.name);
    {
        RecordLock lock(reinterpret_cast<dbCommon*>(&source));
        std::strcpy(source.val,std::string(200,'D').c_str()); source.len=201; source.udf=FALSE;
    }
    put("Records_PolicyLso.DOL",DBR_STRING,"Records_DolSource.VAL$ NPP NMS");
    epicsEnum16 mode=menuOmslsupervisory;
    put("Records_PolicyLso.OMSL",DBR_USHORT,&mode);
    prepareOutput(fixture,1); process(fixture.name,false,false,2);
    process(fixture.readback); checkOutput(fixture,1,false,true);
    mode=menuOmslclosed_loop; put("Records_PolicyLso.OMSL",DBR_USHORT,&mode);
    process(fixture.name,false,false,2); process(fixture.readback);
    check(std::string(output.val)==std::string(200,'D') && output.len==201 && !output.udf &&
          std::string(typed<lsiRecord>(fixture.readback).val)==std::string(200,'D'),
          "lso closed-loop DOL did not preserve long source VAL/LEN");
    {
        RecordLock lock(reinterpret_cast<dbCommon*>(&source));
        std::strcpy(source.val,std::string(300,'T').c_str()); source.len=301;
    }
    process(fixture.name,false,false,2); process(fixture.readback);
    check(source.len==301 && std::string(source.val)==std::string(300,'T') &&
          output.len==256 && std::string(output.val)==std::string(255,'T') &&
          typed<lsiRecord>(fixture.readback).len==256 &&
          std::string(typed<lsiRecord>(fixture.readback).val)==std::string(255,'T'),
          "lso Base pre-DSET DOL capacity boundary mismatch");
    std::printf("{\"event\":\"output_dol\",\"supervisory_ignores_dol\":true,\"closed_loop_data_bytes\":200,\"source_data_bytes\":300,\"base_prepared_data_bytes\":255,\"wire_readback_exact\":true}\n");
}
bool simulatedInput(dbCommon* rec)
{
    switch(static_cast<RecordContext*>(rec->dpvt)->definition.kind) {
    case RecordKind::Ai: return reinterpret_cast<aiRecord*>(rec)->val==85;
    case RecordKind::Longin: return reinterpret_cast<longinRecord*>(rec)->val==85;
    case RecordKind::Int64in: return reinterpret_cast<int64inRecord*>(rec)->val==85;
    case RecordKind::Stringin: return std::strcmp(reinterpret_cast<stringinRecord*>(rec)->val,"simulated")==0;
    case RecordKind::Lsi: return std::strcmp(reinterpret_cast<lsiRecord*>(rec)->val,"simulated")==0 &&
                                 reinterpret_cast<lsiRecord*>(rec)->len==10;
    case RecordKind::Waveform: return reinterpret_cast<waveformRecord*>(rec)->nord==1 &&
                                      *static_cast<epicsUInt64*>(reinterpret_cast<waveformRecord*>(rec)->bptr)==85;
    default: return false;
    }
}
void inputSourceSwitch()
{
    // Each input observes SIMM selecting SIOL while an actual native timeout terminal waits in the Base
    // queue, a return to normal mode with a further native failure, and a SIMM change while a Base SDLY
    // callback is pending, which enters completion DSET without a module generation.
    const epicsUInt16 yes=1,no=0;
    const double delay=0.01,synchronous=-1;
    const auto flnk=[]{ return typed<calcRecord>("Records_SwitchCompleted").val; };
    for(const char* kind:{"Ai","Longin","Int64in","Stringin","Lsi","Waveform"}) {
        const std::string root=std::string("Records_Switch")+kind;
        const char* name=root.c_str();
        auto* rec=record(name);
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        const bool waveform=context->definition.kind==RecordKind::Waveform;
        const auto before=Requests::instance().snapshot().completions;
        const auto links=flnk();
        {
            QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
            check(blocker.queued && blocker.entered.wait(3.0),"source switch callback blocker did not enter");
            { RecordLock lock(rec); dbProcess(rec); check(rec->pact && context->active,"source switch GET did not admit"); }
            check(until([]{return Requests::instance().snapshot().queued==1;}),"source switch terminal did not enter Base queue");
            {
                RecordLock lock(rec);
                check(context->terminal.result && context->terminal.result->outcome==ipc::Outcome::NativeFailure &&
                      context->terminal.result->nativeOutcome==2,"source switch fixture did not produce actual native timeout");
            }
            put((root+".SIMM").c_str(),DBR_USHORT,&yes);
            if(waveform) { RecordLock lock(rec); typed<waveformRecord>(name).busy=TRUE; }
            blocker.release.trigger();
            check(until([&]{RecordLock lock(rec); return !rec->pact && !context->active;}),"failing source switch lost completion");
        }
        unsigned udf=0;
        const auto generation=context->identity.generation;
        {
            RecordLock lock(rec);
            check(Requests::instance().snapshot().completions==before+1 && !context->terminal.result,
                  "failing source switch duplicated or retained terminal");
            check(!context->published && !context->nativeSuccess,"failing source switch counted native publication");
            check(rec->stat==COMM_ALARM && rec->sevr==INVALID_ALARM,"source switch hid the native communication failure");
            check(simulatedInput(rec),"source switch did not select Base SIOL input");
            if(waveform)check(!typed<waveformRecord>(name).busy,"source switch left waveform BUSY set");
            udf=rec->udf;
        }
        check(flnk()==links+1,"failing source switch lost or duplicated Base FLNK");
        check(until([&]{return context->owner->settled();}),"failing source switch retained native ownership");
        put((root+".SIMM").c_str(),DBR_USHORT,&no);
        process(name,false,false,2);
        {
            RecordLock lock(rec);
            check(rec->stat==COMM_ALARM && simulatedInput(rec) && !context->published && !context->nativeSuccess &&
                  context->identity.generation==generation+1,"normal mode failure replaced SIOL value or native state");
        }
        check(flnk()==links+2,"normal mode native failure lost or duplicated Base FLNK");
        put((root+".SDLY").c_str(),DBR_DOUBLE,&delay);
        put((root+".SIMM").c_str(),DBR_USHORT,&yes);
        const auto identity=context->identity;
        const auto delayed=Requests::instance().snapshot().completions;
        {
            QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
            check(blocker.queued && blocker.entered.wait(3.0),"delayed simulation callback blocker did not enter");
            { RecordLock lock(rec); dbProcess(rec); check(rec->pact && !context->active,"Base SDLY did not start simulation"); }
            put((root+".SIMM").c_str(),DBR_USHORT,&no);
            blocker.release.trigger();
            check(until([&]{RecordLock lock(rec); return !rec->pact;}),"Base delayed callback did not finish after mode change");
        }
        {
            RecordLock lock(rec);
            check(context->identity==identity && !context->active && Requests::instance().snapshot().completions==delayed,
                  "ownerless delayed completion admitted or consumed native work");
            check(rec->stat==READ_ALARM && rec->sevr==INVALID_ALARM,"ownerless delayed completion hid missing ownership");
            check(simulatedInput(rec) && !context->published && !context->nativeSuccess,"ownerless delayed completion published input");
        }
        check(flnk()==links+3,"ownerless delayed completion lost or duplicated Base FLNK");
        put((root+".SDLY").c_str(),DBR_DOUBLE,&synchronous);
        std::printf("{\"event\":\"input_source_switch\",\"record\":\"%s\",\"native_failure_alarm\":true,\"siol_selected\":true,\"terminal_released_once\":true,\"native_publication\":false,\"siol_udf\":%u,\"normal_failure_preserved\":true,\"ownerless_admission\":false}\n",
                    name,udf);
    }
}
unsigned delayedSets()
{
    const char* path=std::getenv("SNMP3_RECORD_DELAY_TRACE");
    check(path!=nullptr,"actual UDP trace path absent");
    std::ifstream stream(path); std::string line; unsigned count=0;
    while(std::getline(stream,line))
        if(line.find("\"event\": \"fault_request\"")!=std::string::npos &&
           line.find("\"command\": 163")!=std::string::npos)++count;
    return count;
}
void scanAndProc()
{
    const char* kinds[]={"Ao","Longout","Int64out","Stringout","Lso",
                         "Ai","Longin","Int64in","Stringin","Lsi","Waveform"};
    const epicsUInt8 trigger=1;
    for(const char* kind:kinds) {
        const std::string name=std::string("Records_")+kind;
        auto* rec=record(name.c_str());
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        check(context!=nullptr,"scan/PROC record context absent");
        put((name+".FLNK").c_str(),DBR_STRING,"Records_Completed");
        {
            RecordLock lock(rec); rec->udf=FALSE;
            switch(context->definition.kind) {
            case RecordKind::Ao: reinterpret_cast<aoRecord*>(rec)->val=1.5; break;
            case RecordKind::Longout: reinterpret_cast<longoutRecord*>(rec)->val=31; break;
            case RecordKind::Int64out: reinterpret_cast<int64outRecord*>(rec)->val=INT64_C(9007199254740993); break;
            case RecordKind::Stringout: std::strcpy(reinterpret_cast<stringoutRecord*>(rec)->val,"scan-proc"); break;
            case RecordKind::Lso: {
                auto& value=*reinterpret_cast<lsoRecord*>(rec);
                std::strcpy(value.val,"scan-proc"); value.len=10; break;
            }
            default: break;
            }
        }
        for(const char* route:{"scan","proc"}) {
            const bool periodic=std::strcmp(route,"scan")==0;
            const auto before=Requests::instance().snapshot().completions;
            const auto generation=context->identity.generation;
            const auto links=typed<calcRecord>("Records_Completed").val;
            const auto started=monotonicUs();
            unsigned activeScans=0;
            if(periodic) {
                put((name+".SCAN").c_str(),DBR_STRING,".1 second");
                for(unsigned step=1;step<=2;++step) {
                    check(until([&]{RecordLock lock(rec); return rec->pact && rec->lcnt>=2 &&
                                context->identity.generation==generation+step;}),"periodic active scan was not observed");
                    RecordLock lock(rec);
                    check(!rec->rpro && context->active,"periodic scan replaced or reprocessed the active generation");
                    activeScans+=rec->lcnt;
                    if(step==2)put((name+".SCAN").c_str(),DBR_STRING,"Passive");
                }
            } else {
                put((name+".PROC").c_str(),DBR_UCHAR,&trigger);
                RecordLock lock(rec);
                check(rec->pact && context->active && !rec->rpro,"PROC did not start a fresh asynchronous generation");
                const auto first=context->identity;
                put((name+".PROC").c_str(),DBR_UCHAR,&trigger);
                put((name+".PROC").c_str(),DBR_UCHAR,&trigger);
                check(rec->pact && rec->rpro && context->identity==first,
                      "active PROC did not coalesce behind the captured generation");
            }
            check(until([&]{RecordLock lock(rec); return !rec->pact && !rec->rpro && !context->active &&
                        context->identity.generation==generation+2;}),"scan/PROC generations did not complete");
            check(until([&]{return context->owner->settled();}),"scan/PROC retirement did not settle");
            RecordLock lock(rec);
            const auto completed=Requests::instance().snapshot().completions-before;
            const auto forwarded=typed<calcRecord>("Records_Completed").val-links;
            check(completed==2 && forwarded==2,"scan/PROC completion or FLNK count mismatch");
            check(rec->sevr==NO_ALARM && !context->terminal.result && context->identity.generation==generation+2,
                  "scan/PROC completion retained an error or terminal");
            std::printf("{\"event\":\"scan_proc\",\"record\":\"%s\",\"route\":\"%s\",\"started_us\":%llu,\"ended_us\":%llu,\"handle\":%llu,\"generation_before\":%llu,\"generation\":%llu,\"activation\":%llu,\"revision\":%llu,\"active_scans\":%u,\"completions\":%llu,\"flnk\":%.0f,\"pact\":%u,\"rpro\":%u,\"severity\":%u}\n",
                        name.c_str(),route,(unsigned long long)started,(unsigned long long)monotonicUs(),
                        (unsigned long long)context->handle,(unsigned long long)generation,
                        (unsigned long long)context->identity.generation,(unsigned long long)context->owner->activationId(),
                        (unsigned long long)context->owner->configurationRevision(),activeScans,
                        (unsigned long long)completed,forwarded,rec->pact,rec->rpro,rec->sevr);
        }
    }
}
void activeOutputs()
{
    unsigned index=0;
    for(const char* kind:{"Ao","Longout","Int64out","Stringout","Lso"}) {
        const std::string name=std::string("Records_Active")+kind;
        auto* rec=record(name.c_str());
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        const auto count=Requests::instance().snapshot().completions;
        const auto packets=delayedSets();
        Runtime::instance().takeSupervisionEvents();
        QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
        check(blocker.queued && blocker.entered.wait(3.0),"active output callback blocker did not enter");
        {
            RecordLock lock(rec); rec->udf=FALSE;
            switch(context->definition.kind) {
            case RecordKind::Ao: reinterpret_cast<aoRecord*>(rec)->val=1.5; break;
            case RecordKind::Longout: reinterpret_cast<longoutRecord*>(rec)->val=31; break;
            case RecordKind::Int64out: reinterpret_cast<int64outRecord*>(rec)->val=INT64_C(9007199254740993); break;
            case RecordKind::Stringout: std::strcpy(reinterpret_cast<stringoutRecord*>(rec)->val,"first-short"); break;
            case RecordKind::Lso: {
                auto& value=*reinterpret_cast<lsoRecord*>(rec);
                std::memset(value.val,'F',200); value.val[200]=0; value.len=201; break;
            }
            default: check(false,"unexpected active output record");
            }
            dbProcess(rec); check(rec->pact && context->identity.generation==1,"initial active SET was not admitted");
        }
        check(until([&]{return delayedSets()>packets;}),"actual delayed SET packet not observed");
        ipc::Identity first;
        {
            RecordLock lock(rec);
            check(rec->pact && context->active,"delayed SET completed before active value put");
            first=context->identity;
        }
        const double floating=2.5; const epicsInt32 integer=32;
        const epicsInt64 wide=INT64_C(9007199254740995);
        const char latest[]="latest-text";
        const std::string field=name+".VAL";
        switch(context->definition.kind) {
        case RecordKind::Ao: put(field.c_str(),DBR_DOUBLE,&floating); break;
        case RecordKind::Longout: put(field.c_str(),DBR_LONG,&integer); break;
        case RecordKind::Int64out: put(field.c_str(),DBR_INT64,&wide); break;
        case RecordKind::Stringout:
        case RecordKind::Lso: put(field.c_str(),DBR_STRING,latest); break;
        default: check(false,"unexpected active output put");
        }
        const auto changedAt=monotonicUs();
        {
            RecordLock lock(rec);
            check(rec->pact && rec->rpro && context->identity==first,"active put changed the captured generation");
            dbProcess(rec); check(context->identity==first,"active PROC admitted a replacement generation");
        }
        const char* readback=context->definition.kind==RecordKind::Ao?"Records_FloatRead":
                             context->definition.kind==RecordKind::Longout?"Records_Ai":
                             context->definition.kind==RecordKind::Int64out?"Records_Waveform":"Records_Lsi";
        {
            auto* input=record(readback); RecordLock lock(input);
            dbProcess(input); check(input->pact,"captured-payload readback did not admit");
        }
        unsigned retired=0;
        const bool nativeFinished=until([&]{
            for(const auto& event:Runtime::instance().takeSupervisionEvents())if(event.code==5) {
                ++retired;
                std::printf("{\"event\":\"active_native_retired\",\"record\":\"%s\",\"at\":%llu,\"batch\":%llu}\n",
                            name.c_str(),(unsigned long long)event.at,(unsigned long long)event.batch);
            }
            return retired>=2 && Requests::instance().snapshot().queued==2;
        });
        blocker.release.trigger();
        check(nativeFinished,"active SET/readback native retirement was not observed before callback release");
        process(readback,true,true);
        switch(context->definition.kind) {
        case RecordKind::Ao: check(typed<aiRecord>(readback).val==1.5,"active ao substituted the newer requested payload"); break;
        case RecordKind::Longout: check(typed<aiRecord>(readback).val==31,"active longout substituted the newer requested payload"); break;
        case RecordKind::Int64out: check(*static_cast<epicsUInt64*>(typed<waveformRecord>(readback).bptr)==UINT64_C(9007199254740993),"active int64out lost its captured exact payload"); break;
        case RecordKind::Stringout: check(std::string(typed<lsiRecord>(readback).val)=="first-short","active stringout substituted the newer requested payload"); break;
        case RecordKind::Lso: check(std::string(typed<lsiRecord>(readback).val)==std::string(200,'F') && typed<lsiRecord>(readback).len==201,"active lso lost its captured long payload"); break;
        default: check(false,"unexpected active output readback");
        }
        {
            RecordLock lock(rec);
            std::printf("{\"event\":\"active_output_state\",\"record\":\"%s\",\"pact\":%u,\"rpro\":%u,\"generation\":%llu,\"alarm\":%u,\"severity\":%u,\"completions\":%llu}\n",
                        name.c_str(),rec->pact,rec->rpro,(unsigned long long)context->identity.generation,
                        rec->stat,rec->sevr,(unsigned long long)Requests::instance().snapshot().completions);
        }
        check(until([&]{RecordLock lock(rec); return !rec->pact && !context->active && context->identity.generation==2;}),
              "Base RPRO did not complete the latest requested generation");
        check(Requests::instance().snapshot().completions==count+3,"active SET/RPRO did not finalize exactly twice");
        check(rec->sevr==NO_ALARM,"active RPRO output failed");
        process(readback);
        switch(context->definition.kind) {
        case RecordKind::Ao: check(typed<aiRecord>(readback).val==floating && reinterpret_cast<aoRecord*>(rec)->val==floating,"RPRO ao wire/requested value mismatch"); break;
        case RecordKind::Longout: check(typed<aiRecord>(readback).val==integer && reinterpret_cast<longoutRecord*>(rec)->val==integer,"RPRO longout wire/requested value mismatch"); break;
        case RecordKind::Int64out: check(*static_cast<epicsUInt64*>(typed<waveformRecord>(readback).bptr)==epicsUInt64(wide) && reinterpret_cast<int64outRecord*>(rec)->val==wide,"RPRO int64out wire/requested value mismatch"); break;
        case RecordKind::Stringout: check(std::string(typed<lsiRecord>(readback).val)==latest && std::string(reinterpret_cast<stringoutRecord*>(rec)->val)==latest,"RPRO stringout wire/requested value mismatch"); break;
        case RecordKind::Lso: check(std::string(typed<lsiRecord>(readback).val)==latest && std::string(reinterpret_cast<lsoRecord*>(rec)->val)==latest && reinterpret_cast<lsoRecord*>(rec)->len==sizeof(latest),"RPRO lso wire/requested LEN mismatch"); break;
        default: check(false,"unexpected RPRO readback");
        }
        DBADDR counter{}; double flnk=0; long elements=1;
        check(dbNameToAddr("Records_ActiveCompleted.VAL",&counter)==0 &&
              dbGetField(&counter,DBR_DOUBLE,&flnk,nullptr,&elements,nullptr)==0 && flnk==2*(++index),
              "Base active output FLNK count mismatch");
        std::printf("{\"event\":\"active_output\",\"record\":\"%s\",\"changed_at_us\":%llu,\"first_payload_owned\":true,\"latest_requested_preserved\":true,\"generations\":2}\n",
                    name.c_str(),(unsigned long long)changedAt);
    }
    auto& retry=typed<aoRecord>("Records_ActiveRetry");
    { RecordLock lock(reinterpret_cast<dbCommon*>(&retry)); retry.val=1.5; retry.udf=FALSE; }
    process("Records_ActiveRetry",false,false,2);
    auto* context=static_cast<RecordContext*>(retry.dpvt);
    check(context->identity.generation==1 && retry.val==1.5 && retry.stat==COMM_ALARM,"failed SET changed requested value or replayed implicitly");
    process("Records_ActiveRetry",false,false,2);
    check(context->identity.generation==2 && retry.val==1.5 && retry.stat==COMM_ALARM,"same-value explicit retry was suppressed");
    process("Records_FloatRead");
    check(typed<aiRecord>("Records_FloatRead").val==1.5,"response-lost explicit SET was not applied by the actual agent");
    std::printf("{\"event\":\"explicit_retry\",\"generations\":2}\n");
    auto& native=typed<aoRecord>("Records_ActiveNativeRetry");
    { RecordLock lock(reinterpret_cast<dbCommon*>(&native)); native.val=1.75; native.udf=FALSE; }
    process("Records_ActiveNativeRetry",false,false,2);
    check(static_cast<RecordContext*>(native.dpvt)->identity.generation==1 && native.val==1.75 && native.stat==COMM_ALARM,
          "configured native retry created another record generation");
    process("Records_FloatRead");
    check(typed<aiRecord>("Records_FloatRead").val==1.75,"response-lost native retry SET was not applied by the actual agent");
    std::printf("{\"event\":\"native_retry\",\"generations\":1}\n");
}

// Supervision timeline of one trial: Result (4), Retired (5), containment (6), reap (7), launch (2),
// Ready (3) and dispatch (13), with their monotonic times, printed for the receipt.
std::string timeline(const std::vector<SupervisionEvent>& events)
{
    std::string text="[";
    for(const auto& event:events) {
        if(text.size()>1)text+=",";
        text+="{\"code\":"+std::to_string(event.code)+",\"at\":"+std::to_string(event.at)+
              ",\"epoch\":"+std::to_string(event.epoch)+",\"batch\":"+std::to_string(event.batch)+"}";
    }
    return text+"]";
}
uint64_t firstAt(const std::vector<SupervisionEvent>& events,uint32_t code,uint64_t after=0,uint64_t epoch=0)
{
    for(const auto& event:events)
        if(event.code==code && event.at>=after && (!epoch || event.epoch==epoch))return event.at;
    return 0;
}
// Splice an external frame forwarder into an idle real worker socket. The descriptor
// number and all module/worker objects remain unchanged; only the kernel transport changes.
struct RecordIpcBoundary {
    int descriptor=-1,backup=-1,ends[2]={-1,-1},log=-1;
    pid_t child=0;
    bool spliced=false;
    std::string marker;
    bool present(const char* suffix) const { return std::ifstream(marker+suffix).good(); }
    void signal(const char* suffix) const {
        std::ofstream stream(marker+suffix); stream<<"ready\n"; stream.close();
        check(bool(stream),"record IPC marker write failed");
    }
    void closeOwned() {
        for(int* fd:{&backup,&ends[0],&ends[1],&log})if(*fd>=0) { ::close(*fd); *fd=-1; }
    }
    void start(int fd) {
        descriptor=fd;
        const char* path=std::getenv("SNMP3_RECORD_IPC_CONTROL");
        const char* helper=std::getenv("SNMP3_RECORD_IPC_HELPER");
        const char* python=std::getenv("SNMP3_RECORD_IPC_PYTHON");
        check(path && helper && python,"record IPC boundary configuration absent"); marker=path;
        int pending=-1;
        check(::ioctl(fd,FIONREAD,&pending)==0 && pending==0,"record IPC socket not idle");
        backup=::fcntl(fd,F_DUPFD_CLOEXEC,100);
        check(backup>=0 && ::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0,ends)==0,
              "record IPC socket setup failed");
        log=::open((marker+".log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        check(log>=0,"record IPC helper log open failed");
        // High source descriptors cannot collide with the child's fixed descriptors 3 and 4.
        int peer=::fcntl(ends[1],F_DUPFD_CLOEXEC,100); check(peer>=0,"record IPC peer duplication failed");
        ::close(ends[1]); ends[1]=peer;
        posix_spawn_file_actions_t actions;
        check(posix_spawn_file_actions_init(&actions)==0,"record IPC spawn actions failed");
        const int actionsStatus=posix_spawn_file_actions_adddup2(&actions,ends[1],3) |
            posix_spawn_file_actions_adddup2(&actions,backup,4) |
            posix_spawn_file_actions_adddup2(&actions,log,STDOUT_FILENO) |
            posix_spawn_file_actions_adddup2(&actions,log,STDERR_FILENO);
        if(actionsStatus) { posix_spawn_file_actions_destroy(&actions); check(false,"record IPC spawn redirects failed"); }
        const char* arguments[]={python,"-B",helper,"--mode","record-stale","--parent-fd","3",
                                 "--worker-fd","4","--control",path,nullptr};
        const int code=::posix_spawn(&child,python,&actions,nullptr,const_cast<char* const*>(arguments),environ);
        posix_spawn_file_actions_destroy(&actions);
        check(code==0,"record IPC helper spawn failed");
        check(::dup2(ends[0],fd)==fd,"record IPC descriptor splice failed"); spliced=true;
        check(::fcntl(fd,F_SETFD,FD_CLOEXEC)==0,"record IPC descriptor flags failed");
        closeOwned();
        check(until([&]{return present(".ready");}),"record IPC helper readiness absent");
    }
    bool finish() {
        if(!child) { closeOwned(); return true; }
        bool stopped=true;
        try {
            if(!marker.empty()) { std::ofstream release(marker+".release"); release<<"release\n"; }
            Runtime::instance().stop();
        } catch(...) { stopped=false; }
        int status=0; bool reaped=false,forced=false;
        const auto wait=[&] { const auto result=::waitpid(child,&status,WNOHANG); reaped=result==child; return reaped; };
        until(wait,3000000);
        if(!reaped) {
            forced=true; ::kill(child,SIGTERM); until(wait,1000000);
            if(!reaped) { ::kill(child,SIGKILL); reaped=::waitpid(child,&status,0)==child; }
        }
        const bool passed=stopped && reaped && !forced && WIFEXITED(status) && WEXITSTATUS(status)==0;
        std::printf("{\"event\":\"record_ipc_cleanup\",\"pid\":%ld,\"reaped\":%s,\"forced\":%s,\"passed\":%s}\n",
                    long(child),reaped?"true":"false",forced?"true":"false",passed?"true":"false");
        child=0; closeOwned(); return passed;
    }
    ~RecordIpcBoundary() { finish(); }
};
void staleRecordTrial()
{
    process("Records_Ai");
    auto* rec=record("Records_Ai"); auto* context=static_cast<RecordContext*>(rec->dpvt);
    auto owner=context->owner; const auto seed=context->identity;
    check(seed.generation==1 && context->nativeSuccess,"stale record seed absent");
    auto events=Runtime::instance().takeSupervisionEvents(); int descriptor=-1;
    for(const auto& event:events)if(event.code==17 && event.address==1)descriptor=int(event.detail);
    check(descriptor>=0 && owner->settled(),"stale record live socket absent");
    RecordIpcBoundary boundary; boundary.start(descriptor);
    const auto before=Requests::instance().snapshot().completions;
    const auto links=typed<calcRecord>("Records_Completed").val;
    RecordLock first(rec); dbProcess(rec); check(rec->pact,"stale record first GET not admitted");
    const auto original=context->identity; const auto single=owner->snapshot(1);
    const epicsUInt8 trigger=1; put("Records_Ai.PROC",DBR_UCHAR,&trigger);
    check(rec->rpro,"stale record PROC did not coalesce");
    check(until([&]{return boundary.present(".held") && Requests::instance().snapshot().entered==1 &&
                         context->terminal.result;}),"stale record genuine terminal/retirement absent");
    check(context->terminal.id==original && context->terminal.result->outcome==ipc::Outcome::Complete,
          "stale record first terminal mismatch");
    first.unlock();
    check(until([&]{RecordLock lock(rec); const auto q=owner->snapshot(1);
        return context->identity.generation==3 && rec->pact && q.queued==1 && q.retirementPending==1;}),
        "stale record successor not queued behind retirement");
    const auto queued=context->identity; const auto held=owner->snapshot(1);
    check(held.count==2 && held.bytes==2*single.bytes && queued.admission==original.admission+1,
          "stale record initial reservation mismatch");
    boundary.signal(".queued");
    check(until([&]{return boundary.present(".injected");}),"stale record boundary injection absent");
    bool barrier=false;
    check(until([&] {
        auto next=Runtime::instance().takeSupervisionEvents();
        for(const auto& event:next)if(event.address==1 && event.code==16)barrier=true;
        events.insert(events.end(),next.begin(),next.end());
        return barrier || owner->snapshot(1).count!=2;
    }),"stale record frame processing not observed");
    const auto after=owner->snapshot(1);
    {
        RecordLock lock(rec);
        check(barrier && after.count==held.count && after.bytes==held.bytes && after.queued==1 &&
              after.retirementPending==1 && after.behindRetirement==1 && context->identity==queued &&
              rec->pact && !context->terminal.result && Requests::instance().snapshot().completions==before+1,
              "stale generation retired the current record request");
    }
    const auto releaseAt=monotonicUs(); boundary.signal(".release");
    check(until([&]{RecordLock lock(rec); return !rec->pact && !rec->rpro && !context->active;}),
          "stale record successor did not complete");
    check(until([&]{return owner->settled();}),"stale record reservation retained");
    check(context->identity==queued && context->published && !rec->udf && rec->sevr==NO_ALARM &&
          typed<aiRecord>("Records_Ai").val==-123 && Requests::instance().snapshot().completions==before+2 &&
          typed<calcRecord>("Records_Completed").val==links+2,"stale record completion mutated or duplicated");
    auto final=Runtime::instance().takeSupervisionEvents(); events.insert(events.end(),final.begin(),final.end());
    std::printf("{\"event\":\"stale_record\",\"binding\":%llu,\"source_generation\":%llu,\"successor_generation\":%llu,"
                "\"admission\":%llu,\"held_count\":%llu,\"held_bytes\":%llu,\"release_at\":%llu,\"barrier\":%s,"
                "\"completions\":%llu,\"flnk\":%.0f,\"timeline\":%s}\n",
                (unsigned long long)original.binding,(unsigned long long)original.generation,
                (unsigned long long)queued.generation,(unsigned long long)original.admission,
                (unsigned long long)held.count,(unsigned long long)held.bytes,(unsigned long long)releaseAt,
                barrier?"true":"false",(unsigned long long)(Requests::instance().snapshot().completions-before),
                typed<calcRecord>("Records_Completed").val-links,timeline(events).c_str());
    check(boundary.finish(),"record IPC helper cleanup failed");
}
// Polls the record under its lock until the predicate holds, noting the first time the context
// carries the given generation; returns that time or 0.
template<typename Predicate> uint64_t pollGeneration(dbCommon* rec,uint64_t generation,Predicate done,uint64_t budget)
{
    auto* context=static_cast<RecordContext*>(rec->dpvt); uint64_t admittedAt=0;
    until([&]{
        RecordLock lock(rec);
        if(!admittedAt && context->identity.generation>=generation)admittedAt=monotonicUs();
        return done();
    },budget);
    return admittedAt;
}
std::string readbackText(RecordKind kind,const char* readback)
{
    char text[64];
    switch(kind) {
    case RecordKind::Ao: std::snprintf(text,sizeof(text),"%.9g",typed<aiRecord>(readback).val); return text;
    case RecordKind::Longout: std::snprintf(text,sizeof(text),"%.0f",typed<aiRecord>(readback).val); return text;
    case RecordKind::Int64out:
        std::snprintf(text,sizeof(text),"%llu",(unsigned long long)*static_cast<epicsUInt64*>(typed<waveformRecord>(readback).bptr)); return text;
    default: return typed<lsiRecord>(readback).val;
    }
}
// Each output receives a second write while its first SET is held by the outer UDP delay, with no
// hold on the Base callback consumer, so Base reprocesses the record in production order. The
// outcome, the Result-to-Retired gap of the first generation and whether the second generation was
// admitted before that retirement are printed per trial.
void activeUnforcedOutputs()
{
    for(const char* kind:{"Ao","Longout","Int64out","Stringout","Lso"}) {
        const std::string name=std::string("Records_Active")+kind;
        auto* rec=record(name.c_str());
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        const auto before=Requests::instance().snapshot().completions;
        const auto packets=delayedSets();
        Runtime::instance().takeSupervisionEvents();
        std::string first,latest;
        {
            RecordLock lock(rec); rec->udf=FALSE;
            switch(context->definition.kind) {
            case RecordKind::Ao: reinterpret_cast<aoRecord*>(rec)->val=1.5; first="1.5"; latest="2.5"; break;
            case RecordKind::Longout: reinterpret_cast<longoutRecord*>(rec)->val=31; first="31"; latest="32"; break;
            case RecordKind::Int64out: reinterpret_cast<int64outRecord*>(rec)->val=INT64_C(9007199254740993);
                first="9007199254740993"; latest="9007199254740995"; break;
            case RecordKind::Stringout: std::strcpy(reinterpret_cast<stringoutRecord*>(rec)->val,"first-short");
                first="first-short"; latest="latest-text"; break;
            case RecordKind::Lso: {
                auto& value=*reinterpret_cast<lsoRecord*>(rec);
                std::memset(value.val,'F',200); value.val[200]=0; value.len=201; first=std::string(200,'F'); latest="latest-text"; break;
            }
            default: check(false,"unexpected unforced output record");
            }
            dbProcess(rec); check(rec->pact && context->identity.generation==1,"unforced first SET was not admitted");
        }
        check(until([&]{return delayedSets()>packets;}),"unforced delayed SET packet not observed");
        const std::string field=name+".VAL";
        const double floating=2.5; const epicsInt32 integer=32; const epicsInt64 wide=INT64_C(9007199254740995);
        switch(context->definition.kind) {
        case RecordKind::Ao: put(field.c_str(),DBR_DOUBLE,&floating); break;
        case RecordKind::Longout: put(field.c_str(),DBR_LONG,&integer); break;
        case RecordKind::Int64out: put(field.c_str(),DBR_INT64,&wide); break;
        default: put(field.c_str(),DBR_STRING,"latest-text"); break;
        }
        const auto admittedAt=pollGeneration(rec,2,[&]{
            return Requests::instance().snapshot().completions>=before+1 && !rec->pact && !rec->rpro &&
                   ((context->identity.generation>=2 && Requests::instance().snapshot().completions>=before+2) ||
                    rec->stat==WRITE_ALARM);
        },8000000);
        check(until([&]{return context->owner->settled();}),"unforced trial retained native ownership");
        const auto events=Runtime::instance().takeSupervisionEvents();
        const auto resultAt=firstAt(events,4),retiredAt=firstAt(events,5,resultAt);
        const auto delta=Requests::instance().snapshot().completions-before;
        const char* readback=context->definition.kind==RecordKind::Ao?"Records_FloatRead":
                             context->definition.kind==RecordKind::Longout?"Records_Ai":
                             context->definition.kind==RecordKind::Int64out?"Records_Waveform":"Records_Lsi";
        process(readback);
        RecordLock lock(rec);
        std::printf("{\"event\":\"active_unforced\",\"record\":\"%s\",\"generation\":%llu,\"completions_delta\":%llu,"
                    "\"stat\":\"%s\",\"sevr\":\"%s\",\"pact\":%u,\"wire\":\"%s\",\"first\":\"%s\",\"latest\":\"%s\","
                    "\"result_us\":%llu,\"retired_us\":%llu,\"admitted_us\":%llu,\"branch_exercised\":%s,\"timeline\":%s}\n",
                    name.c_str(),(unsigned long long)context->identity.generation,(unsigned long long)delta,
                    epicsAlarmConditionStrings[rec->stat],epicsAlarmSeverityStrings[rec->sevr],rec->pact,readbackText(context->definition.kind,readback).c_str(),first.c_str(),latest.c_str(),
                    (unsigned long long)resultAt,(unsigned long long)retiredAt,(unsigned long long)admittedAt,
                    admittedAt && retiredAt && admittedAt<retiredAt?"true":"false",timeline(events).c_str());
    }
}
void queuedSimmTrial()
{
    const char* kind=std::getenv("SNMP3_SIMM_KIND");
    const char* mode=std::getenv("SNMP3_SIMM_MODE");
    check(kind && mode,"queued SIMM selection absent");
    const bool deadline=std::strcmp(mode,"deadline")==0;
    check(deadline || std::strcmp(mode,"normal")==0,"unknown queued SIMM mode");
    const std::string name=std::string("Records_Active")+kind;
    auto* rec=record(name.c_str()); auto* context=static_cast<RecordContext*>(rec->dpvt);
    const char* readback=context->definition.kind==RecordKind::Ao?"Records_FloatRead":
                         context->definition.kind==RecordKind::Longout?"Records_Ai":
                         context->definition.kind==RecordKind::Int64out?"Records_Waveform":"Records_Lsi";
    OutputFixture fixture{context->definition.kind,name.c_str(),nullptr,readback};
    prepareOutput(fixture,1);
    DBADDR simmAddress{};
    check(dbNameToAddr((name+".SIMM").c_str(),&simmAddress)==0,"queued SIMM address absent");
    const auto before=Requests::instance().snapshot().completions;
    const auto packets=delayedSets();
    const auto links=typed<calcRecord>("Records_ActiveCompleted").val;
    AlarmMonitor monitor(name);
    std::vector<SupervisionEvent> events=Runtime::instance().takeSupervisionEvents();
    struct Resume {
        pid_t pid=0;
        ~Resume() { if(pid)::kill(pid,SIGCONT); }
    } paused;
    struct Release {
        std::string marker;
        void release() { if(!marker.empty()) { std::ofstream stream(marker+".release"); stream<<"release\n"; } }
        ~Release() { release(); }
    } boundary;
    if(!deadline) {
        const char* marker=std::getenv("SNMP3_RETIRED_HOLD"); check(marker!=nullptr,"Retired hold marker absent");
        boundary.marker=marker;
        pid_t worker=0;
        for(const auto& e:events)if(e.code==2 && e.pid>0)worker=pid_t(e.pid);
        check(worker>0,"Retired hold worker identity absent");
        { std::ofstream stream(boundary.marker+".pid.next"); stream<<worker<<"\n"; stream.close(); check(bool(stream),"worker marker write failed"); }
        check(std::rename((boundary.marker+".pid.next").c_str(),(boundary.marker+".pid").c_str())==0,
              "worker marker rename failed");
        check(until([&]{return std::ifstream(boundary.marker+".attached").good();}),"Retired helper did not attach");
    }
    RecordLock admissionLock(rec);
    dbProcess(rec); check(rec->pact,"queued SIMM first SET did not admit");
    check(until([&]{return delayedSets()==packets+1;}),"queued SIMM first SET not observed");
    check(until([&]{for(const auto& e:Runtime::instance().takeSupervisionEvents()) {
        events.push_back(e); if(deadline && e.code==13 && e.pid>0)paused.pid=pid_t(e.pid);
    } return !deadline || paused.pid>0;}),"queued SIMM worker identity absent");
    if(deadline)check(::kill(paused.pid,SIGSTOP)==0,"queued SIMM worker stop failed");
    const double floating=outputDouble(2); const epicsInt32 integer=outputInteger(2);
    const epicsInt64 wide=outputWide(2); const char* text="latest-short";
    const std::string field=name+".VAL";
    switch(context->definition.kind) {
    case RecordKind::Ao: put(field.c_str(),DBR_DOUBLE,&floating); break;
    case RecordKind::Longout: put(field.c_str(),DBR_LONG,&integer); break;
    case RecordKind::Int64out: put(field.c_str(),DBR_INT64,&wide); break;
    default: put(field.c_str(),DBR_STRING,text); break;
    }
    if(!deadline) {
        check(until([&]{return Requests::instance().snapshot().entered==1 && context->terminal.result;}),
              "queued SIMM first terminal did not enter");
        check(context->terminal.result->outcome==ipc::Outcome::Complete && context->terminal.sent,
              "queued SIMM first terminal was not successful native work");
        check(until([&]{return std::ifstream(boundary.marker+".held").good();}),"actual Retired send was not held");
    }
    const auto pausedAt=monotonicUs();
    admissionLock.unlock();
    QueueSnapshot held;
    uint64_t switchedAt=0; unsigned terminal=0; bool sent=false;
    check(until([&]{
        RecordLock lock(rec);
        const auto q=context->owner->snapshot(1);
        if(context->identity.generation!=2 || !rec->pact || q.queued!=1 || q.retirementPending!=1)return false;
        check(q.count==2 && q.bytes>0 && !context->terminal.result,"queued SIMM ownership precondition differs");
        held=q;
        const epicsUInt16 yes=1;
        check(dbPutField(&simmAddress,DBR_USHORT,&yes,1)==0,"queued SIMM put failed");
        switchedAt=monotonicUs();
        if(!deadline)boundary.release();
        check(until([&]{return Requests::instance().snapshot().entered==1 && context->terminal.result;}),
              "queued SIMM successor terminal did not enter");
        terminal=unsigned(context->terminal.result->outcome); sent=context->terminal.sent;
        check(context->terminal.id==context->identity && context->identity.generation==2 &&
              Requests::instance().snapshot().completions==before+1,"queued SIMM successor ownership changed");
        return true;
    }),"queued SIMM retirement window was not exercised");
    check(until([&]{RecordLock lock(rec); return !rec->pact && !context->active &&
                         Requests::instance().snapshot().completions==before+2;}),"queued SIMM completion lost or duplicated");
    if(paused.pid) { check(::kill(paused.pid,SIGCONT)==0,"queued SIMM worker resume failed"); paused.pid=0; }
    check(until([&]{return context->owner->settled();}),"queued SIMM native retirement did not settle");
    for(const auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
    const auto retiredAt=firstAt(events,5);
    if(deadline)check(until([&]{return monitor.emptyComm>0 && monitor.neverSent>0;}),"queued SIMM repeated COMM alarm message event missing");
    {
        RecordLock lock(rec);
        check(!rec->pact && !rec->rpro && !context->terminal.result && context->identity.generation==2,
              "queued SIMM record did not release ownership");
        check(typed<calcRecord>("Records_ActiveCompleted").val==links+2,"queued SIMM FLNK count mismatch");
        check(terminal==unsigned(deadline?ipc::Outcome::Deadline:ipc::Outcome::Complete) && sent!=deadline,
              "queued SIMM terminal/sent classification differs");
        check(rec->stat==(deadline?COMM_ALARM:NO_ALARM) && rec->sevr==(deadline?INVALID_ALARM:NO_ALARM) && !rec->udf,
              "queued SIMM completion alarm/UDF mismatch");
        check(std::strcmp(rec->amsg,deadline?"deadline before send":"")==0,"queued SIMM alarm message mismatch");
        if(!deadline)check(switchedAt<retiredAt,"SIMM changed only after predecessor retired");
        if(context->definition.kind==RecordKind::Lso) {
            const auto& value=*reinterpret_cast<lsoRecord*>(rec);
            check(std::string(value.val)==text && value.len==std::strlen(text)+1,"queued SIMM changed requested long text");
        } else checkOutput(fixture,2);
    }
    const auto sourcePackets=delayedSets()-packets;
    check(sourcePackets==(deadline?1u:2u),"queued SIMM source wire count mismatch");
    const epicsUInt16 no=0; put((name+".SIMM").c_str(),DBR_USHORT,&no);
    process(readback);
    if(!deadline && context->definition.kind==RecordKind::Lso) {
        const auto& value=typed<lsiRecord>(readback);
        check(std::string(value.val)==text && value.len==std::strlen(text)+1,"queued SIMM long SET was suppressed or altered");
    } else checkOutput(fixture,deadline?1:2,false,true);
    check(context->identity.generation==2 && Requests::instance().snapshot().completions==before+3,
          "queued SIMM return to normal replayed SET");
    std::printf("{\"event\":\"queued_simm\",\"record\":\"%s\",\"mode\":\"%s\",\"paused_at\":%llu,\"switched_at\":%llu,\"retired_at\":%llu,\"queued\":%llu,\"retirement_pending\":%llu,\"count\":%llu,\"bytes\":%llu,\"terminal\":%u,\"sent\":%s,\"source_packets\":%u,\"source_generations\":2,\"source_completions\":2,\"flnk\":2,\"empty_comm_events\":%u,\"never_sent_events\":%u,\"readback\":\"%s\",\"timeline\":%s}\n",
                name.c_str(),mode,(unsigned long long)pausedAt,(unsigned long long)switchedAt,(unsigned long long)retiredAt,
                (unsigned long long)held.queued,(unsigned long long)held.retirementPending,
                (unsigned long long)held.count,(unsigned long long)held.bytes,terminal,sent?"true":"false",sourcePackets,
                monitor.emptyComm.load(),monitor.neverSent.load(),readbackText(context->definition.kind,readback).c_str(),timeline(events).c_str());
}
void retirementTrial()
{
    const char* kind=std::getenv("SNMP3_RETIREMENT_KIND");
    const char* mode=std::getenv("SNMP3_RETIREMENT_MODE");
    check(kind && mode,"retirement trial selection absent");
    const bool deadline=std::strcmp(mode,"deadline")==0;
    check(deadline || std::strcmp(mode,"normal")==0,"retirement mode invalid");
    responseMode("pass"); outputs();
    const std::string name=std::string("Records_")+kind;
    auto* rec=record(name.c_str()); auto* context=static_cast<RecordContext*>(rec->dpvt);
    process(name.c_str());
    std::vector<unsigned char> image; unsigned length=0;
    { RecordLock lock(rec); image=inputImage(rec);
      length=context->definition.kind==RecordKind::Lsi?reinterpret_cast<lsiRecord*>(rec)->len:
             context->definition.kind==RecordKind::Waveform?reinterpret_cast<waveformRecord*>(rec)->nord:0;
      check(context->nativeSuccess && !rec->udf,"retirement seed GET not valid"); }
    const auto before=Requests::instance().snapshot().completions;
    const auto initial=context->identity; const auto links=typed<calcRecord>("Records_Completed").val;
    auto owner=context->owner;
    AlarmMonitor monitor(name);
    std::vector<SupervisionEvent> events=Runtime::instance().takeSupervisionEvents();
    pid_t worker=0; for(const auto& e:events)if(e.address==1 && e.code==2 && e.pid>0)worker=pid_t(e.pid);
    check(worker>0,"retirement actual worker absent");
    struct Release {
        pid_t paused=0; std::string marker;
        void release() {
            if(paused) { const auto pid=paused; paused=0; check(::kill(pid,SIGCONT)==0,"retirement resume failed"); }
            if(!marker.empty()) { std::ofstream stream(marker+".release"); stream<<"release\n"; }
        }
        ~Release() { if(paused)::kill(paused,SIGCONT); if(!marker.empty()) { std::ofstream stream(marker+".release"); stream<<"release\n"; } }
    } boundary;
    if(!deadline) {
        const char* marker=std::getenv("SNMP3_RETIRED_HOLD"); check(marker!=nullptr,"retirement hold marker absent");
        boundary.marker=marker;
        { std::ofstream stream(boundary.marker+".pid.next"); stream<<worker<<"\n"; stream.close(); check(bool(stream),"retirement PID write failed"); }
        check(std::rename((boundary.marker+".pid.next").c_str(),(boundary.marker+".pid").c_str())==0,"retirement PID rename failed");
        check(until([&]{return std::ifstream(boundary.marker+".attached").good();}),"retirement helper attachment absent");
    }
    const auto packets=[] {
        const char* path=std::getenv("SNMP3_RECORD_DELAY_TRACE"); check(path!=nullptr,"retirement wire trace absent");
        std::ifstream stream(path); std::string line; unsigned count=0;
        while(std::getline(stream,line))if(line.find("\"event\": \"fault_request\"")!=std::string::npos)++count;
        return count;
    };
    const auto wireBefore=packets();
    responseMode(deadline?"drop-all":"pass");
    const auto startedAt=monotonicUs();
    RecordLock first(rec); dbProcess(rec);
    check(rec->pact && context->identity.generation==initial.generation+1,"retirement first GET not admitted");
    const auto single=owner->snapshot(1);
    check(single.count==1 && single.bytes>0,"retirement initial accounting mismatch");
    check(until([&]{return packets()==wireBefore+1;}),"retirement first real GET absent");
    if(deadline) { check(::kill(worker,SIGSTOP)==0,"retirement worker hold failed"); boundary.paused=worker; }
    const epicsUInt8 trigger=1; put((name+".PROC").c_str(),DBR_UCHAR,&trigger);
    check(rec->rpro && context->identity.generation==initial.generation+1,"retirement active PROC did not coalesce");
    check(until([&]{return Requests::instance().snapshot().entered==1 && context->terminal.result;}),"retirement first terminal absent");
    check(context->terminal.result->outcome==(deadline?ipc::Outcome::Deadline:ipc::Outcome::Complete) && context->terminal.sent,
          "retirement first terminal mismatch");
    if(!deadline)check(until([&]{return std::ifstream(boundary.marker+".held").good();}),"retirement genuine Retired hold absent");
    first.unlock();
    QueueSnapshot held;
    check(until([&]{ RecordLock lock(rec); const auto q=owner->snapshot(1);
        if(context->identity.generation!=initial.generation+2 || !rec->pact || q.queued!=1 || q.retirementPending!=1)return false;
        held=q; return true;
    }),"retirement successor did not queue");
    const auto queuedAt=monotonicUs();
    check(held.count==2 && held.bytes==2*single.bytes && held.behindRetirement==1,"retirement two-generation accounting mismatch");
    auto* progress=record("Records_Progress"); auto* progressContext=static_cast<RecordContext*>(progress->dpvt);
    { RecordLock lock(progress); dbProcess(progress); check(progress->pact,"independent address GET not admitted"); }
    check(until([&]{ RecordLock lock(progress); return !progress->pact && !progressContext->active; }),"independent address did not complete");
    const auto progressAt=monotonicUs();
    { RecordLock lock(progress); check(progress->sevr==NO_ALARM && !progress->udf &&
        reinterpret_cast<aiRecord*>(progress)->val==-123 && progressContext->published && progressContext->identity.generation==1,
        "independent address GET value mismatch"); }
    const auto during=owner->snapshot(1);
    check(during.count==2 && during.bytes==held.bytes && during.queued==1 && during.retirementPending==1,
          "blocked address progressed before boundary release");
    check(packets()==wireBefore+1,"successor dispatched before predecessor retirement");
    unsigned terminal=0; bool sent=false; uint64_t releaseAt=0; QueueSnapshot consumed;
    {
        RecordLock lock(rec);
        if(!deadline) { releaseAt=monotonicUs(); boundary.release(); }
        check(until([&]{return Requests::instance().snapshot().entered==1 && context->terminal.result;}),"retirement successor terminal absent");
        terminal=unsigned(context->terminal.result->outcome); sent=context->terminal.sent;
        check(context->terminal.id==context->identity && context->identity.generation==initial.generation+2,
              "retirement successor identity mismatch");
    }
    check(until([&]{RecordLock lock(rec); return !rec->pact && !context->active &&
                         Requests::instance().snapshot().completions==before+3;}),"retirement completions missing or duplicated");
    if(deadline) {
        check(until([&]{return monitor.emptyComm>0 && monitor.neverSent>0;}),"input queued deadline message event missing");
        consumed=owner->snapshot(1);
        check(consumed.count==1 && consumed.bytes==single.bytes && consumed.retirementPending==1 && !consumed.queued,
              "retirement consumed predecessor charge released early");
        releaseAt=monotonicUs(); responseMode("pass"); boundary.release();
    }
    check(until([&]{ for(const auto& e:Runtime::instance().takeSupervisionEvents())if(e.address==1)events.push_back(e);
        return owner->settled() && (!deadline || firstAt(events,3,releaseAt,2)); }),"retirement/relaunch did not settle");
    {
        RecordLock lock(rec);
        check(terminal==unsigned(deadline?ipc::Outcome::Deadline:ipc::Outcome::Complete) && sent!=deadline,
              "retirement successor outcome mismatch");
        check(inputImage(rec)==image && !rec->udf && context->nativeSuccess && context->published!=deadline,
              "retirement changed valid input or publication state");
        check(rec->stat==(deadline?COMM_ALARM:NO_ALARM) && rec->sevr==(deadline?INVALID_ALARM:NO_ALARM) &&
              std::strcmp(rec->amsg,deadline?"deadline before send":"")==0,"retirement input alarm mismatch");
        if(context->definition.kind==RecordKind::Lsi)check(reinterpret_cast<lsiRecord*>(rec)->len==length,"retirement changed LEN");
        if(context->definition.kind==RecordKind::Waveform)check(reinterpret_cast<waveformRecord*>(rec)->nord==length &&
              !reinterpret_cast<waveformRecord*>(rec)->busy,"retirement changed NORD/BUSY");
        check(!rec->rpro && !context->terminal.result && typed<calcRecord>("Records_Completed").val==links+2,
              "retirement source ownership or FLNK mismatch");
    }
    const auto sourcePackets=packets()-wireBefore;
    check(sourcePackets==(deadline?1u:2u),"retirement native source request count mismatch");
    process(name.c_str());
    { RecordLock lock(rec); check(context->identity.generation==initial.generation+3 && inputImage(rec)==image &&
          !rec->amsg[0] && context->published && !rec->udf && typed<calcRecord>("Records_Completed").val==links+3,
          "retirement explicit retry did not recover"); }
    check(Requests::instance().snapshot().completions==before+4 && packets()==wireBefore+sourcePackets+1,
          "retirement retry replayed or lost native work");
    for(const auto& e:Runtime::instance().takeSupervisionEvents())if(e.address==1)events.push_back(e);
    const auto resultAt=firstAt(events,4,startedAt),retiredAt=firstAt(events,5,startedAt),reapAt=firstAt(events,7,startedAt);
    if(!deadline)check(resultAt<queuedAt && progressAt<releaseAt && releaseAt<retiredAt,"retirement normal boundary order mismatch");
    else check(progressAt<releaseAt && releaseAt<reapAt,"retirement deadline boundary order mismatch");
    std::printf("{\"event\":\"input_retirement\",\"record\":\"%s\",\"mode\":\"%s\",\"started_at\":%llu,\"queued_at\":%llu,\"progress_at\":%llu,\"released_at\":%llu,\"retired_at\":%llu,\"reap_at\":%llu,\"single_bytes\":%llu,\"held_count\":%llu,\"held_bytes\":%llu,\"consumed_count\":%llu,\"consumed_bytes\":%llu,\"terminal\":%u,\"sent\":%s,\"source_packets\":%u,\"source_generations\":3,\"source_completions\":3,\"flnk\":3,\"empty_comm_events\":%u,\"never_sent_events\":%u,\"retry_recovered\":true,\"timeline\":%s}\n",
        name.c_str(),mode,(unsigned long long)startedAt,(unsigned long long)queuedAt,(unsigned long long)progressAt,
        (unsigned long long)releaseAt,(unsigned long long)retiredAt,(unsigned long long)reapAt,(unsigned long long)single.bytes,
        (unsigned long long)held.count,(unsigned long long)held.bytes,(unsigned long long)consumed.count,(unsigned long long)consumed.bytes,
        terminal,sent?"true":"false",sourcePackets,monitor.emptyComm.load(),monitor.neverSent.load(),timeline(events).c_str());
}
// One deadline trial on the outer-UDP drop-all path: the first SET expires at the record budget
// while a second write waits for Base reprocessing. Prints the Deadline-to-Ready interval of the
// relaunched worker, the second generation's admission time and whether it was dispatched.
void deadlineTrial(const char* label)
{
    auto* rec=record("Records_QueueAo");
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    check(context!=nullptr,"deadline-queue record has no binding");
    const auto before=Requests::instance().snapshot().completions;
    const auto packets=delayedSets();
    Runtime::instance().takeSupervisionEvents();
    const auto firstAdmittedAt=monotonicUs();
    { RecordLock lock(rec); rec->udf=FALSE; reinterpret_cast<aoRecord*>(rec)->val=1.5; dbProcess(rec);
      check(rec->pact,"deadline-queue first SET was not admitted"); }
    check(until([&]{return delayedSets()>packets;}),"deadline-queue SET packet not observed");
    const double latest=2.5; const auto putAt=monotonicUs(); put("Records_QueueAo.VAL",DBR_DOUBLE,&latest);
    const auto admittedAt=pollGeneration(rec,2,[&]{
        return Requests::instance().snapshot().completions>=before+1 && !rec->pact && !rec->rpro &&
               ((context->identity.generation>=2 && Requests::instance().snapshot().completions>=before+2) ||
                rec->stat==WRITE_ALARM);
    },30000000);
    check(until([&]{return context->owner->settled();},30000000),"deadline-queue trial retained native ownership");
    std::vector<SupervisionEvent> events;
    until([&]{ for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
               return firstAt(events,3,firstAt(events,6))!=0; },10000000);
    const auto deadlineAt=firstAt(events,6),readyAt=firstAt(events,3,deadlineAt);
    const auto secondEpoch=[&]{ for(const auto& e:events)if(e.code==3 && e.at>=deadlineAt)return e.epoch; return uint64_t(0); }();
    const auto dispatchedAt=secondEpoch?firstAt(events,13,readyAt,secondEpoch):0;
    unsigned long long generation=0,completions=0; std::string stat,sevr,amsg;
    { RecordLock lock(rec); generation=context->identity.generation; completions=Requests::instance().snapshot().completions-before;
      stat=epicsAlarmConditionStrings[rec->stat]; sevr=epicsAlarmSeverityStrings[rec->sevr]; amsg=rec->amsg; }
    // After a never-sent generation, one more SET on the Ready worker must not inherit its message.
    unsigned long long followGeneration=0; std::string followStat,followAmsg;
    if(std::strcmp(label,"below")==0) {
        const auto base=Requests::instance().snapshot().completions;
        { RecordLock lock(rec); reinterpret_cast<aoRecord*>(rec)->val=3.5; dbProcess(rec);
          check(rec->pact,"deadline-queue follow-up SET was not admitted"); }
        check(until([&]{ RecordLock lock(rec); return !rec->pact && Requests::instance().snapshot().completions>=base+1; },30000000),
              "deadline-queue follow-up did not complete");
        check(until([&]{return context->owner->settled();},30000000),"deadline-queue follow-up retained native ownership");
        RecordLock lock(rec); followGeneration=context->identity.generation;
        followStat=epicsAlarmConditionStrings[rec->stat]; followAmsg=rec->amsg;
    }
    std::printf("{\"event\":\"deadline_queue\",\"trial\":\"%s\",\"generation\":%llu,\"completions_delta\":%llu,"
                "\"stat\":\"%s\",\"sevr\":\"%s\",\"amsg\":\"%s\",\"followup_generation\":%llu,\"followup_stat\":\"%s\","
                "\"followup_amsg\":\"%s\",\"deadline_us\":%llu,\"ready_us\":%llu,\"admitted_us\":%llu,"
                "\"deadline_to_ready_us\":%llu,\"admission_to_ready_us\":%llu,\"second_dispatched\":%s,"
                "\"budget_ms\":%u,\"first_admitted_us\":%llu,\"put_at_us\":%llu,\"dispatched_us\":%llu,\"put_to_dispatch_us\":%llu,"
                "\"timeline\":%s}\n",
                label,generation,completions,stat.c_str(),sevr.c_str(),amsg.c_str(),followGeneration,followStat.c_str(),followAmsg.c_str(),
                (unsigned long long)deadlineAt,(unsigned long long)readyAt,(unsigned long long)admittedAt,
                (unsigned long long)(readyAt>deadlineAt?readyAt-deadlineAt:0),
                (unsigned long long)(admittedAt && readyAt>admittedAt?readyAt-admittedAt:0),
                dispatchedAt && admittedAt && dispatchedAt>admittedAt?"true":"false",
                context->definition.budgetMs,(unsigned long long)firstAdmittedAt,(unsigned long long)putAt,(unsigned long long)dispatchedAt,
                (unsigned long long)(dispatchedAt>putAt?dispatchedAt-putAt:0),timeline(events).c_str());
}
// One near-deadline trial: the outer UDP delay places the response just before the record budget,
// so the result is selected inside the last service tick; prints whether the worker was contained
// after its result had already been selected.
void nearDeadlineTrial()
{
    auto* rec=record("Records_QueueAo");
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    check(context!=nullptr,"near-deadline record has no binding");
    Runtime::instance().takeSupervisionEvents();
    uint64_t admittedAt=0;
    { RecordLock lock(rec); rec->udf=FALSE; reinterpret_cast<aoRecord*>(rec)->val=1.5;
      admittedAt=monotonicUs(); dbProcess(rec); check(rec->pact,"near-deadline SET was not admitted"); }
    const auto deadlineAt=ipc::add(admittedAt,ipc::multiply(context->definition.budgetMs,1000));
    check(until([&]{RecordLock lock(rec); return !rec->pact;},30000000),"near-deadline record did not complete");
    check(until([&]{return context->owner->settled();},30000000),"near-deadline trial retained native ownership");
    std::vector<SupervisionEvent> events;
    until([&]{ for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e); return firstAt(events,5)!=0 || firstAt(events,7)!=0; },3000000);
    for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
    // Counted only when the result was accepted in an earlier service step than the containment
    // and the record completed successfully, i.e. the request had already succeeded.
    const auto resultAt=firstAt(events,4),containedAt=resultAt?firstAt(events,6,resultAt+1):0;
    const auto retiredAt=resultAt?firstAt(events,5,resultAt):0;
    RecordLock lock(rec);
    const bool afterSuccess=containedAt && rec->sevr==NO_ALARM;
    // The grace window was exercised when the result arrived before the deadline but its Retired
    // frame (or the containment) came at or after it.
    const auto settledAt=containedAt && (!retiredAt || containedAt<retiredAt)?containedAt:retiredAt;
    const bool graceExercised=resultAt && resultAt<deadlineAt && settledAt>=deadlineAt && rec->sevr==NO_ALARM;
    std::printf("{\"event\":\"near_deadline\",\"stat\":\"%s\",\"sevr\":\"%s\",\"result_us\":%llu,\"contained_us\":%llu,"
                "\"deadline_us\":%llu,\"retired_us\":%llu,\"grace_exercised\":%s,"
                "\"contained_after_result\":%s,\"timeline\":%s}\n",epicsAlarmConditionStrings[rec->stat],epicsAlarmSeverityStrings[rec->sevr],
                (unsigned long long)resultAt,(unsigned long long)containedAt,(unsigned long long)deadlineAt,
                (unsigned long long)retiredAt,graceExercised?"true":"false",afterSuccess?"true":"false",timeline(events).c_str());
}

// Settles a record after a write while active: the first generation completed and Base's reprocess
// either admitted a second generation and completed it, or was rejected synchronously.
bool settledAfterReprocess(dbCommon* rec,uint64_t before)
{
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    return Requests::instance().snapshot().completions>=before+1 && !rec->pact && !rec->rpro &&
           ((context->identity.generation>=2 && Requests::instance().snapshot().completions>=before+2) ||
            rec->sevr==INVALID_ALARM);
}
// Two-generation reservation accounting: while the second generation waits behind the first one's
// retirement the address holds both charges; with a count limit of one, Base's reprocess is rejected
// synchronously and a later explicit request is admitted once the limit is raised.
void accountingTrials()
{
    constexpr unsigned Trials=3;
    auto* rec=record("Records_ActiveAo");
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    auto scheduler=Runtime::instance().schedulerOwner();
    for(unsigned trial=1;trial<=Trials;++trial) {
        const auto before=Requests::instance().snapshot().completions,base=context->identity.generation;
        const auto packets=delayedSets();
        { RecordLock lock(rec); rec->udf=FALSE; reinterpret_cast<aoRecord*>(rec)->val=1.5; dbProcess(rec);
          check(rec->pact,"accounting first SET was not admitted"); }
        check(until([&]{return delayedSets()>packets;}),"accounting SET packet not observed");
        const auto single=scheduler->snapshot(1);
        const double latest=2.5; put("Records_ActiveAo.VAL",DBR_DOUBLE,&latest);
        QueueSnapshot peak; bool window=false;
        until([&]{
            const auto q=scheduler->snapshot(1);
            if(!window && q.count==2 && q.retirementPending==1 && q.queued==1) { peak=q; window=true; }
            RecordLock lock(rec); return settledAfterReprocess(rec,before);
        },8000000);
        check(until([&]{return context->owner->settled();}),"accounting trial retained native ownership");
        RecordLock lock(rec);
        std::printf("{\"event\":\"accounting_window\",\"trial\":%u,\"observed\":%s,\"single_count\":%llu,\"single_bytes\":%llu,"
                    "\"peak_count\":%llu,\"peak_bytes\":%llu,\"retirement_pending\":%llu,\"queued\":%llu,\"generations\":%llu,"
                    "\"stat\":\"%s\",\"sevr\":\"%s\"}\n",trial,window?"true":"false",
                    (unsigned long long)single.count,(unsigned long long)single.bytes,(unsigned long long)peak.count,
                    (unsigned long long)peak.bytes,(unsigned long long)peak.retirementPending,(unsigned long long)peak.queued,
                    (unsigned long long)(context->identity.generation-base),
                    epicsAlarmConditionStrings[rec->stat],epicsAlarmSeverityStrings[rec->sevr]);
    }
    Runtime::instance().queueLimit("127.0.0.1",1,ipc::DefaultBytes);
    const auto before=Requests::instance().snapshot().completions,base=context->identity.generation;
    const auto packets=delayedSets();
    { RecordLock lock(rec); rec->udf=FALSE; reinterpret_cast<aoRecord*>(rec)->val=1.5; dbProcess(rec);
      check(rec->pact,"limited first SET was not admitted"); }
    check(until([&]{return delayedSets()>packets;}),"limited SET packet not observed");
    const double latest=2.5; put("Records_ActiveAo.VAL",DBR_DOUBLE,&latest);
    check(until([&]{RecordLock lock(rec); return settledAfterReprocess(rec,before);}),"limited reprocess did not settle");
    std::string limited;
    { RecordLock lock(rec);
      limited=std::string("\"limited_generations\":")+std::to_string(context->identity.generation-base)+
              ",\"limited_stat\":\""+epicsAlarmConditionStrings[rec->stat]+"\",\"limited_sevr\":\""+
              epicsAlarmSeverityStrings[rec->sevr]+"\",\"limited_pact\":"+std::to_string(rec->pact); }
    Runtime::instance().queueLimit("127.0.0.1",ipc::DefaultCount,ipc::DefaultBytes);
    check(until([&]{return context->owner->settled();}),"limited trial retained native ownership");
    const auto explicitBefore=Requests::instance().snapshot().completions;
    { RecordLock lock(rec); dbProcess(rec); check(rec->pact,"explicit request after raised limit was not admitted"); }
    check(until([&]{RecordLock lock(rec); return !rec->pact && Requests::instance().snapshot().completions>=explicitBefore+1;}),
          "explicit request did not complete");
    check(until([&]{return context->owner->settled();}),"explicit request retained native ownership");
    RecordLock lock(rec);
    std::printf("{\"event\":\"accounting_limit\",%s,\"explicit_sevr\":\"%s\",\"explicit_completions\":%llu}\n",limited.c_str(),
                epicsAlarmSeverityStrings[rec->sevr],(unsigned long long)(Requests::instance().snapshot().completions-explicitBefore));
}
// Stop while a first generation is Deadline-selected, consumed and unreaped (its worker stopped at the
// worker-signal boundary) and a second generation waits behind that retirement: the successor must
// complete Stopping through Base with FLNK once and drain must succeed before the predecessor is reaped.
void stopWithQueuedSuccessor()
{
    auto* rec=record("Records_QueueAo");
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    auto scheduler=Runtime::instance().schedulerOwner();
    const auto before=Requests::instance().snapshot().completions;
    const auto packets=delayedSets();
    std::vector<SupervisionEvent> events;
    Runtime::instance().takeSupervisionEvents();
    { RecordLock lock(rec); rec->udf=FALSE; reinterpret_cast<aoRecord*>(rec)->val=1.5; dbProcess(rec);
      check(rec->pact,"stop trial first SET was not admitted"); }
    check(until([&]{return delayedSets()>packets;}),"stop trial SET packet not observed");
    int64_t worker=0;
    check(until([&]{ for(auto& e:Runtime::instance().takeSupervisionEvents()) { events.push_back(e); if(e.code==13 && e.pid>0)worker=e.pid; }
                     return worker>0; }),"stop trial worker identity not observed");
    firstActivationWorker=worker;
    check(::kill(pid_t(worker),SIGSTOP)==0,"stop trial worker could not be stopped");
    const double latest=2.5; put("Records_QueueAo.VAL",DBR_DOUBLE,&latest);
    const auto links=typed<calcRecord>("Records_QueueCompleted").val;
    check(until([&]{return Requests::instance().snapshot().completions>=before+1;},30000000),"stop trial first generation did not complete");
    // Base clears RPRO before scanOnce runs the reprocess, so the wait ends only on an observed reprocess:
    // a queued successor holds PACT, or a rejected one has completed its own FLNK.
    const char* waitExit="timeout"; unsigned waitRpro=0,waitPact=0; uint64_t waitGeneration=0; double waitLinks=0;
    until([&]{ RecordLock lock(rec);
               waitRpro=rec->rpro; waitPact=rec->pact; waitGeneration=context->identity.generation;
               waitLinks=typed<calcRecord>("Records_QueueCompleted").val-links;
               if(context->identity.generation>=2 && rec->pact)waitExit="queued";
               else if(waitLinks>=2)waitExit="rejected";
               else return false;
               return true; },3000000);
    const auto held=scheduler->snapshot(1);
    bool queuedSuccessor=false;
    { RecordLock lock(rec); queuedSuccessor=context->identity.generation>=2 && rec->pact; }
    const auto stopAt=monotonicUs();
    Runtime::instance().stop();
    const auto stoppedAt=monotonicUs();
    for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
    const auto records=Requests::instance().snapshot();
    const auto runtime=Runtime::instance().snapshot();
    const auto after=scheduler->snapshot(1);
    RecordLock lock(rec);
    std::printf("{\"event\":\"stop_queued\",\"queued_successor\":%s,\"held_retirement_pending\":%llu,\"held_queued\":%llu,"
                "\"generation\":%llu,\"stat\":\"%s\",\"sevr\":\"%s\",\"pact\":%u,\"flnk_delta\":%.0f,\"completions_delta\":%llu,"
                "\"drain_failed\":%s,\"state\":%d,\"stop_at_us\":%llu,\"stop_duration_us\":%llu,\"reap_at_us\":%llu,\"retirement_pending_after\":%llu,"
                "\"settled\":%s,\"amsg\":\"%s\",\"wait_exit\":\"%s\",\"wait_rpro\":%u,\"wait_pact\":%u,\"wait_generation\":%llu,"
                "\"wait_flnk_delta\":%.0f,\"timeline\":%s}\n",
                queuedSuccessor?"true":"false",
                (unsigned long long)held.retirementPending,(unsigned long long)held.queued,
                (unsigned long long)context->identity.generation,epicsAlarmConditionStrings[rec->stat],
                epicsAlarmSeverityStrings[rec->sevr],rec->pact,typed<calcRecord>("Records_QueueCompleted").val-links,
                (unsigned long long)(records.completions-before),records.drainFailed?"true":"false",int(runtime.state),
                (unsigned long long)stopAt,(unsigned long long)(stoppedAt-stopAt),(unsigned long long)firstAt(events,7),
                (unsigned long long)after.retirementPending,scheduler->settled()?"true":"false",rec->amsg,
                waitExit,waitRpro,waitPact,(unsigned long long)waitGeneration,waitLinks,timeline(events).c_str());
    check(std::strcmp(waitExit,"timeout")!=0,"stop trial reprocess was not observed before stop");
}

// Admits all eleven record kinds on one unanswered address, waits until a batch is on the worker channel, then stops the
// runtime: every record, whether sent or still queued, must complete once with a communication alarm, and every FLNK must run once.
const char* StopKinds[]={"Ai","Longin","Int64in","Stringin","Lsi","Waveform","Ao","Longout","Int64out","Stringout","Lso"};
// Admits the eleven Timeout records of record-stop.db on their unanswered address and waits until a batch is on the
// worker channel, collecting the supervision events seen so far.
void admitStopRecords(std::vector<SupervisionEvent>& events)
{
    Runtime::instance().takeSupervisionEvents();
    for(const char* kind:StopKinds) {
        const std::string name=std::string("Records_Timeout")+kind;
        auto* rec=record(name.c_str());
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        RecordLock lock(rec);
        rec->udf=context->definition.kind<=RecordKind::Waveform;
        if(context->definition.kind==RecordKind::Lso) {
            auto& value=*reinterpret_cast<lsoRecord*>(rec);
            std::strcpy(value.val,"stopping"); value.len=9;
        }
        dbProcess(rec);
        check(rec->pact,(std::string("stop record was not admitted: ")+name).c_str());
    }
    check(until([&]{ for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
                     return firstAt(events,14)!=0; }),"stop batch was not sent to the worker");
}
// Reads every Timeout record under its lock and renders the per-record JSON rows with the summed counts. Once the
// IOC shutdown has freed the locksets and detached the contexts, the records are read unlocked and without a context.
std::string stopRecordRows(unsigned& stopped,unsigned& idle,unsigned& busy,bool locked=true)
{
    stopped=idle=busy=0;
    std::string detail="[";
    for(const char* kind:StopKinds) {
        const std::string name=std::string("Records_Timeout")+kind;
        auto* rec=record(name.c_str());
        auto* context=static_cast<RecordContext*>(rec->dpvt);
        RecordLock lock(locked?rec:nullptr);
        const bool wave=std::strcmp(kind,"Waveform")==0;
        const unsigned waveBusy=wave?typed<waveformRecord>(name.c_str()).busy:0;
        stopped+=rec->stat==COMM_ALARM && rec->sevr==INVALID_ALARM;
        idle+=!rec->pact; busy+=waveBusy;
        if(detail.size()>1)detail+=",";
        detail+="{\"record\":\""+name+"\",\"pact\":"+std::to_string(rec->pact)+",\"stat\":\""+epicsAlarmConditionStrings[rec->stat]+
                "\",\"sevr\":\""+epicsAlarmSeverityStrings[rec->sevr]+"\",\"busy\":"+std::to_string(waveBusy)+
                ",\"published\":"+(context && context->published?"true":"false")+
                ",\"generation\":"+std::to_string(context?context->identity.generation:0)+"}";
    }
    return detail+"]";
}
void stopInFlight()
{
    auto scheduler=Runtime::instance().schedulerOwner();
    const auto before=Requests::instance().snapshot().completions;
    const auto links=typed<calcRecord>("Records_StopCompleted").val;
    std::vector<SupervisionEvent> events;
    admitStopRecords(events);
    const auto held=scheduler->snapshot(1);
    const auto pending=Requests::instance().snapshot();
    const auto stopAt=monotonicUs();
    Runtime::instance().stop();
    const auto stoppedAt=monotonicUs();
    for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
    const auto records=Requests::instance().snapshot();
    const auto runtime=Runtime::instance().snapshot();
    const auto after=scheduler->snapshot(1);
    unsigned stopped=0,idle=0,busy=0;
    const std::string detail=stopRecordRows(stopped,idle,busy);
    std::printf("{\"event\":\"stop_inflight\",\"records\":%s,\"stopped\":%u,\"idle\":%u,\"waveform_busy\":%u,"
                "\"flnk_delta\":%.0f,\"completions_delta\":%llu,\"drain_failed\":%s,\"state\":%d,"
                "\"pending_at_stop\":%llu,\"held_queued\":%llu,\"held_retirement_pending\":%llu,\"retirement_pending_after\":%llu,"
                "\"settled\":%s,\"stop_at_us\":%llu,\"stop_duration_us\":%llu,\"reap_at_us\":%llu,\"timeline\":%s}\n",
                detail.c_str(),stopped,idle,busy,typed<calcRecord>("Records_StopCompleted").val-links,
                (unsigned long long)(records.completions-before),records.drainFailed?"true":"false",int(runtime.state),
                (unsigned long long)pending.active,(unsigned long long)held.queued,(unsigned long long)held.retirementPending,
                (unsigned long long)after.retirementPending,scheduler->settled()?"true":"false",
                (unsigned long long)stopAt,(unsigned long long)(stoppedAt-stopAt),(unsigned long long)firstAt(events,7),
                timeline(events).c_str());
}

// Fills the low-priority Base callback queue before the records are admitted, so every completion enqueue fails and
// the stop must retry them. Released while the runtime thread still runs ("within"), or after that thread has reached
// its stop bound and only the record drain retries ("late"), each record completes once with FLNK; released after the
// drain budget has expired ("after"), the drain fails, the records stay active and the isolated cleanup finalizes them once.
void stopWithEnqueueFailures()
{
    const char* mode=std::getenv("SNMP3_RECORD_RELEASE");
    check(mode && (std::strcmp(mode,"within")==0 || std::strcmp(mode,"late")==0 || std::strcmp(mode,"after")==0),
          "stop release mode absent");
    const bool expire=std::strcmp(mode,"after")==0,late=std::strcmp(mode,"late")==0;
    auto scheduler=Runtime::instance().schedulerOwner();
    const auto before=Requests::instance().snapshot();
    const auto links=typed<calcRecord>("Records_StopCompleted").val;
    QueueBlocker blocker; epicsCallback fillers[8]{};
    blocker.queued=callbackRequest(&blocker.callback)==0;
    check(blocker.queued && blocker.entered.wait(3.0),"stop queue blocker did not enter");
    for(auto& filler:fillers) {
        callbackSetCallback(&QueueBlocker::count,&filler);
        callbackSetUser(&blocker,&filler); callbackSetPriority(priorityLow,&filler);
        check(callbackRequest(&filler)==0,"stop callback queue fill failed");
    }
    std::vector<SupervisionEvent> events;
    admitStopRecords(events);
    std::atomic<bool> finished{false}; uint64_t stoppedAt=0;
    const auto stopAt=monotonicUs();
    std::thread stopper([&]{ Runtime::instance().stop(); stoppedAt=monotonicUs(); finished=true; });
    // Every terminal has been taken and refused by the full queue; the drain keeps retrying them.
    const bool retrying=until([&]{ const auto v=Requests::instance().snapshot();
                                   return v.active==11 && v.pending==11 && v.queued==0 && v.entered==0 &&
                                          v.enqueueFailures>=before.enqueueFailures+11; },3000000);
    const auto held=Requests::instance().snapshot();
    // After the drain budget expires the stop must return on its own, before the queue is released.
    bool expired=false,returned=false;
    if(expire) {
        expired=until([]{ return Requests::instance().snapshot().drainFailed; },5000000);
        returned=until([&]{ return finished.load(); },3000000);
    }
    // The supervisor stop bound is 2 s and the record drain budget 2 s after it; 2.5 s lies inside the drain alone.
    if(late)until([&]{ return monotonicUs()>=ipc::add(stopAt,2500000); },3000000);
    const bool stopping=!finished;
    const auto releaseAt=monotonicUs();
    blocker.release.trigger(); stopper.join();
    check(until([&]{ return blocker.noops==8; }),"stop queue fillers did not drain");
    for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
    const auto records=Requests::instance().snapshot();
    const auto runtime=Runtime::instance().snapshot();
    const bool restart=expire?Runtime::instance().start():false;
    unsigned stopped=0,idle=0,busy=0;
    const std::string detail=stopRecordRows(stopped,idle,busy);
    std::printf("{\"event\":\"stop_enqueue_failed\",\"mode\":\"%s\",\"retrying\":%s,\"still_stopping_at_release\":%s,\"expired_before_release\":%s,\"returned_before_release\":%s,"
                "\"held_active\":%llu,\"held_pending\":%llu,\"held_queued\":%llu,\"enqueue_failures_delta\":%llu,"
                "\"records\":%s,\"stopped\":%u,\"idle\":%u,\"waveform_busy\":%u,\"flnk_delta\":%.0f,\"completions_delta\":%llu,"
                "\"active_after\":%llu,\"drain_failed\":%s,\"state\":%d,\"restart_accepted\":%s,\"settled\":%s,"
                "\"stop_at_us\":%llu,\"release_at_us\":%llu,\"stop_duration_us\":%llu,\"reap_at_us\":%llu,\"timeline\":%s}\n",
                mode,retrying?"true":"false",stopping?"true":"false",expired?"true":"false",returned?"true":"false",
                (unsigned long long)held.active,(unsigned long long)held.pending,(unsigned long long)held.queued,
                (unsigned long long)(records.enqueueFailures-before.enqueueFailures),
                detail.c_str(),stopped,idle,busy,typed<calcRecord>("Records_StopCompleted").val-links,
                (unsigned long long)(records.completions-before.completions),(unsigned long long)records.active,
                records.drainFailed?"true":"false",int(runtime.state),restart?"true":"false",scheduler->settled()?"true":"false",
                (unsigned long long)stopAt,(unsigned long long)releaseAt,(unsigned long long)(stoppedAt-stopAt),
                (unsigned long long)firstAt(events,7),timeline(events).c_str());
}

// Points every Timeout record's FLNK at a record in another lockset through a Channel Access link and holds that
// downstream record's lock: the stop must complete without waiting for the downstream processing, which Base runs on
// its CA link thread once the lock is released. With the lock held through the IOC shutdown, the snmp3 stop still
// completes first; the Base CA link shutdown then waits for the lock, and the pending puts are reported as observed.
void stopWithHeldDownstream()
{
    const char* mode=std::getenv("SNMP3_RECORD_RELEASE");
    check(mode && (std::strcmp(mode,"stop")==0 || std::strcmp(mode,"shutdown")==0),"downstream release mode absent");
    const bool throughShutdown=std::strcmp(mode,"shutdown")==0;
    auto scheduler=Runtime::instance().schedulerOwner();
    const auto before=Requests::instance().snapshot().completions;
    auto* external=record("Records_StopExternal");
    auto* first=record("Records_TimeoutAi");
    bool connected=false;
    until([&]{ RecordLock lock(first); connected=dbIsLinkConnected(&first->flnk)!=0; return connected; },5000000);
    const bool separateLockset=dbLockGetLockId(first)!=dbLockGetLockId(external);
    RecordLock held(external);
    std::vector<SupervisionEvent> events;
    admitStopRecords(events);
    const auto stopAt=monotonicUs();
    std::atomic<bool> finished{false}; uint64_t stoppedAt=0;
    std::thread stopper([&]{ if(throughShutdown)testIocShutdownOk(); else Runtime::instance().stop(); stoppedAt=monotonicUs(); finished=true; });
    // Hold the downstream lock past the record drain budget; the stop must not depend on it.
    until([&]{ return monotonicUs()>=ipc::add(stopAt,2500000); },3000000);
    const bool stillRunning=!finished;
    const auto runtimeHeld=Runtime::instance().snapshot();
    const auto recordsHeld=Requests::instance().snapshot();
    for(auto& e:Runtime::instance().takeSupervisionEvents())events.push_back(e);
    const double externalHeld=reinterpret_cast<calcRecord*>(external)->val;
    const auto releaseAt=monotonicUs();
    held.unlock(); stopper.join();
    // After the IOC shutdown the locksets are gone and nothing else writes the records, so they are read unlocked.
    double externalAfter=0;
    until([&]{ RecordLock lock(throughShutdown?nullptr:external); externalAfter=reinterpret_cast<calcRecord*>(external)->val;
               return externalAfter>=11; },throughShutdown?100000:3000000);
    const auto records=Requests::instance().snapshot();
    const auto runtime=Runtime::instance().snapshot();
    unsigned stopped=0,idle=0,busy=0;
    const std::string detail=stopRecordRows(stopped,idle,busy,!throughShutdown);
    std::printf("{\"event\":\"stop_downstream\",\"mode\":\"%s\",\"link_connected\":%s,\"separate_lockset\":%s,"
                "\"still_running_at_release\":%s,\"state_while_held\":%d,\"completions_while_held\":%llu,\"drain_failed_while_held\":%s,"
                "\"external_while_held\":%.0f,\"external_after_release\":%.0f,\"records\":%s,\"stopped\":%u,\"idle\":%u,\"waveform_busy\":%u,"
                "\"completions_delta\":%llu,\"drain_failed\":%s,\"state\":%d,\"settled\":%s,\"stop_at_us\":%llu,\"release_at_us\":%llu,"
                "\"stop_duration_us\":%llu,\"reap_at_us\":%llu,\"timeline\":%s}\n",
                mode,connected?"true":"false",separateLockset?"true":"false",stillRunning?"true":"false",int(runtimeHeld.state),
                (unsigned long long)(recordsHeld.completions-before),recordsHeld.drainFailed?"true":"false",externalHeld,externalAfter,
                detail.c_str(),stopped,idle,busy,(unsigned long long)(records.completions-before),records.drainFailed?"true":"false",
                int(runtime.state),scheduler->settled()?"true":"false",(unsigned long long)stopAt,(unsigned long long)releaseAt,
                (unsigned long long)(stoppedAt-stopAt),(unsigned long long)firstAt(events,7),timeline(events).c_str());
}

// Replaces one link field through Base's own put path, which asks the device support to detach first.
// Returns true when Base refused the replacement.
bool replacementRefused(const char* field,const char* text)
{
    DBADDR address{};
    check(dbNameToAddr(field,&address)==0,"link field lookup failed");
    return dbPutField(&address,DBR_STRING,text,1)!=0;
}
// Tries to replace the input link of Records_TimeoutAi and the output link of Records_TimeoutAo and reports
// whether Base refused both and left each record's context and binding untouched.
struct DetachProbe { bool taken=false; bool refused=false; bool intact=false; };
DetachProbe probeReplacement(const std::vector<RecordContext*>& contexts,const std::vector<uint64_t>& handles)
{
    DetachProbe probe; probe.taken=true;
    const bool ai=replacementRefused("Records_TimeoutAi.INP","@binding=TimeoutCounter32Read deadline_ms=5000");
    const bool ao=replacementRefused("Records_TimeoutAo.OUT","@binding=TimeoutIntegerWrite deadline_ms=5000");
    probe.refused=ai && ao;
    probe.intact=record("Records_TimeoutAi")->dpvt==contexts[0] && record("Records_TimeoutAo")->dpvt==contexts[1] &&
                 contexts[0]->handle==handles[0] && contexts[1]->handle==handles[1] &&
                 contexts[0]->record==record("Records_TimeoutAi") && contexts[1]->record==record("Records_TimeoutAo");
    return probe;
}
// Link replacement while requests are in flight, after they complete, after the operator stop, and during the record
// drain of a stop whose completions the full Base queue refuses: Base must refuse every attempt and leave the contexts.
void liveDetachCells()
{
    const char* phase=std::getenv("SNMP3_RECORD_PHASE");
    check(phase && (std::strcmp(phase,"live")==0 || std::strcmp(phase,"drain")==0),"live detach phase absent");
    const bool drain=std::strcmp(phase,"drain")==0;
    const char* names[]={"Records_TimeoutAi","Records_TimeoutAo"};
    std::vector<RecordContext*> contexts; std::vector<uint64_t> handles;
    for(const char* name:names) { auto* context=static_cast<RecordContext*>(record(name)->dpvt);
                                  contexts.push_back(context); handles.push_back(context->handle); }
    const auto before=Requests::instance().snapshot();
    QueueBlocker blocker; epicsCallback fillers[8]{};
    if(drain) {
        blocker.queued=callbackRequest(&blocker.callback)==0;
        check(blocker.queued && blocker.entered.wait(3.0),"live detach queue blocker did not enter");
        for(auto& filler:fillers) {
            callbackSetCallback(&QueueBlocker::count,&filler);
            callbackSetUser(&blocker,&filler); callbackSetPriority(priorityLow,&filler);
            check(callbackRequest(&filler)==0,"live detach callback queue fill failed");
        }
    }
    for(const char* name:names) {
        auto* rec=record(name); RecordLock lock(rec);
        rec->udf=std::strcmp(name,"Records_TimeoutAi")==0;
        dbProcess(rec); check(rec->pact,"live detach request was not admitted");
    }
    const auto inFlight=probeReplacement(contexts,handles);
    unsigned activeBefore=0;
    for(const char* name:names) { RecordLock lock(record(name)); activeBefore+=record(name)->pact; }
    DetachProbe idle,stopped,during; unsigned pendingDuring=0; bool stillStopping=false; uint64_t attemptAt=0;
    std::vector<unsigned> alarms;
    if(!drain) {
        check(until([&]{ unsigned open=0; for(const char* name:names) { RecordLock lock(record(name)); open+=record(name)->pact; }
                         return open==0; },8000000),"live detach requests did not complete");
        for(const char* name:names) { RecordLock lock(record(name)); alarms.push_back(record(name)->stat*10+record(name)->sevr); }
        idle=probeReplacement(contexts,handles);
        Runtime::instance().stop();
        stopped=probeReplacement(contexts,handles);
    } else {
        check(until([&]{ const auto v=Requests::instance().snapshot();
                         return v.active==2 && v.pending==2 && v.enqueueFailures>=before.enqueueFailures+2; },3000000),
              "live detach completions were not refused by the full queue");
        std::atomic<bool> finished{false};
        const auto stopAt=monotonicUs();
        std::thread stopper([&]{ Runtime::instance().stop(); finished=true; });
        // 2.5 s lies after the supervisor bound and inside the record drain budget.
        until([&]{ return monotonicUs()>=ipc::add(stopAt,2500000); },3000000);
        stillStopping=!finished; attemptAt=monotonicUs()-stopAt;
        pendingDuring=unsigned(Requests::instance().snapshot().pending);
        during=probeReplacement(contexts,handles);
        blocker.release.trigger(); stopper.join();
        check(until([&]{ return blocker.noops==8; }),"live detach fillers did not drain");
        for(const char* name:names) { RecordLock lock(record(name)); alarms.push_back(record(name)->stat*10+record(name)->sevr); }
    }
    const auto records=Requests::instance().snapshot();
    unsigned open=0;
    for(const char* name:names) { RecordLock lock(record(name)); open+=record(name)->pact; }
    // A probe or measurement a phase does not take prints null, never a false default.
    const auto flag=[](const DetachProbe& p,bool value)->const char* { return !p.taken?"null":value?"true":"false"; };
    const std::string stopping=drain?(stillStopping?"true":"false"):"null";
    const std::string pending=drain?std::to_string(pendingDuring):"null";
    const std::string attempt=drain?std::to_string((unsigned long long)attemptAt):"null";
    std::printf("{\"event\":\"live_detach\",\"phase\":\"%s\",\"active_before\":%u,\"in_flight_refused\":%s,\"in_flight_intact\":%s,"
                "\"idle_refused\":%s,\"idle_intact\":%s,\"stopped_refused\":%s,\"stopped_intact\":%s,"
                "\"during_drain_refused\":%s,\"during_drain_intact\":%s,\"still_stopping_at_attempt\":%s,\"pending_during\":%s,"
                "\"attempt_after_us\":%s,\"open_after\":%u,\"completions_delta\":%llu,\"alarms\":[%u,%u],"
                "\"entry_open\":%s,\"detach_allowed\":%s,\"drain_failed\":%s}\n",
                phase,activeBefore,flag(inFlight,inFlight.refused),flag(inFlight,inFlight.intact),
                flag(idle,idle.refused),flag(idle,idle.intact),flag(stopped,stopped.refused),flag(stopped,stopped.intact),
                flag(during,during.refused),flag(during,during.intact),stopping.c_str(),pending.c_str(),
                attempt.c_str(),open,(unsigned long long)(records.completions-before.completions),
                alarms.size()>0?alarms[0]:0,alarms.size()>1?alarms[1]:0,records.entryOpen?"true":"false",
                records.detachAllowed?"true":"false",records.drainFailed?"true":"false");
}

// The idle isolated case observes detach before any put and storage again after callback join.
// Only value snapshots survive cleanup; a changed storage count prevents saved-context access.
struct RepeatRecord {
    const char* field;
    const char* replacements[2];
    DBADDR address{};
    RecordContext* context=nullptr;
    const void* dset=nullptr;
    std::string original;
    int linkType=-1;
    explicit RepeatRecord(const char* name,const char* first,const char* second)
        : field(name),replacements{first,second} {}
};
RepeatRecord repeatRecords[]={
    RepeatRecord("Records_TimeoutAi.INP","@binding=TimeoutCounter32Read deadline_ms=5000",
                 "@binding=TimeoutIntegerRead deadline_ms=4000"),
    RepeatRecord("Records_TimeoutAo.OUT","@binding=TimeoutIntegerWrite deadline_ms=5000",
                 "@binding=TimeoutFloatWrite deadline_ms=4000")
};
uint64_t repeatContexts=0;
bool repeatReady=false;
std::string jsonText(const std::string& text)
{
    std::string value="\"";
    for(unsigned char c:text) {
        if(c=='\"' || c=='\\') { value+='\\'; value+=char(c); }
        else if(c<32) { char escape[7]; std::snprintf(escape,sizeof(escape),"\\u%04x",unsigned(c)); value+=escape; }
        else value+=char(c);
    }
    return value+'\"';
}
unsigned long long pointerValue(const void* value)
{ return static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(value)); }
std::string repeatSnapshot(const RepeatRecord& saved,bool& safe)
{
    auto* rec=saved.address.precord;
    const auto count=Requests::instance().snapshot().contexts;
    if(!rec || !saved.address.pfield) { safe=false; return "null"; }
    RecordLock lock(rec);
    const auto& link=*static_cast<DBLINK*>(saved.address.pfield);
    const bool readable=saved.context && repeatContexts>0 && count==repeatContexts;
    const std::string linkText=link.type==INST_IO && link.value.instio.string ? jsonText(link.value.instio.string):"null";
    const std::string contextRecord=readable?std::to_string(pointerValue(saved.context->record)):"null";
    safe=readable && !rec->dpvt && !saved.context->record && rec->dset==saved.dset && link.type==INST_IO;
    return "{\"field\":"+jsonText(saved.field)+",\"record\":"+std::to_string(pointerValue(rec))+
           ",\"link_type\":"+std::to_string(link.type)+",\"link\":"+linkText+
           ",\"dset\":"+std::to_string(pointerValue(rec->dset))+",\"dpvt\":"+std::to_string(pointerValue(rec->dpvt))+
           ",\"context_record\":"+contextRecord+",\"contexts\":"+std::to_string(count)+"}";
}
void prepareRepeatDetach()
{
    repeatContexts=Requests::instance().snapshot().contexts;
    repeatReady=repeatContexts>0;
    std::string rows;
    for(auto& saved:repeatRecords) {
        const bool found=dbNameToAddr(saved.field,&saved.address)==0;
        repeatReady=repeatReady && found;
        if(found) {
            RecordLock lock(saved.address.precord);
            const auto& link=*static_cast<DBLINK*>(saved.address.pfield);
            saved.linkType=link.type;
            if(link.type==INST_IO && link.value.instio.string)saved.original=link.value.instio.string;
            saved.dset=saved.address.precord->dset;
            saved.context=static_cast<RecordContext*>(saved.address.precord->dpvt);
            repeatReady=repeatReady && saved.dset && saved.context &&
                        saved.context->record==saved.address.precord && !saved.address.precord->pact &&
                        link.type==INST_IO && !saved.original.empty();
            for(const char* text:saved.replacements) {
                const auto parsed=parseRecordLink(text);
                repeatReady=repeatReady && text[0]=='@' && saved.original!=text+1 && parsed.budgetMs>0;
            }
        }
        bool unused=false;
        if(!rows.empty())rows+=',';
        rows+=repeatSnapshot(saved,unused);
    }
    std::printf("{\"event\":\"repeat_detach_baseline\",\"ready\":%s,\"contexts\":%llu,\"records\":[%s]}\n",
                repeatReady?"true":"false",(unsigned long long)repeatContexts,rows.c_str());
    // The generated registrar already registered the module hook; Base invokes hooks in registration order.
}
void repeatDetachHook(initHookState state)
{
    if(state!=initHookAfterCloseLinks && state!=initHookBeforeFree)return;
    try {
        if(state==initHookBeforeFree) {
            std::printf("{\"event\":\"repeat_detach_before_free\",\"contexts\":%llu}\n",
                        (unsigned long long)Requests::instance().snapshot().contexts);
        } else {
            bool safe=repeatReady;
            std::string rows;
            for(const auto& saved:repeatRecords) {
                bool intact=false;
                const auto row=repeatSnapshot(saved,intact);
                safe=safe && intact;
                if(!rows.empty())rows+=',';
                rows+=row;
            }
            std::printf("{\"event\":\"repeat_detach\",\"records\":[%s]}\n",rows.c_str());
            std::fflush(stdout);
            for(unsigned attempt=0;attempt<2;++attempt)for(auto& saved:repeatRecords) {
                const bool attempted=safe;
                const std::string status=attempted ? std::to_string(dbPutField(
                    &saved.address,DBR_STRING,saved.replacements[attempt],1)):"null";
                bool intact=false;
                const auto row=repeatSnapshot(saved,intact);
                safe=safe && intact;
                std::printf("{\"event\":\"repeat_detach_attempt\",\"field\":%s,\"attempt\":%u,"
                            "\"attempted\":%s,\"replacement\":%s,\"status\":%s,\"snapshot\":%s}\n",
                            jsonText(saved.field).c_str(),attempt+1,attempted?"true":"false",
                            jsonText(saved.replacements[attempt]).c_str(),status.c_str(),row.c_str());
                std::fflush(stdout);
            }
        }
    } catch(...) {
        // Base hooks are C callbacks. Preserve an explicit failure without repairing the observed product state.
        std::printf("{\"event\":\"repeat_detach_error\",\"hook\":%d}\n",int(state));
    }
    std::fflush(stdout);
}

// Each observation is taken under the record lock. Context fields read here are immutable,
// admission-owned, or callback-owned under that lock; queued terminals are read only after Runtime exit.
struct DtypeRecord {
    const char* name;
    const char* counter;
    DBADDR address{};
    RecordContext* context=nullptr;
    epicsUInt16 original=0,alternate=0;
    explicit DtypeRecord(const char* value,const char* links) : name(value),counter(links) {}
};
DtypeRecord dtypeRecords[]={DtypeRecord("Records_DtypeAi","Records_DtypeAiCompleted"),
                            DtypeRecord("Records_DtypeAo","Records_DtypeAoCompleted")};
const char* dtypeMode=nullptr;
uint64_t dtypeContexts=0;
unsigned dtypePackets()
{
    const char* path=std::getenv("SNMP3_RECORD_DELAY_TRACE");
    check(path!=nullptr,"DTYP UDP trace absent");
    std::ifstream stream(path); std::string line; unsigned count=0;
    while(std::getline(stream,line))if(line.find("\"event\": \"fault_request\"")!=std::string::npos)++count;
    return count;
}
void dtypeObservation(const char* phase,bool terminal=false)
{
    const auto requests=Requests::instance().snapshot();
    const auto runtime=Runtime::instance().snapshot();
    auto scheduler=Runtime::instance().schedulerOwner();
    const auto queue=scheduler->snapshot(1);
    std::ostringstream rows;
    for(auto& saved:dtypeRecords) {
        auto* rec=saved.address.precord; RecordLock lock(rec);
        if(&saved!=dtypeRecords)rows<<',';
        const bool readable=saved.context && requests.contexts==dtypeContexts && dtypeContexts>0;
        auto* link=dbGetDevLink(rec);
        rows<<"{\"name\":"<<jsonText(saved.name)<<",\"record\":"<<pointerValue(rec)
            <<",\"dpvt\":"<<pointerValue(rec->dpvt)<<",\"dset\":"<<pointerValue(rec->dset)
            <<",\"dtype\":"<<rec->dtyp<<",\"original\":"<<saved.original<<",\"alternate\":"<<saved.alternate
            <<",\"pact\":"<<unsigned(rec->pact)<<",\"stat\":"<<rec->stat<<",\"sevr\":"<<rec->sevr
            <<",\"udf\":"<<unsigned(rec->udf)<<",\"link_type\":"<<(link?link->type:-1)
            <<",\"link\":"<<(link && link->type==INST_IO && link->value.instio.string?jsonText(link->value.instio.string):"null")
            <<",\"value\":"<<(&saved==dtypeRecords?reinterpret_cast<aiRecord*>(rec)->val:reinterpret_cast<aoRecord*>(rec)->val);
        { auto* counter=record(saved.counter); RecordLock counterLock(counter);
          rows<<",\"flnk\":"<<reinterpret_cast<calcRecord*>(counter)->val; }
        rows<<",\"context\":";
        if(!readable)rows<<"null";
        else {
            const auto& context=*saved.context;
            rows<<"{\"record\":"<<pointerValue(context.record)<<",\"dtype\":"<<context.dtype
                <<",\"dset\":"<<pointerValue(context.dset)<<",\"binding\":"<<pointerValue(context.definition.binding.get())
                <<",\"handle\":"<<context.handle<<",\"generation\":"<<context.identity.generation
                <<",\"admission\":"<<context.identity.admission<<",\"identity_binding\":"<<context.identity.binding
                <<",\"activation\":"<<context.owner->activationId()<<",\"revision\":"<<context.owner->configurationRevision()
                <<",\"published\":"<<(context.published?"true":"false")
                <<",\"native_success\":"<<(context.nativeSuccess?"true":"false")
                <<",\"deadline_ms\":"<<context.definition.budgetMs;
            if(terminal)rows<<",\"terminal\":"<<(context.terminal.result?int(context.terminal.result->outcome):-1)
                            <<",\"terminal_same\":"<<(context.terminal.result && context.terminal.id==context.identity?"true":"false");
            rows<<'}';
        }
        rows<<'}';
    }
    std::printf("{\"event\":\"dtype\",\"mode\":\"%s\",\"phase\":\"%s\",\"at_us\":%llu,"
                "\"packets\":%u,\"contexts\":%llu,\"active\":%llu,\"queued\":%llu,\"pending\":%llu,"
                "\"entered\":%llu,\"completions\":%llu,\"entry_open\":%s,\"drain_failed\":%s,"
                "\"admission\":%s,\"state\":%d,\"exited\":%lu,\"count\":%llu,\"bytes\":%llu,"
                "\"settled\":%s,\"records\":[%s]}\n",dtypeMode,phase,(unsigned long long)monotonicUs(),dtypePackets(),
                (unsigned long long)requests.contexts,(unsigned long long)requests.active,(unsigned long long)requests.queued,
                (unsigned long long)requests.pending,(unsigned long long)requests.entered,(unsigned long long)requests.completions,
                requests.entryOpen?"true":"false",requests.drainFailed?"true":"false",runtime.admission?"true":"false",
                int(runtime.state),runtime.exited,(unsigned long long)queue.count,(unsigned long long)queue.bytes,
                scheduler->settled()?"true":"false",rows.str().c_str());
}
void dtypePut(bool restore,const char* phase,DtypeRecord* selected=nullptr)
{
    for(auto& saved:dtypeRecords) {
        if(selected && selected!=&saved)continue;
        const epicsUInt16 value=restore?saved.original:saved.alternate;
        const auto status=dbPutField(&saved.address,DBR_USHORT,&value,1);
        std::printf("{\"event\":\"dtype_put\",\"mode\":\"%s\",\"phase\":\"%s\",\"name\":\"%s\",\"value\":%u,\"status\":%ld}\n",
                    dtypeMode,phase,saved.name,value,status);
    }
    dtypeObservation(phase);
}
void dtypeProcess()
{
    for(auto& saved:dtypeRecords) { RecordLock lock(saved.address.precord); dbProcess(saved.address.precord); }
}
void dtypeWait()
{
    check(until([] { for(auto& saved:dtypeRecords) { RecordLock lock(saved.address.precord);
                     if(saved.address.precord->pact)return false; } return true; }),"DTYP completion timeout");
    // Omitted release must be observable without preventing normal real shutdown and fixture cleanup.
    until([] {return Runtime::instance().schedulerOwner()->settled();},1000000);
}
void dtypeHook(initHookState state)
{
    if(state!=initHookAtShutdown && state!=initHookAfterCloseLinks && state!=initHookBeforeFree)return;
    try {
        if(state==initHookAtShutdown)dtypeObservation("stop_done");
        else if(state==initHookAfterCloseLinks) {
            dtypeObservation("detached");
            bool dpvtClear=true,recordClear=Requests::instance().snapshot().contexts==dtypeContexts;
            for(const auto& saved:dtypeRecords) {
                RecordLock lock(saved.address.precord);
                dpvtClear=dpvtClear && !saved.address.precord->dpvt;
                if(recordClear)recordClear=!saved.context->record;
            }
            std::printf("{\"event\":\"dtype_detach_checks\",\"mode\":\"%s\","
                        "\"dtype-shutdown-dpvt-cleared\":%s,\"dtype-shutdown-record-cleared\":%s}\n",
                        dtypeMode,dpvtClear?"true":"false",recordClear?"true":"false");
            dtypePut(true,"detached_put");
        } else std::printf("{\"event\":\"dtype_free\",\"mode\":\"%s\",\"contexts\":%llu}\n",dtypeMode,
                          (unsigned long long)Requests::instance().snapshot().contexts);
    } catch(...) { std::printf("{\"event\":\"dtype_error\",\"mode\":\"%s\",\"hook\":%d}\n",dtypeMode,int(state)); }
}
void dtypeCells()
{
    dtypeMode=std::getenv("SNMP3_RECORD_DTYPE_MODE");
    check(dtypeMode && (std::strcmp(dtypeMode,"idle")==0 || std::strcmp(dtypeMode,"active")==0 ||
                       std::strcmp(dtypeMode,"restored")==0 || std::strcmp(dtypeMode,"shutdown")==0),"DTYP mode absent");
    dtypeContexts=Requests::instance().snapshot().contexts;
    for(auto& saved:dtypeRecords) {
        const std::string field=std::string(saved.name)+".DTYP";
        check(dbNameToAddr(field.c_str(),&saved.address)==0,"DTYP field lookup failed");
        dbr_enumStrs choices{}; long options=DBR_ENUM_STRS,count=0;
        check(dbGetField(&saved.address,DBR_USHORT,&choices,&options,&count,nullptr)==0 &&
              (options&DBR_ENUM_STRS) && choices.no_str>1,"DTYP menu lookup failed");
        RecordLock lock(saved.address.precord);
        saved.context=static_cast<RecordContext*>(saved.address.precord->dpvt);
        check(saved.context!=nullptr,"DTYP initial binding missing");
        saved.original=saved.address.precord->dtyp;
        saved.alternate=saved.original;
        for(unsigned i=0;i<choices.no_str;++i)if(i!=saved.original && choices.strs[i][0]) {saved.alternate=i; break;}
        check(saved.original!=saved.alternate,"DTYP alternate menu choice missing");
        saved.address.precord->udf=FALSE;
    }
    initHookRegister(dtypeHook);
    dtypeObservation("initial");
    if(std::strcmp(dtypeMode,"idle")==0) {
        dtypePut(false,"changed"); dtypeProcess(); dtypeWait(); dtypeObservation("refused");
        dtypePut(true,"restored"); dtypeProcess(); dtypeWait(); dtypeObservation("completed");
    } else if(std::strcmp(dtypeMode,"shutdown")!=0) {
        for(auto& saved:dtypeRecords) {
            const std::string suffix=&saved==dtypeRecords?"-ai":"-ao";
            const auto packets=dtypePackets();
            { RecordLock lock(saved.address.precord); dbProcess(saved.address.precord); }
            check(until([&] {return dtypePackets()>packets;}),"DTYP real request not observed");
            dtypeObservation(("inflight"+suffix).c_str());
            dtypePut(false,("changed"+suffix).c_str(),&saved);
            if(std::strcmp(dtypeMode,"restored")==0)dtypePut(true,("restored"+suffix).c_str(),&saved);
            dtypeWait(); dtypeObservation(("completed"+suffix).c_str());
        }
        if(std::strcmp(dtypeMode,"active")==0) {
            dtypeProcess(); dtypeWait(); dtypeObservation("refused");
        }
    } else {
        QueueBlocker blocker; blocker.queued=callbackRequest(&blocker.callback)==0;
        check(blocker.queued && blocker.entered.wait(3.0),"DTYP callback hold failed");
        dtypeProcess();
        check(until([] {auto s=Requests::instance().snapshot(); return s.queued==2 && s.entered==0;}),
              "DTYP native completions not queued");
        dtypeObservation("queued");
        std::atomic<bool> finished{false}; std::exception_ptr error;
        std::thread stopper([&] {try {testIocShutdownOk();} catch(...) {error=std::current_exception();} finished=true;});
        try {
            const bool window=until([&] {const auto r=Runtime::instance().snapshot(); const auto q=Requests::instance().snapshot();
                return !finished && !r.admission && r.exited==r.created && r.created>0 && q.entryOpen && !q.entered;},3000000);
            std::printf("{\"event\":\"dtype_window\",\"mode\":\"%s\",\"reached\":%s,\"still_stopping\":%s}\n",
                        dtypeMode,window?"true":"false",!finished?"true":"false");
            if(window) { dtypeObservation("stop_window",true); dtypePut(false,"changed"); }
        } catch(...) { blocker.release.trigger(); stopper.join(); throw; }
        blocker.release.trigger(); stopper.join();
        if(error)std::rethrow_exception(error);
        return;
    }
    testIocShutdownOk();
}

// Runtime link replacement uses the installed DSET for deletion and current DTYP for addition.
// Retain the original context separately; another support's dpvt is never interpreted as ours.
struct SupportRecord {
    const char* name;
    const char* counter;
    const char* field;
    DBADDR dtype{},link{};
    RecordContext* context=nullptr;
    devSup* originalSupport=nullptr;
    devSup* softSupport=nullptr;
    epicsUInt16 original=0,soft=0;
    std::string originalLink;
    explicit SupportRecord(const char* n,const char* c,const char* f) : name(n),counter(c),field(f) {}
};
SupportRecord supportRecords[]={SupportRecord("Records_SupportAi","Records_SupportAiCompleted","INP"),
                                SupportRecord("Records_SupportAo","Records_SupportAoCompleted","OUT")};
const char* supportMode=nullptr;
uint64_t supportContexts=0;
std::string supportLink(dbCommon* rec)
{
    auto* link=dbGetDevLink(rec);
    const char* text=link->type==INST_IO?link->value.instio.string:
                     link->type==CONSTANT?link->value.constantStr:nullptr;
    return text?text:"";
}
void supportObservation(const std::string& phase)
{
    const auto q=Requests::instance().snapshot();
    const auto r=Runtime::instance().snapshot();
    auto owner=Runtime::instance().schedulerOwner(); const auto queue=owner->snapshot(1);
    std::ostringstream out;
    out<<"{\"event\":\"support\",\"mode\":"<<jsonText(supportMode)<<",\"phase\":"<<jsonText(phase)
       <<",\"at_us\":"<<monotonicUs()<<",\"packets\":"<<dtypePackets()<<",\"contexts\":"<<q.contexts
       <<",\"active\":"<<q.active<<",\"queued\":"<<q.queued<<",\"pending\":"<<q.pending
       <<",\"entered\":"<<q.entered<<",\"completions\":"<<q.completions<<",\"entry_open\":"<<(q.entryOpen?"true":"false")
       <<",\"detach_allowed\":"<<(q.detachAllowed?"true":"false")<<",\"drain_failed\":"<<(q.drainFailed?"true":"false")
       <<",\"admission\":"<<(r.admission?"true":"false")<<",\"state\":"<<int(r.state)
       <<",\"created\":"<<r.created<<",\"exited\":"<<r.exited<<",\"count\":"<<queue.count<<",\"bytes\":"<<queue.bytes
       <<",\"settled\":"<<(owner->settled()?"true":"false")<<",\"records\":[";
    for(auto& saved:supportRecords) {
        auto* rec=saved.dtype.precord; RecordLock lock(rec);
        if(&saved!=supportRecords)out<<',';
        const auto* installed=dbDSETtoDevSup(rec->rdes,rec->dset);
        out<<"{\"name\":"<<jsonText(saved.name)<<",\"record\":"<<pointerValue(rec)
           <<",\"dpvt\":"<<pointerValue(rec->dpvt)<<",\"dset\":"<<pointerValue(rec->dset)
           <<",\"dsxt\":"<<pointerValue(installed?installed->pdsxt:nullptr)
           <<",\"original_dset\":"<<pointerValue(saved.originalSupport->pdset)
           <<",\"original_dsxt\":"<<pointerValue(saved.originalSupport->pdsxt)
           <<",\"soft_dset\":"<<pointerValue(saved.softSupport->pdset)
           <<",\"soft_dsxt\":"<<pointerValue(saved.softSupport->pdsxt)
           <<",\"dtype\":"<<rec->dtyp<<",\"original\":"<<saved.original<<",\"soft\":"<<saved.soft
           <<",\"pact\":"<<unsigned(rec->pact)<<",\"stat\":"<<rec->stat<<",\"sevr\":"<<rec->sevr
           <<",\"link_type\":"<<dbGetDevLink(rec)->type<<",\"link\":"<<jsonText(supportLink(rec))
           <<",\"value\":"<<(&saved==supportRecords?reinterpret_cast<aiRecord*>(rec)->val:reinterpret_cast<aoRecord*>(rec)->val);
        { auto* counter=record(saved.counter); RecordLock lockCounter(counter);
          out<<",\"flnk\":"<<reinterpret_cast<calcRecord*>(counter)->val; }
        out<<",\"context\":";
        if(!saved.context || q.contexts!=supportContexts)out<<"null";
        else {
            const auto& c=*saved.context;
            out<<"{\"pointer\":"<<pointerValue(saved.context)<<",\"record\":"<<pointerValue(c.record)
               <<",\"dtype\":"<<c.dtype<<",\"dset\":"<<pointerValue(c.dset)
               <<",\"binding\":"<<pointerValue(c.definition.binding.get())<<",\"handle\":"<<c.handle
               <<",\"activation\":"<<c.owner->activationId()<<",\"revision\":"<<c.owner->configurationRevision()
               <<",\"generation\":"<<c.identity.generation<<",\"admission\":"<<c.identity.admission
               <<",\"identity_binding\":"<<c.identity.binding<<",\"published\":"<<(c.published?"true":"false")
               <<",\"native_success\":"<<(c.nativeSuccess?"true":"false");
            if(phase=="stop_window")out<<",\"terminal\":"<<(c.terminal.result?int(c.terminal.result->outcome):-1)
                                      <<",\"terminal_same\":"<<(c.terminal.result && c.terminal.id==c.identity?"true":"false");
            out<<'}';
        }
        out<<'}';
    }
    out<<"]}"; std::puts(out.str().c_str());
}
void supportPut(SupportRecord& saved,const std::string& phase,bool restore,bool link,bool incompatible=false)
{
    const epicsUInt16 dtype=restore?saved.original:saved.soft;
    const std::string replacement=restore || incompatible?"@"+saved.originalLink:"123";
    const auto status=link?dbPutField(&saved.link,DBR_CHAR,replacement.c_str(),replacement.size()+1):
                           dbPutField(&saved.dtype,DBR_USHORT,&dtype,1);
    std::printf("{\"event\":\"support_put\",\"mode\":%s,\"phase\":%s,\"name\":%s,\"field\":%s,"
                "\"value\":%s,\"status\":%ld}\n",jsonText(supportMode).c_str(),jsonText(phase).c_str(),
                jsonText(saved.name).c_str(),jsonText(link?saved.field:"DTYP").c_str(),
                (link?jsonText(replacement):std::to_string(dtype)).c_str(),status);
}
void supportPair(const std::string& phase,bool restore,bool link,bool incompatible=false)
{
    for(auto& saved:supportRecords)supportPut(saved,phase,restore,link,incompatible);
    supportObservation(phase);
}
void supportProcess(SupportRecord* selected=nullptr)
{
    for(auto& saved:supportRecords)if(!selected || selected==&saved) {
        RecordLock lock(saved.dtype.precord); dbProcess(saved.dtype.precord);
    }
}
void supportWait()
{
    check(until([] {for(auto& s:supportRecords) {RecordLock lock(s.dtype.precord);
                     if(s.dtype.precord->pact)return false;} return true;}),"support completion timeout");
    until([] {return Runtime::instance().schedulerOwner()->settled();},1000000);
}
void supportHook(initHookState state)
{
    if(state!=initHookAtShutdown && state!=initHookAfterCloseLinks && state!=initHookBeforeFree)return;
    try {
        if(state==initHookBeforeFree) {
            std::printf("{\"event\":\"support_free\",\"mode\":%s,\"contexts\":%llu}\n",jsonText(supportMode).c_str(),
                        (unsigned long long)Requests::instance().snapshot().contexts);
            return;
        }
        if(state==initHookAtShutdown) {
            supportObservation("stop_done");
            if(std::strcmp(supportMode,"before-close")==0) {
                const auto q=Requests::instance().snapshot();
                check(!q.entryOpen && !q.entered && q.detachAllowed,"support pre-close phase missing");
                supportPair("soft-selected",false,false);
                supportPair("transferred",false,true);
                supportPair("snmp-selected",true,false);
                supportPair("return-attempted",true,true);
            }
        } else {
            supportObservation("detached");
            if(std::strcmp(supportMode,"after-close")==0) {
                supportPair("soft-selected",false,false);
                supportPair("repeat-1",false,true); supportPair("repeat-2",false,true);
            }
        }
    } catch(...) {std::printf("{\"event\":\"support_error\",\"mode\":%s,\"hook\":%d}\n",jsonText(supportMode).c_str(),int(state));}
}
void supportCells()
{
    supportMode=std::getenv("SNMP3_RECORD_SUPPORT_MODE");
    check(supportMode!=nullptr,"support mode absent");
    supportContexts=Requests::instance().snapshot().contexts;
    for(auto& saved:supportRecords) {
        check(dbNameToAddr((std::string(saved.name)+".DTYP").c_str(),&saved.dtype)==0 &&
              dbNameToAddr((std::string(saved.name)+"."+saved.field).c_str(),&saved.link)==0,"support field lookup failed");
        dbr_enumStrs choices{}; long options=DBR_ENUM_STRS,count=0;
        check(dbGetField(&saved.dtype,DBR_USHORT,&choices,&options,&count,nullptr)==0 && (options&DBR_ENUM_STRS),"support menu absent");
        RecordLock lock(saved.dtype.precord); auto* rec=saved.dtype.precord;
        saved.original=rec->dtyp; saved.soft=saved.original;
        for(unsigned i=0;i<choices.no_str;++i)if(std::strcmp(choices.strs[i],"Soft Channel")==0)saved.soft=i;
        check(saved.original<choices.no_str && std::strcmp(choices.strs[saved.original],"snmp3")==0 && saved.soft!=saved.original,"support choices absent");
        saved.originalSupport=dbDTYPtoDevSup(rec->rdes,saved.original); saved.softSupport=dbDTYPtoDevSup(rec->rdes,saved.soft);
        check(saved.originalSupport && saved.softSupport && saved.originalSupport->pdset==rec->dset &&
              saved.originalSupport->pdsxt && saved.originalSupport->pdsxt->add_record && saved.originalSupport->pdsxt->del_record &&
              saved.softSupport->pdset && saved.softSupport->link_type==CONSTANT && saved.softSupport->pdsxt==&devSoft_DSXT &&
              saved.softSupport->pdsxt->add_record && saved.softSupport->pdsxt->del_record,"support extension prerequisites differ");
        saved.context=static_cast<RecordContext*>(rec->dpvt); check(saved.context && saved.context->record==rec,"support context absent");
        check(dbGetDevLink(rec)->type==INST_IO,"support original link type differs");saved.originalLink=supportLink(rec);rec->udf=FALSE;
    }
    initHookRegister(supportHook); supportObservation("initial");
    // The incompatible probe must be separately recognized as Base validation, never detach evidence.
    supportPair("probe-selected",false,false);supportPair("probe-rejected",false,true,true);supportPair("probe-restored",true,false);
    if(std::strcmp(supportMode,"idle")==0) {
        supportPair("soft-selected",false,false);supportPair("attempted",false,true);
        bool attached=true;
        for(auto& s:supportRecords) {RecordLock lock(s.dtype.precord);attached=attached && s.dtype.precord->dpvt==s.context && s.context->record==s.dtype.precord && s.dtype.precord->dset==s.originalSupport->pdset;}
        std::printf("{\"event\":\"support_resume\",\"mode\":%s,\"processing\":%s}\n",jsonText(supportMode).c_str(),attached?"true":"false");
        if(attached) {supportPair("restored",true,false);supportProcess();supportWait();supportObservation("completed");}
    } else if(std::strcmp(supportMode,"active")==0) {
        for(auto& s:supportRecords) {
            const std::string label=&s==supportRecords?"-ai":"-ao";const auto packets=dtypePackets();
            supportProcess(&s);check(until([&] {return dtypePackets()>packets;}),"support request not observed");
            supportObservation("inflight"+label);supportPut(s,"selected"+label,false,false);supportObservation("selected"+label);
            supportPut(s,"attempted"+label,false,true);supportObservation("attempted"+label);
            supportWait();supportObservation("completed"+label);
        }
    } else if(std::strcmp(supportMode,"drain")==0) {
        QueueBlocker blocker;blocker.queued=callbackRequest(&blocker.callback)==0;
        check(blocker.queued && blocker.entered.wait(3.0),"support callback hold failed");supportProcess();
        check(until([] {auto q=Requests::instance().snapshot();return q.queued==2 && !q.entered;}),"support queued terminals absent");
        supportObservation("queued"); std::atomic<bool> finished{false};std::exception_ptr error;
        std::thread stopper([&] {try {testIocShutdownOk();}catch(...) {error=std::current_exception();}finished=true;});
        try {
            const bool window=until([&] {auto r=Runtime::instance().snapshot();auto q=Requests::instance().snapshot();
                return !finished && !r.admission && r.created>0 && r.exited==r.created && q.entryOpen && !q.entered;},3000000);
            check(window,"support drain window missed");supportObservation("stop_window");
            supportPair("soft-selected",false,false);supportPair("attempted",false,true);
        } catch(...) {blocker.release.trigger();stopper.join();throw;}
        blocker.release.trigger();stopper.join();if(error)std::rethrow_exception(error);return;
    } else check(std::strcmp(supportMode,"before-close")==0 || std::strcmp(supportMode,"after-close")==0,"unknown support mode");
    testIocShutdownOk();
}
}

namespace {
std::string boundaryName(unsigned row,const std::string& suffix)
{ return "Records_B"+std::to_string(row)+"_"+suffix; }
void boundaryStimulus(unsigned kind,const Value& value)
{
    auto& runtime=Runtime::instance(); auto owner=runtime.schedulerOwner();
    const auto ids=runtime.admit({boundaryHandles[kind]},{value},5000);
    check(ids.size()==1,"boundary stimulus admission absent");
    const auto end=ipc::add(monotonicUs(),8000000);
    TerminalView terminal;
    do { terminal=owner->take(ids[0]); if(terminal.result)break; epicsThreadSleep(0.001); } while(monotonicUs()<end);
    check(terminal.result && terminal.sent && terminal.result->outcome==ipc::Outcome::Complete &&
          terminal.result->nativeOutcome==1,"boundary native stimulus failed");
    owner->release(ids[0]);
    while(!owner->settled() && monotonicUs()<end)epicsThreadSleep(0.001);
    check(owner->settled(),"boundary stimulus retirement failed"); ++boundaryStimuli;
}
struct BoundaryStorage { uint8_t* data; size_t bytes; unsigned count; };
BoundaryStorage boundaryStorage(dbCommon* rec,unsigned slot)
{
    if(slot==0)return {reinterpret_cast<uint8_t*>(reinterpret_cast<stringinRecord*>(rec)->val),40,0};
    if(slot==1) { auto& r=*reinterpret_cast<lsiRecord*>(rec); return {reinterpret_cast<uint8_t*>(r.val),r.sizv,r.len}; }
    auto& r=*reinterpret_cast<waveformRecord*>(rec);
    check(r.ftvl==menuFtypeUCHAR || r.ftvl==menuFtypeULONG,"boundary waveform storage unsupported");
    return {static_cast<uint8_t*>(r.bptr),r.nelm*(r.ftvl==menuFtypeULONG?sizeof(epicsUInt32):1),r.nord};
}
void boundaryInput(unsigned row,const char* tag,const Value& value,const std::string& text,unsigned slot,const char* sample)
{
    const auto name=boundaryName(row,std::string(tag)+(slot==0?"String":slot==1?"Lsi":"Wave"));
    auto* rec=record(name.c_str()); auto* context=static_cast<RecordContext*>(rec->dpvt);
    check(context!=nullptr,"boundary input not attached");
    const bool octets=value.type()==ValueType::Octets,oid=value.type()==ValueType::ObjectId;
    const auto& config=BoundaryRows[row];
    const size_t count=octets?value.octets().size():oid?value.objectId().size():4;
    const size_t capacity=octets?config[0]:oid?(slot==2?config[3]:128):4;
    const size_t storage=slot==0?40:slot==1?config[1]:octets?config[2]:oid?config[4]:config[5];
    const bool accepted=count<=capacity && (slot==2?count<=storage:
        text.size()<storage && text.find('\0')==std::string::npos);
    std::vector<uint8_t> before; bool undefined=false,native=false; unsigned oldCount=0;
    { RecordLock lock(rec); auto image=boundaryStorage(rec,slot); before.assign(image.data,image.data+image.bytes);
      undefined=rec->udf; native=context->nativeSuccess; oldCount=image.count; }
    process(name.c_str(),accepted); ++boundaryGets;
    { RecordLock lock(rec); auto image=boundaryStorage(rec,slot);
      check(context->published==accepted,"boundary input publication mismatch");
      if(accepted) {
          check(!rec->udf && context->nativeSuccess,"boundary valid input state absent");
          if(slot<2) {
              check(std::memcmp(image.data,text.c_str(),text.size()+1)==0,"boundary formatted text mismatch");
              if(slot==1)check(image.count==text.size()+1,"boundary text LEN mismatch");
          } else {
              const auto ip=value.type()==ValueType::IpAddress?value.ipAddress():std::array<uint8_t,4>{{0,0,0,0}};
              const void* expected=octets?static_cast<const void*>(value.octets().data()):
                                   oid?static_cast<const void*>(value.objectId().data()):ip.data();
              const size_t bytes=count*(oid?sizeof(uint32_t):1);
              check(image.count==count && (!bytes || std::memcmp(image.data,expected,bytes)==0),"boundary binary array mismatch");
              check(std::memcmp(image.data+bytes,before.data()+bytes,image.bytes-bytes)==0,"boundary array overwrote tail");
          }
      } else {
          check(rec->stat==READ_ALARM && rec->sevr==INVALID_ALARM,"boundary input rejection alarm mismatch");
          check(std::memcmp(image.data,before.data(),image.bytes)==0 && image.count==oldCount,"boundary input partially published");
          check(context->nativeSuccess==native && rec->udf==(slot==2?false:undefined),"boundary input validity changed on rejection");
      }
    }
    std::printf("{\"event\":\"boundary_input\",\"row\":%u,\"tag\":\"%s\",\"slot\":%u,\"sample\":\"%s\",\"count\":%zu,\"text_bytes\":%zu,\"accepted\":%s,\"prior_native_success\":%s}\n",
                row,tag,slot,sample,count,text.size(),accepted?"true":"false",native?"true":"false");
}
std::vector<uint8_t> boundaryReadback(unsigned row)
{
    const auto name=boundaryName(row,"Readback"); process(name.c_str()); ++boundaryGets;
    RecordLock lock(record(name.c_str())); auto& r=*reinterpret_cast<waveformRecord*>(lock.value);
    auto* p=static_cast<uint8_t*>(r.bptr); return std::vector<uint8_t>(p,p+r.nord);
}
void boundaryOutput(unsigned row,bool longString,size_t length,const char* mode)
{
    const auto name=boundaryName(row,longString?"Lso":"Stringout"); auto* rec=record(name.c_str());
    auto* context=static_cast<RecordContext*>(rec->dpvt); check(context!=nullptr,"boundary output not attached");
    const size_t storage=longString?BoundaryRows[row][1]:40;
    const bool client=std::strcmp(mode,"client")==0,valid=std::strcmp(mode,"valid")==0 || client;
    const size_t captured=client?storage-1:length;
    const bool accepted=valid && captured<=BoundaryRows[row][0];
    boundaryStimulus(0,Value::octets({'s','e','e','d'}));
    const std::vector<uint8_t> prior={'s','e','e','d'};
    std::vector<uint8_t> requested(storage,0); std::fill(requested.begin(),requested.begin()+std::min(length,storage),'Q');
    if(client)requested.back()=0;
    unsigned requestedLen=unsigned(captured+1);
    if(std::strcmp(mode,"zero-len")==0)requestedLen=0;
    if(std::strcmp(mode,"mismatch-len")==0)requestedLen=2;
    if(std::strcmp(mode,"unterminated")==0)requestedLen=unsigned(storage);
    const auto identity=context->identity; const auto completions=Requests::instance().snapshot().completions;
    if(client) {
        std::vector<char> clientValue(storage+2,'Q'); clientValue.back()=0;
        DBADDR address{}; check(dbNameToAddr((name+".VAL$").c_str(),&address)==0,"boundary client field absent");
        const long status=dbPutField(&address,DBR_CHAR,clientValue.data(),long(clientValue.size()));
        check(status==(accepted?0:-1),"boundary client capture return mismatch");
        const auto end=ipc::add(monotonicUs(),8000000); bool idle=false;
        do { { RecordLock lock(rec); idle=!rec->pact; } if(idle && context->owner->settled())break;
             epicsThreadSleep(0.001); } while(monotonicUs()<end);
        check(idle && context->owner->settled(),"boundary client write did not settle");
    } else {
        { RecordLock lock(rec);
          if(longString) { auto& r=*reinterpret_cast<lsoRecord*>(rec); std::memcpy(r.val,requested.data(),storage); r.len=requestedLen; }
          else std::memcpy(reinterpret_cast<stringoutRecord*>(rec)->val,requested.data(),storage);
          rec->udf=FALSE; }
        process(name.c_str(),accepted);
    }
    { RecordLock lock(rec);
      const char* actual=longString?reinterpret_cast<lsoRecord*>(rec)->val:reinterpret_cast<stringoutRecord*>(rec)->val;
      check(std::memcmp(actual,requested.data(),storage)==0,"boundary SET changed requested storage");
      if(longString)check(reinterpret_cast<lsoRecord*>(rec)->len==requestedLen,"boundary SET changed requested LEN");
      check(!rec->pact && !context->active && !context->terminal.result,"boundary SET retained terminal");
      check((rec->sevr==NO_ALARM)==accepted,"boundary SET alarm mismatch");
      if(!accepted)check(rec->stat==WRITE_ALARM && rec->sevr==INVALID_ALARM && context->identity==identity,
                         "boundary invalid SET admitted or wrong alarm");
      else check(context->identity.generation==identity.generation+1,"boundary SET missing exact generation");
    }
    check(Requests::instance().snapshot().completions==completions+(accepted?1:0),"boundary SET completion count mismatch");
    const auto observed=boundaryReadback(row);
    check(observed==(accepted?std::vector<uint8_t>(captured,'Q'):prior),"boundary SET native readback mismatch");
    if(accepted)++boundarySets; else ++boundaryRejected;
    std::printf("{\"event\":\"boundary_output\",\"row\":%u,\"long\":%s,\"mode\":\"%s\",\"length\":%zu,\"captured\":%zu,\"accepted\":%s,\"readback\":true}\n",
                row,longString?"true":"false",mode,length,captured,accepted?"true":"false");
}
void boundaryMatrix()
{
    check(Requests::instance().snapshot().contexts==96,"boundary record inventory mismatch");
    for(unsigned row=0;row<7;++row) {
        for(unsigned sample=0;sample<19;++sample) {
            std::vector<uint8_t> bytes;
            if(sample<17)bytes.assign(BoundaryLengths[sample],'A');
            else bytes=sample==17?std::vector<uint8_t>{'A',0,'B',255}:std::vector<uint8_t>{128,255};
            boundaryStimulus(0,Value::octets({'s','e','e','d'}));
            for(unsigned slot=0;slot<3;++slot)boundaryInput(row,"Octets",Value::octets({'s','e','e','d'}),"seed",slot,"seed");
            boundaryStimulus(0,Value::octets(bytes));
            const std::string text(bytes.begin(),bytes.end());
            const auto label=sample<17?std::to_string(bytes.size()):sample==17?"nul":"high";
            for(unsigned slot=0;slot<3;++slot)boundaryInput(row,"Octets",Value::octets(bytes),text,slot,label.c_str());
        }
        for(unsigned size:{6u,7u,8u,20u,21u}) {
            boundaryStimulus(1,Value::objectId({1,3}));
            for(unsigned slot=0;slot<3;++slot)boundaryInput(row,"Oid",Value::objectId({1,3}),"1.3",slot,"seed");
            std::vector<uint32_t> arcs(size==21?20:size,1); arcs[1]=3; if(size==21)arcs.back()=10;
            std::string text; for(auto arc:arcs)text+=(text.empty()?"":".")+std::to_string(arc);
            boundaryStimulus(1,Value::objectId(arcs));
            for(unsigned slot=0;slot<3;++slot)boundaryInput(row,"Oid",Value::objectId(arcs),text,slot,std::to_string(size).c_str());
        }
        for(unsigned high=0;high<2;++high) {
            const auto ip=high?Value::ipAddress({{255,255,255,255}}):Value::ipAddress({{0,0,0,0}});
            boundaryStimulus(2,ip);
            for(unsigned slot=0;slot<3;++slot)boundaryInput(row,"Ip",ip,high?"255.255.255.255":"0.0.0.0",slot,high?"high":"zero");
        }
        for(bool longString:{false,true}) {
            const size_t storage=longString?BoundaryRows[row][1]:40;
            for(size_t length:BoundaryLengths)if(length<storage)boundaryOutput(row,longString,length,"valid");
            boundaryOutput(row,longString,storage,"unterminated");
            if(longString) {
                boundaryOutput(row,true,4,"zero-len"); boundaryOutput(row,true,4,"mismatch-len");
                boundaryOutput(row,true,storage+2,"client");
            }
        }
    }
    std::printf("{\"event\":\"boundary_summary\",\"rows\":7,\"stimuli\":%u,\"gets\":%u,\"sets\":%u,\"rejected\":%u}\n",
                boundaryStimuli,boundaryGets,boundarySets,boundaryRejected);
}

const char* NumericTags[]={"Integer","Unsigned32","Counter32","Gauge32","TimeTicks","Counter64","OpaqueFloat","OpaqueDouble"};
const char* NumericInputs[]={"Ai","Longin","Int64in","WaveLONG","WaveULONG","WaveINT64","WaveUINT64","WaveFLOAT","WaveDOUBLE"};
const ValueType NumericTypes[]={ValueType::Integer,ValueType::Unsigned32,ValueType::Counter32,ValueType::Gauge32,
                                ValueType::TimeTicks,ValueType::Counter64,ValueType::OpaqueFloat,ValueType::OpaqueDouble};
unsigned numericAcceptedOutputs=0,numericRejectedOutputs=0,numericStimuli=0,numericFailedAfterGood=0;
std::string numericName(const char* tag,const char* suffix)
{ return std::string("Records_Matrix")+tag+suffix; }
double numericDouble(const Value& value)
{
    if(value.type()==ValueType::Integer)return double(value.integer());
    if(value.type()==ValueType::Counter64)return double(value.counter64());
    if(integerType(value.type()))return double(value.unsigned32());
    return value.type()==ValueType::OpaqueFloat?double(value.opaqueFloat()):value.opaqueDouble();
}
int64_t numericSigned(const Value& value)
{
    if(value.type()==ValueType::Integer)return value.integer();
    if(value.type()==ValueType::Counter64)return int64_t(value.counter64());
    if(integerType(value.type()))return value.unsigned32();
    return int64_t(numericDouble(value));
}
uint64_t numericUnsigned(const Value& value)
{
    if(value.type()==ValueType::Counter64)return value.counter64();
    if(value.type()==ValueType::Integer)return uint64_t(value.integer());
    if(integerType(value.type()))return value.unsigned32();
    return uint64_t(numericDouble(value));
}
template<typename Number> bool numericBytes(const void* actual,Number expected)
{ return std::memcmp(actual,&expected,sizeof(expected))==0; }
size_t numericStorage(unsigned slot)
{ return slot==0 || slot==2 || slot==5 || slot==6 || slot==8?8:4; }
void* numericData(dbCommon* rec,unsigned slot)
{
    if(slot==0)return &reinterpret_cast<aiRecord*>(rec)->val;
    if(slot==1)return &reinterpret_cast<longinRecord*>(rec)->val;
    if(slot==2)return &reinterpret_cast<int64inRecord*>(rec)->val;
    return reinterpret_cast<waveformRecord*>(rec)->bptr;
}
void numericExpected(dbCommon* rec,unsigned slot,const Value& expected)
{
    bool match=false;
    switch(slot) {
    case 0: case 8: match=numericBytes(numericData(rec,slot),numericDouble(expected)); break;
    case 1: case 3: match=numericBytes(numericData(rec,slot),epicsInt32(numericSigned(expected))); break;
    case 2: case 5: match=numericBytes(numericData(rec,slot),epicsInt64(numericSigned(expected))); break;
    case 4: match=numericBytes(numericData(rec,slot),epicsUInt32(numericUnsigned(expected))); break;
    case 6: match=numericBytes(numericData(rec,slot),epicsUInt64(numericUnsigned(expected))); break;
    case 7: match=numericBytes(numericData(rec,slot),float(numericDouble(expected))); break;
    }
    check(match,(std::string("numeric native readback bits mismatch: ")+rec->name).c_str());
    if(slot==2 || slot==5 || slot==6) {
        DBADDR address{}; long count=1;
        check(dbNameToAddr((std::string(rec->name)+".VAL").c_str(),&address)==0,"numeric DBR read address absent");
        if(slot==6) {
            epicsUInt64 actual=0;
            check(dbGetField(&address,DBR_UINT64,&actual,nullptr,&count,nullptr)==0 && count==1 &&
                  actual==numericUnsigned(expected),"numeric DBR_UINT64 read lost exact value");
            std::printf("{\"event\":\"numeric_db_read\",\"record\":\"%s\",\"type\":\"UINT64\",\"value\":\"%llu\"}\n",
                        rec->name,(unsigned long long)actual);
        } else {
            epicsInt64 actual=0;
            check(dbGetField(&address,DBR_INT64,&actual,nullptr,&count,nullptr)==0 && count==1 &&
                  actual==numericSigned(expected),"numeric DBR_INT64 read lost exact value");
            std::printf("{\"event\":\"numeric_db_read\",\"record\":\"%s\",\"type\":\"INT64\",\"value\":\"%lld\"}\n",
                        rec->name,(long long)actual);
        }
    }
}
void numericInput(const char* tag,unsigned slot,const Value& expected,unsigned acceptedMask,
                  const char* sample,bool fixed=false)
{
    const auto name=numericName(tag,NumericInputs[slot]);
    auto* rec=record(name.c_str());
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    check(context!=nullptr,"numeric input fixture has no binding");
    const bool accepted=(acceptedMask&(1u<<slot))!=0;
    std::vector<uint8_t> previous;
    bool undefined=false,nativeSuccess=false;
    epicsUInt32 nord=0;
    {
        RecordLock lock(rec);
        const auto* data=static_cast<const uint8_t*>(numericData(rec,slot));
        const size_t bytes=numericStorage(slot)*(slot>=3?2:1);
        previous.assign(data,data+bytes);
        undefined=rec->udf; nativeSuccess=context->nativeSuccess;
        if(slot>=3)nord=reinterpret_cast<waveformRecord*>(rec)->nord;
    }
    process(name.c_str(),accepted);
    {
        RecordLock lock(rec);
        check(context->published==accepted,"numeric publication flag disagrees with declared policy");
        if(accepted) {
            numericExpected(rec,slot,expected);
            check(!rec->udf && context->nativeSuccess,"numeric success did not establish valid native input");
            if(slot>=3) {
                const auto& waveform=*reinterpret_cast<waveformRecord*>(rec);
                check(waveform.nord==1 && !waveform.busy,"numeric waveform length or BUSY mismatch");
                check(std::memcmp(static_cast<const uint8_t*>(waveform.bptr)+numericStorage(slot),
                                  previous.data()+numericStorage(slot),numericStorage(slot))==0,
                      "numeric waveform wrote beyond its single native value");
            }
        } else {
            check(rec->stat==READ_ALARM && rec->sevr==INVALID_ALARM,"numeric rejection alarm mismatch");
            check(std::memcmp(numericData(rec,slot),previous.data(),previous.size())==0,
                  "numeric rejection replaced prior value bytes");
            check(context->nativeSuccess==nativeSuccess,"numeric rejection changed native success state");
            check(rec->udf==(slot>=3?false:undefined),"numeric failure violated Base UDF behavior");
            if(slot>=3)check(reinterpret_cast<waveformRecord*>(rec)->nord==nord,"numeric rejection changed prior NORD");
            if(nativeSuccess)++numericFailedAfterGood;
        }
    }
    std::printf("{\"event\":\"numeric_input\",\"record\":\"%s\",\"sample\":\"%s\",\"accepted\":%s,\"fixed\":%s,\"prior_native_success\":%s,\"preserved_on_rejection\":%s}\n",
                name.c_str(),sample,accepted?"true":"false",fixed?"true":"false",nativeSuccess?"true":"false",accepted?"false":"true");
}
void numericAllInputs(const char* tag,const Value& expected,unsigned mask,const char* sample,bool fixed=false)
{
    for(unsigned slot=0;slot<9;++slot) {
        if(!integerType(expected.type()) && (slot==1 || slot==2))continue;
        numericInput(tag,slot,expected,mask,sample,fixed);
    }
}
const char* numericReadback(unsigned tag)
{ return tag==0?"WaveINT64":tag<6?"WaveUINT64":tag==6?"WaveFLOAT":"WaveDOUBLE"; }
unsigned numericReadbackSlot(unsigned tag)
{ return tag==0?5:tag<6?6:tag==6?7:8; }
void numericSet(unsigned tag,const char* output,const Value& requested,const Value& wire,bool accepted,
                const char* sample,bool stimulus=false)
{
    const auto name=numericName(NumericTags[tag],output);
    auto* rec=record(name.c_str());
    auto* context=static_cast<RecordContext*>(rec->dpvt);
    check(context!=nullptr,"numeric output fixture has no binding");
    const auto identity=context->identity;
    const auto completions=Requests::instance().snapshot().completions;
    const auto readback=numericName(NumericTags[tag],numericReadback(tag));
    std::vector<uint8_t> prior;
    if(!accepted) {
        process(readback.c_str());
        RecordLock lock(record(readback.c_str()));
        const auto* data=static_cast<const uint8_t*>(numericData(lock.value,numericReadbackSlot(tag)));
        prior.assign(data,data+numericStorage(numericReadbackSlot(tag)));
    }
    {
        RecordLock lock(rec);
        rec->udf=FALSE;
        if(!std::strcmp(output,"Ao"))reinterpret_cast<aoRecord*>(rec)->val=numericDouble(requested);
        else if(!std::strcmp(output,"Longout"))reinterpret_cast<longoutRecord*>(rec)->val=epicsInt32(requested.integer());
        else {
            DBADDR address{}; const epicsInt64 value=requested.integer();
            check(dbNameToAddr((name+".VAL").c_str(),&address)==0 && dbPut(&address,DBR_INT64,&value,1)==0,
                  "numeric DBR_INT64 write failed");
            std::printf("{\"event\":\"numeric_db_write\",\"record\":\"%s\",\"value\":\"%lld\"}\n",
                        rec->name,(long long)value);
        }
    }
    process(name.c_str(),accepted);
    {
        RecordLock lock(rec);
        if(!std::strcmp(output,"Ao"))check(numericBytes(&reinterpret_cast<aoRecord*>(rec)->val,numericDouble(requested)),
                                         "numeric ao changed its requested value");
        else if(!std::strcmp(output,"Longout"))check(reinterpret_cast<longoutRecord*>(rec)->val==requested.integer(),
                                                  "numeric longout changed its requested value");
        else check(reinterpret_cast<int64outRecord*>(rec)->val==requested.integer(),"numeric int64out changed its requested value");
        if(!accepted) {
            check(context->identity==identity && !context->active && !rec->pact,"invalid numeric SET admitted native work");
            // Base UDF/INVALID has equal-severity priority for NaN before the DSET runs.
            check(rec->sevr==INVALID_ALARM &&
                  rec->stat==(std::isnan(numericDouble(requested))?UDF_ALARM:WRITE_ALARM),
                  "invalid numeric SET alarm mismatch");
        }
    }
    if(!accepted) {
        // The separate GET adds one completion, while the rejected output adds none.
        check(Requests::instance().snapshot().completions==completions+1,"rejected numeric SET created a completion");
        ++numericRejectedOutputs;
    } else if(stimulus)++numericStimuli; else ++numericAcceptedOutputs;
    process(readback.c_str());
    {
        RecordLock lock(record(readback.c_str()));
        if(accepted)numericExpected(lock.value,numericReadbackSlot(tag),wire);
        else check(std::memcmp(numericData(lock.value,numericReadbackSlot(tag)),prior.data(),prior.size())==0,
                   "rejected numeric SET changed actual native agent state");
    }
    if(!stimulus)std::printf("{\"event\":\"numeric_output\",\"record\":\"%s\",\"sample\":\"%s\",\"accepted\":%s,\"separate_native_readback\":true}\n",
                            name.c_str(),sample,accepted?"true":"false");
}
struct NumericSample {
    const char* name;
    Value value;
    unsigned mask;
};
Value numericInteger(unsigned tag,int64_t value)
{
    if(tag==0)return Value::integer(value);
    if(tag==5)return Value::counter64(uint64_t(value));
    return Value::unsigned32(NumericTypes[tag],uint32_t(value));
}
void numericMatrix()
{
    check(std::fegetround()==FE_TONEAREST,"numeric fixture requires default nearest thread rounding");
    check(Requests::instance().snapshot().contexts==160,"numeric binding inventory mismatch");
    for(unsigned tag=0;tag<8;++tag) {
        std::vector<NumericSample> samples;
        // Masks name the independently declared accepted slots: ai, longin, int64in,
        // waveform LONG/ULONG/INT64/UINT64/FLOAT/DOUBLE, in that order.
        if(tag==0) {
            samples={{"zero",Value::integer(0),511},{"INT32_MIN",Value::integer(INT32_MIN),431},
                     {"negative_float_precision",Value::integer(-16777217),303},{"negative_one",Value::integer(-1),431},
                     {"float_exact_boundary",Value::integer(16777216),511},
                     {"float_loss_boundary",Value::integer(16777217),383},
                     {"INT32_MAX",Value::integer(INT32_MAX),383}};
        } else if(tag<5) {
            samples={{"zero",numericInteger(tag,0),511},{"one",numericInteger(tag,1),511},
                     {"float_exact_boundary",numericInteger(tag,16777216),511},
                     {"float_loss_boundary",numericInteger(tag,16777217),383},
                     {"INT32_MAX",numericInteger(tag,INT32_MAX),383},
                     {"INT32_MAX_plus_one",numericInteger(tag,INT64_C(2147483648)),501},
                     {"UINT32_MAX",numericInteger(tag,UINT32_MAX),373}};
        } else if(tag==5) {
            samples={{"zero",Value::counter64(0),511},{"one",Value::counter64(1),511},
                     {"float_exact_boundary",Value::counter64(16777216),511},
                     {"float_loss_boundary",Value::counter64(16777217),383},
                     {"UINT32_MAX",Value::counter64(UINT32_MAX),373},
                     {"UINT32_MAX_plus_one",Value::counter64(UINT64_C(4294967296)),485},
                     {"double_exact_boundary",Value::counter64(UINT64_C(9007199254740992)),485},
                     {"double_loss_boundary",Value::counter64(UINT64_C(9007199254740993)),100},
                     {"INT64_MAX",Value::counter64(INT64_MAX),100}};
        } else if(tag==6) {
            samples={{"zero",Value::opaqueFloat(0),511},
                     {"negative_max",Value::opaqueFloat(-std::numeric_limits<float>::max()),385},
                     {"negative_min_subnormal",Value::opaqueFloat(-std::numeric_limits<float>::denorm_min()),385},
                     {"negative_zero",Value::opaqueFloat(-0.0f),511},
                     {"min_subnormal",Value::opaqueFloat(std::numeric_limits<float>::denorm_min()),385},
                     {"fractional",Value::opaqueFloat(1.5f),385},
                     {"float_exact_boundary",Value::opaqueFloat(16777216),511},
                     {"INT32_MAX_plus_one",Value::opaqueFloat(2147483648.0f),501},
                     {"max",Value::opaqueFloat(std::numeric_limits<float>::max()),385}};
        } else {
            samples={{"zero",Value::opaqueDouble(0),511},
                     {"negative_max",Value::opaqueDouble(-std::numeric_limits<double>::max()),257},
                     {"INT64_MIN",Value::opaqueDouble(-std::ldexp(1.0,63)),417},
                     {"INT32_MIN",Value::opaqueDouble(INT32_MIN),431},
                     {"negative_zero",Value::opaqueDouble(-0.0),511},
                     {"min_subnormal",Value::opaqueDouble(std::numeric_limits<double>::denorm_min()),257},
                     {"fractional_exact_float",Value::opaqueDouble(1.5),385},
                     {"fractional_inexact_float",Value::opaqueDouble(1.0/3.0),257},
                     {"float_loss_boundary",Value::opaqueDouble(16777217),383},
                     {"UINT32_MAX_plus_one",Value::opaqueDouble(std::ldexp(1.0,32)),485},
                     {"double_exact_boundary",Value::opaqueDouble(std::ldexp(1.0,53)),485},
                     {"INT64_MAX_plus_one",Value::opaqueDouble(std::ldexp(1.0,63)),449},
                     {"UINT64_MAX_plus_one",Value::opaqueDouble(std::ldexp(1.0,64)),385},
                     {"max",Value::opaqueDouble(std::numeric_limits<double>::max()),257}};
        }
        for(const auto& sample:samples) {
            const auto requested=tag<6?Value::integer(numericSigned(sample.value)):sample.value;
            numericSet(tag,tag<6?"Int64out":"Ao",requested,sample.value,true,sample.name,true);
            numericAllInputs(NumericTags[tag],sample.value,sample.mask,sample.name);
        }
    }
    for(const char* tag:{"WideHigh","WideMax"}) {
        responseMode("numeric-seed");
        numericAllInputs(tag,Value::counter64(7),511,"fixed_seed",true);
        responseMode("pass");
        const bool high=!std::strcmp(tag,"WideHigh");
        numericAllInputs(tag,Value::counter64(high?UINT64_C(9223372036854775808):UINT64_MAX),
                         high?449:64,high?"INT64_MAX_plus_one":"UINT64_MAX",true);
    }
    const char* nonfiniteTags[]={"FloatNaN","FloatInfinity","FloatNegativeInfinity","DoubleNaN","DoubleInfinity","DoubleNegativeInfinity"};
    for(unsigned index=0;index<6;++index) {
        const double value=index%3==0?std::numeric_limits<double>::quiet_NaN():
                           index%3==1?std::numeric_limits<double>::infinity():-std::numeric_limits<double>::infinity();
        const auto expected=index<3?Value::opaqueFloat(float(value)):Value::opaqueDouble(value);
        responseMode("numeric-seed");
        numericAllInputs(nonfiniteTags[index],index<3?Value::opaqueFloat(7):Value::opaqueDouble(7),511,"fixed_seed",true);
        responseMode("pass");
        numericAllInputs(nonfiniteTags[index],expected,0,"nonfinite",true);
    }
    for(unsigned tag=0;tag<6;++tag) {
        for(const char* output:{"Longout","Int64out"}) {
            for(int64_t value:{int64_t(INT32_MIN),int64_t(-1),int64_t(0),int64_t(INT32_MAX)}) {
                const bool accepted=tag==0 || value>=0;
                numericSet(tag,output,Value::integer(value),accepted?numericInteger(tag,value):numericInteger(tag,0),
                           accepted,"signed32_target_boundary");
            }
            if(!std::strcmp(output,"Int64out")) {
                for(int64_t value:{int64_t(INT32_MAX)+1,int64_t(UINT32_MAX),int64_t(UINT32_MAX)+1,
                                   INT64_C(9007199254740993),int64_t(INT64_MAX),int64_t(INT64_MIN)}) {
                    const bool accepted=tag==0?(value>=INT32_MIN && value<=INT32_MAX):
                                        tag==5?value>=0:value>=0 && uint64_t(value)<=UINT32_MAX;
                    numericSet(tag,output,Value::integer(value),accepted?numericInteger(tag,value):numericInteger(tag,0),
                               accepted,"signed64_target_boundary");
                }
            }
        }
        const double max=tag==0?double(INT32_MAX):tag==5?std::ldexp(1.0,64)-2048:double(UINT32_MAX);
        for(const double value:{0.0,-1.0,1.5,max,max+(tag==5?2048:1.0)}) {
            const bool accepted=value==0 || (tag==0 && value==-1) || value==max;
            const auto wire=accepted?(tag==5?Value::counter64(uint64_t(value)):numericInteger(tag,int64_t(value))):numericInteger(tag,0);
            numericSet(tag,"Ao",Value::opaqueDouble(value),wire,accepted,"floating_to_integer_boundary");
        }
    }
    for(unsigned tag=6;tag<8;++tag) {
        const double max=tag==6?double(std::numeric_limits<float>::max()):std::numeric_limits<double>::max();
        const double subnormal=tag==6?double(std::numeric_limits<float>::denorm_min()):std::numeric_limits<double>::denorm_min();
        for(const double value:{-max,-subnormal,-0.0,0.0,subnormal,1.0/3.0,max}) {
            const auto wire=tag==6?Value::opaqueFloat(float(value)):Value::opaqueDouble(value);
            numericSet(tag,"Ao",Value::opaqueDouble(value),wire,true,"finite_floating_boundary");
        }
        if(tag==6)numericSet(tag,"Ao",Value::opaqueDouble(std::numeric_limits<double>::max()),
                            Value::opaqueFloat(0),false,"float_overflow");
        for(const double value:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity()})
            numericSet(tag,"Ao",Value::opaqueDouble(value),tag==6?Value::opaqueFloat(0):Value::opaqueDouble(0),false,"nonfinite");
    }
    std::printf("{\"event\":\"numeric_summary\",\"get_pairs\":68,\"waveform_pairs\":48,\"set_pairs\":20,\"fixed_pairs\":60,"
                "\"accepted_outputs\":%u,\"rejected_outputs\":%u,\"stimuli\":%u,\"failed_after_good\":%u}\n",
                numericAcceptedOutputs,numericRejectedOutputs,numericStimuli,numericFailedAfterGood);
}
// After an isolated cleanup of an activation whose stop left a retirement-pending predecessor and a queued
// successor, the same process builds a second activation: a new scheduler and worker serve a complete
// record matrix, and the first activation's worker is gone.
void secondActivation(char** argv)
{
    const auto first=Runtime::instance().snapshot();
    testdbPrepare(); testdbReadDatabase(argv[4],nullptr,nullptr);
    check(snmp3RecordTest_registerRecordDeviceDriver(pdbbase)==0,"second record registrar failed");
    command("snmp3WorkerPath("+quoted(argv[3])+")");
    testdbReadDatabase(argv[5],nullptr,"P=Records_");
    testdbReadDatabase(argv[6],nullptr,"P=Records_");
    check(Requests::instance().snapshot().contexts==0,"second load performed device initialization");
    Runtime::instance().takeSupervisionEvents();
    testIocInitOk();
    const auto second=Runtime::instance().snapshot();
    check(second.activation==first.activation+1 && second.state==State::Running && second.admission,
          "second activation did not start");
    baseline();
    std::vector<SupervisionEvent> events=Runtime::instance().takeSupervisionEvents();
    int64_t worker=0;
    for(const auto& e:events)if(e.code==13 && e.pid>0) { worker=e.pid; break; }
    check(worker>0 && worker!=firstActivationWorker,"second activation did not use a new worker");
    const bool firstGone=::kill(pid_t(firstActivationWorker),0)!=0 && errno==ESRCH;
    check(firstGone,"first activation worker was not reaped");
    std::printf("{\"event\":\"rebuild\",\"first_activation\":%lu,\"second_activation\":%lu,\"first_worker\":%lld,"
                "\"second_worker\":%lld,\"first_worker_gone\":%s,\"second_state\":%d}\n",
                first.activation,second.activation,(long long)firstActivationWorker,(long long)worker,
                firstGone?"true":"false",int(second.state));
    Runtime::instance().report();
    testIocShutdownOk();
    check(Requests::instance().snapshot().contexts==0,"second isolated queue cleanup retained detached contexts");
    check(Runtime::instance().snapshot().state==State::Stopped,"second activation stop outcome mismatch");
    testdbCleanup();
}
}

int main(int argc,char** argv)
{
    if(argc!=9)return 2;
    setvbuf(stdout,nullptr,_IOLBF,0);
    try {
        testPlan(0); check(callbackSetQueueSize(8)==0,"callback queue size setup failed");
        testdbPrepare(); testdbReadDatabase(argv[4],nullptr,nullptr);
        check(snmp3RecordTest_registerRecordDeviceDriver(pdbbase)==0,"record registrar failed");
        command("snmp3Load("+quoted(argv[1])+","+quoted(argv[2])+")");
        command("snmp3WorkerPath("+quoted(argv[3])+")");
        Qualification qualification; qualification.stderrPath=argv[7];
#ifdef __SANITIZE_ADDRESS__
        qualification.sanitizers=true;
#endif
        Runtime::instance().setQualification(qualification);
        testdbReadDatabase(argv[5],nullptr,"P=Records_");
        testdbReadDatabase(argv[6],nullptr,"P=Records_");
        edges=std::strcmp(argv[8],"edges")==0;
        alarms=std::strcmp(argv[8],"alarms")==0;
        active=std::strcmp(argv[8],"active")==0;
        policy=std::strcmp(argv[8],"policy")==0;
        numeric=std::strcmp(argv[8],"numeric")==0;
        activeUnforced=std::strcmp(argv[8],"active-unforced")==0;
        deadlineQueue=std::strcmp(argv[8],"deadline-queue")==0;
        nearDeadline=std::strcmp(argv[8],"near-deadline")==0;
        accounting=std::strcmp(argv[8],"accounting")==0;
        stopQueued=std::strcmp(argv[8],"stop-queued")==0;
        rebuild=std::strcmp(argv[8],"rebuild")==0;
        stopInflight=std::strcmp(argv[8],"stop-inflight")==0;
        stopEnqueueFailed=std::strcmp(argv[8],"stop-enqueue-failed")==0;
        stopDownstream=std::strcmp(argv[8],"stop-downstream")==0;
        liveDetach=std::strcmp(argv[8],"live-detach")==0;
        repeatDetach=std::strcmp(argv[8],"repeat-detach")==0;
        liveDtype=std::strcmp(argv[8],"live-dtype")==0;
        supportTransition=std::strcmp(argv[8],"support-transition")==0;
        scanProc=std::strcmp(argv[8],"scan-proc")==0;
        alarmFlow=std::strcmp(argv[8],"alarm-flow")==0;
        chainFlow=std::strcmp(argv[8],"chain-flow")==0;
        queuedSimm=std::strcmp(argv[8],"queued-simm")==0;
        boundaries=std::strcmp(argv[8],"boundaries")==0;
        retirementFlow=std::strcmp(argv[8],"retirement-flow")==0;
        contract=std::strcmp(argv[8],"contract")==0;
        staleRecord=std::strcmp(argv[8],"stale-record")==0;
        finalClauses=std::strcmp(argv[8],"final-clauses")==0;
        if(finalClauses) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-final.db").c_str(),nullptr,"P=Records_");
        }
        if(staleRecord) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-stale.db").c_str(),nullptr,"P=Records_");
        }
        if(contract) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-contract.db").c_str(),nullptr,"P=Records_");
        }
        if(retirementFlow) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            const char* mode=std::getenv("SNMP3_RETIREMENT_MODE"); check(mode!=nullptr,"retirement mode absent");
            testdbReadDatabase((root+"record-retirement.db").c_str(),nullptr,
                std::strcmp(mode,"deadline")==0?"P=Records_,BUDGET=300":"P=Records_,BUDGET=5000");
        }
        if(boundaries) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            for(unsigned row=0;row<7;++row) {
                const auto& c=BoundaryRows[row];
                const std::string macros="P=Records_B"+std::to_string(row)+"_,ROW="+std::to_string(row)+
                    ",SIZV="+std::to_string(c[1])+",NELM="+std::to_string(c[2])+",OIDNELM="+std::to_string(c[4])+",IPNELM="+std::to_string(c[5]);
                testdbReadDatabase((root+"record-boundaries.db").c_str(),nullptr,macros.c_str());
            }
            boundaryHandles[0]=Runtime::instance().bind("BoundaryOctetsWrite");
            boundaryHandles[1]=Runtime::instance().bind("BoundaryOidWrite");
            boundaryHandles[2]=Runtime::instance().bind("BoundaryIpWrite");
        }
        if(chainFlow) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-chain.db").c_str(),nullptr,"P=Records_");
        }
        if(liveDtype || supportTransition) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+(supportTransition?"record-support.db":"record-dtype.db")).c_str(),nullptr,"P=Records_");
        }
        if(edges) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-edges.db").c_str(),nullptr,"P=Records_");
            testdbReadDatabase((root+"record-order.db").c_str(),nullptr,"P=Records_");
        }
        if(alarms || stopInflight || stopEnqueueFailed || stopDownstream || liveDetach || repeatDetach) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+(alarms?"record-alarms.db":"record-stop.db")).c_str(),nullptr,
                               stopDownstream?"P=Records_,FLNK=Records_StopExternal.PROC CA":"P=Records_");
        }
        if(active || activeUnforced || accounting || queuedSimm) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-active.db").c_str(),nullptr,"P=Records_");
        }
        if(queuedSimm) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            const char* mode=std::getenv("SNMP3_SIMM_MODE"); check(mode!=nullptr,"queued SIMM mode absent");
            const char* macros=std::strcmp(mode,"deadline")==0?"P=Records_,PATH=Deadline,BUDGET=300":"P=Records_,PATH=Active,BUDGET=5000";
            testdbReadDatabase((root+"record-queued-simm.db").c_str(),nullptr,macros);
        }
        if(policy) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-policy.db").c_str(),nullptr,"P=Records_");
        }
        if(deadlineQueue || nearDeadline || stopQueued || rebuild) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            const char* budget=std::getenv("SNMP3_RECORD_BUDGET_MS");
            const char* binding=std::getenv("SNMP3_RECORD_QUEUE_BINDING");
            check(budget && binding,"queue record budget or binding absent");
            const std::string macros=std::string("P=Records_,BUDGET=")+budget+",BINDING="+binding;
            testdbReadDatabase((root+"record-queue.db").c_str(),nullptr,macros.c_str());
        }
        check(Requests::instance().snapshot().contexts==0,"loading DB performed device initialization");
        if(numeric) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-numeric.db").c_str(),nullptr,"P=Records_");
        }
        testIocInitOk();
        if(edges)inputEdges();
        if(!deadlineQueue && !nearDeadline && !stopQueued && !rebuild && !stopInflight && !stopEnqueueFailed && !stopDownstream && !liveDetach && !repeatDetach && !liveDtype && !supportTransition && !scanProc && !alarmFlow && !chainFlow && !queuedSimm && !boundaries && !retirementFlow && !contract && !staleRecord && !finalClauses) { baseline(); pressure("Records_Longin"); pressure("Records_Ao"); }
        if(edges) { capacityAndOrder(); simulation(); outputEdges(); maxPayloadQueue(); }
        if(alarms)nativeTimeouts();
        if(active)activeOutputs();
        if(scanProc)scanAndProc();
        if(alarmFlow)alarmAfterSuccess();
        if(chainFlow)chainAndFanout();
        if(policy) { outputPolicies(); inputSourceSwitch(); }
        if(numeric)numericMatrix();
        if(boundaries)boundaryMatrix();
        if(retirementFlow)retirementTrial();
        if(contract)contractTrial();
        if(staleRecord)staleRecordTrial();
        if(finalClauses)finalClauseTrial();
        if(activeUnforced)activeUnforcedOutputs();
        if(queuedSimm)queuedSimmTrial();
        if(deadlineQueue)deadlineTrial(std::getenv("SNMP3_RECORD_TRIAL")?std::getenv("SNMP3_RECORD_TRIAL"):"unnamed");
        if(nearDeadline)nearDeadlineTrial();
        if(accounting)accountingTrials();
        if(stopQueued || rebuild)stopWithQueuedSuccessor();
        if(stopInflight)stopInFlight();
        if(stopEnqueueFailed)stopWithEnqueueFailures();
        if(stopDownstream)stopWithHeldDownstream();
        if(liveDetach)liveDetachCells();
        if(liveDtype)dtypeCells();
        if(supportTransition)supportCells();
        if(repeatDetach) { prepareRepeatDetach(); initHookRegister(repeatDetachHook); }
        Runtime::instance().report();
        const bool blocked=std::strcmp(argv[8],"shutdown")==0;
        const bool abandoned=std::strcmp(argv[8],"queued-shutdown")==0 ||
                             (stopEnqueueFailed && std::strcmp(std::getenv("SNMP3_RECORD_RELEASE"),"after")==0);
        const bool shutdownDone=liveDtype || supportTransition || (stopDownstream && std::strcmp(std::getenv("SNMP3_RECORD_RELEASE"),"shutdown")==0);
        if(blocked)blockedShutdown(); else if(abandoned && !stopEnqueueFailed)queuedShutdown(); else if(!shutdownDone)testIocShutdownOk();
        // Repeat-detach reports cleanup failures through named runner checks after real database cleanup.
        if(!repeatDetach && !liveDtype && !supportTransition) {
            check(Requests::instance().snapshot().contexts==0,"isolated queue cleanup retained detached contexts");
            check(Runtime::instance().snapshot().state==((blocked || abandoned)?State::IncompleteStopped:State::Stopped),"record runtime stop outcome mismatch");
        }
        testdbCleanup();
        if(liveDtype)
            std::printf("{\"event\":\"dtype_cleanup\",\"mode\":\"%s\",\"contexts\":%llu,\"state\":%d}\n",dtypeMode,
                        (unsigned long long)Requests::instance().snapshot().contexts,int(Runtime::instance().snapshot().state));
        if(supportTransition)
            std::printf("{\"event\":\"support_cleanup\",\"mode\":%s,\"contexts\":%llu,\"state\":%d}\n",jsonText(supportMode).c_str(),
                        (unsigned long long)Requests::instance().snapshot().contexts,int(Runtime::instance().snapshot().state));
        if(rebuild)secondActivation(argv);
        if(repeatDetach)
            std::printf("{\"event\":\"repeat_detach_cleanup\",\"contexts\":%llu,\"state\":%d}\n",
                        (unsigned long long)Requests::instance().snapshot().contexts,int(Runtime::instance().snapshot().state));
        if(stopEnqueueFailed)
            std::printf("{\"event\":\"stop_enqueue_failed_cleanup\",\"completions\":%llu,\"contexts\":%llu}\n",
                        (unsigned long long)Requests::instance().snapshot().completions,
                        (unsigned long long)Requests::instance().snapshot().contexts);
        check(testDone()==0,"Base test harness failed");
        std::printf("{\"event\":\"record_summary\",\"checks\":%u,\"records\":11}\n",checks);
        epicsExit(0);
    } catch(const std::exception& error) {
        std::fprintf(stderr,"record test failed after %u checks: %s\n",checks,error.what());
        Runtime::instance().stop(); epicsExit(1);
    }
    return 1;
}
