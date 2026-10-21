# Address worker supervision

The input/output controller (IOC) accepts owned component requests, schedules each numeric address, supervises one native worker process per address, and returns scheduler-owned terminal results. Net-SNMP runs only in the native worker, so the IOC links no Net-SNMP code.

## Scope

This page explains how the module admits requests, batches them, hands them to the worker, and contains failures. It covers the ownership rules for requests, terminals, and worker processes.

**Out of scope:** commands, limits, timer values, and report fields (see [Worker commands, limits and timers](snmp-worker-reference.md)); the steps that run the verification (see [Verify the worker transport](snmp-worker-verification.md)); the record support contract (see [Module contract and record support](snmp-rewrite-contract.md)). Hardware acceptance, runs on a second operating system, thread sanitizer (TSan) runs, leak checks, and sustained resident set size (RSS) checks are separate work. The module has no IOC command that sends a GET or SET.

Abbreviations on this page: SHA256 is the secure hash algorithm with 256 bits, and SNMP is the Simple Network Management Protocol.

## Products and data flow

The module builds five products.

| Product | Ownership |
| --- | --- |
| `libsnmp3.so` | Config, Runtime, Scheduler, Supervisor, Conversion, Requests, and eleven device support entry tables (DSETs); EPICS Base only |
| `libsnmp3Wire.so` | Checked framing, typed values, and SHA256; free of native code |
| `snmp3Ioc` | Base shell, generated registration, and checked exit |
| `snmp3NativeProbe` | Callable native catalogue; opens no device session |
| `snmp3Worker` | One address, native sessions, callbacks, and owned responses |

A request moves through these owners in order:

1. The component caller creates it from an immutable binding.
2. The scheduler admits it atomically.
3. The scheduler places it in a first-in, first-out (FIFO) batch.
4. The batch crosses a private stream channel for inter-process communication (IPC).
5. The actual worker receives it and calls the native layer.
6. The native layer calls Net-SNMP and returns an owned result.
7. The scheduler terminal consumer takes the result.

The worker sends a Retired frame after the native call unwinds and after it destroys its request and result copies. No frame holds a record pointer. The servicing thread calls no arbitrary completion consumer.

The canonical IPv4-mapped IPv6 form and its IPv4 address share one worker. Other IPv6 addresses keep their complete address bytes. Endpoints keep their original numeric transport address and port. Profiles, engines, and contexts keep distinct native session identities within the address worker. An activation allows at most 64 canonical addresses.

## Admission and ownership

Each consumer registers its own immutable binding. Alias handles for the same definition have separate generation and terminal identities.

A handle holds at most two generations: one whose terminal is consumed and awaits only native retirement, and a successor. The scheduler admits a successor only when every existing generation of the handle is consumed and at most one exists. Otherwise it rejects the admission.

An atomic admission call targets one address. It captures one monotonic origin and a positive integer budget of 1 to 600000 ms. The resulting 64-bit unsigned deadline covers queueing, bootstrap, IPC, discovery, and native service.

Atomic admission publishes all members or none. It is not one SNMP transaction. Each member has its own count, queue charge (Q), and positional owned result. The reference page defines the charge.

## Batching and declared groups

The scheduler dispatches at most one native batch per address. A batch is a contiguous prefix of the FIFO queue whose requests share the endpoint, profile, session, and operation, and are compatible.

Two ungrouped bindings are compatible only with the exact same absolute deadline. A group of one binding counts as ungrouped. Two members of one declared group are compatible whatever their deadlines. A group member never joins an ungrouped request or the request of another group. The batch ends at such a request, with no look-ahead.

The batch respects `maxVarbinds` and both frame limits. Several records of one binding can exceed the declared count, so the cap applies to groups too, and the remainder leaves in the next request. Equal budgets with different origins do not imply equal deadlines. The scheduler never rounds, skips, or extends a deadline.

A request of a declared group carries the earliest deadline among its members. The scheduler writes that deadline into the dispatched commands and the member generations, so completion, expiry, and the retire grace give every member the same outcome. A member whose own deadline has already passed expires alone before dispatch and leaves the queue. The other members leave together.

The scheduler holds a declared group at the queue front while its worker is ready, idle, and not stopping. The front run of the group leaves at the first of these events:

- The run holds a generation of every declared binding.
- No further member has been admitted for 0.5 ms (the quiet gap).
- 2 ms have passed since the admission of the front member (the hold bound).

A group of one binding is not held. A member put back at the queue front after a worker loss is not held either. `Scheduler::holdRelease` returns the release time, or zero when nothing is held. The supervisor includes that time in `nextTimer` only for a ready, idle worker that is not stopping, so a busy worker does not wake the loop.

## Recovery after ambiguity

The first successfully transmitted Batch byte makes every batch member ambiguous for recovery. Only zero-byte work survives worker replacement with its original identity and deadline. The module never replays a GET or SET automatically after ambiguity. Native retries remain Net-SNMP behavior. A SET completion does not establish device readback truth.

## Terminal consumption and retirement

The consumer borrows a terminal through `Scheduler::take` and completes its unique consumption through `release`. The full count and Q stay charged until both consumption and actual request retirement have happened.

