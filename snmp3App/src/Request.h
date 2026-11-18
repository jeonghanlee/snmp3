#ifndef SNMP3_REQUEST_H
#define SNMP3_REQUEST_H

#include "Conversion.h"
#include "Scheduler.h"
#include <callback.h>
#include <condition_variable>
#include <list>

struct dbCommon;
struct typed_rset;
struct ioscan_head;

namespace snmp3 {
enum class RecordKind { Ai, Longin, Int64in, Stringin, Lsi, Waveform,
                        Ao, Longout, Int64out, Stringout, Lso };
// A record link in one of two forms: "@binding=<id> [deadline_ms=<n>]" names a binding of the file, and
// "@<session> <object> [type=<type>] [deadline_ms=<n>] [group=<label>] [systemInfo]" names the object itself,
// by a numeric identifier with its type or by MODULE::name.index, whose type its MIB declares.
// budgetMs is zero when the link gives no deadline.
struct RecordLink {
    std::string definition;
    unsigned budgetMs = 0;
    bool named = false;
    std::string session, object, type, group;
    bool systemInfo = false;
};
RecordLink parseRecordLink(const std::string& text);

// Record storage and conversion choices remain fixed for an initialized activation.
struct RecordBinding {
    const std::shared_ptr<const Binding> binding;
    const RecordKind kind;
    const unsigned budgetMs;
    const size_t storageCapacity;
    const ScalarType scalarType;
    RecordBinding(std::shared_ptr<const Binding> value, RecordKind record,
                  unsigned budget, size_t capacity, ScalarType scalar)
        : binding(std::move(value)), kind(record), budgetMs(budget),
          storageCapacity(capacity), scalarType(scalar) {}
};

enum class CallbackState { Idle, Pending, Queued, Running, Inert };
struct Cycle;
struct RecordContext {
    const RecordBinding definition;
    const std::string link;
    const unsigned dtype;
    const void* const dset;
    const typed_rset* const rset;
    dbCommon* record;
    std::shared_ptr<Scheduler> owner;
    uint64_t handle=0;
    epicsCallback callback{};
    ipc::Identity identity{};
    TerminalView terminal;
    CallbackState callbackState=CallbackState::Idle;
    bool active=false, published=false, nativeSuccess=false;
    // The staged value is published although an alarm is raised: system information of a unit whose engine ID
    // changed.
    bool valueWithAlarm=false;
    unsigned alarm=0;
    const char* message=nullptr;
    // Storage of a message that is built for one result, such as an error status with its index.
    std::string failure;
    unsigned storageType=0;
    void* storage=nullptr;
    std::unique_ptr<Value> staged;
    std::string text;
    // A record that reads through a subscription: the module sends its requests, and the record processes on
    // each result. ready: an outcome waits for the next processing of the record. A synthetic outcome is one
    // that no request produced: the refusal of an endpoint whose unit changed, or a refused admission. The held
    // alarm is the one of the last outcome, raised again when the record is processed without an outcome.
    Cycle* subscription=nullptr;
    bool ready=false;
    unsigned syntheticAlarm=0;
    const char* syntheticMessage=nullptr;
    unsigned heldAlarm=0;
    std::string heldMessage;
    bool (*validate)(RecordContext&);
    void (*prepare)(RecordContext&, const ipc::Result&);
    RecordContext(RecordBinding spec, dbCommon* rec, std::string address,
                  bool (*valid)(RecordContext&), void (*stage)(RecordContext&,const ipc::Result&));
};
struct RecordSnapshot {
    uint64_t contexts=0,active=0,pending=0,queued=0,running=0,inert=0;
    uint64_t entered=0,enqueueFailures=0,completions=0;
    bool entryOpen=false,drainFailed=false,detachAllowed=false;
};

// One bounded visit pass. more: contexts remain that this activation has not visited since the
// pass began, so the caller continues without waiting. retry: a callback enqueue failed and the
// entry must be offered again on a bounded timer.
// Bound on the delay before a failed callback enqueue is offered again.
const uint64_t ServiceRetryUs=10000;
struct ServiceOutcome {
    bool more=false,retry=false;
    // Time at which the next request of a subscription is due; UINT64_MAX when none waits for its time.
    uint64_t timerUs=UINT64_MAX;
};
// Context storage outlives Base callback queue references and isolated record teardown.
class Requests {
public:
    static Requests& instance();
    // subscription and periodMs name the subscription a record reads through; empty for a record that sends
    // its own request when it is processed.
    RecordContext& attach(RecordBinding binding, dbCommon* record, const std::string& link,
                          bool (*validate)(RecordContext&),
                          void (*prepare)(RecordContext&,const ipc::Result&),
                          const std::string& subscription=std::string(), unsigned periodMs=0);
    // The I/O Intr list of the subscription of a record; added tells whether the record joins or leaves the
    // list. The requests of a subscription run while its list holds a record. Returns false for a record
    // without a subscription.
    bool interrupt(RecordContext& context, bool added, ioscan_head** scan);
    // A processing of a subscription record takes the outcome that waits for it, and gives it back when the
    // record has taken value and alarm. take returns false when no outcome waits.
    bool takeOutcome(RecordContext& context);
    void outcomeTaken(RecordContext& context);
    // Whether Base scans: the requests of the subscriptions run only then.
    void scanning(bool open);
    bool attachmentAllowed();
    bool detach(RecordContext& context);
    void start(std::shared_ptr<Scheduler> owner);
    void admit(RecordContext& context, const std::vector<Value>& payload);
    bool completing(RecordContext& context);
    ServiceOutcome service();
    bool drain();
    void permitDetach();
    void queuesDestroyed();
    RecordSnapshot snapshot();
private:
    Requests();
    static void callback(epicsCallback* callback);
    static void cycleProcessed(void* cycle, ioscan_head* scan, int priority);
    void finish(RecordContext& context);
    uint64_t serviceCycles(uint64_t nowUs);
    void startCycle(Cycle& cycle, uint64_t nowUs);
    void deliver(Cycle& cycle);
    void settle(Cycle& cycle);
    std::mutex mutex;
    std::condition_variable changed;
    // Incremented under the mutex with every notification of `changed`, so a wait that began after a
    // notification arrived during a service pass does not sleep through it.
    uint64_t changes=0;
    std::list<std::unique_ptr<RecordContext>> contexts;
    std::list<std::unique_ptr<RecordContext>>::iterator cursor;
    std::shared_ptr<Scheduler> activation;
    std::map<std::string,std::unique_ptr<Cycle>> cycles;
    bool scanOpen=false, stopping=false;
    uint64_t entered=0,enqueueFailures=0,completions=0;
    size_t visited=0;
    bool refused=false;
    bool initializing=true,entryOpen=false,producers=false;
    bool drainFailed=false,detachAllowed=false;
};
}
#endif
