# Independent IOC Verification

## Scope And Prerequisites

Linux x86_64, Python 3, GNU make, Perl, a C++ compiler, ldd and an existing
shared-library EPICS Base 7.0.10 installation are required. No installation
is performed. Run from the rewrite checkout. The runner creates
`configure/RELEASE.local` using `--base`; an existing differing override
is rejected. The effective Base, RULES and install location are checked
before building. Site overrides must not introduce other production code.

## Execution

Use the existing Base path and a new evidence directory. Each invocation
below is one physical line. BASE_PATH is an operator-selected variable.

```bash
BASE_PATH=/absolute/path/to/installed/base
python3 tests/rewrite/run_independence.py --base "$BASE_PATH" --output work/r1-evidence
```

The separate native products additionally require an installed Net-SNMP library and
`net-snmp-config` with the catalogue APIs used by the shipped source.
`NET_SNMP_CONFIG` selects that build tool for the separate native product.

The runner forces the real application build with `make -B -j2 --output-sync=recurse`; it never
builds or modifies the selected Base. Evidence is retained on success and
failure. Repeating a run requires a different `--output` directory.
The process returns zero only when every foundation check passes.

## Observations And Evidence

`results.json` records check outcomes, object names, selected Base,
source/product SHA256 values, loaded-library paths and repeater cleanup.
Adjacent files retain build/configuration/dry-run commands and output,
dependency contents, ldd output, startup files, IOC stdout/loader stderr,
negative controls and per-command exit outcomes.

The real IOC loads the generated registration, reports its capability,
processes a standard longin through PROC and prints its fields. A trace
line is mandatory: constant initialization alone cannot pass processing.
Unknown commands in a script beginning with `on error break` and a missing
startup file must fail. Invalid CLI/Base paths must fail before changing
configuration or building. Dynamic library checks cover actual loader
initialization as well as the link dependency listing.

The runner owns its private caRepeater and waits for termination. Normal
IOC completion is observed through its real exit status. Timeouts are
failures, with forced termination recorded in command outcome files.
Concurrent IOC tests use different private CA ports.

## Limits

PASS qualifies only the independent build and Base foundation. It does
not qualify SNMP requests, conversions, queues, native session cleanup,
APC CA/PVA consumers or in-flight lifecycle behavior. Module-thread
qualification requires the separate lifecycle runner below.
The foundation fixture exercises standard Base support. The product also
registers the eleven candidate snmp3 DSETs; the separate record runner below
qualifies their executed paths. There are no IOC session commands. Full two-OS
and one-hour resource checks remain separate work.
Startup command-error propagation requires `on error break`; the Base
script default remains unchanged.

## Lifecycle Verification

Use the same existing Base path and a new directory:

```bash
python3 tests/rewrite/test_lifecycle.py --base "$BASE_PATH" --output work/r2-evidence
```

This runner first executes the complete shipped independence runner, then
builds a separate tests/rewrite product. The default production build remains
unchanged in scope; it does not compile the test product. The separate test
product links the same snmp3 library and its actual generated Base
registrar. No internal mock replaces thread creation, join, processing or
Base shutdown. Test code never enters the production support library.

Eight case processes exercise normal IOC PINI/shutdown, cold fallback,
pre-init error, partial iocBuild error, explicit/repeated stop, direct Runtime
concurrent stop/repeated activations, actual OS-limit failure and two isolated
databases. The module case runs twenty activations with eight real joinable
caller threads per activation. Test owners join the callers. A separate
Base testdb cycle checks actual record fields and hook restoration after cleanup.
It also requires the same process-owned Config snapshot and frozen flag after
both real database cleanups.

The failure case requires non-root Linux execution with an effective
RLIMIT_NPROC restriction. It initializes Base thread facilities and Runtime,
limits only the child, calls real epicsThreadCreateOpt and restores the limit.
An ineffective limit fails the case. The case then executes the shipped Main
source under a renamed test entry point: a successful Continue-policy script
must still exit 1 because Runtime retains Failed. This direct Main/module
case does not claim an actual iocInit failure integration test.

