#ifndef SNMP3_REQUEST_H
#define SNMP3_REQUEST_H

#include "Conversion.h"
#include "Scheduler.h"
#include <callback.h>
#include <condition_variable>
#include <list>

struct dbCommon;
struct typed_rset;

namespace snmp3 {
enum class RecordKind { Ai, Longin, Int64in, Stringin, Lsi, Waveform,
                        Ao, Longout, Int64out, Stringout, Lso };
struct RecordLink {
    std::string definition;
    unsigned budgetMs = 0;
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
    unsigned alarm=0;
    const char* message=nullptr;
    unsigned storageType=0;
    void* storage=nullptr;
    std::unique_ptr<Value> staged;
    std::string text;
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

// Context storage outlives Base callback queue references and isolated record teardown.
class Requests {
public:
    static Requests& instance();
    RecordContext& attach(RecordBinding binding, dbCommon* record, const std::string& link,
                          bool (*validate)(RecordContext&),
                          void (*prepare)(RecordContext&,const ipc::Result&));
    bool attachmentAllowed();
    bool detach(RecordContext& context);
    void start(std::shared_ptr<Scheduler> owner);
    void admit(RecordContext& context, const std::vector<Value>& payload);
    bool completing(RecordContext& context);
    void service();
    bool drain();
    void permitDetach();
    void queuesDestroyed();
    RecordSnapshot snapshot();
private:
    Requests();
    static void callback(epicsCallback* callback);
    void finish(RecordContext& context);
    std::mutex mutex;
    std::condition_variable changed;
    std::list<std::unique_ptr<RecordContext>> contexts;
    std::list<std::unique_ptr<RecordContext>>::iterator cursor;
    std::shared_ptr<Scheduler> activation;
    uint64_t entered=0,enqueueFailures=0,completions=0;
    bool initializing=true,entryOpen=false,producers=false;
    bool drainFailed=false,detachAllowed=false;
};
}
#endif
