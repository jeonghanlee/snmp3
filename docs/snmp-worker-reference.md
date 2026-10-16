# Worker commands, limits and timers

The Base-only input/output controller (IOC) takes a small set of startup and operator commands, charges every request against storage limits, and runs the servicing loop and each worker on fixed timers. This page lists those values exactly.

## Scope

This page covers the IOC commands, the runtime report fields, the charge formulas, the storage ceilings, the timers, and the diagnostics that the verification cases produce.

**Out of scope:** the reasons behind these rules (see [Address worker supervision](snmp-worker-supervision.md)); the steps that run the verification (see [Verify the worker transport](snmp-worker-verification.md)); the startup configuration fields (see [Startup configuration](snmp-rewrite-config.md)).

Abbreviations on this page: PID is a process identifier, RSS is the resident set size, the physical memory of a process, and JSON is the JavaScript Object Notation.

## Startup commands of the IOC

Build from the checkout root with an existing EPICS Base 7.0.10 and the Net-SNMP development libraries. The file `configure/RELEASE.local` selects Base. The build installs no packages and does not change the selected Base.

```bash
make -j4
```

The startup file accepts `CONFIG_JSON`, `NATIVE_PROBE`, and `WORKER_PATH` through the IOC environment. Configuration must be complete schema-1 JSON, and secret files must be private files owned by the effective user. The [configuration reference](snmp-rewrite-config.md) describes both.

For a configured IOC, select the absolute worker path before the first binding, `iocBuild`, `iocInit`, or stop freezes Config. Repeating the identical path after the freeze is allowed. Changing it is rejected. An empty foundation Config needs no worker.

This IOC shell sequence uses prepared absolute paths:

```text
on error break
dbLoadDatabase("dbd/snmp3Ioc.dbd")
snmp3Ioc_registerRecordDeviceDriver(pdbbase)
snmp3Load("<config_json>", "<native_probe>")
snmp3WorkerPath("<worker_path>")
snmp3QueueLimit("<address>", 1024, 1048576)
iocInit
snmp3Report
snmp3ConfigReport
snmp3RuntimeReport
snmp3Stop
snmp3RuntimeReport
```

Replace `<config_json>`, `<native_probe>`, and `<worker_path>` with the absolute paths of existing files. Replace `<address>` with a numeric address that appears in that JSON. The load and the worker selection must succeed before `iocInit`. The `on error break` line propagates a command failure. The Base Continue policy can proceed after a rejected command, so it does not prove success.

## Operator commands of the IOC

| Command | Contract |
| --- | --- |
| `snmp3WorkerPath(absolutePath)` | Startup product selector; at most 4096 path bytes |
| `snmp3QueueLimit(address,count,bytes)` | Atomic live limits, before or after `iocInit` |
| `snmp3Stop` | Freezes Config, closes admission, contains workers, and joins once |

Count and bytes are unsigned decimal integers. Count ranges from 1 to 16384, and bytes range from 1 to 67108864. The command rejects signs, zero, overflow, unknown addresses, and out-of-range values, and it changes neither prior limits nor work.

A decrease keeps accepted work, including consumed terminals whose native ownership is live. A new admission must fit both prospective totals.

The module has no operator start, rate-reset, credential-replacement, or transport-proxy command. Different credentials need a new IOC process. Reusing an isolated Base database does not unfreeze Config.

## Report commands of the IOC

| Command | Output |
| --- | --- |
| `snmp3Report` | `owned-worker-transport; recordSupport=available`. This line identifies compiled record support. It does not show that the configuration is valid, that the lifecycle is ready, or that device communication succeeded. |
| `snmp3ConfigReport` | The revision, the freeze state, and the definition counts |
| `snmp3RuntimeReport` | The lifecycle, queue, admission, worker, and loop numbers described below |

State Running means the servicing owner is ready. It does not promise that a device is reachable. A worker reports `ready=1` only after the bootstrap and the product and library identity match.

## Runtime report lines

`snmp3RuntimeReport` prints these lines in this order. The `snmp3 queue:`, `snmp3 admit:`, and `snmp3 worker:` lines repeat once for each address. The queue and admit lines of each address print first, address by address. The worker lines follow.

