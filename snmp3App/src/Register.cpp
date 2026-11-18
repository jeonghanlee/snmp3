#include "Runtime.h"
#include "Config.h"
#include "Request.h"

#include <cstdio>
#include <string>
#include <vector>
#include <cstring>
#include <set>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <initHooks.h>
#include <iocsh.h>
#include <epicsExport.h>

namespace {
void report(const iocshArgBuf*)
{
    std::printf("snmp3: Base %s; capability=owned-worker-transport; recordSupport=available\n",
                EPICS_VERSION_SHORT);
}
void runtimeReport(const iocshArgBuf*) { snmp3::Runtime::instance().report(); }
void stop(const iocshArgBuf*)
{
    snmp3::Config::instance().freeze();
    snmp3::Runtime::instance().stop();
}
void configReport(const iocshArgBuf*) { snmp3::Config::instance().report(); }
void workerPath(const iocshArgBuf* arguments)
{
    try {
        if(!arguments[0].sval)throw std::runtime_error("worker path required");
        snmp3::Config::instance().setWorkerPath(arguments[0].sval);
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
void queueLimit(const iocshArgBuf* arguments)
{
    try {
        auto number=[](const char* text) {
            if(!text || !*text)throw std::runtime_error("queue integer required");
            uint64_t value=0;
            for(const char* p=text;*p;++p) {
                if(*p<'0' || *p>'9')throw std::runtime_error("queue integer rejected");
                value=snmp3::ipc::add(snmp3::ipc::multiply(value,10),uint64_t(*p-'0'));
            }
            return value;
        };
        if(!arguments[0].sval)throw std::runtime_error("queue address required");
        snmp3::Runtime::instance().queueLimit(arguments[0].sval,number(arguments[1].sval),number(arguments[2].sval));
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
void load(const iocshArgBuf* arguments)
{
    try {
        if (!arguments[0].sval || !arguments[1].sval)
            throw std::runtime_error("configuration arguments required");
        snmp3::Config::instance().load(arguments[0].sval, arguments[1].sval);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "snmp3: %s\n", error.what());
        iocshSetError(1);
    }
}
// The option words of a command: argv[0] is the command name.
std::vector<std::string> words(const iocshArgBuf& argument)
{
    std::vector<std::string> result;
    for(int index=1;index<argument.aval.ac;++index)result.push_back(argument.aval.av[index]);
    return result;
}
void probePath(const iocshArgBuf* arguments)
{
    try {
        if(!arguments[0].sval)throw std::runtime_error("probe path required");
        snmp3::Config::instance().setProbePath(arguments[0].sval);
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
void session(const iocshArgBuf* arguments)
{
    try {
        if(!arguments[0].sval || !arguments[1].sval)throw std::runtime_error("session name and address required");
        snmp3::Config::instance().session(arguments[0].sval,arguments[1].sval,words(arguments[2]));
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
void options(const iocshArgBuf* arguments)
{
    try {
        if(!arguments[0].sval)throw std::runtime_error("session pattern required");
        snmp3::Config::instance().options(arguments[0].sval,words(arguments[1]));
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
void show(const iocshArgBuf* arguments)
{
    try {
        snmp3::Config::instance().show(arguments[0].sval ? arguments[0].sval : "*",arguments[1].ival);
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
void subscription(const iocshArgBuf* arguments)
{
    try {
        if(!arguments[0].sval || !arguments[1].sval || !arguments[2].sval)
            throw std::runtime_error("subscription name, session and period required");
        snmp3::Config::instance().subscription(arguments[0].sval,arguments[1].sval,arguments[2].sval);
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
void mib(const iocshArgBuf* arguments)
{
    try {
        if(!arguments[0].sval)throw std::runtime_error("MIB path required");
        const bool system=arguments[1].sval && !std::strcmp(arguments[1].sval,"system");
        if(arguments[1].sval && *arguments[1].sval && !system)throw std::runtime_error("second argument must be system");
        snmp3::Config::instance().mib(arguments[0].sval,system);
    } catch(const std::exception& error) { std::fprintf(stderr,"snmp3: %s\n",error.what()); iocshSetError(1); }
}
// The object names of the links of every record of this device type, each once: the words of the form
// MODULE::name.index. A link that cannot be read is left to the initialization of its record.
std::vector<std::string> linkNames()
{
    std::set<std::string> names;
    if(!pdbbase)return {};
    DBENTRY entry;
    dbInitEntry(pdbbase,&entry);
    for(long type=dbFirstRecordType(&entry);!type;type=dbNextRecordType(&entry)) {
        for(long record=dbFirstRecord(&entry);!record;record=dbNextRecord(&entry)) {
            if(dbFindField(&entry,"DTYP"))continue;
            const char* device=dbGetString(&entry);
            if(!device || std::strcmp(device,"snmp3"))continue;
            for(const char* field:{"INP","OUT"}) {
                if(dbFindField(&entry,field))continue;
                const char* text=dbGetString(&entry);
                if(!text || *text!='@')continue;
                try {
                    const auto link=snmp3::parseRecordLink(text);
                    if(link.named && link.object.find("::")!=std::string::npos)names.insert(link.object);
                } catch(const std::exception&) {}
            }
        }
    }
    dbFinishEntry(&entry);
    return std::vector<std::string>(names.begin(),names.end());
}
const iocshArg probeExecutable = {"absolutePath",iocshArgString};
const iocshArg* const probeArguments[] = {&probeExecutable};
const iocshArg sessionName = {"name",iocshArgString};
const iocshArg sessionAddress = {"address",iocshArgString};
const iocshArg optionWords = {"[options]",iocshArgArgv};
const iocshArg* const sessionArguments[] = {&sessionName,&sessionAddress,&optionWords};
const iocshArg sessionPattern = {"pattern",iocshArgString};
const iocshArg* const optionsArguments[] = {&sessionPattern,&optionWords};
const iocshArg showVerbosity = {"verbosity",iocshArgInt};
const iocshArg* const showArguments[] = {&sessionPattern,&showVerbosity};
const iocshArg subscriptionName = {"name",iocshArgString};
const iocshArg subscriptionSession = {"session",iocshArgString};
const iocshArg subscriptionPeriod = {"periodMs",iocshArgString};
const iocshArg* const subscriptionArguments[] = {&subscriptionName,&subscriptionSession,&subscriptionPeriod};
const iocshFuncDef subscriptionDefinition = {"snmp3Subscription",3,subscriptionArguments,
    "Define a set of read objects of one session that the module reads every period, 100 to 3600000 ms; a record "
    "with SCAN I/O Intr whose link names it processes on each result."};
const iocshArg mibPath = {"path",iocshArgString};
const iocshArg mibSystem = {"[system]",iocshArgString};
const iocshArg* const mibArguments[] = {&mibPath,&mibSystem};
const iocshFuncDef mibDefinition = {"snmp3Mib",2,mibArguments,
    "Name a directory or a file of MIB modules of the IOC, searched before the mibs directory of the module and "
    "the directories of the system; with the word system, the directory replaces the ones of the system."};
const iocshFuncDef probeDefinition = {"snmp3ProbePath",1,probeArguments,
    "Select the absolute probe product before the first session; another path afterwards is rejected."};
const iocshFuncDef sessionDefinition = {"snmp3Session",3,sessionArguments,
    "Define a unit under a name: address is numeric, alone or with a port (192.0.2.1:161, [2001:db8::1]:161); "
    "options are name=value words with the names of the profile fields of a configuration file."};
const iocshFuncDef optionsDefinition = {"snmp3Options",2,optionsArguments,
    "Set name=value options of the sessions whose names match the pattern and exist when the command runs."};
const iocshFuncDef showDefinition = {"snmp3Show",2,showArguments,
    "Show the sessions whose names match the pattern: verbosity 0 one line per name, 1 their values and the "
    "objects of their records; no secret values."};
const iocshArg configFile = {"config", iocshArgString};
const iocshArg nativeProbe = {"nativeProbe", iocshArgString};
const iocshArg* const loadArguments[] = {&configFile, &nativeProbe};
const iocshArg workerExecutable = {"absolutePath",iocshArgString};
const iocshArg* const workerArguments[] = {&workerExecutable};
const iocshArg addressArgument = {"address",iocshArgString};
const iocshArg countArgument = {"count",iocshArgString};
const iocshArg bytesArgument = {"bytes",iocshArgString};
const iocshArg* const queueArguments[] = {&addressArgument,&countArgument,&bytesArgument};
const iocshFuncDef workerDefinition = {"snmp3WorkerPath",1,workerArguments,"Select an absolute worker product before snmp3Load; a different path after a load is rejected."};
const iocshFuncDef queueDefinition = {"snmp3QueueLimit",3,queueArguments,"Set per-address count 1-16384 and bytes 1-67108864; retain accepted work."};
const iocshFuncDef loadDefinition = {
    "snmp3Load", 2, loadArguments, "Load schema-1 JSON after snmp3WorkerPath and before binding/iocInit; nativeProbe is absolute."
};
const iocshFuncDef configDefinition = {
    "snmp3ConfigReport", 0, NULL, "Report revision, frozen state and definition counts; no secret values."
};
const iocshFuncDef reportDefinition = {
    "snmp3Report", 0, NULL, "Report implemented capabilities."
};
const iocshFuncDef runtimeDefinition = {
    "snmp3RuntimeReport", 0, NULL, "Report observed module lifecycle state and counts."
};
const iocshFuncDef stopDefinition = {
    "snmp3Stop", 0, NULL, "Freeze configuration, stop and join the module thread for this activation."
};

void hook(initHookState state)
{
    static bool isolatedCleanup=false;
    const char* name = NULL;
    switch (state) {
    case initHookAtIocBuild:
        // The sessions are completed before a record takes a binding; one that lacks a value its version
        // needs is left out, named here, and fails the startup script.
        try { snmp3::Config::instance().finalize(); }
        catch (const std::exception& error) { std::fprintf(stderr, "snmp3: %s\n", error.what()); iocshSetError(1); }
        // The names of all record links are resolved in one run of the probe before a record is initialized;
        // a record whose name did not resolve is rejected at its initialization.
        try {
            for (const auto& message : snmp3::Config::instance().resolve(linkNames()))
                std::fprintf(stderr, "snmp3: %s\n", message.c_str());
        } catch (const std::exception& error) { std::fprintf(stderr, "snmp3: MIB names: %s\n", error.what()); }
        return;
    // The requests of the subscriptions run while Base scans: a result is given to records that can process.
    case initHookAfterIocRunning: snmp3::Requests::instance().scanning(true); return;
    case initHookAtIocPause: snmp3::Requests::instance().scanning(false); return;
    case initHookAfterFinishDevSup: name = "AfterFinishDevSup"; break;
    case initHookAfterInitialProcess: name = "AfterInitialProcess"; break;
    case initHookAtShutdown: name = "AtShutdown"; break;
    case initHookAfterStopScan: name = "AfterStopScan"; break;
    case initHookAfterStopCallback: name = "AfterStopCallback"; break;
    case initHookBeforeFree: isolatedCleanup=true; return;
    case initHookAfterShutdown: name = "AfterShutdown"; break;
    default: return;
    }
    std::printf("snmp3 hook: %s\n", name);
    snmp3::Runtime& runtime = snmp3::Runtime::instance();
    if (state == initHookAfterFinishDevSup) {
        isolatedCleanup=false;
        snmp3::Config::instance().freeze();
        snmp3::Config::instance().report();
        if (!runtime.start()) {
            std::fprintf(stderr, "snmp3: runtime startup failed; admission closed\n");
            iocshSetError(1);
        }
    } else if (state == initHookAtShutdown) {
        runtime.stop();
        snmp3::Requests::instance().permitDetach();
    } else if(state==initHookAfterShutdown && isolatedCleanup) {
        snmp3::Requests::instance().queuesDestroyed();
    }
}
}

extern "C" void snmp3Registrar()
{
    snmp3::Runtime::instance();
    snmp3::Config::instance();
    // Base testdbCleanup removes hooks; each real registrar invocation restores it.
    initHookRegister(hook);
    iocshRegister(&reportDefinition, report);
    iocshRegister(&runtimeDefinition, runtimeReport);
    iocshRegister(&stopDefinition, stop);
    iocshRegister(&loadDefinition, load);
    iocshRegister(&configDefinition, configReport);
    iocshRegister(&workerDefinition, workerPath);
    iocshRegister(&queueDefinition, queueLimit);
    iocshRegister(&probeDefinition, probePath);
    iocshRegister(&mibDefinition, mib);
    iocshRegister(&subscriptionDefinition, subscription);
    iocshRegister(&sessionDefinition, session);
    iocshRegister(&optionsDefinition, options);
    iocshRegister(&showDefinition, show);
}

epicsExportRegistrar(snmp3Registrar);
