#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/snmpv3.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr size_t ConfigLimit = 65536;
constexpr unsigned FirstPort = 1024;
constexpr unsigned LastPort = 65535;
constexpr size_t ContextCount = 3;
constexpr size_t ScalarCount = 26;
const std::array<unsigned char, 8> Engine = {{0x80, 0, 0, 0, 1, 2, 3, 4}};
const char* Contexts[ContextCount] = {"", "alpha", "beta"};
volatile sig_atomic_t Running = 1;
void stop(int) { Running = 0; }
struct FreeVar {
    void operator()(netsnmp_variable_list* p) const { snmp_free_varbind(p); }
};
using Var = std::unique_ptr<netsnmp_variable_list, FreeVar>;
struct Cell {
    Var value, undo;
    unsigned index;
    unsigned context;
};
std::array<Cell, ContextCount * ScalarCount> Cells;

std::string privateConfig(const char* path)
{
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) throw std::runtime_error("fixture configuration open failed");
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & 077) || st.st_size <= 0 || st.st_size > static_cast<off_t>(ConfigLimit)) {
        close(fd);
        throw std::runtime_error("fixture configuration permissions rejected");
    }
    std::string text(static_cast<size_t>(st.st_size), '\0');
    size_t position = 0;
    while (position < text.size()) {
        const ssize_t count = read(fd, &text[position], text.size() - position);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { close(fd); throw std::runtime_error("fixture configuration read failed"); }
        position += static_cast<size_t>(count);
    }
    close(fd);
    if (text.find('\0') != std::string::npos)
        throw std::runtime_error("fixture configuration contains NUL");
    return text;
}

Var value(unsigned index, unsigned context)
{
    netsnmp_variable_list* result = nullptr;
    const long integer = -123L - static_cast<long>(context);
    const unsigned long unsignedValue = 0xffffffffUL;
    const counter64 wide = {index == 19 ? 0x80000000UL : 0xffffffffUL,
                            index == 19 ? 0UL : 0xffffffffUL};
    const unsigned char bytes[] = {'A', 0, 'B', 0xff, static_cast<unsigned char>(context)};
    const oid arcs[] = {1, 3, 6, 1, 4, 1, 53864};
    const unsigned char ip[] = {127, 0, 0, 1};
    const float single = index == 21 ? std::numeric_limits<float>::quiet_NaN() :
                         index == 22 ? std::numeric_limits<float>::infinity() :
                         index == 23 ? -std::numeric_limits<float>::infinity() : -2.25f;
    const double precise = index == 24 ? std::numeric_limits<double>::quiet_NaN() :
                           index == 25 ? std::numeric_limits<double>::infinity() :
                           index == 26 ? -std::numeric_limits<double>::infinity() : 1.0 / 3.0;
    const void* data = &integer;
    size_t length = sizeof(integer);
    unsigned char type = ASN_INTEGER;
    switch (index) {
    case 2: type = ASN_COUNTER; data = &unsignedValue; length = sizeof(unsignedValue); break;
    case 3: type = ASN_GAUGE; data = &unsignedValue; length = sizeof(unsignedValue); break;
    case 4: type = ASN_TIMETICKS; data = &unsignedValue; length = sizeof(unsignedValue); break;
    case 5: case 19: case 20: type = ASN_COUNTER64; data = &wide; length = sizeof(wide); break;
    case 6: type = ASN_OCTET_STR; data = bytes; length = sizeof(bytes); break;
    case 7: type = ASN_OBJECT_ID; data = arcs; length = sizeof(arcs); break;
    case 8: type = ASN_IPADDRESS; data = ip; length = sizeof(ip); break;
    case 9: case 21: case 22: case 23:
        type = ASN_OPAQUE_FLOAT; data = &single; length = sizeof(single); break;
    case 10: case 24: case 25: case 26:
        type = ASN_OPAQUE_DOUBLE; data = &precise; length = sizeof(precise); break;
    case 13: type = SNMP_NOSUCHOBJECT; data = nullptr; length = 0; break;
    case 14: type = SNMP_NOSUCHINSTANCE; data = nullptr; length = 0; break;
    case 15: type = SNMP_ENDOFMIBVIEW; data = nullptr; length = 0; break;
    }
    if (!snmp_varlist_add_variable(&result, nullptr, 0, type,
                                   static_cast<const unsigned char*>(data), length))
        throw std::runtime_error("fixture value allocation failed");
    if (index >= 16 && index <= 18) {
        const long boundary = index == 16 ? INT32_MIN : index == 17 ? INT32_MAX : LONG_MAX;
        snmp_set_var_typed_value(result, ASN_INTEGER,
                                reinterpret_cast<const unsigned char*>(&boundary), sizeof(boundary));
        std::printf("{\"event\":\"fixture_native_integer\",\"index\":%u,\"input\":%ld,"
                    "\"native_stored\":%ld,\"native_length\":%zu}\n",
                    index, boundary, *result->val.integer, result->val_len);
    }
    return Var(result);
}

