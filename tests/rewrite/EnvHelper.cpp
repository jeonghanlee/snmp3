// Stands for the native probe: writes its own environment, one entry per line in order, to a file named from its
// executable path and its mode, and answers --capabilities with a catalogue that the configuration load accepts.
#include <climits>
#include <cstdio>
#include <cstring>
#include <epicsThread.h>
#include <string>
#include <unistd.h>

extern char** environ;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::string mode = !std::strcmp(argv[1], "--capabilities") ? "capabilities"
                             : !std::strncmp(argv[1], "--ipc-fd", 8) ? "ipc-fd" : "";
    if (mode.empty()) return 2;
    epicsThreadSleep(0.0);  // keeps the Base library in the dynamic section, so that its directory shows in ldd
    char self[PATH_MAX];
    const ssize_t length = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (length <= 0) return 1;
    self[length] = 0;
    const std::string path = std::string(self) + "." + mode + ".env";
    FILE* out = std::fopen(path.c_str(), "w");
    if (!out) return 1;
    for (char** entry = environ; *entry; ++entry) std::fprintf(out, "%s\n", *entry);
    if (std::fclose(out)) return 1;
    if (mode == "capabilities")
        std::puts("{\"schema\":1,\"version\":\"helper\",\"authentication\":[{\"name\":\"SHA\",\"type\":1,"
                  "\"oid\":\"1.3.6.1.6.3.10.1.1.3\"}],\"privacy\":[{\"name\":\"AES\",\"type\":1,"
                  "\"oid\":\"1.3.6.1.6.3.10.1.2.4\"}]}");
    return 0;
}
