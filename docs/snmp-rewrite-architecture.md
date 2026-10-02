# Independent SNMP Architecture

## Current Build And Runtime

The top-level Makefile selects only `configure` and `snmp3App`.
`snmp3App/src` builds Base-only support, a native-free shared wire library
and a separate IOC.
`snmp3App/native` builds the separate native library and capability probe;
it also builds the actual address worker. Its tests directory builds isolated
native agent, API probe and adapter driver. Build directory dependencies require
src before native, and src/native before native/tests.
Generated registration combines `base.dbd` and `snmp3.dbd`, retaining
the actual library registrar. The IOC links `snmp3`, dbRecStd, dbCore,
ca and Com. It does not link Net-SNMP or any legacy SNMP production object.

`Main.cpp` owns only Base command-shell entry and checked exit status.
`Register.cpp` owns Base hooks and capability/lifecycle/configuration commands.
`Runtime.cpp` owns one joinable event-driven module thread, Base events and
synchronized state. Scheduler.cpp owns atomic address admission, FIFO, full
reservations and terminal consumption. Supervisor.cpp owns actual spawn, IPC,
deadline containment and exact PID reap. Native requests run in separate workers.
`Json.cpp` uses Base YAJL to produce a bounded strict owned tree. `Config.cpp`
validates security, endpoints, bindings, secret descriptors, and the native
catalogue before publishing one immutable snapshot. Binding and Value headers
define owned component data without record pointers. `Conversion.cpp` checks
record representations; `DeviceSupport.cpp` owns eleven typed DSETs.
`Request.cpp` owns immutable record contexts, terminal borrowing, Base callback
retry, completion entry and lifetime accounting.
The local `configure/RELEASE.local` hook selects an existing Base install;
no machine-specific Base path is committed.

## Component Data Flow

The implemented component caller admits an owned typed request through an
immutable binding. The per-address scheduler sends bounded framed IPC to the
actual address worker. Its native single-session adapter calls Net-SNMP and
returns owned positional results. Scheduler consumers borrow/release reserved
terminals; charge release additionally requires real native retirement or reap.
First-pass DSET captures Base-prepared output and admits through Runtime. The
servicing owner borrows a terminal, reserves callback ownership before enqueue
and retries queue failures. An entered callback takes the record lock, validates
the exact identity, stages checked data/alarm and calls the real rset process.
Only matching DSET completion publishes staged native input. Base selects
simulation, processes monitors/FLNK and clears PACT; the wrapper releases the
terminal before dropping the record lock. Native retirement remains separate.

The native worker holds no record pointers. Transport owners and schedulers
write no record fields. Profiles and contexts retain distinct sessions within
one canonical address worker. The IOC rejects stale activation/epoch/request
identity. Only a contiguous FIFO prefix with exactly equal absolute deadlines
can batch. Ambiguous transmitted work is never automatically replayed.

## Ownership And Dependencies

The runtime owns configuration, queues, requests, IPC and threads.
It does not depend on devSnmp manager/host/group/OID/PV classes, polling
readback callbacks, the legacy network loop or its exit flag. Existing
sources are comparison baselines and are not renamed into the new target.
Net-SNMP owns protocol encoding, native retries, discovery and USM.
Base owns record processing, alarms, callback facilities and thread APIs.

Lifecycle, configuration, native adapter and scheduler have separate qualified
products. Device support has current integrated subset evidence; its complete
record matrix remains pending. Component boundaries do not imply acceptance
of an unexecuted integration path.

## Implemented Configuration Data Flow

The IOC command passes configuration and absolute helper paths to Config.
Config reads strict JSON, observes the real native helper through spawn/pipe,
reads secrets through checked descriptors, and validates every reference.
Candidate construction holds no Config state lock. Publication takes one mutex,
checks frozen state again, replaces the snapshot, and increments its revision.
Any rejection leaves the previous snapshot and revision unchanged.

The native helper enumerates selected Net-SNMP algorithms and tests callable
hash/encryption operations. It emits bounded schema-1 JSON and opens no native
session. The parent bounds output and duration and waits for child cleanup.
Native build flags are confined to native products; the IOC remains Base-only.

A successful API binding owns its snapshot and freezes Config. AfterFinishDevSup
freezes and reports Config before Runtime readiness and PINI. Explicit stop
also freezes, including cold stop. Process-owned Config and its frozen flag
survive isolated database cleanup. The numeric report omits secret values,
paths, and definition identifiers. Exact fields and bounds are in the
[configuration reference](snmp-rewrite-config.md).

## Foundation Verification Flow

