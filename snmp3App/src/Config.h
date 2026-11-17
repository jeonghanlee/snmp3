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
    std::shared_ptr<const Binding> define(const std::string& session, const std::string& object,
                                          const std::string& type, Operation operation, size_t capacity,
                                          const std::string& group, bool systemInfo);
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
    uint64_t revision = 0, nextNumber = 0, nextLink = 0;
    // bound: a record took a binding, so no definition by file or command may change; frozen: the runtime
    // took the configuration, so no record may define a binding either.
    bool frozen = false, bound = false;
    std::string workerPath;
};
}
#endif
