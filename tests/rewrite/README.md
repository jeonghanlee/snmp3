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
python3 tests/rewrite/test_records.py --case live-detach --output work/r6-live-detach
python3 tests/rewrite/test_records.py --case repeat-detach --output work/r6-repeat-detach
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
| live-detach | record-stop.db and actual outer UDP drop-all, two processes; an input and an output record are replaced through Base's own link put while their requests are in flight, after they complete, after the operator stop, and, in the second process, during the record drain of a stop whose completions the full Base callback queue refuses: Base refuses every attempt, each record keeps its context and binding, and each in-flight request completes once with a communication alarm |
| repeat-detach | record-stop.db ai/ao plus the baseline databases; idle isolated shutdown, first detach at AfterCloseLinks, two real INP and two real OUT puts, retained contexts at BeforeFree and zero contexts after testdbCleanup; every refusal preserves the original full INST_IO text/type and dset with null dpvt/context record pointers |

### Repeated Detach

The driver captures the initialized ai/ao attachment, complete INST_IO text,
link type, dset and nonzero global context count under the applicable locks.
After Base closes links, `repeat_detach` records the first detach before any
put. Base discards the shutdown del_record return; the test checks its effects,
not a fabricated first-call status. The driver then calls real `dbPutField`
in this order: INP 1, OUT 1, INP 2, OUT 2. Each replacement has valid snmp3
syntax and differs from the original. Every immediate snapshot is compared
with the pre-shutdown original, including the full string beyond 39 bytes.

Required events, in order, are one `repeat_detach_baseline`, one
`repeat_detach`, four uniquely identified `repeat_detach_attempt` observations,
one `repeat_detach_before_free` and one `repeat_detach_cleanup`. Missing,
duplicate or malformed observations fail. An unsafe attachment or changed
context count suppresses saved-context access and subsequent puts; skipped
puts fail the four-attempt precondition. The hook records failures without
throwing through Base or repairing product state. The later BeforeFree hook
runs after the module hook and reads only the locked Requests count. Cleanup
checks use value snapshots and never access saved record/context pointers.

The runner names first-detach checks `repeat-detach-first-{r}-{property}` and
post-put checks `repeat-detach-{r}-{n}-{property}`, where `r` is `ai` or `ao`
and `n` is `1` or `2`. Properties are `link-type`, `link`, `dset`, `dpvt-null`,
`context-record-null` and `contexts-retained`; post-put checks also include
`refused` for the exact Base `S_dev_badInpType` status. The separate checks
`repeat-detach-contexts-retained-before-free`,
`repeat-detach-cleanup-contexts-zero` and `repeat-detach-cleanup-stopped`
require retained storage until queue destruction, then zero contexts and
Runtime Stopped after actual isolated cleanup. Event order, initial fixture
validity and four executed calls are scenario preconditions, separately named
`repeat-detach-events`, `repeat-detach-fixture-ready` and
`repeat-detach-four-attempts`. Existing exit, child cleanup, secret-sentinel,
dependency and sanitizer checks remain active.

This case qualifies idle isolated ai/ao shutdown. It does not qualify
in-flight non-isolated retention, startup failure, another DTYP/support,
every record kind individually, or production CA access during shutdown.
An error return alone does not prove preserved state: if del_record wrongly
succeeds, Base can replace the link and clear dset when add_record refuses,
while returning the same error code. No product policy is changed by this test.

Build both ordinary targets before the sanitizer build consumes the generated
registrar. Run from the repository root with unused output directories:

