# Work Register

Release line: master
Milestone index: 6f015f8
Canonical path: `docs/milestone-6f015f8.md`
Canonical branch or ref: feature/snmp-base-7.0.10-rewrite
Git upstream: origin/feature/snmp-base-7.0.10-rewrite
Remote tracker: none associated with this register
Recorded date: 2026-11-07
Source baseline: `6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf`

Next session entry point: `docs/milestone-6f015f8.md`, Scope. Assess the five product goals against existing identified evidence before proposing further work. End the assessment when each goal has a bounded verdict; then obtain a dated decision on one concrete product gap, acceptance criterion and stop condition. No work row is Ready. Do not resume deferred tests or measurement-tool development automatically.

## Scope

This generation retains unfinished product and verification work. The accepted baseline is the committed GET-only monitoring scope and documentation at `6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf`. The [prior qualification record](https://github.com/jeonghanlee/snmp3/blob/6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf/docs/milestone-5dff352.md) retains exact source, product, Base, fixture and receipt identities. Source equality is not a new runtime qualification; past results do not transfer automatically to another build.

The original committed premise is that blocked native transport must not stall IOC servicing or another address (`49d5c62feca95f12a13910b73b60667455773b7c`); the first interface/lifecycle contract is `81cdcddd4fb431cba55e5a7999c9f7c923a670e6`. The current assessment must distinguish these five goals:

| Product goal | Required judgment from existing evidence |
| --- | --- |
| Fresh acquisition and publication | Distinguish a fresh device response from retained VAL, advancing TIME or a normal alarm. Preserve the unresolved original ScopedPDU causality. |
| Correct Base completion and FLNK | Use actual identified DSET/callback and CA/PVA evidence. Preserve representation/capacity limits and the accepted compatibility LSI UDF limitation. |
| Periodic acquisition | Distinguish SCAN and Poll/Done counts from actual acquisition opportunities. Do not reconstruct the missing original 88-PV/63-OID/2-second population. |
| Fault, recovery and address isolation | Limit claims to executed loss, late-response, worker-replacement and healthy-address observations. Do not infer unobserved physical outage or exact recovery bounds. |
| Predictable latency and CPU | Preserve the ordinary once pass, sustained failure, baseline variation, lost probe and unresolved strict legacy CPU/freshness comparison. |

Freeze graph, types, capacities, configuration and product identities before any comparison. The legacy 163-input interface, generated358 comparison and final396 application are different workloads. A missing input remains missing evidence. A tool change requires a demonstrated obstacle to one specific product judgment, rather than another possible measurement.

Out of scope: a new equipment campaign, SET application delivery, threshold changes, a tag or release, and automatic resumption of deferred work. Pre-existing uncommitted measurement tools, their two procedure additions and untracked test inputs are excluded from the baseline and remain untouched. The current blocked-callback shutdown behavior and PowerNet-MIB 4.4.6 monitoring input are Keep decisions in [Closed decisions](CLOSED_DOORS.md), not active work.

## Milestone

### Work

| ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| M1 | Negative-control harness outcome classification | Carry-forward | Open | No | none | Scope decided and selected outcomes classified by real execution or retained by dated decision; [detail](#m1---negative-control-harness-outcome-classification) |
| M2 | Predictable acquisition performance and second-client impact | Carry-forward | Deferred | No | D2 | Existing performance criteria met on their identified scope or changed by an explicit decision; observed failures remain; [detail](#m2---predictable-acquisition-performance-and-second-client-impact) |
| M3 | Secret material management for SNMPv3 device access | Carry-forward | Open | No | none | Creation, storage, rotation and exclusion policy decided; procedure and scans verified; [detail](#m3---secret-material-management-for-snmpv3-device-access) |

Current work tally: 0 Complete, 0 In progress, 1 Deferred and 2 Open. No work row is Ready. Backlog contains 2 Deferred rows and is excluded from this tally.

### Decisions

| ID | Decision | Decision Date |
| --- | --- | --- |
| D1 | Defer the unexecuted Channel Access, long active client-write, delayed output-simulation and isolated rebuild/reuse cells. They do not qualify the accepted record scope and resume only by a new decision. | 2026-10-05 |
| D2 | Defer performance execution without an acceptance exception. Retain the once result, failed sustained criterion, baseline variation, lost probe and historical delay question. Resumption requires a bounded product question, acceptance criterion and stop condition; tool changes and new equipment runs are not automatic. | 2026-11-06 |
| D3 | Retain Linux SONAME and symbolic-link versioning as long-term SNMP3 Backlog, excluded from current execution. No implementation dependency or ABI number is assigned before inventory. | 2026-10-24 |

### Milestone Details

#### M1 - Negative-control harness outcome classification

Origin: 6f015f8 / M1
Identity History: none
GitHub Issue: none
Status: Open (unresolved scope; held for discussion by the decision of 2026-10-04)

##### Summary

`tests/rewrite/test_record_controls.py` qualifies a control only when its reference passes and the defective run fails without aborting. A record run killed at its child bound or a stop-queued run without its `stop_queued` event qualifies neither. Several other outcomes have not been classified or executed on the real path.

##### Scope

- Decide whether qualification cells apply the same forced-cleanup rule as record cells. The 2026-10-04 `binding-lookup-qualification` IPv4 defective copy recorded `forced_cleanup=true` in `behind-retirement-4/qualification.receipt.json`, while its reference recorded false.
- Decide how to record a cell when the 900 s `run_cell` bound raises and ends the entire control run.
- Classify stop-queued `rejected`, `timeout`, inconclusive, forced-kill and missing-event outcomes, previously checked by code reading only. A sanitizer abort after the event is currently judged on product checks alone.
- Decide whether to surface the trial failure line, currently only in `records.stderr` among loader diagnostics; `stderr_tail` comes from the harness stderr.
- Decide whether SchedulerTest controls name the failing check instead of only its position, which must be counted in `tests/rewrite/SchedulerTest.cpp`.

Out of scope: product changes and the stop-queued wait rule in `docs/snmp-worker-supervision.md`.

##### Completion Criteria

- A dated decision fixes which outcomes qualify, abort or are judged.
- A dated decision fixes whether to surface the trial failure line and SchedulerTest check names.
- Each active outcome is classified by an executed real-path run, or its absence is recorded by decision.

##### Dependencies And Decisions

- Scope and priority remain unresolved. The 2026-10-04 hold remains in effect; this reset does not authorize implementation.

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
| T1 | Not run | Linux host, sanitizer products | Pending | [Retained scope and observations](https://github.com/jeonghanlee/snmp3/blob/6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf/docs/milestone-5dff352.md#m10---negative-control-harness-outcome-classification); source reading is not real-path classification |

##### Closure Evidence

- none

#### M2 - Predictable acquisition performance and second-client impact

Origin: 6f015f8 / M2
Identity History: none
GitHub Issue: none
Status: Deferred (D2, Decision Date: 2026-11-06)

##### Summary

Remove avoidable software delay and serial-request cost while preserving request ownership and avoiding unnecessary delay to another client. Readiness wakeups and declared-group dispatch are implemented; their current contracts are in `docs/snmp-worker-supervision.md` and `docs/snmp-worker-reference.md`. The remaining performance objectives are not accepted as complete. The [committed performance scope, accepted plan and full evidence](https://github.com/jeonghanlee/snmp3/blob/6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf/docs/milestone-5dff352.md#m14---group-record-requests-and-wake-on-readiness-on-the-record-path) retain the exact historical case definitions, product identities and receipts.

##### Scope

- Preserve matched complete-pass comparison: same objects, group size, tier cut and order, and one request in flight per side, on the required device classes.
- Preserve sustained schedule and second-client criteria separately. Diagnose baseline variation, one lost probe and the historical 103 ms difference only after a bounded resumption decision.
- Preserve request ownership, deadlines, joint failure and retirement behavior if any further product change is selected.
- A strict legacy latency/CPU/freshness comparison remains unqualified. No numerical CPU target is introduced by the deferral or this reset.

Out of scope: SET timing, other device models, changing Net-SNMP, an automatically selected cache redesign, new tools or repeated equipment measurements without a new decision.

##### Completion Criteria

- A new dated decision defines the concrete product question and bounded work before resumption.
- Fixture and device complete-pass criteria remain at most 1.5 times direct under matched conditions. Fixture 0 ms ratios are reported, not judged; 20 and 100 ms ratios are judged.
- Each required device class satisfies the 900-second sustained schedule: zero overruns and lost requests below 0.5 percent.
- The normalized second-client ratio `(module during p95 / module idle p95) / (direct during p95 / direct idle p95)` is at most 1.25. A fresh 60-second idle phase is used on each side. A valid first pair in inclusive 1.15 to 1.35 requires exactly one repeat and the specified median of the two ratios. Invalid evidence cannot be averaged into a pass.
- Existing record, lifecycle, supervisor and qualification checks pass on any changed product. The detailed group, wake, timer and wire criteria remain those of the committed accepted plan.
- A missed complete-pass bound may be accepted only by an explicit dated exception while sustained criteria pass. A sustained or required correctness failure returns to planning and is not closed by that exception. The current deferral grants no exception.

##### Dependencies And Decisions

- D2 keeps execution Deferred. Earlier implementation and diagnostic authorizations do not resume work automatically.
- Declared groups coalesce at dispatch under the earliest member deadline, with quiet gap 0.5 ms and hold bound 2 ms; ungrouped bindings retain exact equal-deadline dispatch. These delivered contracts are not new implementation work.
- The database repository owns application database changes. Generated358 performance observations do not establish final396 or legacy163 equivalence.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-10-10, accepted readiness/grouping plan at `131842c`, with the subsequently recorded scope and execution decisions preserved in the committed performance evidence linked above
Implementation Authorization: historical 2026-10-10 product/test/document authority is suspended for further execution by D2; new bounded direction and separate equipment authority are required before resumption
Superseded Plan Artifacts: none

1. Start with existing original captures, identified tools and receipts. Check input availability before proposing replay; do not reconstruct missing evidence.
2. Define one remaining product judgment and its stop condition. Keep successful-response statistics separate from unsuccessful requests and collection completion.
3. Obtain the resumption decision before product changes, tool changes or measurements. A changed criterion needs its own explicit decision; a tool change needs a specific demonstrated obstacle to the product judgment.
4. Verify any authorized change through the real products and fixtures using the required checks below. Do not repeat until pass or relabel historical results.

##### Test Plan

The [committed test definitions and acceptance numbers](https://github.com/jeonghanlee/snmp3/blob/6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf/docs/milestone-5dff352.md#m14---group-record-requests-and-wake-on-readiness-on-the-record-path) retain the detailed requirements. No execution is authorized by this table.

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Fixture throughput | Real module `--once` and direct multi-object pass, K28, tiers 38/18/302, delays 0/20/100 ms, sequential chunks in priority order | Identified Linux products and external fixture | Ratios at 20/100 ms at most 1.5; 0 ms reported only; preserve actual types and fixture limits |
| T2 | Device complete pass | Five module passes and five direct passes in the same session with identical workload and cuts on each required device class | Identified ordinary products and authorized equipment | Median module/direct at most 1.5; no stop rule fired; record applicable equipment authority and conditions |
| T3 | Sustained load and another client | Real 900 s module/direct loads, fresh idle60 per side, state/measurement/slow periods 5/10/60 s, existing native probe; preserve 900/890 s probe-window difference | Identified ordinary products and authorized equipment | Zero overruns, loss below 0.5 percent, normalized ratio at most 1.25; conditional repeat only as specified |
| T4 | Regression | Actual record, shutdown, startup, CA, controls, lifecycle, supervisor, qualification, native, component, operator and configuration runners on any changed products | Linux host, ordinary and instrumented products | Required cases pass with their real fixtures and recorded limits |
| T5 | Admission, wake and timers | Existing qualification and control cases for wakeups, callback retry, grace, partial frames, child reaping, stop/reuse, group membership, holds, deadlines and charged ownership | Linux host and external fixture | Preserve all current requirements and controls; the historically retired failure-loop case is not reactivated |
| T6 | Group wire behavior | Actual fanout/group database and coinciding tiers through the external proxy; inspect real request events, timeout and expired-member cases | Linux host, identified ordinary products | Existing burst/gap bounds, request counts and shared outcomes hold; report split groups and expired members separately |

##### Verification Results

No test was rerun during this reset. These summaries retain their original products and scope; exact receipts remain in the committed performance detail.

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-13 | Ordinary `e812d4c`, grouped K28, integer-valued loopback fixture | Passed at judged delays | Module/direct ratios 1.11 at 20 ms and 1.03 at 100 ms; 0 ms 2.6 reported only. Not a mixed-type device result. |
| T2 | Historical 2026-10-28/29; ordinary PDU2 2026-11-06 | Historical diagnostic products; current ordinary `fff3ab44a8ef5a64f9e509eeb444da07a3ee7063`, installed Base 7.0.10, generated358 K28 | Historical PDU1 Passed and PDU2 Failed; current ordinary PDU2 Passed | Current medians 212.344/173.300 ms, ratio 1.225297 against 1.5. Historical PDU1 1.014294, PDU2 1.690982. PDU1 not rerun; missing historical PDU2 capture not replayed. |
| T3 | 2026-11-06 | Ordinary PDU2 generated358 K28, idle60 per side and load900 each | Schedules Passed; normalized pair Failed, no Incomplete | Ratio 1.98365581442335 against 1.25. Both sides complete 180/90/15 tier cycles and 615 processed/sent requests, loss/overruns zero. No conditional repeat. Original/replay result agrees; replay success does not make this a performance pass. |
| T4 | 2026-10-10 to 2026-10-13 | Historical readiness/grouping products | Recorded regression executions retained | Exact source/product identities, case counts and control outcomes remain in the committed results; no transfer to a new build is claimed. |
| T5 | 2026-10-11 to 2026-10-13 | Historical ordinary/instrumented products and fixture | Partial recorded coverage | Preserve the committed case-by-case observations and limits; no blanket Passed result. |
| T6 | 2026-10-13 | Ordinary `e812d4c`, fixture/proxy, grouped K28 | Passed for recorded wire-count and failure parts | Five passes at each delay have 70 requests; sustained coinciding tiers have 96 requests; no observed group split, loss or overrun. This is not device qualification. |

Retained evidence needed to resume:

- Once: `work/m14-preparation-20261106-01/`; `equipment-PDU2-manifest.json` SHA-256 `a2ad2434d099019e4de2af14a6d461f88f6761ebcfc03f4d2af8c2f92db4573a`, evaluation SHA-256 `97ac4d385fa2ca34b8ab18dc7caa51affea970812dc3cae416a03f216c9b9114`.
- Sustained: `work/m14-sustained-preparation-20261106-01/`; source manifest SHA-256 `6c3f6583f6da49c4cb45b4f7e30d2099f7752308cc8f2da4f34f5336c0985570`, replay result SHA-256 `84d849e3ff2dfad1ffc17bace23bffdd3072c0f26e6ad265d6c8116c751aa01a`. Successful probe idle/during p95 values are module 8.630261/12.631567 ms and direct 21.533289/15.888297 ms; different ordered windows do not establish causality.
- Order/idle diagnostic: `work/m14-order-diagnostic-20261106-01/`; source manifest SHA-256 `92bf76fdcbc3d6336af6144b06c1076d9f882d68dec557667f328e70f3349888`, replay result SHA-256 `8c139fe888b0d49d23ffd1afd44a2c955f210b50d27f5469575d25dc6e14c2b8`. Short diagnostic ratios 1.879967 and 0.509453 show baseline sensitivity and are not acceptance repeats. One probe was lost after 3003.281538 ms; its exact native failure category and cause are unknown. Concurrent module completions do not identify its cause.
- Historical 103 ms attribution and strict legacy latency/CPU/freshness remain unresolved. Existing evidence availability must be checked before reuse; this reset does not assert that local payloads still exist.

##### Closure Evidence

- none; D2 is a deferral, not completion or a performance exception.

#### M3 - Secret material management for SNMPv3 device access

Origin: 6f015f8 / M3
Identity History: none
GitHub Issue: none
Status: Open (unresolved scope: where secret material lives and how it is created and rotated)

##### Summary

The module reads each community or passphrase from its own file referenced by configuration. The file must be owned by the IOC user with no group or other permissions. Existing deployments keep passphrases in an older single-file format inside project directories. A creation, storage and rotation design is needed that fits the module's raw passphrase files and keeps secrets out of shared repositories and history.

##### Scope

- Decide host locations, who creates secret files, their source and conversion from older files.
- Decide rotation given that credentials are replaced only with a new process.
- Define repository and history exclusion and the scans that verify it.

Out of scope: an external secret store unless selected, and changing configuration-loader file checks.

##### Completion Criteria

- A dated decision covers location, creation, rotation and repository exclusion.
- A procedure is documented without real addresses, user names or secrets.
- Tracked-tree and history scans of the affected repositories find no secrets, or findings have an explicit disposition.

##### Dependencies And Decisions

- Scope and policy remain unresolved; no storage or rotation approach is selected by this reset.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Decide storage and rotation.
2. Write the procedure and an older-format converter only if selected.
3. Run scans and record actual results.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Tracked tree and history | Scan the selected repositories' tracked files and history | Linux host | No secret material, or findings with disposition |
| T2 | Startup | Create files by the procedure and start the real IOC | Linux host | Accepted configuration; secrets absent from diagnostics |
| T3 | Permissions | Existing wrong-owner and wrong-mode configuration cases | Linux host | Rejected as before |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host | Pending | none |
| T2 | Not run | Linux host | Pending | none |
| T3 | Not run | Linux host | Pending | Historical configuration cases do not qualify the unselected creation/rotation procedure; wrong-owner coverage retains its identified fixture limits |

##### Closure Evidence

- none

## Backlog

### Work

| ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| M4 | Deferred record verification cells | Carry-forward | Deferred | No | D1 | Real-path receipts on identified ordinary/instrumented products or dated retirement; [detail](#m4---deferred-record-verification-cells) |
| M5 | Linux shared-library SONAME and symbolic-link versioning | Milestone | Deferred | No | D3 | ABI policy, installed names/links and real consumer upgrade compatibility verified; [detail](#m5---linux-shared-library-soname-and-symbolic-link-versioning) |

Backlog tally: 2 Deferred. Neither item is assigned for execution.

### Backlog Details

#### M4 - Deferred record verification cells

Origin: 6f015f8 / M4
Identity History: none
GitHub Issue: none
Status: Deferred (D1, Decision Date: 2026-10-05)

##### Summary

The cells below were excluded from accepted record qualification and remain unexecuted. Their [committed definitions and exclusion decision](https://github.com/jeonghanlee/snmp3/blob/6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf/docs/milestone-5dff352.md#m11---deferred-record-verification-cells) retain the original conversion and ownership requirements.

##### Scope

- Channel Access signed-minimum int64 access for int64in/int64out, maximum-capacity lsi/lso `VAL$` access, and ao/longout writes.
- Long active client writes, whose exact cell definition must be fixed before execution.
- Mode change of delayed output simulation.
- Isolated rebuild/reuse beyond the existing `rebuild` clause: delayed or abandoned queued completions, lsi/lso pointer-backed buffers and retained maximum-capacity payloads.

Out of scope: other accepted record cells. Short active writes or controlled retirement cases do not qualify these deferred cells.

##### Completion Criteria

- Each listed cell has a real-path receipt on identified ordinary and instrumented products, or is retired by a dated decision.

##### Dependencies And Decisions

- D1 excludes these cells from the accepted record qualification. A new scope decision is required before execution.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Define each cell against shipped records, Channel Access and agent fixtures; name the failing check.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Channel Access | Real production IOC and Base CA clients: signed minima, maximum capacity and ao/longout writes | Linux host, ordinary and instrumented products | Values and alarms meet the retained conversion requirements |
| T2 | Active writes and simulation | Long active client writes and delayed output-simulation mode changes against a delayed SET | Linux host, ordinary and instrumented products | Completion, alarms and ownership meet retained record policy |
| T3 | Isolated rebuild | Rebuild isolated databases with delayed/abandoned completions, lsi/lso buffers and maximum-capacity payloads | Linux host, ordinary and instrumented products | No post-detach access; ownership released as required by the existing `rebuild` case |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary and instrumented products | Deferred | none |
| T2 | Not run | Linux host, ordinary and instrumented products | Deferred | none |
| T3 | Not run | Linux host, ordinary and instrumented products | Deferred | none |

##### Closure Evidence

- none

#### M5 - Linux shared-library SONAME and symbolic-link versioning

Origin: 6f015f8 / M5
Identity History: none
GitHub Issue: #1 - https://github.com/jeonghanlee/snmp3/issues/1
Status: Deferred (D3, Decision Date: 2026-10-24)

##### Summary

Give SNMP3 shared libraries on Linux explicit ABI-versioned SONAMEs and reproducible symbolic-link installation. Distinguish the real versioned file, runtime SONAME and unversioned consumer link name.

##### Scope

- Inventory `snmp3`, `snmp3Wire`, `snmp3Native`, their consumers and retained compatibility names; define the stable ABI boundaries.
- Define compatible/incompatible SONAME changes independently of project release numbers.
- Implement versioned output and symbolic-link creation through EPICS build/install, with `libname.so -> libname.so.<ABI> -> libname.so.<full-version>` for the selected policy.
- Verify installation, upgrades and loading for the IOC, native probe and worker, including actual loaded-library identities.

Out of scope: changing current test products, conditioning existing qualification on this work, a policy for all EPICS modules, and device behavior. This record does not authorize implementation, deployment or release.

##### Completion Criteria

- Each library has an ABI policy, intended ELF SONAME, versioned real file and correct links after normal build/install.
- Actual consumers record intended SONAME dependencies and load the installed libraries; real IOC and worker paths succeed.
- A compatible replacement preserves an already linked consumer. An incompatible ABI uses a distinct SONAME and does not silently replace the old ABI.
- Repeated installation and the package/distribution path preserve valid links and targets. Migration from unversioned libraries is documented and tested.

##### Dependencies And Decisions

- D3 retains this as long-term Backlog. Inventory precedes selecting an ABI number or assigning implementation dependencies.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Inventory exports, consumer dependencies, names and applicable EPICS build/install rules.
2. Propose compatibility/version rules and migration; obtain plan acceptance and implementation authority.
3. Apply the selected build/install policy, consumer identity checks and documentation.
4. Execute actual build, install, consumer and upgrade checks before release.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Build/install | Build and install actual libraries, inspect ELF SONAME/NEEDED, real files and links, then repeat installation | Supported Debian and Rocky Linux build environments | Names, dependencies and links match policy without dangling targets |
| T2 | Consumer runtime | Real installed IOC, probe and worker against shipped loopback fixture; inspect loaded-library identities | Supported Debian and Rocky Linux runtime environments | Intended libraries load; actual acquisition and shutdown succeed |
| T3 | Upgrade/migration | Already linked consumer after compatible replacement; distinct incompatible SONAME; unversioned migration | Isolated installed Linux trees | Compatible consumers work; incompatible ABIs remain distinct; migration/package installation preserve links |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Supported Debian and Rocky Linux build environments | Pending | none |
| T2 | Not run | Supported Debian and Rocky Linux runtime environments | Pending | none |
| T3 | Not run | Isolated installed Linux trees | Pending | none |

##### Closure Evidence

- none

##### GitHub Projection

Title: Version SNMP3 shared libraries with ABI SONAMEs
Labels: enhancement
GitHub Milestone: none
Observed State: open
Observed Labels: enhancement
Observed Milestone: none
Last Compared: 2026-10-24T18:47:50Z; issue updatedAt 2026-10-24T18:47:50Z; title, body, label, assignee jeonghanlee and absent milestone verified with `gh issue view 1 --repo jeonghanlee/snmp3`. Historical external observation; not rechecked during reset.

## History

| Reset Date | Prior State Commit |
| --- | --- |
| 2026-11-07 | 6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf |
