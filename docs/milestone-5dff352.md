# snmp3 Work Register

Release line: unversioned snmp3 rewrite
Milestone index: 5dff352
Canonical path: `docs/milestone-5dff352.md`
Canonical branch or ref: `feature/snmp-base-7.0.10-rewrite`
Git upstream: none configured at initial creation
Remote tracker: none associated with this register
Recorded date: 2026-10-01
Source baseline: `5dff352b9e86abfca74a74c4adc4fc8c1e48079f`

Next session entry point: `docs/milestone-5dff352.md`, M6 (step 8 is done: the AMSG clause, the report counters, the deadline-queue measurements, the documentation, the T12 isolated rebuild case and the ordinary and instrumented matrix re-run on the sources of `2544819`, see M6 T12, T13 and T14). D13 (2026-10-05) disposes of the cells recorded as unexecuted in M6 Verification Results T1 to T12: execute them in order of risk, starting with shutdown and lifecycle, and defer the listed Channel Access and rebuild cells to Backlog M11, so M6 can close once the executed cells have receipts and Closure Evidence lists the deferred ones as waived. M5 is Complete as of 2026-10-05 (see its Closure Evidence). G5 recorded acceptance and implementation authorization of the D7-D9 revisions on 2026-10-03 for the text carried by `c065755d7677e168c6fadae560012b7d9b1dfc47`. Results recorded below qualify only their recorded versions; full M6 acceptance, record advertisement and closure remain pending.

## Scope

This register owns the R1-R8 rewrite checkpoints and the assigned mdBook documentation work. M1-M8 retain their original R1-R8 scope names; M9 is the documentation addition. M, G and D identifiers are local to this canonical document. Architecture and operator contracts remain in their existing documents.

Out of scope: changing implementation or installed dependencies through this documentation update, operating equipment, publishing a site, or changing Git history or remote state.

Complete means the identified checkpoint scope and its qualified verification are complete. It does not close later integration requirements. R6 implementation was authorized on 2026-10-01 (G1) for a plan now superseded by the D7-D9 revision; G5 accepted and authorized that revision on 2026-10-03; M5 implementation under it completed on 2026-10-05 and M6 implementation proceeds under it. R7-R8 and mdBook implementation have not started. Their Blocked status records the missing detailed-plan acceptance and separate implementation authorization; planning may proceed before those gates close. Ready is an execution dependency indicator, not implementation authorization.

Implemented source and executable fixtures are carried by `49d5c62feca95f12a13910b73b60667455773b7c`; architecture/operator documents by `81cdcddd4fb431cba55e5a7999c9f7c923a670e6`; Python cache exclusions by the source baseline above. The current identity is snmp3.

### Verification Boundary

Observed results below are retained executions, not new tests performed when this register was written. Current receipts are local JSON records under `work/name-change-20261001/`; they identify source, product and loaded-library hashes. Receipt digests below fix the selected records. The shipped [verification procedures](../tests/rewrite/README.md) provide the actual runners. Private receipts and this register are not intended as mdBook operator chapters.

The current configuration receipt records wrong-owner secret-file testing as NOT RUN: a differently owned regular fixture was unavailable. The earlier owner-assisted PASS is not transferred to renamed products. ASan/UBSan results cover the executed new code; dependencies are uninstrumented and leak checks are disabled. The standard-record fixture does not establish SNMP record I/O. The M6 results separately identify current record/FLNK/callback observations and unresolved matrix cells. APC hardware/consumer behavior, two-OS qualification, TSan and sustained resource acceptance remain later work.

## Milestone

### Work

| ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| M1 | R1: independent Base 7.0.10 foundation | Milestone | Complete | No | D1, D2 | Independent real IOC build/load and dependency audit; [detail](#m1---r1-independent-base-foundation) |
| M2 | R2: Base lifecycle and thread ownership | Milestone | Complete | No | M1 | Real pre-PINI start, stop, join and isolated reuse; [detail](#m2---r2-base-lifecycle) |
| M3 | R3: immutable configuration, Binding and Value | Milestone | Complete | No | M2 | Qualified configuration/security/ownership matrix with explicit NOT RUN; [detail](#m3---r3-immutable-configuration) |
| M4 | R4: native SNMP adapter | Milestone | Complete | No | M3 | Actual GET/SET, security, response and native lifetime tests; [detail](#m4---r4-native-snmp-adapter) |
| M5 | R5: address queues, IPC and worker supervision | Milestone | Complete | No | M3, M4, D3, D7, D9, D10, G5 | Actual isolated worker, deadline, recovery and IOC qualification, including the D7 per-handle bound and D9 containment/backoff rules; [detail](#m5---r5-address-worker-supervision) |
| M6 | R6: record DSET, conversion, completion and FLNK | Milestone | In progress | No | M1, M2, M3, M4, M5, D5, D6, D7, D8, D11, D12, D13, G1, G5 | Real record/wire order, values, alarms and shutdown verified; [detail](#m6---r6-record-integration) |
| M7 | R7: APC startup/DB migration and comparison | Milestone | Blocked | No | M6, D1, G2 | Actual v2c/v3, CA/PVA, comparator and nested startup error observations; [detail](#m7---r7-apc-migration-and-comparison) |
| M8 | R8: integrated two-OS and resource qualification | Milestone | Blocked | No | M6, M7, G3 | Full identified-source matrix and measured resource acceptance; [detail](#m8---r8-integrated-qualification) |
| M9 | mdBook documentation | Milestone | Blocked | No | M1, M2, M3, M4, M5, D4, G4 | Curated book builds and rendered navigation/links match current behavior; [detail](#m9---mdbook-documentation) |
| G1 | R6 detailed-plan acceptance and execution authorization | External gate | Complete | No | none | Explicit acceptance and separate authorization recorded; [detail](#g1---r6-authorization) |
| G2 | R7 detailed-plan acceptance and execution authorization | External gate | Open | No | none | Target, comparator and permitted equipment scope approved; [detail](#g2---r7-authorization) |
| G3 | R8 detailed-plan acceptance and execution authorization | External gate | Open | No | none | OS environments, instruments and resource acceptance policy approved; [detail](#g3---r8-authorization) |
| G4 | mdBook detailed-plan acceptance and execution authorization | External gate | Open | No | none | Layout, toolchain, build and publication boundary approved; [detail](#g4---mdbook-authorization) |
| G5 | D7-D9 M5/M6 plan revision acceptance and execution authorization | External gate | Complete | No | none | Revised M5 and M6 plans accepted and separately authorized; [detail](#g5---d7-d9-revision-authorization) |

### Decisions

| ID | Decision | Decision Date |
| --- | --- | --- |
| D1 | Use a new explicit configuration/record interface and deliberate APC DB/startup migration; retain the legacy implementation only as a comparator. | 2026-09-29 |
| D2 | Use snmp3 for the active module, libraries, executables, commands and fixtures. | 2026-10-01 |
| D3 | Batch only compatible contiguous FIFO requests with exactly equal absolute deadlines; different deadlines remain separate FIFO work. | 2026-09-30 |
| D4 | Assign mdBook documentation as a separate milestone, using the existing epics-ioc-runner and epicsarchiverap-maven examples as planning inputs. | 2026-10-01 |
| D5 | Use IEEE 754 roundTiesToEven for ao double-to-OpaqueFloat SET conversion; permit the resulting finite binary32 rounding without decimal pre-rounding. | 2026-10-01 |
| D6 | Reject record conversion precision loss and capacity overflow as errors, except the ao-to-OpaqueFloat rounding permitted by D5; preserve prior input data on failure and send no invalid SET. | 2026-10-01 |
| D7 | Admit one new generation for a handle whose previous generation is selected and consumed and awaits only native retirement; queue it behind that retirement in the per-address FIFO, keep the admission-origin deadline and per-generation charges, and key every Scheduler structure by full identity. Reopens M5. | 2026-10-03 |
| D8 | Report a generation that reaches its deadline before dispatch as COMM/INVALID with AMSG `deadline before send`. | 2026-10-03 |
| D9 | Do not contain a worker for an active generation already selected Complete or NativeFailure until its deadline plus 1000 ms without Retired; reset the per-address failure counter on a matched Retired frame. | 2026-10-03 |
| D10 | Tie the charged generation node to the Scheduler container's element type by a compile-time assertion in `Scheduler.cpp`; add no independent InventoryTest node-size comparison. | 2026-10-05 |
| D11 | Accept that no test pins two details of the never-sent Deadline message: that an unsent WorkerFailure result does not stage the message (no record case reaches an unsent WorkerFailure), and that the message reset stays in `Requests::admit` rather than moving into `prepare`. | 2026-10-05 |
| D12 | Accept that no test shows a nonzero `behindRetirement` in the `snmp3 queue:` line of `snmp3RuntimeReport`: the report of a record test process is printed when nothing is queued behind a retirement, so a copy that prints a constant 0 or `retirementPending` in that slot is not detected; the counter's meaning is pinned by the Scheduler and qualification tests. | 2026-10-05 |
| D13 | Execute the cells that M6 Verification Results T1 to T12 record as unexecuted, except the cells moved to Backlog M11 and deferred: the Channel Access variants of T2 to T4, the long active client writes of T7, the delayed output-simulation mode change of T8, and isolated rebuild or reuse of T12 beyond the D7 clause. The deferred cells are excluded from the M6 Completion Criteria and from Test Plan rows T2, T3, T4, T7, T8 and T12, and M6 Closure Evidence lists them as waived checks citing D13 and M11. Execute the rest in order of risk: shutdown, lifecycle, alarm, ordering and reprocess cells first, then input and conversion boundaries, delayed retirement and the stale-generation control. | 2026-10-05 |

### Milestone Details

#### M1 - R1 Independent Base Foundation

Origin: 5dff352 / M1
Identity History: none
GitHub Issue: none associated with this register
Status: Complete

##### Summary

An independent Base 7.0.10 IOC/support target excludes legacy production objects and native SNMP dependencies from its process.

##### Scope

Base version checking, build/load, registration, startup command error handling and source/object/link/loader identity. See the [module contract](snmp-rewrite-contract.md) and [architecture](snmp-rewrite-architecture.md).

Out of scope: SNMP record device support and full communication qualification.

##### Completion Criteria

- The real IOC builds and loads the identified independent source without a forbidden legacy/native dependency.

##### Dependencies And Decisions

- D1 and D2 define the explicit interface and current public identity.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: accepted independent foundation checkpoint, preserved in the module contract and current build/load evidence
Implementation Authorization: completed R1 scope only; no successor implementation authority
Superseded Plan Artifacts: none

1. Build the independent support library, DBD and IOC against Base 7.0.10.
2. Validate real registration, loaded identities and allowed dependencies.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | IOC integration | Shipped independence runner builds and launches the real IOC and inspects objects and loaded libraries. | Linux x86_64; Base 7.0.10 | Correct registration/process behavior and native/legacy independence. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01, retained execution | Linux x86_64; Base 7.0.10 | PASS, 27 checks | `work/name-change-20261001/regressions/lifecycle/independence/results.json`; SHA256 `60c65cf0b3add09d859dc6a9045125ec76d80cabc9a2d4be20b214bb9a621e50` |

##### Closure Evidence

- Current source is committed in `49d5c62`; actual source/product/library identity is retained by the T1 receipt. Later record-driven checks remain M6/M8 requirements.

#### M2 - R2 Base Lifecycle

Origin: 5dff352 / M2
Identity History: none
GitHub Issue: none associated with this register
Status: Complete

##### Summary

Base lifecycle hooks start module servicing before initial processing and join module threads before callback shutdown.

##### Scope

AfterFinishDevSup readiness, AtShutdown stop, cold/partial/repeated/concurrent stop, creation failure and isolated activation reuse. The [lifecycle decision](decisions/snmp-base-lifecycle.md) records the Base ownership boundary; later worker behavior is specified by M5.

Out of scope: record callback drain and shutdown FLNK.

##### Completion Criteria

- Real lifecycle observations establish hook order, retained failure and one join per created thread.

##### Dependencies And Decisions

- M1 provides the independent IOC.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: accepted lifecycle checkpoint and lifecycle ADR, qualified by current real IOC evidence
Implementation Authorization: completed R2 scope only
Superseded Plan Artifacts: none

1. Own readiness, state, stop events and joinable threads through Base APIs.
2. Exercise actual hooks and isolated database teardown with the shipped fixtures.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | IOC lifecycle | Shipped lifecycle runner observes actual PINI, stop/join, repeated shutdown and OS-limit creation failure paths. | Linux x86_64; Base 7.0.10 | Required start/stop order, failure preservation and reusable settled ownership. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01, retained execution | Linux x86_64; Base 7.0.10 | PASS, 38 checks | `work/name-change-20261001/regressions/lifecycle/results.json`; SHA256 `117b1d0772582e769d0167220d6aea162cd1bf5809d80d155f3c3becfded53ee` |

##### Closure Evidence

- Current lifecycle sources and fixtures are committed in `49d5c62`. The qualified checkpoint does not claim record callback completion.

#### M3 - R3 Immutable Configuration

Origin: 5dff352 / M3
Identity History: none
GitHub Issue: none associated with this register
Status: Complete

##### Summary

Strict owned JSON configuration supplies immutable profile, endpoint and Binding data, tagged owned Values and a separate native capability probe.

##### Scope

R3.1-R3.5: atomic configuration publication, security/secret validation, actual native capabilities, owned Binding/Value, freeze and IOC commands. See the [configuration reference](snmp-rewrite-config.md) and [startup decision](decisions/snmp-startup-configuration.md).

Out of scope: network sessions, record-field conversion and record-driven binding integration.

##### Completion Criteria

- Actual shipped configuration paths enforce schema/security/ownership bounds and immutable publication; unavailable privilege-dependent testing remains explicitly unqualified.

##### Dependencies And Decisions

- M2 supplies actual pre-PINI and stop boundaries.
- Current wrong-owner testing remains NOT RUN; the historical owner-assisted result is not evidence for current renamed products.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: approved R3.1-R3.5 plan, SHA256 `3503ab98cf653699cfacf1c131a942d92fd65f16acd462383c8ead443c593a0b`
Implementation Authorization: completed snmp3 R3 scope recorded in the retained R3 handoff
Superseded Plan Artifacts: none

1. Publish complete bounded owned configuration only after all validation succeeds.
2. Probe native capabilities in a separate reaped process and freeze owned bindings at the specified lifecycle boundaries.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Configuration and IOC integration | Shipped configuration runner exercises actual parser, capability child, Binding/Value and IOC freeze paths. | Linux x86_64; effective-UID-owned fixtures | Valid inputs publish; failures preserve the snapshot; diagnostics contain no secrets. |
| T2 | Filesystem security boundary | Actual configuration loader receives an independently owned regular secret file. | Privilege or owner-assisted fixture required | The real ownership check rejects the file. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01, retained execution | Linux x86_64; Base 7.0.10 | PASS, 667 checks with explicit NOT RUN | `work/name-change-20261001/regressions/results.json`; SHA256 `534645493dfde52d15625243e624d4df4b7de4263e2b3d5d862178051c73ae31` |
| T2 | Not run for current products | Independently owned regular fixture unavailable | NOT RUN | The same receipt records `V3.3_wrong_owner`; no current PASS is inferred. |

##### Closure Evidence

- Accepted configuration implementation and fixtures are committed in `49d5c62`, with operator/decision documents in `81cdcdd`. Completion retains the explicit current verification limit; M6 must exercise actual record binding/freeze.

#### M4 - R4 Native SNMP Adapter

Origin: 5dff352 / M4
Identity History: none
GitHub Issue: none associated with this register
Status: Complete

##### Summary

A separate native adapter owns Net-SNMP sessions, PDUs and callbacks and copies typed response data before native storage is released.

##### Scope

R4.1-R4.5: native single-owner sessions, discovery/security/context behavior, typed GET/SET, deadlines/retries, high-FD handling and separate native tests. See the [transport reference](snmp-native-transport.md) and [transport decision](decisions/snmp-native-transport.md).

Out of scope: IOC record device support, equipment changes and total process-memory qualification.

##### Completion Criteria

- Actual installed native APIs and the shipped adapter pass the declared communication, ownership, failure and targeted sanitizer paths.

##### Dependencies And Decisions

- M3 supplies owned configuration and native capability identities.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: approved R4.1-R4.5 plan, SHA256 `6a707681a94607ad520f3d14de99989e7802232b7d28c5a7d6c915789ec8e14a`
Implementation Authorization: recorded R4 execution authorization, 2026-09-30
Superseded Plan Artifacts: none

1. Use actual native session, key derivation, PDU and large-FD APIs through a single-owner adapter.
2. Exercise the real native/agent path and external faults, then document qualified behavior.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Native API | Actual native API preflight runner. | Linux x86_64; selected installed Net-SNMP | Capability, security, type and API behavior match the selected implementation. |
| T2 | Native integration | Shipped adapter communicates with actual loopback agents and external UDP faults. | Linux x86_64 | Exact typed results, retries, timeout and lifetime behavior. |
| T3 | Instrumented native integration | Separate shipped ASan/UBSan adapter product runs the real native matrix. | Instrumented new code; dependencies uninstrumented | No sanitizer diagnostics in the qualified execution. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01, retained execution | Linux x86_64 | PASS, 84 checks | `work/name-change-20261001/native-api/results.json`; SHA256 `a5e2a8977aa2146044f92bb41faf2233eea7e06fc960ef2cd484b215109d99b1` |
| T2 | 2026-10-01, retained execution | Linux x86_64 | PASS, 779 checks | `work/name-change-20261001/native-adapter/results.json`; SHA256 `fe849c9906f1c908640e38a833be021c9bf4d20e9aa09b46ea44179822f37922` |
| T3 | 2026-10-01, retained execution | Separate ASan/UBSan adapter | PASS, 779 checks | `work/name-change-20261001/native-sanitizer/results.json` and its identified product/library receipts |

##### Closure Evidence

- Current native sources and executable fixtures are committed in `49d5c62`, with native contracts in `81cdcdd`. Instrumentation does not qualify unchanged dependencies.

#### M5 - R5 Address Worker Supervision

Origin: 5dff352 / M5
Identity History: none
GitHub Issue: none associated with this register
Status: Complete (reopened by D7 on 2026-10-03; completed 2026-10-05)

##### Summary

Per-address workers isolate native blocking operations. IOC servicing owns bounded admission, framed IPC, absolute deadlines, recovery and exact child reaping.

##### Scope

R5.1-R5.6: checked IPC/value codec and identity, Scheduler, Supervisor/Runtime, separate production worker, actual fixtures and operator contracts. See the [worker reference](snmp-worker-supervision.md) and [supervision decision](decisions/snmp-worker-supervision.md).

Out of scope: record-driven admission/conversion/completion, APC deployment and full resource qualification.

##### Completion Criteria

- Actual component, worker/native, IOC, operator and targeted sanitizer paths establish bounds, isolation, terminal ownership, deadlines and conservative no-replay recovery.

##### Dependencies And Decisions

- M3 supplies immutable data; M4 supplies actual native behavior; D3 fixes exact-deadline batching.
- Count and charged bytes remain owned until terminal consumption and native retirement or closed-channel/exact-PID reap both complete.
- Ambiguous sent SET work cannot be implicitly replayed. Worker recovery and record completion remain separate responsibilities.
- D7 (2026-10-03) reopens M5: the rule "a handle has at most one unretired generation" becomes "at most one retirement-pending (consumed) generation plus at most one unconsumed generation per handle". Count/byte limits charge both. M6 qualification on the production CA path motivated the change.
- D9 (2026-10-03) changes deadline containment for already selected members and resets the failure counter on a matched Retired frame. The 4-launches-per-60 s cap is unchanged.
- Blocked by G5 from 2026-10-03 until G5 completed on 2026-10-03; resumed as Not started, and moved to In progress on 2026-10-03 when step 4 started. Steps 4-5 committed on 2026-10-04 (`98bfdfc`, `0b60abf`); qualification cases for T6 in `e99331c`. SchedulerTest cells cited by T5 and T7: `admission-behind-retirement` introduced in `7ffdf5a` with its current body from `98bfdfc`; `containment-grace` in `0b60abf`; `two-generation-lifecycle` and named-cell selection in `0c60fc4`; `forged-retirement` in `bd4dc0c`. The D7 control harness in `tests/rewrite/test_record_controls.py` was introduced in `bd4dc0c`, and its `uncharged-successor` control was added in `1b6333f` (harness SHA256 `274956d3...`, nine controls). The SchedulerTest cells `grace-outcomes`, `behind-classification` and `take-identity` and the controls `grace-native-failure`, `grace-all-outcomes`, `never-sent-overcount`, `behind-flag-always` and `take-without-identity` were added in `75abe64`.
- T7 retargeting decision, 2026-10-04: the storage-lookup control runs against the SchedulerTest cell `forged-retirement` instead of the qualification case `ipc-stale-behind`, because that case injects the forged frame immediately before the real one and cannot expose the lookup.
- D10 (2026-10-05) amends plan item 4 and T5: the charged generation node is tied to the container's element type by a compile-time assertion in `Scheduler.cpp`, and no independent InventoryTest node-size comparison is added, so the plan wording follows the shipped mechanism.
- The reclosure condition on the current-product executions of M6 step 7 was met on 2026-10-03/04 and is recorded under T5 (SchedulerTest admission cell) and T6 (near-deadline baseline).

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-10-03; D7/D9 revision accepted (G5), text carried by `c065755d7677e168c6fadae560012b7d9b1dfc47`
Implementation Authorization: 2026-10-03; implement steps 4-6 and qualify T1-T7 (G5)
Superseded Plan Artifacts: original R5 draft replaced by the approved corrected plan, SHA256 `f4209d83308b3518c70506a1dd0785276461ade07689bb584463ec98f45bbc38`, accepted with R5 execution authorization 2026-09-30; steps 1-3 below were implemented and qualified under it. Steps 4-6 are the D7/D9 revision.

1. Enforce checked frame, admission/storage/identity, FIFO and exact-deadline batch rules in the shipped components.
2. Own one worker per canonical address, verify actual native/executable identities, retain unsettled ownership and recover only after exact reap.
3. Qualify the real shipped process/native path under external faults and instrumented execution.
4. `Scheduler.{h,cpp}`: key generation storage, the address FIFO and the active batch list by (binding, generation); admit a new generation only when every existing generation of the handle is selected and consumed and at most one exists; `take` and `release` by exact identity, with `take` returning an empty view for any other state; `stop`, both `expire` loops, `reaped`, `workerLost`, `retired`, `matches`, `collect`, `earliestDeadline` and `snapshot` act on exact identities, and frame validation remains membership in the active batch; node-based storage keeps a borrowed terminal's address stable; `fixedGenerationBytes()` follows the new node type through a compile-time assertion that ties the charged node type to the container's element type (amended by D10); the terminal view carries whether the generation was sent; snapshot reports generations queued behind their own handle's retirement and cumulative counts of such admissions and of those ending in a never-sent Deadline.
5. D9: `Scheduler::expire` reports containment for an active member unselected at its deadline, or selected Complete or NativeFailure but unretired at its deadline plus 1000 ms; `Supervisor` resets the address failure counter on a matched Retired frame.
6. Update `docs/snmp-worker-supervision.md` and `docs/decisions/snmp-worker-supervision.md` with the per-handle bound, report classification, retention after a failed or incomplete stop (up to two charged generations per handle), queue sizing for two generations per handle and the D9 rules. Done 2026-10-04: both documents state the two-generation bound, the queued-generation rules, the then unprinted report counters (printed since 2026-10-05, see item 8), the sizing and retention rules and the D9 containment and failure-count rules; the statements were checked against `Scheduler.cpp`, `Supervisor.cpp` and `Runtime.cpp`.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Components | Actual IPC/Scheduler/Supervisor/Runtime test executables. | Normal Linux x86_64 products | Checked bounds, one terminal and joint ownership release. |
| T2 | Worker/native integration | Shipped qualification runner uses real processes and agents with outer-boundary faults. | Linux x86_64 | Peer isolation, exact FIFO/deadline behavior and no ambiguous SET replay. |
| T3 | IOC and operator | Actual IOC lifecycle and documented production commands. | Base 7.0.10 | Pre-PINI servicing, exact stop/join/reap and correct command behavior. |
| T4 | Instrumented integration | Separate module/wire/native/worker/IOC products run the actual matrices; live worker identity is observed. | ASan/UBSan new code; leaks disabled | No sanitizer diagnostics on the qualified paths. |
| T5 | D7 component cells | New SchedulerTest cells with count/byte headroom so the per-handle rule, not a limit, decides admission; a compile-time assertion in `Scheduler.cpp` ties the charged node type to the actual container element type (amended by D10). | Normal and instrumented component products | Admission while the previous generation is consumed and unretired gives `retirementPending=1`, `queued=1`, `count=2`, `bytes=q1+q2`; rejection while it is queued, active-unconsumed or borrowed and for a third generation; dispatch waits for retirement; `take`, `expire`, `stop` and `reaped` act on the correct generation. The current Scheduler fails the admission cell (executed in M6 step 7 and recorded here). Existing T1-T4 receipts are regressions only and do not discriminate D7. |
| T6 | D7/D9 real-worker integration | Shipped qualification runner: same-handle admission after a Deadline is consumed; queued generation dispatched only after reap and Ready in the next epoch; short-budget variant expires in queue without dispatch; stale-frame fault while the `generation+1` identity is really queued; containment grace for an already selected member; failure counter reset. | Linux x86_64 real worker/native/agent | Supervision event order for the queued generation: 6 (containment), 8/9 (SIGTERM/SIGKILL, when the worker does not exit on close), 7 (reap), 2 (launch), 3 (Ready), 13 (dispatch); stale frame rejected with event 16 (stale identity) and no early selection; no containment inside the grace, reported as k of N trials in which a selected member was inside the grace window (k=0 reported as not run); backoff restarts from 250 ms after a matched Retired. The current-product near-deadline k of N from M6 step 7 is recorded here as the baseline before D9. |
| T7 | D7 negative controls | Disposable defective copies, each run against the named cell that must fail. A copy with the per-handle bound removed must fail the third-generation rejection in the SchedulerTest cell admission-behind-retirement. A copy that leaves the second generation uncharged must fail the count and bytes assertion of that same cell. A copy whose lookups are keyed by binding only must fail the SchedulerTest cell two-generation-lifecycle and the qualification case behind-retirement. A copy that validates retirement frames by storage lookup must fail the SchedulerTest cell forged-retirement (retargeted from the qualification case ipc-stale-behind by owner decision of 2026-10-04, because that case injects the forged frame immediately before the real one and cannot expose the lookup). Added after the third-person review of the product code (2026-10-05): a copy that contains an answered NativeFailure member at its deadline, and a copy that gives every selected outcome the grace, must fail the SchedulerTest cell grace-outcomes; a copy that counts every queue expiry as never-sent, and a copy that marks every generation as behind a predecessor, must fail behind-classification; a copy whose `take` ignores the admission number must fail take-identity. Added after `baad1ee` for the D9 exclusivity clause: a copy that gives the grace also to a ChannelFailure member, one that gives it to a WorkerFailure member and one that gives it to a Stopping member must each fail the SchedulerTest cell grace-exclusion. Controls that need record cells are M6 T14 cells. | Separate products, never installed | Each named cell passes on the unmodified products and fails on its control; the failure is observed and retained. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01, retained execution; 2026-10-04 | Normal component products; revised D7/D9 products | PASS, 14 executions; re-run on the revised products PASS, 15 executions (the D7-discriminating Scheduler cells are recorded under T5) | `work/name-change-20261001/components/results.json`; revised: `work/r6-d9-components-20261004-023844/results.json` |
| T2 | 2026-10-01, retained execution; 2026-10-04 | Real worker/native/agent path; revised D7/D9 products | PASS, 26 cases; re-run on the revised products PASS, 28 cases including the two T6 cases | `work/name-change-20261001/qualification/results.json`; SHA256 `3bf4d00cd34f3ce2a78fac083b1e2e5d2581c5fe88a769df9dd27e6aa3d61de8`; individual case receipts identify executed products; revised: `work/r6-p009-qual-all-20261004-001951/results.json` |
| T3 | 2026-10-01, retained execution; 2026-10-04 | Actual IOC and production commands; revised D7/D9 products | PASS, 25 IOC and 15 operator checks; re-run on the revised products PASS, 25 supervisor/IOC checks including the 8 IOC checks and `snmp3RuntimeTest`, and the operator checks | `work/name-change-20261001/ioc/results.json`, `operator/results.json`; revised: `work/r6-d9-test_supervisor-ioc-20261004-014730/results.json`, `work/r6-d9-test_r5_operator-20261004-000357/results.json` |
| T4 | 2026-10-01, retained execution; 2026-10-04 | Separate instrumented products; revised D7/D9 products (ASan/UBSan, leak detection disabled; sources of `1100605`, build `work/r6-m5-t4-sanitizers-20261004-205058/`) | PASS; 26 qualification cases, component/IOC matrices and 6 live workers; re-run on the revised products PASS: 28 qualification cases, the 15 component executions, 25 supervisor/IOC checks and 6 live workers, with no sanitizer diagnostic in any retained stderr | `work/name-change-20261001/sanitizer/results.json`, `sanitizer-components/results.json`, `sanitizer-ioc/results.json`, `live-worker-ioc/spawn-audit.json`; revised, under `work/r6-m5-t4-20261004-205058/`: `sanitizer/results.json`, `sanitizer-components/results.json`, `sanitizer-ioc/results.json`, `live-worker-ioc/spawn-audit.json` (the audit script is kept in that directory); each run retains its own product identities |
| T5 | 2026-10-03, products before D7 (negative control); 2026-10-04 | Normal component products before and after the D7 change; instrumented component products after it | Before D7: FAIL as expected; the 89 existing Scheduler checks and the new cell's 4 preconditions pass and its admission check fails (`admitted:false`). Revised products: PASS; the admission cell admits behind retirement (`retirementPending=1`, `queued=1`, `count=2`, `bytes=q1+q2`, counters 1), rejects an unconsumed or borrowed predecessor and a third generation, the grace cell holds an answered member until one second after its deadline, and with a retirement-pending predecessor and a queued successor `expire` and `stop` act on the successor as never-sent work while `reaped` retires only the predecessor; 151 Scheduler checks. The charged generation node is tied to the container type at compile time. Instrumented component products (ASan/UBSan, sources of `1100605`): PASS, 164 Scheduler checks and the inventory cell. After the third-person review of the product code, 2026-10-05, the cells `grace-outcomes` (NativeFailure answered member held through the grace, a late result contained without grace), `behind-classification` (never-sent counters count only generations behind a predecessor) and `take-identity` (a foreign admission number or generation borrows nothing) were added in `75abe64` (product sources unchanged from `1100605`): 197 Scheduler checks PASS on ordinary and on instrumented component products. Added after `baad1ee`, the cell `grace-exclusion` (a ChannelFailure, WorkerFailure or Stopping member is contained at its deadline, without grace) brings the run to 212 Scheduler checks PASS on ordinary and on instrumented component products (`work/r6-d9excl-components-ordinary-20261005-003623/results.json`, `work/r6-d9excl-components-sanitizer-20261005-003623/results.json`) | Before: `work/r6-scheduler-head-control-20261003-225831/results.json`; revised: `work/r6-d9-components-20261004-023844/results.json`; instrumented: `work/r6-m5-t4-20261004-205058/sanitizer-components/results.json`; with the three added cells: `work/r6-p026-components-ordinary-20261004-235719/results.json`, `work/r6-p026-components-sanitizer-20261004-235719/results.json` |
| T6 | 2026-10-04 | Actual snmp3RecordTest and qualification worker/native paths; near-deadline: outer UDP delay 300 ms, record budgets 314-322 ms, one fresh process per trial | Baseline on the products before D7 with the committed test: in 7 of 10 trials the result was accepted one service step before the deadline (record NO_ALARM) and the worker was still contained; 2 completed without containment and 1 ended Deadline. Revised products: PASS. Near-deadline: Retired arrived after the deadline in 10 of 10 trials; in the 9 trials answered before the deadline no worker was contained, and the remaining trial's response came after its deadline and ended Deadline with containment. Behind-retirement case: with the worker stopped, the successor admitted behind the Deadline predecessor was dispatched only after reap, relaunch 250 ms later and Ready in epoch 2; a 200 ms successor expired unsent (counters admitted 2, never sent 1); the second containment relaunched after 250 ms, the first backoff step, because the matched Retired reset the failure count. Stale-frame case: forged Result/Retired frames carrying the queued successor's identity arrived while it was queued in 5 of 5 trials, were rejected (event 16, stale identity) without selecting the successor early, and every generation completed once | Baseline: `work/r6-headc-near-deadline-20261004-014819/results.json`; revised: `work/r6-d9-near-20261004-000508/results.json`, `work/r6-p009-qual-all-20261004-001951/results.json` |
| T7 | 2026-10-04 | Disposable defective copies of `Scheduler.cpp` (product sources as committed in `bd4dc0c`) compiled with the compiler arguments of the sanitizer build `work/r6-p010-sanitizers-20261004-152351/`; the shipped SchedulerTest and qualification products built unmodified from the test sources, on the real worker/native/agent path | PASS; each named cell passed on the unmodified products and failed on its control, with the defective support library resolved by the loader. Per-handle bound removed: `admission-behind-retirement` fails at its 9th check, the third-generation rejection. Second generation left uncharged: the same cell fails at its 6th check, `count==2` and `bytes==2*first.bytes`. Charge released at consumption instead of at consumption and retirement (an additional control): the same cell fails at its 4th check, `count==1` and `retirementPending==1` after the first generation is consumed. Lookups keyed by binding only: `two-generation-lifecycle` fails at its 6th check, and the qualification case `behind-retirement` fails `real-qualification-complete`, `successor-dispatched-after-reap-relaunch-ready`, `short-successor-expired-unsent` and `backoff-restarted-after-matched-retired`. Frame validation by storage lookup: the SchedulerTest cell `forged-retirement` fails at its 5th check. Added controls: answered NativeFailure member contained at its deadline fails `grace-outcomes` at its 2nd check; every selected outcome given the grace fails it at its 7th check; every queue expiry counted as never-sent and every generation marked behind a predecessor fail `behind-classification` at its 3rd check; `take` ignoring the admission number fails `take-identity` at its 2nd check. Added after `baad1ee`: the grace given also to a ChannelFailure, a WorkerFailure or a Stopping member fails `grace-exclusion` at its 1st, 6th and 11th check. SchedulerTest reports only the failing check's position, counted from the start of the cell | `work/r6-p010-controls-20261004-191025/results.json` for the nine controls of that run (harness SHA256 `274956d3...`); all fourteen controls including the five added ones: `work/r6-p026-controls-20261004-235719/results.json` (harness SHA256 `f976772b...`, sanitizer build `work/r6-p026-sanitizers-20261004-235719/`); all seventeen including the three grace-exclusion controls: `work/r6-d9excl-controls-20261005-003623/results.json` (harness SHA256 `f4d5ad6b...`, sanitizer build `work/r6-d9excl-sanitizers-20261005-003623/`); in each run `complete` and `passed` are true and each control directory holds the mutated source, its digests and the cell output |

##### Closure Evidence

- Accepted runtime/tests are committed in `49d5c62`; worker/operator/decision documents in `81cdcdd`. That evidence established the R5 component boundary for steps 1-3 and preserves the explicit verification limits above.
- Reopened 2026-10-03 by D7; closure requires steps 4-6, T5-T7 and re-run T1-T4 on the revised products. Steps 4-6 are done and T1-T7 pass on the revised products, T4 and T5 including the instrumented products of `1100605`.
- Third-person review of the D7/D9 product code (`98bfdfc`, `0b60abf`), 2026-10-04: no product-code defect and no regression of the R5 invariants; its three findings about what the tests pin were resolved on 2026-10-05 by the added SchedulerTest cells and D7 controls (T5, T7) and D10.
- Closed 2026-10-05: the deliverable (steps 1-6), T1-T7 on the revised products including the instrumented T4 and T5, the third-person review and G5 are complete. Landing: after `git fetch` at 2026-10-05 00:15 (local time), `origin/feature/snmp-base-7.0.10-rewrite` equals `HEAD` `75abe64`, and the committed M5 product, test and document paths show no difference from it. Carrying commits: product `98bfdfc`, `0b60abf`; qualification cases `e99331c`; Scheduler cells `0c60fc4`, `bd4dc0c`, `75abe64`; control harness `bd4dc0c`, `1b6333f`, `75abe64`; documents `1100605`; this closure record is carried by `470e731`.

#### M6 - R6 Record Integration

Origin: 5dff352 / M6
Identity History: none
GitHub Issue: none associated with this register
Status: In progress

##### Summary

Provide dependable record I/O through the owned scheduler/native path. Preserve the requested SET value, report unusable results as alarms, and complete each asynchronous record exactly once through Base record support, including FLNK and shutdown.

##### Scope

Inputs: ai, longin, int64in, stringin, lsi and waveform. Outputs: ao, longout, int64out, stringout and lso. Add immutable record bindings, checked conversions, record-driven admission, terminal delivery, callback accounting and safe record-context teardown. Qualify the complete record/DSET/Runtime/Scheduler/IPC/worker/native/agent path on the current Base 7.0.10 installation.

Out of scope: legacy syntax adapters, manual FLNK execution, custom native security/crypto, installed Base changes, new record types, I/O Intr scanning, automatic SET readback, APC deployment, two-OS/sustained resource acceptance, mdBook implementation, installation, equipment operations and Git/remote mutations.

##### Completion Criteria

- Every advertised record/type combination has actual record/native integration evidence, including its range, precision, capacity and rejection boundaries. No partial value publication or silent loss is advertised without an accepted policy.
- Record/wire observations identify configuration revision, activation, binding, generation and admission, and prove request, terminal response/error, locked value/alarm processing, Base processing/FLNK and PACT clearing. Duplicate/stale delivery cannot mutate a newer record activation.
- SET admission owns the exact first-pass output payload. Completion never substitutes a newer VAL or claims a SET response is a fresh GET. Same-value explicit retry and writes while active follow the documented Base behavior.
- Queue pressure, delayed native retirement, callback saturation and stop preserve full M5 reservations and result ownership until consumption and retirement both finish. No uncharged result queue is introduced.
- Shutdown completes accepted records before links close when Base callbacks can progress. Interrupted drain is explicitly unsuccessful: close new record-processing entry, wait for already entered processing to finish before link closure, and retain pending callback/context ownership through actual Base join/queue cleanup. The attempt budget is not a guarantee of total shutdown duration.
- All required Test Plan rows have identified real-path receipts or explicit unresolved results. A required NOT RUN or external callback limitation does not become a PASS or a Complete milestone.
- The cells that D13 (2026-10-05) moves to Backlog M11 are excluded from the criteria above and from Test Plan rows T2, T3, T4, T7, T8 and T12; Closure Evidence lists them as waived checks citing D13 and M11.

##### Dependencies And Decisions

- M1-M5 are behavioral dependencies: the new DSET must use their configuration ownership, admission-origin deadlines, native adapter, worker isolation and retirement rules. Their existing receipts do not qualify record I/O.
- G1 closed on 2026-10-01 after separate plan acceptance and implementation authorization for the plan now listed under Superseded Plan Artifacts. G5 accepted and authorized the D7-D9 revision on 2026-10-03; M6 was Blocked from 2026-10-03 until then and resumed as In progress. Work on the remaining T1-T14 cells proceeds under the revised plan. Required real record qualification remains pending.
- D1 requires an explicit new record/configuration interface; D2 fixes the snmp3 product identity. D3 remains unchanged: equal configured budgets do not imply equal absolute deadlines or permit a new batching rule.
- Record-scope decision, 2026-10-01: include Base 7.0.10 int64in/int64out alongside the existing seven record types. This adds exact signed 64-bit scalar I/O. D5/D6 separately define the conversion policies; implementation authority for the superseded plan was recorded in G1; the current revision was accepted and authorized through G5.
- Long-string scope decision, 2026-10-01: add Base 7.0.10 lsi/lso for bounded long-string input/output, including SIZV/LEN handling and full-length client access. Keep stringin/stringout for the existing short-string interface. D6 defines capacity-error handling; implementation authority for the superseded plan was recorded in G1; the current revision was accepted and authorized through G5.
- D5 resolves ao double-to-OpaqueFloat SET rounding as IEEE 754 roundTiesToEven, dated 2026-10-01. This decision selects one conversion policy; it is not acceptance or execution authorization for the complete M6 plan.
- D6 resolves the remaining conversion precision/capacity-loss policy as error rejection, dated 2026-10-01. Except for D5, do not round, truncate, wrap or saturate an otherwise unusable record conversion. Preserve prior input data and send no invalid SET. Integer-to-floating input loss, waveform FLOAT narrowing and string/array overflow are covered. No conversion-loss policy choice remains pending; complete-plan acceptance and implementation authorization for the superseded plan were recorded in G1; the current revision was accepted and authorized through G5.
- Waveform state decision, 2026-10-01: follow Base's device-support-owned BUSY contract. snmp3 manages BUSY as FALSE for complete single-varbind publication; do not introduce an initial-BUSY rejection policy. Complete-plan acceptance and implementation authorization for the superseded plan were recorded in G1; the current revision was accepted and authorized through G5.
- Interface spelling, conversion matrix, alarm mapping and callback-drain behavior below are accepted implementation requirements, not claims of implemented or verified product behavior. A later plan revision requires new acceptance and implementation authorization.
- T11 scope decision, 2026-10-04: the D7 cell "worker reaped outside the stop bound" is removed from the required T11 cells. Producing it needs a worker that survives SIGKILL, which only an internal stand-in could provide; IncompleteStopped after an expired drain remains covered by the existing blocked-shutdown receipts.
- T14 wording decision, 2026-10-04: D7 defective-code controls alter `Scheduler.cpp` deliberately; the T14 method now requires the remaining spans to be unchanged except for that altered code.
- D7, D8 and D9 (2026-10-03) revise this plan. Production CA observation (`work/r6-ca-active-20261003-003058/`, repeated once with the same products) showed a Base RPRO reprocess rejected in five of five records: one SET per record on the wire and `completions=38` against 43 expected, so the latest requested value never reached the device. The cause is that the previous generation's native retirement trails its terminal by about one worker poll; the Result-to-Retired gap measured 9.6-10.8 ms on the M5 qualification harness with the current worker product, and the record-path gap is measured by the unheld case of step 7. D7 admits that reprocess behind the retirement instead. The revision depends on the reopened M5.
- D13 (2026-10-05) disposes of the unexecuted cells recorded in M6 Verification Results. Execute: T1 startup, live-field, periodic and concurrent-detach cells; T2 native exception/type-error and failure-after-success cells; T3 remaining structured/text boundaries; T4 remaining long-string boundaries; T5 native errorStatus/exception, failure after a valid GET, competing-alarm and monitor cells; T6 other record, failure and deadline chains, fanout and delayed or duplicate external responses; T7 scan and PROC reprocess routes; T8 a SIMM switch while an output SET is queued behind retirement on the normal and deadline paths; T9 delayed native retirement/retry and other-address progress; T10 full requirement review; T11 other Stopping, waveform and downstream shutdown states; T12 in-flight non-isolated retention, record startup failure and remaining live-detach cells; and the T14 stale-generation control (first check whether the D7 frame-validation controls recorded under M5 T7 already cover it: if they do, cite that receipt as the executed control; if not, build the control). Defer to Backlog M11, excluded from the Completion Criteria and Test Plan rows T2, T3, T4, T7, T8 and T12 and listed in M6 Closure Evidence as waived checks citing D13 and M11: the Channel Access variants of T2 (signed-minimum), T3 (maximum-capacity) and T4 (ao/longout and signed-minimum); the long active client writes of T7; the delayed output-simulation mode change of T8; and isolated rebuild or reuse of T12 beyond the D7 clause that the `rebuild` case closes, including delayed or abandoned queued completions, lsi/lso buffers and maximum-capacity payloads. Order: shutdown, lifecycle, alarm, ordering and reprocess cells, then input and conversion boundaries, then the rest, each designed, implemented, reviewed and committed on its own.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-10-03; D7-D9 revision accepted (G5), text carried by `c065755d7677e168c6fadae560012b7d9b1dfc47`
Implementation Authorization: 2026-10-03; implement steps 7-8 and the remaining T1-T14 qualification under the revised plan, retaining M6 exclusions (G5)
Superseded Plan Artifacts: M6 plan accepted 2026-10-01, reviewed document SHA256 `cb80ae8c978ad5b06e4ae046ca4ac648f5f29634c13e3e5709c6096f6c77eecb`, with implementation authorization 2026-10-01 for eleven-record DSET/conversion/completion/shutdown, shipped T1-T14 fixtures and required documentation; steps 1-6 were implemented under it. The D7-D9 revision below adds steps 7-8 and amends the marked clauses.

Plan date: 2026-10-01

###### Premise And Source Checks

Confirmed finding: `snmp3App/src/snmp3.dbd` registers a registrar only, and `Runtime.h` has no record completion API. `Scheduler.cpp` already provides `take()` and `release()` for a borrowed, owned terminal result. Therefore terminal-to-record delivery is a required new connection, not evidence that M5 is incomplete within its qualified scope.

Confirmed constraint: `Scheduler::admit()` accepts budgets of 1-600000 ms. `release()` marks consumption; collection still requires native retirement. Under D7 a record that has cleared PACT may admit one new generation while its previous generation awaits only retirement; the new generation queues behind that retirement in the per-address FIFO, so the DSET neither waits nor replays. Every Base reprocess route (RPRO, dbNotify restart, PROC, periodic scan, FLNK, direct dbProcess) needs the record lock, and the previous generation is consumed before that lock is released, so D7 applies to inputs and outputs on every route.

Base behavior inspected in the R7.0.10 record sources under `modules/database/src/std/rec/`: `aiRecord.c`, `process`, treats read status 2 as direct VAL; `aoRecord.c`, `process`, prepares OVAL and applies IVOA; `longoutRecord.c`, `conditional_write`, reevaluates OOPT on completion; `int64inRecord.c`, `process`, clears UDF only on successful read status; `int64outRecord.c`, `process`, prepares VAL, applies drive limits/IVOA and has no OOPT; `waveformRecord.c`, `process`, clears UDF without testing the read status before monitors/FLNK. The installed `longinRecord.dbd`/`longoutRecord.dbd` declare DBF_LONG, `stringinRecord.dbd`/`stringoutRecord.dbd` allocate 40-byte strings, and `waveformRecord.dbd` declares FTVL/NELM/NORD/BPTR. The pinned `int64inRecord.dbd.pod`/`int64outRecord.dbd.pod`, VAL definitions and Device Support Routines, declare DBF_INT64 and read/write routines using the signed 64-bit VAL; `menuFtype.dbd.pod`, Menu menuFtype, includes INT64 and UINT64. Record fields and conversion modes must be rechecked against the installation used for implementation.

The pinned `dbCommon.dbd.pod`, Scan Fields, says ordinary dbProcess suppresses processing while PACT is true and FLNK occurs before PACT clears. The actual Base `ioc/db/callback.c`, ProcessCallback, uses dbScanLock and the record's rset process directly. Its `callbackRequest()` returns failure when the queue is full; an ignored return can lose completion. The callback task's queue-drain loop and `callbackStop()` do not establish a bounded shutdown under continuously refilling external producers.

The actual Base `ioc/misc/iocInit.c`, iocShutdown, orders AtShutdown, link closure, scan stop, callback stop and isolated record free; doCloseLinks calls device-support del_record under the record lock. `ioc/db/dbAccess.c`, dbPutField, sends device-link writes through dbPutFieldLink but writes DTYP through ordinary dbPut, without the device-extension callback. `ioc/db/callback.c`, callbackCleanup, deletes callback queues after the threads are joined; join alone does not prove every queued entry ran. The active install is selected by `configure/RELEASE.local`; the source inspected is `/home/jeonglee/gitsrc/EPICS-env/epics-base-src/modules/database/src/`. These source checks establish a planning premise, not runtime verification of new DSET behavior.

The pinned `lsiRecord.dbd.pod`, Input Specification/Run-time Parameters/Device Support Interface, and `lsoRecord.dbd.pod`, Desired Output Parameters/Run-time Parameters/Device Support Interface, define pointer-backed VAL/OVAL, SIZV, LEN/OLEN and read_string/write_string DSET entries. In the actual `std/rec/lsiRecord.c`/`lsoRecord.c`, init_record clamps effective SIZV to 16-32767 bytes; special and lso's IVOA branch set LEN to strlen(VAL)+1, including the terminating NUL. lsi process does not clear UDF on normal device completion; the new DSET must do so on successful input. lso process honors OMSL/IVOA and has no OOPT. `ioc/db/dbChannel.c`, dbChannelCreate, exposes long string fields ending in `$` as CHAR arrays; record put_array_info can truncate oversized client input before the DSET sees it. The plan must distinguish Base/client conversion from the module's bounded SNMP conversion.

Hypotheses requiring execution: output records can preserve completion/error reporting when DSET is skipped in the second pass; module callback retry and shutdown can safely coexist with Base RPRO and isolated cleanup. T6-T12 must confirm these on the real IOC before support is advertised.

The pinned `waveformRecord.dbd.pod`, Run-time Parameters, describes BUSY as a device-support-owned state for sequential acquisition rather than a user configuration parameter. The actual `waveformRecord.c`, process, returns before readValue, monitors, FLNK and PACT clearing when PACT and BUSY are both TRUE. snmp3 must manage this state before invoking completion record support; clearing it only in the second DSET call would be too late.

###### Binding And Conversion Contract

Use `DTYP="snmp3"` with INST_IO INP/OUT text `@binding=<id> deadline_ms=<ms>`. Require exactly those two keys, one existing JSON binding ID and an integer budget in 1-600000 ms; reject missing, duplicate, unknown or trailing input. The budget is explicit and frozen per record, separate from native timeout/retries. The existing Runtime admission timestamp starts the absolute deadline; callback delay does not restart it or change an already selected terminal outcome.

Bind once during device initialization. The record context owns the immutable Binding snapshot, record type, operation, conversion policy, capacity, deadline budget, scheduler owner/activation and handle, plus the initialized DTYP/link/DSET identity. Multiple records may reference one JSON definition but receive separate handles. Check GET for inputs and SET for outputs. Resolve credentials/endpoint/OID only through Config; do not add legacy syntax, inline security values, or network calls during initialization.

Use device-support extensions to reject live INP/OUT replacement: del_record returns a nonzero refusal during ordinary operation and the callback-drain phase without detaching or releasing the existing context. Startup add_record validates eligibility, and init_record performs the single binding registration; snmp3 add_record refuses all post-initialization attachment attempts. AtShutdown establishes a detach phase only after the completion-entry gate is closed and entered processing has reached zero. In that phase del_record performs idempotent detach under the record lock, with no join/wait and no freeing of callback storage that Base may still reference. A concurrent link edit cannot create a new snmp3 binding/request in that phase; if DTYP selects another support, Base may attach that support through its own extension, which snmp3 does not control. An operator snmp3Stop alone does not permit snmp3 rebinding or establish IOC detach permission.

A direct DTYP field write can succeed in Base without calling these extensions. Do not claim that device support rejects the field write itself. Before each admission and completion, compare DTYP/link/DSET identity with the initialized snapshot under the record lock. A mismatch sets LINK/INVALID, admits no new native work, and prevents incoming data from being published through the changed binding. An already accepted generation still completes its owned error/retirement path, without admitting a replacement or freeing its context early. Restoring fields does not create a new binding; deliberate binding changes require a new initialized record activation.

The following matrix is a proposed advertised surface. Numeric tags mean Integer, Unsigned32, Counter32, Gauge32, TimeTicks, Counter64, OpaqueFloat and OpaqueDouble. A scalar GET remains one varbind; a waveform does not imply table walking or an array of numeric SNMP values.

| Record | Operation / native types | Record representation | Proposed checks |
| --- | --- | --- | --- |
| ai | GET / numeric tags | VAL double; direct-value completion status 2 | Finite value; exact integer-to-double conversion required by D6. No raw RVAL conversion mode. |
| longin | GET / integer tags | VAL signed 32-bit | Reject values outside INT32 range, including unsigned/counter overflow; no wrap or saturation. |
| int64in | GET / integer tags | VAL signed 64-bit | Preserve all Integer/Unsigned32/Counter32/Gauge32/TimeTicks values exactly; accept Counter64 only through INT64_MAX. No intermediate double conversion; reject larger unsigned values. |
| stringin | GET / Octets, ObjectId, IpAddress | VAL NUL-terminated byte string | Octets without embedded NUL; decimal dotted OID/IPv4 for structured tags; at most 39 data bytes, with overflow rejected under D6. |
| lsi | GET / Octets, ObjectId, IpAddress | VAL NUL-terminated buffer; LEN includes NUL | Same textual formatting and embedded-NUL checks as stringin; use effective frozen SIZV, at most SIZV-1 data bytes, with overflow rejected under D6; validate before publishing VAL/LEN and clear UDF only on success. |
| waveform | GET / Octets, IpAddress, ObjectId, numeric tags | UCHAR for octets/IPv4; ULONG for OID arcs; LONG, ULONG, INT64, UINT64, FLOAT or DOUBLE for one numeric scalar | Native tag/FTVL agreement; exact conversion and capacity checks under D6; NORD is byte/arc count or 1; no partial BPTR/NORD update. Device support manages BUSY as FALSE; PACT tracks asynchronous SNMP work. Reject other FTVL/type pairs. |
| ao | SET / numeric tags | Capture Base-prepared OVAL | Checked finite/range/integrality conversion to the configured native tag; D5 permits finite double-to-OpaqueFloat rounding using IEEE 754 roundTiesToEven. No raw RVAL mode. |
| longout | SET / integer tags | Capture VAL signed 32-bit | Reject negative-to-unsigned conversion; require OOPT Every Time at initialization and before admitting any later write. |
| int64out | SET / integer tags | Capture Base-prepared VAL signed 64-bit | Check the configured target range: INT32 for Integer, UINT32 for Unsigned32/Counter32/Gauge32/TimeTicks, and nonnegative signed-64 values for Counter64. No intermediate double conversion, wrap or saturation; honor Base drive limits/IVOA. No OOPT field. |
| stringout | SET / Octets | Capture VAL bytes before NUL | Require termination within the 40-byte record field and length within Binding capacity; transmit data bytes without the trailing NUL. |
| lso | SET / Octets | Capture Base-prepared VAL/LEN from the SIZV-sized buffer | Require LEN in 1-SIZV, matching the first terminating NUL; capture LEN-1 data bytes within Binding capacity, without the NUL. Honor OMSL/IVOA; no OOPT field. |

All exception tags, mismatched native types, nonfinite numbers, unsupported pairs, arithmetic overflow and malformed payloads fail explicitly. Use native Value accessors and existing IPC decoding; never reinterpret native library storage in the IOC. TimeTicks remain native tick counts, with no implicit seconds conversion. Structured string formatting is deterministic; binary octets belong in a UCHAR waveform.

D6 requires complete validation against both Binding capacity and initialized record storage (SIZV/NELM or the fixed short-string field), and exact numeric conversion except for D5. Validate before publishing any input value/length or admitting a SET. A failed GET conversion preserves VAL/BPTR and NORD/LEN as applicable and reports an error; follow the record-specific UDF rules below. A failed SET conversion reports an error without native admission or modifying the requested value. The proposed alarm mapping uses READ/INVALID and WRITE/INVALID for local conversion failures; existing native response-capacity failures retain their error path. Do not publish a prefix, silently discard bytes, clamp a number or add an implicit retry. Base/client conversions that occur before DSET entry remain the separate boundary documented below.

For ao SET to OpaqueFloat, convert the captured binary64 OVAL once to binary32 using IEEE 754 roundTiesToEven (D5): select the nearest representable value; an exact halfway value selects the candidate with a zero least-significant significand bit. Permit finite rounding, including correctly rounded subnormal values and signed zero; do not flush representable subnormals to zero or pre-round decimal digits. The existing proposed nonfinite/overflow checks still reject NaN, infinity and an overflowing conversion before SET admission. Preserve the requested record value separately from the captured rounded wire payload. OpaqueDouble uses the original finite double and does not pass through float.

Guarantee D5 independently of any ambient thread rounding mode and restore any temporarily changed thread floating-point state; do not change IOC-wide rounding behavior or assume an unchecked C++ cast guarantees the selected mode. T4 must establish the emitted binary32 bits on the real DSET/native path. The rule definition is corroborated by [Floating-point Expressions, IEEE 754 correspondence](https://docs.oracle.com/en/java/javase/26/docs/specs/jls/jls-15.html#jls-15.4); that source defines the rounding behavior, not the behavior of this module's future C++ implementation. The cable-cert-review NIST GLP 9 Option B decimal report-display policy is not used for this binary conversion.

Use int64in for exact Counter32/TimeTicks scalar input beyond INT32_MAX and for Counter64 values in 0-9223372036854775807. Counter64 values in 9223372036854775808-18446744073709551615 require the advertised numeric waveform with FTVL UINT64 and NELM at least 1 for exact input. int64in/int64out are signed; neither advertises the complete unsigned Counter64 range. Keep longin/longout for signed 32-bit compatibility rather than silently changing their representation. Exact-value tests use IOC database DBR_INT64/DBR_UINT64 access. CA has no signed/unsigned 64-bit integer wire type: qualify decimal DBR_STRING get/put separately and do not claim exact large integers through CA DOUBLE. The Base CA header `db_access.h` defines the available wire types; `ioc/db/dbConvert.c` performs the integer/string conversions. Conversion to a double by another component can still lose precision.

Freeze the initialized effective SIZV for lsi/lso and use checked byte counts, not character counts. With SIZV=256, the complete textual payload may contain at most 255 data bytes plus one NUL; with maximum effective SIZV=32767, the maximum is 32766 data bytes. A successful empty lsi string has LEN=1. Under D6, a failed lsi conversion preserves VAL/LEN/UDF; lso rejects missing termination, inconsistent LEN or an over-capacity captured payload before admission. Binary Octets with embedded NUL remain a waveform use case. Record the Base-owned VAL/OVAL allocations separately from charged module conversion/capture storage, which must be bounded by the initialized record/Binding capacities; no unbounded long-string allocation or second result queue is allowed.

Qualify complete long-string client access through `.VAL$` CHAR arrays and actual IOC database access, including strings longer than 39 bytes. Ordinary CA DBR_STRING access remains limited to its short wire representation. Base can truncate oversized client writes or DOL values before snmp3 receives the prepared VAL; do not claim that DSET validation detects the original client length or prevents every pre-DSET truncation. Record that boundary in T3/T4/T7/T8 and document the sender's capacity requirement. Completion consumes the captured lso payload and never rewrites a newer VAL/LEN; Base still owns OVAL/OLEN and monitor updates.

For ai/ao, support engineering values with LINR NO CONVERSION and document the applicable Base value adjustments; reject a claimed raw linear-conversion configuration rather than silently ignoring it. Test ao OROC/drive limits and OVAL selection on the actual Base code. Initialization returns must preserve the configured output value without claiming hardware readback. No initialization GET or SET is implicit; PINI/scanning can explicitly request I/O after servicing is ready.

Under D6, conversion validates the entire input before publishing its complete value/NORD/LEN. Failure preserves the previous value/NORD/LEN. For ai, longin and int64in, return the appropriate error status so Base preserves the prior UDF; stringin/lsi device support clears UDF only after a valid input and preserves it on failure. A never-successful ai/longin/int64in/stringin/lsi remains undefined.

These input preservation and native-success rules apply to SNMP DSET publication. Base input simulation remains a separate value source: the pinned `lsiRecord.dbd.pod`, Simulation Mode Parameters, defines SIOL input, and the actual `aiRecord.c`/`lsiRecord.c`/`waveformRecord.c`, readValue, can bypass DSET when SIMM changes during an active GET. Preserve Base simulation behavior; do not temporarily force SIMM off, restore a native value after Base processing, or present a simulated value as a successful SNMP publication. Base may change VAL/BPTR, NORD/LEN and UDF through SIOL; D6 does not claim to prevent those Base-owned changes. Qualify all six input records in T5/T8, including ai RAW simulation.

Waveform follows the unchanged Base record contract: processing can clear UDF even on failed input, before monitors and FLNK. Do not restore UDF after record processing or promise that downstream records observed it as TRUE. Report failure through the accepted alarm mapping, preserve BPTR/NORD, and retain a fixed context flag indicating whether any native input has succeeded for diagnostic reporting. Waveform UDF alone is not an indicator of successful SNMP input. T2/T3/T5 must observe this distinction both on an initial failure and after a valid input followed by failure.

For an initialized snmp3 waveform activation, device initialization sets BUSY to FALSE and the first-pass DSET keeps it FALSE. Do not use BUSY to represent a pending network request or expose sequential partial acquisition; use PACT and the owned generation instead. After validating the frozen identity and matching active generation under the record lock, the completion wrapper sets BUSY to FALSE before calling rset process, on success, error, deadline and Stopping paths, including a simulation bypass. This prevents Base's early BUSY return without manually executing FLNK or clearing PACT. No BUSY management occurs through an invalid/stale context or after detach. T1/T6/T8/T11 qualify this behavior; an initial nonzero BUSY is normalized by device initialization rather than treated as an unsupported user setting.

A SET completion reports the status of the captured payload and leaves the current requested VAL intact, including a newer value written while active.

Proposed alarm mapping: invalid binding/configuration uses LINK/INVALID; local admission or conversion failure uses READ/INVALID or WRITE/INVALID by operation; transport, deadline, worker loss and Stopping use COMM/INVALID; native/protocol/exception failures use READ/INVALID or WRITE/INVALID. A generation that reaches its deadline before dispatch reports COMM/INVALID with AMSG `deadline before send` (D8) so the alarm event posts even when the previous outcome was also COMM/INVALID. Apply alarms with recGblSetSevr or recGblSetSevrMsg, preserving Base severity priority and equal-severity behavior. Base remains responsible for record limit/UDF alarms and monitor events. Stage terminal failure alarms before invoking record support so IVOA cannot suppress the error report by skipping DSET.

###### Completion And Ownership Contract

First-pass DSET runs under the record lock, validates the frozen context, converts/captures the output if needed and attempts immediate admission. A rejection is synchronous: set the operation alarm and return an error with PACT left inactive. On success, publish the exact identity into the context, set PACT and return promptly. No DSET waits for IPC/native I/O, joins a thread or polls for queue space.

Runtime servicing visits registered active contexts after supervisor progress and during stop. It borrows a selected terminal exactly once through the existing scheduler owner. Retain only the borrowed view and fixed callback/context metadata; keep the terminal payload in its charged scheduler storage. Any decoded conversion scratch has a checked, bounded allocation included in the record-context/storage accounting, without a second accumulating result queue.

Maintain explicit callback states for pending request, accepted queue entry, running and drained. Reserve queue/running ownership before callbackRequest so an immediately executing callback cannot race publication; on enqueue failure, retain the terminal/context and retry from module servicing without native replay. Allow at most one accepted callback per record generation. Retry work is bounded by registered contexts and must not busy-spin, monopolize another address, or reset deadlines. Account the new fixed record/context storage against the existing registration bound and include any changed reservation formula in the documentation and regression checks.

The custom Base callback retains its context and obtains a lifetime/entry lease through the module completion-entry gate before fetching a record pointer or calling dbScanLock. The gate grants entry and increments its entered-processing count atomically. Release the gate mutex before fetching/locking the record; never hold it across dbScanLock, FLNK, terminal release or a callback wait. A callback that loses the race to gate closure remains inert, does not access record storage, and accounts only its own ownership. An already granted lease covers callbacks waiting for the record lock as well as executing record support.

After obtaining the lease and dbScanLock, verify the record/context lifetime, configuration revision, activation, frozen field identity, handle and exact generation/admission, and require the matching active PACT phase. A frozen-field mismatch follows the binding-error completion rule above; an invalid/stale generation never applies data to another generation.

For an entered callback, prepare checked input data or stage the terminal failure, then call the record's rset process as Base ProcessCallback does. Keep checked native input in staging until the matching DSET completion branch publishes it; the wrapper does not publish input before record support chooses the value source. DSET's completion branch consumes only that staged generation and never admits another request. Base runs monitors, FLNK, RPRO handling and PACT clearing. An entry lease covers record processing, terminal finalization and the last record access, and decrements the entered-processing count only after dropping the record lock.

The wrapper finalizes terminal consumption even if output IVOA, a live OOPT change, or simulation bypasses completion DSET. Validate these paths explicitly; do not depend on a second DSET call to release the result or report its error. Release the terminal and finish generation accounting after record processing, before dropping the record lock, so Base's queued RPRO cannot race the old terminal release. Under D7 the next admission is not rejected for retained native ownership; it is queued behind that retirement. Remaining synchronous rejections (count/byte limits, closed admission, identity mismatch, conversion) are reported without implicit replay. "Latest write" means the last value Base prepares through RPRO coalescing and dbNotify deferral, not client arrival order. First-pass admission is the SIMM commit point: an admitted SET, including one queued behind retirement, is sent even if SIMM changes before dispatch. Put-callback status remains Base notifyOK for every terminal; it does not report SET failure. An invalid/stale context never touches new record fields, and retires only its own borrowed ownership.

For an accepted GET whose completion uses input simulation, finalize the owned native terminal once even though no completion DSET publishes it. Stage native/conversion failure alarms before record processing, preserve Base alarm priority, and keep native outcome separate from SIOL outcome in the receipt. A simulation-only value must not update the context's successful-native-publication flag. No completion path starts a replacement GET. Base's delayed simulation callbacks are not module accepted generations: PACT alone does not prove ownership, and a DSET completion entered without a matching module generation must report the missing ownership without native admission. T8 observes simulation before admission, a live switch during delayed native completion, and return to normal mode, including SDLY-driven Base callbacks.

Require longout OOPT Every Time; initialization reports an unsupported setting and leaves no usable binding. Check runtime changes before admission and surface an invalid setting during completion even if conditional_write skips DSET. ao/int64out/stringout/lso have no OOPT; exercise their IVOA branches. For lso, distinguish its short IVOV field from the long VAL buffer and observe Base's first-pass IVOV copy/termination rather than imposing a new module policy. A value put through Base while active may request RPRO, while repeated PROC/scan activity is not a promise to preserve every intermediate value. A failed or ambiguous SET has no new module retry; an explicit subsequent operator request is separate from the native library's configured retry behavior.

###### Startup And Shutdown Contract

Register all DSETs and initialize contexts before AfterFinishDevSup. That hook freezes configuration and starts the existing Runtime before PINI. Record initialization performs no device I/O. Startup/service failure closes admission and is visible to Main and record errors, not a fabricated ready state.

For snmp3Stop and AtShutdown, close record admission first, preserve selected terminals, select Stopping for remaining work, and service terminal callbacks while Base record locks/callbacks/links are alive. Coordinate supervisor stop/join, callback retry/drain and exact worker reap as separate ownership obligations. Refactor the stop orchestration so it holds neither a record lock nor an operation mutex needed by callbacks while waiting for progress or joining; retain idempotent/concurrent stop and the M5 helper-stop bound.

Successful callback drain requires no module callback pending/queued/running and no record-undelivered accepted generation (amended under D7); a retirement-pending predecessor is an M5 reconcile obligation outside the drain predicate. After a failed or incomplete stop a handle can retain up to two charged generations: the successor is released by identity at isolated queue destruction, the predecessor only by Retired or exact reap. Its completion uses Base FLNK with admission already closed; downstream attempts fail without sending new native commands. A fanout or cyclic external DB is not a shutdown completion barrier.

Use a proposed 2000 ms callback-drain attempt budget, measured separately from native stop/reap. On success or expiry, stop module callback producers and atomically close the completion-entry gate. On expiry, record unsuccessful drain and forbid successful restart. A queued callback without an entry lease can no longer access record storage once the gate closes; retain its context/storage and borrowed terminal ownership until actual exit or proven queue destruction. A lease granted before closure remains counted, including while waiting for dbScanLock, until that callback has finished its final record access and unlocked.

Wait for the entered-processing count to reach zero before returning from AtShutdown and allowing link closure. Use an event/condition wait that holds no record lock, servicing lock or callback-needed operation mutex. Gate closure cannot cancel a callback already inside rset process. This safety wait is separate from the 2000 ms attempt budget; if external record processing does not return, shutdown must remain waiting with a reported external limitation rather than proceed to close links/free storage. No claim of a bounded total IOC shutdown is made.

After that safety wait, AtShutdown establishes detach permission and returns. During doCloseLinks, shutdown del_record detaches record pointers while retaining callback contexts. AfterStopCallback proves that Base callback threads are joined, not that every queued callback ran. For an isolated shutdown, callbackCleanup destroys the queues before AfterShutdown; finalize abandoned queue entries/borrowed terminals only once that destruction is established, without any record access. For a non-isolated shutdown without queue cleanup, retain any still-referenced inert callback contexts. Never use the callback-join hook alone to free queued storage. Context retention and the completion-entry gate jointly prevent post-detach record access; retaining a context alone cannot keep a freed Base record alive. Retain scheduler/native ownership as required by M5 and keep unsuccessfully drained activations ineligible for restart.

The Base init hook is void and cannot veto database free. The module therefore proves absence of entered record processing before AtShutdown returns, prevents new entry before links close, and delays callback-storage release until Base ownership ends. Use real lifecycle tests to verify these distinct boundaries; do not repair installed Base or label expired drain as successful completion.

###### Ordered Implementation And Closing Checks

Steps 1-6 were executed under G1 for the superseded plan. Steps 7-8, and further work on the remaining T1-T14 cells, are executable only after G5 completes. New file names below are proposed; current shipped files remain unchanged by this planning operation.

The numbered steps define implementation order. Their closing checks are final verification obligations, not prerequisites for starting the next implementation step. Steps 1-4 can proceed while their integration checks remain pending; those checks close only after the required DSET, lifecycle support and shipped test products/fixtures are available through step 5. Run the real integrated path for final verification, without replacing missing internal components. M6 remains incomplete until every required T1-T14 check satisfies its completion criteria.

1. Add `snmp3App/src/Conversion.{h,cpp}` and an immutable record-binding definition in `Request.h`; implement the accepted matrix, loss policy, grammar validation and bounded conversion staging. Final verification after step 5: T1-T5 and the defective-conversion controls in T14.
2. Add `snmp3App/src/Request.{h,cpp}` and narrow terminal-delivery hooks in `Runtime.{h,cpp}`; use Scheduler take/release, full identity, fixed context accounting and reliable callback retry. Keep worker/native production code and D3 scheduling semantics intact. Final verification after step 5: T6, T9-T12 and targeted ownership controls in T14.
3. Add `snmp3App/src/DeviceSupport.cpp`; export fresh DSETs for all eleven records, direct ai values, OVAL-based ao writes, exact int64in/int64out VAL access without a double intermediate, bounded lsi/lso VAL/LEN handling and payload capture, output policy checks, record-specific UDF/alarm behavior, Base-compatible waveform BUSY management before record completion, input simulation/value-source separation, frozen-field validation and phase-aware add_record/del_record. Update `snmp3.dbd` and `Makefile` for record declarations, new sources and dbCore linkage. Audit actual IOC/support/wire loaded dependencies. Final verification after step 5: T1-T8 and T13.
4. Extend `Register.cpp` and Runtime/Request stop orchestration for pre-PINI readiness, admission closure, the completion-entry gate/count, callback drain, shutdown-phase detach and retained failed-stop state. Account Base callback join separately from actual queue cleanup and release. Preserve nonblocking later native reconciliation and no configuration reload. Final verification after step 5: T10-T12 and targeted ASan/UBSan cases in T13.
5. Ship `tests/rewrite/db/records.db`, `record-order.db`, `record-output.db`, record-specific config/startup fixtures, `RecordTest.cpp` and `test_records.py`; extend the actual NativeAgent/external UDP fault fixtures for typed edges, delayed/error responses and wire receipts. Wire normal and separate sanitizer products through `tests/rewrite/Makefile` and the existing sanitizer builder. Close all T rows through these shipped paths, with no internal stand-ins.
6. Update `docs/snmp-rewrite-contract.md`, architecture/configuration/worker references and `tests/rewrite/README.md` with the accepted record grammar/matrix, reservation changes, errors, ordering and shutdown limits. Change the reported record capability only after real evidence exists. Record source/product/library hashes, results and unresolved limits here; keep APC/R8/mdBook claims separate. Close with T13 and a requirement-to-T-label review of this detail.
7. D7/D8 test preparation (executed 2026-10-03/04; tests committed in `7ffdf5a`, controls on the products before D7 recorded under M5 T5/T6 and M6 T7/T9/T11; the T12 rebuild case was added and executed on 2026-10-05, see T12). The active-output CA section, the delayed endpoint and the INT64_MIN step first existed only as working-tree changes used by `work/r6-ca-active-20261003-003058/`; this step corrected and committed them. Correct `tests/rewrite/test_record_ca.py` (observed-value observations only; FLNK count as a phase sentinel, not an admission assertion; agent-side SET value record in `NativeAgent.cpp` instead of a same-address readback; INT64_MIN baseline read just before the rejected put; end-of-run counts computed from executed steps; put-callback second write); add `snmp3RecordTest` cases without the consumer hold, recording per trial the interval between supervision codes 4 (Result) and 5 (Retired) for the record's batch; a deadline-queue case with outer UDP drop-all that first records the supervision timeline over repeated fresh-process trials and records, over N trials, the minimum and maximum observed interval from the queued generation's admission to code 3 (Ready) of the next epoch, then runs budgets below the minimum and above the maximum (fresh process per trial); a two-generation accounting case and stop cases with a retirement-pending plus queued generation. Write and execute the new SchedulerTest admission cell (M5 T5), the active CA, put-callback and unheld record cases on the current products first and retain their failing outcome as the executed negative control; the SchedulerTest failure is recorded under M5 T5. Also run a near-deadline case on the current products (response delay close to the record budget, repeated trials) and record as k of N how often a worker is contained after its result was already selected, before the D9 change; record it under M5 T6 as the current-product baseline (k=0 is reported as not observed, not as absence of the defect).
8. D7/D8 record path, after the M5 revision: `Requests::service` takes the terminal by the context's exact identity (done in `98bfdfc` with the Scheduler change); the callback applies a staged alarm message and `DeviceSupport.cpp` `prepare` sets AMSG `deadline before send` for a Deadline that was never sent (both done 2026-10-05; results under T9); `snmp3RuntimeReport` prints the new queue counters (done 2026-10-05, results under T9). Update the contract, configuration, architecture and test references, and, once the counters are printed, the statement in `docs/snmp-worker-supervision.md` that `snmp3RuntimeReport` does not print them (updated 2026-10-05), with the coverage, latest-write and SIMM definitions, the deadline-path threshold (a queued generation dispatches after containment only when its budget exceeds reap plus relaunch plus Ready; about 1.83 s was measured for a worker that does not exit on channel close, while a worker that exits on close is reaped within tens of milliseconds, so the threshold documented for the outer-UDP drop-all case is the minimum-to-maximum range over every relaunch measured by the deadline-queue case, sampling and budget trials alike, citing the run it comes from; the case's threshold calculation is extended accordingly in this step, done 2026-10-05, results under T9) and late-application bound (up to about twice the budget after the operator's put, plus any Base callback delay before the previous generation's completion, as recorded by the put-to-dispatch interval of the T9 deadline-queue cell), client guidance and queue sizing. Add the put-to-dispatch interval per trial to the deadline-queue case so the documented late-application bound has a receipt (done 2026-10-05, results under T9). The documentation updates of this step (contract, configuration, architecture and test references) are done as of 2026-10-05. Re-run the M6 matrices on ordinary and instrumented products (done 2026-10-05 on the sources of `2544819`, results under T13 and T14).

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Binding/startup integration | Load shipped valid/invalid record config/DB fixtures; verify all eleven DSETs, grammar, operation/type/FTVL mismatch, unknown IDs, duplicate record references and freeze; verify effective lsi/lso SIZV at default/minimum/maximum and Base clamping boundaries; include waveform fixtures with default and initially nonzero BUSY, inspecting device initialization and first-pass state; attempt INP/OUT changes and direct DTYP writes while idle/active and concurrent with drain/detach, then complete actual shutdown; run PINI/passive/periodic requests. | Current local Linux Base 7.0.10; actual snmp3Ioc, worker and NativeAgent | No initialization network I/O; independent immutable handles and frozen effective long-string capacity; device initialization normalizes waveform BUSY to FALSE without a BUSY-specific rejection, and pending SNMP work uses PACT with BUSY FALSE; refusal preserves context before detach permission; no new snmp3 attachment/admission during shutdown, with other-support attachment identified separately; accepted DTYP field write cannot silently rebind/admit snmp3 work; shutdown del_record detaches without early callback-storage free. |
| T2 | Numeric input integration | GET each advertised numeric tag through real ai/longin/int64in/numeric waveform records; agent supplies tag-appropriate INT32/UINT32 extrema, exact/inexact values around 2^53 and 2^24, Counter64 INT64_MAX/MAX+1/UINT64_MAX, inexact OpaqueDouble-to-FLOAT input, nonfinite opaque values and exception/type errors; observe exact values through IOC DBR_INT64/DBR_UINT64 access and separate CA decimal DBR_STRING reads; repeat each lossy input after a successful exact value. | records.db; actual native UDP agent and shipped conversion/DSET | D6 rejects Counter64-to-ai and numeric-to-FLOAT precision loss with an error and previous input retained; Counter32/TimeTicks through UINT32_MAX and Counter64 through INT64_MAX remain exact in int64in; larger Counter64 rejects there and remains exact in UINT64 waveform; DB and CA string observations preserve complete integers; no wrap, saturation, partial publication or implicit tick scaling; record-specific UDF behavior observed. |
| T3 | Binary/structured input integration | GET octets with empty/embedded-NUL/high bytes and exact/over-capacity lengths, OID arcs and IPv4 into real stringin/lsi/waveform; vary Binding capacity, SIZV and NELM independently; include 39/40/255/256-byte and maximum SIZV-1/SIZV payloads; observe full long strings through IOC access and CA VAL$ CHAR arrays, compared with short DBR_STRING access; repeat capacity failures after successful input. | records.db; actual worker/native/agent | D6 rejects Binding or record-capacity overflow with an error, preserves prior input bytes/NORD/LEN and publishes no shortened prefix; complete accepted values and exact NORD/LEN; lsi LEN includes NUL, empty success has LEN=1 and failure preserves VAL/LEN/UDF; deterministic formatting; reject unsupported FTVL and unusable strings without partial BPTR/VAL updates; distinguish client representation from native payload. |
| T4 | SET conversion integration | Process real ao/longout/int64out/stringout/lso for every advertised native tag; exercise integral/fractional, negative/unsigned, range/precision, termination and capacity edges; for ao-to-OpaqueFloat use exact binary halfway fixtures selecting both even neighbors, positive/negative values on and around halfway points, ordinary inexact decimals, normal/subnormal/signed-zero and overflow boundaries, and non-default ambient rounding modes; compare actual wire binary32 bits and restored thread state, and verify OpaqueDouble does not narrow; use IOC DBR_INT64 and separate CA decimal DBR_STRING int64out writes at INT64_MIN/MAX, INT32/UINT32 boundaries and 2^53+1; send lso empty/long/exact-capacity values through IOC and CA VAL$ access and observe Base handling of oversized client writes; inspect actual SET packets/agent state, LEN and Base int64out drive limits/IVOA. | record-output.db; actual IOC/worker/native/agent | D5 roundTiesToEven produces the selected binary32 payload without decimal pre-rounding, premature subnormal flushing or ambient-mode dependence; finite rounding is accepted and nonfinite/overflow payloads produce no SET; requested record values remain separate from rounded payloads; lso sends LEN-1 bytes without NUL; invalid captured termination/LEN/Binding capacity produces no SET, with Base pre-DSET truncation identified separately; int64out to Counter64 preserves nonnegative values through INT64_MAX, including 2^53+1, with no double intermediate; invalid target-range/negative-to-unsigned values produce no SET; Base drive/rate adjustments honored; success is separate from GET readback. |
| T5 | Alarm/undefined integration | Produce initial failure, valid GET then failure, local admission/convert error, native errorStatus/errorIndex, exception, timeout and competing Base limit/UDF alarms; observe STAT/SEVR/UDF, monitors, diagnostic success state, long-string LEN and values seen by actual FLNK targets; compare normal SNMP input with T8 simulation results for all six input records; under D8 let an output and an input generation reach its deadline while queued behind retirement. | records.db and external agent/channel faults | In normal SNMP input, ai/longin/int64in/stringin/lsi preserve UDF on failed reads; successful lsi clears UDF and failure preserves VAL/LEN; waveform follows Base UDF clearing even on failure, retains BPTR/NORD and reports INVALID with successful-native-publication state unchanged; Base simulation value/UDF changes are identified separately and never counted as SNMP publication; failures visible even when completion DSET is skipped; a never-sent Deadline reports COMM/INVALID with AMSG `deadline before send` and posts an alarm event after a previous COMM/INVALID. |
| T6 | Base completion/order integration | Observe actual A-FLNK-B request/terminal/record phases and PACT on success/error/timeout, including lsi/lso long-string cases and waveform BUSY before callback rset entry; include fanout to independent addresses with reversed response delays and duplicate external responses. | record-order.db; real record/wire event identities | Waveform BUSY is FALSE before completion rset entry, so Base reaches completion rather than its BUSY early return; Base-owned FLNK once per accepted generation before PACT clears; B starts after A terminal processing and sees the complete lsi VAL/LEN; fanout supplies no completion barrier; no duplicate record mutation. |
| T7 | Active output/retry integration | Change ao/longout/int64out/stringout/lso output through dbPutField/CA while an actual SET is delayed, using IOC DBR_INT64 or CA decimal DBR_STRING access for int64out and CA VAL$ CHAR arrays for long lso values; compare scan/PROC triggers, same-value explicit retry after failed SET, native retries and ambiguous response loss. Under D7 also run the unforced production CA path (no consumer hold) with a plain put and a put-callback second write, and an unheld `snmp3RecordTest` case reporting per trial whether the queued-behind-retirement branch ran (exercised k of N, k=0 reported as not run) and the interval between supervision codes 4 and 5 for the record's batch. | record-output.db; actual delay/drop boundary | First payload stays immutable, later requested VAL/LEN survives, Base RPRO sends the latest prepared request; under D7 it is admitted behind the previous generation's retirement on the normal path, with two SETs on the wire, post-pass NO_ALARM and the agent holding the latest value, and the current products' rejection retained as the executed negative control; identify any client-side/Base pre-DSET truncation separately; no implicit module replay or promise to retain every intermediate update. |
| T8 | Record policy/simulation integration | Exercise longout OOPT Every Time and each rejected setting at initialization/live change; all ao/longout/int64out/stringout/lso IVOA branches and simulation during a delayed completion; exercise lso OMSL/DOL and its short IVOV copied into the effective SIZV-sized buffer. For ai/longin/int64in/stringin/lsi/waveform, use distinguishable agent and SIOL values, simulation before admission, a SIMM switch during successful/failing delayed GET and return to normal mode; include ai RAW and actual SDLY-driven Base callbacks; under D7 switch SIMM while an output SET is queued behind retirement on the normal and deadline paths; then inspect waveform BUSY at completion rset entry, source identity, alarms/UDF, VAL/BPTR/NORD/LEN, native-publication state, terminal release, FLNK and PACT. | Real Base record support, records.db/record-output.db and actual native GET/SET | Unsupported longout OOPT visible without a usable initial binding; no OOPT requirement invented for int64out/lso; completion errors and ownership release survive skipped DSET; no new GET/SET in completion. Simulation before DSET admission sends no native request; an accepted GET terminal is consumed once even when Base selects SIOL and skips completion DSET, with Base-owned simulation changes distinguished from D6 SNMP publication. Waveform completion enters Base with BUSY FALSE even when simulation bypasses DSET; no wrapper publication precedes Base source selection, no simulated value marks native-publication success, and no ownerless PACT completion admits native work. Observe FLNK/PACT and actual delayed callbacks without duplicate module completion; documented Base suppression, prepared VAL/LEN and possible pre-DSET truncation identified. A SET admitted before a SIMM switch is still sent (first-pass admission is the commit point). |
| T9 | Queue/retirement integration | Real record bursts exhaust count/bytes, including maximum-capacity lsi/lso GET/SET payloads and their bounded conversion/capture storage; reduce/increase limits; delay native retirement after record timeout/consumption and retry the same handle; under D7 a deadline-queue case with outer UDP drop-all that first records the minimum and maximum admission-to-Ready interval over N fresh-process trials and then runs budgets below the minimum and above the maximum, in a fresh process per trial, and a two-generation accounting case. | Actual scheduler/IPC/worker/native path; existing queue reports | Count/byte exhaustion rejects synchronously without blocking; a retry of the same handle while only retirement is pending is admitted and queued (D7), giving count 2 and bytes q1+q2; with a budget below the recorded minimum the queued generation ends Deadline without a SET, with a budget above the recorded maximum it dispatches after reap and Ready, and N, the minimum, the maximum, the put-to-dispatch interval per trial and the receipt are recorded; full Q/count remains until both obligations finish; Base-owned long-string buffers distinguished from charged module storage; no accumulating callback result queue; other address continues servicing. |
| T10 | Callback pressure integration | Fill the actual Base callback queue with controlled external callbacks, delay its consumer, complete real GET/SET and inspect retry/queued/running counters; then release pressure. | Shipped RecordTest harness using Base callbacks; unchanged module/native path | Retry failed callbackRequest until one entry is accepted; no duplicate entry; result remains owned, PACT completes once and no native replay/deadline reset occurs. |
| T11 | Shutdown integration | Stop with queued, dispatched, borrowed, enqueue-failed and running completions; hold actual Base record locks or downstream external link processing across attempt expiry, then release the boundary; observe waveform BUSY before Stopping completion, gate entry/exit, FLNK, AtShutdown return and worker join/reap. Under D7 stop with a Deadline-selected, consumed, unreaped generation and a queued successor, with the worker reaped inside the stop bound (the outside-bound variant was removed by the T11 scope decision of 2026-10-04). | Actual IOC lifecycle/record support; real workers and controlled external blocking | Waveform Stopping completion enters Base with BUSY FALSE and completes FLNK/PACT processing when callback progress is available; expiry closes new entry and records unsuccessful drain; already entered processing finishes before link closure; no callback-needed lock held across waits; safety wait can exceed the attempt budget without a false bounded-shutdown claim; the queued successor completes Stopping with FLNK once and drain succeeds while retirement is pending. |
| T12 | Cleanup/activation integration | Execute isolated shutdown/testdbCleanup/rebuild with delayed or abandoned queued completions, including lsi/lso pointer-backed buffers and retained maximum-capacity payloads; exercise phase-aware del_record, Base callback join followed by queue destruction, non-isolated shutdown, startup failure, repeated stop and retained native ownership; under D7 isolated rebuild after a stop that retained a retirement-pending and a queued generation. | Base isolated/non-isolated lifecycle with real SNMP record DBs; ordinary and instrumented module | No post-detach record/buffer access or premature callback-storage free; queued ownership survives join until actual exit/queue cleanup; expired drain blocks restart; configuration remains frozen and old native ownership must settle; the successor is released at queue destruction and the predecessor only by Retired or exact reap. |
| T13 | Product/regression qualification | Build/load normal and separate ASan/UBSan record products; identify sources, binaries and loaded libraries; run targeted T2-T4/T9-T12 and existing shipped M1-M5 regressions. | Current unchanged local Base/native dependencies; instrument new module code | No forbidden legacy/native IOC dependency, new sanitizer finding or foundation regression; identify uninstrumented dependencies and unsupported/unexecuted cells explicitly. |
| T14 | Real-path negative controls | Build isolated defective copies of shipped support with D6 precision/capacity checks, D5 tie selection/rounding-mode handling, callback failure retry, generation check or terminal release deliberately altered or disabled, and the D7 record-cell controls, each failing a named check that passes on the unmodified products: a copy that restarts a queued deadline at dispatch must fail the deadline-queue check below-threshold-queued-generation-not-sent, and copies whose stop selects one generation per binding or whose lookups are keyed by binding only must fail the stop-queued check queued-successor-completes-stopping-once; run their corresponding real record fixtures through scheduler/worker/native/agent spans that are unchanged except for the deliberately altered code (wording amended by owner decision of 2026-10-04 so that D7 controls may alter `Scheduler.cpp`). | Separate disposable qualification products; never installed or substituted into ordinary products | Each targeted assertion detects its defective behavior, including unauthorized lossy/prefix publication or incorrect binary32 tie bits; retain executed control outcome rather than assuming a test would fail. |

All test environments are planned, not executed by this document update. Keep mocks/faults at external agent, UDP/filesystem/clock boundaries; Base callbacks and the internal DSET, Runtime, Scheduler, IPC, worker and native adapter must actually run. Existing component tests supplement this path. Stop/race checks use observed phase sentinels and identities rather than a fixed sleep or final values alone. Retain monotonic event order, actual request/terminal counts, alarms/PACT/UDF/NORD/LEN and source/product/library identities in local result receipts; never include credentials or secret-file contents.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01 | Actual Base 7.0.10 records/DSET/worker/agent and production IOC/CA | Pending; subset PASS | All eleven DSET kinds and aliases; unknown binding/grammar/FTVL rejection; all five unsupported initial longout OOPT choices leave no usable binding; effective SIZV clamps; initial waveform BUSY; idle link/DTYP checks. Production IOC longin PINI runs after pre-PINI servicing and Base FLNK completes once, observed through actual CA. Remaining startup, live-field, periodic and concurrent-detach cells are unexecuted. Record receipts below. |
| T2 | 2026-10-02 | Actual numeric record/native and production IOC/CA path | Pending; subset PASS | All 68 advertised numeric GET pairs, including 48 native-tag/FTVL pairs, receive successful native input. Range/precision/extrema cover signed/unsigned 32-bit, 2^24/2^53, Counter64 INT64_MAX/MAX+1/UINT64_MAX, finite opaque extrema/subnormal/signed zero and initial NaN/infinity rejection. 121 conversion failures after successful native input preserve prior bytes/NORD and native-success state, with Base UDF behavior observed. Actual decimal DBR_STRING CA preserves 2^53+1 and INT64_MAX through int64out SET/int64in GET; UINT64 waveform reads UINT64_MAX exactly through CA STRING. Native exception/type-error and additional failure-after-success cells remain unexecuted; the signed-minimum CA cell is deferred to M11 by D13. |
| T3 | 2026-10-01 | Actual binary/text/structured record/native and production IOC/CA path | Pending; subset PASS | Exact binary octets including NUL, OID arcs and IPv4; text rejects embedded NUL or overflow without prefix publication; lsi VAL/LEN preservation, empty and maximum 32766-byte data. Actual lsi/lso VAL$ CHAR-array CA SET/GET is exact at 0/39/40/200/255 data bytes; CA DBR_STRING view is 39 bytes; short stringin/stringout SET/GET is observed. A 300-byte VAL$ write is rejected by caput before put and preserves prior VAL/LEN/native GET. Remaining structured/text boundaries are unexecuted; maximum-capacity CA cells are deferred to M11 by D13. |
| T4 | 2026-10-02 | Actual output DSET/native SET/GET and production IOC/CA | Pending; subset PASS | All five output DSET kinds and all 20 advertised numeric SET pairs. Signed/unsigned target boundaries, integral/fractional conversion, full nonnegative signed-64 capture and finite floating extrema/subnormal/signed zero are checked by separate native GETs; 67 invalid numeric outputs preserve requested values and produce no native SET. Exact Counter64 SET of 2^53+1; 36 binary32 wire-bit fixtures across four ambient modes; requested ao retained; nonfinite/overflow/negative Counter64/over-capacity SET rejected; empty and maximum lso payload. Base ao drive clipping/rate limiting and OVAL wire capture, all five first-pass IVOV values and lso DOL are verified. Actual CA writes exercise int64out decimal and stringout/lso short/CHAR-array values with separate native GETs. Remaining long-string boundary cells remain unexecuted; ao/longout CA and signed-minimum CA cells are deferred to M11 by D13. |
| T5 | 2026-10-01 | Actual Base value/alarm processing and outer UDP response dropping | Pending; subset PASS | Initial/local conversion failures preserve input data/UDF; errors survive input simulation bypass. All eleven record kinds report COMM/INVALID on actual NativeFailure/timeout code 2; a separate record deadline reports IPC Deadline/COMM. Waveform clears BUSY and follows Base UDF while native-success remains false. Native errorStatus/exception, failure after a valid GET, competing-alarm and monitor matrix remain unexecuted. |
| T6 | 2026-10-01 | Actual ai-FLNK-calc-FLNK-lsi chain | Pending; subset PASS | Source PACT is 1 during Base FLNK; target completes once with complete VAL/LEN. Other record/failure/deadline chains, fanout and delayed/duplicate external responses remain unexecuted. |
| T7 | 2026-10-01; 2026-10-03; 2026-10-04 | Actual Base dbPutField/RPRO, delayed UDP SET and response loss | Pending; subset PASS | All five outputs preserve the captured first payload and process the latest requested value through Base RPRO when admission is available; exact DBR_INT64 and 200-byte first lso payload. Same-value explicit retry uses two module generations/distinct native request IDs; configured native retry uses one generation/the same native request ID. Separate GET observes SET application despite lost successful responses. Scan and PROC reprocess cells remain unexecuted, Channel Access active writes are executed below and long active client writes are deferred to M11 by D13; the retained-ownership admission-rejection cell is replaced under D7 by admission behind retirement. Negative controls on the products before D7 (`c065755`), 2026-10-03/04: on the production CA path, for all five outputs and both a plain put and a put-callback second write while the first SET response was held, one SET reached the wire, the agent stored only the first value, the record ended WRITE/INVALID after the Base reprocess, and the put-callback client exited with status 0 while the device kept the previous value (`work/r6-ca-head-control-20261003-225443/`). The unheld snmp3RecordTest case, run with the committed test on the products before D7, gave the same outcome; the queued-behind-retirement branch ran in 0 of 5 trials (not run), and the record-path Result-to-Retired gap measured 10.09-10.25 ms (`work/r6-headc-active-unforced-20261004-014819/`). Revised products, D7 (`98bfdfc`) on 2026-10-03 and D7/D9 (`0b60abf`) on 2026-10-04: production CA plain put and put-callback second writes sent two SETs per output, the agent stored the first then the latest value, records ended NO_ALARM, and the put-callback client exited after the second delivery; 315 of 315 checks and `completions=44` (`work/r6-d7-ca-20261003-235055/`, repeated with D9 in `work/r6-d9-ca-20261004-000528/`). On the D7 products the unheld case admitted every output's latest value; the queued-behind-retirement branch ran in 5 of 5 trials and the record-path Result-to-Retired gap measured 10.20-10.56 ms (`work/r6-d7-active-unforced-20261003-235016/`). |
| T8 | 2026-10-01; 2026-10-05 | Actual Base simulation, output policy and DSET/native path | Pending; subset PASS | All six inputs use SIOL before admission and after a queued native completion; Base SDLY and ai RAW run; waveform BUSY clears before simulation bypass. All five outputs exercise three IVOA branches on first-pass/completion, live simulation after actual native timeout, synchronous/delayed simulation before admission, preserved current VAL/LEN and exactly-once terminal/FLNK processing. All five unsupported initial/live longout OOPT choices are observed. lso supervisory/closed-loop DOL, short IVOV and Base pre-DSET capacity reduction are verified by separate GET. Input failure and source switching, 2026-10-05, policy case on ordinary products (`inputSourceSwitch`, committed in `fd60298`): for all six inputs, SIMM selecting SIOL while an actual native timeout terminal waits in the Base queue, a return to normal mode with a further native failure, and a SIMM change while a Base SDLY callback is pending; the check `actual-six-input-failing-source-switch` passes with six `input_source_switch` events, each with a native-failure alarm, no native publication, the normal-mode failure preserved, no ownerless admission, SIOL selected and the terminal released once (`work/r6-s8e-regress-20261005-093844/policy/results.json`). Delayed-simulation mode-change cells for the five outputs are deferred to M11 by D13. A SIMM switch while an output SET is queued behind retirement, on the normal and deadline paths, is not yet executed and is run under D13. |
| T9 | 2026-10-01; 2026-10-03; 2026-10-04; 2026-10-05 | Actual maximum long-string/native/queue path | Pending; subset PASS | 32766-byte SET and three GETs; two borrowed terminals retain 796640 charged bytes; byte-exhausted request rejects synchronously; lowering limits preserves ownership and raising them admits a separate explicit request. Delayed native retirement/retry and other-address progress remain unexecuted. Products before D7 (`c065755`), 2026-10-03/04: on the outer-UDP drop-all path the worker exits on channel close; Deadline to Ready of the relaunched worker measured 372-393 ms over all five fresh-process relaunches with the committed test (373-383 ms in the 3 sampling trials), and with budgets below (186 ms) and above (766 ms) that range the Base reprocess was rejected WRITE/INVALID without a second SET (`work/r6-headc-deadline-queue-20261004-014819/`). Two-generation accounting was never reached, while a count limit of one rejected the reprocess synchronously and a raised limit admitted a separate explicit request (`work/r6-accounting-head-control-20261003-231608/`). D7/D9 products (`0b60abf`), 2026-10-04: admission to Ready of a queued successor measured 371-403 ms over all five relaunches of the run (371-391 ms in the 3 sampling trials, 403 and 392 ms in the two budget trials); with a 185 ms budget it ended COMM/INVALID without a second SET, with a 781 ms budget it was dispatched after Ready; the AMSG cell fails until M6 step 8 (`work/r6-d9-deadline-queue-20261004-000528/`). Two generations were held together in 3 of 3 trials (`count=2`, 2 x 5192 bytes) and the count-limit and raised-limit cells passed (`work/r6-d9-accounting-20261004-000528/`). 2026-10-05, AMSG of a never-sent Deadline (D8): the callback applies a staged alarm message and `prepare` stages `deadline before send` only for a Deadline that was not sent. In the deadline-queue case the below-threshold queued generation shows COMM/INVALID with that message; the sent generations (sample, above) and the follow-up SET after the never-sent generation (generation 3, COMM) show none; the stop-queued successor completes Stopping with an empty AMSG. Four controls fail their named checks (message always staged, never staged, staged for any unsent outcome, not reset in `Requests::admit`) while the unmodified products pass; all 21 D7 and AMSG controls PASS. 13 record cases on ordinary products (stop-queued and deadline-queue also on instrumented products), production CA (315 checks) and supervisor/IOC (25 checks) PASS. Report counters (2026-10-05): the `snmp3 queue:` line of `snmp3RuntimeReport` now ends with `behindRetirement`, `behindAdmitted` and `behindNeverSent`; the report of an actual record test process shows 0, 1, 1 in the below-threshold trial and 0, 1, 0 in the above-threshold trial, the sample trials and stop-queued; the check `queue-report-counters` reads that line and a defective copy that prints the admitted count in the never-sent slot (`report-never-sent-miscounted`) fails it, so all 22 D7, AMSG and report controls PASS (`work/r6-s8c-controls-20261005-084755/results.json`); component, supervisor/IOC, operator, 13 record cases and production CA PASS (`work/r6-s8c-regress-20261005-084755/`); the lifecycle runner PASS in `work/r6-s8c-regress-20261005-084755/lifecycle2/` (its first call in `lifecycle/` lacked the required `--base` argument and exited 2 at argument parsing). Deadline-queue measurements (2026-10-05): the case records per trial the put-to-dispatch interval (operator's put to the successor's dispatch) and reports the threshold as the minimum-to-maximum range over every relaunch it measured, sampling and budget trials alike (`deadline_threshold_all`); the run `work/r6-s8e-regress-20261005-093844/deadline-queue/` measured 370.6 to 392.0 ms on ordinary products over five relaunches (sample budget 1000 ms: put-to-dispatch 1370 to 1392 ms; above-threshold budget 783 ms: 1153 ms; below-threshold budget 185 ms: not dispatched) and `deadline-queue-san/` measured 714.6 to 740.1 ms on instrumented products (measured from containment to Ready: 371.4 to 393.2 ms and 715.7 to 741.4 ms; the instrumented put-to-dispatch interval was 1718 to 1732 ms at the 1000 ms budget); individual ordinary-product runs of the case measured 362.2 to 403.4 ms from containment to Ready (for example `work/r6-s8g-deadline-queue-20261005-093844/`) and the retained instrumented runs 710.3 to 745.3 ms (`work/r6-s8b-regress-20261005-081137/`, `work/r6-s8e-regress-20261005-093844/`, `work/r6-s8g-deadline-queue-san-20261005-093844/`, `work/r6-s8h-deadline-queue-san-20261005-093844/`); a worker stopped at the process-signal boundary took 1848 ms from containment to Ready in the qualification `deadline` case on ordinary products (`work/r6-d9-test_qualification-20261004-000357/deadline-4/`), and an earlier measurement recorded in item 8 gave about 1.83 s; the checks `threshold-over-every-relaunch` and `put-to-dispatch-within-late-application-bound` (a successor keeps the deadline of its own admission, so its dispatch follows the put by at most two budgets plus the callback delay) pass and also tie the reported range and interval to the raw per-trial fields, so a copy of the test that measures the interval from the first admission fails the second, and a copy that gives a queued generation a later deadline (`queued-deadline-extended`) fails the second one, so all 23 D7, AMSG, report and deadline controls PASS (`work/r6-s8e-controls-all-20261005-093844/results.json`); the six controls that run the deadline-queue case were re-run after the checks were tied to the raw fields and passed (`work/r6-s8g-controls-20261005-093844/results.json`). Two details of the AMSG change are not pinned by owner decision D11, and a nonzero `behindRetirement` in the printed line is not shown by owner decision D12. Evidence: `work/r6-s8b-controls-20261005-081137/results.json`, `work/r6-s8b-regress-20261005-081137/` (record cases), `work/r6-s8a-regress-20261005-005827/` (CA and supervisor/IOC; product sources identical). |
| T10 | 2026-10-01 | Actual Base callback queue, GET and SET | Pending; subset PASS | External callbacks fill the real queue; failed enqueue retains terminal/full reservation/PACT, retries and completes once without a second accepted entry. Full requirement review remains pending. |
| T11 | 2026-10-01; 2026-10-03; 2026-10-04; 2026-10-05 | Actual entered/queued callback shutdown | Pending; subset PASS | Held record lock makes an entered callback exceed the drain attempt; shutdown waits for its lease. A blocked Base queue leaves a queued callback retained after gate closure/detach. Expired drain refuses restart. Stop in flight, 2026-10-05, `stop-inflight` case (tests committed in `8d70e56`, product sources unchanged since `2544819`): all eleven record kinds are admitted on one unanswered address and the runtime is stopped once a batch is on the worker channel; one generation is sent and ten wait in the queue (`held_queued` 10, `pending_at_stop` 11). Each record completes once with a communication alarm and INVALID severity, no native publication, PACT 0 and waveform BUSY 0; completions and FLNK each rise by exactly 11, the drain succeeds, the state is Stopped, no retirement stays pending and the worker is reaped. Ordinary products 3 of 3 (`work/r6-t11b-stop-inflight-1` to `-3`) and ASan/UBSan products 1 of 1 (`work/r6-t11b-stop-inflight-asan`), after 6 ordinary and 3 instrumented runs of the same case before the review fixes (`work/r6-t11-stop-inflight-1` to `-6`, `work/r6-t11-stop-inflight-asan` and `-asan-2`, `-asan-3`). Two defective copies are detected on the instrumented build, each failing its named check while the unmodified products pass: a `Scheduler::stop` that does not select queued generations (`stop-inflight-every-record-completes-with-alarm`) and a waveform support that holds BUSY set (`stop-inflight-waveform-busy-clear`) (`work/r6-t11b-controls/results.json`). The case does not distinguish the Stopping outcome from other outcomes that give the same alarm; two further candidate controls were not detected and are not kept (a stop that does not select the sent generation, because the supervisor selects every active generation as Stopping when it loses the worker while stopping, and a BUSY edit at the admission site, because `prepare` clears BUSY at completion; `work/r6-t11-controls/stop-active-not-selected` and `work/r6-t11-controls-2/waveform-busy-held`). Every case run listed above printed `enqueueFailures=3` with 11 completions (callback queue size 8), and the unmodified reference run in `work/r6-t11b-controls` printed 2, so the count depends on timing; two or three completion enqueues were refused by the Base queue and all 11 completions were still counted once; the case does not assert the counter. Enqueue-failed completions at stop, 2026-10-05, `stop-enqueue-failed` case (tests committed in `17c494a`, product sources unchanged since `2544819`), three processes that differ only in when the queue is released: the low-priority Base callback queue (size 8) is held by an external blocker and eight fillers before the eleven records are admitted, so every completion enqueue is refused (`held_pending` 11, `held_queued` 0, at least eleven refusals) and the stop retries them. Released while the runtime thread still runs (`within`, stop 31 to 44 ms, shorter than the 2 s supervisor bound), or at 2.5 s after the stop, 0.5 s past the bound and inside the drain budget, when only the record drain retries (`late`), every record completes once with a communication alarm and one FLNK, the drain succeeds and the state is Stopped. Released after the drain budget (`after`), the drain fails, the stop returns before the release, the state is IncompleteStopped, restart is refused, the records keep PACT set, and the isolated cleanup finalizes the eleven completions once without FLNK. Ordinary products 2 of 2 on the final harness (`work/r6-t11c-enqueue-9`, `-10`) and 5 more on earlier harness versions (`-3`, `-5` to `-8`); ASan/UBSan products 1 of 1 (`work/r6-t11d-enqueue-asan-3`) and 2 more (`-1`, `-2`). A defective drain that does not retry its enqueues is detected through the `late` process only (`release-after-stop-bound-completes-every-record-once`, `work/r6-t11d-controls/results.json`, which also re-detects the two stop-inflight controls); its first version aimed at the `within` process was not detected because the runtime thread services completions while it runs (`work/r6-t11c-controls`). Observed stop durations with the queue held: 2.5 s for `late`, ending at the chosen release, and 4.0 s for `after`, the supervisor bound followed by the full record drain budget; Backlog M12 carries that observation for an owner decision. Downstream external link processing, 2026-10-05, `stop-downstream` case (tests committed in `b00fdec`, product sources unchanged since `2544819`), two processes: in the isolated harness every Timeout record's FLNK is a Channel Access link to a record in another lockset (`Records_StopExternal.PROC CA`, link connected, separate lockset), and the downstream record's lock is held past the drain budget. Base runs the link's put on its CA link thread and queues it from the completing record's callback, so the snmp3 completions are not held. With the lock held across the runtime stop (`stop`): the stop takes 31 to 41 ms on ordinary products and 43 ms on the instrumented product and ends Stopped with the drain successful and 11 completions while the lock is still held, every record completes once with a communication alarm and INVALID severity, the downstream value is 0 while held and 11 after the release (eleven processings, one per link; Base keeps one pending put per CA link, so a repeated FLNK would not show in this count, and once per record is shown by the completion count and the stop-inflight case). With the lock held through the IOC shutdown (`shutdown`): the snmp3 hooks AtShutdown to AfterStopCallback complete and the shutdown then waits for the lock in Base's CA link shutdown, 2.5 s until the release; the records read PACT 1 afterwards because Base's link closing sets it (`iocInit.c:700`), so PACT is not asserted in that mode, and the downstream value after the release was 1, 1, 0, 1 and 0 in five runs and is reported, not asserted. Reading Base's code, the value depends on whether the CA link thread is blocked on an attribute read (cleared at shutdown, giving 0) or on the first put (giving 1); that is a code reading, not a probe. Ordinary products 4 of 4 (`work/r6-t11e-downstream-1` to `-4`) and ASan/UBSan products 1 of 1 (`work/r6-t11e-asan-stop-downstream`), each 14 of 14 checks; `-4` ran the committed harness, and the other four ran it before the check was renamed (its old name was `downstream-processes-once-per-record-after-release`), with the same product, fixture and test binary. The stop-inflight and stop-enqueue-failed cases pass again on both products (`work/r6-t11e-stop-inflight-1`, `work/r6-t11e-enqueue-1`, `work/r6-t11e-asan-stop-inflight`, `work/r6-t11e-asan-stop-enqueue-failed`) and the three stop controls are still detected (`work/r6-t11e-controls/results.json`). No defective copy exists for this case because the product only calls the record's `process` and Base owns the forward link; a stop that waited for the downstream would fail `held-downstream-does-not-hold-the-stop`. With this case every cell D13 lists for T11 is executed. Products before D7 (`c065755`), 2026-10-03: with the worker stopped at the worker-signal boundary, a Deadline-selected predecessor stayed retirement-pending, but no successor was queued because the reprocess was rejected; drain succeeded and the worker was reaped about 1.5 s after stop (`work/r6-stop-queued-head-control-20261003-231614/`). A worker that is not reaped within the stop bound cannot be produced through the permitted boundaries, because a stopped process still exits on SIGKILL; by owner decision of 2026-10-04 that cell is no longer required (see Dependencies And Decisions). D7/D9 products (`0b60abf`), 2026-10-04: with a stopped, retirement-pending predecessor and a queued successor, `snmp3Stop` completed the successor as Stopping with FLNK once and PACT cleared, drain succeeded, and the predecessor was reaped about 1.5 s later (`work/r6-d9-stop-queued-20261004-000528/`). |
| T12 | 2026-10-01; 2026-10-05 | Actual isolated queue cleanup and production non-isolated IOC exit | Pending; subset PASS | Contexts survive queued ownership and are cleared only after actual isolated queue cleanup; abandoned terminal finalized once. Production IOC normal exit runs Base AtShutdown through AfterShutdown, verifies the actual worker PID is absent after reap, closes completion entry and reports eight retained inactive contexts without isolated queue destruction. In-flight non-isolated retention, record startup-failure and remaining live-detach cells are unexecuted. Isolated rebuild and reuse beyond the D7 clause, including delayed or abandoned queued completions, lsi/lso pointer-backed buffers and retained maximum-capacity payloads, is deferred to M11 by D13 (the `rebuild` case below retains only an ao record and its queued successor). Isolated rebuild after a stop that retained a retirement-pending and a queued generation, 2026-10-05 (case `rebuild`, sources of `2544819`): the first activation is stopped with a stopped worker, a retirement-pending predecessor and a queued Base reprocess and is cleaned up in isolation; the same process then loads the databases again and starts a second activation (number 2) on a new worker, the first worker is gone, the complete baseline passes on the second activation and the report counters start again at zero (ordinary and instrumented products: `work/r6-matrix-20261005-123214/record-rebuild/`, `work/r6-matrix-20261005-123214/san-record-rebuild/`); the control `rebuild-reuses-scheduler` fails the case checks while the unmodified products pass. The case does not assert that the configuration stays frozen across the rebuild; an expired drain blocking restart is covered by the shutdown case and RuntimeTest. |
| T13 | 2026-10-02; 2026-10-03; 2026-10-05 | Ordinary and separate ASan/UBSan products | Pending; subset PASS | Numeric matrix, current edges and production IOC/CA pass on ordinary and separate instrumented products; ordinary output-policy and full native-adapter regressions pass after the shared fixture extension. The new sanitizer build identifies 15 products, including the actual production IOC. Earlier record/pressure/blocked/queued-shutdown products, native-free loader checks and ordinary M1-M5 regressions pass for their recorded versions; component evidence includes 50166 conversion checks. Full targeted T2-T4/T9-T12 qualification is incomplete; D13 disposes of the remaining cells, and each executed cell runs on both the ordinary and the ASan/UBSan products as the T13 Method requires. D7 products (`98bfdfc`), 2026-10-03: ordinary baseline, edges, active, policy, alarms, shutdown, queued-shutdown and numeric record cases PASS (`work/r6-d7-regression-*-20261003-235137/`); the instrumented rebuild and runs are in the 2026-10-05 matrix re-run below. Matrix re-run 2026-10-05 on the sources of `2544819` (clean working tree) in `work/r6-matrix-20261005-123214/`: ordinary products: foundation regressions (independence, lifecycle, config, native API 84 checks and adapter 779 checks), components (15 executions, 212 Scheduler checks), qualification (28 cases), supervisor/IOC (25 checks), operator (15 checks), the 14 record cases (baseline 11, edges 17, alarms 18, active 21, policy 24, numeric 20, shutdown 12, queued-shutdown 12, stop-queued 14, active-unforced 21, accounting 19, deadline-queue 23, near-deadline 19, rebuild 16 checks) and production CA (315 checks) all PASS; separate ASan/UBSan products (`work/r6-matrix-20261005-123214/sanitizer-build/`, leak detection disabled): components, qualification (28), supervisor/IOC (25), the same 14 record cases and production CA (315) all PASS with no sanitizer diagnostic in the retained stderr and logs. The cells recorded as unexecuted in T1 to T12 are not covered by this run. |
| T14 | 2026-10-01; 2026-10-04; 2026-10-05 | Separately built defective support copies; unchanged record/worker/native spans | Pending; subset PASS | Seven actual controls detected: communication-alarm classification, integer precision, text capacity, binary32 tie selection, ambient-mode dependence, callback retry and terminal release. Defective library load and named assertion failure are observed; the stale-generation control is unexecuted and is run under D13: first check whether the D7 frame-validation controls recorded under M5 T7 already cover it; if they do, cite that receipt as the executed control, if not, build the control. 2026-10-04, `bd4dc0c` sources, sanitizer build `work/r6-p010-sanitizers-20261004-152351/`: the D7 record controls were also detected, each failing its named check while that check passes on the unmodified products: lookups keyed by binding only and a stop that finds only one generation per binding fail `queued-successor-completes-stopping-once` of `stop-queued` (in both, PACT of the queued successor stayed set, FLNK ran once and the drain failed), and a copy that restarts a queued deadline at dispatch fails `below-threshold-queued-generation-not-sent` of `deadline-queue`; the unmodified `deadline-queue` run still fails the unrelated `below-threshold-never-sent-message` until the AMSG work is done (done 2026-10-05; see T9). `work/r6-p010-controls-20261004-191025/results.json`; all fourteen D7 controls re-run with the harness of `75abe64`, the three record controls failing the same named checks: `work/r6-p026-controls-20261004-235719/results.json`; all seventeen with the later harness, the record controls again failing the same checks: `work/r6-d9excl-controls-20261005-003623/results.json`. An earlier run (`work/r6-d7-controls-all-20261004-115736/`) had stopped the runtime before the Base reprocess ran and rejected the unmodified `stop-queued` reference; the cell now waits for the reprocess (`docs/snmp-worker-supervision.md`) and its unmodified run passes (`work/r6-p010-stop-queued-20261004-162208/results.json`). Re-run 2026-10-05 on the sanitizer build of `2544819` (`work/r6-matrix-20261005-123214/sanitizer-build/`): the seven controls above detected again (`work/r6-matrix-20261005-123214/controls-older/results.json`, complete and passed) and all 24 D7, AMSG, report, deadline and restart controls PASS (`work/r6-matrix-20261005-123214/controls-d7/results.json`, complete and passed), each failing its named cell or check while the unmodified products pass. |

Supplemental component observation, 2026-10-01: `Conversion.{h,cpp}` and `Request.h` provide checked scalar/text conversion, software binary64-to-binary32 roundTiesToEven, record-link grammar and immutable record-binding metadata. The actual `snmp3ConversionTest` links the production support library and passed 50166 assertions in both ordinary and separate ASan/UBSan products. Checks include explicit binary32 tie/subnormal/signed-zero/overflow bit fixtures, all four ambient rounding modes with floating-point state checks, hardware-rounding comparison, integer precision/range rejection, text capacity/termination and link grammar. Initial receipts: `work/r6-conversion-20261001/results.json` and `work/r6-conversion-sanitizers-20261001/conversion-results.json`. The current products repeat the component checks through `work/r6-components-alarms-20261001/results.json` and `work/r6-components-alarms-asan-20261001/results.json` (15 executions each). This is component evidence; record closure uses the distinct real-path observations above. Dependencies are uninstrumented and leak checks are disabled.

Selected record receipts, executed 2026-10-01 and 2026-10-02 on local Linux x86_64 / Base 7.0.10, each qualifying its recorded source/product version:

| Execution | Ordinary receipt | ASan/UBSan receipt | Actual custom assertions |
| --- | --- | --- | --- |
| Eleven records, conversion, order, simulation, GET/SET callback pressure and maximum payload queue | `work/r6-final-edges-20261001/results.json` | `work/r6-final-asan-edges-20261001/results.json` | 1263 per product |
| Eleven native timeouts and a separate record deadline | `work/r6-final-alarms-20261001/results.json` | `work/r6-final-asan-alarms-20261001/results.json` | 358 per product |
| Five active output/RPRO paths, explicit retry, native retry and lost SET responses | `work/r6-active-readback-20261001/results.json` | `work/r6-active-readback-asan-20261001/results.json` | 443 ordinary; 444 instrumented |
| Output IVOA/OOPT, simulation/SDLY, ao OVAL/drive and lso OMSL/DOL | `work/r6-policy-dol-20261001/results.json` | `work/r6-policy-asan-20261001/results.json` | 1340 per product |
| Production IOC/CA decimal, CHAR-array strings, PINI/FLNK and normal non-isolated exit | `work/r6-ca-final-20261001/results.json` | `work/r6-ca-final-asan-20261001/results.json` | 119 runner checks per product; separate CA executables |
| Entered callback blocked on record lock during shutdown | `work/r6-final-shutdown-20261001/results.json` | `work/r6-final-asan-shutdown-20261001/results.json` | 213 per product |
| Queued callback retained through gate closure and actual cleanup | `work/r6-final-queued-shutdown-20261001/results.json` | `work/r6-final-asan-queued-shutdown-20261001/results.json` | 213 per product |
| Seven real-path defective-code controls | none; separate products only | `work/r6-final-controls-20261001/results.json` | Seven named failures detected; no timeout/forced-kill qualification |
| All numeric GET/SET pairs, range/precision/extrema and rejected-output wire preservation | `work/r6-numeric-commit-20261002/results.json` | `work/r6-numeric-commit-asan-20261002/results.json` | 13155 per product |
| Current edges after numeric fixture extension | `work/r6-numeric-regression-edges-20261002/results.json` | `work/r6-numeric-regression-asan-edges-20261002/results.json` | 1263 per product |
| Current production IOC/CA after numeric fixture extension | `work/r6-numeric-regression-ca-20261002/results.json` | `work/r6-numeric-regression-asan-ca-20261002/results.json` | 119 runner checks per product |

The record receipts retain executed source/product/library hashes, activation/revision/handle/generation/admission, alarms/UDF/publication state and actual child/cleanup outcomes. The instrumented build is identified by `work/r6-alarms-sanitizers-20261001/sanitizer-build.json`; the alarm-stage record-test rebuild is `work/r6-deadline-asan-rebuild-20261001/snmp3RecordTest.build.json`, the active-output rebuild is `work/r6-active-readback-asan-rebuild-20261001/snmp3RecordTest.build.json`, and the output-policy rebuild is `work/r6-policy-asan-rebuild-20261001/snmp3RecordTest.build.json`. Earlier receipts do not qualify subsequent fixture changes. No sanitizer diagnostic or credential sentinel appeared on the executed positive paths. Each defective control records its actual compilation/mutation and loaded defective library rather than substituting an internal span. Full T1-T14 closure remains pending, and `snmp3Report` still reports `recordSupport=unavailable`.

Active output qualification, 2026-10-01: the actual external UDP proxy delays responses by 750 ms. An external callback holds the unchanged Base callback consumer until the accepted SET and a following GET have both retired natively. This qualifies the RPRO path with admission available: FIFO GET observes the first captured payload, RPRO admits the latest prepared value, and a separate GET observes that value. Each of the five outputs runs Base FLNK twice. The response-loss cases drop actual successful SET responses and distinguish operator retry from native-library retry; their explicit GET observations do not add automatic module readback. The retained-native-ownership admission-rejection path is replaced under D7 by admission behind retirement, which the T7 row records as observed (2026-10-03/04).

Output policy qualification, 2026-10-01 at approximately 22:29 PDT: actual Base 7.0.10 processing, native timeout terminals and external callback gating exercise all five output kinds. Completion IVOA and simulation preserve the current requested VAL/LEN, retain COMM/INVALID and finalize terminal/FLNK once; live unsupported OOPT reports LINK/INVALID even when Base conditional_write skips DSET. First-pass Don't drive outputs admits no SET; Continue normally and Set output to IVOV use the prepared value, verified by separate GET after response loss. Pre-existing Base HIHI/UDF INVALID alarms retain their equal-severity priority. Both synchronous and SDLY-driven simulation before admission leave module identity/completion counts unchanged. Actual ao drive/rate limits prepare VAL=5 and successive OVAL/wire values 1 and 2. lso supervisory ignores DOL, closed_loop transfers 200 data bytes, and a 300-byte source is reduced by Base to 255 data bytes/LEN=256 before DSET capture; GET verifies that prepared value. This last observation qualifies the Base boundary, not rejection of the original DOL length. Ordinary and ASan/UBSan cases each pass 1340 assertions; actual proxy metadata counts 38 SETs. Initial fixture failures are retained and do not qualify these results.

Production CA qualification, 2026-10-01: `test_record_ca.py` launches the shipped production Main, generated IOC registrar, unchanged Base CA server, actual worker/native products and real loopback agent. The selected Base caget/caput execute as separate clients with private CA ports and an owned/reaped repeater. Decimal CA STRING preserves 9007199254740993 and 9223372036854775807 through int64out SET and int64in GET; UINT64 waveform returns 18446744073709551615 exactly. lso/lsi VAL$ CHAR arrays preserve 0/39/40/200/255 data bytes and LEN including NUL; ordinary CA STRING exposes 39 data bytes. caput rejects a 300-byte VAL$ write with Invalid element count before put, and subsequent native GET observes the previous 255-byte value. This client rejection differs from the verified Base DOL truncation above. PINI input and its Base FLNK run after servicing is ready; normal non-isolated IOC exit reaches AfterShutdown. Actual /proc observations identify the owned worker PID/start time before exit and require that PID to be absent afterward; final module reports show 19 completions, a closed entry gate, zero active/queued/running requests and eight retained inactive contexts. Ordinary/instrumented receipts each pass 119 runner checks. The instrumented production IOC build is `work/r6-ca-asan-ioc-build-20261001/snmp3Ioc.build.json`, using production Main and its actual generated registrar; the sanitizer builder now declares this product alongside the record/component products. Base, caget/caput and system/vendor dependencies are uninstrumented; leaks are disabled. Long active client writes over Channel Access are deferred to M11 by D13, and in-flight non-isolated teardown is run under D13; neither is qualified yet.

Native timeout alarm qualification, 2026-10-01: actual response dropping produced READ/INVALID in the pre-correction ai path (`work/r6-alarm-before-20261001-run3/results.json`). Current ordinary and instrumented alarm cases observe NativeFailure/native code 2 before completion for all eleven record kinds, followed by COMM/INVALID, without native publication. The separate 1 ms record deadline produces IPC Deadline and COMM/INVALID. The communication-alarm negative control reinstates the defective classification in an actual compiled support library and fails the named ai alarm assertion normally. Session-open/send/cancellation alarm branches are implemented according to the accepted mapping but are not qualified by this timeout case.

Current ordinary foundation/config/lifecycle regressions: `work/r6-foundation-alarms-20261001/results.json`. Worker/IOC/operator regressions: `work/r6-worker-alarms-20261001/results.json`, `work/r6-supervisor-alarms-20261001/results.json` and `work/r6-operator-alarms-20261001/results.json`. Native API and adapter regressions: `work/r6-native-api-alarms-20261001/results.json` and `work/r6-native-adapter-alarms-20261001/results.json`. Current ordinary/instrumented component receipts are `work/r6-components-alarms-20261001/results.json` and `work/r6-components-alarms-asan-20261001/results.json`, with 15 executions and 50166 conversion assertions per product. They pass for their recorded source/product identities; unavailable wrong-owner qualification retains its explicit NOT RUN. Earlier failed initial/pressure/edge/foundation/alarm runs remain retained under `work/` and do not qualify any row.

Numeric qualification, 2026-10-02: ordinary and separate ASan/UBSan cases each pass 13155 custom assertions and 20 runner checks. The selected receipts above qualify the current shipped numeric DB; their source/product hashes match the current fixture and products. The shipped numeric DB contributes 148 contexts; the unchanged baseline contributes 12, for 160 actual initialized contexts. Every advertised numeric GET pair receives a successful exact input through actual output DSET/worker/native SET and subsequent GET; range/precision rejection is then observed on the same initialized inputs. Fixed actual agent OIDs additionally provide unsigned Counter64 2^63/UINT64_MAX and opaque NaN/infinities. The 60 fixed input pairs are initial-input checks; they do not claim failure after an earlier native success. The matrix records 121 conversion failures after successful input, 68 accepted output cases, 67 rejected outputs and 67 stimulus SETs. Actual agent observations contain 141 SET actions: those 135 admitted numeric SETs plus six baseline/pressure SETs. Rejected outputs leave request identity/completion counts and separately read native value unchanged. Waveform rejection preserves complete BPTR bytes and NORD, while UDF follows Base processing and native-success state remains unchanged. The complete new sanitizer build is `work/r6-numeric-sanitizers-20261002/sanitizer-build.json`; all 15 declared products build successfully with the builder's instrumentation checks, including its documented InventoryTest UBSan exception. New code is instrumented, dependencies remain uninstrumented and leaks are disabled. Current ordinary and instrumented edges each pass 1263 assertions; production IOC/CA each passes 119 runner checks. The ordinary policy regression is `work/r6-numeric-regression-policy-20261002/results.json` (1340 assertions), and the complete ordinary native-adapter regression is `work/r6-numeric-regression-native-20261002/results.json` (779 checks). Every receipt qualifies its identified sources/products only. The cells still unexecuted are listed in the Results rows and disposed of by D13: executed in the order it gives, or deferred to M11. Record support is not advertised.

##### Closure Evidence

- none; planning and implementation acceptance remain separate.

#### M7 - R7 APC Migration And Comparison

Origin: 5dff352 / M7
Identity History: none
GitHub Issue: none associated with this register
Status: Blocked

##### Summary

Migrate APC startup/database definitions to the explicit new interface and compare observable behavior through independent consumers.

##### Scope

Production-shaped profile/endpoint/startup/DB examples, baseline source/firmware identities, v2c/v3 communication, actual CA/PVA consumers and operation/order/alarm/recovery comparison. Record intentional differences and baseline defects separately.

Include startup error propagation tests using a device loader invoked by an enclosing startup script, with `on error break` in both scripts. Exercise the existing common configuration parser, iocsh wrapper and IOC main without replacing internal spans.

Out of scope: E2D parser migration, implicit legacy translation, final-value-only equivalence claims, firmware updates and unapproved installation/equipment actions.

##### Completion Criteria

- Real new-module APC methods and consumers establish the approved wire/record/order/alarm/recovery behavior for the identified target.
- The actual installed IOC, snmp3 driver and nested device loader reject invalid local configuration for both the first and a later device with a nonzero process exit before `iocInit` or interactive-shell entry. Valid SNMPv1/v2c/v3 configuration reaches `iocInit`; credentials remain absent from logs.

##### Dependencies And Decisions

- M6 supplies actual record support; D1 fixes the interface direction.
- G2 remains Open; resume as Not started when approved. Its target/equipment boundary must be explicit before any live operation.
- Decision Date: 2026-10-01. Add nested startup error propagation qualification to this milestone; exclude E2D work. Existing single-script configuration tests do not establish the installed nested-loader acceptance result.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Identify the comparator, target and permitted equipment scope; prepare explicit startup/DB migration and consumer procedures.
2. Extend the shipped fixtures and runner under `tests/rewrite/` with a device loader and enclosing startup script using `on error break` in both. Cover invalid first-device and later-device configuration, valid SNMPv1/v2c/v3 configuration, credential-free diagnostics and actual startup/exit observations. Identify the installed IOC, driver, configuration helper and script paths and retain their hashes in the execution receipts.
3. After M6 and G2 complete, execute the actual new-module APC paths, compare wire and record phases, and run the installed nested-loader startup matrix. Record test observations separately from fixture preparation.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | APC integration | Real new-module startup/DB, v2c/v3 agents and identified CA/PVA consumers. | Approved target and identified firmware/source | Correct values, writes, alarms and recovery through the new interface. |
| T2 | External comparison | Independent baseline/new observation of SET values/order/retries and response/record phases. | Same approved comparison conditions | Differences are attributable and no final-value-only equivalence is asserted. |
| T3 | Installed startup integration | Run the shipped nested device loader and enclosing startup script with the actual installed IOC and driver. Inject invalid local configuration at the first device and after an earlier valid device; also run valid SNMPv1/v2c/v3 cases. Inspect process exit, `iocInit` and interactive-shell observations, and all captured logs for credential sentinels. | Approved unchanged installation; identified IOC, driver, native helper and script hashes; disposable local configuration/secret fixtures | Both error positions terminate with nonzero exit before `iocInit` and interactive-shell entry; valid cases reach `iocInit`; no credential values appear in logs. No internal parser, wrapper or IOC main substitutes. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Target not yet approved | Pending | none |
| T2 | Not run | Comparator not yet approved | Pending | none |
| T3 | Not run | Installed IOC and nested-loader fixtures not yet qualified | Pending | Existing single-script tests do not close this check. |

##### Closure Evidence

- none.

#### M8 - R8 Integrated Qualification

Origin: 5dff352 / M8
Identity History: none
GitHub Issue: none associated with this register
Status: Blocked

##### Summary

Qualify the complete identified implementation across both selected OS environments, consumer/fault paths, supported sanitizers and sustained resource measurements.

##### Scope

Debian 13 and Rocky 8.10 full record/native/lifecycle/fault/consumer matrices; actual negative controls; ASan/UBSan/TSan where the unchanged environment supports execution; 300-second warmup and 3600-second mixed traffic. Measure IOC and every worker RSS, FD, threads, CPU, sessions, queues and completion counts, and map accepted requirements to actual evidence.

Out of scope: silently accepting unsupported sanitizer cells, inferring total memory from module storage bounds, package/image changes and release publishing.

##### Completion Criteria

- Complete real-path matrices identify source/products/libraries on both OSes and retain actual control failures and cleanup observations.
- Resource thresholds are accepted from observed trends; unavailable cells have explicit dispositions and are never reported as PASS.

##### Dependencies And Decisions

- M6 and M7 supply the complete record and consumer paths.
- G3 remains Open; resume as Not started after environments, measurement policy and execution are approved.
- This is qualification, not a versioned release or deployment milestone.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Define identified normal/instrumented products, both OS environments, actual negative controls and measurement/acceptance rules.
2. After prerequisites and G3 complete, run the full shipped paths and sustained workload, then map each requirement to current evidence and explicit limits.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Full integration | Complete real record/native/lifecycle/fault/consumer matrix with actual negative controls. | Debian 13 and Rocky 8.10 | Every approved contract has identified actual execution evidence. |
| T2 | Instrumented integration | Separate supported ASan/UBSan/TSan products exercise the complete shipped paths. | Selected unchanged OS images | New-path defects fail their cells; instrumentation boundaries are explicit. |
| T3 | Sustained resources | Actual 300-second warmup and 3600-second mixed traffic; monitor IOC and all workers. | Approved measurement environments | No missed ownership/cleanup; measured trends satisfy separately accepted thresholds. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Planned two-OS complete implementation | Pending | Current component results are not this integrated qualification. |
| T2 | Not run | Planned supported full-path instrumented products | Pending | none |
| T3 | Not run | Planned sustained workload | Pending | none |

##### Closure Evidence

- none.

#### M9 - mdBook Documentation

Origin: 5dff352 / M9
Identity History: none
GitHub Issue: none associated with this register
Status: Blocked

##### Summary

Provide navigable English operator/developer documentation over the maintained snmp3 architecture, configuration, native transport, worker supervision and verification instructions.

##### Scope

Book configuration, curated SUMMARY navigation, reproducible local build, rendered-page/link verification and a build automation proposal. Evaluate source layout and toolchain choices using the existing epics-ioc-runner and epicsarchiverap-maven implementations. Keep one maintained copy of each product document and exclude milestone/private execution records from operator navigation.

Out of scope: implementing or publishing the book through this register update, silently selecting a hosting target, copying example project titles/URLs or old workflow versions, installing tools and changing unrelated runtime behavior.

##### Completion Criteria

- The selected book toolchain builds the actual snmp3 chapters; source and rendered links resolve, navigation covers the maintained product scope and documented commands match actual shipped behavior.
- Generated output is excluded from source tracking; local and proposed automation builds use the same identified tools. Any publication phase has separate explicit authorization and its own observed result.

##### Dependencies And Decisions

- M1-M5 provide current product documentation; D4 assigns this milestone. It need not wait for M6-M8, except that M5's reclosure under D7 needed the current-product executions of M6 step 7, which were recorded on 2026-10-03/04 and completed M5 on 2026-10-05; later implemented behavior updates the maintained chapters.
- G4 remains Open; resume as Not started after layout/toolchain/build scope and implementation are approved. Publication is not authorized by this assignment.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Select an in-place or dedicated book source layout, curated chapter list and identified compatible toolchain; propose automation and publication boundaries explicitly.
2. After G4 closes, add the book configuration/navigation/build support and generated-output exclusions while preserving the maintained document content.
3. Run the real build and inspect rendered navigation/links and documented commands. Implement a publication phase only if separately approved.

The inspected epics-ioc-runner example uses root `book.toml`, `src = "docs"`, `docs/SUMMARY.md`, `public/` output and a container-based documentation workflow. The inspected epicsarchiverap-maven example uses `docs/book/book.toml`, dedicated `docs/book/src/SUMMARY.md`, a version/checksum-pinned Dockerfile and a Pages workflow using that same image. These are alternative planning inputs; neither layout nor its tool versions is selected for snmp3 by this record.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Documentation build | Execute the real mdBook build against selected snmp3 book sources with the identified toolchain. | Approved local build environment | Build exits successfully; warnings are examined and not hidden. |
| T2 | Rendered documentation | Inspect actual generated HTML, navigation, local assets and links, including subpath behavior if publishing is approved. | T1 output | Correct title/chapter coverage and resolved rendered targets; internal records excluded. |
| T3 | Documentation accuracy | Compare chapters with current contracts and execute the documented shipped IOC/test procedures appropriate to their claimed scope. | Identified current snmp3 products | Commands and outcomes match real behavior; record/APC support is not advertised before qualification. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | snmp3 book/toolchain not selected | Pending | Existing example builds do not qualify snmp3. |
| T2 | Not run | No generated snmp3 book | Pending | none |
| T3 | Not run | No snmp3 book chapters selected | Pending | Existing product receipts do not establish a book verification. |

##### Closure Evidence

- none; assignment records documentation work, not implementation or publication acceptance.

#### G1 - R6 Authorization

Origin: 5dff352 / G1
GitHub Issue: none associated with this register
Status: Complete

##### Summary

The detailed R6 record/conversion/completion/test plan was accepted and separately authorized for implementation on 2026-10-01. This gate applies only to M6 and does not establish product verification.

##### Completion Criteria

- D5/D6 record the accepted IEEE ao-to-OpaqueFloat rounding and all other precision/capacity error-rejection policies. Record acceptance of the identified current M6 plan and separate implementation authorization with their decision dates and exact scope.

##### Verification Results

| Observed At | Result | Evidence |
| --- | --- | --- |
| 2026-10-01 | Complete | Separate plan acceptance and explicit implementation authorization recorded for the reviewed M6 plan; reviewed document SHA256 `cb80ae8c978ad5b06e4ae046ca4ac648f5f29634c13e3e5709c6096f6c77eecb`. |

##### Closure Evidence

- 2026-10-01: M6 plan accepted and separately authorized for implementation; scope is the eleven-record integration, T1-T14 and required documentation. R7-R8, mdBook implementation, installation, equipment operations and Git/remote mutations remain excluded.

#### G2 - R7 Authorization

Origin: 5dff352 / G2
GitHub Issue: none associated with this register
Status: Open

##### Summary

The owner must accept the identified M7 migration/comparison plan and authorize its exact target, credentials/consumer handling and permitted equipment actions.

##### Completion Criteria

- Record accepted target/comparator scope and separate implementation/operation authorization; installation and firmware changes remain excluded unless explicitly added.

##### Verification Results

| Observed At | Result | Evidence |
| --- | --- | --- |
| 2026-10-01 | Pending | No detailed R7 acceptance or execution authorization is recorded. |

##### Closure Evidence

- none.

#### G3 - R8 Authorization

Origin: 5dff352 / G3
GitHub Issue: none associated with this register
Status: Open

##### Summary

The owner must accept M8 environments, real-path matrix, supported instrumentation and resource measurement/acceptance policy, then authorize execution.

##### Completion Criteria

- Record acceptance and separate execution authorization for the identified qualification plan and explicit unavailable-cell/resource dispositions.

##### Verification Results

| Observed At | Result | Evidence |
| --- | --- | --- |
| 2026-10-01 | Pending | No detailed R8 acceptance or execution authorization is recorded. |

##### Closure Evidence

- none.

#### G4 - mdBook Authorization

Origin: 5dff352 / G4
GitHub Issue: none associated with this register
Status: Open

##### Summary

The owner assigned mdBook work on 2026-10-01. The detailed M9 layout/toolchain/build plan and implementation authorization remain pending; assignment does not authorize publication.

##### Completion Criteria

- Record acceptance and separate implementation authorization for the identified M9 plan. Any hosting/publication decision names its scope separately.

##### Verification Results

| Observed At | Result | Evidence |
| --- | --- | --- |
| 2026-10-01 | Pending | mdBook milestone assignment is recorded; no detailed-plan acceptance or implementation/publication authority is recorded. |

##### Closure Evidence

- none.

#### G5 - D7-D9 Revision Authorization

Origin: 5dff352 / G5
GitHub Issue: none associated with this register
Status: Complete

##### Summary

The D7-D9 revisions of the M5 and M6 Implementation Plans (2026-10-03) needed owner acceptance and separate implementation authorization before M5 steps 4-6, M6 steps 7-8 or any remaining M6 T1-T14 work proceeded; both were recorded on 2026-10-03. This gate does not establish product verification.

##### Completion Criteria

- Record acceptance of the revised M5 and M6 plans and separate implementation authorization with their decision dates, exact scope and the commit that carries the accepted text, in this gate and in each plan's `Plan Acceptance` and `Implementation Authorization` fields.

##### Verification Results

| Observed At | Result | Evidence |
| --- | --- | --- |
| 2026-10-03 | Complete | Owner acceptance of the revised M5 and M6 plans and separate implementation authorization; accepted text carried by `c065755d7677e168c6fadae560012b7d9b1dfc47`. |

##### Closure Evidence

- 2026-10-03: revised M5 and M6 plans accepted and separately authorized for implementation; scope is M5 steps 4-6 with T1-T7 and M6 steps 7-8 with the remaining T1-T14 qualification. R7-R8, mdBook implementation, installation, equipment operations and Git/remote mutations remain excluded.

## Backlog

### Work

| ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| M10 | Negative-control harness outcome classification | Carry-forward | Open | No | none | Owner scope decision recorded and each listed outcome either classified by an executed run or kept by decision; [detail](#m10---negative-control-harness-outcome-classification) |
| M11 | Deferred record verification cells | Carry-forward | Deferred | No | D13 | Deferred by D13 on 2026-10-05; the cells run when the owner asks for Channel Access, long-write, delayed-simulation or rebuild/reuse record verification; [detail](#m11---deferred-record-verification-cells) |
| M12 | Stop duration with a blocked callback queue | Carry-forward | Open | No | none | Owner decision on whether the stop bound and the record drain budget may overlap or skip waits that cannot progress, recorded with its date; then the change verified by the `stop-enqueue-failed` case or the observation kept by decision; [detail](#m12---stop-duration-with-a-blocked-callback-queue) |

### Backlog Details

The eight rewrite checkpoints and mdBook work are assigned under Milestone.

#### M10 - Negative-control harness outcome classification

Origin: 5dff352 / M10
Identity History: none
GitHub Issue: none
Status: Open (unresolved scope; held for discussion by owner direction of 2026-10-04)

##### Summary

`tests/rewrite/test_record_controls.py` qualifies a D7 control only when its
reference passes and the defective run fails without aborting; a record run
killed at its child bound or a stop-queued run without its `stop_queued` event
qualifies neither. Several outcomes outside that rule are not yet classified or
have never run on the real path.

##### Scope

- Whether qualification cells apply the same forced-cleanup rule as record
  cells. In the D7 runs of 2026-10-04 (for example
  `work/r6-p010-controls-20261004-162208/`), the defective
  `binding-lookup-qualification` copy shows `forced_cleanup` true in the IPv4
  run's `behind-retirement-4/qualification.receipt.json` while its reference
  shows false.
- The 900 s bound in `run_cell` raises and ends the whole D7 run instead of
  recording that cell's outcome.
- The stop-queued `rejected`, `timeout`, inconclusive, forced-kill and
  missing-event outcomes have been checked by code reading only; a sanitizer
  abort after the `stop_queued` event is judged on the product checks alone.
- The trial failure line is reported only in `records.stderr` among loader
  diagnostics; the recorded `stderr_tail` comes from the harness stderr.
- SchedulerTest controls record no failing check name; `cell.stderr` reports
  only the failing check's position in the cell, which must be counted in
  `tests/rewrite/SchedulerTest.cpp`.

Out of scope: the D7 product changes and the stop-queued wait rule recorded in
`docs/snmp-worker-supervision.md`.

##### Completion Criteria

- An owner decision fixes which outcomes qualify, abort or are judged.
- An owner decision fixes whether the trial failure line is surfaced in the
  control results and whether SchedulerTest controls name their failing check.
- Each outcome kept active is classified by an executed real-path run, or its
  absence is recorded by decision.

##### Dependencies And Decisions

- none

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Discuss the scope items with the owner and record the decision.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Negative-control harness | Run the decided outcomes through `test_record_controls.py --d7-controls` on an identified sanitizer build | Linux host, sanitizer products | Each decided outcome is classified as recorded |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, sanitizer products | Pending | none |

##### Closure Evidence

- none

#### M11 - Deferred record verification cells

Origin: 5dff352 / M11
Identity History: none
GitHub Issue: none
Status: Deferred (D13, 2026-10-05)

##### Summary

Record verification cells that M6 planned but D13 moved out of M6 on 2026-10-05, so that M6 can close on the executed cells. They are not executed and the record support does not claim them.

##### Scope

- Channel Access variants: signed-minimum int64 access for int64in/int64out (M6 T2, T4), maximum-capacity lsi/lso `VAL$` access (M6 T3) and ao/longout writes over Channel Access (M6 T4).
- Long active client writes of M6 T7 as that row names them; their definition is fixed when the work starts.
- The mode change of a delayed output simulation (M6 T8).
- Isolated rebuild or reuse beyond the D7 clause that the `rebuild` case closes, including delayed or abandoned queued completions, lsi/lso pointer-backed buffers and retained maximum-capacity payloads (M6 T12).

Out of scope: every other M6 cell, which D13 executes under M6.

##### Completion Criteria

- Each cell above has a real-path receipt on identified ordinary and instrumented products, or an owner decision retires it with its date.

##### Dependencies And Decisions

- D13 (2026-10-05) defers these cells and excludes them from the M6 Completion Criteria and Test Plan.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Define each cell against the shipped record, Channel Access and agent fixtures and name its failing check.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Channel Access | Run the signed-minimum, maximum-capacity and ao/longout cells through the production IOC and Base Channel Access clients | Linux host, ordinary and instrumented products | Values and alarms match the M6 conversion rules |
| T2 | Active writes and simulation | Run the long active client writes and the delayed output-simulation mode change against a delayed SET | Linux host, ordinary and instrumented products | Completion, alarms and ownership match the M6 policy cells |
| T3 | Isolated rebuild | Rebuild isolated databases with delayed or abandoned completions, lsi/lso buffers and maximum-capacity payloads | Linux host, ordinary and instrumented products | No post-detach access; ownership released as in the `rebuild` case |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary and instrumented products | Deferred | none |
| T2 | Not run | Linux host, ordinary and instrumented products | Deferred | none |
| T3 | Not run | Linux host, ordinary and instrumented products | Deferred | none |

##### Closure Evidence

- none

#### M12 - Stop duration with a blocked callback queue

Origin: 5dff352 / M12
Identity History: none
GitHub Issue: none
Status: Open (unresolved scope: whether the observed stop duration is acceptable)

##### Summary

With the Base callback queue blocked, a runtime stop takes the sum of its sequential waits rather than one bound. Observed by M6 T11 on 2026-10-05 with the `stop-enqueue-failed` case (tests `17c494a`, product sources `2544819`): the supervisor waits its full 2 s stop bound although the worker is reaped at about 31 ms, because the terminals borrowed by the records keep the scheduler unsettled and only a completion can release them, and the completion callbacks are what the queue blocks; the record drain then starts its own 2 s budget and retries the same refused enqueues. Code at `2544819`: the bound `StopBound` (`Supervisor.cpp:20`) and the exit predicate `Supervisor::finished` (`Supervisor.cpp:284-288`), waited for by the runtime thread loop (`Runtime.cpp:119-126`); the drain `Requests::drain` (`Request.cpp:172-191`). Observed: 2.5 s when the queue is released 0.5 s after the bound (the stop ends at the release), 4.0 s when it is released after the drain budget. A reading of the shutdown path, not a dedicated run, suggests that after a failed drain the IOC shutdown hook (`Register.cpp:108-110`) calls the stop again, its repeated-stop branch (`Runtime.cpp:169-176`) drains again, and `service()` returns at once while producers are closed (`Request.cpp:99`), so that drain spins another 2 s with nothing able to change (the `after` process took 6.8 s against 3.3 s for `late`).

##### Scope

- Decide whether a stop with a blocked callback queue may take up to the sum of the stop bound, the drain budget and a repeated drain, or whether waits that cannot progress are to be shortened: for example the record drain running before or alongside the supervisor bound, the supervisor bound not waiting for terminals borrowed by records, or a repeated stop after a failed drain not draining again.
- If a change is chosen: implement it in the runtime stop sequence, keep the `stop-enqueue-failed`, `stop-inflight`, `stop-queued`, `shutdown` and `queued-shutdown` cases and the controls passing, and record the new durations.

Out of scope: the Base callback queue size and priorities; the drain budget value itself unless the decision names it.

##### Completion Criteria

- A dated owner decision is recorded in Dependencies And Decisions.
- If the decision keeps the current behavior, Closure Evidence cites this row's observation and the decision date.
- If the decision changes the stop sequence, the `stop-enqueue-failed` case passes on ordinary and instrumented products with the new durations recorded; the `stop-inflight`, `stop-queued`, `shutdown` and `queued-shutdown` cases still pass on both products; and the controls `drain-without-retry`, `stop-queued-not-selected` and `waveform-busy-held` are still detected.

##### Dependencies And Decisions

- none yet; the owner decision is pending.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Confirm by a dedicated run whether the repeated stop at IOC shutdown drains again after a failed drain.
2. Design the shortened wait the owner chooses and its effect on the stop-inflight, stop-queued, shutdown and queued-shutdown cases.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Stop with a blocked queue | Run the `stop-enqueue-failed` case in its three release modes and read the stop durations | Linux host, ordinary and instrumented products | Durations match the decided bound; every check of the case passes |
| T2 | Other stop cases | Run the `stop-inflight`, `stop-queued`, `shutdown` and `queued-shutdown` cases on the changed products | Linux host, ordinary and instrumented products | Every check passes as before the change |
| T3 | Controls | Run `drain-without-retry`, `stop-queued-not-selected` and `waveform-busy-held` on the changed products | Separate instrumented products | Each control still fails its named check while the unmodified products pass |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary and instrumented products | Pending | The M6 T11 receipts record the current durations. |
| T2 | Not run | Linux host, ordinary and instrumented products | Pending | none |
| T3 | Not run | Separate instrumented products | Pending | none |

##### Closure Evidence

- none
