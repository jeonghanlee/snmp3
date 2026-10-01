#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/snmpusm.h>
#include <cstdio>
#include <cstring>
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
}

int main(int argc, char** argv)
{
    if (argc != 2 || std::strcmp(argv[1], "--capabilities")) {
        std::fprintf(stderr, "Usage: snmp3NativeProbe --capabilities\n");
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
