# snmp3 Work Register

Release line: unversioned snmp3 rewrite
Milestone index: 5dff352
Canonical path: `docs/milestone-5dff352.md`
Canonical branch or ref: `feature/snmp-base-7.0.10-rewrite`
Git upstream: none configured at initial creation
Remote tracker: none associated with this register
Recorded date: 2026-10-01
Source baseline: `5dff352b9e86abfca74a74c4adc4fc8c1e48079f`

Next session entry point: `docs/milestone-5dff352.md`, M6 Implementation Plan, step 5: complete the remaining T1-T14 record qualification, starting with active CA/source-switch cases and isolated reuse/in-flight non-isolated shutdown. Numeric GET/SET pair coverage, range/precision/extrema, actual CA decimal/long-string/PINI, active dbPutField/RPRO, retry and output policy/simulation subset results are recorded below. Numeric exception/type-error and additional failure-after-success cases remain pending. Eleven DSETs, bounded terminal delivery, callback retry and shutdown gates are implemented. G1 retains plan acceptance and separate implementation authorization dated 2026-10-01. Full M6 acceptance, record advertisement and closure remain pending.

## Scope

This register owns the R1-R8 rewrite checkpoints and the assigned mdBook documentation work. M1-M8 retain their original R1-R8 scope names; M9 is the documentation addition. M, G and D identifiers are local to this canonical document. Architecture and operator contracts remain in their existing documents.

Out of scope: changing implementation or installed dependencies through this documentation update, operating equipment, publishing a site, or changing Git history or remote state.

