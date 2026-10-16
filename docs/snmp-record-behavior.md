# Record request behavior

A record of the Experimental Physics and Industrial Control System (EPICS) that uses the snmp3 device support sends one request to its device each time Base processes the record. This page explains what happens between that processing and the record's completion: the order of events, the handling of a write while a request is pending, simulation mode, deadlines, and declared groups of bindings.

## Scope

This page covers the behavior of one record request and of requests that share a declared group. It explains the rules that the record contract states as values.

**Out of scope:** the record types, the alarm mapping, and the exact limits (see [Module contract and record support](snmp-rewrite-contract.md)); the worker protocol and timers (see [Worker commands, limits and timers](snmp-worker-reference.md)); the migration of an existing IOC database.

Abbreviations on this page: IPC is inter-process communication, and K is the number of objects in one request.

## Order of record processing

For each record, events happen in this order:

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

A plain put to a Passive record, or a write to `PROC`, sets the reprocess flag (RPRO). RPRO is one flag. After the pending request completes, Base clears it and reprocesses the record once. Several puts during one pending request therefore give one reprocess that sends only the latest value.

Base holds a put-callback until the processing of the record completes. Then it puts and processes normally, so each put-callback sends its own SET.

The pending request keeps the payload it captured at admission. The module never rewrites or replays it. In both routes, the next request is admitted behind the native retirement of the preceding request and keeps the deadline of its own admission. The DSET neither waits for the retirement nor retries.

A put-callback completes when Base finishes the completion processing of the record, after the final outcome of the request has been applied to the record. The record's alarm status (STAT), severity (SEVR), and alarm message (AMSG) fields carry the outcome. A SET completion does not show device readback. A separate GET does.

## Behavior in simulation mode

Base input simulation is a separate value source. A record whose simulation mode (SIMM) selects simulation reads the simulation input link (SIOL) before admission and sends no native request.

A SIMM change while a request is in flight can make Base bypass the DSET read at completion. The module never forces SIMM off. Whatever Base selects, the completion wrapper releases the terminal once, preserves the requested values and errors, and leaves FLNK and PACT to Base.

## Deadline path of a request

The `deadline_ms` value bounds a request from its admission. Queueing, relaunch, IPC, and native service share one budget.

A request ends at the earlier of its native outcome and its deadline. The profile fields `timeoutMs` and `retries` set the native service time of a lost response, so a `deadline_ms` below that time ends the request as a Deadline instead of a native timeout.

A request that reaches its deadline before transmission is reported as COMM/INVALID with the AMSG text `deadline before send`. A request that was sent keeps an empty AMSG.

A successor admitted behind a retirement keeps the deadline of its own admission. When its predecessor ends in a Deadline, the supervisor contains, reaps, and relaunches the worker. The successor is dispatched only if its budget outlasts the reap, the restart backoff, the relaunch, and the Ready frame. Otherwise it expires unsent.

## Declared groups of bindings

A binding can carry a `group` label (see [Startup configuration](snmp-rewrite-config.md)). The records of the bindings of one group are admitted one by one, each with its own `deadline_ms`. The dispatcher sends the members that are queued together as one native request.

Three consequences are part of the contract:

- **One deadline per request.** A request of a group has the earliest deadline of its members. A member with a later `deadline_ms` can therefore be reported as COMM/INVALID before its own budget has passed, because another member of its request expired first. A member whose own deadline has passed before dispatch expires alone with `deadline before send`. It does not shorten the request of the others.
- **Shared failures.** A timeout, a worker loss, or a native failure of the group request gives every member the same outcome. K records of a group fail together where K single requests would fail one at a time. The loss count of a monitoring set read in groups is counted per request, not per record.
- **A short wait for the rest of the group.** The first member of a group can wait up to 2 ms in the queue, or 0.5 ms after the last member arrived, so that the group leaves whole. This wait is part of the request budget.

## Recovery time after a deadline

With an outer user datagram protocol (UDP) drop-all fault, the time from containment to a Ready worker was about 0.4 s on ordinary builds (362 to 403 ms over the recorded runs). On instrumented builds it was about 0.7 s (710 to 745 ms). Both values include the first restart backoff step of 250 ms.

Each further consecutive failure of the worker lengthens the backoff to 500, 1000, 2000, and then 4000 ms. A consecutive failure is a failure without a matching Retired frame in between. The timer table of the [worker reference](snmp-worker-reference.md#timers-and-fairness) lists the series.

A budget must also cover the backoff step in effect. In the deadline-queue case, a budget of 1000 ms dispatched the queued request in each of three runs on both build types. That budget covers only the first backoff step and left about 260 ms to spare on instrumented builds. A smaller budget lets the queued request expire unsent, which the record reports as COMM/INVALID with the AMSG text `deadline before send`.

A worker that does not react to its closed channel took 1.83 to 1.85 s from containment to Ready on an ordinary build, and longer on an instrumented build. In the measured case, a process signal stopped the worker. The qualification `deadline` case measures this time. The time is the difference between the containment event (code 6) and the Ready event (code 3) of the next epoch. The [verification reference](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/tests/rewrite/README.md) gives the command.

After the put of an operator, the successor is dispatched at most two budgets plus the callback delay later. At a budget of 1000 ms, dispatch came 1370 to 1392 ms after the put on ordinary builds and 1718 to 1732 ms on instrumented builds. A value written while a request that ends in a Deadline is pending therefore reaches the device up to about twice the budget after the put.

A put-callback waits for the pending request to end and then for its own request. A single put-callback therefore completes within the same bound. Each further put-callback queued on the same record adds up to one budget. The [work register](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/milestone-5dff352.md) records the relaunch and put-to-dispatch runs.