The main results.json records named checks, input/product SHA256, actual
loaded-library paths and private repeater cleanup. Per-case command,
stdout/stderr, exit/reap status and startup scripts remain alongside it.
Normal cases require exit 0. Intentional script/failure cases require exit 1;
that expected exit never represents an unobserved graceful success. A timeout
or forced termination always fails. Before execution, a differing local Base
selection or an existing output directory is rejected.

The lifecycle checkpoint has no SNMP request, production completion callback,
queue, native session or worker. In-flight shutdown/FLNK, callback pressure,
storage quiescence, two-OS, sanitizers and one-hour resource qualification
require later implementations and real integration tests. The existing Base
callback-queue issue remains an unresolved external dependency for those tests;
this checkpoint neither patches Base nor demonstrates that issue resolved.

## Configuration verification

Run the complete configuration checkpoint with the existing Base path and a
new evidence directory:

```bash
python3 tests/rewrite/test_config.py --base "$BASE_PATH" --output work/r3-evidence
```

This invocation runs the complete foundation and lifecycle runners first,
including actual rebuilds. It then executes the shipped ConfigTest, native
helper, IOC, and [startup example](config/startup.cmd). Generated secrets are
disposable fixtures in a private evidence directory; no real credentials are
required. IOC/support production targets exclude test source. Separate native
test products are declared in `snmp3App/native/tests` and audited independently.

| Area | Actual exercised path |
| --- | --- |
| Strict JSON | Production YAJL callbacks, JSON5/comments/duplicates/UTF-8 rejection, depth/size/entry bounds |
| Schema | Profile defaults and limits, all security levels, numeric endpoints, OID and capacity validation |
| Secrets | Real opened files, effective ownership when supported, modes, symlink/directory/FIFO/content rejection |
| Publication | Exact retained snapshot/revision on failure, whole replacement, concurrent bind/freeze races |
| Owned values | Actual shipped Value factories/accessors, limits, binary lifetime, floating bits, exception tags |
| Native catalogue | Actual enumeration and crypto calls, every advertised algorithm, loaded native library hashes |
| Helper faults | Real spawn failure, malformed/oversized output, timeout, closed stdout, wait/reap identity |
| IOC startup | Generated registration, pre-PINI freeze, iocBuild/init/stop rejection, Continue-policy preservation |
| Operator contract | Shipped JSON/startup example, command help, numeric reports, diagnostic secret exclusion |

The race gate waits at the external process boundary and then execs the
production helper. Positive capability checks never substitute an internal
native function or catalogue. Fault scripts replace only the selected child
executable for negative cases.

`results.json` records named checks, component assertion counts, actual loaded
library paths/hashes, unchanged source/product hashes, fixture hashes/metadata,
native observations, and cleanup. Per-case stdout, stderr, command outcome,
and startup files remain in the evidence directory. Unreadable fixture content
is hashed before its deliberate permission restriction; metadata identifies it.

The runner requires five-second helper deadlines to return within its outer
ten-second allowance and verifies the recorded child PID/start-time is absent
after return. A helper failure preserves the exact valid snapshot. The private
repeater is waited for; a timeout or forced process termination fails the run.

If the environment cannot create a real differently owned secret file, the
receipt records `V3.3_wrong_owner` under `not_run`. This condition does not
produce a simulated PASS. Non-root Linux with effective RLIMIT_NPROC limits
remains required by the lifecycle regression.

Unique generated secret sentinels must be absent from actual module diagnostics
and the JSON receipt. Plaintext fixture files intentionally contain disposable
secret data; they are not diagnostic output. Retain evidence privately.

PASS qualifies the configuration and idle-thread checkpoint for the identified
products and environment. Actual SNMP session/discovery, native response
ownership, record binding/conversion, queues, two-OS, sanitizers, and sustained
resource qualification are outside this runner. The
[configuration reference](../../docs/snmp-rewrite-config.md) defines exact limits
and startup behavior; it does not advertise IOC SNMP device communication.

