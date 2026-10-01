#include "Runtime.h"

#include <cstdio>
#include <epicsExit.h>
#include <iocsh.h>

int main(int argc, char *argv[])
{
    if (argc > 2) {
        std::fprintf(stderr, "Usage: %s [startup-script]\n", argv[0]);
        epicsExit(2);
        return 2;
    }
    if (argc == 2 && (iocsh(argv[1]) != 0 ||
        snmp3::Runtime::instance().snapshot().state == snmp3::State::Failed)) {
        std::fprintf(stderr, "snmp3Ioc: startup script failed\n");
        epicsExit(1);
        return 1;
    }
    const int shellStatus = iocsh(NULL);
    const int status = shellStatus == 0 &&
        snmp3::Runtime::instance().snapshot().state != snmp3::State::Failed ? 0 : 1;
    epicsExit(status);
    return status;
}
