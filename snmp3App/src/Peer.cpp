#include "Peer.h"
#include <cstdio>
#include <ctime>

namespace snmp3 {
namespace {
std::string hex(const std::vector<uint8_t>& bytes)
{
    static const char digits[] = "0123456789abcdef";
    std::string text;
    for (const auto byte : bytes) { text += digits[byte >> 4]; text += digits[byte & 15]; }
    return text.empty() ? "unknown" : text;
}
}
Peers& Peers::instance() { static Peers peers; return peers; }
PeerState Peers::state(const std::string& endpoint) const
{
    std::lock_guard<std::mutex> guard(mutex);
    const auto found = entries.find(endpoint);
    return found == entries.end() ? PeerState::Normal : found->second.state;
}
// One line of the IOC log for the entry into Changed or Unverified, the record a user traces the event by.
void Peers::enter(const std::string& endpoint, Entry& entry, PeerState state, bool authenticated)
{
    entry.state = state;
    char stamp[32] = "";
    const std::time_t now = std::time(nullptr);
    std::tm utc;
    if (gmtime_r(&now, &utc)) std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    std::fprintf(stderr, "snmp3 engine: endpoint=%s state=%s previous=%s current=%s authenticated=%d time=%s\n",
                 endpoint.c_str(), name(state), hex(entry.previous).c_str(), hex(entry.current).c_str(),
                 authenticated ? 1 : 0, stamp);
}
void Peers::engineChanged(const std::string& endpoint, const std::string& address,
                          const std::vector<uint8_t>& previous, const std::vector<uint8_t>& current, bool verifiable)
{
    std::lock_guard<std::mutex> guard(mutex);
    auto& entry = entries[endpoint];
    if (entry.state != PeerState::Normal) return;
    entry.address = address; entry.previous = previous; entry.current = current;
    if (verifiable) entry.state = PeerState::Pending;
    else enter(endpoint, entry, PeerState::Unverified, false);
}
void Peers::verified(const std::string& endpoint, bool authenticated)
{
    std::lock_guard<std::mutex> guard(mutex);
    const auto found = entries.find(endpoint);
    if (found == entries.end() || found->second.state != PeerState::Pending) return;
    enter(endpoint, found->second, authenticated ? PeerState::Changed : PeerState::Unverified, authenticated);
}
PeerState Peers::shown(const std::string& address) const
{
    std::lock_guard<std::mutex> guard(mutex);
    PeerState result = PeerState::Normal;
    for (const auto& entry : entries) {
        if (entry.second.address != address) continue;
        if (entry.second.state == PeerState::Changed && result == PeerState::Normal) result = PeerState::Changed;
        if (entry.second.state == PeerState::Pending || entry.second.state == PeerState::Unverified)
            result = PeerState::Unverified;
    }
    return result;
}
const char* Peers::name(PeerState state)
{
    return state == PeerState::Normal ? "normal" : state == PeerState::Changed ? "changed" : "unverified";
}
const char* Peers::message(PeerState state)
{
    return state == PeerState::Normal ? nullptr :
           state == PeerState::Changed ? "engine ID changed: restart IOC" : "engine ID unverified: restart IOC";
}
}
