#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/snmpusm.h>
#include <net-snmp/library/parse.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <yajl_gen.h>
#include "Capabilities.h"

namespace {
void check(yajl_gen_status status)
{
    if (status != yajl_gen_status_ok) throw std::runtime_error("capability encoding failed");
}
void string(yajl_gen generator, const char* value)
{
    if (!value) throw std::runtime_error("native algorithm name absent");
    check(yajl_gen_string(generator, reinterpret_cast<const unsigned char*>(value), std::strlen(value)));
}
void algorithm(yajl_gen generator, const snmp3::Algorithm& entry)
{
    check(yajl_gen_map_open(generator));
    string(generator, "name"); string(generator, entry.name.c_str());
    string(generator, "type"); check(yajl_gen_integer(generator, entry.nativeType));
    std::string arcs;
    for (size_t i = 0; i < entry.oid.size(); ++i) {
        if (i) arcs += '.';
        arcs += std::to_string(entry.oid[i]);
    }
    string(generator, "oid"); string(generator, arcs.c_str());
    check(yajl_gen_map_close(generator));
}
// The value type of the module that holds what a MIB declares, by the name a record link uses; empty when the
// module holds no type for the declaration, so that the link must give one.
const char* declaredType(int type)
{
    switch (type) {
    case TYPE_INTEGER: case TYPE_INTEGER32: return "integer";
    case TYPE_UNSIGNED32: case TYPE_UINTEGER: return "unsigned32";
    case TYPE_COUNTER: return "counter32";
    case TYPE_GAUGE: return "gauge32";
    case TYPE_TIMETICKS: return "timeticks";
    case TYPE_COUNTER64: return "counter64";
    case TYPE_OCTETSTR: case TYPE_BITSTRING: return "octets";
    case TYPE_OBJID: return "oid";
    case TYPE_IPADDR: case TYPE_NETADDR: return "ipAddress";
    default: return "";
    }
}
const char* declaredAccess(int access)
{
    switch (access) {
    case MIB_ACCESS_READONLY: return "read-only";
    case MIB_ACCESS_READWRITE: return "read-write";
    case MIB_ACCESS_WRITEONLY: return "write-only";
    case MIB_ACCESS_CREATE: return "read-create";
    default: return "not-accessible";
    }
}
// The number of arcs of the object identifier of a node.
size_t depth(const struct tree* node)
{
    size_t arcs = 0;
    for (const struct tree* up = node; up; up = up->parent) ++arcs;
    return arcs;
}
// Whether a name carries the index of an instance of its object: one arc 0 after a scalar, at least one arc after
// a column of a table.
bool indexed(const struct tree* node, const oid* name, size_t length)
{
    size_t depth = 0;
    for (const struct tree* up = node; up; up = up->parent) ++depth;
    const bool column = node->parent && (node->parent->indexes || node->parent->augments);
    if (column) return length > depth;
    return length == depth + 1 && name[depth] == 0;
}
// Loads the MIB modules the names ask for. The library takes a module from the place registered last: the
// directories of the system are registered first ("-" keeps the ones built into the library, a path replaces
// them), then the entries of the colon-separated list of places in their order, each a directory or one file.
// No MIB module is read unless a name asks for it, and no environment variable chooses a place.
void loadMibs(int count, char** names, const char* system, const char* places, bool descriptions,
              std::string& systemUsed, std::set<std::string>& missing)
{
    setenv("MIBS", "", 1);
    unsetenv("MIBDIRS");
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_MIB_WARNINGS, 0);
    // The text of a DESCRIPTION clause is dropped by the library unless it is asked to keep it.
    if (descriptions) netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SAVE_MIB_DESCRS, 1);
    systemUsed = system;
    if (!std::strcmp(system, "-")) {
        // Of the list built into the library only the absolute directories are kept: its entry under the home of
        // the user is not expanded without HOME and would be read below the working directory.
        const char* builtIn = netsnmp_get_mib_directory();
        const std::string all = builtIn ? builtIn : "";
        systemUsed.clear();
        for (size_t begin = 0; begin < all.size();) {
            size_t end = all.find(':', begin);
            if (end == std::string::npos) end = all.size();
            if (end > begin && all[begin] == '/') systemUsed += (systemUsed.empty() ? "" : ":") + all.substr(begin, end - begin);
            begin = end + 1;
        }
    }
    netsnmp_set_mib_directory(systemUsed.c_str());
    netsnmp_init_mib();
    const std::string list = std::strcmp(places, "-") ? places : "";
    for (size_t begin = 0; begin < list.size();) {
        size_t end = list.find(':', begin);
        if (end == std::string::npos) end = list.size();
        const std::string place = list.substr(begin, end - begin);
        struct stat status;
        if (!place.empty() && !stat(place.c_str(), &status)) {
            if (S_ISDIR(status.st_mode)) add_mibdir(place.c_str());
            else {
                const size_t slash = place.rfind('/');
                add_mibfile(place.c_str(), place.c_str() + (slash == std::string::npos ? 0 : slash + 1));
            }
        }
        begin = end + 1;
    }
    std::set<std::string> asked;
    for (int i = 0; i < count; ++i) {
        const std::string text(names[i]);
        const size_t colons = text.find("::");
        if (colons == std::string::npos || colons == 0) continue;
        const std::string module = text.substr(0, colons);
        if (!asked.insert(module).second) continue;
        netsnmp_read_module(module.c_str());
        if (which_module(module.c_str()) < 0) missing.insert(module);
    }
}
// Resolves object names through MIB files: for each name the identifier, the declared type and the declared
// access, or the kind of its fault.
int resolve(int count, char** names, const char* system, const char* places)
{
    std::string systemUsed;
    std::set<std::string> missing;
    loadMibs(count, names, system, places, false, systemUsed, missing);
    yajl_gen generator = yajl_gen_alloc(NULL);
    if (!generator) return 1;
    int result = 1;
    try {
        check(yajl_gen_map_open(generator));
        string(generator, "schema"); check(yajl_gen_integer(generator, 1));
        string(generator, "system"); string(generator, systemUsed.c_str());
        string(generator, "names"); check(yajl_gen_array_open(generator));
        for (int i = 0; i < count; ++i) {
            const std::string text(names[i]);
            const size_t colons = text.find("::");
            const std::string module = colons == std::string::npos ? "" : text.substr(0, colons);
            check(yajl_gen_map_open(generator));
            string(generator, "text"); string(generator, text.c_str());
            oid name[MAX_OID_LEN];
            size_t length = MAX_OID_LEN;
            const struct tree* node = NULL;
            if (module.empty() || missing.count(module)) { string(generator, "error"); string(generator, "module"); }
            else if (!read_objid(text.c_str(), name, &length) || !(node = get_tree(name, length, get_tree_head()))) {
                string(generator, "error"); string(generator, "name");
            } else if (node->type == TYPE_OTHER || node->type > TYPE_SIMPLE_LAST) {
                // A table, a row, a group, a notification or an identity: a name of the MIB that is no object
                // with a value.
                string(generator, "error"); string(generator, "object");
            } else if (!indexed(node, name, length)) {
                string(generator, "error"); string(generator, "index");
            } else {
                std::string arcs;
                for (size_t k = 0; k < length; ++k) arcs += (k ? "." : "") + std::to_string(static_cast<unsigned long>(name[k]));
                string(generator, "oid"); string(generator, arcs.c_str());
                string(generator, "type"); string(generator, declaredType(node->type));
                string(generator, "access"); string(generator, declaredAccess(node->access));
            }
            check(yajl_gen_map_close(generator));
        }
        check(yajl_gen_array_close(generator));
        string(generator, "missing"); check(yajl_gen_array_open(generator));
        for (const auto& module : missing) string(generator, module.c_str());
        check(yajl_gen_array_close(generator));
        // Every module the library read, the ones a name asked for and the ones those import, with its file.
        // The library also keeps an entry for each module it only found in a place; that one has no import
        // count yet and is left out.
        string(generator, "modules"); check(yajl_gen_array_open(generator));
        for (int id = 0; ; ++id) {
            const struct module* entry = find_module(id);
            if (!entry) break;
            if (entry->no_imports < 0) continue;
            check(yajl_gen_map_open(generator));
            string(generator, "name"); string(generator, entry->name);
            string(generator, "file"); string(generator, entry->file ? entry->file : "");
            check(yajl_gen_map_close(generator));
        }
        check(yajl_gen_array_close(generator)); check(yajl_gen_map_close(generator));
        const unsigned char* output = NULL; size_t size = 0;
        check(yajl_gen_get_buf(generator, &output, &size));
        if (std::fwrite(output, 1, size, stdout) == size && std::putchar('\n') != EOF) result = 0;
    } catch (...) { std::fprintf(stderr, "native name resolution failed\n"); }
    yajl_gen_free(generator);
    return result;
}

