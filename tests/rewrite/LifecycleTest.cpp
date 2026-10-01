#include "Runtime.h"
#include "Config.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/resource.h>
#include <unistd.h>
#include <dbAccess.h>
#include <dbBase.h>
#include <dbUnitTest.h>
#include <epicsExit.h>
#include <epicsUnitTest.h>

// Execute the shipped Main implementation with a separate test entry point.
#define main snmp3ApplicationMain
#include "../../snmp3App/src/Main.cpp"
#undef main

extern "C" int snmp3LifecycleTest_registerRecordDeviceDriver(dbBase*);

namespace {
const unsigned CallerCount = 8;
const unsigned ActivationCount = 20;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void caller(void* argument)
{
    snmp3::Runtime& runtime = *static_cast<snmp3::Runtime*>(argument);
    for (unsigned i = 0; i < 100; ++i) {
        runtime.snapshot();
        runtime.stop();
    }
}
void module()
{
    snmp3::Runtime& runtime = snmp3::Runtime::instance();
    runtime.stop();
    require(runtime.snapshot().created == 0, "stop before start created a thread");
    for (unsigned cycle = 0; cycle < ActivationCount; ++cycle) {
        require(runtime.start(), "runtime failed to start");
        const auto active = runtime.snapshot();
        require(active.state == snmp3::State::Running && active.admission,
                "readiness or admission missing");
        require(runtime.start(), "idempotent start failed");
        epicsThreadId callers[CallerCount] = {};
        epicsThreadOpts options = EPICS_THREAD_OPTS_INIT;
        options.joinable = 1;
        for (unsigned i = 0; i < CallerCount; ++i) {
            callers[i] = epicsThreadCreateOpt("lifecycleStopCaller", caller, &runtime, &options);
            require(callers[i] != NULL, "caller creation failed");
        }
        for (auto id : callers)
            epicsThreadMustJoin(id);
        runtime.stop();
        const auto stopped = runtime.snapshot();
        require(stopped.state == snmp3::State::Stopped && !stopped.admission,
                "runtime did not stop");
        require(stopped.activation == cycle + 1 && stopped.created == cycle + 1 &&
                stopped.exited == cycle + 1 && stopped.joined == cycle + 1,
                "activation or exactly-once join count mismatch");
    }
    runtime.report();
}
void failure(char* script)
{
    require(geteuid() != 0, "real thread failure requires non-root execution");
    // Initialize actual Base facilities and singleton before restricting this child.
    epicsThreadGetIdSelf();
    snmp3::Runtime& runtime = snmp3::Runtime::instance();
    struct rlimit original;
    require(getrlimit(RLIMIT_NPROC, &original) == 0, "getrlimit failed");
    struct rlimit restricted = original;
    restricted.rlim_cur = 0;
    require(setrlimit(RLIMIT_NPROC, &restricted) == 0, "setrlimit failed");
    const bool started = runtime.start();
    const int restored = setrlimit(RLIMIT_NPROC, &original);
    runtime.stop();
    const auto state = runtime.snapshot();
    require(restored == 0, "limit restoration failed");
    require(!started && state.state == snmp3::State::Failed && !state.admission &&
            state.created == 0 && state.exited == 0 && state.joined == 0,
            "actual OS limit did not produce closed failed startup");
    runtime.report();
    // This calls the actual shipped Main, which must epicsExit(1) after a successful script.
    char program[] = "snmp3Ioc";
    char* args[] = {program, script, NULL};
    snmp3ApplicationMain(2, args);
    throw std::runtime_error("failed-runtime Main unexpectedly returned");
}
void isolated(const char* dbd, const char* db)
{
    const auto originalConfig = snmp3::Config::instance().snapshot();
    testPlan(4);
    for (unsigned i = 0; i < 2; ++i) {
        testdbPrepare();
        testdbReadDatabase(dbd, NULL, NULL);
        require(snmp3LifecycleTest_registerRecordDeviceDriver(pdbbase) == 0, "registrar failed");
        testdbReadDatabase(db, NULL, "P=Isolated_");
        testIocInitOk();
        const auto active = snmp3::Runtime::instance().snapshot();
        const auto config = snmp3::Config::instance().snapshot();
        require(config.frozen && config.revision == originalConfig.revision &&
                config.configuration == originalConfig.configuration, "isolated configuration changed");
        require(active.state == snmp3::State::Running && active.activation == i + 1,
                "isolated activation did not restore hook");
        testdbGetFieldEqual("Isolated_PiniProbe.VAL", DBR_LONG, 42);
        testdbGetFieldEqual("Isolated_PiniProbe.PACT", DBR_LONG, 0);
        testIocShutdownOk();
        const auto stopped = snmp3::Runtime::instance().snapshot();
        require(stopped.created == i + 1 && stopped.exited == i + 1 &&
                stopped.joined == i + 1 && !stopped.admission, "isolated join mismatch");
        testdbCleanup();
        const auto retained = snmp3::Config::instance().snapshot();
        require(retained.frozen && retained.configuration == originalConfig.configuration &&
                retained.revision == originalConfig.revision, "database cleanup reset configuration");
    }
    require(testDone() == 0, "Base field assertions failed");
    snmp3::Runtime::instance().report();
}
}
int main(int argc, char** argv)
{
    try {
        require(argc >= 2, "missing test mode");
        if (!std::strcmp(argv[1], "module") && argc == 2)
            module();
        else if (!std::strcmp(argv[1], "failure") && argc == 3)
            failure(argv[2]);
        else if (!std::strcmp(argv[1], "isolated") && argc == 4)
            isolated(argv[2], argv[3]);
        else
            throw std::runtime_error("invalid test arguments");
        std::printf("lifecycle test: PASS\n");
        epicsExit(0);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "lifecycle test: FAIL: %s\n", error.what());
        epicsExit(2);
    }
    return 2;
}