## Native Transport Verification

The [native transport reference](../../docs/snmp-native-transport.md) supplies
the actual preflight, adapter and separate ASan/UBSan commands. Its fixture links
the real installed Net-SNMP agent library and listens on private loopback ports.
The adapter matrix covers versions/security/algorithms, process-pinned USM
compatibility, native retries, positional/typed ownership, large FD and EINTR,
pending close/reentry and actual external UDP faults. No internal mock replaces
the native or adapter path. Native dependencies remain excluded from the IOC.

Run the complete configuration regression after native changes to qualify the
current IOC/support products; previous PASS receipts retain their original
hashes. Native component acceptance does not imply production worker, record,
hardware, two-OS or sustained resource qualification.

## Address Worker Verification

The [worker reference](../../docs/snmp-worker-supervision.md) defines implemented
R5 commands, charge/retirement, IPC/timer/storage limits and actual verification
commands. Build the separate test products before the component/IOC runners:

```bash
make -C tests/rewrite -j4
python3 tests/rewrite/test_r5_components.py --output work/r5-components
python3 tests/rewrite/test_supervisor.py --ioc --output work/r5-ioc
python3 tests/rewrite/test_qualification.py --output work/r5-qualification
python3 tests/rewrite/test_r5_operator.py --output work/r5-operator
```

The actual generated IOC registrar exercises pre-PINI readiness, live limits,
retired-but-unconsumed incomplete stop, nonblocking reconciliation, two isolated
activations and concurrent stop of a SIGSTOP-blocked real worker. Each servicing
thread is joined once and every actual worker PID is waited for. The component
caller uses shipped Runtime APIs; it does not substitute a record DSET.

Qualification includes exact deadline batching/FIFO, all owned types and aliases,
caller mutation, two-address isolation, zero-byte recovery, actual SET commit
followed by worker loss without replay, full/partial/coalesced/stale/malformed IPC,
bootstrap/executable/library rejection, early expiry during real Native/discovery,
live admission/limit/consumption races, high FD, restart exhaustion/backoff,
immutable post-freeze file changes and real pinned-USM conflict. Actual positive
traffic traverses Config/Scheduler/Supervisor/IPC/Worker/Native/agent/consumer.
Only outer process, filesystem or socket boundaries supply faults.

Run separate ASan/UBSan products through the worker reference's selected-product
commands. Private stderr must be present and free of sanitizer reports and secret
sentinels. Base/system/vendor dependencies remain uninstrumented; leak, TSan,
two-OS and sustained resource checks are excluded. No internal positive response
or Ready fixture replaces the actual selected worker.

After production changes, run complete current configuration regressions (which
include R1/R2) and both native phases against the same selected Base:

```bash
python3 tests/rewrite/test_config.py --base "$BASE_PATH" --output work/r5-regressions
python3 tests/rewrite/test_native.py --phase preflight --output work/r5-native-api
python3 tests/rewrite/test_native.py --phase adapter --output work/r5-native-adapter
```

Each PASS qualifies its identified current sources/products, not historical
receipts or record integration. A completion record must map all V5 gates and
include full third-person implementation and second-person operator review.
Privilege-dependent wrong-owner remains an explicit NOT RUN when unavailable.
Evidence directories are mode 0700, generated secrets mode 0600, failed runs are
retained, and diagnostic drops or unwaited children fail qualification.

## Record Verification

Build the current test product and run each case with a new private output
directory. All traffic traverses the shipped record/DSET/Runtime/Scheduler/IPC/
worker/native path and a real loopback agent. Base record support, its callback
queue, FLNK and isolated shutdown run unchanged.