Complete means the identified checkpoint scope and its qualified verification are complete. It does not close later integration requirements. R6 implementation is authorized and in progress; R7-R8 and mdBook implementation have not started. Their Blocked status records the missing detailed-plan acceptance and separate implementation authorization; planning may proceed before those gates close. Ready is an execution dependency indicator, not implementation authorization.

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
| M5 | R5: address queues, IPC and worker supervision | Milestone | Complete | No | M3, M4, D3 | Actual isolated worker, deadline, recovery and IOC qualification; [detail](#m5---r5-address-worker-supervision) |
| M6 | R6: record DSET, conversion, completion and FLNK | Milestone | In progress | No | M1, M2, M3, M4, M5, D5, D6, G1 | Real record/wire order, values, alarms and shutdown verified; [detail](#m6---r6-record-integration) |
| M7 | R7: APC startup/DB migration and comparison | Milestone | Blocked | No | M6, D1, G2 | Actual v2c/v3, CA/PVA, comparator and nested startup error observations; [detail](#m7---r7-apc-migration-and-comparison) |
| M8 | R8: integrated two-OS and resource qualification | Milestone | Blocked | No | M6, M7, G3 | Full identified-source matrix and measured resource acceptance; [detail](#m8---r8-integrated-qualification) |
| M9 | mdBook documentation | Milestone | Blocked | No | M1, M2, M3, M4, M5, D4, G4 | Curated book builds and rendered navigation/links match current behavior; [detail](#m9---mdbook-documentation) |
| G1 | R6 detailed-plan acceptance and execution authorization | External gate | Complete | No | none | Explicit acceptance and separate authorization recorded; [detail](#g1---r6-authorization) |
| G2 | R7 detailed-plan acceptance and execution authorization | External gate | Open | No | none | Target, comparator and permitted equipment scope approved; [detail](#g2---r7-authorization) |
| G3 | R8 detailed-plan acceptance and execution authorization | External gate | Open | No | none | OS environments, instruments and resource acceptance policy approved; [detail](#g3---r8-authorization) |
| G4 | mdBook detailed-plan acceptance and execution authorization | External gate | Open | No | none | Layout, toolchain, build and publication boundary approved; [detail](#g4---mdbook-authorization) |

### Decisions

| ID | Decision | Decision Date |
| --- | --- | --- |
| D1 | Use a new explicit configuration/record interface and deliberate APC DB/startup migration; retain the legacy implementation only as a comparator. | 2026-09-29 |
| D2 | Use snmp3 for the active module, libraries, executables, commands and fixtures. | 2026-10-01 |
| D3 | Batch only compatible contiguous FIFO requests with exactly equal absolute deadlines; different deadlines remain separate FIFO work. | 2026-09-30 |
| D4 | Assign mdBook documentation as a separate milestone, using the existing epics-ioc-runner and epicsarchiverap-maven examples as planning inputs. | 2026-10-01 |
| D5 | Use IEEE 754 roundTiesToEven for ao double-to-OpaqueFloat SET conversion; permit the resulting finite binary32 rounding without decimal pre-rounding. | 2026-10-01 |
| D6 | Reject record conversion precision loss and capacity overflow as errors, except the ao-to-OpaqueFloat rounding permitted by D5; preserve prior input data on failure and send no invalid SET. | 2026-10-01 |

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
Status: Complete

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

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: approved R5.1-R5.6 plan, SHA256 `f4209d83308b3518c70506a1dd0785276461ade07689bb584463ec98f45bbc38`
Implementation Authorization: recorded R5 execution authorization, 2026-09-30
Superseded Plan Artifacts: original R5 draft replaced by the identified approved corrected plan

1. Enforce checked frame, admission/storage/identity, FIFO and exact-deadline batch rules in the shipped components.
2. Own one worker per canonical address, verify actual native/executable identities, retain unsettled ownership and recover only after exact reap.
3. Qualify the real shipped process/native path under external faults and instrumented execution.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Components | Actual IPC/Scheduler/Supervisor/Runtime test executables. | Normal Linux x86_64 products | Checked bounds, one terminal and joint ownership release. |
| T2 | Worker/native integration | Shipped qualification runner uses real processes and agents with outer-boundary faults. | Linux x86_64 | Peer isolation, exact FIFO/deadline behavior and no ambiguous SET replay. |
| T3 | IOC and operator | Actual IOC lifecycle and documented production commands. | Base 7.0.10 | Pre-PINI servicing, exact stop/join/reap and correct command behavior. |
| T4 | Instrumented integration | Separate module/wire/native/worker/IOC products run the actual matrices; live worker identity is observed. | ASan/UBSan new code; leaks disabled | No sanitizer diagnostics on the qualified paths. |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01, retained execution | Normal component products | PASS, 14 executions | `work/name-change-20261001/components/results.json` |
| T2 | 2026-10-01, retained execution | Real worker/native/agent path | PASS, 26 cases | `work/name-change-20261001/qualification/results.json`; SHA256 `3bf4d00cd34f3ce2a78fac083b1e2e5d2581c5fe88a769df9dd27e6aa3d61de8`; individual case receipts identify executed products |
| T3 | 2026-10-01, retained execution | Actual IOC and production commands | PASS, 25 IOC and 15 operator checks | `work/name-change-20261001/ioc/results.json`, `operator/results.json` |
| T4 | 2026-10-01, retained execution | Separate instrumented products | PASS; 26 qualification cases, component/IOC matrices and 6 live workers | `work/name-change-20261001/sanitizer/results.json`, `sanitizer-components/results.json`, `sanitizer-ioc/results.json`, `live-worker-ioc/spawn-audit.json`; each run retains its own product identities |

##### Closure Evidence

- Accepted runtime/tests are committed in `49d5c62`; worker/operator/decision documents in `81cdcdd`. Current evidence establishes the R5 component boundary and preserves the explicit verification limits above.

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

##### Dependencies And Decisions

- M1-M5 are behavioral dependencies: the new DSET must use their configuration ownership, admission-origin deadlines, native adapter, worker isolation and retirement rules. Their existing receipts do not qualify record I/O.
- G1 closed on 2026-10-01 after separate plan acceptance and implementation authorization. Authorized implementation is in progress; required real record qualification remains pending.
- D1 requires an explicit new record/configuration interface; D2 fixes the snmp3 product identity. D3 remains unchanged: equal configured budgets do not imply equal absolute deadlines or permit a new batching rule.
- Record-scope decision, 2026-10-01: include Base 7.0.10 int64in/int64out alongside the existing seven record types. This adds exact signed 64-bit scalar I/O. D5/D6 separately define the conversion policies; implementation authority is recorded below and in G1.
- Long-string scope decision, 2026-10-01: add Base 7.0.10 lsi/lso for bounded long-string input/output, including SIZV/LEN handling and full-length client access. Keep stringin/stringout for the existing short-string interface. D6 defines capacity-error handling; implementation authority is recorded below and in G1.
- D5 resolves ao double-to-OpaqueFloat SET rounding as IEEE 754 roundTiesToEven, dated 2026-10-01. This decision selects one conversion policy; it is not acceptance or execution authorization for the complete M6 plan.
- D6 resolves the remaining conversion precision/capacity-loss policy as error rejection, dated 2026-10-01. Except for D5, do not round, truncate, wrap or saturate an otherwise unusable record conversion. Preserve prior input data and send no invalid SET. Integer-to-floating input loss, waveform FLOAT narrowing and string/array overflow are covered. No conversion-loss policy choice remains pending; complete-plan acceptance and implementation authorization are recorded below and in G1.
- Waveform state decision, 2026-10-01: follow Base's device-support-owned BUSY contract. snmp3 manages BUSY as FALSE for complete single-varbind publication; do not introduce an initial-BUSY rejection policy. Complete-plan acceptance and implementation authorization are recorded below and in G1.
- Interface spelling, conversion matrix, alarm mapping and callback-drain behavior below are accepted implementation requirements, not claims of implemented or verified product behavior. A later plan revision requires new acceptance and implementation authorization.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-10-01; current M6 plan accepted, reviewed document SHA256 `cb80ae8c978ad5b06e4ae046ca4ac648f5f29634c13e3e5709c6096f6c77eecb`
Implementation Authorization: 2026-10-01; implement the same accepted M6 plan, including eleven-record DSET/conversion/completion/shutdown, shipped T1-T14 fixtures and required documentation; retain M6 exclusions
Superseded Plan Artifacts: none

Plan date: 2026-10-01

###### Premise And Source Checks

Confirmed finding: `snmp3App/src/snmp3.dbd` registers a registrar only, and `Runtime.h` has no record completion API. `Scheduler.cpp` already provides `take()` and `release()` for a borrowed, owned terminal result. Therefore terminal-to-record delivery is a required new connection, not evidence that M5 is incomplete within its qualified scope.

Confirmed constraint: `Scheduler::admit()` permits one retained generation per handle and budgets of 1-600000 ms. `release()` marks consumption; collection still requires native retirement. A record that has cleared PACT can encounter retained ownership on its next admission and must report rejection without waiting or silently replaying the command.

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

Proposed alarm mapping: invalid binding/configuration uses LINK/INVALID; local admission or conversion failure uses READ/INVALID or WRITE/INVALID by operation; transport, deadline, worker loss and Stopping use COMM/INVALID; native/protocol/exception failures use READ/INVALID or WRITE/INVALID. Apply alarms with recGblSetSevr, preserving Base severity priority and equal-severity behavior. Base remains responsible for record limit/UDF alarms and monitor events. Stage terminal failure alarms before invoking record support so IVOA cannot suppress the error report by skipping DSET.

###### Completion And Ownership Contract

First-pass DSET runs under the record lock, validates the frozen context, converts/captures the output if needed and attempts immediate admission. A rejection is synchronous: set the operation alarm and return an error with PACT left inactive. On success, publish the exact identity into the context, set PACT and return promptly. No DSET waits for IPC/native I/O, joins a thread or polls for queue space.

Runtime servicing visits registered active contexts after supervisor progress and during stop. It borrows a selected terminal exactly once through the existing scheduler owner. Retain only the borrowed view and fixed callback/context metadata; keep the terminal payload in its charged scheduler storage. Any decoded conversion scratch has a checked, bounded allocation included in the record-context/storage accounting, without a second accumulating result queue.

Maintain explicit callback states for pending request, accepted queue entry, running and drained. Reserve queue/running ownership before callbackRequest so an immediately executing callback cannot race publication; on enqueue failure, retain the terminal/context and retry from module servicing without native replay. Allow at most one accepted callback per record generation. Retry work is bounded by registered contexts and must not busy-spin, monopolize another address, or reset deadlines. Account the new fixed record/context storage against the existing registration bound and include any changed reservation formula in the documentation and regression checks.

The custom Base callback retains its context and obtains a lifetime/entry lease through the module completion-entry gate before fetching a record pointer or calling dbScanLock. The gate grants entry and increments its entered-processing count atomically. Release the gate mutex before fetching/locking the record; never hold it across dbScanLock, FLNK, terminal release or a callback wait. A callback that loses the race to gate closure remains inert, does not access record storage, and accounts only its own ownership. An already granted lease covers callbacks waiting for the record lock as well as executing record support.

After obtaining the lease and dbScanLock, verify the record/context lifetime, configuration revision, activation, frozen field identity, handle and exact generation/admission, and require the matching active PACT phase. A frozen-field mismatch follows the binding-error completion rule above; an invalid/stale generation never applies data to another generation.

For an entered callback, prepare checked input data or stage the terminal failure, then call the record's rset process as Base ProcessCallback does. Keep checked native input in staging until the matching DSET completion branch publishes it; the wrapper does not publish input before record support chooses the value source. DSET's completion branch consumes only that staged generation and never admits another request. Base runs monitors, FLNK, RPRO handling and PACT clearing. An entry lease covers record processing, terminal finalization and the last record access, and decrements the entered-processing count only after dropping the record lock.

The wrapper finalizes terminal consumption even if output IVOA, a live OOPT change, or simulation bypasses completion DSET. Validate these paths explicitly; do not depend on a second DSET call to release the result or report its error. Release the terminal and finish generation accounting after record processing, before dropping the record lock, so Base's queued RPRO cannot race the old terminal release. Native ownership may still prevent the next admission; report that rejection rather than adding implicit replay. An invalid/stale context never touches new record fields, and retires only its own borrowed ownership.

For an accepted GET whose completion uses input simulation, finalize the owned native terminal once even though no completion DSET publishes it. Stage native/conversion failure alarms before record processing, preserve Base alarm priority, and keep native outcome separate from SIOL outcome in the receipt. A simulation-only value must not update the context's successful-native-publication flag. No completion path starts a replacement GET. Base's delayed simulation callbacks are not module accepted generations: PACT alone does not prove ownership, and a DSET completion entered without a matching module generation must report the missing ownership without native admission. T8 observes simulation before admission, a live switch during delayed native completion, and return to normal mode, including SDLY-driven Base callbacks.

Require longout OOPT Every Time; initialization reports an unsupported setting and leaves no usable binding. Check runtime changes before admission and surface an invalid setting during completion even if conditional_write skips DSET. ao/int64out/stringout/lso have no OOPT; exercise their IVOA branches. For lso, distinguish its short IVOV field from the long VAL buffer and observe Base's first-pass IVOV copy/termination rather than imposing a new module policy. A value put through Base while active may request RPRO, while repeated PROC/scan activity is not a promise to preserve every intermediate value. A failed or ambiguous SET has no new module retry; an explicit subsequent operator request is separate from the native library's configured retry behavior.

###### Startup And Shutdown Contract

Register all DSETs and initialize contexts before AfterFinishDevSup. That hook freezes configuration and starts the existing Runtime before PINI. Record initialization performs no device I/O. Startup/service failure closes admission and is visible to Main and record errors, not a fabricated ready state.

For snmp3Stop and AtShutdown, close record admission first, preserve selected terminals, select Stopping for remaining work, and service terminal callbacks while Base record locks/callbacks/links are alive. Coordinate supervisor stop/join, callback retry/drain and exact worker reap as separate ownership obligations. Refactor the stop orchestration so it holds neither a record lock nor an operation mutex needed by callbacks while waiting for progress or joining; retain idempotent/concurrent stop and the M5 helper-stop bound.

Successful callback drain requires no module callback pending/queued/running and no active accepted record generation. Its completion uses Base FLNK with admission already closed; downstream attempts fail without sending new native commands. A fanout or cyclic external DB is not a shutdown completion barrier.

Use a proposed 2000 ms callback-drain attempt budget, measured separately from native stop/reap. On success or expiry, stop module callback producers and atomically close the completion-entry gate. On expiry, record unsuccessful drain and forbid successful restart. A queued callback without an entry lease can no longer access record storage once the gate closes; retain its context/storage and borrowed terminal ownership until actual exit or proven queue destruction. A lease granted before closure remains counted, including while waiting for dbScanLock, until that callback has finished its final record access and unlocked.

Wait for the entered-processing count to reach zero before returning from AtShutdown and allowing link closure. Use an event/condition wait that holds no record lock, servicing lock or callback-needed operation mutex. Gate closure cannot cancel a callback already inside rset process. This safety wait is separate from the 2000 ms attempt budget; if external record processing does not return, shutdown must remain waiting with a reported external limitation rather than proceed to close links/free storage. No claim of a bounded total IOC shutdown is made.

After that safety wait, AtShutdown establishes detach permission and returns. During doCloseLinks, shutdown del_record detaches record pointers while retaining callback contexts. AfterStopCallback proves that Base callback threads are joined, not that every queued callback ran. For an isolated shutdown, callbackCleanup destroys the queues before AfterShutdown; finalize abandoned queue entries/borrowed terminals only once that destruction is established, without any record access. For a non-isolated shutdown without queue cleanup, retain any still-referenced inert callback contexts. Never use the callback-join hook alone to free queued storage. Context retention and the completion-entry gate jointly prevent post-detach record access; retaining a context alone cannot keep a freed Base record alive. Retain scheduler/native ownership as required by M5 and keep unsuccessfully drained activations ineligible for restart.

The Base init hook is void and cannot veto database free. The module therefore proves absence of entered record processing before AtShutdown returns, prevents new entry before links close, and delays callback-storage release until Base ownership ends. Use real lifecycle tests to verify these distinct boundaries; do not repair installed Base or label expired drain as successful completion.

###### Ordered Implementation And Closing Checks

These steps are executable only after G1 closes. New file names below are proposed; current shipped files remain unchanged by this planning operation.

The numbered steps define implementation order. Their closing checks are final verification obligations, not prerequisites for starting the next implementation step. Steps 1-4 can proceed while their integration checks remain pending; those checks close only after the required DSET, lifecycle support and shipped test products/fixtures are available through step 5. Run the real integrated path for final verification, without replacing missing internal components. M6 remains incomplete until every required T1-T14 check satisfies its completion criteria.

1. Add `snmp3App/src/Conversion.{h,cpp}` and an immutable record-binding definition in `Request.h`; implement the accepted matrix, loss policy, grammar validation and bounded conversion staging. Final verification after step 5: T1-T5 and the defective-conversion controls in T14.
2. Add `snmp3App/src/Request.{h,cpp}` and narrow terminal-delivery hooks in `Runtime.{h,cpp}`; use Scheduler take/release, full identity, fixed context accounting and reliable callback retry. Keep worker/native production code and D3 scheduling semantics intact. Final verification after step 5: T6, T9-T12 and targeted ownership controls in T14.
3. Add `snmp3App/src/DeviceSupport.cpp`; export fresh DSETs for all eleven records, direct ai values, OVAL-based ao writes, exact int64in/int64out VAL access without a double intermediate, bounded lsi/lso VAL/LEN handling and payload capture, output policy checks, record-specific UDF/alarm behavior, Base-compatible waveform BUSY management before record completion, input simulation/value-source separation, frozen-field validation and phase-aware add_record/del_record. Update `snmp3.dbd` and `Makefile` for record declarations, new sources and dbCore linkage. Audit actual IOC/support/wire loaded dependencies. Final verification after step 5: T1-T8 and T13.
4. Extend `Register.cpp` and Runtime/Request stop orchestration for pre-PINI readiness, admission closure, the completion-entry gate/count, callback drain, shutdown-phase detach and retained failed-stop state. Account Base callback join separately from actual queue cleanup and release. Preserve nonblocking later native reconciliation and no configuration reload. Final verification after step 5: T10-T12 and targeted ASan/UBSan cases in T13.
5. Ship `tests/rewrite/db/records.db`, `record-order.db`, `record-output.db`, record-specific config/startup fixtures, `RecordTest.cpp` and `test_records.py`; extend the actual NativeAgent/external UDP fault fixtures for typed edges, delayed/error responses and wire receipts. Wire normal and separate sanitizer products through `tests/rewrite/Makefile` and the existing sanitizer builder. Close all T rows through these shipped paths, with no internal stand-ins.
6. Update `docs/snmp-rewrite-contract.md`, architecture/configuration/worker references and `tests/rewrite/README.md` with the accepted record grammar/matrix, reservation changes, errors, ordering and shutdown limits. Change the reported record capability only after real evidence exists. Record source/product/library hashes, results and unresolved limits here; keep APC/R8/mdBook claims separate. Close with T13 and a requirement-to-T-label review of this detail.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Binding/startup integration | Load shipped valid/invalid record config/DB fixtures; verify all eleven DSETs, grammar, operation/type/FTVL mismatch, unknown IDs, duplicate record references and freeze; verify effective lsi/lso SIZV at default/minimum/maximum and Base clamping boundaries; include waveform fixtures with default and initially nonzero BUSY, inspecting device initialization and first-pass state; attempt INP/OUT changes and direct DTYP writes while idle/active and concurrent with drain/detach, then complete actual shutdown; run PINI/passive/periodic requests. | Current local Linux Base 7.0.10; actual snmp3Ioc, worker and NativeAgent | No initialization network I/O; independent immutable handles and frozen effective long-string capacity; device initialization normalizes waveform BUSY to FALSE without a BUSY-specific rejection, and pending SNMP work uses PACT with BUSY FALSE; refusal preserves context before detach permission; no new snmp3 attachment/admission during shutdown, with other-support attachment identified separately; accepted DTYP field write cannot silently rebind/admit snmp3 work; shutdown del_record detaches without early callback-storage free. |
| T2 | Numeric input integration | GET each advertised numeric tag through real ai/longin/int64in/numeric waveform records; agent supplies tag-appropriate INT32/UINT32 extrema, exact/inexact values around 2^53 and 2^24, Counter64 INT64_MAX/MAX+1/UINT64_MAX, inexact OpaqueDouble-to-FLOAT input, nonfinite opaque values and exception/type errors; observe exact values through IOC DBR_INT64/DBR_UINT64 access and separate CA decimal DBR_STRING reads; repeat each lossy input after a successful exact value. | records.db; actual native UDP agent and shipped conversion/DSET | D6 rejects Counter64-to-ai and numeric-to-FLOAT precision loss with an error and previous input retained; Counter32/TimeTicks through UINT32_MAX and Counter64 through INT64_MAX remain exact in int64in; larger Counter64 rejects there and remains exact in UINT64 waveform; DB and CA string observations preserve complete integers; no wrap, saturation, partial publication or implicit tick scaling; record-specific UDF behavior observed. |
| T3 | Binary/structured input integration | GET octets with empty/embedded-NUL/high bytes and exact/over-capacity lengths, OID arcs and IPv4 into real stringin/lsi/waveform; vary Binding capacity, SIZV and NELM independently; include 39/40/255/256-byte and maximum SIZV-1/SIZV payloads; observe full long strings through IOC access and CA VAL$ CHAR arrays, compared with short DBR_STRING access; repeat capacity failures after successful input. | records.db; actual worker/native/agent | D6 rejects Binding or record-capacity overflow with an error, preserves prior input bytes/NORD/LEN and publishes no shortened prefix; complete accepted values and exact NORD/LEN; lsi LEN includes NUL, empty success has LEN=1 and failure preserves VAL/LEN/UDF; deterministic formatting; reject unsupported FTVL and unusable strings without partial BPTR/VAL updates; distinguish client representation from native payload. |
| T4 | SET conversion integration | Process real ao/longout/int64out/stringout/lso for every advertised native tag; exercise integral/fractional, negative/unsigned, range/precision, termination and capacity edges; for ao-to-OpaqueFloat use exact binary halfway fixtures selecting both even neighbors, positive/negative values on and around halfway points, ordinary inexact decimals, normal/subnormal/signed-zero and overflow boundaries, and non-default ambient rounding modes; compare actual wire binary32 bits and restored thread state, and verify OpaqueDouble does not narrow; use IOC DBR_INT64 and separate CA decimal DBR_STRING int64out writes at INT64_MIN/MAX, INT32/UINT32 boundaries and 2^53+1; send lso empty/long/exact-capacity values through IOC and CA VAL$ access and observe Base handling of oversized client writes; inspect actual SET packets/agent state, LEN and Base int64out drive limits/IVOA. | record-output.db; actual IOC/worker/native/agent | D5 roundTiesToEven produces the selected binary32 payload without decimal pre-rounding, premature subnormal flushing or ambient-mode dependence; finite rounding is accepted and nonfinite/overflow payloads produce no SET; requested record values remain separate from rounded payloads; lso sends LEN-1 bytes without NUL; invalid captured termination/LEN/Binding capacity produces no SET, with Base pre-DSET truncation identified separately; int64out to Counter64 preserves nonnegative values through INT64_MAX, including 2^53+1, with no double intermediate; invalid target-range/negative-to-unsigned values produce no SET; Base drive/rate adjustments honored; success is separate from GET readback. |
| T5 | Alarm/undefined integration | Produce initial failure, valid GET then failure, local admission/convert error, native errorStatus/errorIndex, exception, timeout and competing Base limit/UDF alarms; observe STAT/SEVR/UDF, monitors, diagnostic success state, long-string LEN and values seen by actual FLNK targets; compare normal SNMP input with T8 simulation results for all six input records. | records.db and external agent/channel faults | In normal SNMP input, ai/longin/int64in/stringin/lsi preserve UDF on failed reads; successful lsi clears UDF and failure preserves VAL/LEN; waveform follows Base UDF clearing even on failure, retains BPTR/NORD and reports INVALID with successful-native-publication state unchanged; Base simulation value/UDF changes are identified separately and never counted as SNMP publication; failures visible even when completion DSET is skipped. |
| T6 | Base completion/order integration | Observe actual A-FLNK-B request/terminal/record phases and PACT on success/error/timeout, including lsi/lso long-string cases and waveform BUSY before callback rset entry; include fanout to independent addresses with reversed response delays and duplicate external responses. | record-order.db; real record/wire event identities | Waveform BUSY is FALSE before completion rset entry, so Base reaches completion rather than its BUSY early return; Base-owned FLNK once per accepted generation before PACT clears; B starts after A terminal processing and sees the complete lsi VAL/LEN; fanout supplies no completion barrier; no duplicate record mutation. |
| T7 | Active output/retry integration | Change ao/longout/int64out/stringout/lso output through dbPutField/CA while an actual SET is delayed, using IOC DBR_INT64 or CA decimal DBR_STRING access for int64out and CA VAL$ CHAR arrays for long lso values; compare scan/PROC triggers, same-value explicit retry after failed SET, native retries and ambiguous response loss. | record-output.db; actual delay/drop boundary | First payload stays immutable, later requested VAL/LEN survives, Base RPRO sends the latest prepared request when admitted; identify any client-side/Base pre-DSET truncation separately; no implicit module replay or promise to retain every intermediate update. |
| T8 | Record policy/simulation integration | Exercise longout OOPT Every Time and each rejected setting at initialization/live change; all ao/longout/int64out/stringout/lso IVOA branches and simulation during a delayed completion; exercise lso OMSL/DOL and its short IVOV copied into the effective SIZV-sized buffer. For ai/longin/int64in/stringin/lsi/waveform, use distinguishable agent and SIOL values, simulation before admission, a SIMM switch during successful/failing delayed GET and return to normal mode; include ai RAW and actual SDLY-driven Base callbacks, then inspect waveform BUSY at completion rset entry, source identity, alarms/UDF, VAL/BPTR/NORD/LEN, native-publication state, terminal release, FLNK and PACT. | Real Base record support, records.db/record-output.db and actual native GET/SET | Unsupported longout OOPT visible without a usable initial binding; no OOPT requirement invented for int64out/lso; completion errors and ownership release survive skipped DSET; no new GET/SET in completion. Simulation before DSET admission sends no native request; an accepted GET terminal is consumed once even when Base selects SIOL and skips completion DSET, with Base-owned simulation changes distinguished from D6 SNMP publication. Waveform completion enters Base with BUSY FALSE even when simulation bypasses DSET; no wrapper publication precedes Base source selection, no simulated value marks native-publication success, and no ownerless PACT completion admits native work. Observe FLNK/PACT and actual delayed callbacks without duplicate module completion; documented Base suppression, prepared VAL/LEN and possible pre-DSET truncation identified. |
| T9 | Queue/retirement integration | Real record bursts exhaust count/bytes, including maximum-capacity lsi/lso GET/SET payloads and their bounded conversion/capture storage; reduce/increase limits; delay native retirement after record timeout/consumption and retry the same handle. | Actual scheduler/IPC/worker/native path; existing queue reports | Synchronous record rejection without blocking; full Q/count remains until both obligations finish; Base-owned long-string buffers distinguished from charged module storage; no accumulating callback result queue; other address continues servicing. |
| T10 | Callback pressure integration | Fill the actual Base callback queue with controlled external callbacks, delay its consumer, complete real GET/SET and inspect retry/queued/running counters; then release pressure. | Shipped RecordTest harness using Base callbacks; unchanged module/native path | Retry failed callbackRequest until one entry is accepted; no duplicate entry; result remains owned, PACT completes once and no native replay/deadline reset occurs. |
| T11 | Shutdown integration | Stop with queued, dispatched, borrowed, enqueue-failed and running completions; hold actual Base record locks or downstream external link processing across attempt expiry, then release the boundary; observe waveform BUSY before Stopping completion, gate entry/exit, FLNK, AtShutdown return and worker join/reap. | Actual IOC lifecycle/record support; real workers and controlled external blocking | Waveform Stopping completion enters Base with BUSY FALSE and completes FLNK/PACT processing when callback progress is available; expiry closes new entry and records unsuccessful drain; already entered processing finishes before link closure; no callback-needed lock held across waits; safety wait can exceed the attempt budget without a false bounded-shutdown claim. |
| T12 | Cleanup/activation integration | Execute isolated shutdown/testdbCleanup/rebuild with delayed or abandoned queued completions, including lsi/lso pointer-backed buffers and retained maximum-capacity payloads; exercise phase-aware del_record, Base callback join followed by queue destruction, non-isolated shutdown, startup failure, repeated stop and retained native ownership. | Base isolated/non-isolated lifecycle with real SNMP record DBs; ordinary and instrumented module | No post-detach record/buffer access or premature callback-storage free; queued ownership survives join until actual exit/queue cleanup; expired drain blocks restart; configuration remains frozen and old native ownership must settle. |
| T13 | Product/regression qualification | Build/load normal and separate ASan/UBSan record products; identify sources, binaries and loaded libraries; run targeted T2-T4/T9-T12 and existing shipped M1-M5 regressions. | Current unchanged local Base/native dependencies; instrument new module code | No forbidden legacy/native IOC dependency, new sanitizer finding or foundation regression; identify uninstrumented dependencies and unsupported/unexecuted cells explicitly. |
| T14 | Real-path negative controls | Build isolated defective copies of shipped support with D6 precision/capacity checks, D5 tie selection/rounding-mode handling, callback failure retry, generation check or terminal release deliberately altered or disabled; run their corresponding real record fixtures through unchanged scheduler/worker/native/agent spans. | Separate disposable qualification products; never installed or substituted into ordinary products | Each targeted assertion detects its defective behavior, including unauthorized lossy/prefix publication or incorrect binary32 tie bits; retain executed control outcome rather than assuming a test would fail. |

All test environments are planned, not executed by this document update. Keep mocks/faults at external agent, UDP/filesystem/clock boundaries; Base callbacks and the internal DSET, Runtime, Scheduler, IPC, worker and native adapter must actually run. Existing component tests supplement this path. Stop/race checks use observed phase sentinels and identities rather than a fixed sleep or final values alone. Retain monotonic event order, actual request/terminal counts, alarms/PACT/UDF/NORD/LEN and source/product/library identities in local result receipts; never include credentials or secret-file contents.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-10-01 | Actual Base 7.0.10 records/DSET/worker/agent and production IOC/CA | Pending; subset PASS | All eleven DSET kinds and aliases; unknown binding/grammar/FTVL rejection; all five unsupported initial longout OOPT choices leave no usable binding; effective SIZV clamps; initial waveform BUSY; idle link/DTYP checks. Production IOC longin PINI runs after pre-PINI servicing and Base FLNK completes once, observed through actual CA. Remaining startup, live-field, periodic and concurrent-detach cells are unexecuted. Record receipts below. |
| T2 | 2026-10-02 | Actual numeric record/native and production IOC/CA path | Pending; subset PASS | All 68 advertised numeric GET pairs, including 48 native-tag/FTVL pairs, receive successful native input. Range/precision/extrema cover signed/unsigned 32-bit, 2^24/2^53, Counter64 INT64_MAX/MAX+1/UINT64_MAX, finite opaque extrema/subnormal/signed zero and initial NaN/infinity rejection. 121 conversion failures after successful native input preserve prior bytes/NORD and native-success state, with Base UDF behavior observed. Actual decimal DBR_STRING CA preserves 2^53+1 and INT64_MAX through int64out SET/int64in GET; UINT64 waveform reads UINT64_MAX exactly through CA STRING. Native exception/type-error, additional failure-after-success and signed-minimum CA cells remain unexecuted. |
| T3 | 2026-10-01 | Actual binary/text/structured record/native and production IOC/CA path | Pending; subset PASS | Exact binary octets including NUL, OID arcs and IPv4; text rejects embedded NUL or overflow without prefix publication; lsi VAL/LEN preservation, empty and maximum 32766-byte data. Actual lsi/lso VAL$ CHAR-array CA SET/GET is exact at 0/39/40/200/255 data bytes; CA DBR_STRING view is 39 bytes; short stringin/stringout SET/GET is observed. A 300-byte VAL$ write is rejected by caput before put and preserves prior VAL/LEN/native GET. Remaining structured/text boundaries and maximum-capacity CA cells are unexecuted. |
| T4 | 2026-10-02 | Actual output DSET/native SET/GET and production IOC/CA | Pending; subset PASS | All five output DSET kinds and all 20 advertised numeric SET pairs. Signed/unsigned target boundaries, integral/fractional conversion, full nonnegative signed-64 capture and finite floating extrema/subnormal/signed zero are checked by separate native GETs; 67 invalid numeric outputs preserve requested values and produce no native SET. Exact Counter64 SET of 2^53+1; 36 binary32 wire-bit fixtures across four ambient modes; requested ao retained; nonfinite/overflow/negative Counter64/over-capacity SET rejected; empty and maximum lso payload. Base ao drive clipping/rate limiting and OVAL wire capture, all five first-pass IVOV values and lso DOL are verified. Actual CA writes exercise int64out decimal and stringout/lso short/CHAR-array values with separate native GETs. ao/longout CA, signed-minimum CA and remaining long-string boundary cells remain unexecuted. |
| T5 | 2026-10-01 | Actual Base value/alarm processing and outer UDP response dropping | Pending; subset PASS | Initial/local conversion failures preserve input data/UDF; errors survive input simulation bypass. All eleven record kinds report COMM/INVALID on actual NativeFailure/timeout code 2; a separate record deadline reports IPC Deadline/COMM. Waveform clears BUSY and follows Base UDF while native-success remains false. Native errorStatus/exception, failure after a valid GET, competing-alarm and monitor matrix remain unexecuted. |
| T6 | 2026-10-01 | Actual ai-FLNK-calc-FLNK-lsi chain | Pending; subset PASS | Source PACT is 1 during Base FLNK; target completes once with complete VAL/LEN. Other record/failure/deadline chains, fanout and delayed/duplicate external responses remain unexecuted. |
| T7 | 2026-10-01 | Actual Base dbPutField/RPRO, delayed UDP SET and response loss | Pending; subset PASS | All five outputs preserve the captured first payload and process the latest requested value through Base RPRO when admission is available; exact DBR_INT64 and 200-byte first lso payload. Same-value explicit retry uses two module generations/distinct native request IDs; configured native retry uses one generation/the same native request ID. Separate GET observes SET application despite lost successful responses. CA, long active client writes, scan/PROC and retained-ownership admission-rejection cells remain unexecuted. |
| T8 | 2026-10-01 | Actual Base simulation, output policy and DSET/native path | Pending; subset PASS | All six inputs use SIOL before admission and after a queued native completion; Base SDLY and ai RAW run; waveform BUSY clears before simulation bypass. All five outputs exercise three IVOA branches on first-pass/completion, live simulation after actual native timeout, synchronous/delayed simulation before admission, preserved current VAL/LEN and exactly-once terminal/FLNK processing. All five unsupported initial/live longout OOPT choices are observed. lso supervisory/closed-loop DOL, short IVOV and Base pre-DSET capacity reduction are verified by separate GET. Remaining input failure/source-switch and delayed-simulation mode-change cells are unexecuted. |
| T9 | 2026-10-01 | Actual maximum long-string/native/queue path | Pending; subset PASS | 32766-byte SET and three GETs; two borrowed terminals retain 796640 charged bytes; byte-exhausted request rejects synchronously; lowering limits preserves ownership and raising them admits a separate explicit request. Delayed native retirement/retry and other-address progress remain unexecuted. |
| T10 | 2026-10-01 | Actual Base callback queue, GET and SET | Pending; subset PASS | External callbacks fill the real queue; failed enqueue retains terminal/full reservation/PACT, retries and completes once without a second accepted entry. Full requirement review remains pending. |
| T11 | 2026-10-01 | Actual entered/queued callback shutdown | Pending; subset PASS | Held record lock makes an entered callback exceed the drain attempt; shutdown waits for its lease. A blocked Base queue leaves a queued callback retained after gate closure/detach. Expired drain refuses restart; other Stopping/waveform/downstream states remain unexecuted. |
| T12 | 2026-10-01 | Actual isolated queue cleanup and production non-isolated IOC exit | Pending; subset PASS | Contexts survive queued ownership and are cleared only after actual isolated queue cleanup; abandoned terminal finalized once. Production IOC normal exit runs Base AtShutdown through AfterShutdown, verifies the actual worker PID is absent after reap, closes completion entry and reports eight retained inactive contexts without isolated queue destruction. Isolated rebuild/reuse, in-flight non-isolated retention, record startup-failure and remaining live-detach cells are unexecuted. |
| T13 | 2026-10-02 | Ordinary and separate ASan/UBSan products | Pending; subset PASS | Numeric matrix, current edges and production IOC/CA pass on ordinary and separate instrumented products; ordinary output-policy and full native-adapter regressions pass after the shared fixture extension. The new sanitizer build identifies 15 products, including the actual production IOC. Earlier record/pressure/blocked/queued-shutdown products, native-free loader checks and ordinary M1-M5 regressions pass for their recorded versions; component evidence includes 50166 conversion checks. Full targeted T2-T4/T9-T12 qualification remains pending. |
| T14 | 2026-10-01 | Separately built defective support copies; unchanged record/worker/native spans | Pending; subset PASS | Seven actual controls detected: communication-alarm classification, integer precision, text capacity, binary32 tie selection, ambient-mode dependence, callback retry and terminal release. Defective library load and named assertion failure are observed; stale-generation control remains unexecuted. |

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

Active output qualification, 2026-10-01: the actual external UDP proxy delays responses by 750 ms. An external callback holds the unchanged Base callback consumer until the accepted SET and a following GET have both retired natively. This qualifies the RPRO path with admission available: FIFO GET observes the first captured payload, RPRO admits the latest prepared value, and a separate GET observes that value. Each of the five outputs runs Base FLNK twice. The response-loss cases drop actual successful SET responses and distinguish operator retry from native-library retry; their explicit GET observations do not add automatic module readback. The retained-native-ownership admission-rejection path remains pending.

Output policy qualification, 2026-10-01 at approximately 22:29 PDT: actual Base 7.0.10 processing, native timeout terminals and external callback gating exercise all five output kinds. Completion IVOA and simulation preserve the current requested VAL/LEN, retain COMM/INVALID and finalize terminal/FLNK once; live unsupported OOPT reports LINK/INVALID even when Base conditional_write skips DSET. First-pass Don't drive outputs admits no SET; Continue normally and Set output to IVOV use the prepared value, verified by separate GET after response loss. Pre-existing Base HIHI/UDF INVALID alarms retain their equal-severity priority. Both synchronous and SDLY-driven simulation before admission leave module identity/completion counts unchanged. Actual ao drive/rate limits prepare VAL=5 and successive OVAL/wire values 1 and 2. lso supervisory ignores DOL, closed_loop transfers 200 data bytes, and a 300-byte source is reduced by Base to 255 data bytes/LEN=256 before DSET capture; GET verifies that prepared value. This last observation qualifies the Base boundary, not rejection of the original DOL length. Ordinary and ASan/UBSan cases each pass 1340 assertions; actual proxy metadata counts 38 SETs. Initial fixture failures are retained and do not qualify these results.

Production CA qualification, 2026-10-01: `test_record_ca.py` launches the shipped production Main, generated IOC registrar, unchanged Base CA server, actual worker/native products and real loopback agent. The selected Base caget/caput execute as separate clients with private CA ports and an owned/reaped repeater. Decimal CA STRING preserves 9007199254740993 and 9223372036854775807 through int64out SET and int64in GET; UINT64 waveform returns 18446744073709551615 exactly. lso/lsi VAL$ CHAR arrays preserve 0/39/40/200/255 data bytes and LEN including NUL; ordinary CA STRING exposes 39 data bytes. caput rejects a 300-byte VAL$ write with Invalid element count before put, and subsequent native GET observes the previous 255-byte value. This client rejection differs from the verified Base DOL truncation above. PINI input and its Base FLNK run after servicing is ready; normal non-isolated IOC exit reaches AfterShutdown. Actual /proc observations identify the owned worker PID/start time before exit and require that PID to be absent afterward; final module reports show 19 completions, a closed entry gate, zero active/queued/running requests and eight retained inactive contexts. Ordinary/instrumented receipts each pass 119 runner checks. The instrumented production IOC build is `work/r6-ca-asan-ioc-build-20261001/snmp3Ioc.build.json`, using production Main and its actual generated registrar; the sanitizer builder now declares this product alongside the record/component products. Base, caget/caput and system/vendor dependencies are uninstrumented; leaks are disabled. Active CA changes and in-flight non-isolated teardown remain unqualified.

Native timeout alarm qualification, 2026-10-01: actual response dropping produced READ/INVALID in the pre-correction ai path (`work/r6-alarm-before-20261001-run3/results.json`). Current ordinary and instrumented alarm cases observe NativeFailure/native code 2 before completion for all eleven record kinds, followed by COMM/INVALID, without native publication. The separate 1 ms record deadline produces IPC Deadline and COMM/INVALID. The communication-alarm negative control reinstates the defective classification in an actual compiled support library and fails the named ai alarm assertion normally. Session-open/send/cancellation alarm branches are implemented according to the accepted mapping but are not qualified by this timeout case.

Current ordinary foundation/config/lifecycle regressions: `work/r6-foundation-alarms-20261001/results.json`. Worker/IOC/operator regressions: `work/r6-worker-alarms-20261001/results.json`, `work/r6-supervisor-alarms-20261001/results.json` and `work/r6-operator-alarms-20261001/results.json`. Native API and adapter regressions: `work/r6-native-api-alarms-20261001/results.json` and `work/r6-native-adapter-alarms-20261001/results.json`. Current ordinary/instrumented component receipts are `work/r6-components-alarms-20261001/results.json` and `work/r6-components-alarms-asan-20261001/results.json`, with 15 executions and 50166 conversion assertions per product. They pass for their recorded source/product identities; unavailable wrong-owner qualification retains its explicit NOT RUN. Earlier failed initial/pressure/edge/foundation/alarm runs remain retained under `work/` and do not qualify any row.

Numeric qualification, 2026-10-02: ordinary and separate ASan/UBSan cases each pass 13155 custom assertions and 20 runner checks. The selected receipts above qualify the current shipped numeric DB; their source/product hashes match the current fixture and products. The shipped numeric DB contributes 148 contexts; the unchanged baseline contributes 12, for 160 actual initialized contexts. Every advertised numeric GET pair receives a successful exact input through actual output DSET/worker/native SET and subsequent GET; range/precision rejection is then observed on the same initialized inputs. Fixed actual agent OIDs additionally provide unsigned Counter64 2^63/UINT64_MAX and opaque NaN/infinities. The 60 fixed input pairs are initial-input checks; they do not claim failure after an earlier native success. The matrix records 121 conversion failures after successful input, 68 accepted output cases, 67 rejected outputs and 67 stimulus SETs. Actual agent observations contain 141 SET actions: those 135 admitted numeric SETs plus six baseline/pressure SETs. Rejected outputs leave request identity/completion counts and separately read native value unchanged. Waveform rejection preserves complete BPTR bytes and NORD, while UDF follows Base processing and native-success state remains unchanged. The complete new sanitizer build is `work/r6-numeric-sanitizers-20261002/sanitizer-build.json`; all 15 declared products build successfully with the builder's instrumentation checks, including its documented InventoryTest UBSan exception. New code is instrumented, dependencies remain uninstrumented and leaks are disabled. Current ordinary and instrumented edges each pass 1263 assertions; production IOC/CA each passes 119 runner checks. The ordinary policy regression is `work/r6-numeric-regression-policy-20261002/results.json` (1340 assertions), and the complete ordinary native-adapter regression is `work/r6-numeric-regression-native-20261002/results.json` (779 checks). Every receipt qualifies its identified sources/products only. Native exception/type-error, remaining CA, lifecycle and other T1-T14 cells remain pending; record support is not advertised.

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

- M1-M5 provide current product documentation; D4 assigns this milestone. It need not wait for M6-M8; later implemented behavior updates the maintained chapters.
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

## Backlog

### Work

| ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- |

### Backlog Details

No unassigned work is recorded in this generation. The eight rewrite checkpoints and mdBook work are assigned under Milestone.