| Line | Fields |
| --- | --- |
| `snmp3 runtime:` | `state` is Cold, Starting, Running, Stopping, Stopped, IncompleteStopped, or Failed. `admission` is 1 while the runtime accepts new requests. `activation` counts starts. `created`, `exited`, and `joined` count the servicing thread over all activations. |
| `snmp3 records:` | The counters and flags of the record contexts, listed in the next table |
| `snmp3 queue:` | Per address: `count`, `bytes`, the four classes `queued`, `active`, `undelivered`, and `retirementPending`, and the live limits `countLimit` and `byteLimit` against which `count` and `bytes` are charged. Three more numbers end the line: `behindRetirement`, `behindAdmitted`, and `behindNeverSent`. |
| `snmp3 admit:` | Per address, printed after its queue line: the admission statistics described below |
| `snmp3 worker:` | Per address: the address, the epoch, the PID, `ready`, `closing`, the active `batch`, `launches`, `reaps`, and `forced` signals |
| `snmp3 loop:` | `passes`, the number of servicing-loop passes since the process started |

The last three numbers of the queue line mean:

- `behindRetirement` is the current count of queued generations whose predecessor is consumed but not retired.
- `behindAdmitted` is the number of generations admitted behind a predecessor.
- `behindNeverSent` is the number of such generations that reached Deadline before dispatch.

The last two numbers only grow while the scheduler lives.

### Fields of the records line

| Field | Meaning |
| --- | --- |
| `contexts` | Attached records |
| `active` | Records with a request in progress |
| `pending` | Completions whose callback enqueue was refused and is offered again on a timer |
| `queued` | Callbacks waiting in the Base callback queue |
| `running` | Contexts whose callback is executing |
| `inert` | Callbacks that arrived after callback entry was closed; they leave their record untouched |
| `entered` | Callbacks that have entered record processing and not left it; it can be 1 while `running` is 0, because it falls only after the record lock is released |
| `enqueueFailures`, `completions` | Cumulative counts |
| `entryOpen` | 1 from start until the drain of a stop closes callback entry; 0 before the first start |
| `drainFailed` | 1 when the drain of a stop ended at its 2000 ms budget with work left; it stays 1 and refuses a later start |
| `detachAllowed` | 1 only after the AtShutdown hook has permitted detach following a stop; it stays 0 after `snmp3Stop` alone |

### Fields of the admit line

A burst is a run of admission calls, each less than 5 ms after the previous one. A call that adds several generations counts once. All values only grow while the scheduler lives. Times are in microseconds of the admission clock.

| Field | Meaning |
| --- | --- |
| `address` | The address of the line |
| `admissions` | Admission calls accepted for the address; a refused call is not counted |
| `bursts` | Runs of calls less than 5 ms apart, counting a run of one call |
| `burstSpanMaxUs` | The largest time from the first to the last call of one burst |
| `burstGapMaxUs` | The largest time between two consecutive calls inside one burst |

Requests that wait for the previous result (a chain of records) join one burst when the previous result arrives in less than 5 ms. On a fast agent, a chained database can therefore show a span of several results.

### Servicing-loop pass counter

One pass of the servicing loop steps the supervisor, visits the record requests, and then waits for the next wake. The counter never resets, and a restart of the runtime does not clear it. It stays 0 when the configuration has no endpoint. It is meaningful while `state=Running`.

To read the pass rate, read the counter twice a known time apart and divide the difference by the elapsed seconds. Use a window of at least 10 s with a live worker (`pid` not 0, `ready=1`, `closing=0`).

With a live worker and no work, the loop passes about 10 times per second. A measurement of 100 passes in 10 s is typical, and the regression bound is 130 in 10 s. The bound scales with the window.

A much higher rate means work or a busy loop. Two cases raise the rate legitimately:

- A refused callback enqueue raises it to about 100 passes per second while `pending` in `snmp3 records:` is above 0.
- A worker that is being contained or is exiting raises it too, because its exit poll is 10 ms.

A busy loop is a rate above 13 passes per second with `count` and `active` of `snmp3 queue:` at 0 on every address, `batch` of `snmp3 worker:` at 0, and `pending` of `snmp3 records:` at 0.

### Worker idle line

