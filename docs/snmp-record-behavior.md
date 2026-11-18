# Record request behavior

A record of the Experimental Physics and Industrial Control System (EPICS) can admit an asynchronous GET or SET when Base calls the snmp3 device support. Completion processing consumes that request's result without admitting another request. This page explains processing order, pending writes, the outcome of a SET, simulation mode, deadlines, the request size per address, a unit that is lost and returns, the state of the module as records, declared groups of bindings, and the recovery time after a containment.

## Scope

This page covers the behavior of one record request and of requests that share a declared group, the records that process on the reply of a request the module sends by itself, what the records of an endpoint show when its unit rejects a request, does not answer or is replaced, and the records that show the state of the module. It explains the rules that the record contract states as values.

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

## Outcome of a SET

An output record stays active until the result of its SET arrives; it never reports a SET that was only sent. The module keeps no queue of values of its own: three puts that each wait for completion give three SETs in the order written, and plain puts that arrive while a request of the record is pending are merged by Base into one further SET with the latest value.

| Result | Record |
| --- | --- |
| The unit accepts the SET | No alarm; a later GET of the object returns the written value |
| The unit refuses it with an error status | WRITE/INVALID with the AMSG text `<status> index <n>`, or `tooBig` |
| No reply | COMM/INVALID with the AMSG text `no reply`; the record keeps the written value in VAL although the unit did not take it |

## Behavior in simulation mode

Base input simulation is a separate value source. A record whose simulation mode (SIMM) selects simulation reads the simulation input link (SIOL) before admission and sends no native request.

A SIMM change while a request is in flight can make Base bypass the DSET read at completion. The module never forces SIMM off. Whatever Base selects, the completion wrapper releases the terminal once, preserves the requested values and errors, and leaves FLNK and PACT to Base.

## Deadline path of a request

The `deadline_ms` value bounds a request from its admission. Queueing, relaunch, IPC, and native service share one budget.

A request ends at the earlier of its native outcome and its deadline. The profile fields `timeoutMs` and `retries` set the native service time of a lost response, so a `deadline_ms` below that time ends the request as a Deadline instead of a native timeout.

A request that reaches its deadline before transmission is reported as COMM/INVALID with the AMSG text `deadline before send`. A request that was sent and failed carries one of the AMSG texts below; every other outcome of a sent request keeps an empty AMSG.

| AMSG | Alarm | Cause |
| --- | --- | --- |
| `tooBig` | READ or WRITE, INVALID | The unit answered the request with the error status tooBig |
| `unreadable rejection` | READ or WRITE, INVALID | The request timed out and the library counted a reply that it could not parse while the request was pending |
| `no reply` | COMM, INVALID | The request timed out and no unparsable reply was counted |
| `<status> index <n>` | READ or WRITE, INVALID | The unit answered the request with another error status: its name as RFC 3416 lists it and the error index, the position in the request of the value it names, for example `wrongType index 1` |

The count of unparsable replies is the `snmpInASNParseErrs` statistic of the Net-SNMP library, read by the worker when it sends the request and when the request times out. It rises for an unparsable reply on an SNMPv3 session. On an SNMPv2c session it was observed not to rise, so such a reply is reported as `no reply`; SNMPv1 was not run.

## Request size per address

A request holds at most `maxVarbinds` values. When the unit rejects a request, by a tooBig reply or by a reply that ends as `unreadable rejection`, the scheduler sets the limit of that address to half the number of values of the rejected request, rounded down and never below one. The records of the rejected request complete with the alarm above, and the following requests of the address use the lower limit: a rejected request of 61 values is followed by requests of at most 30, and each further rejection halves the limit again, to 15, 7, 3, and 1. A rejection never raises the limit. A `no reply` timeout and every other error status leave the limit unchanged.

The limit belongs to one address and lasts until its worker is restarted; the first request after the restart uses `maxVarbinds` again. `snmp3RuntimeReport` prints the limit and the number of reductions since the IOC started in the `snmp3 batch:` line of the address.

Observed on 2026-11-14 with two APC AP8932 units: with 328 read bindings and no `group`, of 4576 requests in 120 s, 4571 were 134 to 140 bytes, one value each, 2 were engine discovery requests of 64 bytes, and 3 were 160 bytes, and no request was rejected at `maxVarbinds` 61 or 120. A group of 61 integer bindings left as one request of 1460 bytes, which the unit rejected; after the limit became 30 the same group left as requests of 778, 787, and 140 bytes and every record filled.

