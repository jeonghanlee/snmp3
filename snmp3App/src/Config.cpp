#include "Config.h"
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
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <epicsGuard.h>
#include <epicsThread.h>

extern char** environ;

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
std::vector<uint8_t> secret(const Json& node, const char* key, const std::string& directory, bool community)
{
    std::string path = string(node, key);
    if (path.empty()) reject("secret path rejected");
    if (path[0] != '/') path = directory + '/' + path;
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
std::vector<uint8_t> engine(const Json& node, const char* key)
{
    if (!present(node, key)) return {};
    std::string hex = string(node, key);
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

std::string observeCapabilities(const std::string& executable)
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
    if (!error) error = posix_spawn(&pid, executable.c_str(), &actions, NULL, args, environ);
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
Capabilities capabilities(const std::string& executable)
{
    const Json root = Json::parse(observeCapabilities(executable));
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

std::shared_ptr<Configuration> candidate(const std::string& file, const std::string& executable)
{
    char* resolved = realpath(file.c_str(), NULL);
    if (!resolved) reject("configuration path rejected");
    const std::string path(resolved); free(resolved);
    const std::string directory = path.substr(0, path.rfind('/'));
    const Json root = Json::parse(readFile(path, false));
    fields(root, {"schema", "profiles", "endpoints", "bindings"});
    number(required(root, "schema"), 1, 1);
    auto result = std::make_shared<Configuration>();
    result->capabilities = capabilities(executable);
    for (const Json& item : array(root, "profiles")) {
        fields(item, {"id", "version", "timeoutMs", "retries", "maxVarbinds", "checkRanges",
                      "communityFile", "user", "securityLevel", "authAlgorithm", "authSecretFile",
                      "privAlgorithm", "privSecretFile", "contextName", "securityEngineId", "contextEngineId"});
        Profile profile; profile.id = id(item, "id");
        profile.timeoutMs = option(item, "timeoutMs", 1000, 1, 60000);
        profile.retries = option(item, "retries", 3, 0, 10);
        profile.maxVarbinds = option(item, "maxVarbinds", 32, 1, 1024);
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
        fields(item, {"id", "endpoint", "oid", "operation", "valueType", "capacity"});
        BindingDefinition binding; binding.id = id(item, "id"); binding.endpoint = id(item, "endpoint");
        binding.oid = oid(string(item, "oid"));
        const std::string operation = string(item, "operation");
        if (operation == "get") binding.operation = Operation::Get;
        else if (operation == "set") binding.operation = Operation::Set;
        else reject("binding operation rejected");
        binding.valueType = valueType(string(item, "valueType"));
        const unsigned maximum = binding.valueType == ValueType::Octets ? MaxConfigBytes :
            binding.valueType == ValueType::ObjectId ? MaxOidArcs : 1;
        binding.capacity = option(item, "capacity", 1, 1, maximum);
        if (!result->endpoints.count(binding.endpoint)) reject("binding endpoint absent");
        const std::string name = binding.id;
        if (!result->bindings.emplace(name, std::move(binding)).second) reject("duplicate binding");
    }
    return result;
}
epicsThreadOnceId once = EPICS_THREAD_ONCE_INIT;
Config* owner = NULL;
void initialize(void*) { owner = new Config; }
}

Config::Config() : current(std::make_shared<Configuration>()) {}
Config& Config::instance() { epicsThreadOnce(&once, initialize, NULL); return *owner; }
void Config::load(const std::string& file, const std::string& nativeProbe)
{
    { epicsGuard<epicsMutex> guard(mutex); if (frozen) reject("configuration frozen"); }
    auto next = candidate(file, nativeProbe);
    epicsGuard<epicsMutex> guard(mutex);
    if (frozen) reject("configuration frozen");
    if (revision == std::numeric_limits<uint64_t>::max()) reject("configuration revision exhausted");
    current = std::move(next);
    ++revision;
}
std::shared_ptr<const Binding> Config::bind(const std::string& id)
{
    epicsGuard<epicsMutex> guard(mutex);
    auto found = current->bindings.find(id);
    if (found == current->bindings.end()) reject("binding absent");
    auto result = std::make_shared<const Binding>(current, found->second);
    frozen = true;
    return result;
}
void Config::freeze() { epicsGuard<epicsMutex> guard(mutex); frozen = true; }
void Config::setWorkerPath(const std::string& path)
{
    if(path.empty() || path.front()!='/' || path.size()>4096 || path.find('\0')!=std::string::npos)
        reject("absolute worker path required");
    epicsGuard<epicsMutex> guard(mutex);
    if(path==workerPath)return;
    if(frozen)reject("worker path frozen");
    workerPath=path;
}
ConfigState Config::snapshot()
{
    epicsGuard<epicsMutex> guard(mutex);
    return {current, revision, frozen, workerPath};
}
void Config::report()
{
    const ConfigState state = snapshot();
    std::printf("snmp3 config: revision=%llu frozen=%u profiles=%zu endpoints=%zu bindings=%zu\n",
        static_cast<unsigned long long>(state.revision), state.frozen ? 1u : 0u,
        state.configuration->profiles.size(), state.configuration->endpoints.size(), state.configuration->bindings.size());
}
}
