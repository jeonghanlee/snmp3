# Native SNMP transport

## Scope

This reference describes the separate `snmp3Native` library and its local
single-owner API. It implements v1/v2c/v3 GET/SET through Net-SNMP and returns
owned results. The verification driver loads the shipped schema-1 Config and
immutable bindings, then calls this library through real UDP transport.

This component reference covers local native behavior. Worker IPC, address
queues and IOC servicing are described in the [worker reference](snmp-worker-supervision.md).
Record conversion/FLNK, hardware, two-OS and sustained qualification remain
out of scope. Native calls execute in the separate worker, never in the IOC.

## Products and dependencies

| Product | Purpose | Native dependency |
| --- | --- | --- |
| `libsnmp3Native.so` | Session, PDU and event-service ownership | Net-SNMP |
| `snmp3NativeProbe` | Shared callable capability catalogue | Native library and Base Com |
| `snmp3NativeTest` | Actual Config/binding/adapter verification | Native library, snmp3 and Com |
| `snmp3NativeApiProbe` | Direct library ownership/security preflight | Net-SNMP |
| `snmp3NativeAgent` | Disposable protocol/typed-value fixture | Net-SNMP agent library |

Production native flags are confined to `snmp3App/native` and its tests.
The separate ABI InventoryTest uses native headers only as a test input. The IOC and
`libsnmp3` retain their Base-only dependency boundary. The selected installed
headers and libraries govern native behavior; catalogue agreement alone does
not prove binary identity. Test receipts additionally identify actual loaded
library paths and SHA256 values.

The public API is declared in [Native.h](../snmp3App/native/Native.h) and
[Worker.h](../snmp3App/native/Worker.h). [Native.cpp](../snmp3App/native/Native.cpp)
implements compatibility, discovery, PDU ownership and response checks;
[Worker.cpp](../snmp3App/native/Worker.cpp) implements deadline service,
completion delivery and native shutdown. Installed `session_api.h`,
`library/snmp_api.h`, `library/keytools.h`, `library/large_fd_set.h` and
`library/snmp_transport.h` supply the selected API declarations.

## Owner and session lifetime

Create one `Worker` on its servicing thread, once per process. Native
initialization precedes session creation. Owner operations and destruction
must use that same thread. Ordinary user/system configuration, persistent state
and native alarm signals are disabled. The owner closes sessions before native
shutdown. Closing one session does not shut down other sessions.

`Worker::open(profile, endpoint, capabilities)` returns a distinct `Native`
session. Identity includes numeric address/port, immutable profile, version,
security level, user, algorithms, security/context engine IDs and context name.
Secrets remain private owned profile data, outside reported identity.

`Native::submit(bindings, payload, completion)` admits one GET or SET batch and
returns its opaque token. Bindings must match the session endpoint and complete
immutable profile, and share one operation. GET has no payload; SET has one
typed value per binding. Empty and oversized batches are rejected before send.
Request storage owns binding handles, SET values and callback context. Caller
mutation of the submitted vectors does not alter the accepted operation.

`Worker::service(maximumWaitMs)` services native sockets and deadlines.
`takeEvents()` transfers accumulated diagnostic events to the caller. Events
include native operations, session/token identity, monotonic times and actual
socket FD. Callers should consume events. The default local Worker retains its component
behavior; the R5 worker selects bounded event insertion (4096 slots and a numeric
drop counter) and a separate native session/security ledger.

Completions receive owned `NativeResult` data after the native call stack
unwinds. A completion can submit another request or close a session. Keep the
Worker alive throughout API calls and completion delivery; destroying the Worker
from its own completion is unsupported. Callback exceptions are recorded as
`completion_exception` and do not escape through native frames.

`close()` first prevents dispatch, retains pending contexts during real native
close callbacks, and produces one Cancelled result for each remaining request.
Repeated close produces no second native close or terminal result. Sessions can
outlive owner shutdown as closed handles; submit then fails locally. Wrong-thread
owner destruction terminates the process.

## Capability and USM association

The probe and adapter use one callable capability implementation. Opening a
session compares native version and every canonical algorithm name/type/OID
against the immutable Config observation. A mismatch fails before discovery or
profile-session open; there is no algorithm fallback.

For v3, the process pins private security material by resolved security engine
ID and security user. Compatibility requires equal security level, applicable
algorithm metadata and owned secret bytes. Native-derived keys and existing
native user entries must also agree. Profile names, context identity and
transport settings do not change this compatibility rule. Compatible contexts
retain separate sessions.

For an explicit security engine ID, check the tuple before registering candidate
credentials. For an omitted ID, open a discovery-only native session with empty
user and no authentication/privacy. Resolve and close it, check compatibility,
then derive keys and open the credential-bearing session. Discovery dispatches
no application GET/SET and preserves existing shared user keys.

The first association remains pinned until process exit, including after its
last session closes. Conflicts return SecurityConflict without replacing native
users/keys or dispatching candidate application operations. Native timeliness
updates remain enabled. Different engines or users have independent tuples.
Runtime credential replacement requires a new process.

Native APIs derive master/localized keys and perform privacy-key extension.
No custom USM or crypto path is implemented. An omitted context engine ID
inherits the resolved security engine ID; explicit security and context IDs
remain independent. Responses must retain the declared engine/context identity.
Native timeliness REPORT/retry handling can recover a same-engine agent restart.
No application SET replay or production worker restart policy is supplied.

## Results and native representation