int handle(netsnmp_mib_handler*, netsnmp_handler_registration* registration,
           netsnmp_agent_request_info* info, netsnmp_request_info* requests)
{
    auto& cell = *static_cast<Cell*>(registration->my_reg_void);
    for (auto* request = requests; request; request = request->next) {
        auto* variable = request->requestvb;
        switch (info->mode) {
        case MODE_GET:
            if (cell.index == 11 || cell.index == 12) {
                netsnmp_set_request_error(info, request,
                    cell.index == 11 ? SNMP_ERR_TOOBIG : SNMP_ERR_GENERR);
                break;
            }
            snmp_set_var_typed_value(variable, cell.value->type,
                                    cell.value->val.string, cell.value->val_len);
            break;
        case MODE_SET_RESERVE1:
            if (variable->type != cell.value->type)
                netsnmp_set_request_error(info, request, SNMP_ERR_WRONGTYPE);
            break;
        case MODE_SET_ACTION: {
            cell.undo.reset(snmp_clone_varbind(cell.value.get()));
            Var candidate(snmp_clone_varbind(variable));
            if (!cell.undo || !candidate) {
                netsnmp_set_request_error(info, request, SNMP_ERR_RESOURCEUNAVAILABLE);
            } else {
                cell.value = std::move(candidate);
            }
            break;
        }
        case MODE_SET_UNDO:
            if (cell.undo) cell.value = std::move(cell.undo);
            break;
        case MODE_SET_COMMIT:
        case MODE_SET_FREE:
            cell.undo.reset();
            break;
        }
        const auto* pdu = info->asp ? info->asp->pdu : nullptr;
        std::printf("{\"event\":\"agent_handler\",\"mode\":%d,\"index\":%u,"
                    "\"context\":%u,\"type\":%u,\"length\":%zu,\"request_id\":%ld,"
                    "\"version\":%ld,\"security_level\":%d}\n",
                    info->mode, cell.index, cell.context, variable->type,
                    variable->val_len, pdu ? pdu->reqid : -1L,
                    pdu ? pdu->version : -1L, pdu ? pdu->securityLevel : -1);
    }
    return SNMP_ERR_NOERROR;
}
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IOLBF, 0);
    if ((argc != 4 && argc != 5) || (std::strcmp(argv[2], "4") && std::strcmp(argv[2], "6"))) {
        std::fprintf(stderr, "Usage: snmp3NativeAgent PORT FAMILY CONFIG [ENGINE_HEX]\n");
        return 2;
    }
    char* tail = nullptr;
    const unsigned long port = std::strtoul(argv[1], &tail, 10);
    if (!tail || *tail || port < FirstPort || port > LastPort) return 2;
    try {
        std::string config = privateConfig(argv[3]);
        std::vector<unsigned char> engine(Engine.begin(), Engine.end());
        if (argc == 5) {
            const size_t length = std::strlen(argv[4]);
            if (length < 10 || length > 64 || length % 2)
                throw std::runtime_error("fixture engine argument invalid");
            engine.clear();
            for (size_t i = 0; i < length; i += 2) {
                char pair[] = {argv[4][i], argv[4][i + 1], 0};
                char* end;
                const unsigned long byte = std::strtoul(pair, &end, 16);
                if (*end || end != pair + 2) throw std::runtime_error("fixture engine argument invalid");
                engine.push_back(static_cast<unsigned char>(byte));
            }
        }
        const std::string address = std::string(argv[2][0] == '4' ? "udp:127.0.0.1:" : "udp6:[::1]:") + argv[1];
        netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
        netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
        netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_ALARM_DONT_USE_SIG, 1);
        netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_PORTS, address.c_str());
        netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 0);
        netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_AGENTX_MASTER, 0);
        char excludedModules[] = "-smux";
        add_to_init_list(excludedModules);
        if (should_init("smux")) throw std::runtime_error("fixture SMUX exclusion failed");
        if (set_exact_engineID(engine.data(), engine.size()) != SNMPERR_SUCCESS)
            throw std::runtime_error("fixture engine setup failed");
        if (init_agent("snmpd")) throw std::runtime_error("fixture agent initialization failed");
        size_t start = 0;
        while (start < config.size()) {
            const size_t end = config.find('\n', start);
            std::string line = config.substr(start, end == std::string::npos ? end : end - start);
            if (!line.empty()) netsnmp_config_remember(&line[0]);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        for (unsigned context = 0; context < ContextCount; ++context) {
            for (unsigned index = 1; index <= ScalarCount; ++index) {
                auto& cell = Cells[context * ScalarCount + index - 1];
                cell.index = index;
                cell.context = context;
                cell.value = value(index, context);
                oid object[] = {1, 3, 6, 1, 4, 1, 53864, 4, index};
                auto* reg = netsnmp_create_handler_registration("native-fixture", handle,
                    object, OID_LENGTH(object), HANDLER_CAN_RWRITE);
                if (!reg) throw std::runtime_error("fixture registration failed");
                reg->my_reg_void = &cell;
                if (*Contexts[context]) reg->contextName = strdup(Contexts[context]);
                if (netsnmp_register_scalar(reg) != MIB_REGISTERED_OK)
                    throw std::runtime_error("fixture scalar registration failed");
            }
        }
        init_snmp("snmpd");
        if (init_master_agent()) throw std::runtime_error("fixture bind failed");
        std::signal(SIGTERM, stop);
        std::signal(SIGINT, stop);
        setvbuf(stdout, nullptr, _IOLBF, 0);
        unsigned char resolved[MAX_ENGINEID_LENGTH];
        const size_t resolvedLength = snmpv3_get_engineID(resolved, sizeof(resolved));
        std::printf("{\"event\":\"agent_ready\",\"engine_id\":\"");
        for (size_t i = 0; i < resolvedLength; ++i) std::printf("%02x", resolved[i]);
        std::printf("\",\"boots\":%lu}\n", snmpv3_local_snmpEngineBoots());
        while (Running) agent_check_and_process(1);
        snmp_shutdown("snmpd");
        shutdown_agent();
        std::puts("{\"event\":\"agent_stopped\"}");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