An address worker whose standard error is kept writes one line:

```text
worker_loop <reason> <iterations> <monotonic_us> <address> <epoch> <pid>
```

The reason is one of three values:

- `ready`, once the worker has queued its Ready frame.
- `idle`, every 10 s while the worker has no active batch.
- `exit`, at a normal or faulted exit. A signal that ends the process writes no line.

The `iterations` field counts the worker loop iterations since its start. The `monotonic_us` field is the monotonic clock of the host, comparable only on one host and boot. Address and epoch are 0 before Bootstrap is read, for example `worker_loop exit 1 4660666880910 0 0 473547`.

Two idle lines about 10 s apart belong to one idle period. The difference of their `iterations` is the iteration count in that time. A healthy idle worker shows 1 per 10 s, and the regression bound is 30. A longer gap means work interrupted the idle period.

### Qualification stderr file

Production redirects worker standard error to `/dev/null`, and only the test products keep it. A test product calls `Runtime::setQualification` with a `Qualification` whose `stderrPath` is an absolute file path. The call must come before the first record binding or the start, because the configuration freeze ends the right to change it.

The runtime checks the path when it starts, so a bad path or mode makes the start fail with state Failed rather than the call. The directory must be owned by the IOC user with mode 0700. The runtime creates the file with mode 0600. An existing file must already be a regular file of that user with mode 0600. The parent directory comes from the path, so a file directly under `/` fails.

Every worker of the activation appends to that one file. Lines of several addresses interleave, and the same stream also carries the `native_event` lines. A call with the value already set returns without an error, even after the freeze. Loading the configuration alone does not freeze. A configuration with no endpoint builds no supervisor, so the runtime does not check the path. A production IOC has no setting for this file.

The test products `RecordTest.cpp`, `RuntimeTest.cpp`, `StartupTest.cpp`, and `ShutdownTest.cpp` call `setQualification`. `QualificationTest.cpp` passes the same structure to the supervisor directly.

## Charge formulas for a request

For each binding, the charge uses these quantities:

- `O` is four times the object identifier (OID) arc count.
- `V` is 8 plus the maximum Value content bytes.
- `S` is `V` for a SET and zero for a GET.
- `R` is `4 + O + V`.

Content bytes depend on the value type. Integer, Counter64, and Float64 content is 8 bytes. Unsigned32, IP, and Float32 content is 4 bytes. Octets content is its capacity. ObjectId content is `4 + 4*capacity`. Exceptions are 0.

```text
Q = round64(4096 + 4*O + 4*S + 4*R + 144)
Record Q = Q + 512 + 4*(content + textBytes)
Batch payload = 8 + sum(48 + actual encoded SET Value bytes)
Result maximum = 8 + sum(48 + R)
```

Component handles keep the original Q. Record handles reserve additional space for simultaneous decode, conversion, and text staging. Their `content` is `4*capacity` for ObjectId and `capacity` otherwise. Their `textBytes` is `11*capacity` for ObjectId, 16 for IPv4, and `content` otherwise. This extra charge remains until consumption and native retirement both finish. It is a conservative module storage account, not an RSS measurement.

Record registration charges `sizeof(RecordContext)+128+link.capacity()+1` against the 268435456-byte registration bound, with an 8192-byte fixed-context ceiling. A SET registration also reserves `256+2*min(binding capacity,record capacity)` for the capture before admission. Base-owned lsi, lso, and waveform buffers are separate. The callback borrows scheduler storage and creates no accumulating second result queue. The servicing loop visits at most 128 contexts per iteration.

Each handle can hold two charged generations. The count and byte limits of an address must therefore cover two generations of each handle that can hold a successor. An admission that exceeds either limit is rejected and changes nothing. The count limit is at most 16384, so at most 8192 handles of one address can hold a successor at the same time.

Examples at the default limit of 1048576 bytes:

- A nine-arc Integer GET charges 4608 bytes, so the limit admits 227 distinct handles.
- The ten-arc Integer GET fixture charges 4672 bytes.
- A 128-arc Octets GET with capacity 1048576 charges 4202688 bytes, and the SET charges 8397056 bytes. Increase the byte limit before admitting either.

