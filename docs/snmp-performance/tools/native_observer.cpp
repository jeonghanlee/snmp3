#include "Runtime.h"
#include <iocsh.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {
// The observation process selects an existing diagnostic sink before binding.
// No IOC command, transport replacement or native symbol override is installed.
struct Install {
    Install()
    {
        try {
            const char* path = std::getenv("SNMP3_OBSERVER_LOG");
            if (!path || path[0] != '/')
                throw std::runtime_error("absolute observation path required");
            const std::string selected(path);
            // Capability probes and address workers execute without this work library.
            unsetenv("LD_PRELOAD");
            unsetenv("SNMP3_OBSERVER_LOG");
            snmp3::Qualification mode;
            mode.stderrPath = selected;
            snmp3::Runtime::instance().setQualification(mode);
            std::printf("APC_OBSERVER_READY\n");
        } catch (const std::exception&) {
            std::printf("APC_OBSERVER_ERROR\n");
            iocshSetError(1);
        }
        std::fflush(stdout);
    }
};
Install install;
}
