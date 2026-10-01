# ADR: Address workers, exact-deadline FIFO and joint retirement

Date: 2026-10-01
Status: accepted for the verified R5 component and IOC supervision boundary
Source Session: standalone author work in `/home/jeonglee/gitsrc/snmp3`
Source Decisions: User-approved `work/plan-r5-20260930-02.md`, SHA256
`f4209d83308b3518c70506a1dd0785276461ade07689bb584463ec98f45bbc38`, and
`work/contract-r5-20260930-02.md`, SHA256
`3b3d2f1a846a8870d283bfbcf57bc6aa44ea6d755e2147ff1e36107cae83798b`.

## Context

Native discovery/open can block. Device slowness or native failure must not stall
the IOC or another address. Accepted requests own bounded data and must have
admission-based deadlines. Consumption can precede actual native unwind, while
native retirement can precede consumption; either condition alone is insufficient
to release storage. Missing SET acknowledgement does not establish no side effect.

## Decision

Keep IOC Runtime/Scheduler/Supervisor and the shared wire library native-free.
Use one explicitly selected actual worker per canonical numeric address and one
native batch active per worker. Preserve distinct immutable endpoint/profile/
engine/context sessions inside that worker. Bootstrap owns frozen bytes and
Ready checks catalogue, executable and loaded library identity before dispatch.

Use private bounded STREAM frames with checked 64-byte version-1 headers and
explicit typed bodies. Validate size and reservations before allocation. Keep
bounded partial I/O, frame lifetimes and per-address quotas; malformed input
fails its address rather than scanning for a guessed boundary.

Admit atomically against count and full Q, then dispatch the contiguous FIFO
prefix with exactly equal absolute deadlines, session and operation. Never round
deadlines. Retain one count/full Q per generation until unique terminal consumption
AND matching actual retirement or closed-channel/exact old PID reap. No early
reservation transfer or extra uncharged completion queue is allowed.

The first successfully transmitted Batch byte makes the entire batch ambiguous.
Only proven zero-byte requests can survive replacement with their original
identity/deadline. Do not replay ambiguous GET or SET. Net-SNMP retains ownership
of protocol retries and process-pinned USM compatibility.

Contain expired/failed workers locally, retain the dispatch slot through native
unwind or reap, and restart with a new epoch only after old PID wait. Apply the
fixed backoff/rate and address fairness rules. Request-level security conflicts
do not restart healthy workers to change pinned material.

Use AfterFinishDevSup readiness before initial Base processing and AtShutdown
admission/dispatch closure with one join before callback services stop. Retain
unsettled ownership in IncompleteStopped after join and reconcile nonblockingly
before reuse. Exact wire, charge, numeric policy and operator contracts are in
the [worker reference](../snmp-worker-supervision.md).

## Consequences

Slow/blocked native work can require forced process termination. A Deadline
terminal does not prove its request retired. Limits can temporarily be below
retained accepted totals after an operator decrease. Memory ceilings describe
module-owned requested storage, not native heap or total RSS. Config and pinned
credentials remain immutable for process lifetime; complete zeroization is not
guaranteed. Numeric diagnostics identify ownership without credential content.

Readiness proves servicing ownership rather than remote device availability.
R5 implements component transport; record DSET, conversions, callback completion
and FLNK remain R6. No SNMP PV or hardware qualification follows from the
constant-input Base record fixture.

## Alternatives Considered

SEQPACKET requires large values to fit atomic messages. STREAM with explicit
bounded framing supports partial writes without enlarging accepted reservations.
Parallel native batches per address would complicate FIFO/expiry/native unwind.
Batching different deadlines would require cancellation that the actual native
API does not provide. Releasing Q at consumption would leave live worker/native
copies uncharged. Replaying after missing acknowledgement can duplicate SET
side effects. These alternatives do not satisfy the selected ownership contract.

## Verification Or Enforcement

Production assertions, checked codecs and storage ledgers enforce the fixed
contract. Real component, actual IOC and Qualification products exercise the
shipped scheduler -> IPC -> actual worker -> Native -> real agent -> owned
consumer path. Negative IPC cases mutate actual frames at an external UNIX
socket boundary; positive responses and Ready are never internal substitutes.
OS signals and real UDP faults establish expiry, ambiguity, backoff, exact reap,
peer progress, concurrent stop and retained ownership.

Separate ASan/UBSan products instrument new R5 code, retain worker stderr and
identify every selected product/library. Dependencies remain uninstrumented,
leak detection is disabled, and missing observations block acceptance. Full
current R1-R4 regressions and third-person implementation/second-person operator
review qualify this boundary. The [verification sequence](../../tests/rewrite/README.md)
defines current runners and source/product/library identity checks. Historical receipts do not
qualify changed hashes; dependency instrumentation and record integration remain
outside this acceptance.

The [native ADR](snmp-native-transport.md) remains the historical R4 component
decision and is not overwritten by worker integration. No prior review-session
identity, role or shared README authority is inherited by this standalone work.
