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
The foundation has no SNMP DSETs or session commands. Full two-OS,
sanitizer and one-hour resource checks belong to later implementations.
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
