// Observes the environment of a real probe process and of a real worker process started through the shipped
// load and launch paths, and the rejections of the load. Every check runs; a failed check does not stop the rest.
// usage: snmp3EnvTest <probe helper> <worker> <scratch directory>
#include "Config.h"
#include "Supervisor.h"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <poll.h>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace snmp3;
namespace {
unsigned checks = 0, failures = 0;
void report(const char* name, bool pass)
{
    ++checks;
    if (!pass) ++failures;
    std::printf("{\"check\":\"%s\",\"pass\":%s}\n", name, pass ? "true" : "false");
}
// The message of the exception that the call throws; empty when the call does not throw.
std::string rejection(const std::function<void()>& call)
{
    try { call(); } catch (const std::exception& error) { return error.what(); }
    return "";
}
bool mentions(const std::string& message, const char* text) { return message.find(text) != std::string::npos; }
bool exists(const std::string& path) { struct stat state; return stat(path.c_str(), &state) == 0; }
void removeFile(const std::string& path) { unlink(path.c_str()); }
std::string slurp(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}
void write(const std::string& path, const std::string& text, mode_t mode)
{
    std::ofstream(path, std::ios::binary) << text;
    chmod(path.c_str(), mode);
}
std::vector<std::string> split(const std::string& text, char separator)
{
    std::vector<std::string> result;
    std::string item;
    std::istringstream in(text);
    while (std::getline(in, item, separator)) if (!item.empty()) result.push_back(item);
    return result;
}
std::string absolutePath(const std::string& path)
{
    char* resolved = realpath(path.c_str(), nullptr);
    if (!resolved) throw std::runtime_error("path not resolvable: " + path);
    const std::string result(resolved);
    free(resolved);
    return result;
}
// The decided rule: the directory of every library that ldd reports, resolved, sorted, unique, joined by ':'.
std::string expectedLibraryPath(const std::string& executable)
{
    FILE* pipe = popen(("/usr/bin/ldd " + executable).c_str(), "r");
    if (!pipe) throw std::runtime_error("ldd not started");
    std::set<std::string> directories;
    char line[4096];
    while (std::fgets(line, sizeof(line), pipe)) {
        const std::string text(line);
        if (text.find("linux-vdso.so.") != std::string::npos) continue;
        const auto begin = text.find('/');
        const auto end = text.find(" (", begin);
        if (begin == std::string::npos || end == std::string::npos) continue;
        const std::string library = absolutePath(text.substr(begin, end - begin));
        directories.insert(library.substr(0, library.find_last_of('/')));
    }
    pclose(pipe);
    std::string result;
    for (const auto& directory : directories) { if (!result.empty()) result += ':'; result += directory; }
    return result;
}
std::string join(const std::vector<std::string>& list)
{
    std::string result;
    for (const auto& entry : list) result += entry + "\n";
    return result;
}
bool onlyTheFourNames(const std::vector<std::string>& list)
{
    const std::set<std::string> allowed = {"PATH", "LANG", "LC_ALL", "LD_LIBRARY_PATH"};
    for (const auto& entry : list) if (!allowed.count(entry.substr(0, entry.find('=')))) return false;
    return list.size() == allowed.size();
}
}