Pending-byte limits are module-owned reservation bounds, not process RSS bounds. Vendor protocol data units (PDUs), allocator overhead, operating system socket buffers, shared Config snapshots, and dependencies are separate.

## Storage ceilings of the module

| Owned storage or policy | Ceiling |
| --- | --- |
| Pending default per address | 1024 commands; 1048576 charged bytes |
| Registered handles per address | 16384 |
| IOC configuration and bootstrap ledger | 268435456 bytes; the source Config snapshot is excluded |
| Bootstrap body and workspace, per side and address | 16777216 bytes each |
| Decoded immutable worker definitions | 33554432 bytes |
| Module native session and security ledger | 33554432 bytes; 1024 live sessions and 1024 pinned associations |
| Batch or Result payload | 2097152 bytes; at most 1024 members |
| Ready, Retired, and Fault payload | 65536, 32768, and 8 bytes |
| Fixed I/O scratch, per side and address | 131072 bytes |
| Fixed control storage, per side and address | 262144 bytes; at most eight frames |
| Native event storage | 4096 slots; at most 256 bytes per slot |
| IOC diagnostic storage | 1048576 bytes |

The module counts dropped diagnostic events numerically, and a run fails qualification when it loses required observations. Data-frame copies are charged to generations. Checked arithmetic, lengths, tags, and capacity precede allocation and publication, and a rejection preserves reservations. Counter exhaustion rejects newly affected work without wrapping identities. The storage guarantees cover the requested module storage, not total allocator or native heap usage.

## Timers and fairness

| Timer or fairness rule | Value |
| --- | --- |
| Servicing loop wait | Until a worker channel is readable, a channel with unsent bytes is writable, a signal arrives, or the earliest timer is due. Signals are an admission, a stop request, a live queue-limit change, and a terminal released after the stop has stopped accepting work. With a live worker, the exit poll is never more than 100 ms away, so an idle loop passes about 10 times per second. Without a worker process, the loop wakes on the restart backoff, a request deadline, or the stop bound. It has no timed wake only when the configuration has no endpoint (then `passes` stays 0). A refused callback enqueue limits the wait to 10 ms. |
| Exit poll of a contained or exiting worker | 10 ms |
| Exit poll of a live worker | 100 ms; the channel hang-up normally reveals a worker exit, and the poll is the fallback |
| Worker wait | Until its channel or a library socket is ready, a library timer is due, or its earliest own duty is due: Ready limit, partial frame, batch deadline, or idle count line |
| Hold of a declared group at the queue front | Released when the group is whole, 0.5 ms after the last admission of a member (quiet gap), or 2 ms after the admission of the front member (hold bound); a timer only for a ready, idle worker that is not stopping |
| Retry of a refused callback enqueue | 10 ms |
| Record drain wait | Ended at once by a completion, an inert callback (one that arrives after callback entry is closed), or a callback exit. Otherwise 10 ms while a refused enqueue is retried and 50 ms without one. The budget is 2000 ms. |
| Per address, direction, and iteration | 65536 bytes and 64 complete frames; round-robin |
| Partial non-bootstrap frame | 1000 ms from the first byte; the request deadline can be earlier |
| Bootstrap and Ready | 5000 ms from spawn; admission expiry stays independent |
| Stop graceful close, TERM, and KILL-reap observation | +1000, +1500, and +2000 ms from one origin |
| Retired grace after deadline | 1000 ms for a member already selected Complete or NativeFailure |
| Restart backoff | 250, 500, 1000, 2000, and 4000 ms, then 4000 ms; a matched Retired frame restarts the series |
| Restart rate | Four attempts per address in the trailing 60000 ms, including the initial attempt |
| Global spawn | At most one attempt per loop; round-robin over addresses |

When only the record drain retries, a stop returns within 5 ms of the last record completion. While the loop runs, a stop returns within 50 ms. The `stop-enqueue-failed` case measures the first value as `last_completion_to_return_us` in its check `drain-returns-on-completion-wake`.

## Where the waits are built

The code builds the waits in three places:

