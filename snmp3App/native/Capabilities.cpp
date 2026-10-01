#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/snmpusm.h>
#include "Capabilities.h"
#include <limits>

namespace snmp3 {
namespace {
constexpr unsigned MaxAlgorithms = 128;
constexpr size_t CryptoBufferSize = 1024;
Algorithm algorithm(const char* name, int type, const oid* value, int length)
{
    if (!name || !value || length <= 0 || length > MAX_OID_LEN)
        throw std::runtime_error("native algorithm metadata absent");
    Algorithm result;
    result.name = name;
    result.nativeType = type;
    for (int i = 0; i < length; ++i) {
        if (value[i] > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("native algorithm OID width rejected");
        result.oid.push_back(static_cast<uint32_t>(value[i]));
    }
    return result;
}
bool authUsable(const netsnmp_auth_alg_info& info)
{
    const unsigned char input[] = "snmp3 capability check";
    unsigned char output[CryptoBufferSize];
    size_t size = sizeof(output);
    return info.proper_length > 0 &&
        sc_hash(info.alg_oid, info.oid_len, input, sizeof(input) - 1, output, &size) == SNMPERR_SUCCESS &&
        size == static_cast<size_t>(info.proper_length);
}
bool privUsable(const netsnmp_priv_alg_info& info)
{
    if (info.proper_length <= 0 || info.iv_length <= 0 ||
        info.proper_length > static_cast<int>(CryptoBufferSize) ||
        info.iv_length > static_cast<int>(CryptoBufferSize)) return false;
    std::vector<unsigned char> key(info.proper_length, 1), iv(info.iv_length, 1);
    unsigned char plain[16] = {}, encrypted[CryptoBufferSize];
    size_t size = sizeof(encrypted);
    return sc_encrypt(info.alg_oid, info.oid_len, key.data(), key.size(), iv.data(), iv.size(),
                      plain, sizeof(plain), encrypted, &size) == SNMPERR_SUCCESS && size > 0;
}
bool same(const std::map<std::string, Algorithm>& a, const std::map<std::string, Algorithm>& b)
{
    if (a.size() != b.size()) return false;
    for (const auto& entry : a) {
        const auto other = b.find(entry.first);
        if (other == b.end() || entry.second.name != other->second.name ||
            entry.second.nativeType != other->second.nativeType || entry.second.oid != other->second.oid)
            return false;
    }
    return true;
}
}
Capabilities nativeCapabilities()
{
    if (sc_init() != SNMPERR_SUCCESS) throw std::runtime_error("native crypto initialization failed");
    Capabilities result;
    result.nativeVersion = netsnmp_get_version();
    for (unsigned index = 0; index < MaxAlgorithms; ++index) {
        const auto* info = sc_get_auth_alg_byindex(index);
        if (!info) break;
        if (authUsable(*info)) {
            auto entry = algorithm(usm_lookup_auth_str(info->type), info->type, info->alg_oid, info->oid_len);
            if (!result.authentication.emplace(entry.name, entry).second)
                throw std::runtime_error("native authentication catalogue duplicate");
        }
        if (index + 1 == MaxAlgorithms) throw std::runtime_error("native algorithm catalogue limit");
    }
    for (unsigned index = 0; index < MaxAlgorithms; ++index) {
        const auto* info = sc_get_priv_alg_byindex(index);
        if (!info) break;
        if (privUsable(*info)) {
            auto entry = algorithm(usm_lookup_priv_str(info->type), info->type, info->alg_oid, info->oid_len);
            if (!result.privacy.emplace(entry.name, entry).second)
                throw std::runtime_error("native privacy catalogue duplicate");
        }
        if (index + 1 == MaxAlgorithms) throw std::runtime_error("native algorithm catalogue limit");
    }
    return result;
}
bool sameCapabilities(const Capabilities& expected, const Capabilities& observed)
{
    return expected.nativeVersion == observed.nativeVersion &&
        same(expected.authentication, observed.authentication) && same(expected.privacy, observed.privacy);
}
}