// Describes the objects the names give, each MODULE::name without an index: whether it is a scalar or a column of a
// table, the declared type and access, and what the MIB states about it in words: the description, the units and
// the labels of an enumerated integer. The identifier is the one of the object, without an instance.
int describe(int count, char** names, const char* system, const char* places)
{
    std::string systemUsed;
    std::set<std::string> missing;
    loadMibs(count, names, system, places, true, systemUsed, missing);
    yajl_gen generator = yajl_gen_alloc(NULL);
    if (!generator) return 1;
    int result = 1;
    try {
        check(yajl_gen_map_open(generator));
        string(generator, "schema"); check(yajl_gen_integer(generator, 1));
        string(generator, "system"); string(generator, systemUsed.c_str());
        string(generator, "names"); check(yajl_gen_array_open(generator));
        for (int i = 0; i < count; ++i) {
            const std::string text(names[i]);
            const size_t colons = text.find("::");
            const std::string module = colons == std::string::npos || colons == 0 ? "" : text.substr(0, colons);
            check(yajl_gen_map_open(generator));
            string(generator, "text"); string(generator, text.c_str());
            oid name[MAX_OID_LEN];
            size_t length = MAX_OID_LEN;
            const struct tree* node = NULL;
            if (module.empty() || missing.count(module)) { string(generator, "error"); string(generator, "module"); }
            else if (!read_objid(text.c_str(), name, &length) || !(node = get_tree(name, length, get_tree_head()))
                     || depth(node) != length) {
                string(generator, "error"); string(generator, "name");
            } else if (node->type == TYPE_OTHER || node->type > TYPE_SIMPLE_LAST) {
                string(generator, "error"); string(generator, "object");
            } else {
                const bool column = node->parent && (node->parent->indexes || node->parent->augments);
                std::string arcs;
                for (size_t k = 0; k < length; ++k) arcs += (k ? "." : "") + std::to_string(static_cast<unsigned long>(name[k]));
                string(generator, "kind"); string(generator, column ? "column" : "scalar");
                string(generator, "oid"); string(generator, arcs.c_str());
                string(generator, "type"); string(generator, declaredType(node->type));
                string(generator, "access"); string(generator, declaredAccess(node->access));
                string(generator, "description"); string(generator, node->description ? node->description : "");
                string(generator, "units"); string(generator, node->units ? node->units : "");
                string(generator, "enums"); check(yajl_gen_array_open(generator));
                for (const struct enum_list* entry = node->enums; entry; entry = entry->next) {
                    check(yajl_gen_map_open(generator));
                    string(generator, "value"); check(yajl_gen_integer(generator, entry->value));
                    string(generator, "label"); string(generator, entry->label ? entry->label : "");
                    check(yajl_gen_map_close(generator));
                }
                check(yajl_gen_array_close(generator));
            }
            check(yajl_gen_map_close(generator));
        }
        check(yajl_gen_array_close(generator));
        string(generator, "missing"); check(yajl_gen_array_open(generator));
        for (const auto& module : missing) string(generator, module.c_str());
        check(yajl_gen_array_close(generator));
        string(generator, "modules"); check(yajl_gen_array_open(generator));
        for (int id = 0; ; ++id) {
            const struct module* entry = find_module(id);
            if (!entry) break;
            if (entry->no_imports < 0) continue;
            check(yajl_gen_map_open(generator));
            string(generator, "name"); string(generator, entry->name);
            string(generator, "file"); string(generator, entry->file ? entry->file : "");
            check(yajl_gen_map_close(generator));
        }
        check(yajl_gen_array_close(generator)); check(yajl_gen_map_close(generator));
        const unsigned char* output = NULL; size_t size = 0;
        check(yajl_gen_get_buf(generator, &output, &size));
        if (std::fwrite(output, 1, size, stdout) == size && std::putchar('\n') != EOF) result = 0;
    } catch (...) { std::fprintf(stderr, "native name description failed\n"); }
    yajl_gen_free(generator);
    return result;
}
}

