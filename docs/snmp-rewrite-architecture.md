# Architecture of the snmp3 module

The snmp3 module splits one input/output controller (IOC) of the Experimental Physics and Industrial Control System (EPICS) from the native code of the Simple Network Management Protocol (SNMP). The IOC links only EPICS Base. A separate worker process per device address runs Net-SNMP. This page explains how the products fit together, which component owns which data, and how requests, configuration, and lifecycle events move through the module.

## Scope

This page covers the build layout, the ownership of data and threads, and the data flow of requests, configuration, lifecycle, and native calls.

**Out of scope:** the startup configuration fields (see [Startup configuration](snmp-rewrite-config.md)); the worker commands, limits, and timers (see [Worker commands, limits and timers](snmp-worker-reference.md)); the measured request times (see [Request performance](snmp-performance/evaluation.md)).

Other abbreviations on this page: YAJL is the Yet Another JSON Library, UDF is the undefined flag of a record, VAL is the value field of a record, JSON is the JavaScript Object Notation, OID is an object identifier, PDU is a protocol data unit of SNMP, and UDP is the user datagram protocol.

## Build and runtime structure

The top-level Makefile selects only `configure` and `snmp3App`.

- The directory `snmp3App/src` builds the Base-only support library, a native-free shared wire library, and a separate IOC.
- The directory `snmp3App/native` builds the separate native library, the capability probe, and the actual address worker. Its tests directory builds an isolated native agent, an API probe, and an adapter driver.
- The build requires `src` before `native`, and `src` and `native` before `native/tests`.

The generated registration combines `base.dbd` and `snmp3.dbd` and keeps the actual library registrar. The IOC binary depends on `libsnmp3`, `libdbRecStd`, `libdbCore`, and `libCom`. It does not link Net-SNMP or any legacy SNMP production object. The local `configure/RELEASE.local` hook selects an existing Base installation, and the repository commits no machine-specific Base path.

Each source file owns one concern:

| File | Owns |
| --- | --- |
| `Main.cpp` | The Base command-shell entry and the checked exit status |
| `Register.cpp` | The Base hooks and the capability, lifecycle, and configuration commands |
| `Runtime.cpp` | One joinable event-driven module thread, the Base events, and the synchronized state |
| `Scheduler.cpp` | Atomic address admission, the first-in, first-out (FIFO) queue, full reservations, and terminal consumption |
| `Supervisor.cpp` | The actual spawn, IPC, deadline containment, and exact reap of the worker process |
| `Json.cpp` | A bounded strict owned tree built with the YAJL JSON parser of Base |
| `Config.cpp` | Validation of security, endpoints, bindings, secret descriptors, and the native catalogue, then publication of one immutable snapshot |
| Binding and Value headers | Owned component data without record pointers |
| `Conversion.cpp` | Checks of record representations |
| `DeviceSupport.cpp` | The eleven typed device support entry tables (DSETs) |
| `Request.cpp` | Immutable record contexts, terminal borrowing, Base callback retry, completion entry, and lifetime accounting |

Native requests run in separate workers.

## Request data flow

A request moves through the module in this order:

1. The component caller admits an owned typed request through an immutable binding.
2. The per-address scheduler sends bounded framed inter-process communication (IPC) to the actual address worker.
3. The single-session adapter of the worker calls Net-SNMP and returns owned positional results.
4. The scheduler consumers borrow and release the reserved terminals. Releasing the charge also requires real native retirement or the reap of the worker.

For a record, the first-pass DSET captures Base-prepared output and admits the request through Runtime. The servicing owner borrows a terminal, reserves the callback ownership before the enqueue, and retries queue failures.

An entered callback takes the record lock and validates the exact identity. It stages the checked data and alarm, then calls the real `rset` process. Only a matching DSET completion publishes staged native input. Base selects simulation, processes monitors and the forward link (FLNK), and clears the process-active flag (PACT). The wrapper releases the terminal before it drops the record lock. Native retirement stays separate.

A record that has cleared PACT can admit its next request while the native retirement of the previous request is pending. The next request queues behind that retirement in the per-address FIFO and keeps the deadline of its own admission. A Base reprocess therefore carries the latest value without a DSET wait or retry.

When a request reaches its deadline before it is transmitted, the prepare step of the entered callback stages the alarm message (AMSG) `deadline before send`. The callback applies it through the alarm message call of Base before the record process.

The native worker holds no record pointers. Transport owners and schedulers write no record fields. Profiles and contexts keep distinct sessions within one canonical address worker. The IOC rejects stale activation, epoch, and request identity.

Only a contiguous FIFO prefix batches. Ungrouped bindings need exactly equal absolute deadlines. The members of one declared group batch whatever their deadlines and share the earliest one. The module never replays ambiguous transmitted work automatically. The [supervision page](snmp-worker-supervision.md#batching-and-declared-groups) explains the batching rules.

The `snmp3RuntimeReport` command prints the address queue counters, the admission statistics of each address (`snmp3 admit:`), and the number of servicing-loop passes (`snmp3 loop:`). The counters include the generations admitted behind a retirement. The [runtime report lines](snmp-worker-reference.md#runtime-report-lines) describe the fields.

## Ownership and dependencies

The runtime owns the configuration, the queues, the requests, IPC, and the threads. It does not depend on the manager, host, group, OID, or process-variable classes of devSnmp, on polling readback callbacks, or on the legacy network loop or its exit flag. The legacy sources serve only as comparison baselines and stay separate from this module.

Net-SNMP owns the protocol encoding, the native retries, the discovery, and the user-based security model (USM). Base owns the record processing, the alarms, the callback facilities, and the thread APIs.

The lifecycle, the configuration, the native adapter, and the scheduler have separate qualified products. Device support has accepted integration evidence within the [qualification scope](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/milestone-5dff352.md#current-required-verification-results-2026-10-08). The capability report identifies compiled support. Deferred cases and hardware acceptance stay separate from that evidence.

## Configuration data flow

The IOC command passes the configuration and the absolute helper paths to Config. Config reads strict JSON and observes the real native helper through a spawn and a pipe. It reads secrets through checked descriptors and validates every reference.

Candidate construction holds no Config state lock. Publication takes one mutex, checks the frozen state again, replaces the snapshot, and increments its revision. Any rejection leaves the previous snapshot and revision unchanged.

The native helper enumerates the selected Net-SNMP algorithms and tests the callable hash and encryption operations. It emits bounded schema-1 JSON and opens no native session. The parent bounds the output and the duration and waits for the cleanup of the child. Native build flags are confined to the native products, and the IOC stays Base-only.

A successful API binding owns its snapshot and freezes Config. The AfterFinishDevSup hook freezes and reports Config before Runtime readiness and PINI (process at initialization). An explicit stop also freezes Config, including a cold stop. The process-owned Config and its frozen flag survive isolated database cleanup. The numeric report omits secret values, paths, and definition identifiers. The [configuration reference](snmp-rewrite-config.md) lists the exact fields and bounds.

## Lifecycle data flow

Each real registrar invocation registers the Base init hook. The process-owned Runtime initialization uses `epicsThreadOnce` and registers one `epicsAtExit` fallback. Base isolated-database cleanup can remove hooks and registrar records, so the next real registration restores the hook. Runtime events, locks, and storage survive database cleanup for the process lifetime.

The AfterFinishDevSup hook freezes the configuration, then starts the thread and waits for its readiness event before it returns. Base then performs initial processing, including the PINI records. The servicing thread handles deadlines, nonblocking IPC, and supervision.

An empty foundation has no worker and waits for a stop. Configured record requests use the same serving thread and worker ownership, with Base callback threads performing the locked record completion.

The AtShutdown hook closes the admission permission, signals the thread, and joins it before Base reaches AfterStopScan and AfterStopCallback. The late process fallback invokes the same idempotent stop method. The module observes the exit count and the join count separately.

The lifecycle mutex serializes start and stop. The operation mutex protects the startup and configuration operations, and a stop holds it across neither the join nor the record drain. A separate state mutex protects the state, the admission, the thread identity, and the counters. The callback entry mutex is released before `dbScanLock`, the `rset` process, and FLNK. The readiness, join, and entered-callback safety waits hold no state or record lock that progress needs.

A stop closes the record admission, joins the native servicing, and drains borrowed terminals through Base callbacks. The 2000 ms attempt closes the producers and the entry gate on success or expiry. An expiry prevents a restart. Entered callbacks finish before AtShutdown enables detach.

The detached callback storage remains until the actual queue is destroyed in an isolated shutdown. A callback join alone does not permit freeing it. A non-isolated shutdown retains the storage that Base references. The [supervision page](snmp-worker-supervision.md#lifecycle-and-stop) explains the stop rules in full.

## Local native data flow

The separate adapter driver loads the actual Config snapshot and binds owned definitions. One local Worker initializes Net-SNMP, checks the shared capability catalogue, and owns single-session handles. SNMPv3 discovery-only sessions resolve engine IDs before the credentials are registered. Process-lifetime engine and user associations protect shared USM keys across contexts and across close and reopen.

Native PDU builders consume the owned request payload. The large-file-descriptor event service merges the socket interests and the deadlines for every session. Native callbacks copy typed response storage, keep positional OIDs and the status and index, then queue one owned terminal completion. Delivery follows the native-stack unwind. A close keeps pending contexts through the native callbacks and closes each handle once.

Actual native agents and external UDP faults verify this library in separate processes. The IOC services the address queues and IPC while the native calls stay in separate workers. The record products exercise the DSET boundary through actual Base processing and native agents. Hardware traffic remains a separate acceptance requirement. The [native transport page](snmp-native-transport.md) defines the lifetime and security rules.

## Address supervision ownership

The [supervision page](snmp-worker-supervision.md) defines the immutable bootstrap, the product and library handshake, the checked stream frames, the count and charge accounting, the per-address fairness and deadlines, and the stop and restart ownership.

AfterFinishDevSup readiness precedes initial processing. AtShutdown closes dispatch and joins while the Base callback services exist. IncompleteStopped keeps the exact old worker and terminal ownership after the join. It blocks reuse until a nonblocking reconciliation settles it. An expired record drain separately blocks reuse, even after the native ownership settles. The [work register](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/milestone-5dff352.md) records the record coverage, rather than inferring it from a component PASS.

## Structure of the verification runs

The verification runs at several levels, and each level uses the shipped products:

- **Foundation.** The runner validates the selected Base, checks the effective build configuration, forces the application build, and checks its commands, objects, dependencies, and generated registration. It starts a private Base `caRepeater`, observes the socket ownership, then runs the actual IOC. A standard longin record with constant input 42 is processed through PROC with the processing trace enabled. The `dbpr` output must show VAL 42, UDF 0, and PACT 0. The runtime loader diagnostics establish the loaded-library paths. The runner waits for the normal IOC exit and the repeater termination. The negative startup and command-line cases retain their logs. A timeout or forced termination is a failure.
- **Lifecycle.** The runner uses the actual IOC with the generated registration and the PINI database, the shipped Runtime library in a separate test product, and two actual Base isolated databases. The non-root failure child applies `RLIMIT_NPROC=0` before the actual module thread creation, restores its limit, and exercises the shipped Main implementation after a successful Continue-policy script. That direct Main case is supplemental module testing, not a full IOC build-failure integration test. No internal Base or thread function is substituted.
- **Configuration.** A separate ConfigTest links to the shipped library, the real IOC startup files, and the actual native helper. Disposable secret fixtures enter the real file-validation path. External fault helpers replace only the spawned process boundary, and gated race helpers exec the actual native product. The product, source, and fixture hashes, the loaded libraries, the deadlines, and the waited cleanup identify the verification boundary of each run.

The [native transport page](snmp-native-transport.md) and the [supervision page](snmp-worker-supervision.md) describe the native and worker levels. The [verification procedures](snmp-worker-verification.md) run them.
