#ifndef SNMP3_PEER_H
#define SNMP3_PEER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace snmp3 {
// What the IOC knows of the unit behind an SNMPv3 endpoint. Normal until a responder at the address shows another
// engine ID than the one the endpoint used first; from then on, and until the IOC process ends, the endpoint is
// Pending while the responder has not been asked to authenticate, Changed when it answered an authenticated
// request under the profile's secrets, and Unverified when it did not or cannot be asked.
enum class PeerState { Normal, Pending, Changed, Unverified };
// The records that show a state of the module process when it changes: the device support of those records
// names the function to call, and the places where a shown state changes call stateChanged. Without a function
// the call does nothing.
void setStateObserver(void (*function)());
void stateChanged();
class Peers {
public:
    static Peers& instance();
    PeerState state(const std::string& endpoint) const;
    // The worker refused a request because the engine ID differs. Enters Pending when the responder can be
    // asked to authenticate, otherwise Unverified. Later calls for the same endpoint change nothing.
    void engineChanged(const std::string& endpoint, const std::string& address,
                       const std::vector<uint8_t>& previous, const std::vector<uint8_t>& current, bool verifiable);
    // Outcome of an authenticated request of a Pending endpoint.
    void verified(const std::string& endpoint, bool authenticated);
    // The state shown for an address in the runtime report: the gravest state of its endpoints, with Pending
    // shown as Unverified.
    PeerState shown(const std::string& address) const;
    // What the state record and the runtime report show for one endpoint: its state with Pending shown as
    // Unverified, and the time of its entry into Changed or Unverified as the log line printed it, empty before.
    struct View { PeerState state = PeerState::Normal; std::string since; };
    View view(const std::string& endpoint) const;
    static const char* name(PeerState state);
    // Alarm message of the records of an endpoint in that state; null for Normal.
    static const char* message(PeerState state);
private:
    struct Entry { std::string address, since; PeerState state = PeerState::Normal; std::vector<uint8_t> previous, current; };
    void enter(const std::string& endpoint, Entry& entry, PeerState state, bool authenticated);
    mutable std::mutex mutex;
    std::map<std::string, Entry> entries;
};
}
#endif
