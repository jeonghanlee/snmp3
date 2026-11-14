#ifndef SNMP3_ENVIRONMENT_H
#define SNMP3_ENVIRONMENT_H

#include <string>
#include <vector>

namespace snmp3 {
// The environment list of the native probe and of every worker: three fixed entries, the library path and,
// for a qualification run, the two sanitizer entries.
std::vector<std::string> environmentList(const std::string& libraryPath, bool sanitizers);
// The directories of the libraries of an executable as ldd reports them, sorted, unique and joined by ':'.
// Throws std::runtime_error when ldd fails or reports nothing.
std::string libraryPathOf(const std::string& executable);
}
#endif
