# Work Register

Release line: 1.0.0
Milestone index: 1.0.0
Canonical path: `docs/milestone-1.0.0.md`
Canonical branch or ref: master
Git upstream: origin/master
Remote tracker: jeonghanlee/snmp3, GitHub milestone 1.0.0
Recorded date: 2026-11-09
Source baseline: tag `0.0.1` at `8c5bd199f3137bb3c5756c7a2c7d1640122ed788`; register reset from `ba6c558a1118c792e769aed79ded7581538c3888`

Next session entry point: Start M1 in `docs/milestone-1.0.0.md` (probe spawn environment): accept its plan, then change `snmp3App/src/Config.cpp` so the native probe receives the same environment list as the worker in `snmp3App/src/Supervisor.cpp`, and run `tests/rewrite/ConfigTest.cpp`. In parallel, close G1 by observing both laboratory units through the laboratory IOC. M5 and M9 are also Ready.

## Scope

The goal of this release line is that every guarantee in [What snmp3 guarantees against each cause](why-snmp3.md#what-snmp3-guarantees-against-each-cause) reads Implemented, and that the implemented product is verified on one NMC2 unit and one NMC3 unit sharing one IOC. The five causes and the 23 items behind them are recorded in [The operating problems behind snmp3](why-snmp3.md) and the [Review record](legacy-review.md). The release line also includes the table-read request (GETBULK), the SET outcome contract, reply-driven record processing, and shared-library SONAME versioning (D1, D3).

Work is ordered by the code surface each unit shares with the next; the row order is the execution order and the local IDs follow it. Each completed M row adds a `CHANGELOG.md` entry and an annotated tag with the version named in its detail; the final release is `1.0.0`.

Out of scope: performance criteria and acceptance (Backlog M12), the deferred record verification cells (Backlog M13), the negative-control harness classification (Backlog M11), a new measurement tool before an existing one is shown insufficient (D4), and any deployment to the operations network before `1.0.0`.

## Milestone

### Work

| Group | ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Gate | G1 | Laboratory NMC2 and NMC3 units reachable through snmp3 | External gate | Open | No | | Both units answer a GET through the laboratory IOC with snmp3 `0.0.1`; [detail](#g1---laboratory-nmc2-and-nmc3-units-reachable-through-snmp3) |
| E | M1 | Probe spawn environment equal to the worker environment | Milestone | Not started | Yes | | Probe and worker receive the same environment list; `ConfigTest` passes; tag `0.0.2`; [detail](#m1---probe-spawn-environment-equal-to-the-worker-environment) |
| A | M2 | Two-device isolation measurement on the current product | Milestone | Blocked | No | G1, D4 | Measured cadence of the healthy unit with the other unit disconnected; [detail](#m2---two-device-isolation-measurement-on-the-current-product) |
| B | M3 | Request sizing with advertised message size and tooBig split | Milestone | Blocked | No | G1 | NMC2 accepts a 61-OID request without manual `maxVarbinds` tuning; tooBig reaches the record; tag `0.0.3`; [detail](#m3---request-sizing-with-advertised-message-size-and-toobig-split) |
| B | M4 | SET outcome contract | Milestone | Not started | No | M3 | Output records complete only on the SET result; a timeout is an alarm; tag `0.0.4`; [detail](#m4---set-outcome-contract) |
| C | M5 | Reply time stamp on records | Milestone | Not started | Yes | | Record TIME equals the worker receive time with `TSE=-2`; tag `0.0.5`; [detail](#m5---reply-time-stamp-on-records) |
| C | M6 | Reply-driven processing and driver-owned periodic acquisition | Milestone | Not started | No | M3, M5 | `SCAN=I/O Intr` records process on reply arrival at the configured period; tag `0.0.6`; [detail](#m6---reply-driven-processing-and-driver-owned-periodic-acquisition) |
| Performance | M7 | GETBULK table reads | Milestone | Not started | No | M3 | A table read uses one GETBULK request where the profile allows it; tag `0.0.7`; [detail](#m7---getbulk-table-reads) |
| D | M8 | SNMPv3 localized keys and secret procedure | Carry-forward | Blocked | No | G1 | Worker retains localized keys only; documented procedure creates files that reach both units; tag `0.0.8`; [detail](#m8---snmpv3-localized-keys-and-secret-procedure) |
| Release | M9 | Shared-library SONAME and symbolic-link versioning | Carry-forward | Not started | Yes | D3 | Installed libraries carry SONAMEs and links per policy; consumers load them; tag `0.0.9`; [detail](#m9---shared-library-soname-and-symbolic-link-versioning) |
| Release | M10 | Release 1.0.0 | Milestone | Not started | No | M1, M2, M3, M4, M5, M6, M7, M8, M9 | Integrated re-runs and the two-unit acceptance pass; tag `1.0.0` and GitHub release exist; [detail](#m10---release-100) |

Current work tally: 0 Complete, 0 In progress, 7 Not started, 3 Blocked; G1 Open. Ready rows: M1, M5, M9. Backlog contains 1 Open and 2 Deferred rows and is excluded from this tally.

### Decisions

| ID | Decision | Decision Date |
| --- | --- | --- |
| D1 | Open release line 1.0.0 with one goal: every guarantee of the five causes implemented and verified on one NMC2 and one NMC3 unit on one IOC. Include GETBULK table reads, the SET outcome contract and reply-driven record processing. Reset the register so local IDs follow the execution order. | 2026-11-09 |
| D2 | Move the negative-control harness classification, the performance criteria and the deferred record verification cells to Backlog. None of them is among the 23 recorded items; they do not gate `1.0.0`. | 2026-11-09 |
| D3 | Include shared-library SONAME and symbolic-link versioning in `1.0.0`, because `1.0.0` asserts a stable library interface. This supersedes the 2026-10-24 retention of that work as long-term Backlog. | 2026-11-09 |
| D4 | The two-device measurement uses the existing `docs/snmp-performance` tools and `tests/rewrite` runners first. The diagnostic files kept under `work/` since 2026-11-09 remain reference and are used only when an existing tool is shown insufficient for one named judgment. | 2026-11-09 |
| D5 | Defer performance execution without an acceptance exception. Retain the once result, failed sustained criterion, baseline variation, lost probe and historical delay question. Resumption requires a bounded product question, acceptance criterion and stop condition; tool changes and new equipment runs are not automatic. | 2026-11-06 |
| D6 | Defer the unexecuted Channel Access, long active client-write, delayed output-simulation and isolated rebuild/reuse cells. They do not qualify the accepted record scope and resume only by a new decision. | 2026-10-05 |
| D7 | Publish `0.0.1` as the baseline tag of the accepted GET-only monitoring scope and its recorded limits. The tag fixes a reference point for later versions; it is not a completion claim, a performance acceptance or a change to any work row. | 2026-11-09 |

### Milestone Details

#### G1 - Laboratory NMC2 and NMC3 units reachable through snmp3

Origin: 1.0.0 / G1
GitHub Issue: none
Status: Open

##### Summary

M2, M3 and M8 need one AP8932 unit with an NMC2 card and one with an NMC3 card, both reachable from the laboratory IOC host and both answering snmp3 `0.0.1` requests with the laboratory IOC's existing SNMPv3 secret files. The laboratory IOC of the apcpdu repository (commit `3314b1e`) is the known startup. The owner confirms the units are powered, on the laboratory network and configured.

##### Completion Criteria

- One GET per unit completes through the laboratory IOC with `0.0.1`, observed with `caget` on one input record of each unit, and the observation time, unit card generation and module tag are recorded here. No address, host name or credential is recorded.

##### Verification Results

| Observed At | Result | Evidence |
| --- | --- | --- |
| Not run | Pending | Owner observation on the laboratory IOC |

##### Closure Evidence

- none

#### M1 - Probe spawn environment equal to the worker environment

Origin: 1.0.0 / M1
Identity History: none
GitHub Issue: #2 - https://github.com/jeonghanlee/snmp3/issues/2
Status: Not started

##### Summary

`snmp3Load` launches `snmp3NativeProbe --capabilities` with the IOC process environment (`Config.cpp`, `posix_spawn` with `environ`), while the supervisor launches the worker with a fixed list (`PATH`, `LANG`, `LC_ALL`, `LD_LIBRARY_PATH`). A catalogue produced under the IOC environment can name a library set the worker never loads. This unit gives both spawns the same environment list.

##### Scope

- One shared environment list used by the probe spawn and the worker spawn.
- A configuration test that the probe runs with that list.

Out of scope: the catalogue format, the capability comparison in the supervisor, and any change to what the worker loads.

##### Completion Criteria

- The probe and the worker are spawned with byte-identical environment lists built by one function.
- `ConfigTest` and the ordinary matrix stage pass on the changed product.

##### Dependencies And Decisions

- none

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Move the worker environment list of `Supervisor.cpp` into one function visible to `Config.cpp`.
2. Pass that list to the probe `posix_spawn` instead of `environ`.
3. Add a `ConfigTest` case that runs the probe through a helper printing its environment and compares the list.
4. Run `tests/rewrite/run_matrix.py --stages environment,ordinary`.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Configuration | `ConfigTest` case comparing the probe environment with the worker list | Linux host, ordinary product | Lists identical |
| T2 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` | Linux host, ordinary product | All cells PASS |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Linux host, ordinary product | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Spawn the native probe with the worker environment
Labels: bug
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: bug
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:09Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M2 - Two-device isolation measurement on the current product

Origin: 1.0.0 / M2
Identity History: none
GitHub Issue: #3 - https://github.com/jeonghanlee/snmp3/issues/3
Status: Blocked

##### Summary

Guarantee A states that one blocked, slow or absent address changes neither the IOC thread nor another address. The qualification record of `0.0.1` holds loss, late-response and healthy-address observations on fixtures; no run with two physical units exists. This unit measures, on the laboratory IOC with the NMC2 and the NMC3 unit, the record cadence of one unit while the other is disconnected and while it is reconnected, with the existing tools. The result is the reference for M10 and for comparison with the legacy observation of problem 2.

##### Scope

- One IOC, both units, the `0.0.1` product, the monitored record population of the laboratory loaders.
- Three phases of at least 300 s each: both units up, NMC2 disconnected, NMC3 disconnected.
- Record per phase: processing interval and value-change interval of one record per unit, worker containment and relaunch counts, and request outcomes.

Out of scope: performance criteria (M12), a product change, a new tool before an existing tool is shown insufficient (D4).

##### Completion Criteria

- For each phase the healthy unit's processing interval median and 95th percentile are recorded and the ratio to the both-up phase is at most 1.1, or the deviation is recorded with its cause.
- The disconnected unit's records show the configured alarm with its message within one deadline of the disconnection.

##### Dependencies And Decisions

- G1 open; resume as Not started.
- D4 selects the tools.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Select the record per unit and the capture method from `docs/snmp-performance/module-database-procedure.md`.
2. Run the three phases; disconnect by removing the unit's network link, not by stopping the IOC.
3. Record intervals, outcomes and worker counts per phase under `work/` and summarize here.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Device cadence | Three 300 s phases with the existing capture tools on the laboratory IOC | Laboratory IOC, both units, `0.0.1` | Healthy unit ratio at most 1.1 in each phase |
| T2 | Alarm on loss | `camonitor` of the disconnected unit's records during the phase | Laboratory IOC | Alarm with message within one deadline |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Laboratory IOC, both units | Pending | none |
| T2 | Not run | Laboratory IOC | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Measure one unit while the other unit is disconnected
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:12Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M3 - Request sizing with advertised message size and tooBig split

Origin: 1.0.0 / M3
Identity History: none
GitHub Issue: #4 - https://github.com/jeonghanlee/snmp3/issues/4
Status: Blocked

##### Summary

Problem 1 and cause B. `maxVarbinds` is a fixed per-profile value, the session does not advertise a maximum message size, and a tooBig reply is a `ProtocolFailure` without a product response. Net-SNMP sets the SNMPv3 header field `msgMaxSize` from the session `rcvMsgMaxSize` (minimum 484); with no setting the library advertises no limit, and the NMC2 card then returns malformed replies that the library drops. This unit advertises a configured size, splits the batch of an address once per tooBig reply down to a floor, and reports a tooBig at the floor as the record alarm message.

##### Scope

- Profile field `maxMessageSize` (default 1472, range 484 to 65507) set on the worker session.
- On `ProtocolFailure` with status tooBig, the scheduler halves the batch of that address for the retry, down to one varbind, and keeps the reduced batch until the worker restarts.
- Alarm message `tooBig` on the records of a batch that reaches the floor.
- Documentation of the field and the behavior in `snmp-rewrite-config.md`, `snmp-record-behavior.md` and `snmp-native-reference.md`.

Out of scope: GETBULK (M7), learning the agent limit across restarts, and changes to other error statuses.

##### Completion Criteria

- With `maxVarbinds=61` the NMC2 unit's records fill without manual tuning, and the split count per address is visible in `snmp3Report`.
- The fixture agent's tooBig cell produces the split and, at the floor, the `tooBig` message on the record.
- The NMC3 unit's behavior is unchanged at `maxVarbinds=50`.

##### Dependencies And Decisions

- G1 open; resume as Not started.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Add `maxMessageSize` to the configuration schema and the worker open path (`rcvMsgMaxSize`).
2. Add the per-address batch reduction in the scheduler on tooBig and expose it in the report.
3. Add the floor message in the request completion.
4. Extend `NativeTest` with the fixture agent's tooBig cell and `SchedulerTest` with the reduction.
5. Run both units with `maxVarbinds=61` and record the results.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Native | `NativeTest` against the fixture agent cell returning tooBig | Linux host, ordinary product | Split then completion; floor reports the message |
| T2 | Scheduler | `SchedulerTest` cases for the batch reduction and persistence | Linux host | Reduction per address as specified |
| T3 | Device | Both units at `maxVarbinds=61` through the laboratory IOC | Laboratory IOC, both units | NMC2 fills after splits; NMC3 unchanged; no ScopedPDU error in the worker log |
| T4 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` | Linux host | All cells PASS |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Linux host | Pending | none |
| T3 | Not run | Laboratory IOC, both units | Pending | none |
| T4 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Advertise a message size and split the batch on tooBig
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:14Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M4 - SET outcome contract

Origin: 1.0.0 / M4
Identity History: none
GitHub Issue: #5 - https://github.com/jeonghanlee/snmp3/issues/5
Status: Not started

##### Summary

Item 9 of cause B. The legacy module reported a SET as successful before sending it and dropped a timed-out SET without alarm. snmp3 has output device support for `ao`, `longout`, `int64out`, `stringout` and `lso`, but the accepted scope of `0.0.1` is GET-only. This unit qualifies the SET path: an output record completes only when the SET result arrives, a timeout or error is an alarm with its message, and consecutive puts keep their order per record.

##### Scope

- Output record completion on the SET result through the same outcome path as M3.
- Alarm messages for SET timeout, tooBig, and error status with index.
- Readback agreement: the written value equals the next GET of the same OID on the fixture agent.

Out of scope: write to a laboratory unit's outlet; the deferred long active client-write cells (M13).

##### Completion Criteria

- `RecordTest` cases for each output record type show completion on result, alarm on timeout and error, and order of consecutive puts.
- The fixture agent shows the written value on the next GET.

##### Dependencies And Decisions

- M3 must be Complete; the SET path uses the same outcome and message additions.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Review the output completion path in `Request.cpp` and `DeviceSupport.cpp` against the criteria.
2. Add the missing messages and tests; change the product only where a criterion fails.
3. Document the SET contract in `snmp-record-behavior.md`.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Record | `RecordTest` SET cases per output record type against the fixture agent | Linux host, ordinary product | Completion on result; alarms as specified |
| T2 | Readback | GET after SET on the fixture agent | Linux host | Value equals the written value |
| T3 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` | Linux host | All cells PASS |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Linux host | Pending | none |
| T3 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Complete output records on the SET result
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:17Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M5 - Reply time stamp on records

Origin: 1.0.0 / M5
Identity History: none
GitHub Issue: #6 - https://github.com/jeonghanlee/snmp3/issues/6
Status: Not started

##### Summary

Problems 4 and 5, cause C. A record's TIME is its processing time, so a client cannot tell when the value was read from the device. Base 7.0.10 lets device support set the time stamp when `TSE=-2`. This unit carries the worker's receive time of each reply through the IPC result and writes it into the record when `TSE=-2`; `TSE=0` records keep Base behavior.

##### Scope

- Receive time (monotonic and wall clock) in the native result and the IPC result frame.
- Device support writes `prec->time` from the wall-clock receive time when `TSE=-2`.
- Documentation of the field in `snmp-record-behavior.md` and `snmp-native-reference.md`.

Out of scope: `I/O Intr` (M6), UTAG, and time synchronization of the IOC host.

##### Completion Criteria

- With `TSE=-2`, record TIME equals the worker receive time recorded in the result within the IPC transfer latency, for every input record type.
- With `TSE=0`, TIME is unchanged from `0.0.1` behavior.

##### Dependencies And Decisions

- none

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Add the receive time to the native result and the result frame; bump the frame layout version.
2. Write `prec->time` in the completion when `TSE=-2`.
3. Add `RecordTest` cases and an `IpcTest` case for the frame field.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | IPC | `IpcTest` round trip of the receive time | Linux host | Field preserved |
| T2 | Record | `RecordTest` with `TSE=-2` and `TSE=0` per input record type against the fixture agent | Linux host, ordinary product | TIME as specified |
| T3 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` | Linux host | All cells PASS |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host | Pending | none |
| T2 | Not run | Linux host, ordinary product | Pending | none |
| T3 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Stamp records with the reply arrival time
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:20Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M6 - Reply-driven processing and driver-owned periodic acquisition

Origin: 1.0.0 / M6
Identity History: none
GitHub Issue: #7 - https://github.com/jeonghanlee/snmp3/issues/7
Status: Not started

##### Summary

Problems 4 and 5, cause C. In `0.0.1` a record acquires only when it processes, so SCAN sets the acquisition period and processing precedes the value. Base 7.0.10 offers `SCAN=I/O Intr` with `scanIoRequest` and `scanIoSetComplete`. This unit lets a binding declare a period; the module issues the periodic request itself and processes the record when the reply arrives, so the record carries the reply value and time (M5) and SCAN no longer drives acquisition for those records.

##### Scope

- Binding field `period_ms` for input bindings used by `SCAN=I/O Intr` records; `get_ioint_info` in the input DSETs.
- A per-address periodic requester in the scheduler honoring the existing deadline, queue and batch rules (M3).
- `scanIoRequest` on completion and `scanIoSetComplete` to learn when processing finished before the next request.
- Documentation in `snmp-record-behavior.md` and `snmp-rewrite-config.md`.

Out of scope: output records, event-driven SNMP traps, and changing the behavior of `SCAN` periodic records.

##### Completion Criteria

- An `I/O Intr` record processes once per reply at the configured period within one deadline of jitter on the fixture agent, with TIME equal to the receive time.
- A disconnected address stops processing its `I/O Intr` records and the records show the alarm; reconnection resumes without restart.
- `SCAN` periodic records behave as in `0.0.1`.

##### Dependencies And Decisions

- M3 (batch rules) and M5 (receive time) must be Complete.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Add `period_ms` to the binding schema and `get_ioint_info` to the input DSETs.
2. Add the periodic requester and its completion-gated re-request in the scheduler.
3. Add `RecordTest` and `SchedulerTest` cases, including disconnection and reconnection on the fixture agent.
4. Run the laboratory IOC with one `I/O Intr` record per unit.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Record | `RecordTest` `I/O Intr` cases with the fixture agent, including stop and resume of the agent | Linux host, ordinary product | Processing per reply; alarm on loss; resume |
| T2 | Scheduler | `SchedulerTest` for the periodic requester under the deadline, queue and batch rules | Linux host | Rules preserved |
| T3 | Device | One `I/O Intr` record per unit on the laboratory IOC for 300 s | Laboratory IOC, both units | Period held within one deadline |
| T4 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` | Linux host | All cells PASS |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Linux host | Pending | none |
| T3 | Not run | Laboratory IOC, both units | Pending | none |
| T4 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Process records on reply arrival at a declared period
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:30Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M7 - GETBULK table reads

Origin: 1.0.0 / M7
Identity History: none
GitHub Issue: #8 - https://github.com/jeonghanlee/snmp3/issues/8
Status: Not started

##### Summary

A 24-outlet PDU exposes its outlets as table columns. `0.0.1` reads each OID with GET in batches of `maxVarbinds`. SNMPv2c and SNMPv3 offer GETBULK with non-repeaters and max-repetitions, so one request can return a column. This unit adds a table binding kind that the worker reads with GETBULK and maps onto the bound records by index.

##### Scope

- Binding kind for a table column with a base OID, first index, count and max-repetitions.
- Worker GETBULK request and response mapping with the existing response checks (position, tag, storage).
- Interaction with M3: a tooBig on GETBULK reduces max-repetitions for that address.

Out of scope: SNMPv1 profiles, sparse tables, and discovery of table size.

##### Completion Criteria

- The fixture agent's outlet table of 24 rows is read with one GETBULK where the advertised size allows and the values match the GET path.
- On both laboratory units the outlet column fills with the request count reported by `snmp3Report`.

##### Dependencies And Decisions

- M3 must be Complete; the response size and reduction rules are shared.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Add the table binding kind to the configuration schema and the worker request builder.
2. Map the response to the bound positions and reuse the response checks.
3. Add `NativeTest` and `ConfigTest` cases; run both units.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Native | `NativeTest` GETBULK against the fixture agent table | Linux host, ordinary product | Values equal the GET path |
| T2 | Device | Outlet column on both units | Laboratory IOC, both units | Column fills; request count as expected |
| T3 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` | Linux host | All cells PASS |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Laboratory IOC, both units | Pending | none |
| T3 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Read table columns with GETBULK
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:33Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M8 - SNMPv3 localized keys and secret procedure

Origin: 1.0.0 / M8
Identity History: none
GitHub Issue: #9 - https://github.com/jeonghanlee/snmp3/issues/9
Status: Blocked

##### Summary

Cause D. The module reads each passphrase from its own file owned by the IOC's effective user with no group or other permission bits, and the worker derives keys with `generate_Ku` and `netsnmp_extend_kul`. The passphrase bytes stay in worker memory, no procedure defines how the files are created, stored and rotated, and the permitted algorithms per card generation are not recorded. This unit retains only the localized keys in the worker, records the permitted algorithm sets, and documents the creation, storage and rotation procedure. Existing deployments keep passphrases in the legacy `ver3.txt` form; conversion is part of the procedure.

##### Scope

- Worker: after `generate_Ku` and localization, keep `securityAuthLocalKey` and `securityPrivLocalKey` only and zero the passphrase buffers.
- Permitted algorithm sets for NMC2 and NMC3 verified against the units.
- Procedure page: host location, creation from a legacy file, rotation (new process), repository and history exclusion, and the scans that verify it.

Out of scope: an external secret store, and changing the configuration-loader file checks.

##### Completion Criteria

- The worker memory holds no passphrase bytes after session open, shown by the worker's own report of zeroization and a test that inspects the process image on the fixture.
- Files created by the documented procedure reach both units with the strongest algorithm set each card accepts, recorded without addresses, user names or secrets.
- Tracked-tree and history scans of this repository and the apcpdu repository find no secret material, or findings have an explicit disposition.

##### Dependencies And Decisions

- G1 open; resume as Not started.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Retain localized keys only in the worker and zero passphrases; add the report line and a test.
2. Verify SHA-256 and AES-256 acceptance on the NMC3 unit and the strongest set on the NMC2 unit.
3. Write the procedure page and the legacy-file conversion; run the scans.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Worker | Process image inspection after open on the fixture agent | Linux host, ordinary product | No passphrase bytes |
| T2 | Device | Files created by the procedure, each security level and algorithm set, on both units | Laboratory IOC, both units | Accepted sets recorded |
| T3 | Tracked tree and history | Secret scan of this repository and the apcpdu repository | Linux host | No secret material or disposition recorded |
| T4 | Permissions | Existing wrong-owner and wrong-mode configuration cases | Linux host | Rejected as before |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Laboratory IOC, both units | Pending | none |
| T3 | Not run | Linux host | Pending | none |
| T4 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Keep only localized SNMPv3 keys and document secret handling
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:35Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

#### M9 - Shared-library SONAME and symbolic-link versioning

Origin: 1.0.0 / M9
Identity History: none
GitHub Issue: #1 - https://github.com/jeonghanlee/snmp3/issues/1
Status: Not started

##### Summary

Give the snmp3 shared libraries on Linux explicit ABI-versioned SONAMEs and reproducible symbolic-link installation. Distinguish the real versioned file, the runtime SONAME and the unversioned consumer link name. `1.0.0` asserts a stable library interface, so this work precedes the release (D3).

##### Scope

- Inventory `snmp3`, `snmp3Wire`, `snmp3Native`, their consumers and retained compatibility names; define the stable ABI boundaries.
- Define compatible and incompatible SONAME changes independently of project release numbers.
- Implement versioned output and symbolic-link creation through the EPICS build and install, with `libname.so -> libname.so.<ABI> -> libname.so.<full-version>` for the selected policy.
- Verify installation, upgrades and loading for the IOC, native probe and worker, including actual loaded-library identities.

Out of scope: a policy for all EPICS modules, and device behavior.

##### Completion Criteria

- Each library has an ABI policy, intended ELF SONAME, versioned real file and correct links after normal build and install.
- Actual consumers record the intended SONAME dependencies and load the installed libraries; real IOC and worker paths succeed.
- A compatible replacement preserves an already linked consumer. An incompatible ABI uses a distinct SONAME and does not silently replace the old ABI.
- Repeated installation preserves valid links and targets. Migration from unversioned libraries is documented and tested.

##### Dependencies And Decisions

- D3 includes this work in `1.0.0`. Inventory precedes selecting an ABI number.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Inventory exports, consumer dependencies, names and applicable EPICS build and install rules.
2. Propose compatibility and version rules and migration; obtain plan acceptance and implementation authority.
3. Apply the selected build and install policy, consumer identity checks and documentation.
4. Execute actual build, install, consumer and upgrade checks.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Build and install | Build and install actual libraries, inspect ELF SONAME and NEEDED, real files and links, then repeat installation | Debian 13 and Rocky Linux 8.10 build hosts | Names, dependencies and links match policy without dangling targets |
| T2 | Consumer runtime | Real installed IOC, probe and worker against the fixture agent; inspect loaded-library identities | Debian 13 and Rocky Linux 8.10 runtime hosts | Intended libraries load; acquisition and shutdown succeed |
| T3 | Upgrade and migration | Already linked consumer after compatible replacement; distinct incompatible SONAME; unversioned migration | Isolated installed Linux trees | Compatible consumers work; incompatible ABIs remain distinct; migration succeeds |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Debian 13 and Rocky Linux 8.10 build hosts | Pending | none |
| T2 | Not run | Debian 13 and Rocky Linux 8.10 runtime hosts | Pending | none |
| T3 | Not run | Isolated installed Linux trees | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Version SNMP3 shared libraries with ABI SONAMEs
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:39Z; milestone and assignee jeonglee verified with `gh issue list` after the milestone link

#### M10 - Release 1.0.0

Origin: 1.0.0 / M10
Identity History: none
GitHub Issue: #10 - https://github.com/jeonghanlee/snmp3/issues/10
Status: Not started

##### Summary

Release `1.0.0` when every guarantee of the five causes reads Implemented and the combined product is verified on both laboratory units. The release carries the changelog of M1 to M9, the annotated tag and the GitHub release. The operations deployment is a separate decision after the release.

##### Scope

- Integrated re-runs of the checks that later units may have invalidated, on the final combined candidate.
- Two-unit acceptance on the laboratory IOC with the final candidate: M2's three phases, M3's sizing, M6's `I/O Intr` cadence and M8's credentials together.
- Installation on a clean Rocky Linux 8.10 host by the documented procedure.
- Version values, tag, GitHub release, and the next register entry point.

Out of scope: operations deployment, performance acceptance (M12), the deferred cells (M13).

##### Completion Criteria

- Every Release Verification row below is Pass with evidence.
- Tag `1.0.0` and the GitHub release exist on the commit whose `CHANGELOG.md` entry is `1.0.0`.

##### Dependencies And Decisions

- M1 to M9 Complete.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Freeze the candidate commit; run the integrated re-runs (Release Verification 1 to 5) and the version pre-check (Release Verification 8).
2. Run the two-unit acceptance on the candidate (Release Verification 6).
3. Write the `1.0.0` changelog entry and verify it (Release Verification 9); execute Release Execution 1 to 5 under separate `git-workflow` authority.
4. Verify the released objects and the clean-host installation (Release Verification 10, Release Verification 7); then set the next entry point (Release Execution 6).

##### Integrated Verification

| Source Check | Re-run Trigger | Shared Surface | Release Verification Label | Expected Result | Result Evidence |
| --- | --- | --- | --- | --- | --- |
| M1 / T1 | any later change to `Config.cpp` or `Supervisor.cpp` | process spawn | Release Verification 1 | Lists identical | pending |
| M3 / T1, M3 / T2 | M4, M6, M7 | request outcome and batch rules | Release Verification 2 | Split and message as specified | pending |
| M5 / T2 | M6 | completion path | Release Verification 3 | TIME equals receive time | pending |
| M4 / T1 | M6, M7 | outcome path | Release Verification 4 | SET completion and alarms as specified | pending |
| M9 / T2 | any library change after M9 | installed library identity | Release Verification 5 | Intended libraries load | pending |

##### Production Environment Tests

| Release Verification Label | Timing | System | Version | Architecture | Deployment Path | Method | Expected Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Release Verification 6 | pre-change | Laboratory IOC host with both units | candidate | x86_64 | laboratory IOC startup | M2 three phases plus M3, M6 and M8 criteria on the candidate | All criteria met | pending |
| Release Verification 7 | post-release | Clean Rocky Linux 8.10 host | 1.0.0 | x86_64 | documented installation procedure | Install from the release and run the fixture startup | IOC starts and acquires from the fixture agent | pending |

##### Version Changes

| Field | File | Before | Planned After | Pre-check | Pre-check Label | Post-check | Post-check Label |
| --- | --- | --- | --- | --- | --- | --- | --- |
| latest changelog entry | `CHANGELOG.md` | `0.0.9` | `1.0.0` | read the first entry heading | Release Verification 8 | read the first entry heading | Release Verification 9 |

##### Release Execution

| Step | Action | Authorization | Expected Result | Evidence |
| --- | --- | --- | --- | --- |
| 1 | Commit the `1.0.0` changelog entry | Commit/add scope or user-run | Commit on master | pending |
| 2 | Push master | Push scope or user-run | `origin/master` at the commit | pending |
| 3 | Annotated tag `1.0.0` on that commit | Tag create scope or user-run | Tag object | pending |
| 4 | Push the tag | Tag push scope or user-run | Tag on origin | pending |
| 5 | `gh release create 1.0.0 --notes-file` with the changelog entry | user-run | Release page | pending |
| 6 | Set the next entry point of this register | documentation commit | Register points past `1.0.0` | pending |

##### Release Verification Plan

| Label | Layer | Timing | Method | Environment | Expected Result | Evidence Target |
| --- | --- | --- | --- | --- | --- | --- |
| Release Verification 1 | Configuration | pre-change | `ConfigTest` | Linux host | Lists identical | test receipt |
| Release Verification 2 | Native and scheduler | pre-change | `NativeTest`, `SchedulerTest` | Linux host | Split and message as specified | test receipts |
| Release Verification 3 | Record | pre-change | `RecordTest` `TSE=-2` | Linux host | TIME equals receive time | test receipt |
| Release Verification 4 | Record | pre-change | `RecordTest` SET cases | Linux host | Completion and alarms as specified | test receipt |
| Release Verification 5 | Installed libraries | pre-change | consumer identity inspection | Debian 13 and Rocky Linux 8.10 | Intended libraries load | inspection record |
| Release Verification 6 | Device acceptance | pre-change | laboratory two-unit run | Laboratory IOC | All criteria met | run record under `work/` summarized here |
| Release Verification 7 | Installation | post-release | documented installation on a clean host | Rocky Linux 8.10 | IOC acquires from the fixture agent | installation record |
| Release Verification 8 | Version | pre-change | read `CHANGELOG.md` first heading | repository | `0.0.9` | command output |
| Release Verification 9 | Version | post-change | read `CHANGELOG.md` first heading | repository | `1.0.0` | command output |
| Release Verification 10 | Release objects | post-release | `git rev-parse 1.0.0^{commit}` and `gh release view 1.0.0` | repository and GitHub | Tag and release on the changelog commit | command output |

##### Release Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| Release Verification 1 | Not run | Linux host | Pending | none |
| Release Verification 2 | Not run | Linux host | Pending | none |
| Release Verification 3 | Not run | Linux host | Pending | none |
| Release Verification 4 | Not run | Linux host | Pending | none |
| Release Verification 5 | Not run | Debian 13 and Rocky Linux 8.10 | Pending | none |
| Release Verification 6 | Not run | Laboratory IOC | Pending | none |
| Release Verification 7 | Not run | Rocky Linux 8.10 | Pending | none |
| Release Verification 8 | Not run | repository | Pending | none |
| Release Verification 9 | Not run | repository | Pending | none |
| Release Verification 10 | Not run | repository and GitHub | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Release 1.0.0
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-10T01:59Z; issue updatedAt 2026-11-10T01:58:38Z; title, label, assignee jeonghanlee and milestone verified with `gh issue list` after creation.

## Backlog

### Work

| Group | ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Tools | M11 | Negative-control harness outcome classification | Carry-forward | Open | No | D2 | Scope decided and selected outcomes classified by real execution or retained by dated decision; [detail](#m11---negative-control-harness-outcome-classification) |
| Performance | M12 | Predictable acquisition performance and second-client impact | Carry-forward | Deferred | No | D5 | Existing performance criteria met on their identified scope or changed by an explicit decision; [detail](#m12---predictable-acquisition-performance-and-second-client-impact) |
| Records | M13 | Deferred record verification cells | Carry-forward | Deferred | No | D6 | Each listed cell has a real-path receipt or a dated retirement; [detail](#m13---deferred-record-verification-cells) |

### Backlog Details

#### M11 - Negative-control harness outcome classification

Origin: 1.0.0 / M11
Identity History: none
GitHub Issue: none
Status: Open

##### Summary

`tests/rewrite/test_record_controls.py` qualifies a control only when its reference passes and the defective run fails without aborting. Forced-cleanup, child-bound, stop-queued and missing-event outcomes are classified by code reading only. The retained scope and observations are in the [prior register](https://github.com/jeonghanlee/snmp3/blob/ba6c558a1118c792e769aed79ded7581538c3888/docs/milestone-6f015f8.md#m1---negative-control-harness-outcome-classification).

##### Scope

- Decide whether qualification cells apply the same forced-cleanup rule as record cells.
- Decide how to record a cell when the 900 s `run_cell` bound ends the entire control run.
- Classify stop-queued, timeout, inconclusive, forced-kill and missing-event outcomes by execution.
- Decide whether to surface the trial failure line and SchedulerTest check names.

Out of scope: product changes and the stop-queued wait rule in `docs/snmp-worker-supervision.md`.

##### Completion Criteria

- A dated decision fixes which outcomes qualify, abort or are judged.
- Each active outcome is classified by an executed real-path run, or its absence is recorded by decision.

##### Dependencies And Decisions

- D2 places this work in Backlog; the 2026-10-04 hold remains in effect.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Discuss the scope items and record the decision before changing the harness.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Negative-control harness | Run the selected outcomes through `test_record_controls.py --d7-controls` on identified sanitizer products | Linux host, sanitizer products | Each selected outcome is classified as recorded |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, sanitizer products | Pending | none |

##### Closure Evidence

- none

#### M12 - Predictable acquisition performance and second-client impact

Origin: 1.0.0 / M12
Identity History: none
GitHub Issue: none
Status: Deferred

##### Summary

Remove avoidable software delay and serial-request cost while preserving request ownership and avoiding unnecessary delay to another client. Readiness wakeups and declared-group dispatch are implemented. The once result passed, the sustained normalized second-client ratio failed (1.98 against 1.25), and the baseline variation, lost probe and historical 103 ms difference remain unresolved. The committed criteria, test definitions and receipts are in the [prior register](https://github.com/jeonghanlee/snmp3/blob/ba6c558a1118c792e769aed79ded7581538c3888/docs/milestone-6f015f8.md#m2---predictable-acquisition-performance-and-second-client-impact).

##### Scope

- Preserve matched complete-pass comparison and the sustained schedule and second-client criteria as committed.
- Preserve request ownership, deadlines, joint failure and retirement behavior if any further product change is selected.

Out of scope: SET timing, other device models, changing Net-SNMP, an automatically selected cache redesign, new tools or repeated equipment measurements without a new decision.

##### Completion Criteria

- A new dated decision defines the concrete product question and bounded work before resumption.
- The committed complete-pass, sustained schedule and second-client criteria are met on their identified scope, or changed by an explicit decision.

##### Dependencies And Decisions

- D5 keeps execution Deferred. D2 places the work in Backlog.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-10-10, accepted readiness and grouping plan at `131842c`, with the later scope and execution decisions preserved in the committed performance evidence of the prior register
Implementation Authorization: historical 2026-10-10 authority is suspended by D5; new bounded direction and separate equipment authority are required before resumption
Superseded Plan Artifacts: none

1. Start with existing captures, identified tools and receipts; check input availability before proposing replay.
2. Define one remaining product judgment and its stop condition.
3. Obtain the resumption decision before product changes, tool changes or measurements.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Fixture throughput | Real module `--once` and direct multi-object pass as committed | Identified Linux products and external fixture | Ratios at 20 and 100 ms at most 1.5 |
| T2 | Device complete pass | Five module and five direct passes per required device class | Identified ordinary products and authorized equipment | Median module/direct at most 1.5 |
| T3 | Sustained load and another client | Real 900 s module and direct loads with fresh idle phases | Identified ordinary products and authorized equipment | Normalized second-client ratio at most 1.25 |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-13 | Ordinary `e812d4c`, grouped K28, loopback fixture | Passed at judged delays | Ratios 1.11 at 20 ms and 1.03 at 100 ms; detail in the prior register |
| T2 | 2026-11-06 | Ordinary `fff3ab4`, generated358 K28 | Passed on the measured unit | Detail in the prior register |
| T3 | 2026-11-06 | Ordinary `fff3ab4`, generated358 K28 | Failed | Ratio 1.98 against 1.25; detail in the prior register |

##### Closure Evidence

- none; D5 is a deferral, not completion or a performance exception.

#### M13 - Deferred record verification cells

Origin: 1.0.0 / M13
Identity History: none
GitHub Issue: none
Status: Deferred

##### Summary

Cells excluded from the accepted record qualification and still unexecuted: Channel Access signed-minimum int64 access, maximum-capacity `lsi`/`lso` `VAL$` access, `ao`/`longout` writes, long active client writes, delayed output-simulation mode changes, and isolated rebuild and reuse beyond the existing `rebuild` clause. Their definitions are in the [prior register](https://github.com/jeonghanlee/snmp3/blob/ba6c558a1118c792e769aed79ded7581538c3888/docs/milestone-6f015f8.md#m4---deferred-record-verification-cells). M4 of this register covers the SET outcome contract; a cell it executes is recorded there and retired here by decision.

##### Scope

- Each listed cell defined against shipped records, Channel Access and the fixture agent, with its failing check named.

Out of scope: other accepted record cells.

##### Completion Criteria

- Each listed cell has a real-path receipt on identified ordinary and instrumented products, or is retired by a dated decision.

##### Dependencies And Decisions

- D6 excludes these cells from the accepted record qualification. D2 places the work in Backlog.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Define each cell and name the failing check before execution.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Channel Access | Real IOC and Base CA clients: signed minima, maximum capacity and output writes | Linux host, ordinary and instrumented products | Values and alarms meet the retained conversion requirements |
| T2 | Active writes and simulation | Long active client writes and delayed output-simulation mode changes against a delayed SET | Linux host, ordinary and instrumented products | Completion, alarms and ownership meet retained record policy |
| T3 | Isolated rebuild | Rebuild isolated databases with delayed or abandoned completions, `lsi`/`lso` buffers and maximum-capacity payloads | Linux host, ordinary and instrumented products | No post-detach access; ownership released |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary and instrumented products | Deferred | none |
| T2 | Not run | Linux host, ordinary and instrumented products | Deferred | none |
| T3 | Not run | Linux host, ordinary and instrumented products | Deferred | none |

##### Closure Evidence

- none

## History

| Reset Date | Prior State Commit |
| --- | --- |
| 2026-11-09 | ba6c558a1118c792e769aed79ded7581538c3888 |