```bash
make -C snmp3App/native/tests -j4
make -C tests/rewrite -j4
python3 tests/rewrite/test_records.py --case baseline --output work/r6-records
python3 tests/rewrite/test_records.py --case edges --output work/r6-edges
python3 tests/rewrite/test_records.py --case alarms --output work/r6-alarms
python3 tests/rewrite/test_records.py --case active --output work/r6-active
python3 tests/rewrite/test_records.py --case policy --output work/r6-policy
python3 tests/rewrite/test_records.py --case numeric --output work/r6-numeric
python3 tests/rewrite/test_records.py --case shutdown --output work/r6-shutdown
python3 tests/rewrite/test_records.py --case queued-shutdown --output work/r6-queued
python3 tests/rewrite/test_records.py --case stop-queued --output work/r6-stop-queued
python3 tests/rewrite/test_records.py --case active-unforced --output work/r6-unforced
python3 tests/rewrite/test_records.py --case accounting --output work/r6-accounting
python3 tests/rewrite/test_records.py --case deadline-queue --output work/r6-deadline-queue
python3 tests/rewrite/test_records.py --case near-deadline --output work/r6-near-deadline
python3 tests/rewrite/test_records.py --case rebuild --output work/r6-rebuild
python3 tests/rewrite/test_records.py --case stop-inflight --output work/r6-stop-inflight
python3 tests/rewrite/test_records.py --case stop-enqueue-failed --output work/r6-stop-enqueue
python3 tests/rewrite/test_records.py --case stop-downstream --output work/r6-stop-downstream
```

| Case | Shipped fixtures and observed boundary |
| --- | --- |
| baseline | records.db and record-output.db; all eleven DSET kinds, independent handles, exact Counter64 and long strings, refused live links, direct DTYP identity checks, actual GET/SET callback pressure |
| edges | record-edges.db and record-order.db; startup rejection, Base SIZV clamps, precision/capacity rejection with prior-data preservation, binary/OID/IPv4 input, FLNK before PACT clearing, six input simulation switches/SDLY, ai RAW, actual binary32 SET/GET bits across four rounding modes, unusable SET rejection and maximum lsi/lso payloads under live queue limits |
| alarms | record-alarms.db and actual outer UDP response dropping; all eleven record kinds receive native timeout before their 5000 ms record deadline, preserve record-specific UDF/publication state and report COMM/INVALID; a separate 1 ms record deadline is observed as IPC Deadline |
| active | record-active.db; actual 750 ms UDP response delay, dbPutField on all five active outputs, immutable first payload and latest RPRO value, exact DBR_INT64, 200-byte first lso payload, Base FLNK, same-value explicit retry, configured native retry and separate GET after successful SET response loss |
| policy | record-policy.db; three first-pass/completion IVOA branches for all five outputs, initial/live longout OOPT rejection, synchronous/delayed output simulation, terminal release/error/FLNK when DSET is bypassed, ao drive/rate/OVAL capture, lso supervisory/closed-loop DOL, short IVOV and Base pre-DSET capacity boundary with actual SET/GET |
| numeric | record-numeric.db; all 68 advertised numeric GET pairs, including 48 native-tag/waveform-FTVL pairs; all 20 numeric SET pairs; range/precision boundaries, previous data and NORD preservation after valid input, complete Counter64 range, nonfinite input and rejected SET with separate native GET |
| shutdown | An actual module callback has entered but waits for a held Base record lock; drain expiry closes the gate, shutdown waits for the lease to finish and restart is refused |
| queued-shutdown | An external callback holds the actual Base queue; a module completion remains queued after gate closure/detach and retains its storage until actual isolated queue cleanup |
| stop-queued | record-queue.db and actual outer UDP response dropping; a stopped worker leaves a Deadline-selected predecessor retirement-pending while a Base RPRO successor is queued behind it, then `Runtime::stop`, the operation behind `snmp3Stop`, completes the successor once and reaps the predecessor; the `stop_queued` event in `record-observations.json` records the wait rule of the [supervision document](../../docs/snmp-worker-supervision.md) |
| active-unforced | record-active.db; no external callback holds the Base consumer, so each of the five outputs is rewritten while its first SET is delayed 750 ms and the reprocess is admitted behind the first request's native retirement; two generations, two completions, the latest value on the wire and NO_ALARM; the case reports in how many trials the queued-behind-retirement branch ran and the Result-to-Retired gap |
| accounting | record-active.db; three trials of the unheld rewrite observe the window with one consumed generation awaiting retirement and one queued (count 2, charged bytes twice a single generation); a count limit of 1 rejects the reprocess synchronously as WRITE/INVALID with PACT cleared, and raising the limit admits an explicit request |
| deadline-queue | record-queue.db and actual outer UDP drop-all; three sampling trials with a 1000 ms budget measure reap, relaunch and Ready after a Deadline, one below-threshold and one above-threshold budget trial follow, and a follow-up SET after the unsent generation is observed; each trial records the put-to-dispatch interval, the AMSG, the alarm and the report counters, and the case reports the threshold as the range over every relaunch it measured |
| near-deadline | record-queue.db and an outer UDP response delay of 300 ms; ten fresh processes with record budgets of 314 to 322 ms place the response just before the deadline and report whether the worker was contained after its result had been selected |
| rebuild | record-queue.db and actual outer UDP drop-all; the stop-queued state (a stopped worker, a retirement-pending predecessor and a queued Base RPRO successor) is stopped and cleaned up in isolation, then the same process loads the databases again and starts a second activation; the second activation runs the complete baseline on a new worker, the first activation's worker is gone, and the report counters start again at zero |
| stop-inflight | record-stop.db (the eleven Timeout records of the alarms case, each with a FLNK to one counter) and actual outer UDP drop-all; all eleven records are admitted on one address, the case waits until a batch is on the worker channel and stops the runtime; one generation is sent and the others are queued, and every record completes once as a communication alarm with INVALID severity, no native publication, PACT and waveform BUSY clear and exactly one FLNK each, the drain succeeds and the worker is reaped |
| stop-enqueue-failed | record-stop.db and actual outer UDP drop-all, three processes; the low-priority Base callback queue is filled by an external blocker before the eleven records are admitted, so every completion enqueue is refused and the stop retries them; released while the runtime thread still runs, or after its 2 s stop bound while only the record drain retries, every record completes once with a communication alarm and one FLNK and the drain succeeds; released after the drain budget has expired, the drain fails, the stop returns without waiting for the queue, the records stay active with PACT set, restart is refused and the isolated cleanup finalizes the eleven completions once without FLNK |
| stop-downstream | record-stop.db with every FLNK pointed through a Channel Access link at a record in another lockset, and actual outer UDP drop-all, two processes; the downstream record's lock is held past the drain budget while the runtime stops: the stop completes with every record completed once, the drain succeeded and the worker reaped, no downstream processing while held, and eleven downstream processings (one per link; Base keeps one pending put per CA link) on Base's CA link thread after the release; held through the IOC shutdown instead, the snmp3 stop completes first and Base's CA link shutdown waits for the lock; the downstream count after that release is reported, not asserted |

