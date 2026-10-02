#define USE_TYPED_DSET
#include "Request.h"
#include "Config.h"
#include <alarm.h>
#include <dbCommon.h>
#include <devSup.h>
#include <recGbl.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <int64inRecord.h>
#include <int64outRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <lsiRecord.h>
#include <lsoRecord.h>
#include <waveformRecord.h>
#include <menuConvert.h>
#include <menuFtype.h>
#include <epicsExport.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace snmp3;
namespace {
// Native result codes are fixed by the worker IPC contract.
const uint16_t NativeComplete=1,NativeTimeout=2,NativeOpenFailure=3,NativeSendFailure=4,NativeCancelled=10;
bool input(RecordKind kind) { return kind<=RecordKind::Waveform; }
void require(bool valid)
{ if(!valid)throw std::runtime_error("record configuration rejected"); }
template<typename Record> Record& as(dbCommon* record)
{ return *reinterpret_cast<Record*>(record); }
ScalarType scalar(unsigned ftvl)
{
    switch(ftvl) {
    case menuFtypeLONG: return ScalarType::Signed32;
    case menuFtypeULONG: return ScalarType::Unsigned32;
    case menuFtypeINT64: return ScalarType::Signed64;
    case menuFtypeUINT64: return ScalarType::Unsigned64;
    case menuFtypeFLOAT: return ScalarType::Float32;
    case menuFtypeDOUBLE: return ScalarType::Float64;
    default: throw std::runtime_error("waveform type rejected");
    }
}
bool validate(RecordContext& context)
{
    auto* record=context.record;
    if(!record)return false;
    auto* link=dbGetDevLink(record);
    if(!record || record->dpvt!=&context || record->dtyp!=context.dtype ||
       record->dset!=context.dset || record->rset!=context.rset || !link ||
       link->type!=INST_IO || !link->value.instio.string ||
       context.link!=link->value.instio.string)return false;
    switch(context.definition.kind) {
    case RecordKind::Ai: return as<aiRecord>(record).linr==menuConvertNO_CONVERSION;
    case RecordKind::Ao: return as<aoRecord>(record).linr==menuConvertNO_CONVERSION;
    case RecordKind::Longout: return as<longoutRecord>(record).oopt==longoutOOPT_Every_Time;
    case RecordKind::Lsi: {
        const auto& value=as<lsiRecord>(record);
        return value.sizv==context.definition.storageCapacity && value.val==context.storage;
    }
    case RecordKind::Lso: {
        const auto& value=as<lsoRecord>(record);
        return value.sizv==context.definition.storageCapacity && value.val==context.storage;
    }
    case RecordKind::Waveform: {
        const auto& value=as<waveformRecord>(record);
        return value.nelm==context.definition.storageCapacity && value.ftvl==context.storageType &&
               value.bptr==context.storage;
    }
    default: return true;
    }
}
void prepare(RecordContext& context,const ipc::Result& result)
{
    const auto& definition=context.definition;
    const auto& binding=definition.binding->definition();
    if(definition.kind==RecordKind::Waveform)as<waveformRecord>(context.record).busy=FALSE;
    if(result.outcome!=ipc::Outcome::Complete) {
        const bool transport=result.nativeOutcome==NativeTimeout || result.nativeOutcome==NativeOpenFailure ||
                             result.nativeOutcome==NativeSendFailure || result.nativeOutcome==NativeCancelled;
        context.alarm=result.outcome==ipc::Outcome::NativeFailure && !transport ?
                      (input(definition.kind)?READ_ALARM:WRITE_ALARM):COMM_ALARM;
        return;
    }
    if(result.nativeOutcome!=NativeComplete) { context.alarm=input(definition.kind)?READ_ALARM:WRITE_ALARM; return; }
    if(!input(definition.kind))return;
    ipc::Reader reader(result.value);
    require(reader.oid()==binding.oid);
    auto value=ipc::decodeValue(reader,binding,false); reader.end();
    switch(definition.kind) {
    case RecordKind::Ai:
    case RecordKind::Longin:
    case RecordKind::Int64in:
        value=convertScalar(value,definition.scalarType); break;
    case RecordKind::Stringin:
    case RecordKind::Lsi:
        context.text=convertText(value,definition.storageCapacity-1); break;
    case RecordKind::Waveform:
        if(numericType(binding.valueType))value=convertScalar(value,definition.scalarType);
        else {
            const size_t count=value.type()==ValueType::Octets ? value.octets().size():
                               value.type()==ValueType::ObjectId ? value.objectId().size():4;
            require(count<=definition.storageCapacity);
        }
        break;
    default: require(false);
    }
    context.staged.reset(new Value(std::move(value)));
}
template<typename Number> void publishScalar(waveformRecord& record,Number value)
{ std::memcpy(record.bptr,&value,sizeof(value)); record.nord=1; }
void publish(RecordContext& context)
{
    auto* record=context.record;
    const auto& value=*context.staged;
    switch(context.definition.kind) {
    case RecordKind::Ai: as<aiRecord>(record).val=value.opaqueDouble(); break;
    case RecordKind::Longin: as<longinRecord>(record).val=epicsInt32(value.integer()); break;
    case RecordKind::Int64in: as<int64inRecord>(record).val=value.integer(); break;
    case RecordKind::Stringin:
        std::memcpy(as<stringinRecord>(record).val,context.text.c_str(),context.text.size()+1); break;
    case RecordKind::Lsi: {
        auto& target=as<lsiRecord>(record);
        std::memcpy(target.val,context.text.c_str(),context.text.size()+1);
        target.len=epicsUInt32(context.text.size()+1); break;
    }
    case RecordKind::Waveform: {
        auto& target=as<waveformRecord>(record);
        if(value.type()==ValueType::Octets) {
            const auto& bytes=value.octets();
            if(!bytes.empty())std::memcpy(target.bptr,bytes.data(),bytes.size());
            target.nord=epicsUInt32(bytes.size());
        } else if(value.type()==ValueType::ObjectId) {
            const auto& arcs=value.objectId();
            if(!arcs.empty())std::memcpy(target.bptr,arcs.data(),arcs.size()*sizeof(uint32_t));
            target.nord=epicsUInt32(arcs.size());
        } else if(value.type()==ValueType::IpAddress) {
            const auto ip=value.ipAddress(); std::memcpy(target.bptr,ip.data(),ip.size()); target.nord=4;
        } else switch(context.definition.scalarType) {
            case ScalarType::Signed32: publishScalar(target,epicsInt32(value.integer())); break;
            case ScalarType::Unsigned32: publishScalar(target,epicsUInt32(value.unsigned32())); break;
            case ScalarType::Signed64: publishScalar(target,epicsInt64(value.integer())); break;
            case ScalarType::Unsigned64: publishScalar(target,epicsUInt64(value.counter64())); break;
            case ScalarType::Float32: publishScalar(target,value.opaqueFloat()); break;
            case ScalarType::Float64: publishScalar(target,value.opaqueDouble()); break;
        }
        break;
    }
    default: require(false);
    }
    if(context.definition.kind!=RecordKind::Waveform)record->udf=FALSE;
    context.published=true; context.nativeSuccess=true;
}
Value capture(RecordContext& context)
{
    auto* record=context.record;
    const auto& binding=context.definition.binding->definition();
    switch(context.definition.kind) {
    case RecordKind::Ao: return convertNumeric(Value::opaqueDouble(as<aoRecord>(record).oval),binding.valueType,true);
    case RecordKind::Longout: return convertNumeric(Value::integer(as<longoutRecord>(record).val),binding.valueType);
    case RecordKind::Int64out: return convertNumeric(Value::integer(as<int64outRecord>(record).val),binding.valueType);
    case RecordKind::Stringout: return captureText(as<stringoutRecord>(record).val,40,binding.capacity);
    case RecordKind::Lso: {
        auto& value=as<lsoRecord>(record);
        require(value.len>=1 && value.len<=context.definition.storageCapacity);
        return captureText(value.val,context.definition.storageCapacity,binding.capacity,value.len);
    }
    default: throw std::runtime_error("record capture rejected");
    }
}
template<RecordKind Kind> long initialize(dbCommon* record)
{
    try {
        require(Requests::instance().attachmentAllowed() && record->scan!=2);
        auto* link=dbGetDevLink(record);
        require(link && link->type==INST_IO && link->value.instio.string);
        const auto parsed=parseRecordLink(std::string("@")+link->value.instio.string);
        const auto binding=Config::instance().bind(parsed.definition);
        const auto& spec=binding->definition();
        require(spec.operation==(input(Kind)?Operation::Get:Operation::Set));
        size_t capacity=1;
        ScalarType type=ScalarType::Signed32;
        void* storage=nullptr; unsigned storageType=0;
        switch(Kind) {
        case RecordKind::Ai: require(numericType(spec.valueType) && as<aiRecord>(record).linr==menuConvertNO_CONVERSION); type=ScalarType::Float64; break;
        case RecordKind::Ao: require(numericType(spec.valueType) && as<aoRecord>(record).linr==menuConvertNO_CONVERSION); type=ScalarType::Float64; break;
        case RecordKind::Longin: require(integerType(spec.valueType)); break;
        case RecordKind::Longout: require(integerType(spec.valueType) && as<longoutRecord>(record).oopt==longoutOOPT_Every_Time); break;
        case RecordKind::Int64in:
        case RecordKind::Int64out: require(integerType(spec.valueType)); type=ScalarType::Signed64; break;
        case RecordKind::Stringin: require(textualType(spec.valueType)); capacity=40; break;
        case RecordKind::Stringout: require(spec.valueType==ValueType::Octets); capacity=40; break;
        case RecordKind::Lsi: {
            require(textualType(spec.valueType)); auto& value=as<lsiRecord>(record);
            capacity=value.sizv; storage=value.val; require(storage && capacity>=16 && capacity<=32767); break;
        }
        case RecordKind::Lso: {
            require(spec.valueType==ValueType::Octets); auto& value=as<lsoRecord>(record);
            capacity=value.sizv; storage=value.val; require(storage && capacity>=16 && capacity<=32767); break;
        }
        case RecordKind::Waveform: {
            auto& value=as<waveformRecord>(record); capacity=value.nelm; storage=value.bptr; storageType=value.ftvl;
            require(storage && capacity);
            if(spec.valueType==ValueType::Octets || spec.valueType==ValueType::IpAddress)require(value.ftvl==menuFtypeUCHAR);
            else if(spec.valueType==ValueType::ObjectId)require(value.ftvl==menuFtypeULONG);
            else { require(numericType(spec.valueType)); type=scalar(value.ftvl); }
            value.busy=FALSE; break;
        }
        }
        auto& context=Requests::instance().attach(RecordBinding(binding,Kind,parsed.budgetMs,capacity,type),
                                                 record,link->value.instio.string,&validate,&prepare);
        context.storage=storage; context.storageType=storageType;
        return Kind==RecordKind::Ao?2:0;
    } catch(const std::exception&) {
        recGblSetSevr(record,LINK_ALARM,INVALID_ALARM);
        std::fprintf(stderr,"snmp3: record initialization rejected: %s\n",record->name);
        return S_dev_badInpType;
    }
}
long process(dbCommon* record,RecordKind kind)
{
    auto* context=static_cast<RecordContext*>(record->dpvt);
    const unsigned error=input(kind)?READ_ALARM:WRITE_ALARM;
    if(!context || context->definition.kind!=kind || !validate(*context)) {
        recGblSetSevr(record,LINK_ALARM,INVALID_ALARM); return -1;
    }
    try {
        if(record->pact) {
            require(Requests::instance().completing(*context));
            if(context->alarm)return -1;
            if(input(kind)) { require(bool(context->staged)); publish(*context); }
            return kind==RecordKind::Ai?2:0;
        }
        if(kind==RecordKind::Waveform)as<waveformRecord>(record).busy=FALSE;
        std::vector<Value> payload;
        if(!input(kind))payload.push_back(capture(*context));
        Requests::instance().admit(*context,payload);
        return 0;
    } catch(const std::exception&) { recGblSetSevr(record,error,INVALID_ALARM); return -1; }
}
long add(dbCommon* record)
{ return Requests::instance().attachmentAllowed() && !record->dpvt ? 0:S_dev_badInpType; }
long detach(dbCommon* record)
{
    if(!record->dpvt)return Requests::instance().attachmentAllowed()?0:S_dev_badInpType;
    return Requests::instance().detach(*static_cast<RecordContext*>(record->dpvt))?0:S_dev_badInpType;
}
dsxt extensions={&add,&detach};
long deviceInit(int after) { if(!after)devExtend(&extensions); return 0; }
long noInterrupt(int,dbCommon*,IOSCANPVT*) { return S_dev_badInpType; }
template<typename Record,RecordKind Kind> long processTyped(Record* record)
{ return process(reinterpret_cast<dbCommon*>(record),Kind); }
}

