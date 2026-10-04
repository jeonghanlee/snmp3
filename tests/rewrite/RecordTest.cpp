#include "Request.h"
#include "Runtime.h"
#include "Config.h"
#include <alarm.h>
#include <dbAccess.h>
#include <dbBase.h>
#include <dbCommon.h>
#include <dbLock.h>
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
#include <cstdlib>

using namespace snmp3;
extern "C" int snmp3RecordTest_registerRecordDeviceDriver(dbBase*);
namespace {
unsigned checks=0;
bool edges=false;
bool alarms=false;
bool active=false;
bool policy=false;
bool numeric=false;
struct RecordLock {
    dbCommon* value;
    explicit RecordLock(dbCommon* record) : value(record) { dbScanLock(value); }
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
void process(const char* name,bool successful=true,bool cascade=false,uint16_t expectedNative=0,bool expired=false)
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
    if(expectedNative || expired) {
        const auto terminalEnd=ipc::add(monotonicUs(),8000000);
        while(!Requests::instance().snapshot().entered && monotonicUs()<terminalEnd)epicsThreadSleep(0.001);
        check(Requests::instance().snapshot().entered==1,"native terminal callback did not acquire record lease");
        check(context->terminal.result &&
              context->terminal.result->outcome==(expired?ipc::Outcome::Deadline:ipc::Outcome::NativeFailure) &&
              (expired || context->terminal.result->nativeOutcome==expectedNative),"unexpected native terminal classification");
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
    check(Requests::instance().snapshot().contexts==(edges?33u:alarms?24u:active?19u:policy?24u:numeric?160u:12u),"eleven DSET kinds and aliases were not initialized");
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
}

namespace {
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
        else reinterpret_cast<int64outRecord*>(rec)->val=requested.integer();
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
    numericAllInputs("WideHigh",Value::counter64(UINT64_C(9223372036854775808)),449,"INT64_MAX_plus_one",true);
    numericAllInputs("WideMax",Value::counter64(UINT64_MAX),64,"UINT64_MAX",true);
    const char* nonfiniteTags[]={"FloatNaN","FloatInfinity","FloatNegativeInfinity","DoubleNaN","DoubleInfinity","DoubleNegativeInfinity"};
    for(unsigned index=0;index<6;++index) {
        const double value=index%3==0?std::numeric_limits<double>::quiet_NaN():
                           index%3==1?std::numeric_limits<double>::infinity():-std::numeric_limits<double>::infinity();
        const auto expected=index<3?Value::opaqueFloat(float(value)):Value::opaqueDouble(value);
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
        if(edges) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-edges.db").c_str(),nullptr,"P=Records_");
            testdbReadDatabase((root+"record-order.db").c_str(),nullptr,"P=Records_");
        }
        if(alarms) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-alarms.db").c_str(),nullptr,"P=Records_");
        }
        if(active) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-active.db").c_str(),nullptr,"P=Records_");
        }
        if(policy) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-policy.db").c_str(),nullptr,"P=Records_");
        }
        check(Requests::instance().snapshot().contexts==0,"loading DB performed device initialization");
        if(numeric) {
            const std::string root=std::string(argv[5]).substr(0,std::string(argv[5]).find_last_of('/')+1);
            testdbReadDatabase((root+"record-numeric.db").c_str(),nullptr,"P=Records_");
        }
        testIocInitOk();
        if(edges)inputEdges();
        baseline(); pressure("Records_Longin"); pressure("Records_Ao");
        if(edges) { capacityAndOrder(); simulation(); outputEdges(); maxPayloadQueue(); }
        if(alarms)nativeTimeouts();
        if(active)activeOutputs();
        if(policy) { outputPolicies(); inputSourceSwitch(); }
        if(numeric)numericMatrix();
        Runtime::instance().report();
        const bool blocked=std::strcmp(argv[8],"shutdown")==0;
        const bool abandoned=std::strcmp(argv[8],"queued-shutdown")==0;
        if(blocked)blockedShutdown(); else if(abandoned)queuedShutdown(); else testIocShutdownOk();
        check(Requests::instance().snapshot().contexts==0,"isolated queue cleanup retained detached contexts");
        check(Runtime::instance().snapshot().state==((blocked || abandoned)?State::IncompleteStopped:State::Stopped),"record runtime stop outcome mismatch");
        testdbCleanup(); check(testDone()==0,"Base test harness failed");
        std::printf("{\"event\":\"record_summary\",\"checks\":%u,\"records\":11}\n",checks);
        epicsExit(0);
    } catch(const std::exception& error) {
        std::fprintf(stderr,"record test failed after %u checks: %s\n",checks,error.what());
        Runtime::instance().stop(); epicsExit(1);
    }
    return 1;
}