The pressure cases fill the actual Base low-priority queue with external
callbacks. Failed module enqueue retains the terminal, full reservation and
PACT, then retries once consumers can proceed. Shutdown faults hold only the
external callback or actual record-lock boundary; no internal function or
terminal fixture replaces the path.

The alarm case observes the actual borrowed IPC terminal while its callback
holds an entry lease and waits for the test-owned record lock. It requires
NativeFailure/native code 2 for each native timeout and Deadline for the
separate record deadline. UDP proxy receipts identify actual responses dropped
after the agent handled the GET or SET. A failed SET response remains distinct
from the agent's committed value; the module adds no retry.

The active case observes actual SET transmission before changing VAL through
dbPutField. An external callback holds the Base consumer until native retirement
of SET and a following GET, so admission is available for RPRO. FIFO GET observes
the first captured payload; a separate GET after RPRO observes the latest value.
Response-drop metadata distinguishes new request IDs on explicit retries from
the same request ID on configured native retries. A separate GET confirms agent
application despite lost successful SET responses. Channel Access writes are
covered by the separate production runner below. The unforced and accounting
cases do not hold the Base consumer, so the reprocess is admitted behind the previous
request's native retirement instead of being rejected.

The policy case holds only the external Base callback consumer while an actual
SET times out. Live IVOA, SIMM and OOPT changes exercise unchanged Base branches
without replacing DSET or terminal processing. Completion preserves errors and
requested values and releases ownership once, including DSET bypass. Simulation
before admission, with synchronous or delayed Base callbacks, sends no native
request. First-pass invalid-output values and ao prepared OVAL are checked with
separate real GETs. lso DOL uses a long-string CHAR-array link: the 200-byte value
is exact, while Base reduces an oversized 300-byte source to the 255-byte buffer
capacity before DSET capture. This boundary is distinct from snmp3 rejecting
oversized native payloads.