A successor admitted behind a retirement keeps the deadline of its own admission. When its predecessor ends in a Deadline, the worker ends that request at the deadline on its own, and the successor is dispatched after the Retired frame on the same worker. A worker that sends nothing within the 1000 ms retire grace is contained, reaped, and relaunched; the successor is then dispatched only if its budget outlasts the grace, the reap, the restart backoff, the relaunch, and the Ready frame. Otherwise it expires unsent.

## A unit that is lost and returns

A unit that does not answer gives COMM/INVALID with the AMSG text `no reply`, also when it has not answered since the IOC started. SNMP has no connection to restore: when the unit answers again, the next request of a record succeeds, and the records return to normal without a restart of the IOC.

An SNMPv3 session remembers the engine ID, the engine boots and the engine time of its unit. After 2 consecutive requests of an SNMPv3 endpoint without a reply, the worker closes the session of the endpoint and drops the remembered engine time, so that the next request asks the unit for its engine ID and synchronizes its time from the start. This recovers a unit that returns with its engine time behind the remembered one and the same boots, which the library otherwise refuses for good. The least interval between two such new starts follows the beacon of the Channel Access server: 0.02 s at first, doubled each time, 15 s at most; a reply sets it back.

### Another engine ID at the address

A responder that shows another engine ID than the one the endpoint used first is not taken for the same unit. The endpoint enters a state that lasts until the IOC is restarted, also when the earlier engine ID answers again:

| State | Condition |
| --- | --- |
| `changed` | The responder answered an authenticated request under the secrets of the profile with its new engine ID |
| `unverified` | That request failed, or the responder cannot be asked: the profile names its engine ID in the configuration, has no authentication, or the endpoint has no `systemInfo` binding to read |

Until the responder has answered the first reading of a `systemInfo` binding, the endpoint is shown as `unverified`; it becomes `changed` when that reading is answered under authentication, usually within seconds.

In either state:

- no SET is sent; an output record completes with the alarm below;
- no value read from the unit reaches a record; the records keep the value they had;
- every record of the endpoint is COMM/INVALID with the AMSG text `engine ID changed: restart IOC` or `engine ID unverified: restart IOC`;
- in the state `changed` the bindings marked `systemInfo` are still read, and their records show the responder's values with the same alarm and message.

The entry into a state writes one line to the IOC log:

```
snmp3 engine: endpoint=<id> state=<changed|unverified> previous=<hex> current=<hex> authenticated=<0|1> time=<UTC>
```