The real runner validates the selected Base, checks the effective build
configuration, forces the application build and checks its commands,
objects, dependencies and generated registration. It starts a private
Base caRepeater, observes socket ownership, then runs the actual IOC.
A standard longin with constant input 42 is explicitly processed through
PROC with processing trace enabled. Actual dbpr output must show VAL 42,
UDF 0 and PACT 0. Runtime loader diagnostics establish loaded-library paths.
Normal IOC exit and repeater termination are waited for. Negative startup
and CLI cases retain their logs. Timeout/forced termination is a failure.

## Implemented Lifecycle Data Flow

Each real registrar invocation registers the Base init hook; process-owned
Runtime initialization uses epicsThreadOnce and registers one epicsAtExit
fallback. Base isolated-database cleanup may remove hooks and registrar
records, so the next real registration restores the hook. Runtime events,
locks and storage survive database cleanup for the process lifetime.

AfterFinishDevSup freezes configuration, then starts the thread and waits for its readiness event before
returning. Base then performs initial processing, including PINI records.
The servicing thread handles deadlines, nonblocking IPC and supervision.
An empty foundation has no worker and waits for stop. Configured record requests
use the same serving thread and worker ownership, with Base callback threads
performing the locked record completion.
AtShutdown closes admission permission, signals the thread and joins it
before Base reaches AfterStopScan and AfterStopCallback. The late process
fallback invokes the same idempotent stop method. Exit and join counts
are separate observations.

The lifecycle mutex serializes start/stop. The operation mutex protects startup
and configuration operations; stop holds it across neither join nor record
drain. A separate state mutex protects state, admission, thread identity and
counters. The callback entry mutex is released before dbScanLock, rset process
and FLNK. Readiness, join and entered-callback safety waits hold no state or
record lock needed for progress.

Stop closes record admission, joins native servicing and drains borrowed
terminals through Base callbacks. The 2000 ms attempt closes producers and the
entry gate on success or expiry. Expiry prevents restart; entered callbacks
finish before AtShutdown enables detach. Detached callback storage remains
until actual queue destruction in isolated shutdown; callback join alone does
not permit freeing it. Non-isolated shutdown retains Base-referenced storage.

Lifecycle verification runs the actual IOC/generated registration/PINI DB,
the shipped Runtime library in a separate test product, and two actual
Base isolated databases. The non-root failure child applies RLIMIT_NPROC=0
before actual module thread creation, restores its limit, and exercises the
shipped Main implementation after a successful Continue-policy script.
That direct Main case is supplemental module testing, not full IOC build
failure integration. No internal Base/thread function is substituted.

Configuration verification adds a separate ConfigTest linked to the shipped
library, real IOC startup files, and the actual native helper. Disposable
secret fixtures enter the real file-validation path. External fault helpers
replace only the spawned process boundary; gated race helpers exec the actual
native product. Product/source/fixture hashes, loaded libraries, deadlines,
and waited cleanup identify each run's verification boundary.

## Implemented Local Native Data Flow

The separate adapter driver loads the actual Config snapshot and binds owned
definitions. One local Worker initializes Net-SNMP, checks the shared capability
catalogue and owns single-session handles. V3 discovery-only sessions resolve
engine IDs before credential registration. Process-lifetime engine/user
associations protect shared USM keys across contexts and close/reopen.

Native PDU builders consume owned request payload. Large-FD event service merges
socket interests and deadlines for every session. Native callbacks copy typed
response storage, retain positional OIDs and status/index, then queue one owned
terminal completion. Delivery follows native-stack unwind. Close retains pending
contexts through native callbacks and closes each handle once.

Actual native agents and external UDP faults verify this library in separate
processes. The IOC services address queues and IPC while native calls remain in separate
workers. Record products exercise the candidate DSET boundary; its full matrix
and hardware traffic remain pending qualification. The
[native transport reference](snmp-native-transport.md) defines lifetime, security,
representation limits and executable verification commands.

## Address Supervision Ownership

The [worker reference](snmp-worker-supervision.md) defines immutable bootstrap,
actual product/library handshake, checked stream frames, Q/count accounting,
per-address fairness/deadlines and stop/restart ownership. AfterFinishDevSup
readiness precedes initial processing; AtShutdown closes dispatch and joins
while Base callback services still exist. IncompleteStopped retains exact old
worker/terminal ownership after join and blocks reuse until nonblocking
reconciliation settles it. An expired record drain separately blocks reuse even
after native ownership settles. Current record coverage is recorded in the
[work register](milestone-5dff352.md), rather than inferred from component PASS.
