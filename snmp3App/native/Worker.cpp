#include "NativeInternal.h"
#include <net-snmp/library/large_fd_set.h>
#include <chrono>
#include <cerrno>
#include <atomic>
#include <cstring>

namespace snmp3 { namespace detail {
namespace {
std::atomic<bool> ProcessOwnerUsed(false);
int64_t monotonicUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
Service::Service() : Service(false) {}
Service::Service(bool limit) : bounded(limit)
{
    if (ProcessOwnerUsed.exchange(true)) throw std::logic_error("one native owner per process is required");
    if(bounded) {
        nativeBytes=65536;
        sessions.reserve(1024);
        static_assert(sizeof(NativeEvent)+64<=256,"native event slot exceeds bound");
    }
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_ALARM_DONT_USE_SIG, 1);
    init_snmp("snmp3Native");
    event("native_init");
}
Service::~Service()
{
    if (std::this_thread::get_id() != thread) std::terminate();
    accepting = false;
    enter();
    for (auto& entry : sessions) {
        if (auto session = entry.lock()) close(*session);
    }
    leave();
    for (auto& entry : sessions) {
        if (auto session = entry.lock()) session->owner = nullptr;
    }
    snmp_shutdown("snmp3Native");
}
void Service::check() const
{
    if (std::this_thread::get_id() != thread)
        throw std::logic_error("native operation requires its owner thread");
}
void Service::enter() { check(); ++depth; }
void Service::leave()
{
    --depth;
    if (depth) return;
    event("native_unwound");
    for (auto& entry : sessions) {
        if (auto session = entry.lock()) {
            for (auto it = session->requests.begin(); it != session->requests.end();) {
                if (it->second->terminal) it = session->requests.erase(it);
                else ++it;
            }
        }
    }
    if (delivering) return;
    delivering = true;
    while (!completions.empty()) {
        auto task = std::move(completions.front());
        completions.pop_front();
        event("completion_dispatch", 0, task.second.token);
        try { task.first(std::move(task.second)); }
        catch (...) { event("completion_exception"); }
    }
    delivering = false;
}
void Service::event(const char* name, uint64_t session, uint64_t token, int operation, long detail)
{
    if(bounded && std::strlen(name)>64) { if(droppedEvents!=UINT64_MAX)++droppedEvents; return; }
    if(bounded && events.size()>=4096) { if(droppedEvents!=UINT64_MAX)++droppedEvents; return; }
    if(bounded && events.capacity()==0)events.reserve(4096);
    NativeEvent value;
    value.event = name; value.session = session; value.token = token;
    value.operation = operation; value.detail = detail; value.monotonicUs = monotonicUs();
    events.push_back(std::move(value));
}
void Service::reserve(uint64_t bytes)
{
    check();
    if(bounded && (bytes>33554432 || nativeBytes>33554432-bytes))
        throw NativeError(NativeOutcome::InvalidRequest,"native storage bound reached");
    nativeBytes+=bytes;
}
void Service::release(uint64_t bytes)
{ check(); if(bytes>nativeBytes)std::terminate(); nativeBytes-=bytes; }
void Service::finish(Request& request, NativeResult result)
{
    if (request.terminal) return;
    result.token = request.token;
    completions.emplace_back(request.completion, std::move(result));
    request.terminal = true;
    event("terminal", request.session->identity.session, request.token);
}
void Service::close(Session& session)
{
    if (session.closing) return;
    session.closing = true;
    if (session.handle) {
        const int result = snmp_sess_close(session.handle);
        session.handle = nullptr;
        event("native_close", session.identity.session, 0, 0, result);
    }
    for (auto& entry : session.requests) {
        NativeResult result;
        result.outcome = NativeOutcome::Cancelled;
        finish(*entry.second, std::move(result));
    }
}
void Service::poll(unsigned maximumWaitMs)
{
    check();
    netsnmp_large_fd_set descriptors;
    netsnmp_large_fd_set_init(&descriptors, FD_SETSIZE);
    NETSNMP_LARGE_FD_ZERO(&descriptors);
    int count = 0;
    timeval timeout = {static_cast<long>(maximumWaitMs / 1000),
                       static_cast<long>((maximumWaitMs % 1000) * 1000)};
    std::vector<std::shared_ptr<Session>> active;
    for (auto& entry : sessions) {
        auto session = entry.lock();
        if (!session || !session->handle || session->closing) continue;
        int block = 0;
        snmp_sess_select_info2(session->handle, &count, &descriptors, &timeout, &block);
        active.push_back(std::move(session));
    }
    enter();
    const int ready = netsnmp_large_fd_set_select(count, &descriptors, nullptr, nullptr, &timeout);
    if (ready < 0) event(errno == EINTR ? "wait_interrupted" : "wait_failed", 0, 0, 0, errno);
    for (auto& session : active) {
        if (ready > 0) {
            const int result = snmp_sess_read2(session->handle, &descriptors);
            event("native_read", session->identity.session, 0, 0, result);
        }
        snmp_sess_timeout(session->handle);
    }
    netsnmp_large_fd_set_cleanup(&descriptors);
    leave();
}
}}
namespace snmp3 {
Worker::Worker() : state(new detail::Service) {}
Worker::Worker(bool bounded) : state(new detail::Service(bounded)) {}
Worker::~Worker() = default;
std::shared_ptr<Native> Worker::open(const Profile& profile, const Endpoint& endpoint,
                                    const Capabilities& expected)
{
    state->check();
    if (!state->accepting) throw std::logic_error("native owner is shutting down");
    return std::shared_ptr<Native>(new Native(detail::open(*state, profile, endpoint, expected)));
}
void Worker::service(unsigned maximumWaitMs) { state->poll(maximumWaitMs); }
std::vector<NativeEvent> Worker::takeEvents()
{
    state->check();
    std::vector<NativeEvent> events;
    events.swap(state->events);
    return events;
}
uint64_t Worker::droppedEvents() const { state->check(); return state->droppedEvents; }
}