`snmp3RuntimeReport` shows the state of each address in its `snmp3 peer:` line and the state of each endpoint, with the time of the change, in its `snmp3 endpoint:` line; the record `$(P)PeerState` shows the state of an endpoint as a PV (see [State of the module as records](#state-of-the-module-as-records)). After the cause is known, restart the IOC; an endpoint whose profile names its engine ID needs the new engine ID in the configuration first.

SNMPv1 and SNMPv2c carry no engine ID, so a replaced unit cannot be told apart there.

## Records that process on a reply

A record with a periodic `SCAN` sends its request when Base processes it and completes when the reply arrives, so its rate is bound to the scan period: a unit that answers later than the period lowers the rate to a multiple of the period. A subscription turns this around: the module sends the request, and the record processes when the reply has arrived.

```text
snmp3Subscription PDU1Fast PDU1 1000
```

```text
record(longin, "<record_name>") {
    field(DTYP, "snmp3")
    field(SCAN, "I/O Intr")
    field(INP, "@PDU1Fast 1.3.6.1.2.1.1.3.0 type=timeticks")
}
```

| Rule | Behavior |
| --- | --- |
| One request per subscription | The objects of the records of one subscription leave together, in as few requests as the request size rules of the address allow |
| When a request leaves | While a record of the subscription is on the `I/O Intr` list and Base scans; a subscription whose records have all left the list, by a change of `SCAN`, sends nothing. The first one when a record joins a list that held none. The next one when the period has passed, the request before it has ended, and Base has processed its records. A unit that answers within the period is asked once per period; a slower unit is asked again as soon as its reply was processed |
| One processing, one result | Each record processes once per request, with the value, the alarm and the message a request of the record itself gives. The module keeps no value of its own |
| Order of one reply | Base processes the records of one `I/O Intr` list by `PHAS`, then by the order they were loaded, among the records of one `PRIO`. Records of one reply with different `PRIO` have no order between them. A record that must process after the others of a reply takes the highest `PHAS` of that `PRIO` |
| No reply | Each attempt ends as COMM/INVALID with `no reply`; the value stays. When the unit answers again the records are without alarm, with no restart of the IOC. A subscription of another address keeps its period meanwhile |
| Another engine ID | The rules of [Another engine ID at the address](#another-engine-id-at-the-address) hold: no request for a value leaves, the records carry the alarm of the state, and a record marked `systemInfo` is still read in the state `changed` |
| A request that is not admitted | When the queue of the address does not take the request of a subscription, for its count or its bytes, every record of it is READ/INVALID with `request not admitted`. A subscription with more records than the count limit of its address needs a higher limit (`snmp3QueueLimit`) |
| A processing without a result | A put to `PROC` or a link that processes the record changes nothing: the value stays and the alarm of the last result is raised again |

These records are rejected at initialization: a record with `SCAN` set to `I/O Intr` whose link names a session or a binding of the file, a record with another `SCAN` whose link names a subscription, an output record whose link names a subscription, and a record of a subscription whose link carries `group=`, because the records of a subscription leave in one request by themselves. A subscription holds read objects only; the link of an output record names the session. Setting `SCAN` of a record of a session to `I/O Intr` while the IOC runs is refused by Base, which sets it to `Passive`.

The deadline of a request of a subscription is the shortest deadline of its records.

## State of the module as records

The module installs two databases into `db/`. Their records read the state that the IOC process holds; no request goes to a unit. An IOC application loads them with `dbLoadRecords`, the runtime database once and the endpoint database once for each endpoint it wants to show:

```
dbLoadRecords("$(SNMP3)/db/snmp3Runtime.db", "P=<prefix>")
dbLoadRecords("$(SNMP3)/db/snmp3Endpoint.db", "P=<prefix of the unit>,ENDPOINT=<endpoint id>")
```

| Record | Type | Value |
| --- | --- | --- |
| `$(P)RuntimeState` | `mbbi` | State of the module, as the `snmp3 runtime:` line prints it: `Cold`, `Starting`, `Running`, `Stopping`, `Stopped`, `IncompleteStopped`, or `Failed`. `IncompleteStopped` and `Failed` carry MAJOR |
| `$(P)PeerState` | `mbbi` | State of the endpoint after a responder showed another engine ID: `normal`, `changed`, or `unverified`. `changed` and `unverified` carry MAJOR. While the responder is still being asked to authenticate, the record reads `unverified` |
| `$(P)BatchLimit` | `longin` | `limit` of the `snmp3 batch:` line of the address of the endpoint. 0 means that no rejection has lowered the request size, or that the worker of the address has ended or was restarted since. `$(P)BatchSplits` tells the two apart: 0 there means that no request was ever rejected, and a number above 0 beside a limit of 0 means that the limit was lowered and then forgotten with the worker |
| `$(P)BatchSplits` | `longin` | `splits` of the same line, the number of reductions since the IOC started |

`ENDPOINT` is the `id` of an endpoint in the configuration; a record that names an endpoint the loaded configuration does not hold is rejected at initialization, and the IOC prints `snmp3: record initialization rejected: <record>`. Endpoints that share an address share its request size, so their `$(P)BatchLimit` and `$(P)BatchSplits` records read the same values.

Every record has `PINI=YES`, and the macro `SCAN` sets how it is scanned; it defaults to `1 second`. With a periodic choice of the Base SCAN menu, written as the menu writes it, for example `5 second` or `.5 second`, a change of state is visible on a record at most one scan period after it happened. With `SCAN=I/O Intr` the record processes when a state the module shows has changed, without a periodic scan: the records of the device support, `DTYP` `snmp3 state`, share one `I/O Intr` list, and a change of the runtime state, of the state of an endpoint, or of the request size of an address processes all of them. They also process once when Base begins to scan, so that a state that changed during the start of the IOC is shown.

```text
dbLoadRecords("$(SNMP3)/db/snmp3Runtime.db", "P=<prefix>,SCAN=I/O Intr")
```

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