A matching Retired frame proves a normal native unwind and destroyed frame and request copies. Worker loss requires closed channel buffers and an exact wait and reap of the old process identifier (PID). A deadline alone proves neither.

Retirement can come before consumption, or consumption before retirement. Both orders preserve the charge until the joint transition. Stale or duplicate frames cannot change current ownership or select a second terminal.

## Generations queued behind a retirement

A generation admitted behind a retirement-pending predecessor waits in the per-address FIFO. Dispatch keeps at most one active batch per address, so the scheduler does not send the successor before the predecessor's Retired frame or exact reap.

The successor keeps the deadline of its own admission, so waiting consumes its budget. It keeps its own count and Q. If its deadline passes before dispatch, the scheduler selects Deadline for it as never-sent work. A stop selects Stopping for it, and a reap retires only the predecessor. Every scheduler structure and lookup is keyed by binding and generation, so the two generations of one handle never share state.

Reports classify charged work as `queued`, `active`, `undelivered`, or `retirementPending`. The `count` and `bytes` values include all four classes. A borrowed terminal remains `undelivered` until release. `retirementPending` means the terminal was consumed but its native or channel ownership has not ended. The two generations of one handle can sit in two classes at once: the consumed predecessor in `retirementPending` and the queued successor in `queued`. Then `count` is 2.

## IPC and worker spawn

The IOC and each worker communicate over a private close-on-exec AF_UNIX SOCK_STREAM socket pair. Frames have an explicit 64-byte header (FSN5, version 1, big-endian) with a checked kind, length, identity, and direction-local sequence number. Partial and coalesced I/O is normal. Malformed framing is an address failure, and the receiver does not scan to resynchronize. The receiver discards stale data through bounded scratch space. No pointer, native structure, or platform long crosses the channel.

The spawn argument vector contains only the selected executable and `--ipc-fd=3`. Checked file actions map that descriptor, close all other descriptors, and send the production standard streams to `/dev/null`. The environment contains `PATH`, `LANG`, `LC_ALL`, and preflight-hashed library directories only. The module passes no secret file name, no secret bytes, and no inherited `LD_PRELOAD`.

Bootstrap carries the frozen definitions and credentials, and the worker never reopens the secret files. Before application dispatch, the Ready frame must match the capabilities, the executable SHA256, and the loaded-library SHA256. A SecurityConflict does not restart a healthy worker to discard pinned user-based security model (USM) material.

## Containment of worker failures

The supervisor handles due deadlines before optional I/O and spawn. Expiry selects one Deadline for each nonterminal member of the equal-deadline active batch. A batch no byte of which was transmitted is contained at the deadline.

A transmitted batch is not contained at the deadline. The worker ends its batch at the deadline on its own, sends the Result and the Retired frame, closes and discards its native sessions, and takes the next batch; an SNMPv3 session then runs discovery again. Containment of a member selected Deadline, Complete or NativeFailure starts only when its Retired frame is missing 1000 ms after its deadline, so a worker that sends nothing within that grace is contained, reaped and relaunched. A matched Retired frame resets the consecutive failure count of the address. The four-attempt rate cap does not change.

No next batch uses the slot before Retired or exact reap. Peer addresses stay independently serviced. A new epoch follows the actual reap of the old PID. Normal native close and forced termination are separate observations.

The timers assume runnable operating system threads. The module keeps an unreaped process as incomplete ownership and never reports it as successful cleanup.

## Lifecycle and stop

The AfterFinishDevSup hook freezes Config and starts the servicing owner before Base initial processing. The IOC hook performs no synchronous native discovery.

The AtShutdown hook closes admission, dispatch, and recovery. It selects Stopping for outstanding nonterminal work, requests worker close, and joins the service thread once before Base stops its callback services. Terminals already selected keep their outcome.

The runtime states are Cold, Starting, Running, Stopping, Stopped, IncompleteStopped, and Failed. IncompleteStopped retains unconsumed terminal or unreaped child ownership after the join. A later explicit stop, fallback, or start preflight performs only nonblocking reconciliation, and a start is rejected until the retained ownership settles. After that, an isolated activation can create a scheduler and worker epoch. After a failed or incomplete stop, IncompleteStopped retains up to two charged generations per handle: the consumed predecessor awaiting retirement or reap, and its successor.

A failed preflight, a failed thread creation, or an unexpected service failure closes admission. A record stop adds a 2000 ms callback-drain attempt. The entry gate closes after the attempt, followed by a separate safety wait for entered callbacks. A failed record drain stays failed after native reconciliation and refuses a restart.

The AtShutdown hook permits detach only after entered processing reaches zero. Isolated queue destruction, not callback join alone, permits context release. Non-isolated shutdown retains contexts that Base references. External Base processing can extend the safety wait, so the module makes no claim about a bounded total shutdown time.

The [current qualification record](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/milestone-5dff352.md#current-required-verification-results-2026-10-08) lists the accepted lifecycle paths and the remaining reuse limitations.

## Related pages

- [Worker commands, limits and timers](snmp-worker-reference.md) lists the exact values.
- [Verify the worker transport](snmp-worker-verification.md) runs the checks.
- [Worker supervision decision](decisions/snmp-worker-supervision.md) records the ownership decision.