int main(int argc, char** argv)
{
    if (argc != 4) { std::fprintf(stderr, "usage: snmp3EnvTest <probe helper> <worker> <scratch directory>\n"); return 2; }
    setvbuf(stdout, nullptr, _IOLBF, 0);
    try {
        const std::string probe = absolutePath(argv[1]), worker = absolutePath(argv[2]), scratch = absolutePath(argv[3]);
        const std::string dump = probe + ".capabilities.env";
        const std::string json = scratch + "/config.json", failing = scratch + "/failing-worker.sh";
        write(scratch + "/community.txt", "public\n", 0600);
        write(json, "{\"schema\":1,\"profiles\":[{\"id\":\"Community\",\"version\":\"2c\",\"communityFile\":\"community.txt\"}],"
                    "\"endpoints\":[{\"id\":\"Local\",\"address\":\"127.0.0.1\",\"profile\":\"Community\"}],"
                    "\"bindings\":[{\"id\":\"Read\",\"endpoint\":\"Local\",\"oid\":\"1.3.6.1.2.1.1.3.0\","
                    "\"operation\":\"get\",\"valueType\":\"timeticks\"}]}\n", 0600);
        write(failing, "#!/bin/sh\nexit 0\n", 0755);
        // The test process holds a variable outside the list and a library path different from the expected one.
        setenv("SNMP3_ENV_MARKER", "marker", 1);
        setenv("LD_LIBRARY_PATH", "/nonexistent-marker-directory", 1);
        const std::string expectedPath = expectedLibraryPath(worker);

        // T1: the real probe process and the real worker process.
        removeFile(dump);
        std::vector<std::string> probeList, workerList;
        {
            Config config;
            config.setWorkerPath(worker);
            config.load(json, probe);
            probeList = split(slurp(dump), '\n');
            const auto frozen = config.snapshot();
            auto scheduler = std::make_shared<Scheduler>(frozen.configuration, frozen.revision, 1);
            Supervisor owner(scheduler, worker, Qualification());
            const auto end = ipc::add(monotonicUs(), 6000000);
            pid_t pid = 0;
            while (!pid && monotonicUs() < end) {
                owner.step(monotonicUs());
                owner.takeEvents();
                for (const auto& status : owner.snapshots()) if (status.pid) pid = status.pid;
                poll(nullptr, 0, 1);
            }
            if (pid) workerList = split(slurp("/proc/" + std::to_string(pid) + "/environ"), '\0');
            owner.requestStop();
            const auto stopEnd = ipc::add(monotonicUs(), 2000000);
            do { owner.step(monotonicUs()); owner.takeEvents(); if (owner.finished(monotonicUs())) break; poll(nullptr, 0, 1); }
            while (monotonicUs() < stopEnd);
            owner.reconcile();
        }
        removeFile(dump);
        const std::vector<std::string> fixed = {"PATH=/usr/bin:/bin", "LANG=C", "LC_ALL=C"};
        report("probe_and_worker_were_observed", probeList.size() > 0 && workerList.size() > 0);
        report("probe_environment_holds_no_variable_outside_the_list", onlyTheFourNames(probeList));
        report("probe_fixed_entries_equal_the_literals",
               probeList.size() >= 3 && std::vector<std::string>(probeList.begin(), probeList.begin() + 3) == fixed);
        report("probe_library_path_equals_worker_ldd_directories",
               !probeList.empty() && probeList.back() == "LD_LIBRARY_PATH=" + expectedPath);
        report("worker_library_path_equals_worker_ldd_directories",
               !workerList.empty() && workerList.back() == "LD_LIBRARY_PATH=" + expectedPath);
        report("probe_and_worker_environments_byte_identical", join(probeList) == join(workerList) && !probeList.empty());

        // T2: the rejections, each with the probe not started.
        {
            Config config;
            removeFile(dump);
            report("load_without_worker_path_rejected_with_its_reason",
                   mentions(rejection([&] { config.load(json, probe); }), "worker path required"));
            report("no_probe_started_without_worker_path", !exists(dump));
        }
        {
            Config config;
            config.setWorkerPath(failing);
            removeFile(dump);
            report("load_with_failing_ldd_rejected_with_its_reason",
                   mentions(rejection([&] { config.load(json, probe); }), "worker library listing failed"));
            report("no_probe_started_with_failing_ldd", !exists(dump));
        }
        {
            Config config;
            config.setWorkerPath(worker);
            config.load(json, probe);
            removeFile(dump);
            report("worker_path_change_after_load_rejected_with_its_reason",
                   mentions(rejection([&] { config.setWorkerPath(failing); }), "worker path fixed"));
            report("earlier_worker_path_kept", config.snapshot().workerPath == worker);
            report("same_worker_path_accepted_again", rejection([&] { config.setWorkerPath(worker); }).empty());
        }
        removeFile(dump);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "environment test failed: %s\n", error.what());
        return 1;
    }
    std::printf("{\"status\":\"%s\",\"checks\":%u,\"failures\":%u}\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