int main(int argc, char** argv)
{
    if (argc >= 5 && !std::strcmp(argv[1], "--resolve")) return resolve(argc - 4, argv + 4, argv[2], argv[3]);
    if (argc >= 5 && !std::strcmp(argv[1], "--describe")) return describe(argc - 4, argv + 4, argv[2], argv[3]);
    if (argc != 2 || std::strcmp(argv[1], "--capabilities")) {
        std::fprintf(stderr, "Usage: snmp3NativeProbe --capabilities | --resolve SYSTEM PLACES NAME... | --describe SYSTEM PLACES NAME...\n");
        return 2;
    }
    yajl_gen generator = yajl_gen_alloc(NULL);
    if (!generator) return 1;
    int result = 1;
    try {
        const auto capabilities = snmp3::nativeCapabilities();
        check(yajl_gen_map_open(generator));
        string(generator, "schema"); check(yajl_gen_integer(generator, 1));
        string(generator, "version"); string(generator, capabilities.nativeVersion.c_str());
        string(generator, "authentication"); check(yajl_gen_array_open(generator));
        for (const auto& entry : capabilities.authentication) algorithm(generator, entry.second);
        check(yajl_gen_array_close(generator));
        string(generator, "privacy"); check(yajl_gen_array_open(generator));
        for (const auto& entry : capabilities.privacy) algorithm(generator, entry.second);
        check(yajl_gen_array_close(generator)); check(yajl_gen_map_close(generator));
        const unsigned char* output = NULL; size_t size = 0;
        check(yajl_gen_get_buf(generator, &output, &size));
        if (std::fwrite(output, 1, size, stdout) == size && std::putchar('\n') != EOF) result = 0;
    } catch (...) { std::fprintf(stderr, "native capability query failed\n"); }
    yajl_gen_free(generator);
    return result;
}
