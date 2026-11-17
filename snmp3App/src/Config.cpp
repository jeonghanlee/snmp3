#include "Config.h"
#include "Environment.h"
#include "Ipc.h"
#include "Json.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <epicsGuard.h>
#include <epicsString.h>
#include <epicsThread.h>

namespace snmp3 {
namespace {
const size_t MaxConfigBytes = 1024 * 1024;
const size_t MaxSecretBytes = 1024;
const size_t MaxCapabilityBytes = 64 * 1024;
const unsigned ProbeTimeoutMs = 5000;
const size_t MaxIdBytes = 64;
const size_t MaxOidArcs = 128;
const size_t MaxEngineBytes = 32;
const size_t MinEngineBytes = 5;

void reject(const char* message) { throw std::runtime_error(message); }
class Descriptor {
public:
    explicit Descriptor(int value) : fd(value) {}
    ~Descriptor() { reset(); }
    int get() const { return fd; }
    void reset() { if (fd >= 0) close(fd); fd = -1; }
private:
    int fd;
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
};
std::string readFile(const std::string& path, bool secret)
{
    Descriptor fd(open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | (secret ? O_NOFOLLOW : 0)));
    struct stat status;
    if (fd.get() < 0 || fstat(fd.get(), &status) || !S_ISREG(status.st_mode))
        reject("configuration file rejected");
    if (secret && (status.st_uid != geteuid() || (status.st_mode & 0077)))
        reject("secret permissions rejected");
    const size_t limit = secret ? MaxSecretBytes + 2 : MaxConfigBytes;
    if (status.st_size < 0 || static_cast<uint64_t>(status.st_size) > limit)
        reject("file size rejected");
    std::string result;
    char buffer[4096];
    for (;;) {
        const ssize_t count = read(fd.get(), buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) reject("file read failed");
        if (count == 0) break;
        if (result.size() + static_cast<size_t>(count) > limit) reject("file size rejected");
        result.append(buffer, static_cast<size_t>(count));
    }
    return result;
}
void fields(const Json& node, std::initializer_list<const char*> allowed)
{
    if (node.kind != Json::Kind::Object) reject("object required");
    for (const auto& item : node.object)
        if (std::none_of(allowed.begin(), allowed.end(), [&](const char* name) { return item.first == name; }))
            reject("unknown configuration field");
}
const Json& required(const Json& node, const char* key)
{
    auto value = node.object.find(key);
    if (value == node.object.end()) reject("required field absent");
    return value->second;
}
bool present(const Json& node, const char* key) { return node.object.count(key) != 0; }
std::string string(const Json& node)
{
    if (node.kind != Json::Kind::String ||
        std::any_of(node.text.begin(), node.text.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        reject("configuration string rejected");
    return node.text;
}
std::string string(const Json& node, const char* key) { return string(required(node, key)); }
unsigned number(const Json& node, unsigned low, unsigned high)
{
    if (node.kind != Json::Kind::Number || node.text.empty() ||
        std::any_of(node.text.begin(), node.text.end(), [](char c) { return c < '0' || c > '9'; }))
        reject("integer option required");
    errno = 0;
    char* end = NULL;
    const unsigned long long value = std::strtoull(node.text.c_str(), &end, 10);
    if (errno || !end || *end || value < low || value > high) reject("integer option out of range");
    return static_cast<unsigned>(value);
}
unsigned option(const Json& node, const char* key, unsigned fallback, unsigned low, unsigned high)
{
    return present(node, key) ? number(required(node, key), low, high) : fallback;
}
std::string id(const Json& node, const char* key)
{
    const std::string value = string(node, key);
    if (value.empty() || value.size() > MaxIdBytes ||
        std::any_of(value.begin(), value.end(), [](char c) {
            return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.');
        })) reject("configuration identifier rejected");
    return value;
}
const std::vector<Json>& array(const Json& node, const char* key)
{
    const Json& value = required(node, key);
    if (value.kind != Json::Kind::Array) reject("array required");
    return value.array;
}
std::vector<uint8_t> secretFile(const std::string& path, bool community);
std::vector<uint8_t> secret(const Json& node, const char* key, const std::string& directory, bool community)
{
    std::string path = string(node, key);
    if (path.empty()) reject("secret path rejected");
    if (path[0] != '/') path = directory + '/' + path;
    return secretFile(path, community);
}
std::vector<uint8_t> secretFile(const std::string& path, bool community)
{
    std::string bytes = readFile(path, true);
    if (!bytes.empty() && bytes.back() == '\n') {
        bytes.pop_back();
        if (!bytes.empty() && bytes.back() == '\r') bytes.pop_back();
    }
    if (bytes.size() < (community ? 1u : 8u) || bytes.size() > (community ? 255u : MaxSecretBytes) ||
        std::any_of(bytes.begin(), bytes.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        reject("secret content rejected");
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
}
std::vector<uint8_t> engineText(std::string hex);
std::vector<uint8_t> engine(const Json& node, const char* key)
{
    if (!present(node, key)) return {};
    return engineText(string(node, key));
}
std::vector<uint8_t> engineText(std::string hex)
{
    if (hex.compare(0, 2, "0x") == 0 || hex.compare(0, 2, "0X") == 0) hex.erase(0, 2);
    if (hex.size() % 2 || hex.size() < MinEngineBytes * 2 || hex.size() > MaxEngineBytes * 2)
        reject("engine ID length rejected");
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<uint8_t> result;
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int a = digit(hex[i]), b = digit(hex[i + 1]);
        if (a < 0 || b < 0) reject("engine ID hex rejected");
        result.push_back(static_cast<uint8_t>(a * 16 + b));
    }
    if (std::all_of(result.begin(), result.end(), [](uint8_t c) { return c == 0; }) ||
        std::all_of(result.begin(), result.end(), [](uint8_t c) { return c == 255; }))
        reject("engine ID value rejected");
    return result;
}
std::vector<uint32_t> oid(const std::string& value)
{
    std::vector<uint32_t> result;
    size_t begin = value.empty() || value[0] != '.' ? 0 : 1;
    for (;;) {
        const size_t end = value.find('.', begin);
        Json part; part.kind = Json::Kind::Number; part.text = value.substr(begin, end - begin);
        result.push_back(number(part, 0, UINT32_MAX));
        if (result.size() > MaxOidArcs) reject("OID length rejected");
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    if (result.size() < 2 || result[0] > 2 || (result[0] < 2 && result[1] > 39))
        reject("OID root rejected");
    return result;
}

std::string observeCapabilities(const std::string& executable, const std::vector<std::string>& environment)
{
    if (executable.empty() || executable[0] != '/' || executable.find('\0') != std::string::npos)
        reject("native executable rejected");
    int pipeFds[2];
    if (pipe2(pipeFds, O_CLOEXEC)) reject("native pipe failed");
    Descriptor reader(pipeFds[0]), writer(pipeFds[1]);
    const int flags = fcntl(reader.get(), F_GETFL);
    if (flags < 0 || fcntl(reader.get(), F_SETFL, flags | O_NONBLOCK)) reject("native pipe failed");
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions)) reject("native spawn failed");
    int error = posix_spawn_file_actions_adddup2(&actions, writer.get(), STDOUT_FILENO);
    error |= posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    error |= posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    error |= posix_spawn_file_actions_addclose(&actions, reader.get());
    error |= posix_spawn_file_actions_addclose(&actions, writer.get());
    pid_t pid = -1;
    char* args[] = {const_cast<char*>(executable.c_str()), const_cast<char*>("--capabilities"), NULL};
    std::vector<std::string> entries(environment);
    std::vector<char*> envp;
    size_t envBytes = 0;
    for (auto& entry : entries) { envBytes += entry.size() + 1; envp.push_back(&entry[0]); }
    envp.push_back(NULL);
    if (envBytes > 8192) reject("native environment rejected");
    if (!error) error = posix_spawn(&pid, executable.c_str(), &actions, NULL, args, envp.data());
    posix_spawn_file_actions_destroy(&actions);
    if (error) reject("native spawn failed");
    writer.reset();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ProbeTimeoutMs);
    std::string output;
    bool failed = false, eof = false, reaped = false;
    int status = 0;
    try {
        while (!eof || !reaped) {
            if (std::chrono::steady_clock::now() >= deadline) { failed = true; break; }
            char buffer[4096];
            const ssize_t count = read(reader.get(), buffer, sizeof(buffer));
            if (count > 0) {
                if (output.size() + static_cast<size_t>(count) > MaxCapabilityBytes) { failed = true; break; }
                output.append(buffer, static_cast<size_t>(count));
            } else if (count == 0) eof = true;
            else if (errno != EAGAIN && errno != EINTR) { failed = true; break; }
            if (!reaped) {
                const pid_t waited = waitpid(pid, &status, WNOHANG);
                if (waited == pid) reaped = true;
                else if (waited < 0 && errno != EINTR) { failed = true; break; }
            }
            if (!eof || !reaped) {
                pollfd event = {reader.get(), POLLIN, 0};
                poll(eof ? NULL : &event, eof ? 0 : 1, 10);
            }
        }
    } catch (...) { failed = true; }
    if (!reaped) {
        kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    }
    if (failed || !WIFEXITED(status) || WEXITSTATUS(status)) reject("native capability process failed");
    return output;
}
Capabilities capabilities(const std::string& executable, const std::vector<std::string>& environment)
{
    const Json root = Json::parse(observeCapabilities(executable, environment));
    fields(root, {"schema", "version", "authentication", "privacy"});
    if (number(required(root, "schema"), 1, 1) != 1) reject("native schema rejected");
    Capabilities result; result.executable = executable; result.nativeVersion = string(root, "version");
    if (result.nativeVersion.empty() || result.nativeVersion.size() > 128) reject("native version rejected");
    for (const char* key : {"authentication", "privacy"}) {
        auto& algorithms = std::strcmp(key, "authentication") == 0 ? result.authentication : result.privacy;
        for (const Json& item : array(root, key)) {
            fields(item, {"name", "type", "oid"});
            Algorithm alg; alg.name = string(item, "name");
            if (alg.name.empty() || alg.name.size() > 128) reject("native algorithm rejected");
            alg.nativeType = static_cast<int>(number(required(item, "type"), 1, INT32_MAX));
            alg.oid = oid(string(item, "oid"));
            if (!algorithms.emplace(alg.name, std::move(alg)).second) reject("duplicate native algorithm");
        }
    }
    return result;
}
Algorithm algorithm(const Json& node, const char* key, const std::map<std::string, Algorithm>& algorithms)
{
    auto found = algorithms.find(string(node, key));
    if (found == algorithms.end()) reject("native algorithm unavailable");
    return found->second;
}
ValueType valueType(const std::string& value)
{
    const std::map<std::string, ValueType> names = {
        {"integer", ValueType::Integer}, {"unsigned32", ValueType::Unsigned32},
        {"counter32", ValueType::Counter32}, {"gauge32", ValueType::Gauge32},
        {"timeticks", ValueType::TimeTicks}, {"counter64", ValueType::Counter64},
        {"octets", ValueType::Octets}, {"oid", ValueType::ObjectId},
        {"ipAddress", ValueType::IpAddress}, {"opaqueFloat", ValueType::OpaqueFloat},
        {"opaqueDouble", ValueType::OpaqueDouble}
    };
    auto found = names.find(value);
    if (found == names.end()) reject("binding value type rejected");
    return found->second;
}

// A group is read in one request: every member is a GET on one endpoint, the declared count fits the profile
// and the request and the largest response fit one IPC frame.
void validateGroups(const Configuration& configuration)
{
    struct Totals { std::string endpoint; size_t count = 0; uint64_t request = 8, response = 8; };
    std::map<std::string, Totals> groups;
    for (const auto& entry : configuration.bindings) {
        const BindingDefinition& binding = entry.second;
        if (binding.group.empty()) continue;
        if (binding.operation != Operation::Get) reject("group member must be a get binding");
        auto found = groups.find(binding.group);
        if (found == groups.end()) found = groups.emplace(binding.group, Totals()).first;
        Totals& total = found->second;
        if (total.count && total.endpoint != binding.endpoint) reject("group spans endpoints");
        total.endpoint = binding.endpoint;
        ++total.count;
        total.request = ipc::add(total.request, 48);
        total.response = ipc::add(total.response, ipc::add(48, ipc::responseBytes(binding)));
    }
    for (const auto& entry : groups) {
        const Totals& total = entry.second;
        const Endpoint& endpoint = configuration.endpoints.at(total.endpoint);
        if (total.count > configuration.profiles.at(endpoint.profile).maxVarbinds) reject("group exceeds profile maxVarbinds");
        if (total.count > ipc::MaxMembers || total.request > ipc::DataBytes || total.response > ipc::DataBytes)
            reject("group exceeds the request frame");
    }
}
std::shared_ptr<Configuration> candidate(const std::string& file, const std::string& executable,
                                         const std::vector<std::string>& environment)
{
    char* resolved = realpath(file.c_str(), NULL);
    if (!resolved) reject("configuration path rejected");
    const std::string path(resolved); free(resolved);
    const std::string directory = path.substr(0, path.rfind('/'));
    const Json root = Json::parse(readFile(path, false));
    fields(root, {"schema", "profiles", "endpoints", "bindings"});
    number(required(root, "schema"), 1, 1);
    auto result = std::make_shared<Configuration>();
    result->capabilities = capabilities(executable, environment);
    for (const Json& item : array(root, "profiles")) {
        fields(item, {"id", "version", "timeoutMs", "retries", "maxVarbinds", "maxMessageSize", "checkRanges",
                      "communityFile", "user", "securityLevel", "authAlgorithm", "authSecretFile",
                      "privAlgorithm", "privSecretFile", "contextName", "securityEngineId", "contextEngineId"});
        Profile profile; profile.id = id(item, "id");
        profile.timeoutMs = option(item, "timeoutMs", 1000, 1, 60000);
        profile.retries = option(item, "retries", 3, 0, 10);
        profile.maxVarbinds = option(item, "maxVarbinds", 32, 1, 1024);
        profile.maxMessageSize = option(item, "maxMessageSize", 1472, 484, 65507);
        if (present(item, "checkRanges")) {
            const Json& value = required(item, "checkRanges");
            if (value.kind != Json::Kind::Boolean) reject("boolean option required");
            profile.checkRanges = value.boolean;
        }
        const std::string version = string(item, "version");
        if (version == "1" || version == "2c") {
            profile.version = version == "1" ? Version::V1 : Version::V2c;
            if (item.object.size() > 7) reject("profile security fields rejected");
            for (const char* key : {"user", "securityLevel", "authAlgorithm", "authSecretFile", "privAlgorithm",
                                    "privSecretFile", "contextName", "securityEngineId", "contextEngineId"})
                if (present(item, key)) reject("profile security fields rejected");
            profile.community = secret(item, "communityFile", directory, true);
        } else if (version == "3") {
            profile.version = Version::V3;
            if (present(item, "communityFile")) reject("profile security fields rejected");
            profile.user = string(item, "user");
            if (profile.user.empty() || profile.user.size() > 32) reject("USM user rejected");
            const std::string level = string(item, "securityLevel");
            if (level == "noAuthNoPriv") profile.securityLevel = SecurityLevel::NoAuthNoPriv;
            else if (level == "authNoPriv") profile.securityLevel = SecurityLevel::AuthNoPriv;
            else if (level == "authPriv") profile.securityLevel = SecurityLevel::AuthPriv;
            else reject("security level rejected");
            if (profile.securityLevel != SecurityLevel::NoAuthNoPriv) {
                profile.authentication = algorithm(item, "authAlgorithm", result->capabilities.authentication);
                profile.authSecret = secret(item, "authSecretFile", directory, false);
            } else if (present(item, "authAlgorithm") || present(item, "authSecretFile")) reject("unused authentication rejected");
            if (profile.securityLevel == SecurityLevel::AuthPriv) {
                profile.privacy = algorithm(item, "privAlgorithm", result->capabilities.privacy);
                profile.privSecret = secret(item, "privSecretFile", directory, false);
            } else if (present(item, "privAlgorithm") || present(item, "privSecretFile")) reject("unused privacy rejected");
            if (present(item, "contextName")) profile.contextName = string(item, "contextName");
            if (profile.contextName.size() > 255) reject("context name rejected");
            profile.securityEngineId = engine(item, "securityEngineId");
            profile.contextEngineId = engine(item, "contextEngineId");
        } else reject("protocol version rejected");
        const std::string name = profile.id;
        if (!result->profiles.emplace(name, std::move(profile)).second) reject("duplicate profile");
    }
    for (const Json& item : array(root, "endpoints")) {
        fields(item, {"id", "address", "port", "profile"});
        Endpoint endpoint; endpoint.id = id(item, "id"); endpoint.profile = id(item, "profile");
        const std::string address = string(item, "address");
        unsigned char bytes[16]; char normalized[INET6_ADDRSTRLEN];
        const int family = inet_pton(AF_INET, address.c_str(), bytes) == 1 ? AF_INET : AF_INET6;
        if (inet_pton(family, address.c_str(), bytes) != 1 || !inet_ntop(family, bytes, normalized, sizeof(normalized)))
            reject("numeric device address rejected");
        endpoint.address = normalized; endpoint.port = option(item, "port", 161, 1, 65535);
        if (!result->profiles.count(endpoint.profile)) reject("endpoint profile absent");
        const std::string name = endpoint.id;
        if (!result->endpoints.emplace(name, std::move(endpoint)).second) reject("duplicate endpoint");
    }
    for (const Json& item : array(root, "bindings")) {
        fields(item, {"id", "endpoint", "group", "oid", "operation", "valueType", "capacity", "systemInfo"});
        BindingDefinition binding; binding.id = id(item, "id"); binding.endpoint = id(item, "endpoint");
        if (present(item, "group")) binding.group = id(item, "group");
        binding.oid = oid(string(item, "oid"));
        const std::string operation = string(item, "operation");
        if (operation == "get") binding.operation = Operation::Get;
        else if (operation == "set") binding.operation = Operation::Set;
        else reject("binding operation rejected");
        if (present(item, "systemInfo")) {
            const Json& value = required(item, "systemInfo");
            if (value.kind != Json::Kind::Boolean) reject("boolean option required");
            binding.systemInfo = value.boolean;
            if (binding.systemInfo && binding.operation != Operation::Get) reject("binding systemInfo requires get");
        }
        binding.valueType = valueType(string(item, "valueType"));
        const unsigned maximum = binding.valueType == ValueType::Octets ? MaxConfigBytes :
            binding.valueType == ValueType::ObjectId ? MaxOidArcs : 1;
        binding.capacity = option(item, "capacity", 1, 1, maximum);
        if (!result->endpoints.count(binding.endpoint)) reject("binding endpoint absent");
        const std::string name = binding.id;
        if (!result->bindings.emplace(name, std::move(binding)).second) reject("duplicate binding");
    }
    validateGroups(*result);
    uint64_t number = 0;
    for (auto& entry : result->bindings) entry.second.number = ++number;
    return result;
}

// Definitions by shell command. An option is one word name=value with the name of a profile field of the file.
bool validName(const std::string& value)
{
    return !value.empty() && value.size() <= MaxIdBytes &&
        std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                   c == '_' || c == '-' || c == '.';
        });
}
unsigned numberText(const std::string& text, unsigned low, unsigned high)
{
    Json node; node.kind = Json::Kind::Number; node.text = text;
    return number(node, low, high);
}
std::string plainText(const std::string& text)
{
    Json node; node.kind = Json::Kind::String; node.text = text;
    return string(node);
}
const Algorithm& algorithmNamed(const std::string& name, const std::map<std::string, Algorithm>& algorithms)
{
    auto found = algorithms.find(name);
    if (found == algorithms.end()) reject("native algorithm unavailable");
    return found->second;
}
// Sets one value of a profile from an option. The values are checked one by one here; what a version needs as a
// whole is checked when the sessions are completed.
void applyOption(Profile& profile, std::set<std::string>& given, const Capabilities& capabilities,
                 const std::string& word)
{
    const size_t equal = word.find('=');
    if (equal == std::string::npos || equal == 0) reject("option must be name=value");
    const std::string name = word.substr(0, equal), value = plainText(word.substr(equal + 1));
    if (name == "version") {
        if (value == "1") profile.version = Version::V1;
        else if (value == "2c") profile.version = Version::V2c;
        else if (value == "3") profile.version = Version::V3;
        else reject("protocol version rejected");
    } else if (name == "timeoutMs") profile.timeoutMs = numberText(value, 1, 60000);
    else if (name == "retries") profile.retries = numberText(value, 0, 10);
    else if (name == "maxVarbinds") profile.maxVarbinds = numberText(value, 1, 1024);
    else if (name == "maxMessageSize") profile.maxMessageSize = numberText(value, 484, 65507);
    else if (name == "checkRanges") {
        if (value != "true" && value != "false") reject("boolean option required");
        profile.checkRanges = value == "true";
    } else if (name == "communityFile") profile.community = secretFile(value, true);
    else if (name == "user") {
        if (value.empty() || value.size() > 32) reject("USM user rejected");
        profile.user = value;
    } else if (name == "securityLevel") {
        if (value == "noAuthNoPriv") profile.securityLevel = SecurityLevel::NoAuthNoPriv;
        else if (value == "authNoPriv") profile.securityLevel = SecurityLevel::AuthNoPriv;
        else if (value == "authPriv") profile.securityLevel = SecurityLevel::AuthPriv;
        else reject("security level rejected");
    } else if (name == "authAlgorithm") profile.authentication = algorithmNamed(value, capabilities.authentication);
    else if (name == "authSecretFile") profile.authSecret = secretFile(value, false);
    else if (name == "privAlgorithm") profile.privacy = algorithmNamed(value, capabilities.privacy);
    else if (name == "privSecretFile") profile.privSecret = secretFile(value, false);
    else if (name == "contextName") {
        if (value.size() > 255) reject("context name rejected");
        profile.contextName = value;
    } else if (name == "securityEngineId") profile.securityEngineId = engineText(value);
    else if (name == "contextEngineId") profile.contextEngineId = engineText(value);
    else reject("unknown option");
    given.insert(name);
}
// What a session must hold as a whole, by the rules a profile of the file follows.
void checkSession(const Profile& profile, const std::set<std::string>& given)
{
    auto has = [&](const char* name) { return given.count(name) != 0; };
    if (!has("version")) reject("session version absent");
    if (profile.version != Version::V3) {
        for (const char* name : {"user", "securityLevel", "authAlgorithm", "authSecretFile", "privAlgorithm",
                                 "privSecretFile", "contextName", "securityEngineId", "contextEngineId"})
            if (has(name)) reject("profile security fields rejected");
        if (!has("communityFile")) reject("required field absent");
        return;
    }
    if (has("communityFile")) reject("profile security fields rejected");
    if (!has("user") || !has("securityLevel")) reject("required field absent");
    if (profile.securityLevel != SecurityLevel::NoAuthNoPriv) {
        if (!has("authAlgorithm") || !has("authSecretFile")) reject("required field absent");
    } else if (has("authAlgorithm") || has("authSecretFile")) reject("unused authentication rejected");
    if (profile.securityLevel == SecurityLevel::AuthPriv) {
        if (!has("privAlgorithm") || !has("privSecretFile")) reject("required field absent");
    } else if (has("privAlgorithm") || has("privSecretFile")) reject("unused privacy rejected");
}
// The names of the values a profile of a file holds, for a file endpoint that takes options afterwards.
std::set<std::string> givenOf(const Profile& profile)
{
    std::set<std::string> given = {"version"};
    if (profile.version != Version::V3) { given.insert("communityFile"); return given; }
    given.insert("user"); given.insert("securityLevel");
    if (profile.securityLevel != SecurityLevel::NoAuthNoPriv) { given.insert("authAlgorithm"); given.insert("authSecretFile"); }
    if (profile.securityLevel == SecurityLevel::AuthPriv) { given.insert("privAlgorithm"); given.insert("privSecretFile"); }
    if (!profile.contextName.empty()) given.insert("contextName");
    if (!profile.securityEngineId.empty()) given.insert("securityEngineId");
    if (!profile.contextEngineId.empty()) given.insert("contextEngineId");
    return given;
}
// A numeric address alone or with a port: 192.0.2.1, 192.0.2.1:161, 2001:db8::1, [2001:db8::1]:161.
void parseAddress(const std::string& text, Endpoint& endpoint)
{
    std::string host = text, port;
    bool portGiven = false;
    unsigned char bytes[16];
    if (!text.empty() && text[0] == '[') {
        const size_t close = text.find(']');
        if (close == std::string::npos) reject("numeric device address rejected");
        host = text.substr(1, close - 1);
        if (close + 1 < text.size()) {
            if (text[close + 1] != ':') reject("numeric device address rejected");
            port = text.substr(close + 2); portGiven = true;
        }
    } else if (inet_pton(AF_INET6, text.c_str(), bytes) != 1) {
        const size_t colon = text.rfind(':');
        if (colon != std::string::npos) { host = text.substr(0, colon); port = text.substr(colon + 1); portGiven = true; }
    }
    char normalized[INET6_ADDRSTRLEN];
    const int family = inet_pton(AF_INET, host.c_str(), bytes) == 1 ? AF_INET : AF_INET6;
    if (inet_pton(family, host.c_str(), bytes) != 1 || !inet_ntop(family, bytes, normalized, sizeof(normalized)))
        reject("numeric device address rejected");
    endpoint.address = normalized;
    endpoint.port = portGiven ? numberText(port, 1, 65535) : 161u;
}
const char* versionName(Version version)
{ return version == Version::V1 ? "1" : version == Version::V2c ? "2c" : "3"; }
const char* levelName(SecurityLevel level)
{
    return level == SecurityLevel::NoAuthNoPriv ? "noAuthNoPriv" :
           level == SecurityLevel::AuthNoPriv ? "authNoPriv" : "authPriv";
}
const char* typeName(ValueType type)
{
    switch (type) {
    case ValueType::Integer: return "integer";
    case ValueType::Unsigned32: return "unsigned32";
    case ValueType::Counter32: return "counter32";
    case ValueType::Gauge32: return "gauge32";
    case ValueType::TimeTicks: return "timeticks";
    case ValueType::Counter64: return "counter64";
    case ValueType::Octets: return "octets";
    case ValueType::ObjectId: return "oid";
    case ValueType::IpAddress: return "ipAddress";
    case ValueType::OpaqueFloat: return "opaqueFloat";
    default: return "opaqueDouble";
    }
}
epicsThreadOnceId once = EPICS_THREAD_ONCE_INIT;
Config* owner = NULL;
void initialize(void*) { owner = new Config; }
}

