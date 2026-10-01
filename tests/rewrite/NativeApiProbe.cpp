#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/snmpusm.h>
#include <net-snmp/library/keytools.h>
#include <net-snmp/library/large_fd_set.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr long NativeTimeoutUs = 100000;
constexpr unsigned ProbeDeadlineMs = 3000;
constexpr size_t HighFdCount = 1100;
const oid Object[] = {1, 3, 6, 1, 4, 1, 53864, 4, 1, 0};
using Clock = std::chrono::steady_clock;
const auto Start = Clock::now();
long long elapsedUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - Start).count();
}
struct Observation {
    std::string mode;
    unsigned received = 0, timeout = 0, resend = 0, security = 0, failed = 0;
    bool terminal = false;
    long value = 0;
};
int observe(int operation, netsnmp_session*, int request, netsnmp_pdu* pdu, void* data)
{
    auto& state = *static_cast<Observation*>(data);
    if (operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        ++state.received;
        if (pdu && pdu->command == SNMP_MSG_RESPONSE && pdu->variables &&
            pdu->variables->type == ASN_INTEGER && pdu->variables->val.integer)
            state.value = *pdu->variables->val.integer;
        state.terminal = state.mode != "return-zero" || state.received > 1;
    } else if (operation == NETSNMP_CALLBACK_OP_TIMED_OUT) {
        ++state.timeout; state.terminal = true;
    } else if (operation == NETSNMP_CALLBACK_OP_RESEND) {
        ++state.resend;
    } else if (operation == NETSNMP_CALLBACK_OP_SEC_ERROR) {
        ++state.security; state.terminal = true;
    } else if (operation == NETSNMP_CALLBACK_OP_SEND_FAILED) {
        ++state.failed; state.terminal = true;
    }
    const int result = state.mode == "return-zero" &&
        operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE && state.received == 1 ? 0 : 1;
    std::printf("{\"event\":\"native_callback\",\"operation\":%d,\"request_id\":%d,"
                "\"command\":%d,\"return\":%d,\"elapsed_us\":%lld}\n",
                operation, request, pdu ? pdu->command : -1, result, elapsedUs());
    return result;
}
std::string secret(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("probe fixture file unavailable");
    std::string value((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (!value.empty() && value.back() == '\n') value.pop_back();
    if (value.empty() || value.size() > 1024 || value.find('\0') != std::string::npos)
        throw std::runtime_error("probe fixture file invalid");
    return value;
}
void derive(netsnmp_session& session, const char* authPath, const char* privPath)
{
    const auto auth = secret(authPath);
    const auto privacy = secret(privPath);
    session.securityAuthProto = usmHMACSHA1AuthProtocol;
    session.securityAuthProtoLen = OID_LENGTH(usmHMACSHA1AuthProtocol);
    session.securityPrivProto = usmAESPrivProtocol;
    session.securityPrivProtoLen = OID_LENGTH(usmAESPrivProtocol);
    session.securityAuthKeyLen = sizeof(session.securityAuthKey);
    session.securityPrivKeyLen = sizeof(session.securityPrivKey);
    if (generate_Ku(session.securityAuthProto, session.securityAuthProtoLen,
                    reinterpret_cast<const unsigned char*>(auth.data()), auth.size(),
                    session.securityAuthKey, &session.securityAuthKeyLen) != SNMPERR_SUCCESS ||
        generate_Ku(session.securityAuthProto, session.securityAuthProtoLen,
                    reinterpret_cast<const unsigned char*>(privacy.data()), privacy.size(),
                    session.securityPrivKey, &session.securityPrivKeyLen) != SNMPERR_SUCCESS)
        throw std::runtime_error("probe native derivation failed");
}
std::vector<unsigned char> discover(const char* peer, const char* user)
{
    netsnmp_session probe;
    snmp_sess_init(&probe);
    probe.flags &= ~SNMP_FLAGS_DONT_PROBE;
    probe.peername = const_cast<char*>(peer);
    probe.version = SNMP_VERSION_3;
    probe.securityModel = SNMP_SEC_MODEL_USM;
    probe.securityLevel = SNMP_SEC_LEVEL_NOAUTH;
    probe.securityName = const_cast<char*>("");
    probe.securityAuthProto = usmNoAuthProtocol;
    probe.securityAuthProtoLen = OID_LENGTH(usmNoAuthProtocol);
    probe.securityPrivProto = usmNoPrivProtocol;
    probe.securityPrivProtoLen = OID_LENGTH(usmNoPrivProtocol);
    probe.timeout = NativeTimeoutUs;
    probe.retries = 1;
    void* handle = snmp_sess_open(&probe);
    std::printf("{\"event\":\"discovery_open\",\"success\":%s,\"elapsed_us\":%lld}\n",
                handle ? "true" : "false", elapsedUs());
    if (!handle) throw std::runtime_error("probe discovery open failed");
    const auto* resolved = snmp_sess_session(handle);
    if (!resolved || !resolved->securityEngineID || !resolved->securityEngineIDLen) {
        snmp_sess_close(handle);
        throw std::runtime_error("probe discovery did not resolve an engine");
    }
    std::vector<unsigned char> engine(resolved->securityEngineID,
                                     resolved->securityEngineID + resolved->securityEngineIDLen);
    const bool absent = !usm_get_user(engine.data(), engine.size(), user);
    std::printf("{\"event\":\"discovery\",\"engine_length\":%zu,\"candidate_user_absent\":%s}\n",
                engine.size(), absent ? "true" : "false");
    const int closed = snmp_sess_close(handle);
    std::printf("{\"event\":\"discovery_close\",\"return\":%d}\n", closed);
    return engine;
}
void service(void* handle, Observation& state)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(ProbeDeadlineMs);
    netsnmp_large_fd_set descriptors;
    netsnmp_large_fd_set_init(&descriptors, FD_SETSIZE);
    while (!state.terminal && Clock::now() < deadline) {
        NETSNMP_LARGE_FD_ZERO(&descriptors);
        int count = 0, block = 1;
        timeval timeout = {0, NativeTimeoutUs};
        snmp_sess_select_info2(handle, &count, &descriptors, &timeout, &block);
        if (block) timeout = {0, NativeTimeoutUs};
        const int result = netsnmp_large_fd_set_select(count, &descriptors, nullptr, nullptr, &timeout);
        if (result < 0 && errno != EINTR) break;
        if (result > 0) snmp_sess_read2(handle, &descriptors);
        snmp_sess_timeout(handle);
    }
    netsnmp_large_fd_set_cleanup(&descriptors);
}
}

