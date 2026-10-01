#ifndef SNMP3_BINDING_H
#define SNMP3_BINDING_H

#include "Value.h"
#include <map>
#include <memory>
#include <string>

namespace snmp3 {
enum class Version { V1, V2c, V3 };
enum class SecurityLevel { NoAuthNoPriv, AuthNoPriv, AuthPriv };
enum class Operation { Get, Set };
struct Algorithm {
    std::string name;
    int nativeType = 0;
    std::vector<uint32_t> oid;
};
struct Capabilities {
    std::string executable, nativeVersion;
    std::map<std::string, Algorithm> authentication, privacy;
};
struct Profile {
    std::string id, user, contextName;
    Version version = Version::V2c;
    SecurityLevel securityLevel = SecurityLevel::NoAuthNoPriv;
    unsigned timeoutMs = 1000, retries = 3, maxVarbinds = 32;
    bool checkRanges = true;
    Algorithm authentication, privacy;
    std::vector<uint8_t> community, authSecret, privSecret;
    // Empty engine storage declares discovery/inheritance, never an explicit empty ID.
    std::vector<uint8_t> securityEngineId, contextEngineId;
};
struct Endpoint {
    std::string id, address, profile;
    unsigned port = 161;
};
struct BindingDefinition {
    std::string id, endpoint;
    std::vector<uint32_t> oid;
    Operation operation = Operation::Get;
    ValueType valueType = ValueType::Integer;
    size_t capacity = 1;
};
struct Configuration {
    Capabilities capabilities;
    std::map<std::string, Profile> profiles;
    std::map<std::string, Endpoint> endpoints;
    std::map<std::string, BindingDefinition> bindings;
};
class Binding {
public:
    Binding(std::shared_ptr<const Configuration> owner, const BindingDefinition& definition)
        : config(std::move(owner)), spec(definition) {}
    const BindingDefinition& definition() const { return spec; }
    const Endpoint& endpoint() const { return config->endpoints.at(spec.endpoint); }
    const Profile& profile() const { return config->profiles.at(endpoint().profile); }
    const Capabilities& capabilities() const { return config->capabilities; }
private:
    const std::shared_ptr<const Configuration> config;
    const BindingDefinition spec;
};
}
#endif