extern "C" {
aidset snmp3DevAi={{6,nullptr,&deviceInit,&initialize<RecordKind::Ai>,&noInterrupt},&processTyped<aiRecord,RecordKind::Ai>,nullptr};
aodset snmp3DevAo={{6,nullptr,&deviceInit,&initialize<RecordKind::Ao>,&noInterrupt},&processTyped<aoRecord,RecordKind::Ao>,nullptr};
longindset snmp3DevLongin={{5,nullptr,&deviceInit,&initialize<RecordKind::Longin>,&noInterrupt},&processTyped<longinRecord,RecordKind::Longin>};
longoutdset snmp3DevLongout={{5,nullptr,&deviceInit,&initialize<RecordKind::Longout>,&noInterrupt},&processTyped<longoutRecord,RecordKind::Longout>};
int64indset snmp3DevInt64in={{5,nullptr,&deviceInit,&initialize<RecordKind::Int64in>,&noInterrupt},&processTyped<int64inRecord,RecordKind::Int64in>};
int64outdset snmp3DevInt64out={{5,nullptr,&deviceInit,&initialize<RecordKind::Int64out>,&noInterrupt},&processTyped<int64outRecord,RecordKind::Int64out>};
stringindset snmp3DevStringin={{5,nullptr,&deviceInit,&initialize<RecordKind::Stringin>,&noInterrupt},&processTyped<stringinRecord,RecordKind::Stringin>};
stringoutdset snmp3DevStringout={{5,nullptr,&deviceInit,&initialize<RecordKind::Stringout>,&noInterrupt},&processTyped<stringoutRecord,RecordKind::Stringout>};
lsidset snmp3DevLsi={{5,nullptr,&deviceInit,&initialize<RecordKind::Lsi>,&noInterrupt},&processTyped<lsiRecord,RecordKind::Lsi>};
lsodset snmp3DevLso={{5,nullptr,&deviceInit,&initialize<RecordKind::Lso>,&noInterrupt},&processTyped<lsoRecord,RecordKind::Lso>};
wfdset snmp3DevWaveform={{5,nullptr,&deviceInit,&initialize<RecordKind::Waveform>,&noInterrupt},&processTyped<waveformRecord,RecordKind::Waveform>};
}
epicsExportAddress(dset,snmp3DevAi);
epicsExportAddress(dset,snmp3DevAo);
epicsExportAddress(dset,snmp3DevLongin);
epicsExportAddress(dset,snmp3DevLongout);
epicsExportAddress(dset,snmp3DevInt64in);
epicsExportAddress(dset,snmp3DevInt64out);
epicsExportAddress(dset,snmp3DevStringin);
epicsExportAddress(dset,snmp3DevStringout);
epicsExportAddress(dset,snmp3DevLsi);
epicsExportAddress(dset,snmp3DevLso);
epicsExportAddress(dset,snmp3DevWaveform);
