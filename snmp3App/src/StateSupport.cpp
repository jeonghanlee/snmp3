#define USE_TYPED_DSET
#include "Config.h"
#include "Peer.h"
#include "Runtime.h"
#include <alarm.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <devSup.h>
#include <recGbl.h>
#include <longinRecord.h>
#include <mbbiRecord.h>
#include <menuScan.h>
#include <epicsExport.h>
#include <climits>
#include <cstdio>
#include <stdexcept>
#include <string>

// Device support "snmp3 state": records that read the state the IOC process holds of the module and of one
// endpoint. No request goes to a unit. The link names what is read:
//   runtime             state of the module, the number of State in Runtime.h           (mbbi)
//   peer=<id>           state of the endpoint: 0 normal, 1 changed, 2 unverified        (mbbi)
//   batchLimit=<id>     limit of the address of the endpoint, as the report prints it   (longin)
//   batchSplits=<id>    reductions of that limit since the IOC started                  (longin)
//   bulks=<id>          requests handed to the worker as GETBULK since the IOC started  (longin)
// <id> is the id of an endpoint of the loaded configuration; a record that names another one, or another item
// than its record type reads, is rejected at initialization.
// With SCAN "I/O Intr" a record processes when a state the module shows has changed: the records of this device
// support share one list, and a change of any shown state processes all of them.
using namespace snmp3;
namespace {
enum class Item { RuntimeState, PeerState, BatchLimit, BatchSplits, Bulks };
struct Source {
    Item item;
    std::string endpoint, address;
};
void require(bool valid)
{ if(!valid)throw std::runtime_error("state record configuration rejected"); }
// The I/O Intr list of the state records. Base frees its lists with the database, so the list is made anew each
// time the device support is initialized.
IOSCANPVT stateScan=nullptr;
void notify() { if(stateScan)scanIoRequest(stateScan); }
long deviceInit(int after)
{
    if(!after) { scanIoInit(&stateScan); setStateObserver(&notify); }
    return 0;
}
long interrupt(int,dbCommon* record,IOSCANPVT* scan)
{
    if(!record->dpvt || !stateScan)return S_dev_badInpType;
    *scan=stateScan;
    return 0;
}
Source parse(const std::string& text)
{
    Source source;
    if(text=="runtime") { source.item=Item::RuntimeState; return source; }
    const auto split=text.find('=');
    require(split!=std::string::npos);
    const auto name=text.substr(0,split);
    if(name=="peer")source.item=Item::PeerState;
    else if(name=="batchLimit")source.item=Item::BatchLimit;
    else if(name=="batchSplits")source.item=Item::BatchSplits;
    else { require(name=="bulks"); source.item=Item::Bulks; }
    source.endpoint=text.substr(split+1);
    const auto configuration=Config::instance().snapshot().configuration;
    require(bool(configuration));
    const auto found=configuration->endpoints.find(source.endpoint);
    require(found!=configuration->endpoints.end());
    source.address=ipc::addressKey(found->second.address);
    return source;
}
long initialize(dbCommon* record,bool state)
{
    try {
        auto* link=dbGetDevLink(record);
        require(link && link->type==INST_IO && link->value.instio.string);
        const auto source=parse(link->value.instio.string);
        require(state==(source.item==Item::RuntimeState || source.item==Item::PeerState));
        record->dpvt=new Source(source);
        return 0;
    } catch(const std::exception&) {
        recGblSetSevr(record,LINK_ALARM,INVALID_ALARM);
        std::fprintf(stderr,"snmp3: record initialization rejected: %s\n",record->name);
        return S_dev_badInpType;
    }
}
long initializeState(dbCommon* record) { return initialize(record,true); }
long initializeNumber(dbCommon* record) { return initialize(record,false); }
long readState(mbbiRecord* record)
{
    const auto* source=static_cast<const Source*>(record->dpvt);
    if(!source) { recGblSetSevr(record,LINK_ALARM,INVALID_ALARM); return -1; }
    if(source->item==Item::RuntimeState)record->rval=epicsUInt32(Runtime::instance().snapshot().state);
    else {
        const auto state=Peers::instance().view(source->endpoint).state;
        record->rval=state==PeerState::Normal ? 0u : state==PeerState::Changed ? 1u : 2u;
    }
    return 0;
}
long readNumber(longinRecord* record)
{
    const auto* source=static_cast<const Source*>(record->dpvt);
    if(!source) { recGblSetSevr(record,LINK_ALARM,INVALID_ALARM); return -1; }
    uint64_t value=0;
    const auto owner=Runtime::instance().schedulerOwner();
    if(owner)for(const auto id:owner->addresses()) {
        if(owner->addressKey(id)!=source->address)continue;
        const auto queue=owner->snapshot(id);
        value=source->item==Item::BatchLimit ? queue.batchLimit : source->item==Item::BatchSplits ? queue.splits : queue.bulks;
        break;
    }
    record->val=value>uint64_t(INT_MAX) ? INT_MAX : epicsInt32(value);
    return 0;
}
}

extern "C" {
mbbidset snmp3DevStateMbbi={{5,nullptr,&deviceInit,&initializeState,&interrupt},&readState};
longindset snmp3DevStateLongin={{5,nullptr,nullptr,&initializeNumber,&interrupt},&readNumber};
}
epicsExportAddress(dset,snmp3DevStateMbbi);
epicsExportAddress(dset,snmp3DevStateLongin);
