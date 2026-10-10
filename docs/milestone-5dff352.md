# snmp3 Work Register

Release line: unversioned snmp3 rewrite
Milestone index: 5dff352
Canonical path: `docs/milestone-5dff352.md`
Canonical branch or ref: `feature/snmp-base-7.0.10-rewrite`
Git upstream: none configured at initial creation
Remote tracker: none associated with this register
Recorded date: 2026-10-01
Source baseline: `5dff352b9e86abfca74a74c4adc4fc8c1e48079f`

Next session entry point: `docs/milestone-5dff352.md`, M7 Local AP8932 Execution Scope and Verification Results. Both local implementation reviews are complete; retain the qualified local IOC and evidence, and obtain owner direction for the remaining comparator, compatibility and hardware scope before extending execution. Full M7 remains Blocked, G2 Open and M11 Deferred. No commit, push, sibling change, equipment operation, memory or handoff is authorized by this acceptance.

## Scope

This register owns the R1-R8 rewrite checkpoints and the assigned mdBook documentation work. M1-M8 retain their original R1-R8 scope names; M9 is the documentation addition. M, G and D identifiers are local to this canonical document. Architecture and operator contracts remain in their existing documents.

Out of scope: changing implementation or installed dependencies through this documentation update, operating equipment, publishing a site, or changing Git history or remote state.

Complete means the identified checkpoint scope and its qualified verification are complete. It does not close later integration requirements. R6 implementation was authorized on 2026-10-01 (G1) for a plan now superseded by the D7-D9 revision; G5 accepted and authorized that revision on 2026-10-03; M5 implementation under it completed on 2026-10-05 and M6 completed within its accepted scope on 2026-10-08. M6 Closure Evidence identifies landed deliverables, actual verification and retained exclusions. R7 has an authorized local AP8932 subset in progress; full R7, R8 and mdBook implementation remain gated. Their Blocked status records the missing detailed-plan acceptance and separate implementation authorization; planning may proceed before those gates close. Ready is an execution dependency indicator, not implementation authorization.

Implemented source and executable fixtures are carried by `49d5c62feca95f12a13910b73b60667455773b7c`; architecture/operator documents by `81cdcddd4fb431cba55e5a7999c9f7c923a670e6`; Python cache exclusions by the source baseline above. The current identity is snmp3.

### Verification Boundary

Paths written `<local>/...` in this register and in `docs/decisions/` name receipt, plan and session
directories that are retained outside the repository and are not tracked; the digests recorded with
them identify their content.

Observed results below are retained executions, not new tests performed when this register was written. Current receipts are local JSON records under `<local>/name-change-20261001/`; they identify source, product and loaded-library hashes. Receipt digests below fix the selected records. The shipped [verification procedures](../tests/rewrite/README.md) provide the actual runners. Private receipts and this register are not intended as mdBook operator chapters.

The current configuration receipt records wrong-owner secret-file testing as NOT RUN: a differently owned regular fixture was unavailable. The earlier owner-assisted PASS is not transferred to renamed products. ASan/UBSan results cover the executed new code; dependencies are uninstrumented and leak checks are disabled. The standard-record fixture does not establish SNMP record I/O. M6 Current Required Verification Results and Closure Evidence identify accepted record/FLNK/callback observations, deferred cells and explicit limits; dated earlier results retain their historical product identities. APC hardware/consumer behavior, two-OS qualification, TSan and sustained resource acceptance remain later work.

## Milestone

### Work

| ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| M1 | R1: independent Base 7.0.10 foundation | Milestone | Complete | No | D1, D2 | Independent real IOC build/load and dependency audit; [detail](#m1---r1-independent-base-foundation) |
| M2 | R2: Base lifecycle and thread ownership | Milestone | Complete | No | M1 | Real pre-PINI start, stop, join and isolated reuse; [detail](#m2---r2-base-lifecycle) |
| M3 | R3: immutable configuration, Binding and Value | Milestone | Complete | No | M2 | Qualified configuration/security/ownership matrix with explicit NOT RUN; [detail](#m3---r3-immutable-configuration) |
| M4 | R4: native SNMP adapter | Milestone | Complete | No | M3 | Actual GET/SET, security, response and native lifetime tests; [detail](#m4---r4-native-snmp-adapter) |
| M5 | R5: address queues, IPC and worker supervision | Milestone | Complete | No | M3, M4, D3, D7, D9, D10, G5 | Actual isolated worker, deadline, recovery and IOC qualification, including the D7 per-handle bound and D9 containment/backoff rules; [detail](#m5---r5-address-worker-supervision) |
| M6 | R6: record DSET, conversion, completion and FLNK | Milestone | Complete | No | M1, M2, M3, M4, M5, D5, D6, D7, D8, D11, D12, D13, G1, G5 | Required real-path verification and capability landed at ade4c58; D13/M11 exclusions and dated limits retained; [detail](#m6---r6-record-integration) |
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
| T1 | 2026-10-01, retained execution | Linux x86_64; Base 7.0.10 | PASS, 27 checks | `<local>/name-change-20261001/regressions/lifecycle/independence/results.json`; SHA256 `60c65cf0b3add09d859dc6a9045125ec76d80cabc9a2d4be20b214bb9a621e50` |

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
| T1 | 2026-10-01, retained execution | Linux x86_64; Base 7.0.10 | PASS, 38 checks | `<local>/name-change-20261001/regressions/lifecycle/results.json`; SHA256 `117b1d0772582e769d0167220d6aea162cd1bf5809d80d155f3c3becfded53ee` |

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
| T1 | 2026-10-01, retained execution | Linux x86_64; Base 7.0.10 | PASS, 667 checks with explicit NOT RUN | `<local>/name-change-20261001/regressions/results.json`; SHA256 `534645493dfde52d15625243e624d4df4b7de4263e2b3d5d862178051c73ae31` |
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
| T1 | 2026-10-01, retained execution | Linux x86_64 | PASS, 84 checks | `<local>/name-change-20261001/native-api/results.json`; SHA256 `a5e2a8977aa2146044f92bb41faf2233eea7e06fc960ef2cd484b215109d99b1` |
| T2 | 2026-10-01, retained execution | Linux x86_64 | PASS, 779 checks | `<local>/name-change-20261001/native-adapter/results.json`; SHA256 `fe849c9906f1c908640e38a833be021c9bf4d20e9aa09b46ea44179822f37922` |
| T3 | 2026-10-01, retained execution | Separate ASan/UBSan adapter | PASS, 779 checks | `<local>/name-change-20261001/native-sanitizer/results.json` and its identified product/library receipts |

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
| T1 | 2026-10-01, retained execution; 2026-10-04 | Normal component products; revised D7/D9 products | PASS, 14 executions; re-run on the revised products PASS, 15 executions (the D7-discriminating Scheduler cells are recorded under T5) | `<local>/name-change-20261001/components/results.json`; revised: `<local>/r6-d9-components-20261004-023844/results.json` |
| T2 | 2026-10-01, retained execution; 2026-10-04 | Real worker/native/agent path; revised D7/D9 products | PASS, 26 cases; re-run on the revised products PASS, 28 cases including the two T6 cases | `<local>/name-change-20261001/qualification/results.json`; SHA256 `3bf4d00cd34f3ce2a78fac083b1e2e5d2581c5fe88a769df9dd27e6aa3d61de8`; individual case receipts identify executed products; revised: `<local>/r6-p009-qual-all-20261004-001951/results.json` |
| T3 | 2026-10-01, retained execution; 2026-10-04 | Actual IOC and production commands; revised D7/D9 products | PASS, 25 IOC and 15 operator checks; re-run on the revised products PASS, 25 supervisor/IOC checks including the 8 IOC checks and `snmp3RuntimeTest`, and the operator checks | `<local>/name-change-20261001/ioc/results.json`, `operator/results.json`; revised: `<local>/r6-d9-test_supervisor-ioc-20261004-014730/results.json`, `<local>/r6-d9-test_r5_operator-20261004-000357/results.json` |
| T4 | 2026-10-01, retained execution; 2026-10-04 | Separate instrumented products; revised D7/D9 products (ASan/UBSan, leak detection disabled; sources of `1100605`, build `<local>/r6-m5-t4-sanitizers-20261004-205058/`) | PASS; 26 qualification cases, component/IOC matrices and 6 live workers; re-run on the revised products PASS: 28 qualification cases, the 15 component executions, 25 supervisor/IOC checks and 6 live workers, with no sanitizer diagnostic in any retained stderr | `<local>/name-change-20261001/sanitizer/results.json`, `sanitizer-components/results.json`, `sanitizer-ioc/results.json`, `live-worker-ioc/spawn-audit.json`; revised, under `<local>/r6-m5-t4-20261004-205058/`: `sanitizer/results.json`, `sanitizer-components/results.json`, `sanitizer-ioc/results.json`, `live-worker-ioc/spawn-audit.json` (the audit script is kept in that directory); each run retains its own product identities |
| T5 | 2026-10-03, products before D7 (negative control); 2026-10-04 | Normal component products before and after the D7 change; instrumented component products after it | Before D7: FAIL as expected; the 89 existing Scheduler checks and the new cell's 4 preconditions pass and its admission check fails (`admitted:false`). Revised products: PASS; the admission cell admits behind retirement (`retirementPending=1`, `queued=1`, `count=2`, `bytes=q1+q2`, counters 1), rejects an unconsumed or borrowed predecessor and a third generation, the grace cell holds an answered member until one second after its deadline, and with a retirement-pending predecessor and a queued successor `expire` and `stop` act on the successor as never-sent work while `reaped` retires only the predecessor; 151 Scheduler checks. The charged generation node is tied to the container type at compile time. Instrumented component products (ASan/UBSan, sources of `1100605`): PASS, 164 Scheduler checks and the inventory cell. After the third-person review of the product code, 2026-10-05, the cells `grace-outcomes` (NativeFailure answered member held through the grace, a late result contained without grace), `behind-classification` (never-sent counters count only generations behind a predecessor) and `take-identity` (a foreign admission number or generation borrows nothing) were added in `75abe64` (product sources unchanged from `1100605`): 197 Scheduler checks PASS on ordinary and on instrumented component products. Added after `baad1ee`, the cell `grace-exclusion` (a ChannelFailure, WorkerFailure or Stopping member is contained at its deadline, without grace) brings the run to 212 Scheduler checks PASS on ordinary and on instrumented component products (`<local>/r6-d9excl-components-ordinary-20261005-003623/results.json`, `<local>/r6-d9excl-components-sanitizer-20261005-003623/results.json`) | Before: `<local>/r6-scheduler-head-control-20261003-225831/results.json`; revised: `<local>/r6-d9-components-20261004-023844/results.json`; instrumented: `<local>/r6-m5-t4-20261004-205058/sanitizer-components/results.json`; with the three added cells: `<local>/r6-p026-components-ordinary-20261004-235719/results.json`, `<local>/r6-p026-components-sanitizer-20261004-235719/results.json` |
| T6 | 2026-10-04 | Actual snmp3RecordTest and qualification worker/native paths; near-deadline: outer UDP delay 300 ms, record budgets 314-322 ms, one fresh process per trial | Baseline on the products before D7 with the committed test: in 7 of 10 trials the result was accepted one service step before the deadline (record NO_ALARM) and the worker was still contained; 2 completed without containment and 1 ended Deadline. Revised products: PASS. Near-deadline: Retired arrived after the deadline in 10 of 10 trials; in the 9 trials answered before the deadline no worker was contained, and the remaining trial's response came after its deadline and ended Deadline with containment. Behind-retirement case: with the worker stopped, the successor admitted behind the Deadline predecessor was dispatched only after reap, relaunch 250 ms later and Ready in epoch 2; a 200 ms successor expired unsent (counters admitted 2, never sent 1); the second containment relaunched after 250 ms, the first backoff step, because the matched Retired reset the failure count. Stale-frame case: forged Result/Retired frames carrying the queued successor's identity arrived while it was queued in 5 of 5 trials, were rejected (event 16, stale identity) without selecting the successor early, and every generation completed once | Baseline: `<local>/r6-headc-near-deadline-20261004-014819/results.json`; revised: `<local>/r6-d9-near-20261004-000508/results.json`, `<local>/r6-p009-qual-all-20261004-001951/results.json` |
| T7 | 2026-10-04 | Disposable defective copies of `Scheduler.cpp` (product sources as committed in `bd4dc0c`) compiled with the compiler arguments of the sanitizer build `<local>/r6-p010-sanitizers-20261004-152351/`; the shipped SchedulerTest and qualification products built unmodified from the test sources, on the real worker/native/agent path | PASS; each named cell passed on the unmodified products and failed on its control, with the defective support library resolved by the loader. Per-handle bound removed: `admission-behind-retirement` fails at its 9th check, the third-generation rejection. Second generation left uncharged: the same cell fails at its 6th check, `count==2` and `bytes==2*first.bytes`. Charge released at consumption instead of at consumption and retirement (an additional control): the same cell fails at its 4th check, `count==1` and `retirementPending==1` after the first generation is consumed. Lookups keyed by binding only: `two-generation-lifecycle` fails at its 6th check, and the qualification case `behind-retirement` fails `real-qualification-complete`, `successor-dispatched-after-reap-relaunch-ready`, `short-successor-expired-unsent` and `backoff-restarted-after-matched-retired`. Frame validation by storage lookup: the SchedulerTest cell `forged-retirement` fails at its 5th check. Added controls: answered NativeFailure member contained at its deadline fails `grace-outcomes` at its 2nd check; every selected outcome given the grace fails it at its 7th check; every queue expiry counted as never-sent and every generation marked behind a predecessor fail `behind-classification` at its 3rd check; `take` ignoring the admission number fails `take-identity` at its 2nd check. Added after `baad1ee`: the grace given also to a ChannelFailure, a WorkerFailure or a Stopping member fails `grace-exclusion` at its 1st, 6th and 11th check. SchedulerTest reports only the failing check's position, counted from the start of the cell | `<local>/r6-p010-controls-20261004-191025/results.json` for the nine controls of that run (harness SHA256 `274956d3...`); all fourteen controls including the five added ones: `<local>/r6-p026-controls-20261004-235719/results.json` (harness SHA256 `f976772b...`, sanitizer build `<local>/r6-p026-sanitizers-20261004-235719/`); all seventeen including the three grace-exclusion controls: `<local>/r6-d9excl-controls-20261005-003623/results.json` (harness SHA256 `f4d5ad6b...`, sanitizer build `<local>/r6-d9excl-sanitizers-20261005-003623/`); in each run `complete` and `passed` are true and each control directory holds the mutated source, its digests and the cell output |

##### Closure Evidence

- Accepted runtime/tests are committed in `49d5c62`; worker/operator/decision documents in `81cdcdd`. That evidence established the R5 component boundary for steps 1-3 and preserves the explicit verification limits above.
- Reopened 2026-10-03 by D7; closure requires steps 4-6, T5-T7 and re-run T1-T4 on the revised products. Steps 4-6 are done and T1-T7 pass on the revised products, T4 and T5 including the instrumented products of `1100605`.
- Third-person review of the D7/D9 product code (`98bfdfc`, `0b60abf`), 2026-10-04: no product-code defect and no regression of the R5 invariants; its three findings about what the tests pin were resolved on 2026-10-05 by the added SchedulerTest cells and D7 controls (T5, T7) and D10.
- Closed 2026-10-05: the deliverable (steps 1-6), T1-T7 on the revised products including the instrumented T4 and T5, the third-person review and G5 are complete. Landing: after `git fetch` at 2026-10-05 00:15 (local time), `origin/feature/snmp-base-7.0.10-rewrite` equals `HEAD` `75abe64`, and the committed M5 product, test and document paths show no difference from it. Carrying commits: product `98bfdfc`, `0b60abf`; qualification cases `e99331c`; Scheduler cells `0c60fc4`, `bd4dc0c`, `75abe64`; control harness `bd4dc0c`, `1b6333f`, `75abe64`; documents `1100605`; this closure record is carried by `470e731`.

#### M6 - R6 Record Integration

Origin: 5dff352 / M6
Identity History: none
GitHub Issue: none associated with this register
Status: Complete

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
- G1 closed on 2026-10-01 after separate plan acceptance and implementation authorization for the plan now listed under Superseded Plan Artifacts. G5 accepted and authorized the D7-D9 revision on 2026-10-03; M6 was Blocked from 2026-10-03 until then and resumed as In progress. The required T1-T14 executions under the revised plan are recorded in Current Required Verification Results, which owns their review acceptance and limitations. Capability advertisement and repository landing are complete; Closure Evidence records the M6 completion assessment and unchanged exclusions.
- D1 requires an explicit new record/configuration interface; D2 fixes the snmp3 product identity. D3 remains unchanged: equal configured budgets do not imply equal absolute deadlines or permit a new batching rule.
- Record-scope decision, 2026-10-01: include Base 7.0.10 int64in/int64out alongside the existing seven record types. This adds exact signed 64-bit scalar I/O. D5/D6 separately define the conversion policies; implementation authority for the superseded plan was recorded in G1; the current revision was accepted and authorized through G5.
- Long-string scope decision, 2026-10-01: add Base 7.0.10 lsi/lso for bounded long-string input/output, including SIZV/LEN handling and full-length client access. Keep stringin/stringout for the existing short-string interface. D6 defines capacity-error handling; implementation authority for the superseded plan was recorded in G1; the current revision was accepted and authorized through G5.
- D5 resolves ao double-to-OpaqueFloat SET rounding as IEEE 754 roundTiesToEven, dated 2026-10-01. This decision selects one conversion policy; it is not acceptance or execution authorization for the complete M6 plan.
- D6 resolves the remaining conversion precision/capacity-loss policy as error rejection, dated 2026-10-01. Except for D5, do not round, truncate, wrap or saturate an otherwise unusable record conversion. Preserve prior input data and send no invalid SET. Integer-to-floating input loss, waveform FLOAT narrowing and string/array overflow are covered. No conversion-loss policy choice remains pending; complete-plan acceptance and implementation authorization for the superseded plan were recorded in G1; the current revision was accepted and authorized through G5.
- Waveform state decision, 2026-10-01: follow Base's device-support-owned BUSY contract. snmp3 manages BUSY as FALSE for complete single-varbind publication; do not introduce an initial-BUSY rejection policy. Complete-plan acceptance and implementation authorization for the superseded plan were recorded in G1; the current revision was accepted and authorized through G5.
- Interface spelling, conversion matrix, alarm mapping and callback-drain behavior below are accepted implementation requirements, not claims of implemented or verified product behavior. A later plan revision requires new acceptance and implementation authorization.
- T11 scope decision, 2026-10-04: the D7 cell "worker reaped outside the stop bound" is removed from the required T11 cells. Producing it needs a worker that survives SIGKILL, which only an internal stand-in could provide; IncompleteStopped after an expired drain remains covered by the existing blocked-shutdown receipts.
- T14 wording decision, 2026-10-04: D7 defective-code controls alter `Scheduler.cpp` deliberately; the T14 method now requires the remaining spans to be unchanged except for that altered code.
- D7, D8 and D9 (2026-10-03) revise this plan. Production CA observation (`<local>/r6-ca-active-20261003-003058/`, repeated once with the same products) showed a Base RPRO reprocess rejected in five of five records: one SET per record on the wire and `completions=38` against 43 expected, so the latest requested value never reached the device. The cause is that the previous generation's native retirement trails its terminal by about one worker poll; the Result-to-Retired gap measured 9.6-10.8 ms on the M5 qualification harness with the current worker product, and the record-path gap is measured by the unheld case of step 7. D7 admits that reprocess behind the retirement instead. The revision depends on the reopened M5.
- D13 (2026-10-05) disposes of the unexecuted cells recorded in M6 Verification Results. Execute: T1 startup, live-field, periodic and concurrent-detach cells; T2 native exception/type-error and failure-after-success cells; T3 remaining structured/text boundaries; T4 remaining long-string boundaries; T5 native errorStatus/exception, failure after a valid GET, competing-alarm and monitor cells; T6 other record, failure and deadline chains, fanout and delayed or duplicate external responses; T7 scan and PROC reprocess routes; T8 a SIMM switch while an output SET is queued behind retirement on the normal and deadline paths; T9 delayed native retirement/retry and other-address progress; T10 full requirement review; T11 other Stopping, waveform and downstream shutdown states; T12 in-flight non-isolated retention, record startup failure and remaining live-detach cells; and the T14 stale-generation control (first check whether the D7 frame-validation controls recorded under M5 T7 already cover it: if they do, cite that receipt as the executed control; if not, build the control). Defer to Backlog M11, excluded from the Completion Criteria and Test Plan rows T2, T3, T4, T7, T8 and T12 and listed in M6 Closure Evidence as waived checks citing D13 and M11: the Channel Access variants of T2 (signed-minimum), T3 (maximum-capacity) and T4 (ao/longout and signed-minimum); the long active client writes of T7; the delayed output-simulation mode change of T8; and isolated rebuild or reuse of T12 beyond the D7 clause that the `rebuild` case closes, including delayed or abandoned queued completions, lsi/lso buffers and maximum-capacity payloads. Order: shutdown, lifecycle, alarm, ordering and reprocess cells, then input and conversion boundaries, then the rest, each designed, implemented, reviewed and committed on its own.

###### Remaining Verification Order (2026-10-08)

Decision Date: 2026-10-08

Proceed sequentially through all remaining required M6 verification, excluding the four deferred groups in M11. This confirms D13's scope and updates the next work order after the completed lifecycle supplements. It does not add a deferral or change the accepted behavior and verification limitations in D11, D12 and the dated T11 decision.

1. T1/T7: periodic SCAN and explicit PROC while a record is active. Distinguish these from the existing idle PROC and active client-write evidence.
2. T2/T5: native failure after a valid GET, errorStatus/errorIndex, exceptions and type errors, competing alarms and monitor observations. Retain the existing numeric conversion failure-after-success evidence.
3. T6: remaining record chains on success, failure and deadline, fanout to independent addresses, and delayed or duplicate external responses.
4. T8: SIMM changes while an output SET is queued behind retirement, on normal and deadline paths. The delayed output-simulation mode change remains in M11.
5. T2-T4: remaining numeric, structured/text and long-string boundaries, excluding the Channel Access variants in M11.
6. T9: remaining delayed native retirement, same-handle retry, accounting and progress at another address.
7. Reconcile every required T1-T12 clause with the retained receipts and later supplements, and execute any remaining required cell before closure, including unmatched live-field or concurrent-detach cases. Assess the existing D7 controls against T14's record-path requirement; cite sufficient executed evidence or add and run the missing control. Complete T13 qualification on identified ordinary and ASan/UBSan products with the required regressions.

For each item, establish the exact uncovered clauses before adding tests, then implement, execute and review the bounded verification before proceeding. Existing receipts count only for the paths and product identities they actually cover; historical pending rows do not by themselves require repeating a completed supplement. This order records the agreed scope, not a new PASS result or acceptance of an unwritten detailed supplement. M6 stays In progress until every required clause has qualifying evidence; M11 stays Deferred. Git operations retain their separate authorization requirements.

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

The actual Base `ioc/misc/iocInit.c`, iocShutdown, orders AtShutdown, link closure, scan stop, callback stop and isolated record free; doCloseLinks calls device-support del_record under the record lock. `ioc/db/dbAccess.c`, dbPutField, sends device-link writes through dbPutFieldLink but writes DTYP through ordinary dbPut, without the device-extension callback. `ioc/db/callback.c`, callbackCleanup, deletes callback queues after the threads are joined; join alone does not prove every queued entry ran. The active install is selected by `configure/RELEASE.local`; the source inspected is `~/gitsrc/EPICS-env/epics-base-src/modules/database/src/`. These source checks establish a planning premise, not runtime verification of new DSET behavior.

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
7. D7/D8 test preparation (executed 2026-10-03/04; tests committed in `7ffdf5a`, controls on the products before D7 recorded under M5 T5/T6 and M6 T7/T9/T11; the T12 rebuild case was added and executed on 2026-10-05, see T12). The active-output CA section, the delayed endpoint and the INT64_MIN step first existed only as working-tree changes used by `<local>/r6-ca-active-20261003-003058/`; this step corrected and committed them. Correct `tests/rewrite/test_record_ca.py` (observed-value observations only; FLNK count as a phase sentinel, not an admission assertion; agent-side SET value record in `NativeAgent.cpp` instead of a same-address readback; INT64_MIN baseline read just before the rejected put; end-of-run counts computed from executed steps; put-callback second write); add `snmp3RecordTest` cases without the consumer hold, recording per trial the interval between supervision codes 4 (Result) and 5 (Retired) for the record's batch; a deadline-queue case with outer UDP drop-all that first records the supervision timeline over repeated fresh-process trials and records, over N trials, the minimum and maximum observed interval from the queued generation's admission to code 3 (Ready) of the next epoch, then runs budgets below the minimum and above the maximum (fresh process per trial); a two-generation accounting case and stop cases with a retirement-pending plus queued generation. Write and execute the new SchedulerTest admission cell (M5 T5), the active CA, put-callback and unheld record cases on the current products first and retain their failing outcome as the executed negative control; the SchedulerTest failure is recorded under M5 T5. Also run a near-deadline case on the current products (response delay close to the record budget, repeated trials) and record as k of N how often a worker is contained after its result was already selected, before the D9 change; record it under M5 T6 as the current-product baseline (k=0 is reported as not observed, not as absence of the defect).
8. D7/D8 record path, after the M5 revision: `Requests::service` takes the terminal by the context's exact identity (done in `98bfdfc` with the Scheduler change); the callback applies a staged alarm message and `DeviceSupport.cpp` `prepare` sets AMSG `deadline before send` for a Deadline that was never sent (both done 2026-10-05; results under T9); `snmp3RuntimeReport` prints the new queue counters (done 2026-10-05, results under T9). Update the contract, configuration, architecture and test references, and, once the counters are printed, the statement in `docs/snmp-worker-supervision.md` that `snmp3RuntimeReport` does not print them (updated 2026-10-05), with the coverage, latest-write and SIMM definitions, the deadline-path threshold (a queued generation dispatches after containment only when its budget exceeds reap plus relaunch plus Ready; about 1.83 s was measured for a worker that does not exit on channel close, while a worker that exits on close is reaped within tens of milliseconds, so the threshold documented for the outer-UDP drop-all case is the minimum-to-maximum range over every relaunch measured by the deadline-queue case, sampling and budget trials alike, citing the run it comes from; the case's threshold calculation is extended accordingly in this step, done 2026-10-05, results under T9) and late-application bound (up to about twice the budget after the operator's put, plus any Base callback delay before the previous generation's completion, as recorded by the put-to-dispatch interval of the T9 deadline-queue cell), client guidance and queue sizing. Add the put-to-dispatch interval per trial to the deadline-queue case so the documented late-application bound has a receipt (done 2026-10-05, results under T9). The documentation updates of this step (contract, configuration, architecture and test references) are done as of 2026-10-05. Re-run the M6 matrices on ordinary and instrumented products (done 2026-10-05 on the sources of `2544819`, results under T13 and T14).

###### Repeated Detach Verification Plan

Plan Status: accepted
Plan Acceptance: 2026-10-07; plan20261006_181836
Implementation Authorization: 2026-10-07; P001-P004 and V001-V005, recorded in auth20261007_091729
Plan Date: 2026-10-06
Scope: M6 / T12 repeated `del_record`, with the associated T14 controls. These fields apply only to this bounded supplement; the existing G5 acceptance and authorization above remain historical authority for the earlier plan.

Preserve the current product and the Binding And Conversion Contract. Repeated detach need not return success to preserve the detached state. The current missing work is verification: the exploratory `repeat-detach` case emits observations but `test_records.py` has no case-specific assertions. Its generic runner and C++ cleanup checks remain present. No product defect or change to the meaning of idempotent detach is established by an error return alone.

The real path is `test_records.py` -> shipped `snmp3RecordTest` and DB fixtures -> Base `iocShutdown` / `doCloseLinks` -> snmp3 `del_record` -> `initHookAfterCloseLinks` -> actual `dbPutField` for INP/OUT -> snmp3 `del_record` again -> isolated queue and record cleanup. In Base 7.0.10, a successful `del_record` allows link replacement before `add_record`; a subsequent add refusal does not restore the original link and clears `dset`. Therefore the final put error code alone cannot establish that the detach refusal preserved the record.

Ordered work, after acceptance and implementation authorization of this supplement:

1. Extend `tests/rewrite/RecordTest.cpp` using the existing `record-stop.db` ai/ao records. Before shutdown, capture each original full INST_IO string and link type, `dset`, non-null `dpvt` / context pointer, and context count under the applicable locks. At `initHookAfterCloseLinks`, record the first-detach effects before any put: both `dpvt` values and saved contexts' record pointers are null, original links and `dset` values remain, and context storage is retained. Record actual observations, not fixed success literals. Also observe the retained context count at `initHookBeforeFree`, after the module hook and before Base destroys callback queues. Register the test hook after the module registrar so Base invokes the module hook first; verify this ordering against the actual registrar and Base hook list. Read this count through the locked `Requests::snapshot()` and compare it with the nonzero pre-shutdown count, without dereferencing saved record/context pointers. Emit and flush a distinct BeforeFree event. Never dereference saved record/context pointers after isolated cleanup.
2. In that hook, attempt two INP edits and two OUT edits through real `dbPutField`, each with valid snmp3 syntax different from the original text. Capture the status and full link/type, `dset`, `dpvt`, context record pointer and context count immediately after each attempt. Expect `S_dev_badInpType`, unchanged original links and `dset`, null pointers, and retained context count. Do not infer intermediate preservation from the final state. Treat initial fixture validity and four attempted calls as scenario preconditions. Do not throw across the Base hook; emit and flush observations before cleanup. On an unsafe precondition or corrupted pointer, record the failure and skip unsafe dereferences or further puts without repairing the record.
3. Add mandatory, independently named checks in `tests/rewrite/test_records.py`: first detach observed; each of the four attempts refused; each original link/type preserved; each `dset` preserved; detached pointers remain null; contexts retained through AfterCloseLinks and at BeforeFree (named check `repeat-detach-contexts-retained-before-free`); isolated cleanup completed with zero contexts and Runtime Stopped. Require exactly one lifecycle event, four uniquely identified attempt observations, exactly one BeforeFree event, and a post-cleanup event; absence, duplication or malformed fields fail. Preserve the existing process exit, child cleanup, credential-sentinel and sanitizer checks. Normal shutdown and `testdbCleanup` must execute.
4. Extend `tests/rewrite/test_record_controls.py` with compiled defective support copies, keeping the shipped test, Base, fixtures, Runtime/Scheduler/IPC/worker/native spans unchanged except for the declared product mutation. Add `repeat-detach` to the mandatory-event guard. Require a positive reference run with the named check present and passing, and each control to fail its named check after the repeat-detach event, without timeout, forced cleanup or runner abort. For this case, inspect positive presence rather than treating absence from the failed-check list as a pass. Do not redesign unrelated controls or the M10 runner work.
5. Build production targets with root `make -j2`, then build the ordinary record driver and generated registrar with `make -C tests/rewrite -j2`, and only then build fresh sanitizer products. Retain both ordinary build logs, command outcomes and resulting executable hashes. Execute fresh ordinary and ASan/UBSan positive runs, the new controls against current instrumented products, and the existing `live-detach` and `rebuild` regressions on both product sets. Maintain a term-by-term map from every asserted product property to an executed control, or an explicit redundancy/precondition explanation. A control that fails several terms proves only that combination; independent properties need separate controls. Do not claim a mutation outcome before running the actual compiled copy.
6. Add the case, commands, event requirements, coverage limits and exact control list to `tests/rewrite/README.md`. Record source/product/library hashes, receipts and observed outcomes in this document's existing T12/T14 results only after execution. Keep this supplement draft until accepted; do not mark T12 or M6 complete. Obtain third-person review of the implementation and second-person review of changed documentation before any commit.

Required control families are: empty-`dpvt` detach incorrectly accepted; refused repeat changes only a link; refused repeat changes only `dset`; first detach leaves `dpvt` attached; first detach leaves the context record pointer attached; context storage is released before queue destruction, including a compiled control that releases contexts at AfterStopCallback and must fail `repeat-detach-contexts-retained-before-free` at BeforeFree; and isolated cleanup retains detached contexts. Final source edits and check names are fixed during implementation before running controls. Add any further control needed by the term map, or document an actual implication that makes the term redundant. Product-copy faults that merely crash or time out do not qualify the named-check criterion.

Verification uses current local Linux x86_64 / Base 7.0.10, `records.db`, `record-output.db`, `record-stop.db`, the existing runner's loopback agent/proxy, generated registrar and actual support libraries. The repeat case qualifies an isolated, idle shutdown and repeated link-edit-triggered detach. It does not qualify in-flight non-isolated retention, startup failure, alternate DTYP/support attachment, all eleven record types individually, CA client access during shutdown, or the remaining D13 cells. ASan/UBSan covers newly built module/test products; Base/system/vendor dependencies remain uninstrumented and leak detection remains disabled under the existing procedure.

No ADR promotion is required because product behavior and design policy are unchanged. A new product defect or a change to the accepted lifecycle contract returns to the owner as a separate decision. Preserve all pre-existing working-tree changes and retained receipts; recovery is correction of this supplement or isolated test work, without destructive Git operations.

###### Accepted Non-Isolated Shutdown Verification Supplement

Plan Status: accepted
Plan Acceptance: 2026-10-07; plan20261007_204300, SHA256 `629fb947cdee85c8b50c1f94e3faf0205c95021af0911de34ad766762fb08b35`, after independent third-person and separate second-person plan review with no findings
Implementation Authorization: 2026-10-07; P001-P004 and V001-V005, recorded in `<local>/review_sessions/20261003_013534_rpro-admission/plan/auth20261007_210218_codex_gpt6_root_nonisolated-shutdown.md`

This bounded T12/T14 supplement preserves the production lifecycle contract, historical G5 authorization and completed repeated-detach work. Its immutable detailed plan is `<local>/review_sessions/20261003_013534_rpro-admission/plan/plan20261007_204300_codex_gpt6_root_nonisolated-shutdown.md`.

1. P001: add `tests/rewrite/db/record-shutdown.db` (passive low-priority ai Integer GET and ao OpaqueFloat SET, 60000 ms deadlines, distinguishable values and shared calc FLNK counter), `ShutdownTest.cpp` and `snmp3ShutdownTestRegistrar.dbd`. Build `snmp3ShutdownTest` from unchanged production Main, a generated registrar and an external callback observer; extend the test Makefile and sanitizer builder. Ordinary iocInit and shell exit must reach the actual non-isolated Base lifecycle; no isolated test initialization or cleanup substitutes are permitted.
2. P002: add `test_record_shutdown.py`. The production `inflight` case uses real CA clients and a drop-all outer UDP proxy; require both records active with zero completions before exit, then normal Stopped shutdown, two inactive retained contexts, closed entry, exactly two completions and the identified worker reaped. The companion `retained` case holds the real low-priority callback consumer until AfterCloseLinks. Require actual GET/SET responses and native retirement with two queued callbacks and unconsumed reservations; real drain expiry, entry closure, dpvt/context-record detachment, inert late callbacks after join, unchanged values/publication/FLNK and reservations, IncompleteStopped and rejected restart without new activation/thread/worker. Observe each phase exactly once with typed fields; no BeforeFree. Check inventory before saved-context dereferences. A cleanup watchdog or forced exit fails the run.
3. P003: add the bounded `--shutdown-controls` group in `test_record_controls.py`. Compile six defective support copies: release after callback join, release after shutdown, omitted dpvt clear, omitted context record clear, bypassed closed-entry return, and omitted drainFailed recording. Each must complete real shutdown normally and fail its own named check, which occurs exactly once and passes in its reference. Verify loaded library hashes and every child cleanup receipt. Crashes, timeouts and sanitizer termination are not detection. Map each asserted property to an executed control, demonstrated implication or explicit precondition/limitation.
4. P004: document the two cases, commands, controls, retained ownership and limits in `tests/rewrite/README.md`; record only observed results in T12/T14 here. Publish a work-local handoff with source/product identities and review evidence.

V001 requires root `make -j2`, `make -C tests/rewrite -j2`, then a fresh full ASan/UBSan build with logs, outcomes and source/product hashes. V002 requires both cases in three ordinary fresh-process trials and one instrumented trial, every trial passing. V003 repeats isolated `queued-shutdown` and the production CA runner on both product sets. V004 executes all six controls against passing references and checks missing/duplicate/malformed event rejection using retained real observations (parser validation only). V005 requires first-person author review, independent third-person implementation/execution review, a distinct second-person documentation pass and exact scope/diff checks.

All fixture processes and CA clients must have reaping receipts. Base/CA/system/vendor dependencies remain uninstrumented and leak detection stays disabled. Non-isolated retained allocations are expected, not a leak-free result; callback thread join is not queue destruction. Product source changes, startup failures, all eleven record kinds individually, long-string/maximum-capacity qualification, isolated reuse, other D13 cells, memory writes and Git/remote changes are excluded. A product defect or lifecycle-contract change returns to the owner. T12 and M6 remain open.

###### Accepted Startup-Failure Verification Supplement

Plan Status: accepted
Plan Acceptance: 2026-10-07; owner accepted the unchanged P001-P004 plan after third-person review 1 and second-person review 2, both PASS
Implementation Authorization: 2026-10-07; owner explicitly authorized P001-P004 implementation and its stated verification; no product behavior, Git/remote or handoff/memory changes

Scope audit date: 2026-10-07. Source basis: `68a9611112bccbffdb52b038e0a421cc8a3676ed`. This is a source and retained-receipt audit, not a new integration execution or acceptance of T11/T12. The existing accepted supplements and their results remain historical evidence.

| Area | Existing evidence | Remaining boundary |
| --- | --- | --- |
| T11 shutdown | `stop-inflight`, `stop-enqueue-failed`, `stop-downstream`, entered/queued shutdown and D7 `stop-queued`; T11 records every D13 cell as executed | Reconcile the full T11 method with its receipts at acceptance; its current result remains Pending, subset PASS |
| T12 detach and non-isolated ownership | `live-detach`, `repeat-detach` and the production/companion non-isolated cases | Record-bearing startup failure remains unexecuted; these cases do not close all T12 requirements |
| T1 initialization rejection | `RecordTest.cpp::inputEdges` checks null dpvt for unknown binding, invalid grammar, unsupported OOPT and invalid FTVL | Rejected records and initialized records need explicit observations through startup failure and shutdown |
| Earlier lifecycle failure | `LifecycleTest.cpp::failure` uses real RLIMIT_NPROC and the shipped Main; configuration IOC tests use a Soft Channel PINI record | Neither is a failed startup with attached snmp3 record contexts |
| Other D13 lifecycle work | T1 still lists startup, live-field, periodic and concurrent-detach cells | Keep these open; the proposed supplement does not cover all live DTYP/support attachment or periodic processing |

Retained receipts reopened for this audit include `<local>/r6-t11b-stop-inflight-1/results.json`, `<local>/r6-t11c-enqueue-10/results.json`, `<local>/r6-t11e-downstream-4/results.json`, `<local>/r6-t11e-asan-stop-downstream/results.json`, `<local>/r6-repeat-20261007/ordinary-1/results.json`, `<local>/nonisolated-v2-1/results.json` and `<local>/r6-matrix-20261005-123214/record-edges/results.json`. Their recorded verdicts are passing for their identified products; reading them does not rerun or extend their coverage.

Confirmed coverage gap: `DeviceSupport.cpp::initialize` can reject one record without making Runtime startup fail. Separately, `Runtime.cpp::start` constructs Supervisor after record binding and before `Requests::start`; a preflight exception records Failed and closes admission. `Main.cpp` checks Failed even after a Continue-policy startup script. These are distinct paths and need distinct observations. The Base 7.0.10 `iocInit.c` source places record initialization before AfterFinishDevSup and calls del_record during shutdown before callback join; non-isolated shutdown retains the queues. These source observations define the proposed checks, not their outcomes.

Proposed bounded scope: ai/ao initialization rejection and Runtime preflight failure after valid ai/ao contexts exist, using the real Base 7.0.10 lifecycle. The external failure condition is an absolute worker path that does not exist inside the run's private directory. `Config::setWorkerPath` accepts an absolute path without opening it, while Supervisor resolves it during startup. Reaching that failure with initialized contexts must be observed before the case qualifies.

1. P001: add a shipped `tests/rewrite/db/record-startup.db` with valid ai/ao records and separately identified rejected input/output records, retaining distinguishable values. Add `StartupTest.cpp` and its generated test registrar, built from unchanged production Main, plus `test_record_startup.py`; update the test Makefile and sanitizer builder. Observers use actual Base hooks and inspect context inventory before dereferencing saved pointers. They must not replace DSET, Runtime, Scheduler or cleanup operations.
2. P002: implement separate normal-start/rejected-record and Runtime-preflight-failure processes. The normal case must show rejected records without usable contexts while valid records complete real GET/SET through the existing NativeAgent. The failure case must show valid contexts before the real preflight failure, Failed with admission closed, no accepted request or worker traffic, preserved record values, and process exit 1 through the unchanged Main. Exercise both Break and Continue script policies; do not equate a rejected record with whole-IOC startup failure or use an explicit shell exit to bypass Main's failure check.
3. P003: observe shutdown after the failure: entry closed before detach, record and context pointers cleared at AfterCloseLinks, inactive context storage retained through AfterStopCallback and AfterShutdown, no BeforeFree in the non-isolated case, and Failed preserved through repeated stop and a refused restart. Use the actual production IOC for public startup/exit observations and the companion only for internal lifecycle observations. No isolated cleanup helper, test-written state repair or direct call to a substituted startup path is permitted.
4. P004: add named assertions and bounded defective-code controls for failure-state preservation, admission refusal and detach/storage observations. Each control must reach the intended phase, load the identified defective support, exit as expected without a crash/timeout/forced cleanup, and fail a named check that passes in its reference. Record properties without an independent control explicitly. Document the runner and boundaries in the test README, and place observed results and the next procedure in this milestone.

Proposed verification: build the root and test products, then fresh ASan/UBSan products; run each new case on ordinary and instrumented products, preserving source/product/library identities, ordered events, expected exit status and child cleanup receipts. Re-run `edges`, `repeat-detach` and the non-isolated shutdown cases on both product sets because the fixtures, registration and shutdown observations overlap. Keep parser validation separate from IOC execution. Review the plan before acceptance, and require independent implementation review plus a separate second-person documentation pass before commit preparation. No new case, control or regression has run under this draft.

Open feasibility item: real thread-creation failure after `Requests::start` is a different startup phase. Applying the old RLIMIT_NPROC method before the startup hook may instead fail Supervisor's real `ldd` child before Requests starts. Do not claim that method covers thread creation with attached records. Establish an external-boundary method in a separately reviewed extension before claiming that phase; no internal-function substitute and no implicit waiver of T12 startup coverage.

Exclusions: product behavior changes, all eleven record kinds individually, pointer-backed/max-capacity payload qualification, isolated rebuild/reuse, the remaining T1 field/periodic/race cells, alarm/conversion work, M12 stop-duration changes, installed Base changes, equipment operation, memory/handoff files and Git/remote mutations. A discovered product defect or a proposed contract change requires a separate scope decision. This supplement alone cannot close T1, T12 or M6.

###### Startup-Failure Implementation Results

2026-10-07, local Linux x86_64 / Base 7.0.10, product source basis `68a9611112bccbffdb52b038e0a421cc8a3676ed`: P001-P004 implemented within the accepted test/build/documentation scope. Production sources under `snmp3App/` are unchanged. Independent implementation review 1 and the separate second-person documentation pass both passed with zero must-fix and zero minor findings. No commit, push, handoff or memory change is included.

| Plan item | Delivered files | Executed evidence |
| --- | --- | --- |
| P001 | `tests/rewrite/db/record-startup.db`, `StartupTest.cpp`, `snmp3StartupTestRegistrar.dbd`, `Makefile`, `build_r5_sanitizers.py` | Root/test Make exit 0 in `<local>/startup-build/`; 17 fresh ASan/UBSan products and source/product hashes in `<local>/startup-san/sanitizer-build.json` |
| P002 | `tests/rewrite/test_record_startup.py`, shipped ai/ao fixture and companion | Five cases each pass in `<local>/startup-ordinary-3/` and `<local>/startup-asan-2/`: normal 31 checks, production Break/Continue 18 each, companion failure Break/Continue 39 each |
| P003 | Companion lifecycle observer and runner assertions | Ordered initialization/start/attempt/stop/shutdown/restart observations in both companion failure cases; record/context pointers clear at AfterCloseLinks, context count remains two through callback join and AfterShutdown, no BeforeFree; repeated stop preserves Failed and restart is refused |
| P004 | `tests/rewrite/test_record_controls.py`, runner, `tests/rewrite/README.md`, this detail | All seven compiled controls detected by their named checks, with complete lifecycle, identified defective library and normal child cleanup: `<local>/startup-controls-2/results.json`; check-to-control limits are in the README |

The normal process distinguishes two individually rejected records from whole-IOC failure: two valid contexts exist before Runtime starts and valid GET/SET complete through the real worker/native/NativeAgent path. Input publication/native-success flags are one; output SET completion does not publish a readback, so its corresponding flags remain zero. The ordinary and instrumented production Main processes exit 1 for both Break and Continue after actual missing-worker preflight failure. Companion observations additionally show READ/WRITE INVALID on actual processing attempts, clear PACT, no accepted work or wire request, preserved fixture values, entry closed before detach and Failed retained through shutdown. Each case retains exact startup scripts, raw stdout/stderr, ordered events, source/product/loaded-library hashes and child exit/reaping receipts. No internal startup, processing or cleanup span is substituted.

The named regressions also pass on both products in `<local>/startup-reg-ordinary/` and `<local>/startup-reg-asan/`: `edges` 17 checks, `repeat` (repeat-detach) 55, shutdown `inflight` 26 and `retained` 46. Their execution receipts identify the actual argv and exit 0. Compiled sources/products and these regression drivers remained unchanged after those runs; only startup assertion/documentation corrections followed. All sanitizer build source/product hashes were rechecked against the current files without mismatch.

Assertion validation is separate from IOC execution: `<local>/startup-assertion-audit-2/audit.py` applies 392 probes to retained actual observations, including missing/duplicate events, missing or wrong-type fields and wrong-but-well-typed failure-state/counter/context-count values. All probes pass. The earlier audit in `<local>/startup-assertion-audit/` exposed missing intermediate context-count assertions; the current runner checks the count at every phase, and the complete ordinary/instrumented startup matrix and seven controls were rerun afterward. Earlier execution directories remain evidence of their identified versions, not results for the final assertions.

Module/native/test products are instrumented; installed Base and system/vendor libraries are uninstrumented, with leak detection disabled. Non-isolated process-exit allocations are expected; no leak-free claim is made. This executes only ai/ao initialization rejection and Runtime preflight failure before Requests starts. Thread-creation failure after Requests starts, other record kinds and maximum-capacity pointer-backed payloads, remaining field/periodic/race cells and full T1/T12/M6 acceptance remain open. M6 remains In progress; T1, T12 and T14 remain Pending, subset PASS. P001-P004 are complete within this supplement; T1, T12, T14 and M6 remain open. Next: commit preparation only when requested by the owner.

Independent implementation review 1, 2026-10-07: PASS, zero must-fix and zero minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261007_234608_subagent_gpt6_startup_plan_r1_initial.md`, SHA256 `f5ee78b6c0a9fee93049707b7d7d73a9722d1befff4584c54102f543699fe92c`. The frozen nine-file implementation target and seventeen author evidence hashes remained unchanged during review. Reviewer executions independently passed both five-case startup matrices, all seven compiled controls and their reference, and the eight ordinary/instrumented regression cases. Receipt roots use `<local>/startup-impl-review-ordinary-r1/`, `startup-impl-review-asan-r1/`, `startup-impl-review-controls-r1/` and `startup-impl-review-reg-*` under `work/`. The additional production probe processed the rejected records before valid GET/SET; actual IOC exit 0, two native requests, preserved rejected values and normal shutdown are recorded in `<local>/startup-impl-review-public-r1/`. Its original script result remains failed because of two incorrect text-format expectations; `audit.py`/`audit.json` separately read the real stdout correctly and do not claim a new IOC execution. The separate second-person documentation result follows below.

Separate second-person implementation-documentation review, 2026-10-07: PASS, zero must-fix and zero minor findings. This is review 2 overall for the implemented target and the first separate reader pass, from the test operator and next-step reader seat. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261007_234926_subagent_gpt6_startup_plan_r1_on_rev20261007_234608.md`, SHA256 `9a8d3b829effc244ec2aec20cd09105b1789c20ce16cbf7eba55eca4bc6a51fb`. All changed README and canonical passages were read against source, CLI help and retained receipts. The nine-file reader target and seventeen evidence hashes remained unchanged during the pass; no new IOC, build, control or sanitizer execution was performed. The result supports the completion and next-action status above without closing the remaining milestone work.

###### Startup-Failure Plan Review

2026-10-07, independent third-person review 1: PASS, zero must-fix and zero minor findings. Reviewed the complete draft above and the next-session entry point at document SHA256 `ee17625a5e484753c82ade9bbc824794fd004c8a5bf66fe222e38e2e5ba4a281`; those passages remained unchanged during review. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261007_231431_subagent_gpt6_startup_plan_r1_initial.md`, SHA256 `53d8bccf502fd545ba8e9443cd29261251c393cf1b8116ab836834e221b06e98`.

Actual feasibility executions used the existing production IOC and shipped `record-shutdown.db`, with a nonexistent worker path: Break and Continue both exited 1 without forced cleanup and reported Failed, closed admission, two retained inactive contexts and zero completions after real shutdown. Continue also preserved the fixture values and Failed through repeated stop. The existing `edges` case passed 17/17 checks. Evidence: `<local>/startup-plan-review-r1/receipts.json`, script stdout/stderr and `edges/results.json`. These runs establish bounded feasibility, not qualification of the proposed new fixture, intermediate pointer observations, admission-refusal checks, restart checks, controls or sanitizer coverage. No new supplement implementation ran. The draft remained unchanged through that review; owner acceptance and implementation authorization remain pending. The subsequent separate second-person outcome follows.

2026-10-07, review 2 overall and the first separate second-person pass: PASS, zero must-fix and zero minor findings. The incoming implementer seat covered the complete draft, next-session entry point and third-person outcome at document SHA256 `b89d0e79f100c0bac135838de03f8a20a0078d4f67a06b1d8af269a988815d9d`. Source and retained execution evidence were reopened; no new IOC, control, build or sanitizer execution ran. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261007_232050_subagent_gpt6_startup_plan_r1_on_rev20261007_231431.md`, SHA256 `e56b889181a6d8887e5bf5f74e9a956749dfbee81c0744491f6c7e3091fbe75c`. P001-P004 and their verification requirements are unchanged. Both plan review passes are complete; Plan Status remains draft, Plan Acceptance none and Implementation Authorization none until owner direction.

###### Accepted Thread-Creation Failure Verification Supplement

Plan Status: accepted
Plan Acceptance: 2026-10-07 (Pacific); owner directed the next step after both independent plan passes completed without findings
Implementation Authorization: 2026-10-07 (Pacific); owner direction to proceed to the next step authorizes P001-P004 and their stated verification for this reviewed supplement

Planning date: 2026-10-07 (Pacific). Source basis: `c03771c60a23f3065e593b8df77235401bbc370d`. The owner selected thread-creation failure as the next lifecycle item and authorized feasibility investigation and planning. This supplement extends the completed preflight supplement without changing its accepted scope or historical results. Both plan review passes are complete; the owner has accepted this extension and authorized its implementation.

Premise: `Runtime::start` constructs Supervisor, calls `Requests::start`, increments activation and only then calls the actual Base `epicsThreadCreateOpt`. A thread-creation failure therefore differs from preflight failure: activation is one, created/exited/joined are zero, and the Requests entry gate has already opened. Runtime admission stays closed; actual record processing is refused by `Runtime::admit`. Stop subsequently closes the Requests entry gate. Do not require that gate to be closed immediately at the create-failed observation or treat an open entry gate as accepted record work. Existing `LifecycleTest.cpp::failure` has no attached snmp3 records and applies RLIMIT_NPROC before Supervisor preflight; that method alone does not qualify this cell.

Feasibility evidence, 2026-10-07 (Pacific), local non-root Linux x86_64 / Base 7.0.10: `<local>/thread-failure-feasibility/evidence.json` identifies five executions using unchanged production/companion binaries and the shipped `tests/rewrite/db/record-startup.db`. `production-break` and `production-continue` pass 13 feasibility checks each; `companion-break-host` and `companion-continue` pass 18 each. Every failed IOC exits 1 normally, reports one create-failed at activation one without preflight-failed, retains two idle contexts and sends no native request. Companion observations show contexts before startup, an open Requests entry gate at the failed-start phase, real processing/stop/shutdown, detach and refused restart. The `normal` traced comparison passes 10 checks, completes real GET/SET and exits 0 with one created/exited/joined Runtime thread. Child/fixture cleanup and log scans pass. These are feasibility checks, not the future shipped assertion set or sanitizer/control qualification.

The external tracer launches only its own IOC child and observes its main-thread syscall entry/exit. A PTY makes the existing lifecycle line observable before the next call. After the real `starting activation=1` line, it temporarily lowers only that child's RLIMIT_NPROC soft limit to zero at a CLONE_THREAD creation call, lets the unchanged syscall run, observes the kernel result, and restores the original limit before resuming user code. All four failed feasibility executions observed actual clone3 returning EAGAIN, with the original soft/hard limit restored. No instruction, register, return value, internal function or signal disposition is overwritten. A prior sandbox-denied TRACEME attempt exited 125 before IOC exec and is retained separately, not counted as an IOC test. Tracing permission is a local execution prerequisite, not permission to alter host-wide policy. The mechanisms are defined by the Linux man-pages [ptrace](https://man7.org/linux/man-pages/man2/ptrace.2.html) and [getrlimit/prlimit](https://man7.org/linux/man-pages/man2/getrlimit.2.html); local source and executed receipts establish this IOC path.

Proposed scope: ai/ao record-bearing Runtime thread-creation failure after successful Supervisor preflight and Requests start, through unchanged Main and real non-isolated Base shutdown. Preserve rejection behavior, repeated-stop and failed-restart semantics, context retention, existing fixtures and the five existing startup cases. No change to production or installed Base code.

1. P001: add `tests/rewrite/helpers/runtime_thread_limit.py` as a Linux x86_64 external tracing helper. Use read-only syscall information and clone3 argument inspection; never use ptrace memory/register writes or synthetic syscall results. Match the existing Runtime starting line, constrain only the next qualifying main-thread CLONE_THREAD syscall, and require a real EAGAIN. Record ABI, PID, timestamps, observed flags/result, limit before/during/after, target identities and clean reaping. Handle clone3-to-clone fallback explicitly without claiming ENOSYS is the intended failure. Preserve the hard limit and parent limits. Missing markers, unsupported ABI, ineffective limits, denied tracing, restoration failure or unexpected trace stops produce an explicit non-passing result. Bound waits and capture error/timeout/cleanup evidence; close owned descriptors and reap only owned children. A fault-disabled tracing mode must leave limits untouched.
2. P002: extend `tests/rewrite/test_record_startup.py` with `thread-normal`, `thread-break`, `thread-continue`, `production-thread-break` and `production-thread-continue`. Select normal/preflight/thread behavior explicitly; do not infer failure solely from a case name being different from normal. Keep the old five cases and default-all behavior, so the full matrix has ten cases. Reuse unchanged `StartupTest.cpp`, its registrar, production Main and `record-startup.db`; no new C++ observer or database fixture is required by the observed path. Thread cases use a valid worker and real native probe/configuration, then the external helper; preflight cases retain the missing-worker method. Keep private ports, real NativeAgent/UDP observer, loaded-library/source/product hashes, raw logs, normal child cleanup and sanitizer/secret checks. The traced normal case must complete GET/SET with no limit changes; both failed policies must return Main exit 1.
3. P003: make failure assertions phase-aware in that runner. Require two attached contexts before startup; thread failure has one starting/create-failed pair, no preflight-failed/ready/created event, activation one and zero created/exited/joined. At started, observe Failed and Runtime admission closed with the Requests entry gate open. Actual processing must leave values/identities unchanged, set READ/WRITE INVALID, clear PACT and accept no work. Verify both stop calls close entry and preserve Failed; real shutdown clears both pointer directions while retaining two contexts through callback join/AfterShutdown, without BeforeFree. Refused restart must preserve state, counts, handles, pointer/value/publication state and reservations. Keep preflight expectations at activation zero. Missing/duplicate/malformed evidence must not qualify. Validate parser predicates separately on retained actual events; do not count those probes as integration executions.
4. P004: extend `tests/rewrite/test_record_controls.py` with `--startup-phase preflight|thread` for the startup-control group, defaulting to preflight and rejecting use outside that group. Execute seven bounded compiled controls for thread failure: lost Failed state, ownerless acceptance while Runtime admission is closed, omitted refusal alarm, premature release after callback join, premature release after shutdown, omitted record dpvt clear and omitted context record clear. The preflight ownerless mutation checks missing producers and cannot discriminate the later thread-failure phase; use an explicit closed-Runtime-admission mutation for the thread variant, while retaining the old preflight mutation. Each reference must pass; every defective run must load the identified support, reach actual kernel refusal and the intended phase, complete the expected lifecycle/exit without crash/forced cleanup, and fail its named assertion. Re-run all seven original preflight controls because shared runner/control code changes. Document unsupported environments, commands, check-to-control limits and evidence interpretation in `tests/rewrite/README.md`; record execution/review outcomes here.

Proposed verification, from the repository root, each output directory new:

```bash
make -j2
make -C tests/rewrite -j2
python3 tests/rewrite/build_r5_sanitizers.py --output work/thread-start-sanitizers
python3 tests/rewrite/test_record_startup.py --output work/thread-start-ordinary
products=work/thread-start-sanitizers/products
runner=tests/rewrite/test_record_startup.py
python3 "$runner" --products "$products" --sanitizers --output work/thread-start-asan
receipt=work/thread-start-sanitizers/sanitizer-build.json
controls=tests/rewrite/test_record_controls.py
python3 "$controls" --build-receipt "$receipt" --startup-controls --output work/thread-start-preflight-controls
python3 "$controls" --build-receipt "$receipt" --startup-controls --startup-phase thread --output work/thread-controls
```

Run all ten cases on ordinary and fresh ASan/UBSan products. Re-run `edges`, `repeat-detach`, and both non-isolated shutdown cases on each product set using their existing shipped commands. Retain actual OS-limit evidence and the loaded support identity in thread-control qualification. Unsupported tracing/OS restrictions must remain NOT RUN/non-passing, never a silent fallback to preflight failure or an internal stub. The traced normal comparison establishes functional progress, not timing equivalence. ASan/UBSan dependencies remain uninstrumented and leak detection is disabled; ptrace compatibility is unverified until these executions run. Require independent plan review before acceptance, then independent implementation review and a separate second-person changed-document pass before commit preparation.

Exclusions: product/Base behavior changes, host-wide limits or policy changes, privileges added to the IOC, internal mocks or return-value injection, resource exhaustion outside the owned IOC, all record types individually, maximum-capacity buffers, isolated rebuild/reuse, concurrent field/detach cases, other D13 cells, equipment, handoff/memory files and Git/remote mutations. The ordinary feasibility method is established; the shipped extension, sanitizer compatibility, controls and full T1/T12/T14/M6 acceptance remain pending. Next procedure: execute P001-P004 and the stated verification, then obtain independent implementation review and a separate second-person changed-document pass before commit preparation.

###### Thread-Creation Failure Plan Review

2026-10-07 (Pacific), independent third-person review 1: PASS, zero must-fix and zero minor findings. Reviewed the complete draft and Next session entry point at whole-document SHA256 `021c709ae6bf3a3c11fa89be379f9e85d92cb17f40696d76d9f3ece0add5528c`; the target stayed unchanged during review. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_010537_subagent_gpt6_thread_plan_r1_initial.md`, SHA256 `bbcc1e11f20638e139f632248f371c93dc666555d54b802d064f15421d341c8c`. Five new ordinary-product feasibility executions passed 72/72 checks (companion Break/Continue 18 each, production Break/Continue 13 each, traced normal 10). The four failure runs observed real clone3 EAGAIN and IOC exit 1; normal GET/SET completed with IOC exit 0. An independently written raw-receipt auditor passed 167/167 checks across retained author runs and new reviewer runs, including subsequent unrestricted Base thread creation after limit restoration. Evidence: `<local>/thread-plan-review-r1/audit.json` and the five run directories. Receipt checks are not additional IOC executions. New shipped implementation, sanitizer compatibility, fallback/error-path qualification, compiled controls and the regression matrix remain unexecuted. No plan clauses changed; Plan Status remains draft, Plan Acceptance none and Implementation Authorization none. A separate second-person plan pass has not run.

2026-10-07 (Pacific), review 2 overall and the first separate second-person plan pass: PASS, zero must-fix and zero minor findings. The incoming implementer seat covered the complete draft, Next session entry point and third-person result at document SHA256 `674222d53f707c156e785bf6ebcb4f6bd99954dd2da53fe3a75d05a2d4446cbc`. Source and retained receipts support the scope, commands, outcomes and evidence limits. Current CLI help and proposed command syntax were checked; no new IOC, build, sanitizer or compiled-control execution ran. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_011223_subagent_gpt6_thread_plan_r1_on_rev20261008_010537.md`, SHA256 `f0425c38f04a690baeea301ac3b5ba1375c0b022152ba33c143768459e25cc27`. The plan clauses remained unchanged. Both plan review passes are complete; Plan Status remains draft, Plan Acceptance none and Implementation Authorization none pending owner direction.

###### Thread-Creation Failure Implementation Results

2026-10-07 (Pacific), P001-P004 implemented with production and installed Base unchanged. `helpers/runtime_thread_limit.py` performs read-only main-thread tracing and real owned-IOC resource restriction/restoration; the startup runner selects ten explicit cases with phase-specific state/entry/worker checks, and the control runner selects preflight or thread failure. The thread ownerless-acceptance control sets active/PACT with an Inert callback when Runtime admission is closed, so the real drain expires without attempting a nonexistent Scheduler identity. README specifies the execution prerequisites, commands, evidence and discrimination limits.

Root and test builds returned 0; `<local>/thread-start-sanitizers/sanitizer-build.json` identifies 17 successfully built fresh ASan/UBSan products. Final ordinary and instrumented startup runs both pass all ten cases, with per-case check counts 31/18/18/40/40/34/44/44/22/22 in runner order. Evidence: `<local>/thread-start-ordinary-verified/results.json` and `<local>/thread-start-asan-verified/results.json`, with exact scripts, raw output, trace, child receipts and loaded identities under each case. All eight failed cases on each product set exit 1 naturally; both normal cases exit 0. Thread failure records actual EAGAIN, restored child limits and unchanged parent limits.

`<local>/thread-start-preflight-controls-verified/results.json` and `<local>/thread-controls-final/results.json` both pass all seven compiled controls. Each full reference passes, each defective support is identified as loaded, each run completes the required lifecycle with clean child handling, and its named check fails. `<local>/thread-start-regressions/` records edges 17/17, repeat-detach 55/55, and non-isolated inflight 26/26 plus retained 46/46 on both ordinary and instrumented products; all six driver invocations return 0.

`<local>/thread-helper-errors/results.json` records four actual process-boundary probes: missing marker, failed exec, unexpected stop and the real 60-second deadline. Each returns non-passing trace evidence, preserves parent limits and reaps the owned child; the stop/deadline cases correctly identify forced cleanup and are not qualified IOC runs. `<local>/thread-start-execution/parser-checks.json` records 597 passing parser-only probes derived from retained real observations, covering missing/duplicate/malformed phases, phase-specific activation/entry, restart changes and trace evidence. These are not 597 IOC executions. The kernel here used clone3; clone fallback, other ABIs, deliberately ineffective limits and restoration errors were not executed. No leak-free or timing-equivalence claim is made.

Earlier attempts remain excluded from final qualification: `<local>/thread-start-ordinary/` exposed a preflight-only no-worker-report assertion, replaced with zero worker counters for the later phase; `<local>/thread-controls/` exposed an abort from the initial ownerless mutation's nonexistent identity. The final control represents an ownerless Inert callback and completes real drain/shutdown, without runner repair of product state. Earlier passing intermediate matrices are retained but final qualification uses the paths above. Final identity index and independent review target: `<local>/thread-start-execution/review-target.json`. Independent implementation review and the separate second-person changed-document pass remain pending; M6/T1/T12/T14 acceptance, commit and push remain outside this result.

Independent implementation review 1, 2026-10-07 (Pacific): PASS, zero must-fix and zero minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_015538_subagent_gpt6_thread_plan_r1_implementation.md`, SHA256 `44124aa03379347fc8089dac663a1c244a3300c4da13b83bc95060411edf1b12`. The reviewer independently ran all ten ordinary and all ten ASan/UBSan startup cases (313 checks per matrix), with evidence under `<local>/thread-implementation-review-r1-ordinary/` and `<local>/thread-implementation-review-r1-asan/`. Independent receipt/hash analysis passed 681/681 in `<local>/thread-implementation-review-r1/audit.json`. The reviewer-authored `boundaries.py`, executed by the Facilitator through normal runtime approval, passed 19/19 actual signal/timer/limit/descriptor/file-boundary checks; the reviewer inspected its raw receipts. These boundary/audit counts are not additional IOC executions. All five target and 90 evidence hashes remained fixed through review. No corrective finding or target edit was required. The separate second-person changed-document pass remains pending.

Separate second-person implementation review 2, 2026-10-07 (Pacific): PASS, zero must-fix and zero minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_015924_subagent_gpt6_thread_plan_r1_on_rev20261008_015538.md`, SHA256 `70bf1230574f0d89b5f695bc136cece873679329a16525fcbf6ecc2b7007f7d2`. The complete changed README and canonical passages were checked from the incoming test operator seat against actual source and receipts; CLI help, command syntax, counts and unchanged identities were checked, without a new IOC/build/control/helper qualification execution. Both required implementation reviews are complete, with no corrective changes. P001-P004 and the stated verification are complete within this supplement. Next action: prepare the commit only when requested by the owner. M6/T1/T12/T14 acceptance and formal session closure remain pending; no Git/remote, handoff or memory mutation is part of this completion. Final current identities: `<local>/thread-start-execution/final-state.json`.

###### Accepted Live DTYP Verification Supplement

Plan Status: accepted
Plan Acceptance: 2026-10-08 (Pacific); owner approved after third-person correction recheck and separate second-person review passed without findings
Implementation Authorization: 2026-10-08 (Pacific); owner explicitly authorized P001-P004 implementation and V001-V005 verification

Planning date: 2026-10-08 (Pacific). Source basis: `bbc8abec116e0586a8ab6202d72a02144f4f41fc`. The owner selected the live-field verification planning option. This bounded M6 T1/T12 supplement and its T14 controls preserve the accepted Binding And Conversion Contract. Earlier G5 and supplement authorizations do not constitute acceptance of this new detailed plan.

Scope: ai Integer GET and ao OpaqueFloat SET through actual Base field writes, snmp3 admission/completion, and isolated IOC shutdown. Qualify DTYP mismatch handling, restoration, retained ownership and actual detach with the field still changed. These two records cover the shared identity check and distinct input/output completion paths; they do not qualify all eleven record kinds individually. Existing live INP/OUT replacement checks remain regressions.

Out of scope: product/Base behavior changes, rebinding policy changes, attaching another device support, other live fields, periodic SCAN, all-record-type coverage, Channel Access writes during shutdown, unsynchronized writes to freed records, a general concurrency or TSan claim, M11/M12 work, memory/handoff files and Git/remote mutations. A discovered product defect or required policy change returns to the owner before any product edit. No ADR change is proposed.

**Source premises and remaining hypotheses.** No new IOC execution was performed for this draft; these are source findings and proposed observations, not runtime qualification results.

- Confirmed coverage gap: `tests/rewrite/RecordTest.cpp::baseline` changes idle `Records_Ai.DTYP`, processes it and checks LINK alarm, then restores the field and processes it again. `liveDetachCells` changes INP/OUT, not DTYP. Neither establishes active/shutdown DTYP behavior.
- Confirmed source path: the pinned Base `dbCommon.dbd.pod`, DTYP field declaration and device-type description, defines DTYP as DBF_DEVICE without process-passive behavior. Base `ioc/db/dbAccess.c::dbPutField` routes link fields to `dbPutFieldLink`; DTYP instead goes through locked `dbPut`. Therefore a successful DTYP put alone neither proves rejection nor changes the installed DSET/context. At execution, identify the selected Base installation from `configure/RELEASE.local` and verify this source premise against that installation; the planning source tree is `EPICS-env/epics-base-src/modules/database/src`.
- Confirmed product contract and source: `DeviceSupport.cpp::validate` compares current DTYP with immutable `RecordContext::dtype`. `process` checks before admission; `Request.cpp::Requests::callback` checks before prepare/publication and still invokes Base record processing before `finish` releases the terminal. The plan must observe both first-pass refusal and completion-time failure, not infer one from the other.
- Confirmed shutdown source: Base `iocInit.c::doCloseLinks` resolves device support from the installed DSET, not the current DTYP. It calls del_record under the record lock and sets PACT during link closure. The snmp3 hook permits detach only after completion entry closes and entered processing finishes. AfterCloseLinks is before callback join and isolated queue destruction. Do not require PACT zero after that Base phase or read saved records/contexts after isolated cleanup.
- Hypotheses to verify: mismatch during an accepted request preserves its identity and captured SET payload, raises LINK/INVALID without input publication, completes once and retires normally; restoring only DTYP before completion permits the original request to finish normally; shutdown detaches both pointer directions despite changed DTYP and admits no replacement. These remain NOT RUN until the real paths below execute. No unresolved product-policy choice is asserted by this draft.

**Required cases.** Each case uses a fresh process, passive ai/ao records, separate Base FLNK counters, distinguishable initial/native/SET values, and a valid alternate DTYP choice resolved from each record's loaded menu. Do not assume the alternate choice is numeric zero. Only real `dbPutField` changes DTYP; do not assign `record->dtyp`, replace DSET, call an internal completion directly, or change context state to arrange the expected outcome.

| Case | Required real stimulus | Required observation |
| --- | --- | --- |
| `idle` | Change DTYP on attached idle records, explicitly process, restore DTYP, then explicitly process again. | Put succeeds and the field changes while DSET, context, binding and handle stay original. Mismatched processing raises LINK/INVALID with no admission or wire request. Restoration alone sends nothing; the later explicit request succeeds with the same binding/handle. |
| `active` | Admit real GET/SET, observe the actual outer UDP request and PACT, delay the response externally, then change DTYP and allow the real response/completion. Keep DTYP changed for a later explicit processing attempt. | The original request identity/payload remains; each accepted request completes once with LINK/INVALID and PACT clear before shutdown. Input data and successful-native-publication state are preserved. Base FLNK runs once per completion. No replacement is admitted and the later mismatched attempt sends nothing. The already transmitted SET may have changed the agent: verify its original captured payload separately and never claim rollback. |
| `restored` | With a real request outstanding and completion held at the same external boundary, change DTYP and restore its original value before completion. | Both puts are observed with no new request or changed binding/handle. The original identity completes successfully once; input publication and the original SET payload match the ordinary path. No sticky mismatch or automatic rebind is assumed. |
| `shutdown` | Hold the real Base callback consumer; admit GET/SET and observe successful native terminals with completion still queued or pending. Start actual isolated `iocShutdown`; while it is still running with admission closed, completion entry open and no entered processing, change DTYP. Release the hold so real drain/completion and shutdown proceed. | Prove the overlap from state/phase observations, not a sleep alone; require Runtime thread exit before release to exercise the stop/drain interval. Completion reports mismatch while records are still attached, with the accepted identity completed once and no new wire work. AfterCloseLinks observes null dpvt and context record pointers despite changed DTYP, unchanged installed DSET, retained contexts and closed entry. A further DTYP field put in that hook must not attach snmp3 or admit work. BeforeFree retains contexts; post-cleanup reports zero contexts. |

All cases observe actual put status, current/frozen DTYP, link/type, DSET, context/record association, handle and full request identity, value/publication state, PACT, STAT/SEVR, per-record FLNK count, Runtime/Requests state and scheduler reservations. A field put is not a processing request. Check statuses and state immediately after each step so later restoration cannot hide an intermediate failure. Read record data under its Base record lock; read mutable context fields only with an established quiescent phase or existing safe observation mechanism. Do not add a product observer solely to bypass missing synchronization. Check inventory before saved-context access, and stop unsafe dereferences while recording failure if an early-release control invalidates that precondition. Release test holds and join test threads on every failure path without repairing product state; timeout, forced exit, missing phase or unsafe access is non-passing.

**Ordered implementation, after separate approval.**

1. P001: add `tests/rewrite/db/record-dtype.db` with the two passive records and separate FLNK counters; extend `tests/rewrite/RecordTest.cpp` with the four modes, real field writes, observations and shutdown hooks. Reuse current record driver, generated registrar, Runtime/Requests and NativeAgent. Keep production sources and installed Base unchanged. Use existing outer UDP delay and Base callback-consumer holds; first prove each intended phase was reached. Emit observations before cleanup, catch errors inside C hooks, and use only inventory snapshots after record/context storage is destroyed.
2. P002: extend `tests/rewrite/test_records.py` with `--case live-dtype`, running all four modes in fresh child processes, explicit phase inventories and separately named predicates. Reuse real native/agent/proxy and existing process/loaded-library checks. Record wire request and response identities, declared deadlines, source/product/Base/library hashes, exact command outcomes and normal child cleanup. A missed active/stop window is NOT RUN or a failing case, never an implicit pass. Parser probes use retained real events and are reported separately from IOC execution.
3. P003: extend `tests/rewrite/test_record_controls.py` with a bounded `--dtype-controls` group, mutually exclusive with existing groups. Compile four separate defective support copies: omit only the DTYP comparison in `validate`; omit terminal release in `Requests::finish`; omit the first-detach dpvt clear; omit the context record-pointer clear together with the cleanup-guard companion edit described below. Map them respectively to named mismatch/refusal checks, `dtype-retirement-settled`, `dtype-shutdown-dpvt-cleared` and `dtype-shutdown-record-cleared`. Require the same named check present once and passing in a reference, the intended defective library actually loaded, full required lifecycle phases and normal process/fixture cleanup before accepting detection. A crash, abort or watchdog is not a qualified control. For the context record-pointer control only, also replace `require(!context->record);` in `Requests::queuesDestroyed()` with `(void)context->record;` in the separately compiled defective copy, following the existing `CONTEXT_CLEANUP_GUARD` pattern. Observe the uncorrected non-null pointer and evaluate `dtype-shutdown-record-cleared` before context destruction; neither the runner nor the defective copy may repair the pointer. Retain actual queue/context destruction, post-cleanup observations and normal child/fixture cleanup. Both source edits and the loaded defective-library identity must be recorded. This compound control detects the omitted detach clear; it does not independently qualify the bypassed cleanup assertion. Production sources, the unmodified reference and Base retain that assertion. The shared-validator mutation tests the DTYP comparison, not independent necessity of both validation call sites; document both limits and a term-to-control/precondition map. Do not mutate Base or replace unmodified internal spans.
4. P004: document supported environment, four modes, commands, phase requirements, expected alarms and the distinction between a field write, binding and transmitted SET in `tests/rewrite/README.md`. Record executed evidence in this canonical supplement only after it exists. Build ordinary root and test targets and fresh ASan/UBSan products with the existing builder, which already builds RecordTest; no new binary is needed. Preserve existing drivers and run the verification below. Obtain independent third-person implementation review and a separate second-person changed-document pass before commit preparation. Completion of this supplement does not close M6 or all T1/T12/T14 cells.

**Verification and acceptance.** V001: root/test builds and the fresh sanitizer build must succeed with recorded source/product identities. V002: all four modes pass in three ordinary fresh-process trials and one ASan/UBSan trial; report per-mode results and actual phase preconditions, not only an aggregate return code. V003: all four compiled controls qualify against a passing instrumented reference; distinguish checks that one control jointly detects from independently discriminated properties. For the compound context record-pointer control, require the unchanged faulty pointer observation and named-check failure before real destruction, complete post-cleanup evidence and no claim that the bypassed cleanup assertion was independently tested. V004: baseline, live-detach, repeat-detach and queued-shutdown regressions pass on both product sets, since the shared RecordTest/runner and shutdown observation path change. V005: missing/duplicate/malformed phase observations fail parser checks, then independent technical and reader reviews have no unresolved in-scope findings. ASan/UBSan covers rebuilt module/test products; Base/system/vendor dependencies remain uninstrumented and leak detection is disabled. No leak-free, all-record-type or timing-equivalence claim follows.

The following are proposed commands for the new CLI, not current executable verification. Run from the repository root and use new output directories for every invocation. The ordinary command is repeated with three distinct output names; regression cases also need separate ordinary/instrumented directories.

```bash
make -j2
make -C tests/rewrite -j2
python3 tests/rewrite/build_r5_sanitizers.py --output work/dtype-sanitizers
runner=tests/rewrite/test_records.py
python3 "$runner" --case live-dtype --output work/dtype-ordinary-1
python3 "$runner" --case live-dtype --output work/dtype-ordinary-2
python3 "$runner" --case live-dtype --output work/dtype-ordinary-3
products=work/dtype-sanitizers/products
python3 "$runner" --case live-dtype --products "$products" --sanitizers --output work/dtype-asan
receipt=work/dtype-sanitizers/sanitizer-build.json
controls=tests/rewrite/test_record_controls.py
python3 "$controls" --build-receipt "$receipt" --dtype-controls --output work/dtype-controls
```

Current result: review-6 terminal-type finding F-subagent_gpt6_dtype_r6-001 is implemented and independently verified. Both new ordinary/instrumented four-mode invocations passed 72 checks; 36 malformed terminal-field probes and four valid-type/wrong-value probes were rejected as intended. Existing 213 structural/77 DTYP probes and five retained control/reference parser verdict sets remain correct. Technical correction review 7 and separate changed-document reader review 8 passed with zero unresolved must-fix/minor findings. P001-P004 and V001-V005 are complete within this supplement. Product/C++ sources are unchanged; no broader M6 acceptance follows. Next action: prepare the commit only on owner request.

###### Live DTYP Plan Review

2026-10-08 (Pacific), independent third-person review 1: CHANGES REQUIRED, one medium must-fix and zero minor findings. Frozen target whole-file SHA256 `bbfdb7cf5ba12a25448bf8f81f850daa9e247c6ddbcc0a9cb671dccd1c55d799`; the complete draft and entry point stayed unchanged during review. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_085021_subagent_gpt6_dtype_plan_r1_initial.md`, SHA256 `d497dc5ec147cea1ec1d2fba2c95f4802b7d3adb1805205f1086729dbcf585a7`.

Finding `F-subagent_gpt6_dtype_plan_r1-001` concerns P003: omitting only the context record-pointer clear leaves a non-null pointer that `Requests::queuesDestroyed()` rejects, preventing the full cleanup required by the control qualification. The existing repeated-detach controls pair that fault with a test-only cleanup-guard edit, retaining the faulty pointer observation and actual context destruction. The reviewer recommends explicitly declaring that compound mutation and its limit: it does not independently test the cleanup assertion. State: needs-user-decision; no correction has been applied, and plan acceptance/implementation authorization remain none.

Actual reviewer executions under `<local>/dtype-plan-review-r1/`: baseline passed 11 runner/206 driver checks; queued-shutdown passed 12 runner/213 driver checks; an unmodified instrumented repeat-detach reference passed 55 runner checks. The one-edit pointer fault, using byte-identical retained products with the defective library confirmed loaded, returned child exit 1 without crash or forced cleanup; the real isolated path reached AfterShutdown but retained 23 contexts and emitted no post-cleanup event. This confirms the cleanup conflict, not a qualified DTYP control. An earlier symlink-based attempt loaded the reference library and is excluded. `audit.json` records five provenance checks; those are receipt analysis, not extra IOC runs. No new DTYP case/control, fresh build or second-person pass ran. P001-P004 clauses remain unchanged pending owner disposition; no broader M6 acceptance follows.

2026-10-08 (Pacific), finding 001 disposition: accepted by the owner and applied to P003/V003. The context record-pointer control now explicitly includes the existing cleanup-guard companion edit only in its defective support copy, preserves observation of the faulty pointer before real destruction, requires complete cleanup evidence and records the compound-control limit. No other plan scope or implementation order changed. Independent correction recheck is pending; Plan Status remains draft, Plan Acceptance none and Implementation Authorization none. This direction authorizes the plan correction, not implementation.

2026-10-08 (Pacific), independent third-person correction recheck, review 2: PASS, zero unresolved must-fix and zero minor findings. Finding F-subagent_gpt6_dtype_plan_r1-001 is addressed in P003/V003. Reviewed target whole-file SHA256 `10deab9bce18564b58e3fa5f9af9981f0efa64dd81823f7fea46058e35ea835a`; report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_090420_subagent_gpt6_dtype_plan_r1_on_rev20261008_085021.md`, SHA256 `b38ae5dfe8b467dc2aedd492c45ba12272548b3b2d4563be1dc9bf5a9ae10777`. The recheck compared the correction with existing source and retained real execution receipts; no new IOC, build, control, sanitizer or parser execution ran. This establishes plan coherence, not qualification of the proposed DTYP cases or controls. A separate second-person plan pass remains pending; Plan Status remains draft, Plan Acceptance none and Implementation Authorization none.

2026-10-08 (Pacific), review 3 overall and the first separate second-person plan pass: PASS, zero unresolved must-fix and zero minor findings. The incoming implementer seat covered the complete supplement, review/disposition passages and Next session entry point at frozen whole-file SHA256 `676e3ad1fbdb4bcfee9bba5c10cc9ed35fc1006b3ba94f8376684acb0858b6b4`. Source, current CLI help and retained real receipts support the actionable scope, prerequisites, implementation order, commands, acceptance criteria, evidence limits and authorization state. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_150230_subagent_gpt6_dtype_plan_r1_on_fup20261008_090420.md`, SHA256 `b6264804295db8983179bcaaf20c51582dc85c5f3f52ca4fe5bf4971cfb4f430`. No new IOC, build, sanitizer, compiled-control or parser qualification ran. Both required plan review passes are complete; plan clauses remain unchanged. Plan Status remains draft, Plan Acceptance none and Implementation Authorization none pending owner direction.


###### Live DTYP Implementation And Verification

2026-10-08 (Pacific), authorized P001-P004 implementation: `record-dtype.db` adds passive ai/ao and separate FLNK counters; RecordTest runs idle, active, restored and isolated-shutdown modes with actual DTYP puts, loaded-menu choices, real UDP observations and pre-destruction pointer checks. The runner validates phase inventories, identity/binding preservation, alarms/publication, retirement, detach and actual cleanup; the control runner builds the four designated defective libraries. `tests/rewrite/README.md` specifies commands, outcomes and qualification limits. Production/Base code is unchanged.

| Check | Executed result | Evidence |
| --- | --- | --- |
| V001 | Ordinary root/test builds passed; all 17 fresh ASan/UBSan products built. | `<local>/dtype-root-final-build.log`, `<local>/dtype-test-final-build.log`, `<local>/dtype-sanitizers/sanitizer-build.json` |
| V002 | Four modes passed in each of three ordinary invocations and one instrumented invocation: 68 runner checks per invocation, 16 fresh IOC processes. | `<local>/dtype-execution/final-ordinary-{1,2,3}/results.json`, `<local>/dtype-execution/final-asan/results.json`, per-mode child receipts and real observations |
| V003 | All four compiled controls qualified against a passing instrumented reference. Each actual defective library failed its designated check with complete lifecycle observations, normal IOC/fixture exits and no forced cleanup or sanitizer diagnostics. The pointer control observed its uncorrected pointer and evaluated its named failure before real destruction. | `<local>/dtype-controls-final/results.json`, per-control `mutation.json`, compiler/loader receipts and `cell/run/` events |
| V004 | Baseline 11, live-detach 14, repeat-detach 55 and queued-shutdown 12 runner checks passed on both ordinary and instrumented products. | `<local>/dtype-execution/{baseline,live-detach,repeat-detach,queued-shutdown}-{ordinary,asan}/results.json` |
| V005 | All 213 missing/duplicate/malformed-event probes and 77 wrong-DTYP probes are rejected through shipped `dtype_checks` and `Runner`, using retained real IOC events; the unmodified events pass 50 predicates. Four mutually exclusive control-group CLI checks passed before the correction. Review 3's expected-DTYP finding 002 is accepted, implemented and independently verified by technical correction review 4; separate changed-document reader review 5 also passed. Review 6's terminal-type finding F-subagent_gpt6_dtype_r6-001 is implemented and independently verified by technical correction review 7; separate changed-document reader review 8 also passed. The terminal-type correction rejects 36 malformed/missing-field probes and four valid-type/wrong-value probes; these are parser checks, not additional IOC executions. No unresolved in-scope review finding remains. | `<local>/dtype-execution/cli-check.json`; `<local>/dtype-correction/parser-results.json`; independent correction reviews below |

`<local>/dtype-execution/verification.json` records result hashes, executed check counts and control outcomes. The first control run exposed a runner predicate that combined forbidden extra requests with the original delivery-window precondition. The final predicate checks the original two windows separately while the exact request-count check rejects additional work; only the complete final control run qualifies. The final four-mode matrix was rerun after this correction. Regression receipts precede that live-dtype-only predicate refinement; their RecordTest source/product is unchanged. No failed preliminary run is counted as final acceptance.

The shared-validator control does not independently qualify both call sites. The context-pointer control does not independently test the cleanup assertion it deliberately bypasses. Base/system/vendor dependencies remain uninstrumented and leak detection disabled; no all-record, race-free, leak-free or timing-equivalence claim follows. Parser replays are not additional IOC executions. The P004/V005 independent implementation and separate second-person changed-document reviews are complete; M6 and full T1/T12/T14 acceptance remain open.

Independent third-person implementation review 1, 2026-10-08 (Pacific): PASS, zero must-fix and zero minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_153650_subagent_gpt6_dtype_plan_r1_initial.md`, SHA256 `31216c67fc4dbcad60083e0aba1c3d283d4f530b2c163a6c1649083be6a44136`. All six frozen target files matched `<local>/dtype-execution/review-target.json` throughout the review. Independent actual instrumented execution passed 68 checks (`<local>/dtype-implementation-review-r1-asan/`); actual execution with the final context-pointer defective library failed only `dtype-shutdown-record-cleared`, with all four children exiting normally after real cleanup (`<local>/dtype-implementation-review-r1-record/`). The separate per-child provenance and mutation audit is retained in `<local>/dtype-implementation-review-r1-audit.json`; it is a receipt audit, not another IOC execution. No implementation correction was required. The separate second-person changed-document pass is still pending.

Separate second-person implementation review 2, 2026-10-08 (Pacific): PASS, zero unresolved must-fix and zero minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_154139_subagent_gpt6_dtype_plan_r1_on_rev20261008_153650.md`. The incoming test operator seat covered all changed README and canonical passages against current source, CLI help and retained real outputs. Both command blocks passed shell syntax checks, documented counts and limits matched the receipts, and all six target hashes remained unchanged through review. No new IOC, build, sanitizer, control or parser qualification ran, and no correction was required. Both required implementation reviews are complete; P001-P004 and V001-V005 are complete within this supplement. Next action: prepare the commit only on owner request. M6, full T1/T12/T14 acceptance and formal session closure remain pending. Report SHA256: `21a0c2b0dea16ad1d22ccde956d2b9443f7770070849ed92d2182ba0491f594a`.

Owner-requested third-person implementation review 3 overall, second technical pass, 2026-10-08 (Pacific): CHANGES REQUIRED, one medium must-fix and zero minor findings. Finding F-subagent_gpt6_dtype_plan_r1-002 shows that `test_records.py` checks current DTYP at put phases but omits the expected value at completion/refusal and compares detached DTYP only with stop_done. The shipped parser accepted actual-event-derived observations with shutdown stop_done/detached both restored to original, and three other wrong completion/refusal values, passing all 46 parser predicates in each case. These are parser acceptance probes, not observed IOC misbehavior. A new actual four-mode ordinary execution passed 68 checks with correct DTYP values, normal child exits and real cleanup (`<local>/dtype-implementation-review-r3-live/`). Probe evidence: `<local>/dtype-implementation-review-r3-probes.json`; retained-receipt audit: `<local>/dtype-implementation-review-r3-audit.json`. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_160411_subagent_gpt6_dtype_plan_r1_on_fup20261008_154139.md`, SHA256 `ce0884f19716384546142d437fcf671d186e2eb1f68b774b50cfde95185175c8`. All six target hashes stayed fixed through this review. Proposed correction is phase-specific expected DTYP checks, respecting sequential ai/ao changes, plus retained-real-event wrong-value probes and verification of existing scenario/control outcomes. Finding 002 is awaiting owner disposition; no correction or new second-person pass was performed. Earlier completion statements record the state before this newly observed verification gap; supplement acceptance now requires its resolution.

Finding 002 correction authorization and implementation, 2026-10-08 (Pacific): the owner accepted all reported corrections. Each `dtype-<mode>-current-dtype` predicate now checks both records at every required phase against the initial loaded-menu original/alternate, advancing expectations only at explicit changed/restored puts and preserving sequential ai/ao state. The README describes this condition. `<local>/dtype-correction/parser_checks.py` executes shipped `dtype_checks` and `Runner` on the retained real review-3 observations: the original passes 50 checks; all 213 missing/duplicate/malformed probes and 77 wrong-DTYP probes are rejected, including every record/phase and the coupled stop_done/detached restoration (`parser-results.json` in the same directory). These are parser replays, not IOC executions. C++ fixture/driver, product sources and compiled products are unchanged. Actual four-mode runs and four compiled controls are being repeated; independent correction recheck and separate changed-document review remain pending.

Finding 002 correction verification, 2026-10-08 (Pacific): the corrected shipped runner passed 72/72 checks in each of three ordinary invocations and one ASan/UBSan invocation, covering 16 fresh IOC processes (`<local>/dtype-correction/{ordinary-1,ordinary-2,ordinary-3,asan}/results.json`). All four separately rebuilt defective libraries qualified against the passing 72-check instrumented reference (`<local>/dtype-correction/controls/results.json`, complete and passed), with their designated checks failing, actual defective-library identity confirmed and normal child/fixture cleanup. The 213 structural and 77 semantic parser probes passed, including the coupled shutdown stop_done/detached fault. `<local>/dtype-correction/commands.json` retains exact invocation outcomes; `verification.json` indexes results and hashes. C++ fixture/driver and production sources/products are unchanged; qualified existing ordinary/instrumented products were reused. Prior baseline/live-detach/repeat-detach/queued-shutdown regressions were not rerun for this live-dtype-only predicate correction. No broader coverage or independent control discrimination is added. The correction is awaiting the independent technical recheck and separate second-person changed-document pass.

Independent finding 002 correction recheck, implementation review 4, 2026-10-08 (Pacific): PASS with zero must-fix/minor findings. Finding 002 is implemented and independently verified. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_162634_subagent_gpt6_dtype_plan_r1_on_fup20261008_160411.md`, SHA256 `01fcc9719380d47c6a4119a427a3d0956520d11730bf726c74b94be6306d35f1`. An independently derived 38-phase/two-record expectation table confirmed all 76 individual values and rejected their opposite values plus the coupled shutdown fault; these 77 parser probes are not IOC executions. A new actual ordinary four-mode invocation passed 72 checks (`<local>/dtype-correction-review-r4-live/`); `<local>/dtype-correction-review-r4.json` records independent semantic coverage and 40 actual child-receipt audits, of which four children are new executions by this reviewer. All six target hashes remained fixed through the recheck. No additional correction was required; the separate second-person changed-document pass remains pending.

Separate finding 002 changed-document review, implementation review 5 overall and second reader pass, 2026-10-08 (Pacific): PASS with zero must-fix and zero minor findings. The maintainer acceptance seat covered the complete correction text and supplement context against current source and retained actual results. Historical and corrected counts, parser/IOC/audit distinctions, unchanged-product reuse, omitted unrelated regression reruns and remaining scope were verified without new runtime qualification. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_163457_subagent_gpt6_dtype_plan_r1_on_fup20261008_162634.md`. All six reader-target hashes remained unchanged. Finding 002 is implemented and independently verified; both required correction reviews are complete. P001-P004 and V001-V005 are complete within this supplement. Next action: prepare the commit only on owner request. M6, full T1/T12/T14 acceptance and formal session closure remain pending; no Git/remote, handoff or memory action is included. Report SHA256: `faf77abf4fc8bc1ba1501855fa04e0d0835ef52d2944228a309670ba78f1bb53`.

Fresh-context third-person implementation review 6, 2026-10-08 (Pacific): CHANGES REQUIRED, zero must-fix and one low-severity minor finding. F-subagent_gpt6_dtype_r6-001: dtype_inventory does not validate the phase-specific shutdown stop_window terminal type, so JSON true and floating 1.0 satisfy the integer success comparison; independent ai and ao boolean replays each pass all 50 shipped parser predicates. This violates the malformed-observation rejection requirement in C3/V005 but does not establish IOC misbehavior. A new actual ASan/UBSan four-mode invocation passed 72/72 checks with normal IOC/fixture cleanup (`<local>/dtype-review-r6-asan/`). Saved-JSON parser replays and retained-result/product/receipt audit are separate evidence (`<local>/dtype-review-r6-evidence-final/audit.json`). Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_171133_subagent_gpt6_dtype_r6_initial.md`, SHA256 `558180219edbfa3b6147f27c8168f8f7dd8ee3b339ab547be272b8b64a36e475`. All six target hashes remained unchanged through review. Proposed correction is exact integer terminal and boolean terminal_same schema validation at the required shutdown phase, with retained-real-event wrong-type probes for each record. Owner disposition is pending; no correction or new reader pass was performed. Prior finding 002 remains resolved; historical passes do not establish current zero-finding acceptance after this new evidence.

Review-6 terminal-type correction, 2026-10-08 (Pacific): the owner accepted F-subagent_gpt6_dtype_r6-001 and authorized its bounded correction. dtype_inventory now requires exact integer terminal and boolean terminal_same in each shutdown stop_window record before the existing success/identity proof. The README states this requirement. `<local>/dtype-terminal-types/verify.py` uses the actual review-6 ASan observations: all 36 saved-JSON wrong-type/missing-field probes fail dtype-events; all four valid-type but wrong outcome/identity values pass the schema and fail dtype-shutdown-window. Original observations pass 50 predicates, 213 structural and 77 wrong-DTYP probes still fail, and the passing reference plus four existing defective-control event sets retain all 50 parser verdicts (`parser-results.json`). These five control event replays are not new compiled-control executions. New actual ordinary and ASan/UBSan four-mode invocations each pass 72 checks, covering eight fresh IOC processes (`<local>/dtype-terminal-types/{ordinary,asan}/results.json`); commands.json and verification.json retain invocation outcomes and result hashes. C++/product sources and qualified ordinary/instrumented products are unchanged; no new build, compiled-control run or unrelated regression run was needed for this phase-specific schema correction. Independent technical correction and separate reader reviews remain pending.

Independent terminal-type correction review 7, 2026-10-08 (Pacific): PASS, zero unresolved must-fix/minor findings. F-subagent_gpt6_dtype_r6-001 is implemented and independently verified. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_173924_subagent_gpt6_dtype_r6_on_rev20261008_171133.md`, SHA256 `91156fa0100a2a876846fc5866f1dc960f5b7857ae852794d3da9aa063d5c39d`. Independent parser execution rejected 32 wrong-type/missing cases at dtype-events and eight valid-type/wrong-value cases at dtype-shutdown-window, reexecuted all 36 author-saved type inputs and five original finding inputs, and preserved five control/reference verdict sets. Genuine events passed 50 checks including the 37 snapshots without terminal fields. The reviewer audited both new 72-check executions and their 12 normal IOC/fixture receipts; no new IOC/build/control qualification ran in this recheck. Evidence: `<local>/dtype-review-r7/results.json`. All six frozen target hashes remained unchanged. The separate changed-document reader pass remains pending.

Separate terminal-type changed-document review, implementation review 8 overall and third reader pass, 2026-10-08 (Pacific): PASS, zero unresolved must-fix and zero minor findings. The test-maintainer seat covered the complete changed README explanation, current canonical passages and review-6/correction/recheck record in accepted-plan context. Source and retained real evidence support the exact-type versus value-proof distinction, author 36/4 and independent 32/8 parser counts, prior 213/77 checks, 50 parser predicates, two actual 72-check invocations and the separate replay/audit classifications. Product reuse and omitted build, compiled-control and unrelated regression runs remain explicit. No new qualification ran in this reader pass. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_174627_subagent_gpt6_dtype_r6_on_fup20261008_173924.md`. All six reader-target hashes remained unchanged. F-subagent_gpt6_dtype_r6-001 is implemented and independently verified; both required correction reviews are complete. P001-P004 and V001-V005 are complete within this bounded supplement. Next action: prepare the commit only on owner request. M6, full T1/T12/T14 acceptance and formal session closure remain pending; no Git/remote, handoff or memory action is included. Report SHA256: `19e561d8e61280a0ba34e6a6747d756da0b480736bd8dd1816ccc2b0b67f626b`.

###### Accepted Device-Support Transition Verification Supplement

Plan Status: accepted
Plan Acceptance: 2026-10-08; reviewed P001-P004, plan20261008_184537 and reviews 1-2
Implementation Authorization: 2026-10-08; P001-P004/V001-V005, `auth20261008_191402`

Planning date: 2026-10-08 (Pacific). Source basis: `01b68b617b387271e398810b82bf90cfdf115aec`. The owner directed preparation of the next T1/T12 verification plan after the remaining-work comparison. Existing D13 and the Binding And Conversion Contract define the behavior; this supplement proposes tests, not a policy change. Earlier supplement acceptance does not authorize this new implementation. The canonical plan is this section; the session plan artifact is an immutable reference to its draft version, not another work register.

Scope: passive ai Integer GET and ao OpaqueFloat SET, selecting Base's loaded Soft Channel device support through real DTYP and INP/OUT writes. Distinguish refusal before detach permission, transfer to another installed support while the original context is still attached, refusal after the first detach, and attempted return to snmp3. Use actual Base shutdown phases and retained snmp3 ownership. These two record kinds qualify this shared extension path, not every DSET separately.

Out of scope: production or installed Base changes, new device support, periodic SCAN, production CA writes during shutdown, arbitrary concurrent field-write stress, other record kinds or pointer-backed payloads, other D13 cells, M11/M12 work, Git/remote mutations and handoff/memory. Implementation and verification are authorized only for P001-P004/V001-V005 below. A product defect or required contract change returns to the owner.

**Premises classified before drafting.**

| Class | Evidence and implication |
| --- | --- |
| Confirmed coverage gap | `RecordTest.cpp::dtypeCells` changes DTYP without attempting a device-link replacement; `liveDetachCells` and `repeatDetachHook` retain snmp3 DTYP during their link puts. Existing passes do not establish their combination or another-support attachment. |
| Confirmed source distinction | Base `ioc/db/dbAccess.c::dbPutField` routes DTYP through locked `dbPut`, whereas `dbPutFieldLink` selects the proposed support from current DTYP and the old support from installed DSET. Link-type validation and extension availability checks precede the old `del_record`; a failed incompatible link is not evidence that snmp3 refused detach. |
| Confirmed source boundary | `Request.cpp::Requests::detach` refuses until `detachAllowed`; `DeviceSupport.cpp::detach` also refuses an empty dpvt after initialization. `DeviceSupport.cpp::add` refuses post-initialization attachment. `Register.cpp` grants detach permission only after real Runtime stop in AtShutdown. |
| Confirmed Base support path | The generated `dbd/snmp3RecordTest.dbd` contains ai/ao Soft Channel devices. Base `ioc/dbStatic/dbStaticRun.c::dbInitDevSup` supplies its own `devSoft_DSXT` for CONSTANT support; both extension entries succeed without allocating a snmp3 context. This is installed Base behavior, not a test-written replacement. |
| Confirmed non-transactional failure path | After the old detach succeeds, Base installs the new link, clears dpvt, selects the new DSET and calls new `add_record`. If that call fails, Base clears DSET and sets PACT. Therefore a rejected return from Soft Channel to snmp3 must not be described as restoring the prior Soft Channel link/DSET. |
| Hypothesis requiring real execution | The five cases below reach the intended extension/lifecycle phases, obey the source-derived results, retain context storage through BeforeFree and clean up normally. No combined-case runtime result exists yet. |

Base source inspected: `EPICS-env/epics-base-src`, tag R7.0.10, `modules/database/src/`, including `ioc/db/dbAccess.c::dbPutFieldLink`, `ioc/dbStatic/dbStaticLib.c::dbCanSetLink`, `ioc/dbStatic/dbStaticRun.c::dbInitDevSup`, `ioc/misc/iocInit.c::doCloseLinks` and `std/dev/devAiSoft.c`/`devAoSoft.c`. The selected installation comes from `configure/RELEASE.local`; its header reports Base 7.0.10. The pinned `dbCommon.dbd.pod` DTYP field is DBF_DEVICE without process-passive behavior. Source inspection is a planning premise, not proof that the installed binary has executed these paths. Execution must identify that binary, generated DBD and loaded support and fail explicitly if prerequisites differ.

**Cases and observable acceptance.** Each mode runs in a fresh isolated IOC process using the actual worker/native/loopback agent path. Resolve both snmp3 and the exact Soft Channel choice from each loaded menu and verify the corresponding DSET/DSXT. Use a numeric CONSTANT replacement appropriate for Soft Channel, not an INST_IO string that Base rejects before calling the old extension. Record a separate incompatible-link probe as Base validation only. Do not directly invoke an extension or change DSET, dpvt, context pointers or detach permission to create a phase.

| Mode | Real stimulus | Required observation |
| --- | --- | --- |
| `idle` | With initialized, idle snmp3 contexts, select Soft Channel through DTYP and attempt the compatible CONSTANT INP/OUT writes. Restore snmp3 DTYP after observing refusal, then explicitly process real GET/SET. | Each link put returns the snmp3 refusal status; original complete INST_IO link/type, installed DSET, context/handle/binding and both pointer associations remain. A field write alone emits no native request. Explicit processing after restoration succeeds using the original binding. |
| `active` | For ai then ao, observe an actual UDP request, delay the response externally, change DTYP and attempt the compatible link write while the request remains active. Keep the changed DTYP through completion. | Link replacement is refused without detachment or changed captured request identity/payload. The original completion occurs once with LINK/INVALID and no input publication; an already transmitted SET can affect the agent. No replacement request is admitted. The original snmp3 DSET still performs shutdown detach. |
| `drain` | Hold the actual Base callback consumer until both native terminals are queued, begin real isolated shutdown and observe Runtime exit, closed admission, open completion entry and no entered processing. Change DTYP and attempt both compatible link writes while stop is still draining, then release the hold. | Detach permission is still false at each attempt; both link writes are refused with ownership/link/DSET preserved. Actual mismatch completions finish once while attached, entry then closes, and Base detaches normally. A missed overlap window is non-passing. |
| `before-close` | Register the observer after the production hook. In its AtShutdown callback, require closed entry, zero entered processing, detach permission true and the original contexts still attached. Select Soft Channel and put compatible links before Base doCloseLinks. Then select snmp3 again and attempt the original INST_IO links. | The first transfer succeeds through real old detach/new add: DSET becomes the resolved Soft Channel DSET, link/type becomes CONSTANT and both original snmp3 pointer associations clear while contexts remain retained. Returning to snmp3 fails at its post-initialization add: no new context/handle/request, dpvt remains null, DSET becomes null and PACT becomes true; observe the installed replacement link rather than assume rollback. No record processing follows these puts. |
| `after-close` | Allow Base doCloseLinks to perform the first detach. In AfterCloseLinks, first observe null pointer associations and the retained snmp3 DSET/link, then select Soft Channel and attempt compatible INP/OUT writes twice per record. | Both repetitions are refused by the old snmp3 empty-dpvt rule. Original link/type and installed DSET remain, DTYP retains its explicit changed value, and no support is attached or native work admitted. Existing empty-dpvt refusal is preserved. |

For every case, observations include put statuses, exact current DTYP and resolved original/target choices, installed DSET/DSXT identities, both original pointer associations, full link text/type, context inventory, handle/binding/activation/revision and request generation/admission, PACT/value/publication/alarms, per-record FLNK, Runtime/Requests entry/admission/detach states, and scheduler count/bytes. Expected current DTYP and DSET/link state are derived independently for every phase, including refusal/completion and failed new-add states. Error status alone does not prove preserved ownership. PACT zero is required at completed active work, not after Base link closure or a failed new-add.

Use record locks for record data and established quiescent phases for mutable context fields. Never reinterpret another support's dpvt as RecordContext; retain and safely validate the original snmp3 context separately. Observe transfer/refusal inside actual hooks while records exist, release holds and join threads on all failure paths, catch exceptions inside C hooks, and perform no record/context dereference after destruction. BeforeFree must retain the original context inventory; after testdbCleanup only inventory/state snapshots may prove zero contexts and settled ownership. Inspect a failed control without processing a partially detached record or repairing its state.

**Ordered implementation after plan acceptance and separate authorization.**

1. P001: add `tests/rewrite/db/record-support.db` with passive ai/ao and individual FLNK counters; extend `RecordTest.cpp` with the five modes, real field puts, loaded-menu/support resolution and phase observations. Reuse the existing native/agent, external UDP delay and callback-consumer hold. Register test observers after production registration and assert the required hook state rather than assume registration order establishes it. No new product observer, binary, internal stub or custom device support. V001/V002 verify this item.
2. P002: extend `tests/rewrite/test_records.py` with proposed `--case support-transition` and case-specific `--support-mode` selection (default all five; reject the option on other cases). Build an explicit ordered event inventory for the selected mode set and named assertions for each row above, using exact JSON types and phase-specific expected values. Capture real wire identities, SET payload and normal cleanup. A one-mode control run cannot be reported as a five-mode positive matrix. V002/V005 verify this item.
3. P003: extend `tests/rewrite/test_record_controls.py` with proposed mutually exclusive `--support-controls`. Compile three isolated product faults, with a mode-specific reference and the unchanged shipped driver/runner: omit the detach-permission guard in `Requests::detach` and run only `idle` to fail `support-idle-refusal`; accept empty-dpvt detach in `DeviceSupport.cpp::detach` and run only `after-close` to fail `support-after-close-refusal`; allow post-initialization `DeviceSupport.cpp::add` and run only `before-close` to fail `support-return-to-snmp3-refused`. In the first control, detect the unexpected transfer before the reference-only restored-processing step and skip that step on the faulty path, then complete real shutdown/cleanup without repairing pointers. Require the designated failure, actual defective-library identity, mandatory safe phases and normal cleanup; a crash, timeout, missing observation or forced cleanup never qualifies. Do not run the permission-bypass fault with accepted active work. Existing live-DTYP controls remain regressions for pointer clearing and terminal release; these three new controls do not independently discriminate every observed property. V003 verifies this item.
4. P004: document the five modes, source-derived boundaries, prerequisites, commands and control limits in `tests/rewrite/README.md`; record actual results here only after execution. Build ordinary root/test products and fresh ASan/UBSan products using the existing builder. Re-run baseline, live-detach, repeat-detach, live-dtype and queued-shutdown on both product sets because their driver/runner/cleanup paths are shared. Require independent implementation review and a separate second-person changed-document pass before commit preparation. V001/V004/V005 verify this item.

**Verification.** V001: root/test builds and the fresh sanitizer build succeed, with source, fixture, generated DBD, product, Base and loaded-library identities retained. V002: all five modes pass in three ordinary fresh-process invocations and one ASan/UBSan invocation, with actual phase preconditions and complete normal IOC/agent/proxy cleanup; record counts after execution rather than prescribe invented totals. V003: all three separately compiled controls fail their named checks against passing mode-specific instrumented references; each follows its stated safe path and records any reference-only step skipped after a detected fault. V004: all five named regressions pass on ordinary and instrumented products, and the existing four live-DTYP compiled controls still qualify under the shared runner. V005: retained actual events pass through the shipped parser; missing/duplicate phases, wrong field types, wrong current DTYP/DSET/link, wrong pointer associations and false no-reattach outcomes must be rejected. Parser replays remain separate from IOC execution. Independent technical and reader reviews have no unresolved in-scope findings.

The planned CLI is now implemented. Commands below reproduce the accepted verification procedure; executed output paths and outcomes are recorded separately below. Run from the repository root with a new output directory each time. For each V004 regression use the same runner/product selection with its case name and a unique output path.

```bash
make -j2
make -C tests/rewrite -j2
python3 tests/rewrite/build_r5_sanitizers.py --output work/support-sanitizers
runner=tests/rewrite/test_records.py
products=work/support-sanitizers/products
python3 "$runner" --case support-transition --output work/support-ordinary-1
python3 "$runner" --case support-transition --output work/support-ordinary-2
python3 "$runner" --case support-transition --output work/support-ordinary-3
python3 "$runner" --case support-transition --products "$products" --sanitizers --output work/support-asan
receipt=work/support-sanitizers/sanitizer-build.json
controls=tests/rewrite/test_record_controls.py
python3 "$controls" --build-receipt "$receipt" --support-controls --output work/support-controls
python3 "$controls" --build-receipt "$receipt" --dtype-controls --output work/support-dtype-controls
```

Recovery and limits: preserve all existing fixtures and results. Unexpected installed-support prerequisites or hook ordering produce explicit non-passing evidence and a plan correction, not an internal substitute or a broadened product change. Retain failed execution output; correct only authorized test/worktree files and rerun affected verification. ASan/UBSan covers rebuilt module/test products, not Base/system/vendor dependencies, and leak detection remains disabled. No general concurrency, all-record, timing-equivalence or whole-M6 closure claim follows. No ADR change is proposed because the binding contract is unchanged.

Current result, 2026-10-08 (Pacific): P001-P004 are implemented without product-source changes. V001-V005 PASS, including independent technical implementation review 1 and separate changed-document reader review 2, both without findings. The accepted supplement is complete only for its two record kinds and five modes. M6 and full T1/T12/T14 remain pending.

Device-support transition execution: `RecordTest.cpp` and the shipped `record-support.db` exercise actual Base DTYP and compatible INP/OUT puts in five fresh isolated processes per matrix invocation. `test_records.py` validates ordered, typed phase inventories, request/support/link identities, refusal and transfer results, real wire behavior and normal cleanup. `test_record_controls.py` adds the three restricted compiled defects. The test README documents prerequisites, commands, the five modes, safe control paths and coverage limits. These map respectively to P001, P002, P003 and P004.

| Verification | Actual result and evidence |
| --- | --- |
| V001 | Root and test builds pass (`<local>/support-execution/root.json`, `tests.json`); all 17 fresh sanitizer products build successfully (`<local>/support-sanitizers/sanitizer-build.json`). Source, fixture, generated DBD, installed Base, products and loaded-library identities are retained in actual run receipts. |
| V002 | Three ordinary matrix invocations and one ASan/UBSan invocation pass 97 checks each, with five fresh mode processes each: `<local>/support-final-ordinary-1/`, `<local>/support-final-ordinary-2/`, `<local>/support-final-ordinary-3/`, `<local>/support-final-asan/`. Actual idle restoration succeeds; active and draining mismatches finish once with LINK/INVALID; first transfer after stop succeeds; return to snmp3 fails without rollback; repeated after-close transfers are refused. All 14 contexts remain through BeforeFree and actual cleanup leaves zero. |
| V003 | All three separately compiled controls qualify against passing mode-specific instrumented references (`<local>/support-support-controls/results.json`, complete and passed). Permission bypass fails `support-idle-refusal`, empty-dpvt acceptance fails `support-after-close-refusal`, and post-init add acceptance fails `support-return-to-snmp3-refused`. Actual defective libraries load and normal shutdown/cleanup finishes. The idle defect explicitly skips restored processing after observed loss of original attachment. Combined checks also fail; these controls do not independently isolate every predicate. |
| V004 | Baseline, live-detach, repeat-detach, live-dtype and queued-shutdown pass on both ordinary and instrumented products, with 11, 14, 55, 72 and 12 runner checks respectively (`<local>/support-regression-ordinary-<case>/`, `<local>/support-regression-asan-<case>/`). All four existing live-DTYP compiled controls qualify (`<local>/support-dtype-controls/results.json`, complete and passed). |
| V005 | 12,386 actual-event parser replay checks pass (`<local>/support-execution/parser-probes.json`), covering the unchanged reference, selected modes, missing/duplicate events, missing/wrong-type fields and wrong typed support/link/pointer/request/record values. These are parser checks, not new IOC runs. A separate 51-check receipt/identity/CLI audit passes (`<local>/support-execution/verification.json`), including rejecting mode selection on another case and combined control groups before output creation. Independent technical implementation review 1 and separate changed-document reader review 2 PASS without findings; V005 is complete within the accepted supplement's scope. |

Independent device-support transition implementation review 1, 2026-10-08 (Pacific): PASS, zero must-fix and zero minor findings. The complete six-file change and accepted P001-P004/V001-V005 were reviewed at the frozen hashes in `<local>/support-execution/review-target.json`. Fresh ordinary and instrumented full matrices each pass 97 checks. Three direct reruns of the existing compiled support defects fail their designated checks with normal IOC/agent/proxy cleanup. The reviewer's independent receipt/identity audit passes 107 checks, and 109 additional actual-event parser replays validate mode ordering/selection, nested containers, gate values and coordinated false skip outcomes (`<local>/support-impl-review-r1/audit.json`). Those replays are not IOC executions. Three initial control invocations missing the sanitizer option are retained and excluded; only the corrected `control-*-asan` executions qualify. Existing four DTYP controls, regressions and build receipts were audited, not newly rebuilt or rerun by this review. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_195821_subagent_gpt6_thread_plan_r1_support-implementation.md`, SHA256 `a83bbdadda8fd024b0938ee92e6c4e4b32234a38c1af8ffab64ede61b13c49af`. No target changed through publication. At publication of review 1, the separate changed-document reader pass remained pending; implementation review 2 subsequently completed that pass without findings.

Separate device-support transition implementation reader review 2, 2026-10-08 (Pacific): PASS, zero must-fix and zero minor findings. The test-maintainer seat covered all changed README and canonical supplement passages, current entry and execution/review results. Forty document/CLI/receipt checks pass (`<local>/support-impl-review-r1/reader-check.json`); no new IOC, product build, compiled-control or parser qualification ran. The six targets remained at `<local>/support-execution/reader-target.json` hashes through publication; only supported current-status wording and this result were recorded afterward. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_200316_subagent_gpt6_thread_plan_r1_on_rev20261008_195821.md`, SHA256 `0def5003b3283c2054099e60add4538816873d4fb7f8f389ebda5d0e44d0e0f0`. Both required implementation reviews are complete without findings. The accepted supplement is complete within its stated scope; wider M6 acceptance remains pending.

Execution provenance and limits: `<local>/support-execution/qualification.json` records exact matrix, regression and control commands and return codes. The final receipts above qualify the current sources; earlier `support-ordinary-*` runs precede the final request/record predicates and are not the final matrix. The initial failed trial remains retained. No process crash, timeout, forced cleanup, sanitizer diagnostic or credential sentinel qualifies any passing cell. Sanitizers cover rebuilt module/native/test products; installed Base and system/vendor dependencies remain uninstrumented, with leak detection disabled. No all-record, general-concurrency, timing-equivalence, leak-freedom or wider M6 closure claim follows. The following plan-review entries describe the earlier draft state, before owner acceptance and execution.

Independent device-support transition plan review 1, 2026-10-08 (Pacific): PASS, zero must-fix and zero minor findings. The complete draft and entry point were checked against the accepted binding contract, current product/test sources, local Base R7.0.10 source, installed support symbols and generated DBD. Thirty source/identity, prospective shell-syntax and retained-event audit checks passed (`<local>/support-plan-review-r1/audit.json`); retained events were read through the shipped parser. The independent additional check traced runtime add without init_record and cleanup after failed add leaves DSET null. This establishes source-supported feasibility, not runtime qualification: no new IOC, build, combined mode or compiled control ran. All five proposed modes and three controls remain NOT RUN. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_190342_subagent_gpt6_thread_plan_r1_support-plan.md`, SHA256 `37523e8012add34669d5db4ed0cff627bab00bd5f335ab5365c12abef66fc480`. The frozen whole-file hash remained `c77d44f262d94f66592de470687715929282e61df769eed0193a3f6b0c8f6e6c` through review publication. Only current review status and next-action text were updated afterward; plan clauses and draft/acceptance/authorization fields are unchanged. Separate second-person plan review, owner acceptance and implementation authorization remain pending.

Separate device-support transition plan second-person review, review 2 overall and first reader pass, 2026-10-08 (Pacific): PASS, zero must-fix and zero minor findings. The incoming-implementer seat covered the entire supplement, current entry and review-1 outcome. Thirteen document/CLI/receipt consistency checks passed (`<local>/support-plan-review-r1/reader-audit.json`), including actual CLI help, prospective shell syntax, prior audit counts and source/receipt identities. No new IOC, build, combined-mode, compiled-control or support-parser qualification ran. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_191156_subagent_gpt6_thread_plan_r1_on_rev20261008_190342.md`, SHA256 `6a4ebc61d683929e7ed00583751c7c56962c63c5a0cb2f959e3b2f9a802a88e5`. The target remained at whole-file SHA256 `648c9faf4e0e071589046d29eea4135f60b814b83cea7d4e681a7ab457f5bb19` through publication. Both plan passes are complete; no plan clauses changed. Plan Status remains draft, Plan Acceptance none and Implementation Authorization none. Next: owner plan acceptance and separate implementation authorization; wider M6 acceptance remains pending.

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

Current status is in [Current Required Verification Results](#current-required-verification-results-2026-10-08). The dated table and supplements below preserve their execution-time scope and status.

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01 | Actual Base 7.0.10 records/DSET/worker/agent and production IOC/CA | Pending; subset PASS | All eleven DSET kinds and aliases; unknown binding/grammar/FTVL rejection; all five unsupported initial longout OOPT choices leave no usable binding; effective SIZV clamps; initial waveform BUSY; idle link/DTYP checks. Production IOC longin PINI runs after pre-PINI servicing and Base FLNK completes once, observed through actual CA. Remaining startup, live-field, periodic and concurrent-detach cells are unexecuted. Record receipts below. |
| T2 | 2026-10-02 | Actual numeric record/native and production IOC/CA path | Pending; subset PASS | All 68 advertised numeric GET pairs, including 48 native-tag/FTVL pairs, receive successful native input. Range/precision/extrema cover signed/unsigned 32-bit, 2^24/2^53, Counter64 INT64_MAX/MAX+1/UINT64_MAX, finite opaque extrema/subnormal/signed zero and initial NaN/infinity rejection. 121 conversion failures after successful native input preserve prior bytes/NORD and native-success state, with Base UDF behavior observed. Actual decimal DBR_STRING CA preserves 2^53+1 and INT64_MAX through int64out SET/int64in GET; UINT64 waveform reads UINT64_MAX exactly through CA STRING. Native exception/type-error and additional failure-after-success cells remain unexecuted; the signed-minimum CA cell is deferred to M11 by D13. |
| T3 | 2026-10-01 | Actual binary/text/structured record/native and production IOC/CA path | Pending; subset PASS | Exact binary octets including NUL, OID arcs and IPv4; text rejects embedded NUL or overflow without prefix publication; lsi VAL/LEN preservation, empty and maximum 32766-byte data. Actual lsi/lso VAL$ CHAR-array CA SET/GET is exact at 0/39/40/200/255 data bytes; CA DBR_STRING view is 39 bytes; short stringin/stringout SET/GET is observed. A 300-byte VAL$ write is rejected by caput before put and preserves prior VAL/LEN/native GET. Remaining structured/text boundaries are unexecuted; maximum-capacity CA cells are deferred to M11 by D13. |
| T4 | 2026-10-02 | Actual output DSET/native SET/GET and production IOC/CA | Pending; subset PASS | All five output DSET kinds and all 20 advertised numeric SET pairs. Signed/unsigned target boundaries, integral/fractional conversion, full nonnegative signed-64 capture and finite floating extrema/subnormal/signed zero are checked by separate native GETs; 67 invalid numeric outputs preserve requested values and produce no native SET. Exact Counter64 SET of 2^53+1; 36 binary32 wire-bit fixtures across four ambient modes; requested ao retained; nonfinite/overflow/negative Counter64/over-capacity SET rejected; empty and maximum lso payload. Base ao drive clipping/rate limiting and OVAL wire capture, all five first-pass IVOV values and lso DOL are verified. Actual CA writes exercise int64out decimal and stringout/lso short/CHAR-array values with separate native GETs. Remaining long-string boundary cells remain unexecuted; ao/longout CA and signed-minimum CA cells are deferred to M11 by D13. |
| T5 | 2026-10-01 | Actual Base value/alarm processing and outer UDP response dropping | Pending; subset PASS | Initial/local conversion failures preserve input data/UDF; errors survive input simulation bypass. All eleven record kinds report COMM/INVALID on actual NativeFailure/timeout code 2; a separate record deadline reports IPC Deadline/COMM. Waveform clears BUSY and follows Base UDF while native-success remains false. Native errorStatus/exception, failure after a valid GET, competing-alarm and monitor matrix remain unexecuted. |
| T6 | 2026-10-01 | Actual ai-FLNK-calc-FLNK-lsi chain | Pending; subset PASS | Source PACT is 1 during Base FLNK; target completes once with complete VAL/LEN. Other record/failure/deadline chains, fanout and delayed/duplicate external responses remain unexecuted. |
| T7 | 2026-10-01; 2026-10-03; 2026-10-04 | Actual Base dbPutField/RPRO, delayed UDP SET and response loss | Pending; subset PASS | All five outputs preserve the captured first payload and process the latest requested value through Base RPRO when admission is available; exact DBR_INT64 and 200-byte first lso payload. Same-value explicit retry uses two module generations/distinct native request IDs; configured native retry uses one generation/the same native request ID. Separate GET observes SET application despite lost successful responses. Scan and PROC reprocess cells remain unexecuted, Channel Access active writes are executed below and long active client writes are deferred to M11 by D13; the retained-ownership admission-rejection cell is replaced under D7 by admission behind retirement. Negative controls on the products before D7 (`c065755`), 2026-10-03/04: on the production CA path, for all five outputs and both a plain put and a put-callback second write while the first SET response was held, one SET reached the wire, the agent stored only the first value, the record ended WRITE/INVALID after the Base reprocess, and the put-callback client exited with status 0 while the device kept the previous value (`<local>/r6-ca-head-control-20261003-225443/`). The unheld snmp3RecordTest case, run with the committed test on the products before D7, gave the same outcome; the queued-behind-retirement branch ran in 0 of 5 trials (not run), and the record-path Result-to-Retired gap measured 10.09-10.25 ms (`<local>/r6-headc-active-unforced-20261004-014819/`). Revised products, D7 (`98bfdfc`) on 2026-10-03 and D7/D9 (`0b60abf`) on 2026-10-04: production CA plain put and put-callback second writes sent two SETs per output, the agent stored the first then the latest value, records ended NO_ALARM, and the put-callback client exited after the second delivery; 315 of 315 checks and `completions=44` (`<local>/r6-d7-ca-20261003-235055/`, repeated with D9 in `<local>/r6-d9-ca-20261004-000528/`). On the D7 products the unheld case admitted every output's latest value; the queued-behind-retirement branch ran in 5 of 5 trials and the record-path Result-to-Retired gap measured 10.20-10.56 ms (`<local>/r6-d7-active-unforced-20261003-235016/`). |
| T8 | 2026-10-01; 2026-10-05 | Actual Base simulation, output policy and DSET/native path | Pending; subset PASS | All six inputs use SIOL before admission and after a queued native completion; Base SDLY and ai RAW run; waveform BUSY clears before simulation bypass. All five outputs exercise three IVOA branches on first-pass/completion, live simulation after actual native timeout, synchronous/delayed simulation before admission, preserved current VAL/LEN and exactly-once terminal/FLNK processing. All five unsupported initial/live longout OOPT choices are observed. lso supervisory/closed-loop DOL, short IVOV and Base pre-DSET capacity reduction are verified by separate GET. Input failure and source switching, 2026-10-05, policy case on ordinary products (`inputSourceSwitch`, committed in `fd60298`): for all six inputs, SIMM selecting SIOL while an actual native timeout terminal waits in the Base queue, a return to normal mode with a further native failure, and a SIMM change while a Base SDLY callback is pending; the check `actual-six-input-failing-source-switch` passes with six `input_source_switch` events, each with a native-failure alarm, no native publication, the normal-mode failure preserved, no ownerless admission, SIOL selected and the terminal released once (`<local>/r6-s8e-regress-20261005-093844/policy/results.json`). Delayed-simulation mode-change cells for the five outputs are deferred to M11 by D13. A SIMM switch while an output SET is queued behind retirement, on the normal and deadline paths, is not yet executed and is run under D13. |
| T9 | 2026-10-01; 2026-10-03; 2026-10-04; 2026-10-05 | Actual maximum long-string/native/queue path | Pending; subset PASS | 32766-byte SET and three GETs; two borrowed terminals retain 796640 charged bytes; byte-exhausted request rejects synchronously; lowering limits preserves ownership and raising them admits a separate explicit request. Delayed native retirement/retry and other-address progress remain unexecuted. Products before D7 (`c065755`), 2026-10-03/04: on the outer-UDP drop-all path the worker exits on channel close; Deadline to Ready of the relaunched worker measured 372-393 ms over all five fresh-process relaunches with the committed test (373-383 ms in the 3 sampling trials), and with budgets below (186 ms) and above (766 ms) that range the Base reprocess was rejected WRITE/INVALID without a second SET (`<local>/r6-headc-deadline-queue-20261004-014819/`). Two-generation accounting was never reached, while a count limit of one rejected the reprocess synchronously and a raised limit admitted a separate explicit request (`<local>/r6-accounting-head-control-20261003-231608/`). D7/D9 products (`0b60abf`), 2026-10-04: admission to Ready of a queued successor measured 371-403 ms over all five relaunches of the run (371-391 ms in the 3 sampling trials, 403 and 392 ms in the two budget trials); with a 185 ms budget it ended COMM/INVALID without a second SET, with a 781 ms budget it was dispatched after Ready; the AMSG cell fails until M6 step 8 (`<local>/r6-d9-deadline-queue-20261004-000528/`). Two generations were held together in 3 of 3 trials (`count=2`, 2 x 5192 bytes) and the count-limit and raised-limit cells passed (`<local>/r6-d9-accounting-20261004-000528/`). 2026-10-05, AMSG of a never-sent Deadline (D8): the callback applies a staged alarm message and `prepare` stages `deadline before send` only for a Deadline that was not sent. In the deadline-queue case the below-threshold queued generation shows COMM/INVALID with that message; the sent generations (sample, above) and the follow-up SET after the never-sent generation (generation 3, COMM) show none; the stop-queued successor completes Stopping with an empty AMSG. Four controls fail their named checks (message always staged, never staged, staged for any unsent outcome, not reset in `Requests::admit`) while the unmodified products pass; all 21 D7 and AMSG controls PASS. 13 record cases on ordinary products (stop-queued and deadline-queue also on instrumented products), production CA (315 checks) and supervisor/IOC (25 checks) PASS. Report counters (2026-10-05): the `snmp3 queue:` line of `snmp3RuntimeReport` now ends with `behindRetirement`, `behindAdmitted` and `behindNeverSent`; the report of an actual record test process shows 0, 1, 1 in the below-threshold trial and 0, 1, 0 in the above-threshold trial, the sample trials and stop-queued; the check `queue-report-counters` reads that line and a defective copy that prints the admitted count in the never-sent slot (`report-never-sent-miscounted`) fails it, so all 22 D7, AMSG and report controls PASS (`<local>/r6-s8c-controls-20261005-084755/results.json`); component, supervisor/IOC, operator, 13 record cases and production CA PASS (`<local>/r6-s8c-regress-20261005-084755/`); the lifecycle runner PASS in `<local>/r6-s8c-regress-20261005-084755/lifecycle2/` (its first call in `lifecycle/` lacked the required `--base` argument and exited 2 at argument parsing). Deadline-queue measurements (2026-10-05): the case records per trial the put-to-dispatch interval (operator's put to the successor's dispatch) and reports the threshold as the minimum-to-maximum range over every relaunch it measured, sampling and budget trials alike (`deadline_threshold_all`); the run `<local>/r6-s8e-regress-20261005-093844/deadline-queue/` measured 370.6 to 392.0 ms on ordinary products over five relaunches (sample budget 1000 ms: put-to-dispatch 1370 to 1392 ms; above-threshold budget 783 ms: 1153 ms; below-threshold budget 185 ms: not dispatched) and `deadline-queue-san/` measured 714.6 to 740.1 ms on instrumented products (measured from containment to Ready: 371.4 to 393.2 ms and 715.7 to 741.4 ms; the instrumented put-to-dispatch interval was 1718 to 1732 ms at the 1000 ms budget); individual ordinary-product runs of the case measured 362.2 to 403.4 ms from containment to Ready (for example `<local>/r6-s8g-deadline-queue-20261005-093844/`) and the retained instrumented runs 710.3 to 745.3 ms (`<local>/r6-s8b-regress-20261005-081137/`, `<local>/r6-s8e-regress-20261005-093844/`, `<local>/r6-s8g-deadline-queue-san-20261005-093844/`, `<local>/r6-s8h-deadline-queue-san-20261005-093844/`); a worker stopped at the process-signal boundary took 1848 ms from containment to Ready in the qualification `deadline` case on ordinary products (`<local>/r6-d9-test_qualification-20261004-000357/deadline-4/`), and an earlier measurement recorded in item 8 gave about 1.83 s; the checks `threshold-over-every-relaunch` and `put-to-dispatch-within-late-application-bound` (a successor keeps the deadline of its own admission, so its dispatch follows the put by at most two budgets plus the callback delay) pass and also tie the reported range and interval to the raw per-trial fields, so a copy of the test that measures the interval from the first admission fails the second, and a copy that gives a queued generation a later deadline (`queued-deadline-extended`) fails the second one, so all 23 D7, AMSG, report and deadline controls PASS (`<local>/r6-s8e-controls-all-20261005-093844/results.json`); the six controls that run the deadline-queue case were re-run after the checks were tied to the raw fields and passed (`<local>/r6-s8g-controls-20261005-093844/results.json`). Two details of the AMSG change are not pinned by owner decision D11, and a nonzero `behindRetirement` in the printed line is not shown by owner decision D12. Evidence: `<local>/r6-s8b-controls-20261005-081137/results.json`, `<local>/r6-s8b-regress-20261005-081137/` (record cases), `<local>/r6-s8a-regress-20261005-005827/` (CA and supervisor/IOC; product sources identical). |
| T10 | 2026-10-01 | Actual Base callback queue, GET and SET | Pending; subset PASS | External callbacks fill the real queue; failed enqueue retains terminal/full reservation/PACT, retries and completes once without a second accepted entry. Full requirement review remains pending. |
| T11 | 2026-10-01; 2026-10-03; 2026-10-04; 2026-10-05 | Actual entered/queued callback shutdown | Pending; subset PASS | Held record lock makes an entered callback exceed the drain attempt; shutdown waits for its lease. A blocked Base queue leaves a queued callback retained after gate closure/detach. Expired drain refuses restart. Stop in flight, 2026-10-05, `stop-inflight` case (tests committed in `8d70e56`, product sources unchanged since `2544819`): all eleven record kinds are admitted on one unanswered address and the runtime is stopped once a batch is on the worker channel; one generation is sent and ten wait in the queue (`held_queued` 10, `pending_at_stop` 11). Each record completes once with a communication alarm and INVALID severity, no native publication, PACT 0 and waveform BUSY 0; completions and FLNK each rise by exactly 11, the drain succeeds, the state is Stopped, no retirement stays pending and the worker is reaped. Ordinary products 3 of 3 (`<local>/r6-t11b-stop-inflight-1` to `-3`) and ASan/UBSan products 1 of 1 (`<local>/r6-t11b-stop-inflight-asan`), after 6 ordinary and 3 instrumented runs of the same case before the review fixes (`<local>/r6-t11-stop-inflight-1` to `-6`, `<local>/r6-t11-stop-inflight-asan` and `-asan-2`, `-asan-3`). Two defective copies are detected on the instrumented build, each failing its named check while the unmodified products pass: a `Scheduler::stop` that does not select queued generations (`stop-inflight-every-record-completes-with-alarm`) and a waveform support that holds BUSY set (`stop-inflight-waveform-busy-clear`) (`<local>/r6-t11b-controls/results.json`). The case does not distinguish the Stopping outcome from other outcomes that give the same alarm; two further candidate controls were not detected and are not kept (a stop that does not select the sent generation, because the supervisor selects every active generation as Stopping when it loses the worker while stopping, and a BUSY edit at the admission site, because `prepare` clears BUSY at completion; `<local>/r6-t11-controls/stop-active-not-selected` and `<local>/r6-t11-controls-2/waveform-busy-held`). Every case run listed above printed `enqueueFailures=3` with 11 completions (callback queue size 8), and the unmodified reference run in `<local>/r6-t11b-controls` printed 2, so the count depends on timing; two or three completion enqueues were refused by the Base queue and all 11 completions were still counted once; the case does not assert the counter. Enqueue-failed completions at stop, 2026-10-05, `stop-enqueue-failed` case (tests committed in `17c494a`, product sources unchanged since `2544819`), three processes that differ only in when the queue is released: the low-priority Base callback queue (size 8) is held by an external blocker and eight fillers before the eleven records are admitted, so every completion enqueue is refused (`held_pending` 11, `held_queued` 0, at least eleven refusals) and the stop retries them. Released while the runtime thread still runs (`within`, stop 31 to 44 ms, shorter than the 2 s supervisor bound), or at 2.5 s after the stop, 0.5 s past the bound and inside the drain budget, when only the record drain retries (`late`), every record completes once with a communication alarm and one FLNK, the drain succeeds and the state is Stopped. Released after the drain budget (`after`), the drain fails, the stop returns before the release, the state is IncompleteStopped, restart is refused, the records keep PACT set, and the isolated cleanup finalizes the eleven completions once without FLNK. Ordinary products 2 of 2 on the final harness (`<local>/r6-t11c-enqueue-9`, `-10`) and 5 more on earlier harness versions (`-3`, `-5` to `-8`); ASan/UBSan products 1 of 1 (`<local>/r6-t11d-enqueue-asan-3`) and 2 more (`-1`, `-2`). A defective drain that does not retry its enqueues is detected through the `late` process only (`release-after-stop-bound-completes-every-record-once`, `<local>/r6-t11d-controls/results.json`, which also re-detects the two stop-inflight controls); its first version aimed at the `within` process was not detected because the runtime thread services completions while it runs (`<local>/r6-t11c-controls`). Observed stop durations with the queue held: 2.5 s for `late`, ending at the chosen release, and 4.0 s for `after`, the supervisor bound followed by the full record drain budget; Backlog M12 carries that observation for an owner decision. Downstream external link processing, 2026-10-05, `stop-downstream` case (tests committed in `b00fdec`, product sources unchanged since `2544819`), two processes: in the isolated harness every Timeout record's FLNK is a Channel Access link to a record in another lockset (`Records_StopExternal.PROC CA`, link connected, separate lockset), and the downstream record's lock is held past the drain budget. Base runs the link's put on its CA link thread and queues it from the completing record's callback, so the snmp3 completions are not held. With the lock held across the runtime stop (`stop`): the stop takes 31 to 41 ms on ordinary products and 43 ms on the instrumented product and ends Stopped with the drain successful and 11 completions while the lock is still held, every record completes once with a communication alarm and INVALID severity, the downstream value is 0 while held and 11 after the release (eleven processings, one per link; Base keeps one pending put per CA link, so a repeated FLNK would not show in this count, and once per record is shown by the completion count and the stop-inflight case). With the lock held through the IOC shutdown (`shutdown`): the snmp3 hooks AtShutdown to AfterStopCallback complete and the shutdown then waits for the lock in Base's CA link shutdown, 2.5 s until the release; the records read PACT 1 afterwards because Base's link closing sets it (`iocInit.c:700`), so PACT is not asserted in that mode, and the downstream value after the release was 1, 1, 0, 1 and 0 in five runs and is reported, not asserted. Reading Base's code, the value depends on whether the CA link thread is blocked on an attribute read (cleared at shutdown, giving 0) or on the first put (giving 1); that is a code reading, not a probe. Ordinary products 4 of 4 (`<local>/r6-t11e-downstream-1` to `-4`) and ASan/UBSan products 1 of 1 (`<local>/r6-t11e-asan-stop-downstream`), each 14 of 14 checks; `-4` ran the committed harness, and the other four ran it before the check was renamed (its old name was `downstream-processes-once-per-record-after-release`), with the same product, fixture and test binary. The stop-inflight and stop-enqueue-failed cases pass again on both products (`<local>/r6-t11e-stop-inflight-1`, `<local>/r6-t11e-enqueue-1`, `<local>/r6-t11e-asan-stop-inflight`, `<local>/r6-t11e-asan-stop-enqueue-failed`) and the three stop controls are still detected (`<local>/r6-t11e-controls/results.json`). No defective copy exists for this case because the product only calls the record's `process` and Base owns the forward link; a stop that waited for the downstream would fail `held-downstream-does-not-hold-the-stop`. With this case every cell D13 lists for T11 is executed. Products before D7 (`c065755`), 2026-10-03: with the worker stopped at the worker-signal boundary, a Deadline-selected predecessor stayed retirement-pending, but no successor was queued because the reprocess was rejected; drain succeeded and the worker was reaped about 1.5 s after stop (`<local>/r6-stop-queued-head-control-20261003-231614/`). A worker that is not reaped within the stop bound cannot be produced through the permitted boundaries, because a stopped process still exits on SIGKILL; by owner decision of 2026-10-04 that cell is no longer required (see Dependencies And Decisions). D7/D9 products (`0b60abf`), 2026-10-04: with a stopped, retirement-pending predecessor and a queued successor, `snmp3Stop` completed the successor as Stopping with FLNK once and PACT cleared, drain succeeded, and the predecessor was reaped about 1.5 s later (`<local>/r6-d9-stop-queued-20261004-000528/`). |
| T12 | 2026-10-01; 2026-10-05; 2026-10-07 | Actual isolated queue cleanup and production non-isolated IOC exit | Pending; subset PASS | Contexts survive queued ownership and are cleared only after actual isolated queue cleanup; abandoned terminal finalized once. Production IOC normal exit runs Base AtShutdown through AfterShutdown, verifies the actual worker PID is absent after reap, closes completion entry and reports eight retained inactive contexts without isolated queue destruction. As of 2026-10-05, in-flight non-isolated retention, record startup-failure and the repeated shutdown `del_record` cell were unexecuted (the last is described under the live-detach case below; its 2026-10-07 execution is recorded after this table). Isolated rebuild and reuse beyond the D7 clause, including delayed or abandoned queued completions, lsi/lso pointer-backed buffers and retained maximum-capacity payloads, is deferred to M11 by D13 (the `rebuild` case below retains only an ao record and its queued successor). Isolated rebuild after a stop that retained a retirement-pending and a queued generation, 2026-10-05 (case `rebuild`, sources of `2544819`): the first activation is stopped with a stopped worker, a retirement-pending predecessor and a queued Base reprocess and is cleaned up in isolation; the same process then loads the databases again and starts a second activation (number 2) on a new worker, the first worker is gone, the complete baseline passes on the second activation and the report counters start again at zero (ordinary and instrumented products: `<local>/r6-matrix-20261005-123214/record-rebuild/`, `<local>/r6-matrix-20261005-123214/san-record-rebuild/`); the control `rebuild-reuses-scheduler` fails the case checks while the unmodified products pass. The case does not assert that the configuration stays frozen across the rebuild; an expired drain blocking restart is covered by the shutdown case and RuntimeTest. Live link replacement, 2026-10-05, `live-detach` case (tests committed in `d333a17`, product sources unchanged since `2544819`), two processes: Base's own link put (`dbPutField` on `INP` of a `TimeoutAi` and `OUT` of a `TimeoutAo`) is attempted while the requests are in flight, after they complete, after the operator stop (`Runtime::stop`; admission and detach permission closed) and, in the second process, 2.5 s after the stop of a run whose completions the full Base callback queue refuses, which is 0.5 s into the record drain with two completions pending. Base refuses every attempt (its link put asks the device support's `del_record` first and stops when it refuses), each record keeps its context pointer, handle and binding, and both in-flight requests complete once with a communication alarm and INVALID severity. Ordinary products 4 of 4 on the final check set (`<local>/r6-t12h-live-1`, `-2`, `<local>/r6-t12i-live-1`, and the reviewer run `<local>/r6-p76-run`) and ASan/UBSan 1 of 1 on the product of that set (`<local>/r6-t12g-asan-1`; the later edits changed only controls, README and comments). Seventeen defective copies each fail their named check with the reference passing and no abort: thirteen through that check's one deciding term, and four through several (a detach allowed always fails four terms and stops the live process early, so its live event is not printed; a detach allowed when idle or after the stop fails two; a detach allowed while pending fails three) (`<local>/r6-t12b-controls`, `-t12c-controls`, `-t12d-controls`, `-t12e-controls`, `-t12f-controls`, `-t12h-controls`, `-t12i-controls`, `results.json`): a detach allowed always, when idle, while pending or after the stop; a refused detach that raises an alarm or changes the handle at each of the four moments; a stop reported with entry open or detach allowed; a drain always recorded as failed; a completion counted twice; a completion that leaves the record active; and a refusal that releases the record, with or without an alarm. Terms that no copy isolates are recorded in the check comment: three scenario preconditions (both phases ran, the stop still running at the attempt, two completions pending) and five `refused` terms that are redundant with their `intact` terms, because Base clears `dpvt` before `add_record` whenever it accepts a link put and the support never restores it. A copy that reports a drain as successful early was written, run and removed, because this case releases the queue inside the drain budget and the copy changes nothing observable (`<local>/r6-t12f-controls/results.json` keeps it, which is why its top-level `passed` is false); a reviewer probe on the current tree shows that the `after` process of the `stop-enqueue-failed` case, which releases after the budget, detects it (`<local>/r6-p74-probe/out/results.json`), and no shipped control carries it. Not executed as of 2026-10-05: a repeated `del_record` after the shutdown has detached the contexts. Base's `doCloseLinks` calls `del_record` once and leaves the device support set, and a later device-link put enters Base's link put, which calls `del_record` again (`dbAccess.c:1138-1162`); a test can issue that put from the hook after close links, so the cell is executable and stays open under D13. The control receipts ran older versions of the check script, which were only extended afterwards (`r6-t12b` to `-t12d` before the drain intact term, `-t12e` and `-t12f` and `-t12h` before the final comment, `-t12i` on the committed script); the detections hold on the committed checks, which a reviewer re-evaluated on the printed events. The control libraries are not bit-reproducible between builds; the mutated source hash is the stable identity. The repeated-detach and non-isolated outstanding-work results dated 2026-10-07 follow this table. |
| T13 | 2026-10-02; 2026-10-03; 2026-10-05 | Ordinary and separate ASan/UBSan products | Pending; subset PASS | Numeric matrix, current edges and production IOC/CA pass on ordinary and separate instrumented products; ordinary output-policy and full native-adapter regressions pass after the shared fixture extension. The new sanitizer build identifies 15 products, including the actual production IOC. Earlier record/pressure/blocked/queued-shutdown products, native-free loader checks and ordinary M1-M5 regressions pass for their recorded versions; component evidence includes 50166 conversion checks. Full targeted T2-T4/T9-T12 qualification is incomplete; D13 disposes of the remaining cells, and each executed cell runs on both the ordinary and the ASan/UBSan products as the T13 Method requires. D7 products (`98bfdfc`), 2026-10-03: ordinary baseline, edges, active, policy, alarms, shutdown, queued-shutdown and numeric record cases PASS (`<local>/r6-d7-regression-*-20261003-235137/`); the instrumented rebuild and runs are in the 2026-10-05 matrix re-run below. Matrix re-run 2026-10-05 on the sources of `2544819` (clean working tree) in `<local>/r6-matrix-20261005-123214/`: ordinary products: foundation regressions (independence, lifecycle, config, native API 84 checks and adapter 779 checks), components (15 executions, 212 Scheduler checks), qualification (28 cases), supervisor/IOC (25 checks), operator (15 checks), the 14 record cases (baseline 11, edges 17, alarms 18, active 21, policy 24, numeric 20, shutdown 12, queued-shutdown 12, stop-queued 14, active-unforced 21, accounting 19, deadline-queue 23, near-deadline 19, rebuild 16 checks) and production CA (315 checks) all PASS; separate ASan/UBSan products (`<local>/r6-matrix-20261005-123214/sanitizer-build/`, leak detection disabled): components, qualification (28), supervisor/IOC (25), the same 14 record cases and production CA (315) all PASS with no sanitizer diagnostic in the retained stderr and logs. The cells recorded as unexecuted in T1 to T12 are not covered by this run. |
| T14 | 2026-10-01; 2026-10-04; 2026-10-05; 2026-10-07 | Separately built defective support copies; unchanged record/worker/native spans | Pending; subset PASS | Seven actual controls detected: communication-alarm classification, integer precision, text capacity, binary32 tie selection, ambient-mode dependence, callback retry and terminal release. Defective library load and named assertion failure are observed; the stale-generation control is unexecuted and is run under D13: first check whether the D7 frame-validation controls recorded under M5 T7 already cover it; if they do, cite that receipt as the executed control, if not, build the control. 2026-10-04, `bd4dc0c` sources, sanitizer build `<local>/r6-p010-sanitizers-20261004-152351/`: the D7 record controls were also detected, each failing its named check while that check passes on the unmodified products: lookups keyed by binding only and a stop that finds only one generation per binding fail `queued-successor-completes-stopping-once` of `stop-queued` (in both, PACT of the queued successor stayed set, FLNK ran once and the drain failed), and a copy that restarts a queued deadline at dispatch fails `below-threshold-queued-generation-not-sent` of `deadline-queue`; the unmodified `deadline-queue` run still fails the unrelated `below-threshold-never-sent-message` until the AMSG work is done (done 2026-10-05; see T9). `<local>/r6-p010-controls-20261004-191025/results.json`; all fourteen D7 controls re-run with the harness of `75abe64`, the three record controls failing the same named checks: `<local>/r6-p026-controls-20261004-235719/results.json`; all seventeen with the later harness, the record controls again failing the same checks: `<local>/r6-d9excl-controls-20261005-003623/results.json`. An earlier run (`<local>/r6-d7-controls-all-20261004-115736/`) had stopped the runtime before the Base reprocess ran and rejected the unmodified `stop-queued` reference; the cell now waits for the reprocess (`docs/snmp-worker-supervision.md`) and its unmodified run passes (`<local>/r6-p010-stop-queued-20261004-162208/results.json`). Re-run 2026-10-05 on the sanitizer build of `2544819` (`<local>/r6-matrix-20261005-123214/sanitizer-build/`): the seven controls above detected again (`<local>/r6-matrix-20261005-123214/controls-older/results.json`, complete and passed) and all 24 D7, AMSG, report, deadline and restart controls PASS (`<local>/r6-matrix-20261005-123214/controls-d7/results.json`, complete and passed), each failing its named cell or check while the unmodified products pass. The repeated-detach and non-isolated outstanding-work results dated 2026-10-07 follow this table. |

Repeated detach qualification, 2026-10-07, T12/T14, local Linux x86_64 / Base 7.0.10: `repeat-detach` runs actual idle isolated shutdown with the shipped ai/ao `record-stop.db` fixtures and unchanged product sources on HEAD `d333a1794cffbbb4b1270599d90c36c072880c32`. Fresh ordinary and ASan/UBSan products each pass 55 runner checks, including 46 case-specific checks (`<local>/r6-repeat-20261007/ordinary-1/results.json`, `asan-1/results.json` under the same root). Both record pointers detach at the first AfterCloseLinks observation; all four real INP/OUT puts return `S_dev_badInpType` (33685511), preserve full original INST_IO text/type and dset, and leave dpvt/context record pointers null. All 23 contexts remain through every attempt and BeforeFree; after actual `testdbCleanup`, contexts are zero and Runtime is Stopped. The ordered lifecycle, four attempts, BeforeFree and post-cleanup events are mandatory. Ordinary and instrumented `live-detach` and `rebuild` regressions also pass (14 and 16 checks per product, respectively), in the same root's `live-detach-ordinary`, `live-detach-asan`, `rebuild-ordinary` and `rebuild-asan` directories.

T14 repeated-detach controls, 2026-10-07: all 45 separately compiled defective support copies fail their specified named checks against passing references. Each reference requires that exact check to be present once and pass. Each defective driver exits normally with ordered events, real cleanup, no runner abort, timeout, forced cleanup, sanitizer diagnostic or credential sentinel, and a loaded-library hash matching its actual defective support. `<local>/r6-repeat-20261007/control-executions.json` records commands and outcomes; each `control-<name>/results.json` records its detection, and `term-map.json` maps all 46 checks to executed controls or scenario preconditions. The AfterStopCallback and BeforeFree release controls each fail only `repeat-detach-contexts-retained-before-free`. Both empty-dpvt acceptance controls retain the usual first-put error while changing link and dset, proving that error status alone is insufficient. Context-record faults also disable the declared cleanup assertion that would abort on the injected pointer; actual destruction remains intact and no test repairs state. The exact 45-control families and scope are in `tests/rewrite/README.md`, Repeated Detach Controls.

T14 external cleanup qualification, 2026-10-07: repeat-detach controls aggregate forced cleanup across all child receipts and require the external agent and proxy normal-stop checks to pass. Three controls were rebuilt and rerun with this condition (`repeat-release-after-stop-callback`, `repeat-ao-2-context-record-null`, `repeat-ai-empty-accepted`); all qualify in `<local>/r6-repeat-20261007/cleanup-fix-controls/results.json`. In separate actual outer-boundary executions, an agent stopped with SIGSTOP required the real runner's timeout/SIGKILL cleanup, and an owned UDP proxy killed with SIGKILL exited abnormally without runner-forced cleanup. Both controls were rejected with exit 1 and `passed: false`; the aggregate forced-cleanup values were true and false respectively. Receipts: `cleanup-fix-agent-stop/execution.json` and `cleanup-fix-proxy-exit/execution.json` under the same root. Record drivers exited normally and each intended BeforeFree check failed. No internal path was substituted. The earlier 45 controls were audited, not all rerun after this guard change: all 270 reference/control child receipts show normal exits, reaping and no forced cleanup, so those recorded detections remain valid.

Build and provenance: `<local>/r6-repeat-20261007/ordinary-build.json`, `ordinary-products-2.json` and the matching build stdout/stderr identify root and test Make executions and the final ordinary driver; `sanitizers-1/sanitizer-build.json` records all 15 fresh instrumented products. Each run records source, product and loaded-library hashes; `verification-summary.json` indexes the six positive runs and 45 controls. The separate `observation-validation.json` records 143 unit-validation cases using retained real-path observations with missing, duplicated or malformed fields; it is not another IOC integration run. Product code is unchanged; module/test products are instrumented, dependencies remain uninstrumented and leak detection is disabled. This result closes only the idle isolated repeated-detach cell, not in-flight non-isolated retention, startup failure, other DTYP/support attachment, every record type separately, production CA during shutdown, T12 as a whole or M6. The separate second-person recheck of the historical live-detach result paragraph passed on 2026-10-07.

T12 non-isolated outstanding-work supplement, 2026-10-07: the production `inflight` and companion `retained` cases each passed 3 of 3 ordinary fresh-process trials (`<local>/nonisolated-v2-1/`, `-2/`, `-3/`) and 1 of 1 ASan/UBSan trial (`<local>/nonisolated-v2-asan/`), with 26 and 46 runner checks per respective case. Actual CA puts admitted passive ai Integer GET and ao OpaqueFloat SET through the shipped DB. The production Main/registrar case exited with two active requests pending at exit, then reported Stopped, two retained inactive contexts, two completions, closed entry and an identified/reaped worker. The companion used the unchanged Main and ordinary Base lifecycle. Two successful Result/Retired pairs preceded exit while two Base callbacks remained queued and both generations charged/unconsumed. Real drain expiry closed entry; AfterCloseLinks observed null dpvt and context record pointers before releasing the external hold. AfterStopCallback and AfterShutdown observed two Inert active contexts, no publication/FLNK/completion, unchanged generation reservations, a live callback queue system and IncompleteStopped. Both contexts retained non-null, sent, identity-matched successful terminals from queued through restart; null record/context associations persisted from AfterCloseLinks through restart, and the Inert count stayed two from AfterStopCallback through restart. A real restart attempt was refused without new activation/thread/worker. Fallback was checked separately. All child receipts were normal, with no forced cleanup, watchdog, sanitizer diagnostic or credential sentinel. The unchanged isolated `queued-shutdown` runner passed 12 checks per product set before this Python assertion correction (`<local>/nonisolated-reg-isolated/`, `<local>/nonisolated-reg-iso-asan/`), and the unchanged production CA runner passed 315 checks per set (`<local>/nonisolated-reg-ca/`, `<local>/nonisolated-reg-ca-asan/`); these four regressions were not rerun after the assertion-only correction.

T14 non-isolated controls, 2026-10-07: all six compiled defective support copies completed actual shutdown and failed their named checks against the passing instrumented reference (`<local>/nonisolated-v2-controls/results.json`, complete and passed). The two premature-release controls fail their respective context-count checks at AfterStopCallback and AfterShutdown; omitted dpvt clear and context-record clear each fail only their separate pointer check; bypassed closed entry fails the Inert count check; omitted drainFailed recording fails the expiry flag check. Additional failures are combined observations, not individual-term discrimination. Intended defective library hashes and normal child cleanup were observed. These fresh executions use the final qualification predicate, including worker reap and actual pending/native/queued/wire preconditions, and the corrected 46-check retained runner. Earlier executions under `<local>/nonisolated-controls/` and the later predicate-only audit `<local>/nonisolated-execution/control-guard-audit.json` remain historical evidence; that audit is not counted as an additional integration run. The shipped test README contains the complete term map, including observations without independent controls.

Non-isolated provenance and limits: `<local>/nonisolated-build-final/` retains root/test Make and sanitizer build command receipts/stdout/stderr; `<local>/nonisolated-san/sanitizer-build.json` records 16 fresh instrumented products, including the companion built from unchanged production Main. `<local>/nonisolated-execution-v2/` holds exact corrected matrix/control command outcomes, the current source/evidence index, eight passing input/cleanup receipt audits and 68 passing assertion-validation checks derived from the new actual retained observations. The latter comprise one unchanged reference and 67 wrong-but-well-typed terminal, pointer or Inert-value changes, all rejected by the required check. `<local>/nonisolated-execution/` retains the initial 26/45-check matrix, control outcomes and 429 passing schema/parser checks; that matrix preceded the correction that checks terminal and detached-state preservation after callbacks execute. Each case retains raw phase/transport observations, worker PID/start time, CA stimulus/exit times and complete reports; parsing probes are not new IOC runs. Source and product hashes identify each executed version. Only Python assertions and their documentation changed after the final builds and four regressions; compiled sources/products and regression drivers stayed unchanged. Product sources are unchanged. Module/native/test products are instrumented; installed Base, CA and system/vendor dependencies are not, and leak detection is disabled. Companion worker stderr is captured; the production case retains the production stderr policy. Expected retained allocations do not establish leak freedom. The bounded ai/ao in-flight non-isolated executions passed; startup failure, all record kinds individually, other D13 cells and complete T12/M6 acceptance remain open. Independent third-person implementation recheck and the separate second-person changed-document review passed on 2026-10-07, with the post-callback assertion gap corrected and no remaining in-scope findings.

Supplemental component observation, 2026-10-01: `Conversion.{h,cpp}` and `Request.h` provide checked scalar/text conversion, software binary64-to-binary32 roundTiesToEven, record-link grammar and immutable record-binding metadata. The actual `snmp3ConversionTest` links the production support library and passed 50166 assertions in both ordinary and separate ASan/UBSan products. Checks include explicit binary32 tie/subnormal/signed-zero/overflow bit fixtures, all four ambient rounding modes with floating-point state checks, hardware-rounding comparison, integer precision/range rejection, text capacity/termination and link grammar. Initial receipts: `<local>/r6-conversion-20261001/results.json` and `<local>/r6-conversion-sanitizers-20261001/conversion-results.json`. The current products repeat the component checks through `<local>/r6-components-alarms-20261001/results.json` and `<local>/r6-components-alarms-asan-20261001/results.json` (15 executions each). This is component evidence; record closure uses the distinct real-path observations above. Dependencies are uninstrumented and leak checks are disabled.

Selected record receipts, executed 2026-10-01 and 2026-10-02 on local Linux x86_64 / Base 7.0.10, each qualifying its recorded source/product version:

| Execution | Ordinary receipt | ASan/UBSan receipt | Actual custom assertions |
| --- | --- | --- | --- |
| Eleven records, conversion, order, simulation, GET/SET callback pressure and maximum payload queue | `<local>/r6-final-edges-20261001/results.json` | `<local>/r6-final-asan-edges-20261001/results.json` | 1263 per product |
| Eleven native timeouts and a separate record deadline | `<local>/r6-final-alarms-20261001/results.json` | `<local>/r6-final-asan-alarms-20261001/results.json` | 358 per product |
| Five active output/RPRO paths, explicit retry, native retry and lost SET responses | `<local>/r6-active-readback-20261001/results.json` | `<local>/r6-active-readback-asan-20261001/results.json` | 443 ordinary; 444 instrumented |
| Output IVOA/OOPT, simulation/SDLY, ao OVAL/drive and lso OMSL/DOL | `<local>/r6-policy-dol-20261001/results.json` | `<local>/r6-policy-asan-20261001/results.json` | 1340 per product |
| Production IOC/CA decimal, CHAR-array strings, PINI/FLNK and normal non-isolated exit | `<local>/r6-ca-final-20261001/results.json` | `<local>/r6-ca-final-asan-20261001/results.json` | 119 runner checks per product; separate CA executables |
| Entered callback blocked on record lock during shutdown | `<local>/r6-final-shutdown-20261001/results.json` | `<local>/r6-final-asan-shutdown-20261001/results.json` | 213 per product |
| Queued callback retained through gate closure and actual cleanup | `<local>/r6-final-queued-shutdown-20261001/results.json` | `<local>/r6-final-asan-queued-shutdown-20261001/results.json` | 213 per product |
| Seven real-path defective-code controls | none; separate products only | `<local>/r6-final-controls-20261001/results.json` | Seven named failures detected; no timeout/forced-kill qualification |
| All numeric GET/SET pairs, range/precision/extrema and rejected-output wire preservation | `<local>/r6-numeric-commit-20261002/results.json` | `<local>/r6-numeric-commit-asan-20261002/results.json` | 13155 per product |
| Current edges after numeric fixture extension | `<local>/r6-numeric-regression-edges-20261002/results.json` | `<local>/r6-numeric-regression-asan-edges-20261002/results.json` | 1263 per product |
| Current production IOC/CA after numeric fixture extension | `<local>/r6-numeric-regression-ca-20261002/results.json` | `<local>/r6-numeric-regression-asan-ca-20261002/results.json` | 119 runner checks per product |

The record receipts retain executed source/product/library hashes, activation/revision/handle/generation/admission, alarms/UDF/publication state and actual child/cleanup outcomes. The instrumented build is identified by `<local>/r6-alarms-sanitizers-20261001/sanitizer-build.json`; the alarm-stage record-test rebuild is `<local>/r6-deadline-asan-rebuild-20261001/snmp3RecordTest.build.json`, the active-output rebuild is `<local>/r6-active-readback-asan-rebuild-20261001/snmp3RecordTest.build.json`, and the output-policy rebuild is `<local>/r6-policy-asan-rebuild-20261001/snmp3RecordTest.build.json`. Earlier receipts do not qualify subsequent fixture changes. No sanitizer diagnostic or credential sentinel appeared on the executed positive paths. Each defective control records its actual compilation/mutation and loaded defective library rather than substituting an internal span. Full T1-T14 closure remains pending, and `snmp3Report` still reports `recordSupport=unavailable`.

Active output qualification, 2026-10-01: the actual external UDP proxy delays responses by 750 ms. An external callback holds the unchanged Base callback consumer until the accepted SET and a following GET have both retired natively. This qualifies the RPRO path with admission available: FIFO GET observes the first captured payload, RPRO admits the latest prepared value, and a separate GET observes that value. Each of the five outputs runs Base FLNK twice. The response-loss cases drop actual successful SET responses and distinguish operator retry from native-library retry; their explicit GET observations do not add automatic module readback. The retained-native-ownership admission-rejection path is replaced under D7 by admission behind retirement, which the T7 row records as observed (2026-10-03/04).

Output policy qualification, 2026-10-01 at approximately 22:29 PDT: actual Base 7.0.10 processing, native timeout terminals and external callback gating exercise all five output kinds. Completion IVOA and simulation preserve the current requested VAL/LEN, retain COMM/INVALID and finalize terminal/FLNK once; live unsupported OOPT reports LINK/INVALID even when Base conditional_write skips DSET. First-pass Don't drive outputs admits no SET; Continue normally and Set output to IVOV use the prepared value, verified by separate GET after response loss. Pre-existing Base HIHI/UDF INVALID alarms retain their equal-severity priority. Both synchronous and SDLY-driven simulation before admission leave module identity/completion counts unchanged. Actual ao drive/rate limits prepare VAL=5 and successive OVAL/wire values 1 and 2. lso supervisory ignores DOL, closed_loop transfers 200 data bytes, and a 300-byte source is reduced by Base to 255 data bytes/LEN=256 before DSET capture; GET verifies that prepared value. This last observation qualifies the Base boundary, not rejection of the original DOL length. Ordinary and ASan/UBSan cases each pass 1340 assertions; actual proxy metadata counts 38 SETs. Initial fixture failures are retained and do not qualify these results.

Production CA qualification, 2026-10-01: `test_record_ca.py` launches the shipped production Main, generated IOC registrar, unchanged Base CA server, actual worker/native products and real loopback agent. The selected Base caget/caput execute as separate clients with private CA ports and an owned/reaped repeater. Decimal CA STRING preserves 9007199254740993 and 9223372036854775807 through int64out SET and int64in GET; UINT64 waveform returns 18446744073709551615 exactly. lso/lsi VAL$ CHAR arrays preserve 0/39/40/200/255 data bytes and LEN including NUL; ordinary CA STRING exposes 39 data bytes. caput rejects a 300-byte VAL$ write with Invalid element count before put, and subsequent native GET observes the previous 255-byte value. This client rejection differs from the verified Base DOL truncation above. PINI input and its Base FLNK run after servicing is ready; normal non-isolated IOC exit reaches AfterShutdown. Actual /proc observations identify the owned worker PID/start time before exit and require that PID to be absent afterward; final module reports show 19 completions, a closed entry gate, zero active/queued/running requests and eight retained inactive contexts. Ordinary/instrumented receipts each pass 119 runner checks. The instrumented production IOC build is `<local>/r6-ca-asan-ioc-build-20261001/snmp3Ioc.build.json`, using production Main and its actual generated registrar; the sanitizer builder now declares this product alongside the record/component products. Base, caget/caput and system/vendor dependencies are uninstrumented; leaks are disabled. Long active client writes over Channel Access are deferred to M11 by D13, and in-flight non-isolated teardown is run under D13; neither is qualified yet.

Native timeout alarm qualification, 2026-10-01: actual response dropping produced READ/INVALID in the pre-correction ai path (`<local>/r6-alarm-before-20261001-run3/results.json`). Current ordinary and instrumented alarm cases observe NativeFailure/native code 2 before completion for all eleven record kinds, followed by COMM/INVALID, without native publication. The separate 1 ms record deadline produces IPC Deadline and COMM/INVALID. The communication-alarm negative control reinstates the defective classification in an actual compiled support library and fails the named ai alarm assertion normally. Session-open/send/cancellation alarm branches are implemented according to the accepted mapping but are not qualified by this timeout case.

Current ordinary foundation/config/lifecycle regressions: `<local>/r6-foundation-alarms-20261001/results.json`. Worker/IOC/operator regressions: `<local>/r6-worker-alarms-20261001/results.json`, `<local>/r6-supervisor-alarms-20261001/results.json` and `<local>/r6-operator-alarms-20261001/results.json`. Native API and adapter regressions: `<local>/r6-native-api-alarms-20261001/results.json` and `<local>/r6-native-adapter-alarms-20261001/results.json`. Current ordinary/instrumented component receipts are `<local>/r6-components-alarms-20261001/results.json` and `<local>/r6-components-alarms-asan-20261001/results.json`, with 15 executions and 50166 conversion assertions per product. They pass for their recorded source/product identities; unavailable wrong-owner qualification retains its explicit NOT RUN. Earlier failed initial/pressure/edge/foundation/alarm runs remain retained under `work/` and do not qualify any row.

Numeric qualification, 2026-10-02: ordinary and separate ASan/UBSan cases each pass 13155 custom assertions and 20 runner checks. The selected receipts above qualify the current shipped numeric DB; their source/product hashes match the current fixture and products. The shipped numeric DB contributes 148 contexts; the unchanged baseline contributes 12, for 160 actual initialized contexts. Every advertised numeric GET pair receives a successful exact input through actual output DSET/worker/native SET and subsequent GET; range/precision rejection is then observed on the same initialized inputs. Fixed actual agent OIDs additionally provide unsigned Counter64 2^63/UINT64_MAX and opaque NaN/infinities. The 60 fixed input pairs are initial-input checks; they do not claim failure after an earlier native success. The matrix records 121 conversion failures after successful input, 68 accepted output cases, 67 rejected outputs and 67 stimulus SETs. Actual agent observations contain 141 SET actions: those 135 admitted numeric SETs plus six baseline/pressure SETs. Rejected outputs leave request identity/completion counts and separately read native value unchanged. Waveform rejection preserves complete BPTR bytes and NORD, while UDF follows Base processing and native-success state remains unchanged. The complete new sanitizer build is `<local>/r6-numeric-sanitizers-20261002/sanitizer-build.json`; all 15 declared products build successfully with the builder's instrumentation checks, including its documented InventoryTest UBSan exception. New code is instrumented, dependencies remain uninstrumented and leaks are disabled. Current ordinary and instrumented edges each pass 1263 assertions; production IOC/CA each passes 119 runner checks. The ordinary policy regression is `<local>/r6-numeric-regression-policy-20261002/results.json` (1340 assertions), and the complete ordinary native-adapter regression is `<local>/r6-numeric-regression-native-20261002/results.json` (779 checks). Every receipt qualifies its identified sources/products only. The cells still unexecuted are listed in the Results rows and disposed of by D13: executed in the order it gives, or deferred to M11. Record support is not advertised.

###### Periodic SCAN And Active PROC Results

Observed At: 2026-10-08. Remaining Verification Order item 1, bounded T1/T7 route coverage, PASS on ordinary and separate ASan/UBSan products. The shipped `scan-proc` case drives all eleven record types through actual Base periodic scan and `dbPutField(PROC)`, with 22 record/route observations and 44 matching native requests/responses. Both periodic generations observe active scan attempts without RPRO; two active PROC writes retain the first identity and coalesce into one successor. Each route completes exactly two generations and two Base FLNK executions with NO_ALARM and cleared PACT/RPRO. Normal isolated shutdown leaves zero contexts. Actual output SETs establish valid agent values before the input routes.

Author receipts: `<local>/scan-proc-ordinary-2/results.json` (55 checks, SHA256 `9852c3450c00e2fea36741b64f019e56d3089661e1b9f92789bc81dc37f1c1e5`) and `<local>/scan-proc-asan/results.json` (55 checks, SHA256 `d65836e91e65f326302e9545062b5d060b04d6fcab967ba10576c62eca64353a`). Build identity: `<local>/remaining-sanitizers-2/sanitizer-build.json`, 17 separately instrumented products. Existing ordinary and instrumented baseline (11 checks each) and active (21 checks each) regressions PASS in `<local>/scan-proc-baseline`, `<local>/scan-proc-baseline-asan`, `<local>/scan-proc-active` and `<local>/scan-proc-active-asan`.

Independent technical review 1: PASS, no must-fix or minor findings. Fresh ordinary and ASan/UBSan executions each pass 55 checks in `<local>/scan-proc-review-r1/{ordinary,asan}`. The additional actual wire, agent, generation/handle continuity, source/product/library and cleanup audit passes 297 checks in `<local>/scan-proc-review-r1/audit.json`. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_205728_subagent_gpt6_thread_plan_r1_scan-proc.md`, SHA256 `c5900cdd30082c08841334eef4f874036b13627e3154f61275187e0cf0cb334d`. Separate changed-document reader review 2 subsequently passed without findings.

The initial `<local>/scan-proc-ordinary-1` run failed on the agent's initial embedded-NUL octets and is excluded; final executions use real SETs before the lsi GET. Sanitizers cover rebuilt products, not installed Base/system/vendor dependencies; leak detection is disabled. No new compiled negative control ran for this Base-owned route behavior. These results close the periodic/PROC cells only, not all T1/T7 requirements or M6. M11 remains deferred. Item 1 is accepted within its bounded periodic/PROC scope; proceed to item 2, native failure after a valid GET and alarms, under the existing authorization.

Separate changed-document reader review 2: PASS, no must-fix or minor findings. The incoming-test-maintainer pass covered all changed README passages, the canonical verification order, item-1 results and entry point; 20 document/CLI/retained-receipt checks pass in `<local>/scan-proc-review-r1/reader-check.json`. No new runtime qualification ran. Both item-1 implementation review passes are complete. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_210044_subagent_gpt6_thread_plan_r1_on_rev20261008_205728.md`, SHA256 `42026dec51ab8d89ede9f3b792a4d0f37186f6c9cd6062b5b7b06344f289a351`.

###### Failure After A Valid GET Results

Observed At: 2026-10-08. Remaining Verification Order item 2, bounded T2/T5 alarm-flow coverage, PASS on ordinary and separate ASan/UBSan products. Each of six input record types completes a successful GET before each of eight actual outer-UDP response faults: errorStatus/index, tooBig, invalid errorIndex, three exception tags, incompatible type and response loss. All 48 combinations preserve previous storage/UDF/LEN/NORD, retain native-success history without publishing failed input, complete Base FLNK once and deliver the expected READ/INVALID or COMM/INVALID through real DBE_ALARM subscriptions. Actual terminal metadata distinguishes native Complete exception values from NativeFailure. Additional ai runs verify competing MAJOR limit and UDF alarms do not hide native INVALID, followed by successful recovery. The full case observes 105 native requests/responses, 50 fault terminals and normal isolated cleanup.

Author ordinary `<local>/alarm-flow-ordinary-2/results.json` (SHA256 `2202f6c9da406c7be5af889b7708a5d8d792a7d8652dc6eea6fb7f181ad4105e`) and instrumented `<local>/alarm-flow-asan/results.json` (SHA256 `4d3c4e21ac0ab8e11f58b2830864c1791513946bf0d825fa202cf3174ebd31ba`) each PASS110. Separate product identity: `<local>/remaining-sanitizers-3/sanitizer-build.json`, 17 successful rebuilt products. Ordinary and instrumented baseline/alarms/policy regressions respectively PASS11/18/24 in `<local>/alarm-flow-{baseline,alarms,policy}-{regression,asan}`. Shared UDP adapter regression: `<local>/alarm-flow-adapter-regression/results.json`, PASS779.

Independent technical review 1: PASS, no findings. Fresh ordinary execution PASS110 with 1,551 driver checks in `<local>/alarm-flow-review-r1/ordinary`; source/product, record identity/admission, request/response/terminal, cleanup and retained author execution audit PASS228 in `<local>/alarm-flow-review-r1/audit.json`. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_211349_subagent_gpt6_thread_plan_r1_alarm-flow.md`, SHA256 `fbf0f85d29adfb8deea55fe48403884b18822cfe94905fcce3284afdb4aa8a72`. Reviewer sanitizer evidence is audited retained author execution, not a new reviewer run. Separate changed-document reader review 2 subsequently passed without findings.

The first author trial is retained and excluded: the Python predicate used the wrong IPC NativeFailure number; the corrected predicate uses Ipc.h value 6 and the case was executed again. Sanitizers cover rebuilt products, not installed Base/system/vendor dependencies; leaks are disabled. No compiled negative control ran for this supplement. This result does not close all T2/T5 or M6: queued-deadline AMSG monitor transitions and values read by FLNK targets remain required later cells. M11 is unchanged.

Separate changed-document reader review 2: PASS, no must-fix or minor findings. The incoming-test-maintainer pass covered every alarm-flow README addition, the canonical result and current entry point, with 25 passing document/CLI/retained-receipt checks in `<local>/alarm-flow-review-r1/reader-check.json`. No new runtime qualification ran. Both review passes for this bounded addition are complete. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_211657_subagent_gpt6_thread_plan_r1_on_rev20261008_211349.md`, SHA256 `0350f736155a45e1be4bb47ddee8a0b4c1c1ae6d26ba0bf9017f5c65365193cf`.

Item 2 is accepted within this bounded T2/T5 scope. Next: item 3, remaining record chains on success, failure and deadline, fanout to independent addresses, and delayed or duplicate external responses, under the existing authorization.

###### Record Chain And Fanout Results

Observed At: 2026-10-08. Remaining Verification Order item 3, bounded T6 chain/fanout coverage, PASS on ordinary and separate ASan/UBSan products. The shipped `chain-flow` case runs eleven fresh IOC processes, one per source type. Each source completes four actual A-FLNK-observer-FLNK-B chains: successful native response, errorStatus failure, duplicate response and record deadline after response loss. The native ai B uses the independent IPv6 agent. All 44 observations show source PACT=1 at the Base-owned FLNK, final alarm/UDF visible in the calc observer, exactly two module completions and one final target FLNK. lsi/lso local VAL$ links supply all 200 data bytes and LEN=201 to the real Soft Channel lsi. Waveform BUSY is set while the actual terminal callback waits on the record lock; Base subsequently completes and BUSY is false. This is behavioral evidence through the unchanged processing path, not direct rset-entry instrumentation.

Each IOC also runs two actual fanout trials with independent IPv4/IPv6 addresses: response delays 750/100 ms, then 100/750 ms. All 22 observations show the fanout FLNK completing while both native targets remain active, the fast target finishing while the slow one is active, and exactly one completion/FLNK per target. Four proxy traces retain actual request/response identities and delay intervals. The invocation observes 110 source-side requests/responses including 11 duplicate forwards, 44 native chain-target GETs and 44 fanout GETs. Normal isolated shutdown and external fixture cleanup complete.

Author ordinary `<local>/chain-flow-ordinary-3/results.json` (SHA256 `06ed970fbd80ecaab7cfc15be387ede903f451183ea81fa8534647bd10364ca3`) and instrumented `<local>/chain-flow-asan/results.json` (SHA256 `2d151825551bdebb809caacb9c8cb8bb8de507ea470090e4f4f8bf65566d1345`) each PASS104. Separate build: `<local>/remaining-sanitizers-5/sanitizer-build.json`, 17 successful products. Shared-driver regressions `<local>/chain-{baseline,edges,alarm-flow}{,-asan}` respectively PASS11, PASS17 and PASS110 on each product set; shared native UDP adapter `<local>/chain-adapter/results.json` PASS779.

Two initial author trials are retained and excluded. `<local>/chain-flow-ordinary-1` mixed successive deadline trials in one IOC and observed worker restart delaying the next request; the final case uses a fresh IOC per source and places the deadline last. `<local>/chain-flow-ordinary-2` exposed the Soft Channel observer's plain VAL short-string view; the final shipped fixture uses the real local VAL$ link. No product source changed. ASan/UBSan covers rebuilt products, with installed Base/system/vendor dependencies uninstrumented and leaks disabled. No new compiled negative control ran. Broader T5/T6 clause reconciliation, T14 discrimination, M6 closure and all M11 deferred groups remain outside this result. The timeout chain here is record Deadline before native timeout, not a separate native-timeout chain; duplicate responses are immediate copies, not delayed old-generation controls. Preserve these distinctions during final clause reconciliation. Separate changed-document reader review 2: PASS, zero findings; document, CLI and retained-receipt checks PASS29 (`<local>/chain-review-r1/reader-check.json`), with no new IOC, build or control execution. Both required review passes for the bounded item 3 chain/fanout addition are complete. Item 3 is accepted within the scope and limits recorded here; next is item 4, queued output SET SIMM changes on normal and deadline paths. The delayed output-simulation mode change remains in M11. This acceptance does not complete all T6 clauses or M6. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_214219_subagent_gpt6_thread_plan_r1_on_rev20261008_213641.md`, SHA256 `c7bb3d118c6e4960d93fcce06700931e9ee05f947ec14d1c3ee90e39afc357e9`.

Independent technical review 1: PASS, no must-fix or minor findings. Fresh ordinary execution `<local>/chain-review-r1/ordinary` PASS104; independent raw agent/wire/record/provenance audit `<local>/chain-review-r1/audit.json` PASS334. The audit independently identifies all 44 native chain-target GETs by excluding actual fanout request IDs from the IPv6 agent trace and correlates fast/slow delivery with record completion observations. Retained author sanitizer/regression evidence was audited; the reviewer did not rerun ASan. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_213641_subagent_gpt6_thread_plan_r1_chain-flow.md`, SHA256 `191fb89e4952912f8ccd356749c44fd0f1928a72892ae00b0678d716bba2ed11`.

###### Queued Output SIMM Results

Observed At: 2026-10-08. Remaining Verification Order item 4, bounded T8 and output T5 coverage: final ordinary and separate ASan/UBSan executions PASS63 each. Ten fresh IOC processes cover five output kinds on normal and deadline paths. Every SIMM change occurs with generation 2 active, one queued request, one pending retirement and two charged generations. Exact identities, terminal/sent status, two source completions and Base FLNK executions, preserved requested VAL/LEN, cleared PACT/RPRO and released terminal ownership are required. Ten separate native GETs verify latest values on normal paths and first values on deadline paths. Normal isolated shutdown and external fixture cleanup complete.

On the normal path, the runner verifies the actual worker parent and executable, then holds its genuine first Retired header at the Linux x86_64 sendto syscall entry using ptrace. No frame, register, result or internal function is replaced. The driver observes the real successful terminal, lets Base complete and admit RPRO, changes SIMM while the successor is queued, then releases the original send. Both captured SETs reach the agent. Each trace/record pair requires exact epoch/batch and ordered hold, observed boundary, SIMM change, detach/release and actual Retired timestamps. These controlled boundaries do not measure the unheld retirement interval; active-unforced retains that separate purpose.

On the deadline path, actual worker SIGSTOP follows the first wire SET; SIGCONT follows the never-sent successor Deadline. Only the first SET reaches the agent. Real DBE_ALARM metadata observes empty-message COMM/INVALID followed by COMM/INVALID with `deadline before send` in all five cases. Driver scope guards release either boundary on failure, and the ptrace helper detaches on error. A missed boundary, denied ptrace, helper error, forced cleanup or incomplete inventory is non-passing.

Qualified author receipts: `<local>/queued-simm-ordinary-7/results.json`, SHA256 `7badf00e1ce6fc85ab9a17cffee00c51a483cea0bd808fcbe158737d257594bd`; `<local>/queued-simm-asan-4/results.json`, SHA256 `462cbc5703fe3a8531058d65bc914011cd77e337cba842194ef14bacaee85589`. Build `<local>/remaining-sanitizers-9/sanitizer-build.json` identifies 17 successful separate products. Existing active-unforced/deadline-queue/alarm-flow cases PASS21/23/110 respectively on ordinary products (`<local>/queued-simm-final-{active,deadline,alarms}`) and final instrumented products (`<local>/queued-simm-trace-{active,deadline,alarms}-asan`). The ordinary regressions preceded the target-only ptrace addition; the instrumented regressions use the final driver.

Initial/intermediate attempts are retained and excluded from final qualification. `<local>/queued-simm-asan`, `<local>/queued-simm-ordinary-4`, `<local>/queued-simm-asan-3` and independent `<local>/queued-simm-review-r1/ordinary-2` missed a required queued/change-before-Retired window; early field-address lookup and a signal sent after observing completion were insufficient timing controls. `<local>/queued-simm-ordinary-5` records actual sandbox EPERM and failing runner checks; it is not skipped coverage. Final ptrace runs use normal runtime approval. Earlier passing iterations are not substituted for the final five-file implementation and strengthened trace checks.

Sanitizers cover rebuilt products, not installed Base/system/vendor dependencies, and leaks are disabled. No product behavior changed and no compiled negative control ran for this addition. Long active client writes and delayed output-simulation mode changes remain in M11. Queued input AMSG monitoring and final T5/T8 clause reconciliation remain required. Independent technical review 1 and separate second-person review 2 both PASS without findings. This execution result does not close M6.

Independent technical review 1: PASS, zero current must-fix/minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_221516_subagent_gpt6_thread_plan_r1_queued-simm.md`, SHA256 `ad4345dfe89f66de9fdf7235798ad87da2dc67d9584a20d8b3def6feeaacfdef`. The reviewer specified an independent ordinary command, which the Facilitator executed through approved runtime escalation; `<local>/queued-simm-review-r1/ordinary-4` PASS63. Independent raw evidence audit PASS358 (`<local>/queued-simm-review-r1/audit-v4.json`); this count is not additional IOC executions. A separate actual baseline run with an inherited missing-marker request exited nonpassing with only the helper check failed, while the IOC and external fixtures exited normally (`<local>/queued-simm-review-r1/helper-error`). Successful boundary release, pre-attach EPERM and missing-marker error propagation are executed evidence; post-attach timeout, detach failure and exception-driven C++ release were source-reviewed only. Separate second-person review 2: PASS, zero findings; `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_222207_subagent_gpt6_thread_plan_r1_on_rev20261008_221516.md`, SHA256 `8d426e02bd44f2e557ba2d3cfc236d345057918eae88102321c367469362e3db`. Item 4 is accepted within the recorded bounded T8/output T5 scope; queued input AMSG monitoring, final clause reconciliation and the remaining M6 sequence stay open. Next: item 5 under auth20261008_204828.


###### Text And Structured Boundary Results

Observed At: 2026-10-08. Remaining Verification Order item 5, remaining T3/T4 structured/text boundaries: ordinary and separate ASan/UBSan `boundaries` executions PASS16 each. Seven fixture instances independently vary octet Binding capacity, SIZV and NELM; OID waveform Binding and NELM; and IPv4 NELM. The 1,050 input observations include octet lengths 0/1/14/15/16/17/38/39/40/41/254/255/256/257/32765/32766/32767, embedded NUL/high bytes, non-NUL high bytes, 6/7/8-arc OIDs, OID text lengths 39/40 and zero/all-255 IPv4. Octet/OID capacity failures follow actual valid seed GETs and preserve complete previous storage, LEN/NORD and native-success history with Base-specific UDF behavior. The three-element IPv4 array is initial-failure coverage because no four-byte IPv4 value can fit it. Accepted text and arrays are checked byte-for-byte with exact lengths and unchanged waveform tails.

All 157 output observations use real stringout/lso processing and separate full-capacity native GET readback. There are 120 accepted record SETs and 37 rejected captures; rejection preserves requested storage/LEN, has WRITE/INVALID, creates no admission/completion and leaves the agent's seed value intact. Invalid captures include unterminated strings, LEN=0, inconsistent LEN and independent Binding overflow. Oversized idle Base dbPutField VAL$ writes truncate to SIZV-1 before DSET; the resulting complete prefix is accepted or rejected according to Binding capacity. The real synchronous dbPutField return is checked as zero for admission and -1 for local DSET rejection. LEN greater than allocated storage is not injected into Base.

Preparatory public Runtime SETs establish actual binary/OID/IPv4 agent values; they are native stimulus operations, not output-DSET coverage. Exact external totals are 1,207 GETs and 627 SETs (507 stimuli plus 120 record SETs), with 627 agent commits. The unchanged Base/DSET/Scheduler/worker/native path performs every tested input and output. Normal IOC/agent/proxy cleanup and native-free IOC loading are required. No internal substitute is used.

Qualified receipts: `<local>/boundaries-ordinary-2/results.json`, SHA256 `60f9948f5b576da7a4db33222fa2500861b8ef5cec85a3f341c3d259b1a66ce1`; `<local>/boundaries-asan/results.json`, SHA256 `4d49cf15a0eeb803283627b0d2b1738de7d3fa4d5bc2a5ef8b843976dbd8a959`. Build `<local>/remaining-sanitizers-11/sanitizer-build.json` identifies 17 successful separate products. Existing numeric and edges cases PASS20/17 on both ordinary (`<local>/boundaries-numeric`, `<local>/boundaries-edges`) and instrumented (`<local>/boundaries-numeric-asan`, `<local>/boundaries-edges-asan`) products. These retain numeric GET/SET and binary32 coverage on the final driver; the preceding alarm-flow supplement supplies native failures after valid numeric input.

The initial `<local>/boundaries-ordinary` attempt is nonpassing and excluded: its generic field-put helper rejected the expected synchronous -1 for an oversized client value whose Base-truncated prefix still exceeds Binding capacity. Final qualification uses the explicit accepted/rejected return check. No product behavior changed. Instrumentation covers rebuilt products, not installed Base/system/vendor libraries, and leak checks remain disabled. Deferred CA variants, long active client writes and other M11 groups are unchanged. Independent technical review 1 PASS with zero findings: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_223421_subagent_gpt6_thread_plan_r1_boundaries.md`, SHA256 `64fec2961c29f6038f7ec8c7a1c78fc20b5225c2cc20ec3ca719093bdf47f93f`. Reviewer execution `<local>/boundaries-review-r1/ordinary` PASS16; its independent complete transaction/commit audit PASS55 (`<local>/boundaries-review-r1/audit.json`, SHA256 `0d2d169bd3a1c0108c364801c21d4ae865f332b0e49f4cc800913bad3819a031`). Audit checks are evidence analysis, not additional IOC executions. Separate second-person review 2 PASS without findings: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_223651_subagent_gpt6_thread_plan_r1_on_rev20261008_223421.md`, SHA256 `5cabd8858181c317d8dcf5b32923ff6eb8b7745a5e08cc25ae455922de5bf1d2`. Both required review passes for this bounded addition are complete; item 5 is accepted within the recorded scope and limits. Next: item 6 under auth20261008_204828. Full T2-T4 clause reconciliation and M6 closure remain pending item 7; M11 exclusions are unchanged.

###### Input Retirement And Retry Results

Observed At: 2026-10-08. Remaining Verification Order item 6, bounded T9 and input T5 coverage: ordinary and separate ASan/UBSan `retirement-flow` executions PASS101 each. Twelve fresh IOC processes cover six input record kinds on normal and deadline paths, each after a valid native GET. An active PROC request becomes one Base RPRO successor, queued behind the first generation's pending retirement. Both generations remain fully charged: 2 x 5,192 bytes for ai/longin/int64in/waveform, 2 x 14,912 for stringin and 2 x 17,408 for lsi in both executions. A real GET at the independent IPv6 address completes with -123 before the held IPv4 boundary is released.

Normal trials hold the actual worker's genuine Retired send at the external ptrace boundary after seed operations, without modifying any frame or register. Both source GETs then succeed. Deadline trials drop the first actual GET response and hold that worker with SIGSTOP; the successor expires unsent at its own 300 ms budget. After both Base completions, the still-held predecessor retains one count and its full byte charge. SIGCONT permits genuine Retired or exact reap to release it; these executions accepted genuine Retired before reap. The driver waits for settlement and a new Ready worker before explicit retry. Every deadline trial observes a real DBE_ALARM event for COMM/INVALID with empty AMSG, followed by a second event with `deadline before send` at the same alarm level. Both executions observe one event of each kind per deadline trial.

Every trial checks exact successor terminal identity, full previous input storage and LEN/NORD, native-success history and UDF, cleared PACT/RPRO/BUSY and released terminal ownership. Explicit same-handle GET retry succeeds, clears AMSG and produces the third source FLNK. Totals per execution are 60 preparatory SETs, 42 source GETs, 12 independent-address GETs and 120 module completions. Six queued deadline generations produce no native request. These are actual Base/DSET/Runtime/Scheduler/worker/native/agent paths; no internal span is replaced.

Qualified receipts: `<local>/retirement-ordinary-1/results.json`, SHA256 `7703c7a00f8923462d7e2be63ccbe65cbfc630f20cbf82a5033dd66843f77f9e`; `<local>/retirement-asan-1/results.json`, SHA256 `942271aeaebeb99d0ab5ec23346ae89947d4a49459a5b3250e03604f9f3647eb`. Separate build `<local>/remaining-sanitizers-12/sanitizer-build.json` identifies 17 products. Shared-runner ordinary regressions PASS11 baseline (`<local>/retirement-baseline/results.json`) and PASS63 queued SIMM (`<local>/retirement-queued-regression/results.json`). All twelve IOC receipts return normally; no forced cleanup, sanitizer diagnostic or secret sentinel qualifies as success.

This controlled external hold does not measure the natural unheld retirement interval. The existing output deadline-queue and accounting cases supply the measured below/above-Ready budgets and count-limit variants; this addition does not replace them. No new compiled negative control ran. Rebuilt module products are instrumented; installed Base and system/vendor dependencies remain uninstrumented and leaks disabled. Technical review 1 identified one evidence-language defect: charge release had been incorrectly described as waiting for reap. The corrected wording above distinguishes retained charges during the hold from accepted Retired before reap. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_225259_subagent_gpt6_thread_plan_r1_retirement-flow.md`, SHA256 `e97e0e4acc301cdb8a05ab994ab331ac53b4e6fe775bc1bad1cbbe01788071eb`. Independently requested ordinary execution `<local>/retirement-review-r1/ordinary/results.json` PASS101; raw-receipt audit PASS330 (`<local>/retirement-review-r1/audit-v2.json`, SHA256 `7ebc81918bce3ea3d30c4dcca9470a5c7c302a8da198112bb9076fc44fe26fa8`). Audit checks are evidence analysis, not extra IOC executions. Technical correction recheck 2 PASS resolves F-retirement-001; the documented release condition matches accepted Retired before reap. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_225548_subagent_gpt6_thread_plan_r1_on_rev20261008_225259.md`, SHA256 `538538be6b756483f8cf56f190d17f9a64ff511562db549b605f1cce4055c3bc`. No runtime source changed or new IOC ran for this documentation correction. Separate second-person review 3 PASS with zero findings: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_225735_subagent_gpt6_thread_plan_r1_on_fup20261008_225548.md`, SHA256 `8f4a60cfddedb9df48956d2e1ea3631b6ad8c762c09650a189efcdb278f63031`. Both required review passes are complete and item 6 is accepted within this bounded T9/input T5 scope. Next: item 7 under auth20261008_204828. Full T1-T12 clause reconciliation, T14 control adequacy and final T13 regressions remain item 7; M6 stays In progress and M11 exclusions are unchanged.

###### Initialization, Successful Input Simulation And Live LINR Results

Observed At: 2026-10-08. Remaining Verification Order item 7, bounded T1/T8 clauses: ordinary and separate ASan/UBSan `contract` executions PASS21 each. The actual IOC rejects 47 unusable configurations without contexts and initializes 17 valid contexts. Eleven DSET kinds receive independent handles; lsi/lso default SIZV=41 and frozen capacity, refused live SIZV writes, deadline endpoints 1/600000 ms, and waveform initial BUSY clearing are checked. The external UDP trace contains no initialization I/O and exactly 26 requests/responses (17 GET and nine SET), matching module completions.

All six input records consume a genuine successful terminal while Base selects distinguishable SIOL values, without marking native publication or native success. A subsequent native GET recovers actual data; complete values, LEN/NORD, UDF, BUSY, PACT and three FLNK executions per input are checked. Actual idle and active dbPutField(LINR) for ai/ao exercise local rejection, successful native completion with LINK/INVALID after live validation fails, terminal release and restoration. The process-passive idle write returns -1 after preserving SLOPE; the active write requests Base RPRO without a second native admission. Restoring NO CONVERSION itself starts the recovery request.

- `<local>/contract-ordinary-2/results.json`: SHA256 `ef6a12541c49ff418e6cfec7eb916dece6e26303ecd366c115ce761368c80e96`.
- `<local>/contract-asan-contract/results.json`: SHA256 `31e25657a4af800901d7c876870e6b03c0b6c64abe11099d21a54e0bcd7fd25c`.

Fresh instrumented products: `<local>/remaining-sanitizers-13/sanitizer-build.json` (17 declared products). Current-source ordinary and instrumented baseline/edges/policy regressions pass 11/17/24 runner checks: `<local>/contract-regression-{baseline,edges,policy}/results.json` and `<local>/contract-asan-{baseline,edges,policy}/results.json`. Each receipt includes source, executable and loaded-library identities. Base/system/vendor libraries remain uninstrumented; leak detection is disabled.

Limits: budget extrema qualify initialization only; Base-refused SIZV changes do not emulate an internal buffer replacement. The first ordinary attempt failed because the initial test assumed that dbPutField(LINR) never processes; it is retained in `<local>/contract-ordinary-1` and supplies no passing qualification. The corrected test runs Base's real process-passive path. Production source is unchanged. The bounded contract supplement is accepted after technical review 1, F-contract-001 placement correction recheck 2 (PASS), and separate reader review 3 (PASS without findings). Independent ordinary execution PASS21/456 assertions and raw audit PASS83 (`<local>/contract-review-r1/audit-v2.json`, SHA256 `08a995a84c01f0bb2c2fa798e1607badcbb7f5993406e711823012e1b07102f8`). Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_231325_subagent_gpt6_thread_plan_r1_contract.md`, SHA256 `88013f3a6bf4a7b185ef226d445793bc17d2ee962da1cc6da331c72a2e50a8e7`. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_231537_subagent_gpt6_thread_plan_r1_on_rev20261008_231325.md`, SHA256 `e263ca6b2c7b4fdbd3c4538fff5253ea15e08dcde29c2cd5cf4928d016abc870`. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_232006_subagent_gpt6_thread_plan_r1_on_fup20261008_231537.md`, SHA256 `b4dcf30cd5cf72fea9cb34828dde5edce7a11b7afa591de076de74e2b064db83`. These reviews do not close final T13, T14, M11 or M6. Next: missing T14 record-path generation control and final T13 qualification under item 7.

###### Record-Path Stale-Generation Control Results

Observed At: 2026-10-08. Remaining Verification Order item 7, bounded T14 generation discrimination: ordinary and separate ASan/UBSan `stale-record` executions PASS15 runner checks and 52 C++ assertions each. Existing D7 `storage-validation`/`take-without-identity` controls exercise Scheduler components; `ipc-stale-behind` exercises the real owner/native path but no record/DSET. They therefore do not alone supply T14's record-path discrimination. The new test uses the actual ai record, Base PROC/RPRO/FLNK, Requests, Scheduler, IPC, genuine worker and native GETs. Production code is unchanged.

After a seed GET, an external UNIX socket forwarder holds the genuine Retired for generation 2 while generation 3 queues behind it. A copy of that real frame changes only the generation from 2 to 1 (byte offset 87), preserving binding/admission and header identities. A stale copy of the real Result follows with the next sequence; actual Supervisor event 16 confirms parsing beyond the forged Retired. Normal products preserve count 2 and 10,384 charged bytes, one retirement-pending predecessor and one queued successor, without an extra terminal/completion. The original Retired is then forwarded and generation 3 completes once. Three GETs, three completions and three FLNK executions include the seed. The helper remains connected through normal Runtime stop, exits without signals and is reaped; isolated Base cleanup leaves zero contexts/charges.

The separately compiled `stale-generation` control removes only generation comparison from Scheduler matching while preserving binding/admission checks. It loads the defective library and returns 1 at the named actual record assertion `stale generation retired the current record request` after 41 checks. Helper/agent/proxy cleanup succeeds normally; no timeout, abort, forced cleanup or sanitizer termination supplies detection. Both unmodified product executions pass the same path. The control library and record driver are disposable products, not replacements for ordinary products.

- `<local>/stale-record-ordinary-2/results.json`: SHA256 `9bc4655524169264da375fe97713714d0bb9b3798a98c8abb61c84013bd49c41`.
- `<local>/stale-record-asan/results.json`: SHA256 `ceb0b3128b758be3afd9990226fe0aa8deea5bdb5e49463eeac604bd92d98ad5`.
- `<local>/stale-record-control-2/results.json`: SHA256 `9053d52fb5bb056c332e96171b68eb784aa355c2b2aac1ae079610700db01456`.

Build: `<local>/remaining-sanitizers-14/sanitizer-build.json`, 17 declared products. Shared external helper regressions `ipc-stale` and `ipc-stale-behind` pass on ordinary (`<local>/stale-record-ipc-regression/results.json`) and instrumented (`<local>/stale-record-ipc-asan/results.json`) products; ordinary `contract` regression PASS21 (`<local>/stale-record-contract-regression/results.json`). Receipts identify sources/products/loaded libraries. Base/system/vendor dependencies are uninstrumented; leak detection is disabled. This is one real ai retirement-generation discrimination case, not every stale-frame or activation combination.

Unqualified setup attempts are retained separately: `<local>/stale-record-ordinary-1` aborted before IOC launch because the runner lacked its sys import; the first control invocation failed at Python module load because an edit duplicated the new tuple into unrelated control dictionaries. The corrected control source has one new entry in CONTROLS only; no defective product ran in that first attempt. These failures are not qualification evidence. The bounded supplement is accepted after technical review 1 (`rev20261008_233154`) and separate reader review 2 (`fup20261008_233410`), both PASS without findings. Reports: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_233154_subagent_gpt6_thread_plan_r1_stale-record.md`, SHA256 `8beafb610a70d9ce613200a219fe88ee36cc3d84fd6f357ba78aa3ed7f35ea39`; `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_233410_subagent_gpt6_thread_plan_r1_on_rev20261008_233154.md`, SHA256 `cd0fd31877d9e4154c2b2efea62a043d8e99a8665027c426e12037780dbbbbdc`. Reviewer ordinary execution PASS15/52 and raw audit PASS88 are retained in `<local>/stale-record-review-r1/`; audit SHA256 `b5eac8664f00ec39db9a9bfc92067653957175487fd39f1643c202d864ffc3da`. Final T1-T12 reconciliation and T13 qualification remain required; M11 exclusions and M6 In progress are unchanged.

###### Fixed Numeric Failure After Success Results

Observed At: 2026-10-08. Item 7 T2 reconciliation identified that the earlier 60 fixed high-Counter64/nonfinite pairs qualified initial input only. The shipped numeric case now sends each pair through a real successful GET of exact value 7 and then the original fixed agent response. Only the external UDP response varbind is changed for the seed; native parsing, worker, IPC, Scheduler, conversion and Base records remain real. Original and seed bytes and request IDs are retained. Product sources are unchanged.

All 60 seeds succeed. Five fixed high-Counter64 destinations accept the exact large value; 55 lossy/nonfinite destinations reject and preserve the prior bytes/NORD/native-success state under the existing Base UDF checks. Together with the earlier mutable matrix this produces 176 failures after successful input. Ordinary and separate ASan/UBSan numeric executions each PASS25 runner checks and 14,019 C++ assertions, with 959 GETs, 141 SETs and 1,100 module completions; all proxy requests have actual agent responses. No internal binding, record buffer or conversion is substituted to establish successful input.

- `<local>/numeric-retained-ordinary/results.json`: PASS25, SHA256 `b547f78609ab63d950de0751426710424d20ec4f1db56324efa593073305ddb2`.
- `<local>/numeric-retained-asan/results.json`: PASS25, SHA256 `c24155cff43d310f0569db09bc3b788904e063d0e61069404a6fe70cf20d049a`.

Build: `<local>/remaining-sanitizers-15/sanitizer-build.json`, 17 products. Native/module/test code is instrumented; Base/system/vendor dependencies remain uninstrumented and leaks disabled. These results cover IOC database access; M11 CA exclusions are unchanged. Technical review 1 and separate reader review 2 both PASS without findings; this bounded supplement is accepted. Reports: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_234251_subagent_gpt6_thread_plan_r1_numeric-retained.md`, SHA256 `8c9c6ad0bd637a94b2ab98006ece735369949eec860ec0e5c03dbb13243c032e`; `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261008_234742_subagent_gpt6_thread_plan_r1_on_rev20261008_234251.md`, SHA256 `fd6b0e245aa8d3bddaed33fd8e98465509c42202cfb7aa80b55d6c9671e3b5b8`. Independent numeric PASS25/14,019 and alarm-flow PASS110; fresh raw audit PASS438 in `<local>/numeric-retained-review-r1/audit.json`, SHA256 `f4f77ab9031ce89e05b0f614b616bcebdfa25bbd2a1dd4c39aefc55e8930d49c`. Final T13 and required-clause reconciliation remain open; M6 is In progress.

###### Explicit IOC Integer Access Results

Observed At: 2026-10-08. Item 7 T2/T4 reconciliation distinguished direct record-storage inspection from the required IOC DBR access. The numeric case now additionally executes dbGetField with DBR_INT64 for int64in/INT64 waveform and DBR_UINT64 for UINT64 waveform. Numeric int64out requests use real dbPut(DBR_INT64) under the record lock, then the existing explicit dbProcess path. This verifies database conversion/access without claiming dbPutField process-passive or CA behavior.

Ordinary and separate ASan/UBSan numeric executions each PASS26 runner checks and 14,659 C++ assertions. Each contains 268 exact DBR reads and 104 DBR writes, including INT64_MIN/MAX, INT32/UINT32 boundaries, 2^53+1 and unsigned UINT64_MAX reads. The existing exact native readback and invalid-SET checks still run: 959 actual GETs, 141 SETs, 1,100 completions, 176 failure-after-success inputs and zero final contexts/reservations. Product sources and wire traffic counts are unchanged.

- `<local>/numeric-access-ordinary/results.json`: PASS26, SHA256 `58ae2466e4e55ee7ab2f366e953492534b4c2f06734161369ff8cf3713982255`.
- `<local>/numeric-access-asan/results.json`: PASS26, SHA256 `57705037a9500d7b159397693e9ec4ec388081809ba9155628c4b359d1b055fb`.

Build: `<local>/remaining-sanitizers-16/sanitizer-build.json`, 17 products. Existing module/native/test instrumentation and uninstrumented Base/vendor/system boundaries apply; leaks remain disabled. The preceding full ordinary qualification `<local>/final-m6-ordinary/commands.json` passed all 38 commands before this test-only extension. At this supplement checkpoint, the final record-test binary was awaiting qualification; unchanged foundation/worker/production-IOC binaries retain their identified receipts. The bounded explicit IOC integer access supplement is accepted after technical review 1 and separate reader review 2, both PASS without findings. The current T13/reconciliation outcome is recorded below; M6 remains In progress. No M11 or product-policy change is included.

Technical report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261008_235457_subagent_gpt6_thread_plan_r1_numeric-access.md`, SHA256 `dbd6e701d201f3648c5800d69eaea7c557b331d405752fed3111bd3baa9797aa`; separate reader report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261009_000053_subagent_gpt6_thread_plan_r1_on_rev20261008_235457.md`, SHA256 `08f26ba59027c2dedca9745f128df19d9454b1343688304757fbb8a34296cc02`. Independent ordinary execution PASS26/14,659; raw audit PASS107 at `<local>/numeric-access-review-r1/audit.json`, SHA256 `ce4239b5907b37e4b485a87266732f4d3af6330aa35aa12a1ba8444faf8371b8`. The reader pass audited retained observations and actual CLI help; it did not run a new IOC or accept the separate final batch.

###### Final Required-Clause Supplement Results

Observed At: 2026-10-08 (Pacific). The requirement audit identified three remaining real-path gaps: int64out drive clipping, direct callback Pending/Queued/Running observations across the original deadline, and binary64 values immediately below/above positive and negative binary32 halfway points. The shipped final-clauses case closes these bounded execution gaps without product changes. Ordinary and separate ASan/UBSan runs each pass 14 runner checks and 396 C++ assertions.

The two callback records retain one charged 5,192-byte generation through failed enqueue, accepted queue entry and running callback held by the real record lock. A genuine successful terminal remains unchanged after the original admission deadline, then completes with one Base FLNK and NO_ALARM. This is observable selected-outcome preservation and absence of native replay; no private Scheduler deadline is exposed after selection. Three real DBR_INT64 writes clip at exact Base drive limits above 2^53 and match separate native GETs. Sixteen adjacent-rounding trials cover four values and four ambient modes. Exactly 20 GETs, 20 SETs and 40 module completions occur, followed by normal isolated cleanup.

- `<local>/final-clauses-ordinary/results.json`: PASS14, SHA256 `549b93a10acdfc34a71c1f1da709460c898b9814c4f2507b70fa85d39181edd4`.
- `<local>/final-clauses-asan/results.json`: PASS14, SHA256 `5989b4bf7da39b8c9ec19205843ace287cdb4a6fe03e94ed7f9445183de1b5f3`.

Build: `<local>/remaining-sanitizers-17/sanitizer-build.json`, 17 products. Module/native/test code is instrumented; installed Base/system/vendor dependencies are uninstrumented and leaks disabled. Independent technical and separate reader review are tracked by the current results below. All thirty cases on the final record-test product have now run in both modes; earlier matrices retain their historical product identities. M11 and the existing dated limitations are unchanged.

###### Current Required Verification Results (2026-10-08)

Observed At: 2026-10-08 (Pacific). This section reconciles the accepted T1-T14 methods and D13 with the actual executions below. Earlier dated result rows and supplement narratives remain historical evidence for their identified products; their former Pending or unexecuted descriptions do not describe the current required matrix. The four M11 groups and existing D11/D12/T11 decisions remain unchanged. No additional waiver is introduced.

Record evidence selection: all 30 shipped cases pass on ordinary and separate ASan/UBSan products. Ordinary cases through chain-flow use `<local>/final-m6-records-v2-ordinary/<case-directory>/results.json`; queued-simm, boundaries, retirement-flow, contract, stale-record and final-clauses use `<local>/final-m6-records-v3-ordinary/<case-directory>/results.json`. Every ASan record case uses `<local>/final-m6-records-v2-asan/<case-directory>/results.json`. A case directory is the CLI case prefixed with `records-`. These selections are enumerated with exact commands, receipt hashes and return codes in the respective commands.json files and the final audit inventory. The numeric case is included in both thirty-case selections.

| Label | Current result | Required clauses and actual evidence |
| --- | --- | --- |
| T1 | PASS within required scope | baseline, edges and contract initialize all eleven DSET kinds, independent handles and frozen bindings; reject grammar/operation/class/FTVL/OOPT/LINR/I/O Intr errors without initialization traffic; inspect default/minimum/maximum/clamped SIZV and waveform BUSY. scan-proc executes periodic and active PROC routes for all eleven kinds. Existing production CA covers PINI/passive processing. live-detach, repeat-detach, live-dtype and support-transition execute actual idle/active/draining/detach link and DTYP operations and shutdown. The direct DTYP field write can succeed; the request path detects the mismatch. The accepted live-DTYP/other-support supplements cover their stated ai/ao pairs and modes, not an invented all-type concurrent-edit matrix. |
| T2 | PASS within required scope | numeric executes all 68 advertised numeric GET pairs, integer/float extrema and precision boundaries, fixed high Counter64/nonfinite values after real successful seed GETs, 176 failures after good input, exact signed/unsigned IOC DBR access and complete integer comparisons. alarm-flow supplies six input kinds times eight native/protocol/type faults after a valid GET. Current production CA checks exact decimal DBR_STRING reads including 2^53+1, INT64_MAX and UINT64_MAX. The additional signed-minimum CA group remains M11; its general coverage is not inferred from existing selected CA cells. |
| T3 | PASS within required scope | edges and boundaries exercise empty, embedded-NUL, high-byte, exact/over-capacity octets; OID arcs/text and IPv4; independent Binding/SIZV/NELM; maximum 32,766-byte accepted payloads; previous bytes/NORD/LEN preservation and untouched array tails. Maximum-size IOC evidence is actual record storage inspection. chain-flow reads 200-byte lsi/lso through real local VAL$ links. Production CA reads 0/39/40/200/255-byte values through VAL$ CHAR access and contrasts the short DBR_STRING view. Maximum-capacity CA remains M11; no maximum-size DB API read is claimed. |
| T4 | PASS within required scope | numeric and edges execute all five output kinds and all twenty numeric SET pairs, target range/fractional/precision rejection, exact IOC DBR_INT64, independent native readback, finite OpaqueDouble and binary32 D5 behavior. edges covers exact halfway/even neighbors, subnormal/normal/signed-zero/overflow and four ambient modes; final-clauses adds the immediately adjacent positive/negative binary64 values in all four modes. boundaries checks empty/long/exact/over-capacity text, termination and LEN, actual Base oversized writes and no invalid SET. policy checks ao drive/rate/OVAL, all IVOA branches and lso DOL; final-clauses directly verifies int64out drive clipping above 2^53. Current CA covers int64out decimal and stringout/lso access. M11 ao/longout and signed-minimum CA variants remain excluded. |
| T5 | PASS within required scope | alarms, numeric, alarm-flow, policy and contract cover initial and post-success failures, local admission/conversion errors, native errorStatus/errorIndex/exceptions/timeout, competing ai alarms, actual DBE_ALARM observations and recovery. Base-specific UDF and lsi LEN behavior remain distinguished from native-publication history. queued-simm and retirement-flow observe output/input never-sent Deadline with COMM/INVALID and a new deadline-before-send message event after the previous COMM/INVALID. D11's unpinned unsent WorkerFailure/message-reset location remains the accepted limitation. |
| T6 | PASS within required scope | edges and chain-flow execute Base-owned FLNK ordering, all eleven source kinds on success/protocol failure/record deadline/duplicate response, complete 200-byte lsi/lso data at the next target, and independent-address fanout with reversed delays. There are 44 source chains and 22 fanout observations. PACT is true at FLNK and clears after completion; waveform BUSY behavior is observed before release of the held real record lock and after completion. This is real processing-path behavior, not substituted rset-entry instrumentation. |
| T7 | PASS within required scope | active and current production CA cover all five outputs, immutable first payload, later requested value, Base RPRO/latest request, ordinary and put-callback writes, explicit same-value retry, configured native retry and ambiguous response loss. scan-proc distinguishes active SCAN from active PROC across all eleven kinds. active-unforced reports the actually exercised retirement branch and Result-to-Retired intervals per trial, without substituting a hold. The measured results appear below. Long active client writes remain M11. |
| T8 | PASS within required scope | policy covers all longout OOPT settings, all five output IVOA branches, delayed-completion simulation and lso OMSL/DOL/IVOV. edges, policy and contract cover all six input kinds with distinct SIOL/native values, pre-admission simulation, successful and failing delayed GET, return to normal, ai RAW and actual SDLY callbacks. queued-simm executes all five output kinds with an already queued second generation on normal and deadline paths. It verifies source selection, Base-owned value/UDF changes, native-publication history, exact terminal release, FLNK/PACT and waveform BUSY. M11 delayed output-simulation mode-change scope remains excluded. |
| T9 | PASS within required scope | edges/maxPayloadQueue and accounting check actual count/byte exhaustion, maximum lsi/lso module storage, limit reduction/increase and full retained charges. active, deadline-queue and retirement-flow execute same-handle admission behind retirement, exact two-generation accounting, explicit recovery and independent IPv6 progress. deadline-queue records three initial fresh-process threshold samples and both budget trials; all five measured relaunches define the retained range and each put-to-dispatch interval. Below-threshold work expires unsent; above-threshold work dispatches after reap/Ready. D12's unpinned nonzero printed behindRetirement state remains explicit. |
| T10 | PASS | baseline pressure uses actual Base queue saturation for GET/SET and checks enqueue retry, reservation retention and exactly one completion; its agent trace has thirteen GET handlers and six committed SETs, matching nineteen module completions without native replay. final-clauses additionally observes Pending, Queued and Running under external callback and real record-lock holds. The actual successful terminal remains owned with the same full identity after its original admission deadline, then completes once. The observation proves selected-outcome preservation/no replay; it does not expose the private stored deadline after selection. Requests::service source independently confirms callback retry performs neither a new admission nor a deadline write. |
| T11 | PASS within dated scope | shutdown, queued-shutdown, stop-inflight, stop-enqueue-failed, stop-downstream and stop-queued cover dispatched/borrowed/queued/enqueue-failed/running completions, real record-lock and downstream blocking, waveform BUSY/FLNK, closed entry, unsuccessful expired drain, entered-callback safety wait and actual worker reap. D7's queued successor completes Stopping once while predecessor retirement remains pending. The impossible-to-construct outside-stop-bound reap variant remains excluded by the 2026-10-04 decision. No total stop-duration guarantee is claimed; M12 is separate. |
| T12 | PASS within required scope | rebuild executes the accepted D7 isolated stop/cleanup/new-activation path; queued-shutdown, shutdown, live-detach, repeat-detach, live-dtype and support-transition observe phase-aware detach, retained callback storage, actual queue destruction and exact native retirement/reap. Existing production and companion non-isolated cases each rerun on ordinary/ASan products. All ten record-bearing startup modes rerun on both: each of the preflight and thread-failure groups has one normal reference and four failure modes; thread failures use the actual kernel syscall boundary. No installed Base modification or internal stand-in supplies failure. Remaining broader isolated reuse/delayed/abandoned/long-buffer variants stay M11. |
| T13 | PASS for the executed current-product regression scope | All thirty record cases pass per mode. Unchanged non-record products retain freshly executed foundation/lifecycle/independence, native API/adapter, components, worker qualification, supervisor, operator, production CA, non-isolated and startup receipts. Build 17 contains seventeen identified products; only RecordTest changed from build 16. The final audit checks actual product/source/library identities and child outcomes. The inherited M3 differently-owned secret-file fixture is explicitly NOT RUN; it is not silently counted as a pass. Instrumentation limits are stated below. |
| T14 | PASS within required scope | Eight separately compiled default defects are actually detected: stale generation, communication-alarm classification, precision, capacity, binary32 tie, ambient mode, callback retry and terminal release. Three fresh D7 record controls detect binding-only lookup, queued deadline restart and stop selecting only one generation. Their unchanged references pass. Every control has actual defective-library identity and its named failure; the deadline-restart control fails the runner assertion after normal IOC exits. Earlier accepted repeated-detach, startup, live-DTYP and support-transfer controls remain separately identified evidence for those supplements. No claim of rerunning every historical control is made. |

Actual environment: Debian 13 x86_64, installed EPICS Base 7.0.10 at `~/gitsrc/alsu-epics-environment/1.3.0/debian-13/7.0.10/base`, ordinary products plus separate ASan/UBSan build 17. Base, vendor and system libraries remain uninstrumented; leak detection is disabled. All seventeen build products have ASan identity; InventoryTest has no UBSan import, so no claim of seventeen UBSan-importing products is made. Build 16 and 17 have identical bytes and source inventories for the sixteen products other than RecordTest. The earlier non-record receipts therefore retain their identified current-product scope. The eight default controls retain their build-16 test identity and unchanged defect/assertion paths; the three D7 controls ran on build 17.

Selected non-record evidence: foundation PASS667, lifecycle PASS38, independence PASS27; ordinary native API PASS84, native adapter PASS779, operator PASS15, components 15 executions, qualification 28 cases, supervisor PASS25, production CA PASS315, shutdown 2 cases and startup 10 modes. The corresponding seven instrumented groups omit native API/operator; native-adapter has its own instrumented build. Non-record receipts are under `<local>/final-m6-ordinary/` and `<local>/final-m6-asan/`; foundation is under `<local>/final-m6-foundation/`. The earlier record receipts in those two command batches are historical and are not the current thirty-case selection.

The retained ordinary v2 queued-simm attempt failed because the external owned-worker ptrace helper received EPERM. It is excluded from PASS selection, preserved with its error, and replaced by the authorized v3 execution. The inherited M3 `V3.3_wrong_owner` remains NOT RUN: an independently owned regular secret fixture could not be created (chown errno 22). No M6 waiver or secret-permission relaxation follows.

Actual measurements (microseconds; observations, not timing guarantees):

| Mode | Unforced Result-to-Retired, five trials | Exercised retirement branch |
| --- | --- | --- |
| Ordinary | 10411, 10279, 10281, 10357, 10434 | 5/5 |
| ASan/UBSan | 13635, 10292, 10788, 10471, 10913 | 5/5 |

| Mode | Trial | Budget (ms) | Admission-to-Ready (us) | Put-to-dispatch (us) | Second dispatched |
| --- | --- | --- | --- | --- | --- |
| ordinary | sample-1 | 1000 | 382495 | 1381795 | True |
| ordinary | sample-2 | 1000 | 371438 | 1371468 | True |
| ordinary | sample-3 | 1000 | 371174 | 1371067 | True |
| ordinary | below | 185 | 381481 | 0 | False |
| ordinary | above | 764 | 381813 | 1145200 | True |
| asan | sample-1 | 1000 | 727512 | 1729312 | True |
| asan | sample-2 | 1000 | 729136 | 1729693 | True |
| asan | sample-3 | 1000 | 716920 | 1714482 | True |
| asan | below | 358 | 730069 | 0 | False |
| asan | above | 1458 | 740089 | 2198773 | True |

The initial three-sample threshold is 371174–382495 us ordinary and 716920–729136 us instrumented; all five relaunches span 371174–382495 us and 716920–740089 us respectively. Zero put-to-dispatch denotes an unsent generation, not zero latency. Each below-budget run records deadline-before-send and a later explicit recovery; each above-budget run dispatches after reap/Ready. Raw measurements reside in each selected case's `record-observations.json`.

Receipt audit: `<local>/final-m6-audit.py` checks retained real receipts rather than executing a replacement integration path. Its 10,250 checks over 161 result receipts PASS, including current source/product hashes, identified libraries, selected command receipt hashes, actual child exit/reap outcomes, control failures and the unchanged production source tree. Foundation inventories unused RecordTest/test_records source files from before the supplement; those two source entries are excluded there and checked by the current record receipts. Documentation hashes are historical execution identities and may change during recording.

| Evidence | SHA256 |
| --- | --- |
| `<local>/final-m6-audit.json` | `f117545a8334755b5332bb837462d12e59dbb433d2600ad4ce79830f2836c8d5` |
| `<local>/final-m6-records-v2-ordinary/commands.json` | `60d3c5e44bf0418c8048f4d6c1352ac3052d5dfe9e84cfc13045458671fea345` |
| `<local>/final-m6-records-v3-ordinary/commands.json` | `37466a1a8e0d2d6c92961430601c7925a939b256cd46eef688aceb80a9fce1eb` |
| `<local>/final-m6-records-v2-asan/commands.json` | `5aa54f2a7a84e4be1f2ed95bb5dcf47be9715e24056e7f726e089f7781de4754` |
| `<local>/remaining-sanitizers-17/sanitizer-build.json` | `621713236a342a9b06c639c626493209df6d54c41003867ca948e03ff9d6aebb` |
| `<local>/final-m6-controls/results.json` | `9460c1bdd4e83593279aaa52b1b28b2d9c3624cbd510b1f855b580246bd4e489` |
| `<local>/final-m6-d7-controls/results.json` | `4bb8d2dfbf077abeda5874ec1d2f37d9fe4910f1e03100569cba2e88506c405a` |

Review acceptance: independent technical review 1 PASS, with zero unresolved must-fix/minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261009_003402_subagent_gpt6_thread_plan_r1_m6-reconciliation.md`, SHA256 `8ee523a3fbc89e8f2cbd2568dcc98b861ed45c9e4ef79b4299e92e94c798170d`. The reviewer executed final-clauses (PASS14/396), independently checked 210 raw observations and 3,218 reconciliation checks, including sixty current record cases and 168 record child receipts. First reader review 2 identified F-final-reader-001; the documentation-only correction was accepted by bounded second reader review 3, PASS with no unresolved findings. Required runtime executions and their reviews are complete within the recorded exclusions. Reader reports: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261009_003900_subagent_gpt6_thread_plan_r1_on_rev20261009_003402.md`, SHA256 `5842e6606b12ef2f854114eb0fcda6cd72ec7d1c1a20d2f1b092862ed97ef351`; `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261009_004422_subagent_gpt6_thread_plan_r1_on_fup20261009_003900.md`, SHA256 `996c68bd112f9fc128df27b4dbb987034f58dd2f5c06ae7ba69bea893d443fa6`. Required runtime executions are complete within the stated exclusions. At this verification checkpoint, `recordSupport=unavailable` and repository landing/closure remained separate work. The subsequent Capability Advertisement Supplement owns the current report value and its focused verification; the full-matrix receipts above retain their original identities.


###### Capability Advertisement Supplement (2026-10-08)

Plan Status: accepted
Plan Acceptance: 2026-10-08; owner selected option 1, the existing report format with recordSupport=available
Implementation Authorization: 2026-10-08; owner explicitly directed execution of option 1

Premise: the required record verification and independent technical/reader reviews are accepted in Current Required Verification Results. Commit `7e21dd568cb16974f8f2cc6d554a39ada5250aea` carries those tests and records; its same-named origin branch was observed at that commit after push. The static report string still says unavailable. The capability is a compiled module feature, not a record-enable switch, lifecycle state, configuration validity or device-reachability result.

P001: change only the recordSupport value in `snmp3App/src/Register.cpp`; retain Base version and owned-worker-transport spelling. Update the literal report assertions in `tests/rewrite/run_independence.py` and `tests/rewrite/test_r5_operator.py`.

P002: align current support/coverage descriptions in `docs/snmp-rewrite-contract.md`, `docs/snmp-rewrite-config.md`, `docs/snmp-rewrite-architecture.md` and `docs/snmp-worker-supervision.md`. Preserve all runtime contracts, historical measurements and deferred test boundaries. State the static-report meaning and direct readers to current qualified coverage; repair the two local links if the candidate heading is renamed.

P003: verify the changed report through the actual production IOC, run ordinary and instrumented record baseline regressions, obtain independent technical and separate reader review, and record the identified products/results here. No record-name list, behavior change, new support policy, M11 expansion, Git mutation, handoff or memory work is included. The initial worktree is clean at 7e21dd5; existing review-session evidence is local-only.

Verification sequence from the checkout:

```bash
CAP_BASE=~/gitsrc/alsu-epics-environment/1.3.0/debian-13/7.0.10/base
python3 -B tests/rewrite/run_independence.py --base "$CAP_BASE" --output work/capability-independence
make -C tests/rewrite -j2
python3 -B tests/rewrite/test_r5_operator.py --output work/capability-operator
python3 -B tests/rewrite/test_records.py --case baseline --output work/capability-records
python3 -B tests/rewrite/build_r5_sanitizers.py --output work/capability-sanitizers
CAP_PRODUCTS=work/capability-sanitizers/products
python3 -B tests/rewrite/test_records.py --products "$CAP_PRODUCTS" --sanitizers --output work/capability-records-asan
```

The instrumented production IOC also runs the real generated registrar and snmp3Report before/after iocInit and after snmp3Stop, with exact output, exit/reap, loader identity and sanitizer diagnostics retained. Report output remains available at every lifecycle point. Installed Base/system/vendor libraries remain uninstrumented, with leak detection disabled. The full previously accepted matrix retains its original product identities; this bounded output change does not relabel old receipts as new executions.

Verification Results, observed 2026-10-08 (Pacific): production report and the two shipped report assertions now use `recordSupport=available`. The only production source delta from 7e21dd5 is this literal in Register.cpp; record, configuration, scheduler and lifecycle code are unchanged. Actual execution on Debian 13 x86_64 with the existing Base 7.0.10 installation produced the following results.

| Actual execution | Result | Receipt SHA256 |
| --- | --- | --- |
| `<local>/capability-independence/results.json` | PASS27; forced application build, actual IOC report, native-free load and Base processing | `7fc349f25709833955ae7605d066367b99336790f2e02299bae3d10e951c4167` |
| `<local>/capability-operator/results.json` | PASS15; actual configured IOC commands, new report, Running/Stopped and worker joins | `4958e2d8d8560d1db0c1d01523e83680ddd35659cfd7f888129d1da813064034` |
| `<local>/capability-records/results.json` | PASS11 runner checks / 206 C++ assertions; eleven actual record kinds | `9a545f7482a92fbf095ccb3740786b3f9f016fd24b8960f8eac012dcdb1b1e04` |
| `<local>/capability-records-asan/results.json` | PASS11 runner checks / 206 C++ assertions on separate instrumented products | `6fe88dedbae4ea28ac127fad321fb032d479c90c79e11335fd9ae39d51a780ca` |
| `<local>/capability-report-ordinary/results.json` | PASS7; exact report before init, while Running and after stop | `3c3e769cf8b922728b1782639dc30a8804c450fcf16166b17d3283a0144a6669` |
| `<local>/capability-report-asan/results.json` | PASS7; same three actual IOC observations on instrumented products | `c98be55da3e16fe2d39b2ae79e536b534517cfdbd2b44f2be263cb6545765010` |

Separate build: `<local>/capability-sanitizers/sanitizer-build.json`, SHA256 `8765d07f049c8943fae4188ab5be0fd2ac3a5eb877c3fed16e6957c59fb4d33c`; seventeen products compile successfully. The compiled new module/native/test code is instrumented; Base/system/vendor dependencies and leak checks retain the limits stated above. Each report receipt identifies the actual loaded support library and records normal exit/reap without forced cleanup or sanitizer diagnostics. Before iocInit the actual configuration report has revision 0 and zero definitions, while the capability is already available. The state sequence is Cold, Running, Stopped; capability stays identical. This does not assert device communication or configuration success.

The extra observation procedure is `<local>/capability-report.py`; each result directory retains its actual production-IOC startup script and stdout/stderr. The script invokes the production IOC and generated registrar directly, using empty configuration; it substitutes no internal runtime span. The shipped baseline separately exercises real records, worker, native transport and agent. Earlier full-matrix receipts remain evidence for their identified earlier products. These focused executions qualify the literal-only delta; the full matrix and negative controls were not rerun for this change.

Review Acceptance: independent technical review 1 PASS, with zero unresolved must-fix/minor findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261009_012645_subagent_gpt6_thread_plan_r1_capability.md`, SHA256 `55afffe12a4f6a1abf00dd74780ecb3790b83915cd010b919cd1c81631f90cad`. The reviewer independently executed the configured operator path (PASS15) and a real IOC configuration-rejection/Cold-stop probe (PASS9), and checked retained receipts, source/product identities and document links (audit PASS800; not runtime executions).

Separate second-person reader review 2 PASS, with zero unresolved findings. Both required reviews of the capability supplement are complete. This reader pass checked the changed documentation, CLI and retained actual receipts; no new runtime execution ran. At this review checkpoint, M6 remained In progress pending repository landing and closure. Subsequent landing and completion are recorded in Closure Evidence below. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261009_013125_subagent_gpt6_thread_plan_r1_on_rev20261009_012645.md`, SHA256 `c1f9a6281b9485114d2ae80c08ff4a3324d10d9f610713c7ba17ff7863371c3b`. The capability execution and its reviews themselves included no Git, memory or handoff change.


##### Closure Evidence

Completion Date: 2026-10-08 (Pacific). M6 is Complete for the accepted required scope. G1/G5 implementation gates and the required verification and capability reviews are satisfied. No linked GitHub issue or unresolved M6 external gate remains. This completion does not close M7-M9, Backlog M10-M12, or the formal review session.

The full required-verification checkpoint is `7e21dd568cb16974f8f2cc6d554a39ada5250aea`; the separately reviewed capability checkpoint is `ade4c58a1ceb2e6cdf39c981db7023c22970e5db`. The only later production change is the static unavailable-to-available report literal. The full thirty-case ordinary and thirty-case ASan/UBSan selections and eleven compiled controls retain their original product identities. The capability supplement records its separate actual IOC and ordinary/instrumented record-baseline executions. Closure adds no new IOC, build or control execution and does not relabel old receipts as tests of rebuilt binaries.

Repository landing was observed after `git fetch origin` completed with exit 0 at 2026-10-09 04:22:35 UTC. HEAD and fetched `origin/feature/snmp-base-7.0.10-rewrite` both identified `ade4c58a1ceb2e6cdf39c981db7023c22970e5db`; their complete tracked trees both identified `d0ef8050c81c02734a5a2e7ac76c15b8c8c0520e`. The full-verification commit is an ancestor, and the working tree was clean before this closure-record update. Recheck with a fetch of origin, compare HEAD and its upstream plus both tree IDs, and verify that the two named commits remain ancestors. This landing observation covers the deliverables; the new closure-record text requires its own subsequent commit.

| Completion criterion | Accepted evidence | Closure result |
| --- | --- | --- |
| Advertised record/type and loss boundaries | T1-T5; baseline, numeric, edges, boundaries, contract and final-clauses; actual storage, wire and native readback | Satisfied within D13 scope |
| Full identity and once-only Base completion | T1, T5-T8, T10, T12, T14; record/FLNK/PACT observations and actual stale-generation control | Satisfied |
| Immutable first SET and explicit retry | T4, T7-T9; active, active-unforced, CA, policy, queued-simm and retirement-flow | Satisfied within D13 scope |
| Charged ownership through consumption and retirement | T9-T10, T14; accounting, deadline-queue, callback pressure and genuine retirement/reap observations | Satisfied; D11/D12 limitations unchanged |
| Shutdown, detach and callback storage safety | T11-T12; drain/stop/lock/downstream, isolated/non-isolated, detach, rebuild and startup cases | Satisfied within dated T11 scope; no total-duration guarantee |
| Required T1-T14 execution and honest limits | Current Required Verification Results and Capability Advertisement Supplement; exact selected receipts, source/products and compiled controls | Satisfied; inherited NOT RUN and instrumentation limits remain explicit |
| D13 waived-check accounting | The four groups listed below remain assigned to Deferred M11 | Accounted for; not executed PASS results |

Waived M6 checks under D13 (Decision Date: 2026-10-05), retained in Backlog M11:

1. T2/T3/T4 Channel Access variants: signed-minimum int64in/int64out access, maximum-capacity lsi/lso VAL$ access, and ao/longout writes.
2. T7 long active client-write verification.
3. T8 delayed output simulation-mode changes.
4. T12 isolated rebuild/reuse beyond the accepted D7 rebuild case, including delayed or abandoned queued completions, lsi/lso pointer-backed buffers and maximum-capacity payloads.

Other limits remain unchanged: D11's unpinned unsent WorkerFailure/message-reset placement, D12's unobserved printed nonzero behindRetirement counter, and the 2026-10-04 T11 outside-stop-bound reap exclusion. The M3 differently-owned secret-file fixture remains NOT RUN. Base/system/vendor dependencies are uninstrumented; leaks and TSan were not qualified, and InventoryTest's UBSan-import exception remains recorded. APC equipment/consumer behavior, two-OS and sustained resource acceptance belong to later work. M10's control-harness questions and M12's total-stop-duration decision remain Open. No new waiver, security relaxation or successful result is inferred for any of these limits.

Closure evidence review 1: PASS, no unresolved findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261009_042804_subagent_gpt6_thread_plan_r1_m6-closure.md`, SHA256 `b78d4042e546157d9747df607a8b9e03e980ad4aa076a1151782ddfca46af2dc`. The reviewer independently compared 5,429 recorded source entries from 161 receipts with the landed full-verification commit, preserving the two already disclosed unused foundation source entries, and audited selected runtime observations, product identities and required controls. The new checks inspect real retained evidence; they are not new runtime executions.

| Closure audit | Observed result | SHA256 |
| --- | --- | --- |
| `<local>/m6-closure-audit.json` | PASS413; fetched landing, source delta, retained receipts and sixty selected record cases | `efc5e2bc82b5b5bd1c695cd9c1fc31a4834cf549f18356c874f473a9afe8d31c` |
| `<local>/m6-closure-review/source-commit-audit.json` | 5,429 entries and 95 source paths compared with the full-verification commit; disclosed unused foundation entries preserved | `da251e1dca7584270e83d7082f52e3fa90a8dad6fd29a1c0de1bbb59c60d2256` |
| `<local>/m6-closure-review/closure-audit.json` | PASS569; required-case inventory, raw receipt/product checks, limits and landing | `450bef66e640d9d113bcd5196d6ce10484e2b4a942e53f6da4ee8f465d727b5b` |

Separate second-person reader review 2: PASS, zero unresolved reader-action or truth findings. Both reviews of the M6 completion record are complete. This pass checked the full canonical delta and retained evidence; no new runtime execution ran. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261009_043233_subagent_gpt6_thread_plan_r1_on_rev20261009_042804.md`, SHA256 `225ace7f32af7877750f3b7efc2babd889e0155b0dc9cd3d8f79e4d715b7ae63`. Next: prepare this documentation change for commit under the Git workflow; then prepare the M7 detailed plan and obtain G2 acceptance and separate execution authorization. M6 remains Complete, G2 Open, M7 implementation Blocked and M11 Deferred. No equipment operation, deployment, Git execution, handoff or formal review-session closure is authorized by this acceptance.



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
- Decision Date: 2026-10-09. Prepare the draft using the owner-identified sibling apcpdu repository. This identifies source material, not an approved hardware target, comparator product, cross-repository implementation or permitted equipment action.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

The following is a reviewable proposal, not an executable authorization. The canonical detail owns the maintained plan; the session reference is `<local>/review_sessions/20261003_013534_rpro-admission/plan/plan20261009_091421_codex_gpt6_root_apc-migration.md`. Independent third-person draft review 1 is PASS, with zero unresolved findings; reader review 2 identified the historical-scope correction recorded below. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261009_092303_subagent_gpt6_thread_plan_r1_m7-plan.md`, SHA256 `8ec9314c1cec744d816bac0ec2d2a37a9e3893726af0212d0dc913cf062c9f2e`. The review read both whole targets and independently checked 43 APC source files, 43 distinct template OID names and ten scaled source declarations; these are offline source checks, not runtime verification. Audit: `<local>/m7-plan-review-r1/audit.json`, SHA256 `3daaab46799d5ba298a6e0e37d870978fcbacea210e5d713a12f872022d249d8`. The reviewed canonical SHA256 was `765f03c867fef01683c07be9f2fa7e6bb5500d94915b4994235f5f9298e5753b`; At the technical-review checkpoint, only the review-status paragraph and next entry changed to record that result. The Local AP8932 Execution Scope below was added afterward under auth20261009_154800 and was not part of review 1's frozen target. Full M7 acceptance and implementation authorization remain none; bounded local acceptance and authorization are recorded separately below. Reader review 2 (fup20261009_155223, SHA256 `99a72da9892b2cb4a98d20b1d87cdb807142555129881a6fdb902a4d691766c8`) identified this historical-scope defect as F-m7-local-reader-001; bounded review 3 PASS verifies the correction, with no unresolved reader findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261009_155418_subagent_gpt6_thread_plan_r1_on_fup20261009_155223.md`, SHA256 `8c9c037bfb02f6638f09174a1e3592a41d4b91d9abefac39c727e23c5218f0c0`. The local reader gate is complete; auth20261009_154800 authorizes only the bounded local execution.

##### Draft Migration And Comparison Plan

Source inspection on 2026-10-09 used snmp3 commit `5e3ac5a59618a9ff0ad593281b00db798b48e9b1` and apcpdu commit `54345c73c9f2c7fb8210a8b39e593564f61271b5`. APC application sources, loaders, templates and test sources match that apcpdu commit. Its working tree has an unrelated modified milestone document and an untracked IOC service configuration; neither is comparator source or permission to modify that repository. Its configured local Base path selects 7.0.10. Build files select `devSnmp` and `pvxsIoc`; the exact installed driver, libraries and generated products still need execution-time identification. Earlier APC test reports are historical evidence, not snmp3 qualification.

The source inventory includes `apcpduApp/iocsh/pdu_rpdu2g.iocsh`, `pdu_ap7800b.iocsh`, `apcpduApp/Db/`, `apcpduApp/src/Makefile`, `apcpduMain.cpp`, `apcpduInfoScan.cpp`, `tests/test_pva.py`, `tests/snmp_peer.py`, `scripts/check_apcpdu_pvs.bash`, and `mibs/PowerNet-MIB`. The rPDU2 loader has 16/24-outlet variants and optional sensor, PVA and setting-write branches; AP7800B has a separate loader. Presence in the inventory does not select all variants as required hardware scope.

| Classification | Source-supported premise | Plan consequence |
| --- | --- | --- |
| Confirmed incompatibility | apcpdu loaders call `devSnmpSetSnmpVersion`, `devSnmpSetMaxOidsPerReq` and the legacy credential loader; templates use symbolic OIDs and `DTYP=Snmp`. snmp3 requires explicit profiles/endpoints/bindings, numeric OIDs and `DTYP=snmp3`. | P002 replaces the configuration/link interface deliberately; no legacy parser adapter. |
| Confirmed incompatibility | `pdu_load_rpdu2g.template` and `pdu_load_ap7800b.template` use `LINR=SLOPE`, including 0.1 and 0.01 scaling. The snmp3 record contract requires direct ai VAL with NO CONVERSION. | P003 must preserve engineering values through an explicit DB conversion; changing DTYP/link text alone is insufficient. |
| Confirmed startup gap | `apcpduMain.cpp` ignores the startup `iocsh` return, enters `iocsh(NULL)`, and returns zero; both existing APC device loaders omit `on error break`. snmp3 `Main.cpp` and `Register.cpp` implement different error handling. | P004 must qualify the actual migrated application main and nested loaders; testing snmp3Ioc alone cannot establish APC startup termination. This is source evidence, not an executed failure result. |
| Confirmed configuration constraint | snmp3 `docs/snmp-rewrite-config.md`, Publication and freeze conditions: successful loads replace the whole snapshot; binding or Base device initialization freezes it. | P002 assembles all selected devices before binding. Independent per-device JSON loads must not silently discard earlier devices. |
| Confirmed test boundary | APC `tests/snmp_peer.py` is a loopback SNMPv2c peer. `test_pva.py` launches the real APC binary, installed DB/loaders and CA/PVA clients. | Reuse real application paths, with only the external agent substituted; add actual v3 agent coverage separately. Existing v2c results cannot qualify v3. |
| Hypothesis requiring execution | APC outlet fanout processes raw input, command readback and decoded state in sequence. The snmp3 contract explicitly provides no asynchronous fanout completion barrier. | P003/T5 must delay real replies and observe decoding/summary publication. Do not declare unchanged fresh-value ordering from source order. |
| Owner decision | Hardware model/card, selected loader variants, comparator product and permitted actions are not selected. | Resolve the G2 choices below before implementation/operation; no automatic selection from site startup files. |

###### Ordered Work

1. **P001 - Fix comparison inputs and boundaries.** Record the accepted APC source commit, original `devSnmp` source/library identity, IOC executable, generated DBD/DB/loaders, Base/PVXS/native libraries, MIB digest and independent CA/PVA client versions. Identify each required loader variant and firmware/card class. Preserve the original comparator products; use separate candidate products and prefixes/ports so the baseline and changed IOC do not share mutable build outputs or compete to write hardware. Comparison uses the same approved external-agent conditions and records intentional differences. Close with T2/T8 input checks before changing application files.
2. **P002 - Prepare explicit configuration and loader migration.** Candidate APC paths are `configure/RELEASE` or approved local build overrides, `apcpduApp/src/Makefile`, `apcpduApp/iocsh/`, and new non-secret configuration examples. Resolve each symbolic OID from the pinned shipped MIB, preserving instance index, ASN.1 type, operation, signedness and capacity. Build one complete schema-1 configuration for all selected devices, with unique binding IDs and explicit per-record deadline budgets; load it before any record binding. Separate numeric endpoint address from port and refer to owned secret files without copying deployed credentials. Replace legacy driver commands and link strings, register/link snmp3 and retain the APC information scheduler and PVA support. No polling, timeout, retry, max-OID or skip-readback setting is assumed equivalent by name or number. T4 proves complete multi-device configuration and explicit rejection of malformed/duplicate/missing references.
3. **P003 - Preserve the public DB/consumer contract under asynchronous reads.** Candidate files are the selected device, phase, bank, outlet, sensor, scanner and PVA templates/substitutions under `apcpduApp/Db/`. Inventory every public PV and group field before editing: name, record/type, unit, scale, enum, alarm, string representation and access policy. Proposed conversion uses private raw snmp3 records and existing public value records where scaling is needed; exact record/link choices require plan review before acceptance. Preserve 0.1/0.01 conversions once only. Drive derived state from the corresponding completed input and keep existing FLNK consumers accounted for, without claiming a common device sample or a fanout barrier. Preserve independent command and readback PVs. Treat legacy quoted string formatting, removal of independent cache polling, request timing and alarm timing as explicit comparison differences requiring acceptance, not accidental compatibility. T5/T6 use the actual generated database and delayed/reordered outer-agent replies to check fresh values, units, alarms and same-value recovery.
4. **P004 - Qualify the actual nested startup and application main.** Candidate files are APC `apcpduApp/src/apcpduMain.cpp`, device loaders and enclosing startup examples, plus shipped fixtures/runner under snmp3 `tests/rewrite/` that select the real migrated application. Apply `on error break` to both loader levels and propagate startup failure to a nonzero process exit before the interactive shell. Reuse the snmp3 failure contract rather than adding an internal test replacement; retain the runtime Failed-state check. Test invalid configuration at the first loader and after an earlier valid loader, retaining a valid complete configuration until rejection and never invoking `iocInit` on the failed path. Test valid SNMPv1/v2c/v3 configuration separately from successful network I/O. T3 must execute the accepted installed APC executable and loaded snmp3 library with an stdin marker, bounded process wait, explicit before/after-init observations and credential-sentinel scan of all captured logs. A missing installation/product identity is NOT RUN, not a substitute snmp3Ioc result.
5. **P005 - Build the local comparison tests around shipped paths.** Extend APC `tests/test_pva.py`, `tests/snmp_peer.py` and the existing checker as needed for the selected public interface; extend snmp3 `tests/rewrite/` only for reusable configuration/startup/agent coverage. Run both identified real IOC applications and independent clients, retaining raw wire events and record/client observations. The v2c peer may supply delay, drop, changed value, malformed response and SET outcomes at the external boundary. For v3 use the actual installed native agent path, extending the shipped native-agent fixture with the selected APC OIDs as necessary; do not add a fake internal security/session implementation. T2 and T4-T7 close local behavior. Run the existing relevant snmp3 record/configuration and APC consumer regressions; preserve baseline defects and differences rather than rewriting expected values to force equivalence.
6. **P006 - Verify only approved equipment actions.** After G2 records the exact target, installation, operator and permitted OIDs/actions, run T1 and the hardware portion of T2 on that target. Begin with identified read-only observations and no automatic write-back. Outlet On/Off/Cycle, master commands, threshold/coldstart/delay settings, interruption and recovery operations each require an explicit permitted action and recovery procedure. A request to compare does not authorize any of them. Read-only hardware acceptance does not close required write/physical-action cells; unresolved cells remain NOT RUN unless the owner makes a separate dated scope decision. Do not infer authorization from existing `WRITE_EN` or `LIMIT_EN` settings.
7. **P007 - Record evidence and accepted differences.** Update the approved APC operator/test documents and this M7 detail with source/product/library/script hashes, selected test cases, timestamps, raw event references, result counts and limitations. snmp3 `tests/rewrite/README.md` owns any new shipped runner instructions. Keep private targets, credentials and deployment configuration outside public records. Review all retained public PV/group behavior and every T1-T8 result; preserve M11 and all M6 exclusions. No APC deployment, two-OS qualification, sustained resource acceptance or milestone completion is implied by a local pass.

###### Open Choices Before Acceptance

| Choice | Alternatives and consequence | State |
| --- | --- | --- |
| Required target/variants | AP8932/rPDU2 with selected 16/24-outlet/sensor options; AP7800B; or both. Broader selection increases the DB and equipment matrix. Record the actual card/firmware class separately. | Owner selection pending; inventory is not acceptance |
| Comparator | Preserve products from apcpdu `54345c7` with the identified installed legacy driver, or choose another explicitly identified deployed baseline. The latter needs its own source/product correspondence. | Proposed source located; product selection pending |
| Repository/build boundary | Develop the APC candidate in an isolated apcpdu checkout while snmp3 owns reusable tests, or first produce a local integration example within snmp3. An example alone does not establish deployed APC migration. | Owner selection pending; no external checkout edit authorized |
| Equipment scope | Local-only preparation; real-equipment reads; or a named set of writes and recovery actions. A smaller authorization leaves the remaining hardware checks pending. | No equipment actions authorized |
| Compatibility differences | Accept explicit new timing/string/alarm semantics, or preserve selected public behavior through the APC DB layer. Neither option may reintroduce legacy syntax parsing or silently change snmp3's accepted contract. | Exact differences require baseline observations and review |

M6 is a completed behavioral dependency. G2 is the remaining external acceptance/authorization condition; the source inventory is not an additional execution gate. Plans and read-only source checks may proceed while G2 is Open. No new library installation, firmware operation, production IOC restart, service configuration edit, Git/remote mutation, memory or handoff write is included in this draft.

###### Execution And Recovery Boundaries

All new tests below are prospective. Select exact commands only after the candidate layout and installation paths are accepted; no command in this draft starts an IOC. Local tests must bind only loopback addresses with explicit CA/PVA search settings, disposable secrets, owned child processes and bounded cleanup. Real devices must never be discoverable through local test defaults. Run the real shipped templates/loaders/main/driver/clients; generate fixtures from those sources, never reconstruct an internal path in Python. Preserve baseline and failed-run evidence, and report cleanup failure separately from test outcomes. Stop on an unexpected SET, target mismatch, lost observation, missing product identity or unresolved consumer difference. Hardware rollback is a pre-approved operator action, not automatic replay of old commands. Earlier approvals of M6 do not authorize this plan.

##### Local AP8932 Execution Scope

Decision Date: 2026-10-09. The owner accepted and authorized a separate snmp3 verification IOC based on apcpdu, then selected AP8932 24-outlet first. This resolves the initial local target and construction scope only. The full M7 hardware scope, comparator-product acceptance and compatibility decisions remain pending; G2 remains Open. Authorization: `<local>/review_sessions/20261003_013534_rpro-admission/plan/auth20261009_154800_codex_gpt6_root_apc-local.md`.

Local Plan Status: accepted
Local Plan Acceptance: 2026-10-09, separate verification IOC based on apcpdu, AP8932 24-outlet first
Local Implementation Authorization: 2026-10-09, construct, build and verify the local IOC; no equipment or deployment

Implementation is confined to snmp3 `tests/rewrite/build_apcpdu.py`, `tests/rewrite/test_apcpdu.py`, test documentation and this canonical record, with generated application/products/evidence under a new `work/` directory. apcpdu commit `54345c7` supplies the actual committed DB, scheduler, MIB, group definitions and outer-agent fixture through read-only export. The sibling working tree, deployed startup and secrets stay untouched. Build against the existing Base 7.0.10, snmp3 and PVXS libraries. The generated real application uses the shipped snmp3 Main.cpp, generated registration and original APC information scheduler; it is not an internal-function test substitute.

The initial candidate loads the rPDU2 24-outlet, two-bank, identity, phase, readback and command DB with the original read-only PVA group. Optional sensors and LIMIT_EN setting write-back stay disabled. Preserve every selected public record name and type. Replace legacy INST_IO links with complete explicit schema-1 bindings resolved from the committed MIB. For scaled ai values, a private raw snmp3 ai completes into a private calc and then the original public soft ai, retaining the existing public FLNK consumers and metadata; transfer scanning/request triggers to the raw record. Remove the outlet fanout's premature decoded-state processing and invoke decoding from raw-status completion. Existing periodic alarm summary remains eventual, not an all-input completion barrier. Raw octet strings replace legacy formatted quotes as an explicit local comparison difference; hardware acceptance is not implied.

The build helper requires explicit `--apcpdu`, `--base`, `--pvxs` and new `--output` paths, verifies the selected baseline files and installed prerequisites, expands the actual shipped substitutions, constructs the migrated DB/configuration and runs the real EPICS build. The test runner requires the generated `--ioc` directory and a new `--output` directory. It uses only loopback SNMP endpoints and isolated CA/PVA ports, the original outer PDU fixture, the real built binary and actual CA/PVA clients. No live endpoint is accepted by the local runner. Exact CLI forms: `python3 tests/rewrite/build_apcpdu.py --apcpdu <source> --base <base> --pvxs <pvxs> --output <new-build-dir>` and `python3 tests/rewrite/test_apcpdu.py --ioc <build-dir> --output <new-evidence-dir>`.

Local verification checks build/registration/library identity; correct Model, phase/bank values and all 24 decoded outlet states; real CA-to-PVA values/types and read-only group policy; first/later nested startup rejection and no interactive-shell entry; valid v1/v2c/v3 configuration (not v3 wire qualification); loss and same-value recovery; and zero unintended SNMP SET. Local command SET testing, when present, addresses only the resettable outer peer and cannot establish physical action. Retain exact source/product/fixture hashes, commands, process outcomes and cleanup. This is initial local coverage of T3-T8; it does not close T1/T2, actual v3 APC communication, full legacy comparison, delayed/reordered-response matrix, optional branches or M7. Record each uncovered check explicitly rather than treating the build as qualification.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | APC integration | Real new-module startup/DB, v2c/v3 agents and identified CA/PVA consumers. | Approved target and identified firmware/source | Correct values, writes, alarms and recovery through the new interface. |
| T2 | External comparison | Independent baseline/new observation of SET values/order/retries and response/record phases. | Same approved comparison conditions | Differences are attributable and no final-value-only equivalence is asserted. |
| T3 | Installed startup integration | Run the shipped nested device loader and enclosing startup script with the actual installed IOC and driver. Inject invalid local configuration at the first device and after an earlier valid device; also run valid SNMPv1/v2c/v3 cases. Inspect process exit, `iocInit` and interactive-shell observations, and all captured logs for credential sentinels. | Approved unchanged installation; identified IOC, driver, native helper and script hashes; disposable local configuration/secret fixtures | Both error positions terminate with nonzero exit before `iocInit` and interactive-shell entry; valid cases reach `iocInit`; no credential values appear in logs. No internal parser, wrapper or IOC main substitutes. |
| T4 | Configuration and mapping | Expand shipped APC substitutions; inventory selected public PVs and numeric OID/type/operation/capacity mappings. Load the complete multi-device candidate through the real parser; test duplicate IDs, missing references and replacement/freeze errors. | Identified MIB, generated DB and native helper; loopback-only endpoints | Every selected device remains represented before binding; invalid input is rejected without publishing a partial candidate; no deployed secret appears in fixtures/logs. |
| T5 | Record completion and conversion | Use real snmp3 APC records with delayed/reordered/dropped replies; observe raw and derived records, units, alarms, PACT and FLNK, scanner pause/resume and same-value recovery. | Selected actual DB/loader variants and outer agents | Scaling occurs exactly once; derived publications use the intended completed input; retained/stale values and asynchronous ordering remain explicit; no false simultaneous-sample claim. |
| T6 | CA/PVA consumer contract | Run the shipped checker and actual caget/pvxget/pvxmonitor/pvxput consumers against the migrated APC application. Compare field inventories, types, strings, enums, units, alarms, group enable/disable and selected optional branches. | Identified Base/PVXS clients and linked APC IOC | Approved public interface preserved; group writes rejected where read-only; every intentional difference listed and accepted; no hand-built group stands in for the application. |
| T7 | Commands and recovery | From real command PVs observe SET value/order/retry, independent readback, failed SET, explicit retry and device-loss recovery; separately enable only the selected setting-write cases. | Resettable loopback agents; hardware subset only under P006 | No unintended SET at startup, read-only polling or recovery; approved command/write-back cases have attributable requests and outcomes; successful SET and physical confirmation are distinct. |
| T8 | Identity and regression | Hash source/products/generated inputs/loaded libraries, run relevant existing snmp3 and APC regressions, inspect baseline versus candidate selection and child cleanup. | Separate identified baseline/candidate products | Results identify the exact real path, are reproducible, and do not overwrite or relabel baseline evidence; missing prerequisites remain NOT RUN. |

##### Verification Results

The initial authorized local AP8932 subset ran on 2026-10-09 UTC using Base 7.0.10, installed PVXS 1.5.2-dirty, the current snmp3 shared libraries and the committed APC outer PDU fixture. `<local>/ap8932-build-02/build.json` identifies the generated application, exported sources, native products, DB/DBD and library hashes. The same shipped snmp3 Main.cpp and generated registration form the real application entry path. Source/product identity and executable behavior are separate evidence; these results do not claim an installed production APC migration or full M7 acceptance.

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Physical target not approved | Pending | No equipment operation |
| T2 | Not run | Comparator not approved | Pending | No legacy/new comparator run |
| T3 | 2026-10-09 UTC | Generated application and nested loaders, disposable loopback configuration | Local subset PASS: first/later invalid configuration exits 1 before iocInit and stdin shell sentinel. v1/v2c/v3 configuration-only acceptance passes; only v2c communication reaches iocInit. Full installed loader matrix remains open. | `<local>/ap8932-test-05/result.json`, valid/reject IOC logs |
| T4 | 2026-10-09 UTC | Actual committed substitutions expanded by installed msi; numeric OIDs from committed MIB | Local subset PASS: 400 original record names/types registered, 178 bindings, nine conversions and 100 group fields. Multi-device replacement/freeze and full invalid-input cases not run here. | `<local>/ap8932-build-02/inventory.json`, `original.db`, `db/ap8932.db`; real registration/CA checks |
| T5 | 2026-10-09 UTC | Real v2c record path against original outer fixture | Local subset PASS: phase/bank and device power threshold scaling, 24 outlet decode states, cold invalidity and loss/same-value recovery. Delay/reorder and scanner pause/resume remain open. | IOC/client/peer logs and `outage-fields.json` under `<local>/ap8932-test-05` |
| T6 | 2026-10-09 UTC | Actual caget, pvxget and pvxput; original group definitions | Local subset PASS: 100 group mappings have matching CA values and expected PVA types; group put rejected without changing readback. Legacy checker, pvxmonitor, optional group branches and full metadata matrix not run. | `client-*.json/.out/.err` and result checks |
| T7 | 2026-10-09 UTC | Resettable loopback fixture; no command writes requested | Local subset PASS: no SET during startup, polling, outage or recovery. Command/failed-SET/retry/write-back and physical action remain unqualified. | `acceptance/peer.json`, `snmp-0.jsonl` |
| T8 | 2026-10-09 UTC | Separate generated product and actual loaded shared libraries | Local subset PASS: source/product hash checks, actual loaded-library identities, normal IOC exit, worker reaped and queues empty, setup-failure cleanup. Existing broad snmp3/APC regressions not rerun. | `build.json`, `loaded-libraries.json`, `processes.json`, terminal IOC logs |

Local build command: `python3 tests/rewrite/build_apcpdu.py --apcpdu <source> --base <installed-base> --pvxs <installed-pvxs> --output work/ap8932-build-02`; exact resolved arguments and build output are in the build evidence. Local test command: `python3 tests/rewrite/test_apcpdu.py --ioc work/ap8932-build-02 --output work/ap8932-test-05`. Exit 0; 252/252 checks PASS. The binding admission deadline is 15,000 ms; source SCAN and information periods remain unchanged. This is a local feasibility setting, not an approved production latency contract.

Earlier execution evidence remains retained: test-01 could not establish PVA connectivity inside the restricted sandbox; test-02 used the initial 2,000 ms deadline and did not reach healthy identity/summary; test-03 passed 248 checks with the current product before the added registration/cleanup predicates; test-04 rejected the runner's overstrict closing-flag predicate although worker reaping and queue cleanup completed. The final predicate checks actual resource outcomes and allows the retained closing flag. Only test-05 qualifies the final runner. No product C++ source or sibling checkout changed.

Independent implementation review 1 is PASS with zero unresolved findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/rev20261009_161510_subagent_gpt6_thread_plan_r1_ap8932-implementation.md`, SHA256 `8d6531c62400f02c5aa4b17fd2164e9f2243f5ded3a7794e89012966117ccea3`. The reviewer built a fresh IOC and ran standalone acceptance with no preceding startup suite: 245/245 runtime checks, exit 0. A separate real-source/receipt audit passed 1,100/1,100 predicates; these are not additional IOC executions. Evidence: `<local>/ap8932-review-r1/build/build.json`, `acceptance/result.json` (SHA256 `458bd887c8544abb6a4729f40bcdbc3bdbb337fbba22329b8588ddbdc5e81f07`) and `audit.json` (SHA256 `6bfac72f34498f61c9f775380a113d207587880beb9b653a9cb4aa7dea84aa96`). Separate reader review 2 is PASS with zero unresolved findings. Report: `<local>/review_sessions/20261003_013534_rpro-admission/reviews/fup20261009_161800_subagent_gpt6_thread_plan_r1_on_rev20261009_161510.md`, SHA256 `5cd8f5226fdcd14c8b8e1216c43ef692ed43be05372f98c7d337bb68092e1f13`. Both local implementation reviews are complete. Author qualification remains 252/252 runtime checks and independent standalone acceptance 245/245 runtime checks, with the separate 1,100/1,100 source/receipt audit; the reader pass adds no runtime evidence. This accepts only the authorized local subset with all recorded limits. This local evidence does not close M7, G2, hardware, v3 wire, comparator, command SET, optional variants or the full asynchronous matrix.

Evidence identities: build receipt SHA256 `3a082f6c0f791facb731ccba39b3af39046e3d12a493efa2cf01a08fe7e61604`; final result SHA256 `2a4a2f139dc042098a3307ffe27b13be2de8fb3c4220721891a7d3aa1396d353`.

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
| M13 | PowerNet-MIB 4.6.0 review for the AP8932 database | Carry-forward | Open | No | none | Owner decision to adopt 4.6.0 or keep 4.4.6 recorded with its date, and any `rPDU2Adv*` scope decided; adoption verified by regeneration and a new run of the database's integration test; [detail](#m13---powernet-mib-460-review-for-the-ap8932-database) |
| M14 | Group record requests and keep requests in flight on the record path | Carry-forward | Open | No | none | Owner decision on the direction recorded with its date; then the monitoring set of the evaluation read through the module within 1.5 times the direct Net-SNMP pass at the same group size and requests in flight on each device class, with the sustained schedule criteria met; [detail](#m14---group-record-requests-and-keep-requests-in-flight-on-the-record-path) |
| M15 | Secret material management for SNMPv3 device access | Carry-forward | Open | No | none | Owner decision on creation, location, rotation and repository exclusion recorded with its date; procedure documented and verified by a scan of tracked files and history; [detail](#m15---secret-material-management-for-snmpv3-device-access) |

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
  `<local>/r6-p010-controls-20261004-162208/`), the defective
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

#### M13 - PowerNet-MIB 4.6.0 review for the AP8932 database

Origin: 5dff352 / M13
Identity History: none
GitHub Issue: none
Status: Open (unresolved scope: whether the AP8932 database adopts PowerNet-MIB 4.6.0 and whether any `rPDU2Adv*` object enters it)

##### Summary

The apcpdu AP8932 database is generated from PowerNet-MIB 4.4.6 and pins that file by hash. The vendor publishes 4.6.0 (header version 4.6.0, created 2026-07-06). A static comparison of the 91 objects behind the 358 monitored instances (`docs/snmp-performance/inputs/ap8932-oids.tsv`) found no difference; the review and its evidence are in `docs/powernet-460-review/`. The decision to adopt 4.6.0, and any extension of the database scope, remain open for later review.

##### Scope

- Decide whether the AP8932 database adopts 4.6.0 or keeps 4.4.6.
- Decide whether any of the 49 `rPDU2` objects added since 4.4.6 enters the database scope, in particular `rPDU2AdvBank*` and `rPDU2AdvPhase*` for 0.1 A threshold resolution. This needs the required resolution and the AP8932 firmware that supports the objects from the PDU firmware release notes or the device.
- If 4.6.0 is adopted: replace the retained MIB, its hash pin, version labels, generated files and the database guide together in the database repository, then regenerate, build and run the database's integration test in a new evidence directory.

Out of scope: physical PDU operation, production deployment, other PDU models, sensor objects, and removing the hash check to make a new file pass.

##### Completion Criteria

- A dated owner decision on adoption of 4.6.0 is recorded in Dependencies And Decisions.
- A dated owner decision on the `rPDU2Adv*` scope is recorded.
- If 4.6.0 is adopted, the regenerated database builds, the selected numeric OIDs resolve against the 4.6.0 file with an independent translator, and the database's integration test passes in a new evidence directory.
- If 4.4.6 is kept, Closure Evidence cites the review and the decision date.

##### Dependencies And Decisions

- none yet; the owner decisions are pending.
- Database changes are made in the database repository, not in this repository.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Owner decides adoption and the `rPDU2Adv*` scope using `docs/powernet-460-review/review.md`.
2. If adopted, the database repository updates the retained MIB, pin, labels, generated files and guide, then regenerates and builds.
3. Run the verification below and record the receipts.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Static object comparison | Run `docs/powernet-460-review/evidence/compare_mib.py` with the 4.4.6 file, the 4.6.0 file (obtained from the vendor; its sha256 is in `docs/powernet-460-review/review.md`) and the monitoring set `docs/snmp-performance/inputs/ap8932-oids.tsv` | Linux host, Python 3 | All selected objects identical; added and changed objects listed |
| T2 | Independent OID resolution | Resolve every selected numeric OID against the 4.6.0 file with Net-SNMP `snmptranslate` and compare with the generated bindings | Linux host, Net-SNMP | All 358 OIDs match the database's bindings |
| T3 | Regenerated database | Regenerate with the adopted MIB, build the IOC, and run the database's shipped integration test with a new output directory | Linux x86_64; Base 7.0.10; loopback SNMP test agent | Test completes with no failed check; record inventory of the current database (544 records) unless the scope decision changes it |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-09 | Linux host, Python 3 | PASS, 91 of 91 monitored objects (358 instances) identical, 0 missing, 49 `rPDU2` objects added, 4 body-changed outside the set | `docs/powernet-460-review/evidence/compare-result.json`; SHA256 `ad2ff674ee4441ce3bfbce478576c589fefe5ab1ffb3392e7b769c1936ac1d5e`; 4.6.0 file SHA256 `5d4d0e4470378082c3dfeb2797d81938d461be6e8a9e2f59002c93564b347e00` |
| T2 | Not run | Linux host, Net-SNMP | Pending | none |
| T3 | Not run | Linux x86_64; Base 7.0.10 | Pending | The passing 4.4.6 result does not qualify 4.6.0. |

##### Closure Evidence

- none

#### M14 - Group record requests and keep requests in flight on the record path

Origin: 5dff352 / M14
Identity History: none
GitHub Issue: none
Status: Open (unresolved scope: which of the candidate changes is taken)

##### Summary

The record path sends one object per request, one request in flight per address, with 20 to 30 ms
between a completion and the next request. Measured against devices of two management card
generations, a pass over a 358-object monitoring set takes about 134 s (older card) and 11 s (newer
card) through the module, against 4 to 6 s and about 0.2 s when the same objects are read with
Net-SNMP in grouped requests. The evidence, the cost model and the device selection considerations are
in `docs/snmp-performance/evaluation.md`; the repeatable procedure and reference values are in
`docs/snmp-performance/test-procedure.md`.

##### Scope

Decide and then implement a change so that record reads reach the grouped-request speed of a device:

- Candidate changes, not exclusive, with the effect the measurements support
  (`docs/snmp-performance/evaluation.md`):

  | Candidate | Expected effect | Cost or risk |
  | --- | --- | --- |
  | Wake on socket and pipe readiness; send Result and Retired together | Removes most of the 20 to 30 ms between requests (the share of the 10 ms waits was not isolated); the newer card gains most (about 32 ms to a few ms per record) and the older card little (about 370 ms per record) | Small; no change to the batching or ownership rules |
  | Coalesce contiguous compatible requests under the earliest member deadline | Requests per pass fall by about K; direct grouped passes of 358 objects took 5.3 to 6.2 s (older card, K = 32 to 48) and 0.16 s (newer card, K = 28 to 48) against 66.5 s and 0.75 s | Revisits decision D3 (exact equal deadlines); group size must stay below the response-size ceiling (K = 64 failed on the older card) |
  | A binding that reads several objects into one record | The same request reduction, visible to the database (for example one waveform per table column) | Configuration schema extension; record conversion rules for arrays |
  | Several requests in flight per address | Older card: passes 11 and 27 percent shorter with 2 and 3 in flight at K = 32; newer card: none; also removes the idle gap | Deadline and retirement accounting; devices lose requests above a device-specific count |

- Profile parameters that follow: objects per request bounded by response bytes, requests in flight,
  request timeout above the device request p95 at that size, retries.
- Device classes seen so far: the older card answered single-object requests with a median of 30 ms
  (mean about 190 ms) and lost 2 of 10 outstanding requests while answering 5 fully; a request of 64
  objects returned `tooBig`. The newer card answered with a median of 1.6 ms and lost 6 of 20
  outstanding requests.

Out of scope: writes (SET) path timing, other device models, changing the Net-SNMP version.

##### Completion Criteria

- A dated owner decision on the direction is recorded in Dependencies And Decisions.
- Through the module on each device class, reading the monitoring set of the evaluation takes at most
  1.5 times the direct Net-SNMP pass at the same group size and requests in flight (reference values
  in the test procedure).
- The sustained schedule of the test procedure passes on each class: no tier overruns its period,
  lost requests below 0.5 percent, second client p95 at most twice its idle value.
- The existing record, lifecycle, supervisor and qualification runners still pass on the changed
  products.

##### Dependencies And Decisions

- none yet; the owner decision is pending.
- If coalescing is chosen, decision D3 (exact equal deadlines) is revisited.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Owner chooses the direction using `docs/snmp-performance/evaluation.md`.
2. Draft the detailed plan for the chosen change and its test plan; accept it separately.
3. Implement and verify with the procedure.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Fixture throughput | `docs/snmp-performance/tools/run_fixture_module.py` and `docs/snmp-performance/tools/run_fixture_baseline.py` with the added round trip 0, 20, 100 ms | Linux host, fixture agent | Module pass within 1.5 times the direct pass at the same group size and requests in flight |
| T2 | Device pass through the module | Phases 1 and 2 of the procedure with the module as the reader, per device class | Hardware of two card generations | Per-object time within 1.5 times the direct figure; no stop rule fired |
| T3 | Sustained schedule | Phase 4 of the procedure through the module | Hardware of two card generations | Overruns 0, loss below 0.5 percent, second client p95 ratio at most 2 |
| T4 | Regression | The shipped record, lifecycle, supervisor and qualification runners | Linux host | All checks pass, including the deadline and shutdown cases |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, fixture agent | Pending | Current module baseline: about 30 ms per record at zero added round trip, `docs/snmp-performance/results/reference-run.json` |
| T2 | Not run | Hardware of two card generations | Pending | Current module baseline: 362 to 396 ms per record (older card), 32 ms (newer card) |
| T3 | Not run | Hardware of two card generations | Pending | The reference schedule was run with Net-SNMP direct, not through the module |
| T4 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

#### M15 - Secret material management for SNMPv3 device access

Origin: 5dff352 / M15
Identity History: none
GitHub Issue: none
Status: Open (unresolved scope: where secret material lives and how it is created and rotated)

##### Summary

The module reads each community or passphrase from its own file referenced by the configuration; the
file must be owned by the IOC user with no group or other permissions. Existing deployments keep
SNMPv3 passphrases as plain text in an older single-file format inside project directories, which
is not suitable for a shared repository or its history. A creation, storage and rotation design is
needed that fits the module's file format.

##### Scope

- Decide where secret files live on a deployment host, who creates them and from what source, and how
  existing plain-text files are converted to the module's raw passphrase files.
- Decide how credentials are rotated given that the module replaces credentials only with a new
  process.
- Keep secret material out of tracked files and out of repository history; define the scan that
  shows it.

Out of scope: introducing an external secret store unless the owner chooses it; changes to the file
checks already implemented by the configuration loader.

##### Completion Criteria

- A dated owner decision on location, creation, rotation and repository exclusion is recorded.
- The procedure is documented without any real address, user name or secret.
- A scan of the tracked tree and of the history of the repositories that carried secrets finds no
  secret material, or the findings are listed with the owner's disposition.

##### Dependencies And Decisions

- none yet; the owner decision is pending.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Owner states the intended storage and rotation approach.
2. Write the procedure and, if chosen, a conversion helper for the older format.
3. Run the scan and record its result.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Tracked tree and history scan | Secret scan over tracked files and history of the repositories named by the owner | Linux host | No secret material, or findings with disposition |
| T2 | Startup with created files | Create files by the documented procedure and start the IOC with the module configuration | Linux host | Configuration accepted; secrets absent from diagnostics |
| T3 | Permission rejection | Existing configuration cases for wrong owner and wrong mode | Linux host | Rejected as before |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host | Pending | none |
| T2 | Not run | Linux host | Pending | none |
| T3 | Not run | Linux host | Pending | The configuration runner already records the permission cases |

##### Closure Evidence

- none