```bash
RD_RUN=work/repeat-verification
RD_BUILD=work/repeat-sanitizers
RD_DRIVER=tests/rewrite/test_records.py
make -j2
make -C tests/rewrite -j2
python3 -B tests/rewrite/build_r5_sanitizers.py --output "$RD_BUILD"
RD_PRODUCTS="$RD_BUILD/products"
python3 -B "$RD_DRIVER" --case repeat-detach --output "$RD_RUN-ordinary"
python3 -B "$RD_DRIVER" --case repeat-detach --products "$RD_PRODUCTS" --sanitizers --output "$RD_RUN-asan"
python3 -B "$RD_DRIVER" --case live-detach --output "$RD_RUN-live"
python3 -B "$RD_DRIVER" --case live-detach --products "$RD_PRODUCTS" --sanitizers --output "$RD_RUN-live-asan"
python3 -B "$RD_DRIVER" --case rebuild --output "$RD_RUN-rebuild"
python3 -B "$RD_DRIVER" --case rebuild --products "$RD_PRODUCTS" --sanitizers --output "$RD_RUN-rebuild-asan"
```

Retain ordinary build stdout/stderr and exit codes as well as executable
hashes. The sanitizer builder retains its compilation receipts; each runner
retains source/product/loaded-library hashes, raw events and child receipts.
Module/test products are instrumented; Base/system/vendor dependencies are
uninstrumented and leak detection is disabled.

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

The D7 controls alter one shipped support source (`Scheduler.cpp`; `DeviceSupport.cpp` or `Request.cpp` for the never-sent message, waveform BUSY, drain retry and detach controls; `Runtime.cpp` for the report, restart and operator-stop detach controls) and run their named
component, qualification or record cell against the same build. Each control
directory keeps `cell/cell.stdout` and `cell/cell.stderr`; qualification and
record cells also keep their run output under `cell/run/`. A record cell whose
trial was killed at its child bound, or a stop-queued, stop-inflight, stop-enqueue-failed, stop-downstream or live-detach
cell without its `stop_queued`, `stop_inflight`, `stop_enqueue_failed`, `stop_downstream` or `live_detach` event, qualifies neither a reference nor a control:

```bash
D7="per-handle-bound early-release uncharged-successor binding-lookup-component binding-lookup-qualification"
D7="$D7 binding-lookup-record"
D7="$D7 queued-deadline-restart stop-one-generation storage-validation"
D7="$D7 grace-native-failure grace-all-outcomes never-sent-overcount behind-flag-always take-without-identity"
D7="$D7 grace-channel-failure grace-worker-failure grace-stopping"
D7="$D7 never-sent-message-always never-sent-message-absent"
D7="$D7 never-sent-message-any-outcome never-sent-message-not-reset report-never-sent-miscounted"
D7="$D7 queued-deadline-extended rebuild-reuses-scheduler stop-queued-not-selected waveform-busy-held drain-without-retry detach-always-allowed"
D7="$D7 detach-allowed-when-idle stop-permits-detach detach-allowed-while-pending refused-detach-raises-alarm refused-detach-perturbs-active-handle"
D7="$D7 entry-reported-open-after-stop detach-reported-allowed-after-stop drain-always-failed refused-detach-toggles-active-handle refused-detach-toggles-idle-handle refused-detach-perturbs-stopped-handle refused-detach-perturbs-pending-handle completion-counted-twice"
D7="$D7 retried-completion-leaves-record-active refusal-releases-record refusal-releases-record-with-alarm"
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
| Live detach | detach-always-allowed, detach-allowed-when-idle, detach-allowed-while-pending, stop-permits-detach, refused-detach-raises-alarm, refused-detach-perturbs-active-handle, entry-reported-open-after-stop, detach-reported-allowed-after-stop, drain-always-failed, refused-detach-toggles-active-handle, refused-detach-toggles-idle-handle, refused-detach-perturbs-stopped-handle, refused-detach-perturbs-pending-handle, completion-counted-twice, retried-completion-leaves-record-active, refusal-releases-record, refusal-releases-record-with-alarm |

The shipped controls cover communication-alarm classification, integer
precision, text capacity, binary32 tie selection, ambient-rounding dependence,
callback retry and terminal release.
The stale-generation control remains pending. Current partial coverage and the complete
required T1-T14 matrix belong to the canonical
[work register](../../docs/milestone-5dff352.md); passing these cases does not
close R6 or advertise record support. Receipts retain source/product/library
hashes, request identities, alarms/UDF/publication state, actual child exits and
cleanup observations without credentials.

### Repeated Detach Controls

These 45 controls run the shipped repeat-detach driver and fixtures against
compiled defective support copies. All undeclared product spans remain intact.
Here `r` expands to `ai` and `ao`, and `n` expands to `1` and `2`; every
combination is a separate named control. The normal source tree is not edited.

| Control or family | Count | Targeted property |
| --- | --- | --- |
| `repeat-first-{r}-link`, `repeat-first-{r}-link-type`, `repeat-first-{r}-dset` | 6 | First detach preserves full text, link type and dset independently. |
| `repeat-first-{r}-dpvt-null`, `repeat-first-{r}-context-record-null` | 4 | First detach clears each pointer independently. |
| `repeat-{r}-{n}-refused` | 4 | Each call returns the exact refusal code; another nonzero code fails. |
| `repeat-{r}-{n}-link`, `repeat-{r}-{n}-link-type`, `repeat-{r}-{n}-dset` | 12 | Each call preserves each original field independently. |
| `repeat-{r}-{n}-dpvt-null`, `repeat-{r}-{n}-context-record-null` | 8 | Each call leaves each pointer null independently. |
| `repeat-{r}-{n}-contexts-retained` | 4 | Context storage remains present immediately after each call. |
| `repeat-{r}-empty-accepted` | 2 | A zero detach return with empty dpvt must fail the first post-put link check, even if add_record returns the usual error. |
| `repeat-release-after-close` | 1 | Both first-detach context-count checks fail after premature release. |
| `repeat-release-after-stop-callback` | 1 | The mandatory BeforeFree count check fails if callback join releases storage. |
| `repeat-release-before-free` | 1 | The same check fails if the module BeforeFree hook releases storage before Base queue destruction. |
| `repeat-cleanup-retains-contexts` | 1 | Actual cleanup must leave zero contexts. |
| `repeat-cleanup-not-stopped` | 1 | Actual cleanup must leave Runtime Stopped. |

Every property-specific family targets its corresponding runner name by
replacing the leading `repeat-` with `repeat-detach-`. The three release
controls target `repeat-detach-first-ai-contexts-retained` (AfterCloseLinks)
or `repeat-detach-contexts-retained-before-free` (the later two); both
first-detach counts read the same global storage before any put.

Context-record faults deliberately leave a non-null record pointer. Their
mutation also removes the `queuesDestroyed` assertion on that pointer so the
real cleanup can finish and the named observation can be judged. The repeat
variants therefore declare both DeviceSupport.cpp and Request.cpp edits;
first-detach variants use Request.cpp only. No test repairs the pointer, no
cleanup span is replaced, and no crash counts as detection. A link-type fault
also makes the typed INST_IO text unavailable; the separate text-only control
establishes independent string discrimination. A safety skip can cause later
precondition failures, but only the specified observed property failure
qualifies its control.

The positive reference must exit successfully with the exact named check
present once and passing. The defective run must emit the lifecycle event,
fail that named check, exit its record driver normally and finish without
runner abort, timeout, forced cleanup, sanitizer diagnostics or secret sentinel.
Forced cleanup is aggregated across all child receipts, including the external
agent and UDP proxy. Their `normal-stop` checks must also be present and pass;
an abnormal external exit fails qualification even without forced cleanup.
Its actual loader receipt must identify the defective library and its hash.
All required event counts/order must still pass. These stricter reference and
control conditions are scoped to repeat-detach; other D7 cases keep their
existing rules.

Use the fresh sanitizer receipt above and an unused control output directory.
The name list is read from the shipped control definitions; each control has
its own product and run directory. A single named control can replace
`$RD_CONTROLS` in the last command.

```bash
RD_RECEIPT="$RD_BUILD/sanitizer-build.json"
RD_CONTROL_DRIVER=tests/rewrite/test_record_controls.py
RD_LIST='import test_record_controls as c; print(" ".join(c.REPEAT_CONTROLS))'
RD_CONTROLS=$(PYTHONPATH=tests/rewrite python3 -B -c "$RD_LIST")
python3 -B "$RD_CONTROL_DRIVER" --build-receipt "$RD_RECEIPT" --d7-controls $RD_CONTROLS --output "$RD_RUN-controls"
```

### Non-isolated shutdown with outstanding records

`test_record_shutdown.py` runs two fresh-process cases with `record-shutdown.db`:
passive low-priority ai Integer GET and ao OpaqueFloat SET, 60000 ms record
deadlines, initial values 71 and 19.5, and a shared calc FLNK counter.
`--case inflight|retained|all` selects the case; the default is `all`.

The `inflight` case runs the production `snmp3Ioc`, its generated registrar and
unchanged Main. Actual CA puts admit both records while an outer UDP proxy
forwards requests but drops replies. Before shell `exit`, both PACT fields must
be true, Runtime must have two active contexts and zero completions, an actual
application request must have reached the proxy, and neither the record nor
native timeout may have elapsed. The final fallback report must be Stopped with
closed admission/entry, two retained inactive contexts, exactly two completions,
no callback activity and successful drain. The worker identified before exit by
PID/start time must be reaped. This case does not read private record pointers
or per-record alarm fields after server shutdown.

The `retained` case runs `snmp3ShutdownTest`, built from the same production
Main plus an observer and a generated test registrar. Its explicit setup command
registers the observer after the production hook. It uses ordinary `iocInit`
and shell `exit`, with no isolated test initialization or cleanup. A real
external callback holds the single low-priority Base consumer while actual CA
requests traverse the pass-through proxy, worker/native path and NativeAgent.
Two selected successful native results must have real Result/Retired events
before exit; both callbacks remain queued and both generations remain charged
and unconsumed. `Scheduler::settled()` is not a readiness condition here.

Real Runtime drain expires. AtShutdown observes closed entry and detach
permission; AfterCloseLinks observes null record dpvt and context record
pointers, then releases only the external hold. AfterStopCallback observes two
Inert callbacks, retained contexts/results/reservations, zero completions and
unchanged record values/publication flags/FLNK. The callback queue system still
exists after its threads join. AfterShutdown preserves that state, reports
IncompleteStopped, and a real Runtime start attempt must fail without a new
activation/thread/worker. The later fallback report is checked separately.
There must be no BeforeFree phase.

`retained-terminal-preserved` requires both contexts to retain a non-null,
sent, identity-matched successful terminal at queued, AtShutdown,
AfterCloseLinks, AfterStopCallback, AfterShutdown and restart. Both pointer
checks require null associations from AfterCloseLinks through restart; the
Inert count must remain two from AfterStopCallback through restart.

Every required observer phase (`prepared`, `queued`, `AtShutdown`,
`AfterCloseLinks`, `AfterStopCallback`, `AfterShutdown`, `restart`) occurs once,
in order, with complete typed fields and monotonic timestamps. Missing,
duplicate or malformed observations fail. Inventory is checked before reading
saved context pointers. A watchdog can release the external hold for cleanup,
but any watchdog, abnormal child exit, forced cleanup, sanitizer diagnostic,
credential sentinel, missing receipt or runner abort fails the case. IOC, CA
clients, private repeater, agent and proxy have individual exit/reaping receipts;
`results.json` aggregates them. Loaded-library and input/product hashes accompany
raw stdout/stderr and per-case observations.

Build root and tests before generating the separate instrumented products.
Every output directory below must be unused; use a fresh prefix for another
execution. Run from the repository root. The required matrix is three ordinary
trials and one instrumented trial, both cases passing in every trial, followed
by isolated queued-shutdown and production CA regressions on both product sets.

```bash
NS_BUILD=work/nonisolated-san
NS_RUN=work/nonisolated
NS_DRIVER=tests/rewrite/test_record_shutdown.py
make -j2
make -C tests/rewrite -j2
python3 tests/rewrite/build_r5_sanitizers.py --output "$NS_BUILD"
NS_PRODUCTS="$PWD/$NS_BUILD/products"
python3 "$NS_DRIVER" --output "$NS_RUN-1"
python3 "$NS_DRIVER" --output "$NS_RUN-2"
python3 "$NS_DRIVER" --output "$NS_RUN-3"
python3 "$NS_DRIVER" --products "$NS_PRODUCTS" --sanitizers --output "$NS_RUN-asan"
NS_ISOLATED=tests/rewrite/test_records.py
python3 "$NS_ISOLATED" --case queued-shutdown --output "$NS_RUN-reg-isolated"
python3 "$NS_ISOLATED" --case queued-shutdown --products "$NS_PRODUCTS" --sanitizers --output "$NS_RUN-reg-iso-asan"
NS_CA=tests/rewrite/test_record_ca.py
python3 "$NS_CA" --output "$NS_RUN-reg-ca"
python3 "$NS_CA" --products "$NS_PRODUCTS" --sanitizers --output "$NS_RUN-reg-ca-asan"
```

The separate `--shutdown-controls` group uses all six controls when given
without names. Each compiles a declared defective support copy and the unchanged
companion sources using the fresh sanitizer receipt. The receipt's source and
product hashes must still match. A shared positive reference must pass the whole
case and contain exactly one passing occurrence of each named check. A defective
copy qualifies only when its named check occurs once and fails, the intended
library hash is observed in the IOC, all phases complete, and every child cleans
up normally. A crash, timeout or sanitizer termination is not detection.
Other control groups retain their existing behavior.

```bash
NS_RECEIPT="$NS_BUILD/sanitizer-build.json"
NS_CONTROLS=tests/rewrite/test_record_controls.py
python3 "$NS_CONTROLS" --build-receipt "$NS_RECEIPT" --shutdown-controls --output "$NS_RUN-controls"
```

| Defective support control | Independently targeted check |
| --- | --- |
| nonisolated-release-after-join | retained-contexts-after-callback-join |
| nonisolated-release-after-shutdown | retained-contexts-after-shutdown |
| nonisolated-detach-dpvt | retained-record-dpvt-detached |
| nonisolated-detach-record | retained-context-record-detached |
| nonisolated-late-entry | retained-late-callbacks-inert |
| nonisolated-false-drain | retained-drain-expiry-recorded |

The following term map distinguishes targeted discrimination from observed
preconditions and coverage limits. Execution outcomes belong in the canonical
M6 T12/T14 results; this table defines what the checks can establish.

| Checks/properties | Evidence class and limit |
| --- | --- |
| Six named checks above | Each has its own compiled control. Other terms failing in the same run are not credited as independent discrimination. |
| Phase inventory and field types | Required observation preconditions; missing/duplicate/malformed validation uses copies of retained real events and is parser validation, not additional integration execution. |
| Terminal presence, sent status, terminal identity match and successful outcome at each ownership phase; pointer and Inert preservation at later phases | Direct synchronized observations. Wrong-but-well-typed value changes in retained real observations validate the assertions only; they are not separately compiled product controls. |
| Bind readiness, actual IOC/CA requests, both PACT values, pending Runtime counts, native/wire GET and SET, native retirement, queued counts and deadlines | Actual fixture and scenario preconditions; no individual defective-product control for each conjunct. |
| Closed entry, zero entered processing, detach permission, exact generation identity, unchanged publication/native-success flags and values, zero FLNK/completions, retained count/bytes and unconsumed results | Direct synchronized observations across named phases. The six controls can also disturb some combinations; those combined failures do not independently qualify each term. |
| IncompleteStopped, restart rejection and unchanged activation/thread counters | Direct public Runtime call and pre/post snapshots; no separate restart control in this group. Native launch events and final worker counts are additional observations, not a replacement for the snapshots. |
| Live queue after callback join, no BeforeFree, hook order, separate fallback state | Actual Base lifecycle and queue-status observations; thread join is not a proof of queue destruction. No independent Base mutation is in scope. |
| Production Stopped exit, two inactive contexts, exactly two completions and closed entry | Actual production Main/registrar case and complete final Runtime report. The retained-companion controls do not independently discriminate these production-case terms. |
| Worker identity/reap, all child receipts, secret/sanitizer scans, IOC loaded libraries | Required execution/provenance preconditions. A control cannot qualify by failing cleanup or crashing; sanitizer silence alone does not establish pointer safety. |

Scope is this ai/ao pair on the selected local Base 7.0.10/Linux installation.
Retained allocations at non-isolated process exit are expected; no leak-free
claim is made. ASan/UBSan instruments fresh module, companion, worker/native and
test products; installed Base, CA tools and system/vendor dependencies remain
uninstrumented, and leak detection is disabled. The production inflight case
uses the production worker stderr policy; the retained companion additionally
captures worker stderr through the public qualification setting. Startup
failure, all eleven record kinds individually, long-string/maximum-capacity
buffers, isolated reuse and the remaining D13 cells require separate evidence.


## Record Startup Failure

`test_record_startup.py` runs five fresh processes on local Linux/Base 7.0.10.
The shipped `db/record-startup.db` contains valid ai Integer GET and ao
OpaqueFloat SET records, an ai referencing an unknown binding, and an ao
referencing the GET binding. The latter two are rejected individually during
device initialization. They do not imply that the whole IOC failed to start.
All records are passive without PINI processing, with distinguishable initial
values. No equipment endpoint is used; the runner starts the existing native
agent and a loopback UDP observer in each case.

| Case | Executable | Required behavior |
| --- | --- | --- |
| normal | snmp3StartupTest | Two usable contexts and two rejected records; valid GET/SET complete through the real worker/native/agent path; successful Main exit and non-isolated shutdown |
| production-break | snmp3Ioc | Real Runtime preflight failure with the Break script policy; exit 1 through unchanged Main |
| production-continue | snmp3Ioc | Successful commands run after the failed startup hook, but unchanged Main still exits 1 |
| failure-break | snmp3StartupTest | Same failure plus observations of initialization, processing attempts, repeated stop, pointer detachment, context retention and refused restart |
| failure-continue | snmp3StartupTest | The same internal observations under Continue, followed by Main exit 1 |

The companion links unchanged production Main with a generated registrar and
`StartupTest.cpp`. Its setup command registers the observer after production
registration and before iocInit. AfterInitDatabase captures the two valid
contexts before Runtime starts. Missing-worker cases select a nonexistent
absolute path inside their private evidence directory: real Supervisor
construction fails before Requests starts or a Runtime thread is created.
No internal function is replaced. This does not exercise thread-creation
failure after Requests starts.

The failure companion calls actual `dbProcess` on both valid records after
initial processing, then observes READ/INVALID and WRITE/INVALID, PACT clear,
unchanged values, no generation/admission identity, and no accepted reservation
or completion. Two actual Runtime stops follow. The normal companion calls
`dbProcess` through its exercise command and waits for real completion and
native retirement; GET publishes -123 and SET sends 19.5. Output success does
not mark an input publication or native-success flag.

AtShutdown is observed after production stop and detach permission. Real Base
link closure clears record dpvt and context record pointers; both are checked
again after callback join, AfterShutdown and the failed-case restart attempt.
Saved contexts are dereferenced only when inventory still contains two contexts
and there is no active/entered work. Non-isolated context storage remains
allocated. No BeforeFree or isolated cleanup helper participates. Base may set
PACT while closing links, so the PACT-clear refusal check belongs before
shutdown. Failed must survive repeated stop and the refused restart, with no
new activation or thread.

From the repository root, build the ordinary products before running:

```bash
make -j2
make -C tests/rewrite -j2
python3 tests/rewrite/test_record_startup.py --output work/startup-check
```

Every output directory must be new. `--case` selects one row above; its default
`all` runs all five. Fresh instrumented products include the startup companion:

```bash
python3 tests/rewrite/build_r5_sanitizers.py --output work/startup-sanitizers
products=work/startup-sanitizers/products
runner=tests/rewrite/test_record_startup.py
python3 "$runner" --products "$products" --sanitizers --output work/startup-sanitized-check
```

Recheck `edges` and `repeat-detach` with `test_records.py`, and both cases of
`test_record_shutdown.py`, on ordinary and instrumented products. Their commands
and evidence requirements are defined in their sections above. The startup
runner retains the real startup scripts, raw output, ordered typed events,
source/product/library hashes, private ports, child exit/reaping receipts,
checks and per-case results. Script EOF reaches Main naturally; no explicit
exit bypasses Main's Failed check. Expected IOC exits are 0 for normal and 1
for the four failed starts. Crashes, timeouts, forced cleanup, missing events,
observer errors, secret sentinels or sanitizer diagnostics fail the run.

### Startup Controls

The controls build separate defective support copies against a fresh sanitizer
build receipt. The tracked product sources remain unchanged. Each runs the
actual `failure-break` companion with the shipped fixture and normal Base
shutdown. Controls do not qualify from an abnormal exit or failed cleanup.

```bash
receipt=work/startup-sanitizers/sanitizer-build.json
runner=tests/rewrite/test_record_controls.py
python3 "$runner" --build-receipt "$receipt" --startup-controls --output work/startup-controls
```

| Control | Required failed check |
| --- | --- |
| startup-failed-state-lost | startup-failed-state-preserved |
| startup-ownerless-accepted | startup-no-accepted-work |
| startup-refusal-alarm-omitted | startup-admission-refused-with-alarm |
| startup-release-after-join | startup-storage-retained-after-join |
| startup-release-after-shutdown | startup-storage-retained-after-shutdown |
| startup-detach-dpvt | startup-record-pointers-cleared |
| startup-detach-record | startup-context-pointers-cleared |

Every reference must pass its full case and contain the named check exactly
once. A defective execution must reach the initial two-context and real failed
startup observations, complete the full ordered lifecycle, load the identified
support library, exit 1, clean up every child normally, and fail that exact
check. A lost Failed state may permit a later preflight attempt; this is an
additional failure, not grounds to skip observing the intended earlier failure.
The ownerless-acceptance control deliberately leaves records active without a
producer; the real drain then expires. No test repairs that state.

| Properties | Discrimination and limits |
| --- | --- |
| Seven named checks above | Each has its own compiled control. Additional checks failing in that run are combined observations, not independent controls for every term. |
| Initial values, two distinct handles, rejected records, phase order/types and real processing attempts | Fixture and observation preconditions; no separate defective-product control for each term. |
| Values, context counts, handles, generation identities, publication flags, reservations and later pointer preservation | Direct observations; no separate control for each field at each phase. |
| Refused restart and unchanged counters | Actual Runtime start call with before/after snapshots. The state-loss control can also disturb this result, but is not a dedicated restart control. |
| Production policy and exit behavior | Actual production Main/registrar processes under both policies. Companion controls do not independently discriminate every production assertion. |
| Normal GET/SET, wire bytes, invalid-record isolation and worker reap counters | Actual native-path evidence; no normal-case control group. The final worker report requires one launch/reap and zero current PID. |
| Child cleanup, library identity and sanitizer/secret scans | Mandatory qualification preconditions, not optional outcome checks. |

The scope is ai/ao initialization rejection and preflight failure with attached
contexts on this Base version. Retained process-exit allocations are expected;
no leak-free claim is made. New module/native/test products are instrumented;
installed Base and system/vendor libraries remain uninstrumented and leak
checks are disabled. Thread-creation failure, all eleven kinds separately,
long-string/max-capacity buffers, isolated reuse and other lifecycle cells
remain outside this case. Execution results belong in the canonical milestone.
