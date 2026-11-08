# Record request behavior

A record of the Experimental Physics and Industrial Control System (EPICS) can admit an asynchronous GET or SET when Base calls the snmp3 device support. Completion processing consumes that request's result without admitting another request. This page explains processing order, pending writes, simulation mode, deadlines, and declared groups of bindings.

## Scope

This page covers the behavior of one record request and of requests that share a declared group. It explains the rules that the record contract states as values.

**Out of scope:** the record types, the alarm mapping, and the exact limits (see [Module contract and record support](snmp-rewrite-contract.md)); the worker protocol and timers (see [Worker commands, limits and timers](snmp-worker-reference.md)); the migration of an existing IOC database.

Abbreviations on this page: IPC is inter-process communication, and K is the number of objects in one request.

## Order of record processing

For an admitted asynchronous request, events happen in this order:

1. The device support admits the request.
2. The terminal response or error arrives.
3. The callback applies the value and the alarm under the record lock.
4. Base completes the record processing.
5. Base runs the forward link (FLNK).
6. Base clears the process-active flag (PACT).

The device support does not run FLNK manually. A fanout record does not imply an asynchronous completion barrier. A successful SET completion and a device readback are distinct observations.

The generation and activation identity reject stale or duplicate completions and prevent an implicit SET replay. Checked input stays staged until Base selects DSET completion rather than simulation. The callback finalizes its terminal even when simulation or an output policy skips the device support entry table (DSET). A failed `callbackRequest` keeps the borrowed result and the reservation, and the module retries it without native replay or a deadline reset.

## Latest write on a pending record

A write to an output record whose request is pending takes one of two Base routes.

A plain put to a process-passive field of a Passive record, or a write to `PROC`, sets the reprocess flag (RPRO). RPRO is one flag. After the pending request completes, Base clears it and reprocesses the record once. Several such puts therefore give one reprocess using the latest value, subject to Base output policies and request admission.

Base holds a put-callback until the processing of the record completes. Then it puts and processes normally. Each put-callback has a separate processing attempt; a SET requires Base to select the snmp3 DSET and the module to admit the request.

The pending request keeps the payload it captured at admission. The module never rewrites or replays it. In both routes, the next request is admitted behind the native retirement of the preceding request and keeps the deadline of its own admission. The DSET neither waits for the retirement nor retries.

A put-callback completes when Base finishes the completion processing of the record, after the final outcome of the request has been applied to the record. The record's alarm status (STAT), severity (SEVR), and alarm message (AMSG) fields carry the outcome. A SET completion does not show device readback. A separate GET does.

## Behavior in simulation mode

Base input simulation is a separate value source. A record whose simulation mode (SIMM) selects simulation reads the simulation input link (SIOL) before admission and sends no native request.

A SIMM change while a request is in flight can make Base bypass the DSET read at completion. The module never forces SIMM off. Whatever Base selects, the completion wrapper releases the terminal once, preserves the requested values and errors, and leaves FLNK and PACT to Base.

## Deadline path of a request

The `deadline_ms` value bounds a request from its admission. Queueing, relaunch, IPC, and native service share one budget.

A request ends at the earlier of its native outcome and its deadline. The profile fields `timeoutMs` and `retries` set the native service time of a lost response, so a `deadline_ms` below that time ends the request as a Deadline instead of a native timeout.

A request that reaches its deadline before transmission is reported as COMM/INVALID with the AMSG text `deadline before send`. A request that was sent keeps an empty AMSG.

A successor admitted behind a retirement keeps the deadline of its own admission. When its predecessor ends in a Deadline, the worker ends that request at the deadline on its own, and the successor is dispatched after the Retired frame on the same worker. A worker that sends nothing within the 1000 ms retire grace is contained, reaped, and relaunched; the successor is then dispatched only if its budget outlasts the grace, the reap, the restart backoff, the relaunch, and the Ready frame. Otherwise it expires unsent.

## Declared groups of bindings

A binding can carry a `group` label (see [Startup configuration](snmp-rewrite-config.md)). The records of the bindings of one group are admitted one by one, each with its own `deadline_ms`. The dispatcher combines compatible members at the queue front, subject to profile and IPC batch limits. One declared group can span several native requests.

Three consequences are part of the contract:

- **One deadline per request.** A request of a group has the earliest deadline of its members. A member with a later `deadline_ms` can therefore be reported as COMM/INVALID before its own budget has passed, because another member of its request expired first. A member whose own deadline has passed before dispatch expires alone with `deadline before send`. It does not shorten the request of the others.
- **Shared failures.** A timeout, a worker loss, or a native failure of the group request gives every member the same outcome. K records of a group fail together where K single requests would fail one at a time. The loss count of a monitoring set read in groups is counted per request, not per record.
- **A short wait for the rest of the group.** The dispatcher allows up to 2 ms from the first member's admission, or 0.5 ms after the last member arrived, for more members to join. This collection wait ends when the group is complete and is part of the request budget. Worker readiness and earlier requests can cause additional queueing.

## Recovery time after a containment

After containment, recovery requires the worker to exit and be reaped, the restart backoff to pass, and a replacement worker to send Ready. The first backoff is 250 ms. Consecutive failures increase it to 500, 1000, 2000, and then 4000 ms. A matching Retired frame resets the failure count. The [worker timer reference](snmp-worker-reference.md#timers-and-fairness) gives the configured limits.

A successor keeps the deadline of its own admission throughout recovery. If its budget expires before dispatch, it completes unsent with COMM/INVALID and `deadline before send`. Its budget must outlast any remaining retire grace, reap, backoff, relaunch, and Ready exchange for dispatch to remain possible.

The request budget does not bound time spent waiting for Base processing or callback delivery. A put-callback can complete with an alarm without a SET being transmitted. Neither a put nor a put-callback guarantees device delivery within two request budgets. SET completion and GET readback remain separate observations.

The [qualification record](https://github.com/jeonghanlee/snmp3/blob/6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf/docs/milestone-5dff352.md) identifies the products and measured relaunch and put-to-dispatch observations. Those measurements do not establish a fixed recovery or client-completion bound for another execution.
