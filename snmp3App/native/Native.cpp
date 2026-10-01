#include "NativeInternal.h"
#include "Capabilities.h"
#include <net-snmp/library/keytools.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/snmpusm.h>
#include <net-snmp/library/large_fd_set.h>
#include <arpa/inet.h>
#include <algorithm>
#include <cstring>
#include <limits>

namespace snmp3 { namespace detail {
namespace {
using Bytes = std::vector<uint8_t>;
using Oids = std::vector<oid>;
struct FreePdu { void operator()(netsnmp_pdu* value) const { snmp_free_pdu(value); } };
using Pdu = std::unique_ptr<netsnmp_pdu, FreePdu>;
template<typename T> std::vector<T> copy(const T* data, size_t size)
{
    if (!size) return {};
    if (!data) throw NativeError(NativeOutcome::ResponseRejected, "native storage absent");
    return std::vector<T>(data, data + size);
}
Oids nativeOid(const std::vector<uint32_t>& value) { return Oids(value.begin(), value.end()); }
void requireOid(const std::vector<uint32_t>& value)
{
    if (value.size() < 2 || value.size() > MAX_OID_LEN || value[0] > 2 ||
        (value[0] < 2 && value[1] > 39) ||
        static_cast<uint64_t>(value[0]) * 40 + value[1] > MAX_SUBID ||
        std::any_of(value.begin(), value.end(), [](uint32_t arc) { return arc > MAX_SUBID; }))
        throw NativeError(NativeOutcome::InvalidRequest, "native OID representation rejected");
}
bool sameAlgorithm(const Algorithm& a, const Algorithm& b)
{
    return a.name == b.name && a.nativeType == b.nativeType && a.oid == b.oid;
}
bool compatible(const SecurityMaterial& a, const SecurityMaterial& b)
{
    return a.level == b.level && (a.level == SecurityLevel::NoAuthNoPriv ||
        (sameAlgorithm(a.authentication, b.authentication) && a.authSecret == b.authSecret)) &&
        (a.level != SecurityLevel::AuthPriv ||
        (sameAlgorithm(a.privacy, b.privacy) && a.privSecret == b.privSecret));
}
bool sameProfile(const Profile& a, const Profile& b)
{
    return a.id == b.id && a.version == b.version && a.securityLevel == b.securityLevel &&
        a.user == b.user && a.contextName == b.contextName && a.securityEngineId == b.securityEngineId &&
        a.contextEngineId == b.contextEngineId && a.timeoutMs == b.timeoutMs && a.retries == b.retries &&
        a.maxVarbinds == b.maxVarbinds && a.checkRanges == b.checkRanges && a.community == b.community &&
        a.authSecret == b.authSecret && a.privSecret == b.privSecret &&
        sameAlgorithm(a.authentication, b.authentication) && sameAlgorithm(a.privacy, b.privacy);
}
Oids authProtocol(const Profile& profile)
{
    return profile.securityLevel == SecurityLevel::NoAuthNoPriv ?
        Oids(usmNoAuthProtocol, usmNoAuthProtocol + OID_LENGTH(usmNoAuthProtocol)) :
        nativeOid(profile.authentication.oid);
}
Oids privProtocol(const Profile& profile)
{
    return profile.securityLevel != SecurityLevel::AuthPriv ?
        Oids(usmNoPrivProtocol, usmNoPrivProtocol + OID_LENGTH(usmNoPrivProtocol)) :
        nativeOid(profile.privacy.oid);
}
Bytes localized(const Profile& profile, const Bytes& secret, const Bytes& engine, bool privacy)
{
    auto protocol = authProtocol(profile);
    unsigned char master[USM_LENGTH_KU_HASHBLOCK];
    size_t masterSize = sizeof(master);
    if (generate_Ku(protocol.data(), protocol.size(), secret.data(), secret.size(), master,
                    &masterSize) != SNMPERR_SUCCESS)
        throw NativeError(NativeOutcome::SecurityFailure, "native master key derivation failed");
    unsigned char* storage = static_cast<unsigned char*>(calloc(1, USM_PRIV_KU_LEN));
    if (!storage) throw std::bad_alloc();
    size_t size = USM_PRIV_KU_LEN;
    int status = generate_kul(protocol.data(), protocol.size(), engine.data(), engine.size(),
                              master, masterSize, storage, &size);
    if (status == SNMPERR_SUCCESS && privacy) {
        const auto oidValue = privProtocol(profile);
        const auto* info = sc_get_priv_alg_byoid(oidValue.data(), oidValue.size());
        if (!info) { free(storage); throw NativeError(NativeOutcome::SecurityFailure, "native privacy metadata absent"); }
        status = netsnmp_extend_kul(info->proper_length, protocol.data(), protocol.size(),
            info->type, const_cast<unsigned char*>(engine.data()), engine.size(), &storage, &size, USM_PRIV_KU_LEN);
    }
    if (status != SNMPERR_SUCCESS) {
        free(storage);
        throw NativeError(NativeOutcome::SecurityFailure, "native localized key derivation failed");
    }
    std::unique_ptr<unsigned char, decltype(&free)> owner(storage, free);
    return Bytes(storage, storage + size);
}
bool cachedMaterial(const Profile& profile, const Bytes& engine, const SecurityMaterial& material)
{
    const auto* user = usm_get_user(engine.data(), engine.size(), profile.user.c_str());
    if (!user) return true;
    return copy(user->authProtocol, user->authProtocolLen) == authProtocol(profile) &&
        copy(user->privProtocol, user->privProtocolLen) == privProtocol(profile) &&
        copy(user->authKey, user->authKeyLen) == material.authKey &&
        copy(user->privKey, user->privKeyLen) == material.privKey;
}
std::string peer(const Endpoint& endpoint)
{
    unsigned char address[sizeof(in6_addr)];
    if (endpoint.port == 0 || endpoint.port > 65535)
        throw NativeError(NativeOutcome::OpenFailure, "native endpoint port invalid");
    if (inet_pton(AF_INET, endpoint.address.c_str(), address) == 1)
        return "udp:" + endpoint.address + ":" + std::to_string(endpoint.port);
    if (inet_pton(AF_INET6, endpoint.address.c_str(), address) == 1)
        return "udp6:[" + endpoint.address + "]:" + std::to_string(endpoint.port);
    throw NativeError(NativeOutcome::OpenFailure, "native endpoint requires a numeric address");
}
Bytes discover(Service& owner, const Profile& profile, std::string& address)
{
    netsnmp_session probe;
    snmp_sess_init(&probe);
    probe.flags &= ~SNMP_FLAGS_DONT_PROBE;
    probe.peername = &address[0];
    probe.version = SNMP_VERSION_3;
    probe.timeout = static_cast<long>(profile.timeoutMs) * 1000;
    probe.retries = profile.retries;
    probe.securityModel = SNMP_SEC_MODEL_USM;
    probe.securityLevel = SNMP_SEC_LEVEL_NOAUTH;
    probe.securityName = const_cast<char*>("");
    probe.securityAuthProto = usmNoAuthProtocol;
    probe.securityAuthProtoLen = OID_LENGTH(usmNoAuthProtocol);
    probe.securityPrivProto = usmNoPrivProtocol;
    probe.securityPrivProtoLen = OID_LENGTH(usmNoPrivProtocol);
    void* handle = snmp_sess_open(&probe);
    owner.event("discovery_open", 0, 0, 0, handle ? 1 : 0);
    if (!handle) throw NativeError(NativeOutcome::OpenFailure, "native discovery failed");
    Bytes engine;
    try {
        auto* session = snmp_sess_session(handle);
        if (!session || session->securityEngineIDLen < 5 || session->securityEngineIDLen > 32)
            throw NativeError(NativeOutcome::OpenFailure, "native discovery engine invalid");
        engine = copy(session->securityEngineID, session->securityEngineIDLen);
    } catch (...) {
        snmp_sess_close(handle);
        throw;
    }
    owner.event("discovery_close", 0, 0, 0, snmp_sess_close(handle));
    return engine;
}
void requireAlgorithm(const Algorithm& algorithm, const std::map<std::string, Algorithm>& catalogue)
{
    const auto found = catalogue.find(algorithm.name);
    if (found == catalogue.end() || !sameAlgorithm(algorithm, found->second))
        throw NativeError(NativeOutcome::CapabilityMismatch, "native profile algorithm mismatch");
}
Value responseValue(const netsnmp_variable_list& variable, const BindingDefinition& expected)
{
    const auto type = expected.valueType;
    if (variable.type == SNMP_NOSUCHOBJECT || variable.type == SNMP_NOSUCHINSTANCE ||
        variable.type == SNMP_ENDOFMIBVIEW) {
        if (variable.val_len) throw NativeError(NativeOutcome::ResponseRejected, "native exception length invalid");
        return Value::exception(variable.type == SNMP_NOSUCHOBJECT ? ValueType::NoSuchObject :
            variable.type == SNMP_NOSUCHINSTANCE ? ValueType::NoSuchInstance : ValueType::EndOfMibView);
    }
    switch (variable.type) {
    case ASN_INTEGER:
        if (type == ValueType::Integer && variable.val_len == sizeof(long) && variable.val.integer &&
            *variable.val.integer >= INT32_MIN && *variable.val.integer <= INT32_MAX)
            return Value::integer(*variable.val.integer);
        break;
    case ASN_COUNTER:
    case ASN_GAUGE:
    case ASN_TIMETICKS: {
        unsigned long scalar = 0;
        if (variable.val_len == sizeof(scalar) && variable.val.integer)
            std::memcpy(&scalar, variable.val.integer, sizeof(scalar));
        if (((variable.type == ASN_COUNTER && type == ValueType::Counter32) ||
             (variable.type == ASN_GAUGE && (type == ValueType::Gauge32 || type == ValueType::Unsigned32)) ||
             (variable.type == ASN_TIMETICKS && type == ValueType::TimeTicks)) &&
             variable.val_len == sizeof(scalar) && variable.val.integer && scalar <= UINT32_MAX)
            return Value::unsigned32(type, static_cast<uint32_t>(scalar));
        break;
    }
    case ASN_COUNTER64:
        if (type == ValueType::Counter64 && variable.val_len == sizeof(counter64) && variable.val.counter64 &&
            variable.val.counter64->high <= UINT32_MAX && variable.val.counter64->low <= UINT32_MAX)
            return Value::counter64((static_cast<uint64_t>(variable.val.counter64->high) << 32) |
                                    variable.val.counter64->low);
        break;
    case ASN_OCTET_STR:
        if (type == ValueType::Octets && variable.val_len <= expected.capacity)
            return Value::octets(copy(variable.val.string, variable.val_len));
        break;
    case ASN_OBJECT_ID:
        if (type == ValueType::ObjectId && variable.val_len % sizeof(oid) == 0 &&
            variable.val_len / sizeof(oid) <= expected.capacity && variable.val_len / sizeof(oid) <= MAX_OID_LEN) {
            const auto native = copy(variable.val.objid, variable.val_len / sizeof(oid));
            std::vector<uint32_t> value;
            for (auto arc : native) {
                if (arc > UINT32_MAX) throw NativeError(NativeOutcome::ResponseRejected, "native OID width invalid");
                value.push_back(static_cast<uint32_t>(arc));
            }
            return Value::objectId(std::move(value));
        }
        break;
    case ASN_IPADDRESS:
        if (type == ValueType::IpAddress && variable.val_len == 4 && variable.val.string) {
            std::array<uint8_t, 4> value;
            std::copy(variable.val.string, variable.val.string + 4, value.begin());
            return Value::ipAddress(value);
        }
        break;
    case ASN_OPAQUE_FLOAT:
        if (type == ValueType::OpaqueFloat && variable.val_len == sizeof(float) && variable.val.floatVal)
            return Value::opaqueFloat(*variable.val.floatVal);
        break;
    case ASN_OPAQUE_DOUBLE:
        if (type == ValueType::OpaqueDouble && variable.val_len == sizeof(double) && variable.val.doubleVal)
            return Value::opaqueDouble(*variable.val.doubleVal);
        break;
    }
    throw NativeError(NativeOutcome::ResponseRejected, "native response type or storage invalid");
}
void append(netsnmp_pdu& pdu, const BindingDefinition& binding, const Value& value)
{
    if (value.type() != binding.valueType)
        throw NativeError(NativeOutcome::InvalidRequest, "native SET type mismatch");
    auto object = nativeOid(binding.oid);
    const void* data = nullptr;
    size_t length = 0;
    unsigned char tag = ASN_NULL;
    long integer = 0;
    unsigned long unsignedValue = 0;
    counter64 wide;
    float single;
    double precise;
    std::array<oid,MAX_OID_LEN> oidValue{};
    static_assert(MAX_OID_LEN==128 && sizeof(oid)<=8,"native OID scratch exceeds generation inventory");
    std::array<uint8_t, 4> ip;
    switch (value.type()) {
    case ValueType::Integer:
        if (value.integer() < INT32_MIN || value.integer() > INT32_MAX)
            throw NativeError(NativeOutcome::InvalidRequest, "native SET integer width invalid");
        integer = static_cast<long>(value.integer()); tag = ASN_INTEGER; data = &integer; length = sizeof(integer); break;
    case ValueType::Counter32:
    case ValueType::Gauge32:
    case ValueType::Unsigned32:
    case ValueType::TimeTicks:
        unsignedValue = value.unsigned32(); data = &unsignedValue; length = sizeof(unsignedValue);
        tag = value.type() == ValueType::Counter32 ? ASN_COUNTER :
            value.type() == ValueType::TimeTicks ? ASN_TIMETICKS : ASN_GAUGE; break;
    case ValueType::Counter64:
        wide.high = value.counter64() >> 32; wide.low = value.counter64() & UINT32_MAX;
        tag = ASN_COUNTER64; data = &wide; length = sizeof(wide); break;
    case ValueType::Octets:
        if (value.octets().size() > binding.capacity)
            throw NativeError(NativeOutcome::InvalidRequest, "native SET octet capacity exceeded");
        tag = ASN_OCTET_STR; data = value.octets().data(); length = value.octets().size(); break;
    case ValueType::ObjectId:
        requireOid(value.objectId());
        if (value.objectId().size() > binding.capacity)
            throw NativeError(NativeOutcome::InvalidRequest, "native SET OID invalid");
        std::copy(value.objectId().begin(),value.objectId().end(),oidValue.begin());
        tag = ASN_OBJECT_ID; data = oidValue.data(); length = value.objectId().size() * sizeof(oid); break;
    case ValueType::IpAddress:
        ip = value.ipAddress(); tag = ASN_IPADDRESS; data = ip.data(); length = ip.size(); break;
    case ValueType::OpaqueFloat:
        single = value.opaqueFloat(); tag = ASN_OPAQUE_FLOAT; data = &single; length = sizeof(single); break;
    case ValueType::OpaqueDouble:
        precise = value.opaqueDouble(); tag = ASN_OPAQUE_DOUBLE; data = &precise; length = sizeof(precise); break;
    default: throw NativeError(NativeOutcome::InvalidRequest, "native SET exception invalid");
    }
    if (!snmp_pdu_add_variable(&pdu, object.data(), object.size(), tag,
                              static_cast<const unsigned char*>(data), length)) throw std::bad_alloc();
}
int callback(int operation, netsnmp_session*, int requestId, netsnmp_pdu* pdu, void* data) noexcept
{
    auto& request = *static_cast<Request*>(data);
    auto& session = *request.session;
    auto& owner = *session.owner;
    try {
        owner.event("native_callback", session.identity.session, request.token, operation, requestId);
        if (request.terminal) return 1;
        NativeResult result;
        if (session.closing) result.outcome = NativeOutcome::Cancelled;
        else if (operation == NETSNMP_CALLBACK_OP_RESEND || operation == NETSNMP_CALLBACK_OP_CONNECT)
            return 0;
        else if (operation == NETSNMP_CALLBACK_OP_TIMED_OUT) result.outcome = NativeOutcome::Timeout;
        else if (operation == NETSNMP_CALLBACK_OP_SEC_ERROR) result.outcome = NativeOutcome::SecurityFailure;
        else if (operation == NETSNMP_CALLBACK_OP_SEND_FAILED) result.outcome = NativeOutcome::SendFailure;
        else if (operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
            if (!pdu || pdu->command != SNMP_MSG_RESPONSE) return 0;
            if (session.profile.version == Version::V3 &&
                (copy(pdu->securityEngineID, pdu->securityEngineIDLen) != session.identity.securityEngineId ||
                 copy(pdu->contextEngineID, pdu->contextEngineIDLen) != session.identity.contextEngineId ||
                 std::string(pdu->contextName ? pdu->contextName : "", pdu->contextNameLen) != session.identity.contextName))
                throw NativeError(NativeOutcome::ResponseRejected, "native response identity invalid");
            result.errorStatus = pdu->errstat; result.errorIndex = pdu->errindex;
            if (pdu->errstat != SNMP_ERR_NOERROR) {
                result.outcome = NativeOutcome::ProtocolFailure;
                if (pdu->errindex < 0 || static_cast<size_t>(pdu->errindex) > request.bindings.size())
                    result.outcome = NativeOutcome::ResponseRejected;
            } else {
                auto* variable = pdu->variables;
                for (auto& binding : request.bindings) {
                    const auto& expected = binding->definition();
                    const auto object = nativeOid(expected.oid);
                    if (!variable || copy(variable->name, variable->name_length) != object)
                        throw NativeError(NativeOutcome::ResponseRejected, "native response positional OID mismatch");
                    result.variables.push_back({expected.oid, responseValue(*variable, expected)});
                    variable = variable->next_variable;
                }
                if (variable) throw NativeError(NativeOutcome::ResponseRejected, "native response extra variable");
            }
        } else return 0;
        owner.finish(request, std::move(result));
        return 1;
    } catch (...) {
        try {
            NativeResult result;
            result.outcome = NativeOutcome::ResponseRejected;
            if (pdu) { result.errorStatus = pdu->errstat; result.errorIndex = pdu->errindex; }
            owner.finish(request, std::move(result));
        } catch (...) { std::terminate(); }
        return 1;
    }
}
}
Session::~Session()
{
    if(owner && reservation) { owner->release(reservation); --owner->liveCount; }
}
std::shared_ptr<Session> open(Service& owner, const Profile& profile, const Endpoint& endpoint,
                              const Capabilities& expected)
{
    constexpr uint64_t SessionCharge=32768, AssociationCharge=32768;
    struct Reservation {
        Service& owner;
        uint64_t bytes;
        explicit Reservation(Service& value,uint64_t count) : owner(value),bytes(count) { owner.reserve(bytes); }
        ~Reservation() { if(bytes)owner.release(bytes); }
    };
    if(owner.bounded) {
        owner.sessions.erase(std::remove_if(owner.sessions.begin(),owner.sessions.end(),[](const std::weak_ptr<Session>& s) { return s.expired(); }),owner.sessions.end());
        if(owner.liveCount>=1024)throw NativeError(NativeOutcome::InvalidRequest,"native session bound reached");
    }
    Reservation sessionReservation(owner,owner.bounded ? SessionCharge:0);
    const bool potentialAssociation=owner.bounded && profile.version==Version::V3 &&
        (profile.securityEngineId.empty() || !owner.security.count(SecurityTuple(profile.securityEngineId,profile.user)));
    if(potentialAssociation && owner.pinnedCount>=1024)throw NativeError(NativeOutcome::InvalidRequest,"native association bound reached");
    Reservation associationReservation(owner,potentialAssociation ? AssociationCharge:0);
    const auto observed = nativeCapabilities();
    if (!sameCapabilities(expected, observed)) {
        owner.event("capability_mismatch");
        throw NativeError(NativeOutcome::CapabilityMismatch, "native capability observation mismatch");
    }
    std::string address = peer(endpoint);
    if (profile.timeoutMs == 0 || profile.timeoutMs > static_cast<unsigned long>(LONG_MAX / 1000) ||
        profile.retries > INT_MAX || profile.maxVarbinds == 0 || endpoint.profile != profile.id)
        throw NativeError(NativeOutcome::OpenFailure, "native profile settings invalid");
    auto result = std::make_shared<Session>();
    result->owner = &owner;
    result->reservation=sessionReservation.bytes; sessionReservation.bytes=0;
    if(result->reservation)++owner.liveCount;
    result->profile = profile;
    auto& identity = result->identity;
    identity.session = owner.nextSession++; identity.endpoint = endpoint;
    identity.profile = profile.id; identity.version = profile.version; identity.securityLevel = profile.securityLevel;
    identity.user = profile.user; identity.contextName = profile.contextName;
    identity.authentication = profile.authentication; identity.privacy = profile.privacy;
    netsnmp_session native;
    snmp_sess_init(&native);
    native.peername = &address[0];
    native.version = profile.version == Version::V1 ? SNMP_VERSION_1 :
        profile.version == Version::V2c ? SNMP_VERSION_2c : SNMP_VERSION_3;
    native.timeout = static_cast<long>(profile.timeoutMs) * 1000; native.retries = profile.retries;
    Oids auth, privacy;
    SecurityMaterial material;
    if (profile.version == Version::V3) {
        if (profile.user.empty() || profile.user.size() > 32 || profile.contextName.size() > 255 ||
            (!profile.securityEngineId.empty() && (profile.securityEngineId.size() < 5 || profile.securityEngineId.size() > 32)) ||
            (!profile.contextEngineId.empty() && (profile.contextEngineId.size() < 5 || profile.contextEngineId.size() > 32)))
            throw NativeError(NativeOutcome::OpenFailure, "native security identity invalid");
        if (profile.securityLevel != SecurityLevel::NoAuthNoPriv)
            requireAlgorithm(profile.authentication, observed.authentication);
        if (profile.securityLevel == SecurityLevel::AuthPriv)
            requireAlgorithm(profile.privacy, observed.privacy);
        identity.securityEngineId = profile.securityEngineId.empty() ? discover(owner, profile, address) : profile.securityEngineId;
        identity.contextEngineId = profile.contextEngineId.empty() ? identity.securityEngineId : profile.contextEngineId;
        const SecurityTuple tuple(identity.securityEngineId, profile.user);
        material.level = profile.securityLevel;
        material.authentication = profile.authentication; material.privacy = profile.privacy;
        material.authSecret = profile.authSecret; material.privSecret = profile.privSecret;
        const auto existing = owner.security.find(tuple);
        if (existing != owner.security.end() && !compatible(existing->second, material)) {
            Profile pinned;
            pinned.user = profile.user; pinned.securityLevel = existing->second.level;
            pinned.authentication = existing->second.authentication; pinned.privacy = existing->second.privacy;
            const bool preserved = cachedMaterial(pinned, identity.securityEngineId, existing->second);
            owner.event("pinned_material_preserved", identity.session, 0, 0, preserved ? 1 : 0);
            owner.event("security_conflict", identity.session);
            throw NativeError(NativeOutcome::SecurityConflict, "native USM security material conflicts");
        }
        if (profile.securityLevel != SecurityLevel::NoAuthNoPriv)
            material.authKey = localized(profile, profile.authSecret, identity.securityEngineId, false);
        if (profile.securityLevel == SecurityLevel::AuthPriv)
            material.privKey = localized(profile, profile.privSecret, identity.securityEngineId, true);
        if ((existing != owner.security.end() &&
            (existing->second.authKey != material.authKey || existing->second.privKey != material.privKey)) ||
            !cachedMaterial(profile, identity.securityEngineId, material)) {
            owner.event("security_conflict", identity.session);
            throw NativeError(NativeOutcome::SecurityConflict, "native USM cached material conflicts");
        }
        const bool inserted=owner.security.emplace(tuple, material).second;
        if(inserted && owner.bounded) { ++owner.pinnedCount; associationReservation.bytes=0; }
        owner.event("security_material_checked", identity.session);
        native.securityModel = SNMP_SEC_MODEL_USM;
        native.securityLevel = profile.securityLevel == SecurityLevel::NoAuthNoPriv ? SNMP_SEC_LEVEL_NOAUTH :
            profile.securityLevel == SecurityLevel::AuthNoPriv ? SNMP_SEC_LEVEL_AUTHNOPRIV : SNMP_SEC_LEVEL_AUTHPRIV;
        native.securityName = const_cast<char*>(profile.user.c_str()); native.securityNameLen = profile.user.size();
        native.contextName = const_cast<char*>(profile.contextName.c_str()); native.contextNameLen = profile.contextName.size();
        native.securityEngineID = identity.securityEngineId.data(); native.securityEngineIDLen = identity.securityEngineId.size();
        native.contextEngineID = identity.contextEngineId.data(); native.contextEngineIDLen = identity.contextEngineId.size();
        auth = authProtocol(profile); privacy = privProtocol(profile);
        native.securityAuthProto = auth.data(); native.securityAuthProtoLen = auth.size();
        native.securityPrivProto = privacy.data(); native.securityPrivProtoLen = privacy.size();
        if (profile.securityLevel != SecurityLevel::NoAuthNoPriv) {
            native.securityAuthKeyLen = sizeof(native.securityAuthKey);
            if (generate_Ku(auth.data(), auth.size(), profile.authSecret.data(), profile.authSecret.size(),
                            native.securityAuthKey, &native.securityAuthKeyLen) != SNMPERR_SUCCESS)
                throw NativeError(NativeOutcome::SecurityFailure, "native authentication key derivation failed");
        }
        if (profile.securityLevel == SecurityLevel::AuthPriv) {
            native.securityPrivKeyLen = sizeof(native.securityPrivKey);
            if (generate_Ku(auth.data(), auth.size(), profile.privSecret.data(), profile.privSecret.size(),
                            native.securityPrivKey, &native.securityPrivKeyLen) != SNMPERR_SUCCESS)
                throw NativeError(NativeOutcome::SecurityFailure, "native privacy key derivation failed");
        }
    } else {
        if (profile.community.empty()) throw NativeError(NativeOutcome::OpenFailure, "native community absent");
        native.community = const_cast<unsigned char*>(profile.community.data()); native.community_len = profile.community.size();
    }
    void* handle = snmp_sess_open(&native);
    std::unique_ptr<void, decltype(&snmp_sess_close)> handleOwner(handle, snmp_sess_close);
    owner.event("native_open", identity.session, 0, 0, handle ? 1 : 0);
    if (!handle) throw NativeError(NativeOutcome::OpenFailure, "native session open failed");
    if (profile.version == Version::V3) {
        const bool matches = cachedMaterial(profile, identity.securityEngineId, material);
        owner.event("native_material_matches", identity.session, 0, 0, matches ? 1 : 0);
        if (!matches) throw NativeError(NativeOutcome::SecurityConflict, "native registered material mismatch");
    }
    netsnmp_large_fd_set descriptors;
    netsnmp_large_fd_set_init(&descriptors, FD_SETSIZE);
    NETSNMP_LARGE_FD_ZERO(&descriptors);
    int count = 0, block = 1;
    timeval timeout = {0, 0};
    snmp_sess_select_info2(handle, &count, &descriptors, &timeout, &block);
    netsnmp_large_fd_set_cleanup(&descriptors);
    owner.event("socket_interest", identity.session, 0, 0, count);
    const auto* transport = snmp_sess_transport(handle);
    owner.event("socket_descriptor", identity.session, 0, 0, transport ? transport->sock : -1);
    owner.sessions.emplace_back(result);
    result->handle = handleOwner.release();
    return result;
}
uint64_t submit(Session& session, const std::vector<std::shared_ptr<const Binding>>& bindings,
                const std::vector<Value>& payload, Native::Completion completion)
{
    auto& owner = *session.owner;
    owner.check();
    if (!owner.accepting || session.closing || !session.handle || bindings.empty() ||
        bindings.size() > session.profile.maxVarbinds || !completion || !bindings.front())
        throw NativeError(NativeOutcome::InvalidRequest, "native request not admitted");
    const auto operation = bindings.front()->definition().operation;
    if ((operation == Operation::Set && payload.size() != bindings.size()) ||
        (operation == Operation::Get && !payload.empty()))
        throw NativeError(NativeOutcome::InvalidRequest, "native request payload count mismatch");
    auto request = std::unique_ptr<Request>(new Request);
    request->session = &session; request->token = owner.nextToken++;
    request->operation = operation; request->bindings = bindings;
    request->payload = payload; request->completion = std::move(completion);
    Pdu pdu(snmp_pdu_create(operation == Operation::Get ? SNMP_MSG_GET : SNMP_MSG_SET));
    if (!pdu) throw std::bad_alloc();
    for (size_t index = 0; index < bindings.size(); ++index) {
        const auto& binding = bindings[index];
        if (!binding || binding->definition().operation != operation ||
            binding->endpoint().id != session.identity.endpoint.id || !sameProfile(binding->profile(), session.profile) ||
            binding->endpoint().address != session.identity.endpoint.address ||
            binding->endpoint().port != session.identity.endpoint.port ||
            !sameCapabilities(binding->capabilities(), nativeCapabilities()))
            throw NativeError(NativeOutcome::InvalidRequest, "native binding session mismatch");
        requireOid(binding->definition().oid);
        if (operation == Operation::Set) append(*pdu, binding->definition(), request->payload[index]);
        else {
            const auto object = nativeOid(binding->definition().oid);
            if (!snmp_add_null_var(pdu.get(), object.data(), object.size())) throw std::bad_alloc();
        }
    }
    const uint64_t token = request->token;
    auto* context = request.get();
    session.requests.emplace(token, std::move(request));
    owner.enter();
    const int sent = snmp_sess_async_send(session.handle, pdu.get(), callback, context);
    owner.event("native_send", session.identity.session, token, 0, sent);
    if (sent) pdu.release();
    else {
        pdu.reset();
        owner.event("caller_pdu_freed", session.identity.session, token);
        NativeResult result; result.outcome = NativeOutcome::SendFailure;
        owner.finish(*context, std::move(result));
    }
    owner.leave();
    return token;
}
}}
namespace snmp3 {
Native::~Native()
{
    try { close(); } catch (...) { std::terminate(); }
}
uint64_t Native::submit(const std::vector<std::shared_ptr<const Binding>>& bindings,
                        const std::vector<Value>& payload, Completion completion)
{
    if (!state->owner) throw NativeError(NativeOutcome::InvalidRequest, "native owner closed");
    return detail::submit(*state, bindings, payload, std::move(completion));
}
void Native::close()
{
    if (!state->owner) return;
    auto& owner = *state->owner;
    owner.enter();
    owner.close(*state);
    owner.leave();
}
const NativeIdentity& Native::identity() const { return state->identity; }
}
