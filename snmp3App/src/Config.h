#ifndef SNMP3_CONFIG_H
#define SNMP3_CONFIG_H

#include "Binding.h"
#include <epicsMutex.h>
#include <set>
#include <vector>

namespace snmp3 {
struct ConfigState {
    std::shared_ptr<const Configuration> configuration;
    uint64_t revision;
    bool frozen;
    std::string workerPath;
};
class Config {
public:
    Config();
    static Config& instance();
    void load(const std::string& file, const std::string& nativeProbe);
    // Definitions by shell command. A session is one unit under a name, with the values a profile has in a
    // file; options is a list of name=value words. Every call is rejected once iocInit has begun.
    void setProbePath(const std::string& absolutePath);
    void session(const std::string& name, const std::string& address, const std::vector<std::string>& options);
    void options(const std::string& pattern, const std::vector<std::string>& options);
    // Completes the sessions before the first record takes a binding: every session must hold the values its
    // version needs. A session that does not is left out, and the first reason is thrown.
    void finalize();
    // The binding of a record that names its object in its link: object is a numeric identifier, type the
    // name of a value type, capacity the size of the record's value for octets and object identifiers.
    // subscription names the subscription the record reads through, or is empty.
    std::shared_ptr<const Binding> define(const std::string& session, const std::string& object,
                                          const std::string& type, Operation operation, size_t capacity,
                                          const std::string& group, bool systemInfo,
                                          const std::string& subscription = std::string());
    // A subscription: a set of read objects of one session that the module reads by itself every period. Its
    // name shares the names of the sessions, so that the first word of a record link means one thing.
    void subscription(const std::string& name, const std::string& session, const std::string& periodMs);
    // Whether a name is a subscription; gives its session and its period.
    bool subscriptionOf(const std::string& name, std::string& session, unsigned& periodMs);
    // A place of MIB files of the IOC: a directory or one file. With system set, the path is a directory that
    // replaces the directories of the system for this IOC.
    void mib(const std::string& path, bool system);
    // Resolves the object names of the record links through the MIB places, in one run of the probe. The names
    // are the object words of the form MODULE::name.index. Returns one line per module that no place holds and
    // per name that did not resolve, is no object or carries no index.
    std::vector<std::string> resolve(const std::vector<std::string>& names);
    // Removes the binding a record defined when the record is rejected afterwards.
    void discard(const std::string& id);
    void show(const std::string& pattern, int verbosity);
    std::shared_ptr<const Binding> bind(const std::string& id);
    void freeze();
    void setWorkerPath(const std::string& absolutePath);
    ConfigState snapshot();
    void report();
private:
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    // What a MIB declares for one name: the identifier with its index, the value type by the name a link uses
    // (empty when the MIB gives none the module holds), and whether the object can be read and written.
    struct Resolved {
        std::vector<uint32_t> oid;
        std::string type;
        bool readable = false, writable = false;
    };
    struct Subscription {
        std::string session;
        unsigned periodMs = 0;
    };
    // A binding that a record defined by a name: the name, and whether its type came from the link.
    struct NamedObject {
        std::string name;
        bool typeFromLink = false;
    };
    struct Session {
        Endpoint endpoint;
        Profile profile;
        std::set<std::string> given;
        bool fromFile = false;
    };
    void rebuild(bool complete, std::string* reason);
    void finalizeLocked();
    void requireOpen() const;
    const Capabilities& probedCapabilities();
    epicsMutex mutex;
    // The configuration in force: the definitions of the file and of the sessions. It is one object from the
    // first binding on, and the bindings that records define are added to it until it is frozen.
    std::shared_ptr<Configuration> current;
    std::shared_ptr<Configuration> loaded;
    std::map<std::string, Session> sessions;
    Capabilities probed;
    bool haveProbed = false;
    std::string probePath;
    // MIB places of the IOC in the order given, the directory that replaces the ones of the system (empty: the
    // ones built into the library), and what the last run of the probe for names reported.
    std::vector<std::string> mibPlaces;
    std::string mibSystem, mibSystemUsed;
    std::map<std::string, Resolved> resolved;
    std::vector<std::pair<std::string, std::string>> mibModules;
    std::map<std::string, NamedObject> namedObjects;
    std::map<std::string, Subscription> subscriptions;
    // The subscription of a binding that a record defined on one.
    std::map<std::string, std::string> subscribedObjects;
    unsigned probeStarts = 0;
    uint64_t revision = 0, nextNumber = 0, nextLink = 0;
    // bound: a record took a binding, so no definition by file or command may change; frozen: the runtime
    // took the configuration, so no record may define a binding either.
    bool frozen = false, bound = false;
    std::string workerPath;
};
}
#endif