int main(int argc, char** argv)
{
    if (argc != 8) {
        std::fprintf(stderr, "Usage: snmp3NativeApiProbe CASE PEER VERSION COMMUNITY_FILE AUTH_FILE PRIV_FILE USER\n");
        return 2;
    }
    setvbuf(stdout, nullptr, _IOLBF, 0);
    Observation state;
    state.mode = argv[1];
    std::vector<int> heldDescriptors;
    void* handle = nullptr;
    try {
        netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
        netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
        init_snmp("native-api-probe");
        if (state.mode == "high-fd") {
            rlimit limits;
            if (getrlimit(RLIMIT_NOFILE, &limits))
                throw std::runtime_error("probe descriptor limits unavailable");
            const rlim_t required = HighFdCount + FD_SETSIZE;
            if (limits.rlim_max < required)
                throw std::runtime_error("probe hard descriptor limit insufficient");
            if (limits.rlim_cur < required) {
                limits.rlim_cur = required;
                if (setrlimit(RLIMIT_NOFILE, &limits))
                    throw std::runtime_error("probe process descriptor limit failed");
            }
            for (size_t i = 0; i < HighFdCount; ++i) {
                const int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
                if (fd < 0) throw std::runtime_error("probe descriptor reservation failed");
                heldDescriptors.push_back(fd);
            }
        }
        netsnmp_session session;
        snmp_sess_init(&session);
        session.peername = argv[2];
        session.timeout = NativeTimeoutUs;
        session.retries = state.mode == "return-zero" ? 2 : 1;
        session.version = !std::strcmp(argv[3], "1") ? SNMP_VERSION_1 :
            !std::strcmp(argv[3], "3") ? SNMP_VERSION_3 : SNMP_VERSION_2c;
        std::string community;
        std::vector<unsigned char> discovered;
        if (session.version == SNMP_VERSION_3) {
            discovered = discover(argv[2], argv[7]);
            session.securityModel = SNMP_SEC_MODEL_USM;
            session.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
            session.securityName = argv[7];
            session.securityNameLen = std::strlen(argv[7]);
            session.securityEngineID = discovered.data();
            session.securityEngineIDLen = discovered.size();
            derive(session, argv[5], argv[6]);
        } else {
            community = secret(argv[4]);
            session.community = reinterpret_cast<unsigned char*>(&community[0]);
            session.community_len = community.size();
        }
        handle = snmp_sess_open(&session);
        std::printf("{\"event\":\"session_open\",\"success\":%s,\"elapsed_us\":%lld}\n",
                    handle ? "true" : "false", elapsedUs());
        if (!handle) throw std::runtime_error("probe session open failed");
        if (state.mode == "warm-discovery") {
            auto* before = usm_get_user(discovered.data(), discovered.size(), argv[7]);
            if (!before || !before->authKey || !before->privKey)
                throw std::runtime_error("probe cached keys unavailable");
            const std::vector<unsigned char> auth(before->authKey, before->authKey + before->authKeyLen);
            const std::vector<unsigned char> privacy(before->privKey, before->privKey + before->privKeyLen);
            const std::vector<oid> authProtocol(before->authProtocol, before->authProtocol + before->authProtocolLen);
            const std::vector<oid> privProtocol(before->privProtocol, before->privProtocol + before->privProtocolLen);
            const auto engine = discover(argv[2], argv[7]);
            const auto* after = usm_get_user(engine.data(), engine.size(), argv[7]);
            const bool preserved = engine == discovered && before == after &&
                after && auth == std::vector<unsigned char>(after->authKey, after->authKey + after->authKeyLen) &&
                privacy == std::vector<unsigned char>(after->privKey, after->privKey + after->privKeyLen) &&
                authProtocol == std::vector<oid>(after->authProtocol, after->authProtocol + after->authProtocolLen) &&
                privProtocol == std::vector<oid>(after->privProtocol, after->privProtocol + after->privProtocolLen);
            std::printf("{\"event\":\"warm_discovery\",\"cached_material_preserved\":%s}\n",
                        preserved ? "true" : "false");
        }
        int count = 0, block = 1;
        timeval timeout = {0, 0};
        netsnmp_large_fd_set descriptors;
        netsnmp_large_fd_set_init(&descriptors, FD_SETSIZE);
        NETSNMP_LARGE_FD_ZERO(&descriptors);
        snmp_sess_select_info2(handle, &count, &descriptors, &timeout, &block);
        std::printf("{\"event\":\"socket_interest\",\"nfds\":%d,\"fd_set_size\":%d}\n", count, FD_SETSIZE);
        netsnmp_large_fd_set_cleanup(&descriptors);
        auto* pdu = snmp_pdu_create(SNMP_MSG_GET);
        if (!pdu || !snmp_add_null_var(pdu, Object, OID_LENGTH(Object)))
            throw std::runtime_error("probe PDU allocation failed");
        if (state.mode == "send-fail") pdu->version = 999;
        const int sent = snmp_sess_async_send(handle, pdu, observe, &state);
        std::printf("{\"event\":\"native_send\",\"return\":%d,\"elapsed_us\":%lld}\n", sent, elapsedUs());
        if (!sent) {
            snmp_free_pdu(pdu);
            std::puts("{\"event\":\"caller_pdu_freed\"}");
        } else if (state.mode != "pending-close") {
            service(handle, state);
        }
        const int closed = snmp_sess_close(handle);
        handle = nullptr;
        std::printf("{\"event\":\"native_close\",\"return\":%d,\"elapsed_us\":%lld}\n", closed, elapsedUs());
        snmp_shutdown("native-api-probe");
        for (int fd : heldDescriptors) close(fd);
        std::printf("{\"event\":\"summary\",\"received\":%u,\"timeout\":%u,\"resend\":%u,"
                    "\"security\":%u,\"failed\":%u,\"value\":%ld}\n",
                    state.received, state.timeout, state.resend, state.security, state.failed, state.value);
        return 0;
    } catch (const std::exception& error) {
        if (handle) snmp_sess_close(handle);
        for (int fd : heldDescriptors) close(fd);
        snmp_shutdown("native-api-probe");
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