The numeric case initializes 148 numeric record contexts in addition to the
12 baseline contexts. Every advertised GET pair first receives an exact zero
through an actual SET and GET, before tag-specific range/precision values.
Independent declared acceptance masks cover INT32/UINT32 boundaries, 2^24,
2^53, signed-64 boundaries, finite floating extrema, subnormals and signed zero.
Fixed agent OIDs provide Counter64 2^63/UINT64_MAX and opaque NaN/infinities;
these are initial-input checks rather than failure-after-success checks.
Rejected input preserves full prior value bytes and NORD, follows the actual
Base UDF rule and leaves the diagnostic native-success state unchanged.
Output cases capture the real Base-prepared value and use separate GETs to
verify wire state, including unchanged agent state after a rejected SET.
Agent SET-action counts distinguish accepted output/stimulus commands from
rejected outputs. The case covers IOC database access; CA qualification uses
the separate production runner below.

For separate ASan/UBSan products:

```bash
python3 tests/rewrite/build_r5_sanitizers.py --output work/r6-sanitizer-build
R6_PRODUCTS=work/r6-sanitizer-build/products
python3 tests/rewrite/test_records.py --case edges --products "$R6_PRODUCTS" --sanitizers --output work/r6-asan-edge
python3 tests/rewrite/test_records.py --case alarms --products "$R6_PRODUCTS" --sanitizers --output work/r6-asan-alarm
python3 tests/rewrite/test_records.py --case active --products "$R6_PRODUCTS" --sanitizers --output work/r6-asan-active
python3 tests/rewrite/test_records.py --case policy --products "$R6_PRODUCTS" --sanitizers --output work/r6-asan-policy
python3 tests/rewrite/test_records.py --case numeric --products "$R6_PRODUCTS" --sanitizers --output work/r6-asan-num
python3 tests/rewrite/test_records.py --case shutdown --products "$R6_PRODUCTS" --sanitizers --output work/r6-asan-stop
```

Run queued-shutdown with the same selected products in another new output
directory. New module, wire, worker and native code are instrumented;
Base/system/vendor dependencies are uninstrumented and leak checks are disabled.

Production CA access uses a separate runner:

```bash
python3 tests/rewrite/test_record_ca.py --output work/r6-ca
python3 tests/rewrite/test_record_ca.py --products "$R6_PRODUCTS" --sanitizers --output work/r6-asan-ca
```

This runner launches the actual snmp3Ioc Main and generated registrar with
record-ca.db, the unchanged Base CA server, actual worker/native products and
loopback agent. Private CA ports and a private repeater isolate the clients;
all child exit/reap and loaded-library identities are recorded. The sanitizer
builder includes a separate production snmp3Ioc for this path. Base caget/caput
and Base/system/vendor dependencies remain uninstrumented.

For each of the five active outputs the runner also writes a first and then a
latest value while the first SET response is held by the outer UDP proxy, once
with plain puts (RPRO) and once with put-callbacks that Base defers until the
pending request completes. It observes two SETs on the wire, the agent storing
the first and then the latest value, the record idle without alarm, and the
put-callback completing only after the second SET.