Config::Config() : current(std::make_shared<Configuration>()) {}
Config& Config::instance() { epicsThreadOnce(&once, initialize, NULL); return *owner; }
void Config::load(const std::string& file, const std::string& nativeProbe)
{
    std::string worker;
    { epicsGuard<epicsMutex> guard(mutex); requireOpen(); worker = workerPath; }
    if (worker.empty()) reject("worker path required before load");
    std::string libraryPath;
    try { libraryPath = libraryPathOf(worker); } catch (const std::exception&) { reject("worker library listing failed"); }
    auto next = candidate(file, nativeProbe, environmentList(libraryPath, false));
    epicsGuard<epicsMutex> guard(mutex);
    requireOpen();
    if (workerPath != worker) reject("worker path changed during load");
    if (revision == std::numeric_limits<uint64_t>::max()) reject("configuration revision exhausted");
    if (!probePath.empty() && probePath != nativeProbe) reject("probe path changed");
    for (const auto& entry : sessions)
        if (next->endpoints.count(entry.first)) reject("name defined twice");
    probePath = nativeProbe;
    loaded = std::move(next);
    nextNumber = loaded->bindings.size();
    rebuild(false, nullptr);
    ++revision;
}
void Config::requireOpen() const
{
    if (frozen || bound) reject("configuration frozen");
}
// The capabilities the probe reports, observed once: the ones of the loaded file when there is one.
const Capabilities& Config::probedCapabilities()
{
    if (loaded) return loaded->capabilities;
    if (haveProbed) return probed;
    if (probePath.empty()) reject("probe path required before a session");
    if (workerPath.empty()) reject("worker path required before a session");
    std::string libraryPath;
    try { libraryPath = libraryPathOf(workerPath); } catch (const std::exception&) { reject("worker library listing failed"); }
    probed = capabilities(probePath, environmentList(libraryPath, false));
    haveProbed = true;
    return probed;
}
// Builds the configuration in force from the file and the sessions. With complete set, a session that does not
// hold what its version needs is left out and the first such reason is given.
void Config::rebuild(bool complete, std::string* reason)
{
    // Without a session the configuration in force is the one of the file itself, or the one in force when
    // there is no file either: no copy is made, so the object stays the same across a restart of the database.
    if (sessions.empty()) {
        if (loaded) current = loaded;
        return;
    }
    auto next = loaded ? std::make_shared<Configuration>(*loaded) : std::make_shared<Configuration>();
    if (!loaded && haveProbed) next->capabilities = probed;
    for (const auto& entry : sessions) {
        const Session& session = entry.second;
        if (complete) {
            try { checkSession(session.profile, session.given); }
            catch (const std::exception& error) {
                if (reason && reason->empty()) *reason = "session " + entry.first + ": " + error.what();
                next->endpoints.erase(entry.first);
                continue;
            }
        }
        // The profile of a session lives under a name of its own beside the profiles of the file.
        std::string name = entry.first;
        while (loaded && loaded->profiles.count(name)) name += ".session";
        Profile profile = session.profile; profile.id = name;
        Endpoint endpoint = session.endpoint; endpoint.profile = name;
        next->profiles[name] = std::move(profile);
        next->endpoints[entry.first] = std::move(endpoint);
    }
    current = std::move(next);
}
void Config::setProbePath(const std::string& path)
{
    if (path.empty() || path.front() != '/' || path.size() > 4096 || path.find('\0') != std::string::npos)
        reject("absolute probe path required");
    epicsGuard<epicsMutex> guard(mutex);
    if (path == probePath) return;
    requireOpen();
    if (!probePath.empty()) reject("probe path changed");
    probePath = path;
}
void Config::session(const std::string& name, const std::string& address, const std::vector<std::string>& words)
{
    epicsGuard<epicsMutex> guard(mutex);
    requireOpen();
    if (!validName(name)) reject("configuration identifier rejected");
    if (sessions.count(name) || current->endpoints.count(name)) reject("name defined twice");
    Session next;
    next.endpoint.id = name;
    parseAddress(address, next.endpoint);
    const Capabilities& capabilities = probedCapabilities();
    for (const auto& word : words) applyOption(next.profile, next.given, capabilities, word);
    if (sessions.size() >= 4096) reject("session count rejected");
    sessions.emplace(name, std::move(next));
    rebuild(false, nullptr);
    if (!revision) revision = 1;
}
void Config::options(const std::string& pattern, const std::vector<std::string>& words)
{
    epicsGuard<epicsMutex> guard(mutex);
    requireOpen();
    const Capabilities& capabilities = probedCapabilities();
    // The command acts on every session whose name matches: the ones a command defined and the endpoints of
    // the file, which become sessions of their own with the values of their profile.
    std::map<std::string, Session> changed;
    for (const auto& entry : current->endpoints) {
        if (!epicsStrGlobMatch(entry.first.c_str(), pattern.c_str())) continue;
        auto found = sessions.find(entry.first);
        Session next;
        if (found != sessions.end()) next = found->second;
        else {
            next.endpoint = entry.second;
            next.profile = current->profiles.at(entry.second.profile);
            next.given = givenOf(next.profile);
            next.fromFile = true;
        }
        for (const auto& word : words) applyOption(next.profile, next.given, capabilities, word);
        // An endpoint of the file is complete and its bindings name it, so it must stay complete: an option
        // that its profile cannot take fails the command and leaves the file's values.
        if (next.fromFile) checkSession(next.profile, next.given);
        changed.emplace(entry.first, std::move(next));
    }
    if (changed.empty()) reject("session absent");
    for (auto& entry : changed) sessions[entry.first] = std::move(entry.second);
    rebuild(false, nullptr);
}
void Config::finalizeLocked()
{
    if (bound) return;
    std::string reason;
    rebuild(true, &reason);
    bound = true;
    if (!reason.empty()) throw std::runtime_error(reason);
}
void Config::finalize() { epicsGuard<epicsMutex> guard(mutex); finalizeLocked(); }
std::shared_ptr<const Binding> Config::bind(const std::string& id)
{
    epicsGuard<epicsMutex> guard(mutex);
    // The sessions are completed for the lookup; only a binding that is found fixes the definitions.
    if (!bound) rebuild(true, nullptr);
    auto found = current->bindings.find(id);
    if (found == current->bindings.end()) reject("binding absent");
    bound = true;
    return std::make_shared<const Binding>(current, found->second);
}
std::shared_ptr<const Binding> Config::define(const std::string& session, const std::string& object,
                                              const std::string& type, Operation operation, size_t capacity,
                                              const std::string& group, bool systemInfo)
{
    epicsGuard<epicsMutex> guard(mutex);
    if (frozen) reject("configuration frozen");
    if (!bound) rebuild(true, nullptr);
    if (!current->endpoints.count(session)) reject("session absent");
    BindingDefinition binding;
    binding.endpoint = session;
    binding.oid = oid(object);
    binding.operation = operation;
    binding.valueType = valueType(type);
    const size_t maximum = binding.valueType == ValueType::Octets ? MaxConfigBytes :
        binding.valueType == ValueType::ObjectId ? MaxOidArcs : 1;
    binding.capacity = maximum == 1 ? 1 : capacity;
    if (binding.capacity < 1 || binding.capacity > maximum) reject("integer option out of range");
    if (!group.empty()) {
        if (!validName(group)) reject("configuration identifier rejected");
        binding.group = group;
    }
    binding.systemInfo = systemInfo;
    if (systemInfo && operation != Operation::Get) reject("binding systemInfo requires get");
    do { binding.id = "link." + std::to_string(++nextLink); } while (current->bindings.count(binding.id));
    if (current->bindings.size() >= 4096) reject("binding count rejected");
    binding.number = ++nextNumber;
    const std::string name = binding.id;
    auto placed = current->bindings.emplace(name, binding).first;
    try { validateGroups(*current); }
    catch (const std::exception&) { current->bindings.erase(placed); --nextNumber; throw; }
    bound = true;
    return std::make_shared<const Binding>(current, placed->second);
}
void Config::discard(const std::string& id)
{
    epicsGuard<epicsMutex> guard(mutex);
    if (frozen || id.compare(0, 5, "link.") != 0) return;
    current->bindings.erase(id);
}
void Config::show(const std::string& pattern, int verbosity)
{
    epicsGuard<epicsMutex> guard(mutex);
    for (const auto& entry : current->endpoints) {
        if (!epicsStrGlobMatch(entry.first.c_str(), pattern.c_str())) continue;
        if (verbosity < 1) { std::printf("snmp3 session: name=%s\n", entry.first.c_str()); continue; }
        const Endpoint& endpoint = entry.second;
        const Profile& profile = current->profiles.at(endpoint.profile);
        std::printf("snmp3 session: name=%s address=%s port=%u version=%s timeoutMs=%u retries=%u maxVarbinds=%u "
                    "maxMessageSize=%u checkRanges=%u", entry.first.c_str(), endpoint.address.c_str(), endpoint.port,
                    versionName(profile.version), profile.timeoutMs, profile.retries, profile.maxVarbinds,
                    profile.maxMessageSize, profile.checkRanges ? 1u : 0u);
        if (profile.version == Version::V3)
            std::printf(" user=%s securityLevel=%s authAlgorithm=%s privAlgorithm=%s contextName=%s",
                        profile.user.c_str(), levelName(profile.securityLevel),
                        profile.authentication.name.empty() ? "none" : profile.authentication.name.c_str(),
                        profile.privacy.name.empty() ? "none" : profile.privacy.name.c_str(),
                        profile.contextName.empty() ? "none" : profile.contextName.c_str());
        std::printf("\n");
        for (const auto& item : current->bindings) {
            const BindingDefinition& binding = item.second;
            if (binding.endpoint != entry.first || item.first.compare(0, 5, "link.") != 0) continue;
            std::string arcs;
            for (const auto arc : binding.oid) arcs += (arcs.empty() ? "" : ".") + std::to_string(arc);
            std::printf("snmp3 object: session=%s id=%s oid=%s type=%s operation=%s group=%s systemInfo=%u\n",
                        entry.first.c_str(), item.first.c_str(), arcs.c_str(), typeName(binding.valueType),
                        binding.operation == Operation::Get ? "get" : "set",
                        binding.group.empty() ? "none" : binding.group.c_str(), binding.systemInfo ? 1u : 0u);
        }
    }
}
void Config::freeze() { epicsGuard<epicsMutex> guard(mutex); frozen = true; }
void Config::setWorkerPath(const std::string& path)
{
    if(path.empty() || path.front()!='/' || path.size()>4096 || path.find('\0')!=std::string::npos)
        reject("absolute worker path required");
    epicsGuard<epicsMutex> guard(mutex);
    if(path==workerPath)return;
    if(frozen || bound)reject("worker path frozen");
    if(revision>0)reject("worker path fixed by a loaded configuration");
    workerPath=path;
}
ConfigState Config::snapshot()
{
    epicsGuard<epicsMutex> guard(mutex);
    return {current, revision, frozen || bound, workerPath};
}
void Config::report()
{
    const ConfigState state = snapshot();
    std::printf("snmp3 config: revision=%llu frozen=%u profiles=%zu endpoints=%zu bindings=%zu\n",
        static_cast<unsigned long long>(state.revision), state.frozen ? 1u : 0u,
        state.configuration->profiles.size(), state.configuration->endpoints.size(), state.configuration->bindings.size());
}
}
