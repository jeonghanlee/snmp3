# Independent SNMP Module Contract

## Implemented Foundation

The default build produces `libsnmp3` and `snmp3Ioc` against EPICS
Base 7.0.10. Compilation rejects other Base major/revision/modification
versions. The support library registers `snmp3Report`, `snmp3RuntimeReport`,
`snmp3Stop`, `snmp3Load`, `snmp3ConfigReport`,
`snmp3WorkerPath` and `snmp3QueueLimit`. Capability is
`owned-worker-transport; recordSupport=unavailable`. The native-free
`libsnmp3Wire` is shared with the actual separate worker.

The IOC uses Base `iocsh` and `epicsExit`. With a startup file, a nonzero
`iocsh` result exits with status 1. Without a file, or after a successful
file, it accepts interactive commands. More than one file argument exits
with status 2. Startup files must begin with `on error break` to propagate
command errors: Base's default script policy is `continue`. The IOC does
not change that Base policy. A returning startup script also fails when
the module retains Failed state, even under Continue. An `exit` command returns from
the current iocsh invocation. In a startup file, Main checks that return and
the retained Failed state, then enters the interactive shell on success.
Returning from the interactive shell leads Main to call epicsExit.

Owned startup definitions, address admission queues, framed IPC and native
worker supervision are implemented. Component callers use the actual
scheduler/worker/native path. No SNMP DSET or record conversion is implemented.
The servicing thread owns deadlines/IPC/reap and never calls native session APIs.
The separate capability probe performs no session open. Native dependencies and
legacy production sources remain excluded from IOC/support/wire products.
The standard longin fixture verifies Base processing, not an SNMP request.

## Selected Interface Direction

The SNMP interface will use new explicit configuration and record bindings.
APC startup files and databases will be migrated deliberately. A legacy
syntax translation adapter is excluded. Existing DBs cannot bind SNMP
records to this foundation IOC. Previous verification qualifies only its
identified previous source, never this independent target.

## Implemented Value And Planned Record Contract

The shipped Value API owns exact signed 64-bit, unsigned 32-bit, Counter64,
binary octets, OID arcs, IPv4, Float32, Float64, and exception tags. Accessors
check tags. Binding handles own immutable profile/endpoint definitions and
their configuration snapshot. No record-field conversion is implemented.

Later implementation will support inputs ai, longin, stringin and waveform;
outputs ao, longout and stringout. Each binding owns endpoint identity,
OID, native value type, capacity, conversion policy and operation.
Integer range, floating-point precision and waveform capacity policies
require concrete contracts and real tests before support is advertised.

Per-record ordering will be request, terminal response/error, locked value
and alarm processing, Base completion, FLNK, then Base clears PACT.
Device support will not manually run FLNK. Fanout does not imply an
asynchronous completion barrier. Successful SET completion and device
readback remain distinct observations. Generation and activation identity
will reject stale or duplicate completions and prevent implicit SET replay.
These are planned module requirements, not verified foundation behavior.

## Implemented Startup Configuration Contract

Schema-1 strict JSON defines complete owned security/profile/endpoint/binding
snapshots. Failed loads preserve the exact published snapshot and revision;
successful loads before freeze replace the whole configuration. A successful
first API binding, AfterFinishDevSup, or snmp3Stop freezes every setting.
Isolated database cleanup does not permit reload.

SNMPv3 configuration accepts native discovery/inheritance declarations and
explicit security/context engine IDs. Algorithms must be advertised and
callable by the selected native helper. The separate native library performs
discovery and sessions; configuration loading itself performs no network I/O.
Secret files require effective-UID ownership and private
permissions, and reports omit secret bytes and identifiers. The
[configuration reference](snmp-rewrite-config.md) defines all fields, limits,
commands, and startup examples.

## Implemented Capacity Contract

Defaults are 1024 commands/address and 1048576 charged bytes. Atomic live
`snmp3QueueLimit` accepts count 1-16384 and bytes 1-67108864. Decreases
preserve accepted work; new admission must fit both totals. All pending states,
including consumed terminals awaiting real retirement/reap, retain full Q/count.
Checked frame/capacity/identity/storage validation precedes publication.
Exact charge, wire, storage and timer rules are in the
[worker reference](snmp-worker-supervision.md).

## Implemented Lifecycle And Supervision

State is Cold, Starting, Running, Stopping, Stopped, IncompleteStopped or Failed.
Start returns after servicing readiness; duplicate Running start creates nothing.
A later isolated activation can start only after old terminal/native/process
ownership settles. Failed preflight/thread creation closes admission and
retains failure; unexpected service failure also closes admission. There is
no operator start, credential replacement or rate reset command.

AfterFinishDevSup freezes Config and starts servicing before initial processing.
AtShutdown closes admission/dispatch/recovery, preserves already selected
terminals, selects Stopping for nonterminal work and joins once before Base
callback services stop. Stop retains old PID/context/reservations when cleanup
cannot be proven. IncompleteStopped blocks start and retains tracking after join;
later stop/fallback/start preflight performs only nonblocking reconciliation.
Repeated/concurrent stop does not create a second join. Cold stop creates no
thread and freezes Config. Empty foundation activations create no native worker.

One worker/address and one active native batch prevent following work from
using a live old slot. Restart requires exact old PID reap and a new epoch.
The conservative first-Batch-byte recovery rule prevents ambiguous SET replay.
Native retry remains distinct. Reports separate actual lifecycle, queue ownership
and worker state; Running does not promise device availability.

Process-owned Config/Runtime survive isolated cleanup and never permit reload.
Readiness waits and joins retain no servicing state lock. Base init hooks are
void, so future DSET must reject closed admission; Main also checks retained
Failed state after a startup script returns. Record callback drain, conversion,
PACT completion and shutdown FLNK remain later R6 integration requirements.

## Verification Boundary

The shipped foundation runner checks real build inputs, generated
registration, actual loader identity, standard-record processing,
startup failures and owned-process cleanup on Linux x86_64. It retains
logs and source/product hashes. The lifecycle runner adds the real idle-thread,
PINI, concurrent stop, isolated reuse and OS-limit failure cases. The configuration
runner adds shipped Config/Value/Binding paths, actual native catalogue and
fault children, publication races, startup freeze, and diagnostic secret checks.
Separate native API/adapter runners exercise actual sessions, ownership, typed
GET/SET, discovery/USM, large-FD deadlines and external faults. Separate ASan/UBSan
products qualify new native code for their executed paths, with uninstrumented
dependencies explicitly bounded. The [native transport contract](snmp-native-transport.md)
defines that component boundary. Actual R5 owner/IPC/worker/Native traffic, external faults and actual IOC
lifecycle use separate current component/IOC/qualification runners. Targeted
ASan/UBSan also instrument new R5 code; dependencies remain uninstrumented and
leaks disabled. Record-driven SNMP, APC, two-OS and sustained qualification
require later implementations and fixtures.