- The servicing loop waits in `Runtime::pause` (`Runtime.cpp`). It waits until `Supervisor::nextWake` is due, a worker channel descriptor is ready, or the `Wake` descriptor is ready. `nextWake` is the earlier of `nextTimer`, which holds the event timers, and `nextPoll`, the exit poll of each live worker.
- The address worker waits in `Worker::service` (`native/Worker.cpp`) until its channel or a library socket is ready, or `AddressWorker::nextTimer()` (`native/WorkerMain.cpp`) is due. Before its native session exists, it waits with a plain `poll()` on the channel and the same timer.
- The record drain waits on a condition variable in `Requests::drain` (`Request.cpp`). The timer table above lists its durations.

A duty that is not part of the matching `nextTimer` is acted on only at the next wake. In the supervisor, that wake is the 100 ms poll when a worker is alive; without a worker, it is a signal or a channel event. In the worker, it is a channel or socket event, because the worker has no poll of its own.

An event that must wake the loop signals `Wake`. An admission, a stop, and a queue-limit change do. The scheduler also calls the notifier when a terminal is released after the stop has lowered the accepting flag of the scheduler. The same stop first sets `admission=0` in the report and lowers the flag a moment later. The release lets the stop settle, and the loop evaluates that once per pass.

A change to the duties of the loop therefore changes three things: its `nextTimer`, the timer table above, and the `wake-loop` case of `tests/rewrite/test_records.py` (bounds of 130 loop passes and 30 worker iterations per 10 s). A matching control belongs in the Wake And Timer Controls section of `tests/rewrite/README.md`.

## Diagnostics of the verification cases

### The wake-loop case

The `wake-loop` case reads two idle lines of one worker 9.5 to 10.5 s apart. The first idle line comes 10 s after Ready and the second 20 s after, so the case observes for at least 20 s after Ready and waits 22 s. It checks `worker-idle-iterations-within-bound` in the `workers.stderr` file of its output directory.

### The stop-queued case

A record cell that stops the runtime behind a Base reprocess must first observe that reprocess. The Base function `recGblFwdLink` clears RPRO and hands the record to `scanOnce`. Between the completion of the predecessor and the reprocess, the record shows neither PACT nor RPRO.

The `stop-queued` cell therefore counts FLNK of `Records_QueueCompleted` from before the predecessor completes. It waits up to 3 s for one of two results:

- A queued successor holds PACT (`wait_exit` is `queued`).
- The counter has risen by 2, so a rejected reprocess has completed its own FLNK (`wait_exit` is `rejected`).

Otherwise the cell records `timeout`.

A run writes its `stop_queued` event to `record-observations.json` and its trial output to `records.stderr` in the output directory of the run. Under a D7 control output, these files are in `<control>/cell/run/` and `reference-record-stop-queued/run/`. The event carries `wait_exit` and the record state when the wait ended: `wait_rpro`, `wait_pact`, `wait_generation`, and `wait_flnk_delta`. A passing run shows `queued`, 0, 1, 2, and 1. The `pact` and `generation` fields are read after the stop has drained the record, so they do not describe the wait.

A run is inconclusive in either of two cases:

- Its event shows `timeout`.
- It has no event, and its `records.stderr` has a `record test failed after N checks` line. That trial stopped before the wait, and the line names the failed step.

For such a run, `test_records.py` writes the events, skips the product checks, and reports `aborted`. The D7 controls count it neither as a passing reference nor as a detected defect. Rerun it. If a timeout recurs, the `wait_` fields show which of three states applies:

- The reprocess was never scheduled (RPRO is set).
- The reprocess is pending (neither PACT nor RPRO).
- The record held PACT without a new generation.

A run without the event and without the failure line ended without its failure report, usually inside the stop under test. The files `records.stderr` and `records.receipt.json` show where it ended. The product checks of that run are judged and fail. A trial that exceeds the 180 s child bound is killed, shows `forced_cleanup` in `records.receipt.json`, and is judged the same way. The D7 controls count neither kind of run as a detected defect.

### Receipts of the verification runs

Receipts keep the sources, headers, products, and libraries; the argument vector without secrets; the monotonic identity and events; the terminal consumption; the native observations; the file descriptor and PID cleanup; and every child wait. Generated secret sentinels must be absent from diagnostics. A PASS qualifies the identified invocation only. Full completion also requires current foundation, configuration, lifecycle, and native regressions and full reviews.