Actual decimal CA STRING access preserves 2^53+1 and INT64_MAX through
int64out SET/int64in GET; UINT64 waveform reads UINT64_MAX exactly. lso/lsi
VAL$ CHAR arrays qualify 0/39/40/200/255 data bytes and LEN, while ordinary
CA STRING exposes 39 data bytes. caput rejects an oversized 300-byte VAL$
write before put; the requested value and later native GET remain unchanged.
This differs from Base DOL reducing a long source before DSET entry in the
policy case. PINI input, Base FLNK and normal non-isolated exit also run.
Other reprocess routes (scan, PROC) and in-flight non-isolated shutdown require separate cases.

Real-path negative controls build defective copies of the production support
in private directories. They reuse the compiler arguments from an identified
sanitizer build, compile the unchanged shipped record test and use the same
worker/native/agent products. Each control requires its named assertion to fail
normally, and loader diagnostics must identify the defective library. A build
failure, timeout, forced kill or unrelated failure does not qualify a control.

```bash
BUILD_RECEIPT=work/r6-sanitizer-build/sanitizer-build.json
python3 tests/rewrite/test_record_controls.py --build-receipt "$BUILD_RECEIPT" --output work/r6-controls
```

The D7 controls alter one shipped support source (`Scheduler.cpp`; `DeviceSupport.cpp` or `Request.cpp` for the never-sent message, waveform BUSY and drain retry controls; `Runtime.cpp` for the report and restart controls) and run their named
component, qualification or record cell against the same build. Each control
directory keeps `cell/cell.stdout` and `cell/cell.stderr`; qualification and
record cells also keep their run output under `cell/run/`. A record cell whose
trial was killed at its child bound, or a stop-queued, stop-inflight, stop-enqueue-failed or stop-downstream cell
without its `stop_queued`, `stop_inflight`, `stop_enqueue_failed` or `stop_downstream` event, qualifies neither a reference nor a control:

```bash
D7="per-handle-bound early-release uncharged-successor binding-lookup-component binding-lookup-qualification"
D7="$D7 binding-lookup-record"
D7="$D7 queued-deadline-restart stop-one-generation storage-validation"
D7="$D7 grace-native-failure grace-all-outcomes never-sent-overcount behind-flag-always take-without-identity"
D7="$D7 grace-channel-failure grace-worker-failure grace-stopping"
D7="$D7 never-sent-message-always never-sent-message-absent"
D7="$D7 never-sent-message-any-outcome never-sent-message-not-reset report-never-sent-miscounted"
D7="$D7 queued-deadline-extended rebuild-reuses-scheduler stop-queued-not-selected waveform-busy-held drain-without-retry"
python3 tests/rewrite/test_record_controls.py --build-receipt "$BUILD_RECEIPT" --d7-controls $D7 --output work/r6-d7-controls
```

Each D7 control runs its named component, qualification or record case, which
must pass on the unmodified products and fail on the control. The controls fall
into these groups:

| Group | Controls |
| --- | --- |
| Scheduler admission and ownership | per-handle-bound, early-release, uncharged-successor, storage-validation, binding-lookup-component, binding-lookup-qualification, binding-lookup-record, stop-one-generation |
| Containment grace | grace-native-failure, grace-all-outcomes, grace-channel-failure, grace-worker-failure, grace-stopping |
| Counting and identity | never-sent-overcount, behind-flag-always, take-without-identity |
| Never-sent message | never-sent-message-always, never-sent-message-absent, never-sent-message-any-outcome, never-sent-message-not-reset |
| Queue report | report-never-sent-miscounted |
| Queued deadline | queued-deadline-restart, queued-deadline-extended |
| Restart | rebuild-reuses-scheduler |
| Stop in flight | stop-queued-not-selected, waveform-busy-held, drain-without-retry |

The shipped controls cover communication-alarm classification, integer
precision, text capacity, binary32 tie selection, ambient-rounding dependence,
callback retry and terminal release.
The stale-generation control remains pending. Current partial coverage and the complete
required T1-T14 matrix belong to the canonical
[work register](../../docs/milestone-5dff352.md); passing these cases does not
close R6 or advertise record support. Receipts retain source/product/library
hashes, request identities, alarms/UDF/publication state, actual child exits and
cleanup observations without credentials.
