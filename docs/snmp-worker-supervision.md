# Address worker transport and supervision

## Scope

The Base-only IOC accepts owned component requests, schedules each numeric
address, supervises a separate native worker and returns scheduler-owned terminal
results. Net-SNMP runs only in the native worker. This reference covers component
admission, IPC, accounting, failure containment and IOC lifecycle.

Record DSET, conversion and completion are implemented as a qualification
candidate; their complete matrix is pending. The
[record contract](snmp-rewrite-contract.md#record-qualification-candidate)
defines that boundary. The standard foundation longin fixture processes
constant 42; it is not an
SNMP record. Hardware acceptance, two-OS, TSan, leaks and sustained RSS checks
remain separate work. There is no IOC command that sends GET or SET.

## Products and data flow

| Product | Ownership |
| --- | --- |
| `libsnmp3.so` | Config, Runtime, Scheduler, Supervisor, Conversion, Requests and eleven DSETs; Base-only |
| `libsnmp3Wire.so` | Checked framing, typed values and SHA256; native-free |
| `snmp3Ioc` | Base shell, generated registration and checked exit |
| `snmp3NativeProbe` | Callable native catalogue; no device session |
| `snmp3Worker` | One address, native sessions, callbacks and owned responses |

Component caller -> immutable binding -> atomic scheduler admission -> FIFO
batch -> private stream IPC -> actual worker -> Native -> Net-SNMP -> owned
Result -> scheduler terminal consumer. The worker sends Retired after native
unwind and after destroying its request/result copies. No frame holds a record
pointer. The servicing thread calls no arbitrary completion consumer.

Canonical IPv4-mapped IPv6 and its IPv4 address share one worker. Other IPv6
addresses retain their complete address bytes. Endpoints keep their original
numeric transport address and port; profiles, engines and contexts retain
distinct native session identities within the address worker. At most 64
canonical addresses are allowed per activation.

## Startup and operator commands

Run from the checkout root with the selected existing Base 7.0.10 and Net-SNMP
development libraries. `configure/RELEASE.local` selects Base; this build does not
install packages or change the selected Base. Supply complete schema-1 JSON and
effective-user-owned private secret files as described in the
[configuration reference](snmp-rewrite-config.md).

```bash
make -j4
```

For a configured IOC, select the absolute worker path before the first binding,
iocBuild/iocInit, or stop freezes Config. Repeating the identical path is allowed
after freeze; changing it is rejected. An empty foundation Config needs no worker.
The shipped startup file accepts `CONFIG_JSON`, `NATIVE_PROBE` and `WORKER_PATH`
through the IOC environment. In an IOC shell, this sequence uses prepared
absolute paths:

```text
on error break
dbLoadDatabase("dbd/snmp3Ioc.dbd")
snmp3Ioc_registerRecordDeviceDriver(pdbbase)
snmp3Load("/absolute/config.json", "/absolute/snmp3NativeProbe")
snmp3WorkerPath("/absolute/snmp3Worker")
snmp3QueueLimit("127.0.0.1", 1024, 1048576)
iocInit
snmp3Report
snmp3ConfigReport
snmp3RuntimeReport
snmp3Stop
snmp3RuntimeReport
```

Replace the three absolute placeholders with existing selected files, and the
numeric address with one configured in that JSON. Load and worker selection must
succeed before iocInit. `on error break` propagates command failure; Base's
Continue policy can proceed after a rejected command and does not prove success.

`snmp3Report` reports `owned-worker-transport; recordSupport=unavailable`.
`snmp3ConfigReport` reports revision, freeze and definition counts.
`snmp3RuntimeReport` reports actual lifecycle/queue/worker numbers. Running
means the servicing owner is ready; it does not promise device reachability.
Worker `ready=1` requires matching bootstrap and product/library identity.

| Command | Contract |
| --- | --- |
| `snmp3WorkerPath(absolutePath)` | Startup product selector; at most 4096 path bytes |
| `snmp3QueueLimit(address,count,bytes)` | Atomic live limits, before or after iocInit |
| `snmp3Stop` | Freeze Config, close admission, contain workers and join once |

Count and bytes are unsigned decimal integers: count 1-16384, bytes 1-67108864.
Signs, zero, overflow, unknown addresses and out-of-range values are rejected
without changing prior limits or work. Decreases retain accepted work, including
consumed terminals whose native ownership remains live. New admission must fit
both prospective totals. There is no operator start, rate-reset, credential
replacement or transport-proxy command. Different credentials require a new IOC
process. Isolated Base database reuse does not unfreeze Config.

## Admission, FIFO and terminal ownership

Register each consumer's immutable binding separately. Alias handles for the
same definition have separate generation and terminal identities. A handle has
at most one unretired generation. An atomic call targets one address and captures
one monotonic origin and a positive integer budget of 1-600000 ms. Its uint64
deadline includes queueing, bootstrap, IPC, discovery and native service.

Dispatch at most one native batch per address. Take a contiguous compatible FIFO
prefix with the exact same absolute deadline, endpoint/profile/session and
operation. Respect maxVarbinds and both frame limits. Equal budgets with different
origins do not imply equal deadlines; no rounding, skipping or extension occurs.
Atomic admission publishes all members or none, but does not promise one SNMP
transaction. Each member has its own count, Q and positional owned result.

The first successfully transmitted Batch byte makes every batch member ambiguous
for recovery. Only zero-byte work survives worker replacement with its original
identity/deadline. Neither GET nor SET is automatically replayed after ambiguity.
Native retries remain Net-SNMP's behavior. SET completion does not establish
device readback truth.

The consumer borrows a terminal through `Scheduler::take` and completes its
unique consumption through `release`. Full count and Q remain until BOTH
consumption and actual request retirement. A matching Retired proves normal
native unwind and destroyed frame/request copies. Worker loss requires closed
channel buffers AND exact old PID wait/reap. Deadline alone proves neither.
Both orders, retirement before consumption and consumption before retirement,
preserve the charge until their joint transition. Stale/duplicate frames cannot
change current ownership or select a second terminal.

Reports classify `queued`, `active`, `undelivered` and `retirementPending`;
`count` and `bytes` include all of them. A borrowed terminal remains undelivered
until release. `retirementPending` means its terminal was consumed but its native
or channel ownership has not ended. Worker reports identify address, epoch, PID,
ready/closing, active batch, launches, reaps and forced signals.

## Charge and storage limits

For each binding, O is four times the OID arc count, V is 8 plus maximum Value
content bytes, S is V for SET and zero for GET, and R is 4 + O + V. Integer,
Counter64 and Float64 content is 8 bytes; unsigned32/IP/Float32 content is 4;
Octets content is capacity; ObjectId content is 4 + 4*capacity; exceptions are 0.

```text
Q = round64(4096 + 4*O + 4*S + 4*R + 144)
Record Q = Q + 512 + 4*(content + textBytes)
Batch payload = 8 + sum(48 + actual encoded SET Value bytes)
Result maximum = 8 + sum(48 + R)
```

Component handles retain the original Q. Record handles additionally reserve
simultaneous decode/conversion/text staging: content is 4*capacity for ObjectId
and capacity otherwise; textBytes is 11*capacity for ObjectId, 16 for IPv4 and
content otherwise. This extra charge remains until consumption and native
retirement both finish. It is conservative module storage accounting, not an
RSS measurement.

Record registration charges `sizeof(RecordContext)+128+link.capacity()+1`
against the 268435456-byte registration bound, with an 8192-byte fixed-context
ceiling. SET registration also reserves `256+2*min(binding capacity,record
capacity)` for pre-admission capture. Base-owned lsi/lso/waveform buffers are
separate. The callback borrows scheduler storage; it creates no accumulating
second result queue. Servicing visits at most 128 contexts per iteration.

A nine-arc Integer GET charges 4608 bytes, admitting 227 under the default
1048576-byte limit. The ten-arc Integer GET fixture charges 4672. A 128-arc,
capacity-1048576 Octets GET charges 4202688 and SET charges 8397056; increase
the byte limit before admitting either. Pending-byte limits are module-owned
reservation bounds, not process RSS bounds. Vendor PDUs, allocator overhead,
OS socket buffers, shared R3 snapshots and dependencies are separate.

| Owned storage or policy | Ceiling |
| --- | --- |
| Pending default/address | 1024 commands; 1048576 charged bytes |
| Registered handles/address | 16384 |
| IOC configuration/bootstrap ledger | 268435456 bytes; source R3 snapshot excluded |
| Bootstrap body/workspace per side/address | 16777216 bytes each |
| Decoded immutable worker definitions | 33554432 bytes |
| Module native session/security ledger | 33554432 bytes; 1024 live sessions and 1024 pinned associations |
| Batch or Result payload | 2097152 bytes; at most 1024 members |
| Ready / Retired / Fault payload | 65536 / 32768 / 8 bytes |
| Fixed I/O scratch/side/address | 131072 bytes |
| Fixed control storage/side/address | 262144 bytes; at most eight frames |
| Native event storage | 4096 slots; at most 256 bytes/slot |
| IOC diagnostic storage | 1048576 bytes |

Dropped diagnostic events are counted numerically and fail qualification when
required observations are lost. Data-frame copies are charged to generations.
Checked arithmetic, lengths, tags and capacity precede allocation/publication;
rejection preserves reservations. Counter exhaustion rejects new affected work
without wrapping identities. Storage guarantees cover requested module storage,
not total allocator/native heap usage.

## IPC, containment and lifecycle

Use a private CLOEXEC AF_UNIX SOCK_STREAM socketpair. Frames have an explicit
64-byte FSN5/version-1 big-endian header, checked kind/length/identity and
direction-local sequence. Partial/coalesced I/O is normal; malformed framing is
an address failure, with no resynchronization scan. Stale data is discarded
through bounded scratch. No pointer, native structure or platform long is sent.

Spawn argv contains only the selected executable and `--ipc-fd=3`; checked file
actions map that descriptor, close other descriptors and send production standard
streams to /dev/null. Environment contains PATH, LANG, LC_ALL and preflight-hashed
library directories only. No secret filename, bytes or inherited LD_PRELOAD are
passed. Bootstrap owns frozen definitions and credentials, never reopens their
files. Ready matches capabilities, executable SHA256 and loaded library SHA256
before application dispatch. SecurityConflict does not restart a healthy worker
to discard pinned USM material.

| Timer or fairness rule | Value |
| --- | --- |
| Idle wait | At most 10 ms and no later than due timer |
| Per address/direction/iteration | 65536 bytes and 64 complete frames; round-robin |
| Partial non-bootstrap frame | 1000 ms from first byte; request deadline can be earlier |
| Bootstrap/Ready | 5000 ms from spawn; admission expiry remains independent |
| Stop graceful close / TERM / KILL-reap observation | +1000 / +1500 / +2000 ms from one origin |
| Restart backoff | 250, 500, 1000, 2000, 4000 ms, then 4000 ms |
| Restart rate | Four attempts/address in trailing 60000 ms, including initial attempt |
| Global spawn | At most one attempt/loop; round-robin addresses |

Due deadlines are handled before optional I/O/spawn. Expiry selects one Deadline
per still-nonterminal member of the equal-deadline active batch and starts
containment. No next batch uses that slot before Retired or exact reap. Peer
addresses remain independently serviced. A new epoch follows actual old PID reap.
Normal native close and forced termination are separate observations. These
timers assume runnable OS threads; an unreaped process is retained as incomplete
ownership, never represented as successful cleanup.

AfterFinishDevSup freezes Config and starts the servicing owner before Base
initial processing. The IOC hook performs no synchronous native discovery.
AtShutdown closes admission/dispatch/recovery, selects Stopping for outstanding
nonterminal work, requests worker close and joins the service thread once before
Base stops callback services. Already selected terminals retain their outcome.

Runtime states are Cold, Starting, Running, Stopping, Stopped, IncompleteStopped
and Failed. IncompleteStopped retains unconsumed terminal or unreaped child
ownership after join. Later explicit stop/fallback/start preflight performs only
nonblocking reconciliation; start is rejected until retained ownership settles.
Then isolated activation can create a new scheduler and worker epoch. Failed
preflight/thread creation closes admission; unexpected service failure also
closes admission. Record stop adds a 2000 ms callback-drain attempt, followed by
entry-gate closure and a separate safety wait for entered callbacks. Failed
record drain stays failed after native reconciliation and refuses restart.
AtShutdown permits detach only after entered processing reaches zero; isolated
queue destruction, rather than callback join alone, permits context release.
Non-isolated shutdown retains still-referenced contexts. The complete record
lifecycle qualification remains pending; external Base processing can extend
the safety wait without a bounded total shutdown claim.

## Execute real verification

Each output directory must be new and private. These commands use shipped code,
real native loopback agents and faults only at outer OS/socket boundaries:

```bash
make -C tests/rewrite -j4
python3 tests/rewrite/test_r5_components.py --output work/r5-components
python3 tests/rewrite/test_supervisor.py --ioc --output work/r5-ioc
python3 tests/rewrite/test_qualification.py --output work/r5-qualification
python3 tests/rewrite/test_r5_operator.py --output work/r5-operator
python3 tests/rewrite/build_r5_sanitizers.py --output work/r5-sanitizer-build
```

Select its separate products for the instrumented executions:

```bash
R5_PRODUCTS="$PWD/work/r5-sanitizer-build/products"
python3 tests/rewrite/test_qualification.py --products "$R5_PRODUCTS" --sanitizers --output work/r5-san
python3 tests/rewrite/test_supervisor.py --products "$R5_PRODUCTS" --sanitizers --ioc --output work/r5-san-ioc
```

Both libraries and actual worker/owner/IOC test products are instrumented with
ASan/UBSan. Dependencies are uninstrumented and leak detection is disabled.
Private qualification stderr is retained and scanned; production /dev/null
cannot establish sanitizer success. Test-only outer IPC forwarding mutates real
frames while retaining the selected actual worker and full native path; Runtime
rejects transport overrides and exposes no production selector for them.

Receipts retain sources/headers/products/libraries, argv without secrets,
monotonic identity/events, terminal consumption, native observations, FD/PID
cleanup and every child wait. Generated secret sentinels must be absent from
diagnostics. PASS qualifies the identified invocation only; full completion also
requires current foundation/config/lifecycle/native regressions and full reviews.

The [verification sequence](../tests/rewrite/README.md) gives those regression
commands. The [supervision ADR](decisions/snmp-worker-supervision.md) records the
ownership decision. Historical R4 component evidence retains its original scope.