| Outcome | Meaning |
| --- | --- |
| Complete | Accepted response with owned positional values |
| Timeout | Native request exhausted its actual deadline/retries |
| OpenFailure | Local/native open failed; no application request admitted |
| SendFailure | Native send failed; caller-owned PDU released |
| SecurityFailure | Native security-error callback or real key-setup failure |
| ProtocolFailure | Response carries SNMP error status/index |
| SecurityConflict | Process-pinned USM material disagrees |
| CapabilityMismatch | Immutable catalogue disagrees with actual native APIs |
| ResponseRejected | Response identity, position, tag or storage invalid |
| Cancelled | Session/owner closed with the request pending |
| InvalidRequest | Batch, binding, payload or native representation invalid |

Open/admission errors use `NativeError`; an admitted operation produces one
terminal completion. Wrong credentials can produce a native timeout rather
than SecurityFailure. A particular key-derivation failure is qualified only
when an actual native call fails; an open failure does not prove it.

Error status and index remain separate from values. Response count, positional
OID and storage/type checks precede copying. Repeated requested OIDs remain
distinct positions. Missing, extra, reordered or inconsistent duplicate
varbinds are rejected. Exceptions retain NoSuchObject/NoSuchInstance/EndOfMibView
tags. Intermediate REPORT and RESEND observations are not successful data.

| Value | Native transport contract |
| --- | --- |
| Integer | Integer32 range, even though owned Value can store signed 64-bit |
| Unsigned32/Gauge32 | Same ASN wire tag; expected binding supplies the owned tag |
| Counter32/TimeTicks | Distinct native tags, unsigned 32-bit range |
| Counter64 | Exact high/low 32-bit native fields, owned unsigned 64-bit value |
| Octets | Owned bytes including NUL, bounded by binding capacity |
| OID | 2-128 arcs, 32-bit native subidentifiers and binding capacity |
| IPv4 | Exactly four native bytes |
| Opaque Float32/Float64 | Exact native storage width and retained floating bits |

On the verified Linux x86_64 native selection, `MAX_SUBID` is 4294967295.
The native OID parser additionally limits `firstArc * 40 + secondArc` to that
value. Transport rejects unsupported SET/binding OIDs before dispatch; Config's
broader owned arc representation does not bypass this native restriction.
`checkRanges=false` cannot bypass wire or storage safety. Native truncation is
reported as observed output; lost integer bits are never reconstructed.
EPICS field conversion and precision policies remain separate integration work.

## Event service and retry behavior

The owner initializes and separately zeros a dynamic large-FD set, merges
interests/deadlines from every active single-session handle, then calls the
native large-FD select/read APIs. It services every active session's native
timeout after socket activity or EINTR. Tests exercise successful traffic and
timeout with an actual socket FD above FD_SETSIZE, plus mixed deadlines while
another session continues receiving traffic.

Immutable timeout/retry settings govern native behavior. Net-SNMP alone owns
retransmission and discovery. Packet and callback timestamps/counts describe
actual retries; no application retry loop is added. Native discovery/open can
block; the test parent bounds child duration. Production process containment is described in the worker reference; native
open remains blocking within that separate process.

## Execute verification

Run from the checkout root on Linux x86_64 with Python 3, GNU make, a C++
compiler, the selected shared-library Base 7.0.10 and installed Net-SNMP
development/agent libraries. `configure/RELEASE.local` must already identify
that Base. No package or Base installation is performed.

Each evidence directory must be new. These commands build real products,
run direct native preflight, run the actual adapter matrix, and repeat that
matrix with separate ASan/UBSan products:

```bash
make -j2
python3 tests/rewrite/test_native.py --phase preflight --output work/r4-api-02
python3 tests/rewrite/test_native.py --phase adapter --output work/r4-native-02
python3 tests/rewrite/test_native.py --phase adapter --sanitizers --output work/r4-sanitized-02
```

The runners create mode-0700 evidence directories and mode-0600 generated
secret/config files. A separate real native agent listens only on owned
unprivileged IPv4/IPv6 loopback ports. SET changes disposable fixture storage.
The UDP fault process mutates messages actually exchanged at the outer network
boundary; it never substitutes an internal adapter function. Recorded packet
metadata excludes raw packets and credentials. Failed runs are retained.

PASS means all checks in that invocation passed. `results.json` identifies
sources, selected headers, products and loaded libraries; command receipts
include return/reap status, timing and forced-cleanup classification. Every
owned child is waited for. Secret sentinels and sanitizer diagnostics must be
absent from recorded output. Neither phase alone is full implementation
acceptance.

`--sanitizers` instruments the new native library, helper, driver and native
fixtures with ASan/UBSan. Base, Config support, system and vendor native
dependencies remain uninstrumented. Leak detection is disabled; this is access
and undefined-behavior verification, not leak/resource qualification. The real
FD-exhaustion open-failure case first opens/closes a native session and executes
an actual local admission rejection. It releases its held descriptors on failed
open before fixture unwinding. This case qualifies an initialized native process.

Run the complete foundation/lifecycle/configuration regression with the same
Base, assigning its existing absolute installation path to BASE_PATH:

```bash
BASE_PATH=/absolute/path/to/installed/base
python3 tests/rewrite/test_config.py --base "$BASE_PATH" --output work/r4-regressions-02
```

This rebuilds and runs the real IOC and Config paths. Privilege-dependent
wrong-owner secret cells can remain explicitly NOT RUN. Historical owner
receipts apply only to their original source/product hashes. Configuration
validation, report/help and startup behavior are defined in the
[startup reference](snmp-rewrite-config.md).
