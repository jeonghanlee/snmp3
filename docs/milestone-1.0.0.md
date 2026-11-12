# Work Register

Release line: 1.0.0
Milestone index: 1.0.0
Canonical path: `docs/milestone-1.0.0.md`
Canonical branch or ref: master
Git upstream: origin/master
Remote tracker: jeonghanlee/snmp3, GitHub milestone 1.0.0
Recorded date: 2026-11-09
Source baseline: tag `0.0.1` at `8c5bd199f3137bb3c5756c7a2c7d1640122ed788`; register reset from `ba6c558a1118c792e769aed79ded7581538c3888`

Next session entry point: Continue M2 in `docs/milestone-1.0.0.md`: step 1 is done (results in T1, T2 and T6; each further write run on outlet 8 waits for the owner's confirmation). The outlet 8 records, the startup scripts and the run and timing tools are built and tested against test agents (plan items 4 to 8 are prepared), and the short check of the legacy IOC passed on the units on 2026-11-11 (reads only). The plan, revised after a review against the test framework, was accepted by the owner on 2026-11-11; step 2 (legacy IOC) needs the owner to authorize it and to confirm that PDU1 may be cut from its network on the typed handshake, and the run tool creates and removes the legacy credential file itself with the owner's permission. The exported and built legacy IOC is under `work/baseline-legacy-20261110-01/` and the snmp3 measurement IOC for step 3 under `work/baseline-snmp3-20261110-01/`. M1, M6 and M10 are also Ready; G1 is Open.

## Scope

The goal of this release line is that every guarantee in [What snmp3 guarantees against each cause](why-snmp3.md#what-snmp3-guarantees-against-each-cause) reads Implemented, and that the implemented product is verified on one NMC2 unit and one NMC3 unit sharing one IOC. The five causes and the 23 items behind them are recorded in [The operating problems behind snmp3](why-snmp3.md) and the [Review record](legacy-review.md). The release line also includes the table-read request (GETBULK), the SET outcome contract, reply-driven record processing, and shared-library SONAME versioning (D1, D3).

Work is ordered by the code surface each unit shares with the next; the row order is the execution order and the local IDs follow it. Each completed M row adds a `CHANGELOG.md` entry and an annotated tag with the version named in its detail; the final release is `1.0.0`.

Out of scope: performance criteria and acceptance (Backlog M13), the deferred record verification cells (Backlog M14), the negative-control harness classification (Backlog M12), a new measurement tool before an existing one is shown insufficient (D4), and any deployment to the operations network before `1.0.0`.

## Milestone

### Work

| Group | ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Gate | G1 | Laboratory NMC2 and NMC3 units reachable through snmp3 | External gate | Open | No | | Both units answer a GET through the laboratory IOC with snmp3 `0.0.1`; [detail](#g1---laboratory-nmc2-and-nmc3-units-reachable-through-snmp3) |
| E | M1 | Probe spawn environment equal to the worker environment | Milestone | Not started | Yes | | Probe and worker receive the same environment list; `ConfigTest` passes; tag `0.0.2`; [detail](#m1---probe-spawn-environment-equal-to-the-worker-environment) |
| Baseline | M2 | Baseline measurement on the two laboratory units | Milestone | In progress | No | D8, D9, D11 | Net-SNMP 5.9.4 read directly and the installed legacy module measured on both units, with the response time of a request and the interval of new values recorded; no product change; [detail](#m2---baseline-measurement-on-the-two-laboratory-units) |
| A | M3 | Two-device isolation measurement on the current product | Milestone | Blocked | No | G1, D4, M2 | Measured cadence of the healthy unit with the other unit disconnected; [detail](#m3---two-device-isolation-measurement-on-the-current-product) |
| B | M4 | Request sizing with advertised message size and tooBig split | Milestone | Blocked | No | G1 | NMC2 accepts a 61-OID request without manual `maxVarbinds` tuning; tooBig reaches the record; tag `0.0.3`; [detail](#m4---request-sizing-with-advertised-message-size-and-toobig-split) |
| B | M5 | SET outcome contract | Milestone | Not started | No | M4 | Output records complete only on the SET result; a timeout is an alarm; tag `0.0.4`; [detail](#m5---set-outcome-contract) |
| C | M6 | Reply time stamp on records | Milestone | Not started | Yes | | Record TIME equals the worker receive time with `TSE=-2`; tag `0.0.5`; [detail](#m6---reply-time-stamp-on-records) |
| C | M7 | Reply-driven processing and driver-owned periodic acquisition | Milestone | Not started | No | M4, M6 | `SCAN=I/O Intr` records process on reply arrival at the configured period; tag `0.0.6`; [detail](#m7---reply-driven-processing-and-driver-owned-periodic-acquisition) |
| Performance | M8 | GETBULK table reads | Milestone | Not started | No | M4 | A table read uses one GETBULK request where the profile allows it; tag `0.0.7`; [detail](#m8---getbulk-table-reads) |
| D | M9 | SNMPv3 localized keys and secret procedure | Carry-forward | Blocked | No | G1, D10 | Worker retains localized keys only; the configuration carries localized keys and a documented procedure creates the files that reach both units; tag `0.0.8`; [detail](#m9---snmpv3-localized-keys-and-secret-procedure) |
| Release | M10 | Shared-library SONAME and symbolic-link versioning | Carry-forward | Not started | Yes | D3 | Installed libraries carry SONAMEs and links per policy; consumers load them; tag `0.0.9`; [detail](#m10---shared-library-soname-and-symbolic-link-versioning) |
| Release | M11 | Release 1.0.0 | Milestone | Not started | No | M1, M2, M3, M4, M5, M6, M7, M8, M9, M10 | Integrated re-runs and the two-unit acceptance pass; tag `1.0.0` and GitHub release exist; [detail](#m11---release-100) |

Current work tally: 0 Complete, 1 In progress, 7 Not started, 3 Blocked; G1 Open. Ready rows: M1, M6, M10. Backlog contains 1 Open and 2 Deferred rows and is excluded from this tally.

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
| D8 | Measure the baseline before any product change: Net-SNMP 5.9.4 read directly, and the installed legacy module `snmp-1.1.0.4ja`, on one NMC2 unit that carries a load and one NMC3 unit without load. Reads on both units, writes only on the unit without load. The two-unit measurement of the current product compares against these results. The baseline carries no verdict; the criteria are those of the rows that use its numbers. | 2026-11-10 |
| D9 | Allow writes to outlet 8 of PDU1 (NMC2, loaded) only: immediate off, immediate on and immediate reboot. No other object of PDU1 is written. This amends D8, which allowed writes only on the unit without load. Each run restores the original state. | 2026-11-10 |
| D10 | Configure snmp3 with localized keys. Both unit models, the NMC2 unit and the NMC3 unit, accept access with the localized keys computed from the existing passphrases and the unit's engine ID (SHA and AES-128), with no change on the unit, and a wrong key is rejected. The configuration carries localized keys instead of passphrase files, one pair (an authentication key and a privacy key) for each unit because the keys depend on the engine ID of the unit, and the tool and the procedure that produce and install them are developed in the SNMPv3 key work. Existing legacy deployments keep passphrase files. | 2026-11-11 |
| D11 | Create the credential file `ver3.txt` of the legacy module only for the duration of the baseline runs: the run tool writes it from the existing secret files with mode 600 in a new mode-700 directory on tmpfs, and removes it as soon as the IOC has read it, when the run stops or when the tool is terminated; nothing is written under the repository, and each creation needs the owner's permission. | 2026-11-11 |

### ID Migration

A baseline row, created as M15 and placed as M2 on 2026-11-10 so the row order stays the execution order, moved every later row down by one. No GitHub issue carries a row number.

| Old ID | Current ID | Reason | Updated References |
| --- | --- | --- | --- |
| M2 | M3 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M3 | M4 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M4 | M5 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M5 | M6 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M6 | M7 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M7 | M8 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M8 | M9 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M9 | M10 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M10 | M11 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M11 | M12 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M12 | M13 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |
| M13 | M14 | Baseline row inserted before it | Work table, Deps, details, anchors, Integrated Verification sources |

### Milestone Details

#### G1 - Laboratory NMC2 and NMC3 units reachable through snmp3

Origin: 1.0.0 / G1
GitHub Issue: none
Status: Open

##### Summary

M3, M4 and M9 need one AP8932 unit with an NMC2 card and one with an NMC3 card, both reachable from the laboratory IOC host and both answering snmp3 `0.0.1` requests with the laboratory IOC's existing SNMPv3 secret files. The laboratory IOC of the apcpdu repository (commit `3314b1e`) is the known startup. The owner confirms the units are powered, on the laboratory network and configured.

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

#### M2 - Baseline measurement on the two laboratory units

Origin: 1.0.0 / M15
Identity History: M15 -> M2 (created as the next free ID, then placed second so the row order is the execution order, 2026-11-10)
GitHub Issue: #11 - https://github.com/jeonghanlee/snmp3/issues/11
Status: In progress

##### Summary

The release goal compares snmp3 with the module it replaces, so the starting point is measured first, with no product change. Two layers are measured on one NMC2 unit that carries a load and one NMC3 unit without load: Net-SNMP 5.9.4 read directly with the tools of `docs/snmp-performance`, and the installed legacy module `snmp-1.1.0.4ja` (library string `devSnmp 1.1.0.3`) running in the laboratory IOC of the application repository. The legacy module is used as installed in the Debian 13 distribution tree on Base 7.0.10; EPICS Base and the module are not modified. The slower response of the NMC2 unit is attributed to the card by the owner; load and card are not separated.

##### Scope

- Units: PDU1 (NMC2, loaded, reads, and writes on outlet 8 only under D9) and PDU2 (NMC3, no load, reads and writes). Addresses, user names and secrets appear only in files under an ignored `work/` directory.
- Step 1, Net-SNMP directly: `run_device_phases.py` phases 1 and 2 on the 358 objects of `docs/snmp-performance/inputs/ap8932-oids.tsv` (5 repeats, tool stop rules), after the read-only phase 0 probe. The `sysLocation` write is on PDU2 only, through a sender that refuses any label other than PDU2, on `sysLocation` with the original value restored.
- Step 1, outlet 8 of PDU1 (D9): `outlet_probe` switches outlet 8 off and on in cycles and reboots it once, times each SET and the time until the state reads as set, and restores the original state. The object identifiers are constants of the program, a label other than PDU1 or a missing write flag is refused before a session opens, and `CYCLES 0` reads only. Every write run needs the owner's confirmation; the first write run is one cycle without a reboot.
- Step 2, legacy IOC: the legacy laboratory IOC of the application repository branch `refactor/snmp-db-processing`, exported with `git archive` and built against the distribution Base. Loader switches: no outlet command records (`WRITE_EN=#`) and no limit enforcement (`LIMIT_EN` unset) on either unit. The load template still loads the master control command records (`MasterControlCmd_` and `MasterControlCmd`), the only records of the loaded templates with an SNMP output link, so they are removed from the exported copy and the difference to the branch is recorded with the results; the read population is unchanged. Each unit also gets one `snmpEngineTime` read record, and PDU2 one `stringout` on `sysLocation`. Two runs: the module defaults (10 s timeout, 5 retries, and 32 objects per request, passed as `MAXOIDS=32` because the loader's own default is 24) and the laboratory IOC operations values (2 s, 1 retry, 20 objects for PDU1 and 50 for PDU2). Phases per run: both units connected 300 s, PDU1 cut 300 s, PDU2 cut 300 s, reconnect. The owner cuts and restores a unit on a typed handshake. The defaults run holds the cut phases for 600 s instead of 300 s. The legacy module reads its SNMPv3 credentials from a file `ver3.txt` (keys `SecurityName`, `AuthType`, `PrivType`, `AuthPassphrase`, `PrivPassphrase` and `SecurityLevel`, one file for both hosts), which holds the passphrases in plain text; the module reads it once when its configuration call runs and keeps the credentials in memory. The run tool therefore (D11) creates the file only with the owner's permission, from the existing secret files of the targets file (both units hold the same user, algorithms and secrets, which the tool checks), with mode 600 in a new mode-700 directory on tmpfs, and removes it as soon as the IOC log shows `iocInit`, when the freshness records are connected, when the run stops or when the tool is terminated; nothing is written under the repository. Its values are not written in this register.
- Step 2, outlet 8 of PDU1 (D9): of the outlet command records of the legacy write template, only the one of outlet 8 loads on PDU1; it is the write template loaded for channel 8 alone, and the load check lists exactly one SNMP output record, `OutletCmd8_` of PDU1, where the unchanged export lists none. Its SET is timed from the put to the changed device state (T7), and the original state is restored after each run. Every write run needs the owner's confirmation.
- Step 3, snmp3 `0.0.1`: the measurement IOC described in the paragraph on the preparation of step 3 (one snmp3 input per legacy SNMP input, plus one `snmpEngineTime` read record per unit), on the same two units, with the laboratory IOC values (`timeoutMs` 2000, `retries` 1), run through the same phases, relay recorder and interval script as the operations run of step 2 and under the same PDU1 cut handshake. Outlet 8 of PDU1 also gets one `longout` record on its command object (D9), so that step 3 measures the SET of outlet 8 as step 2 does (T9); that record is built, and every write run needs the owner's confirmation.
- Two quantities in every step: the response time of one request (p50, p95, maximum) and the interval between new values of the `snmpEngineTime` record (p50, p95, maximum, from `camonitor -tsc`). The request time of steps 2 and 3 is read at the wire with a small user-space UDP relay recorder; the interval is analyzed with a short script. Eleven scratch tools are written because no existing tool does these jobs: the relay recorder (`udp_relay.py`), the interval script (`interval_stats.py`), the direct SET senders (`set_probe` and `outlet_probe`), the run tools (`phase_runner.py`, `make_run_scripts.py` and `snmp3_run_config.py`), the timers of a SET through an IOC record (`outlet_watch` with `outlet_set_time.py`, and `location_set_time.py`), and `gen_measure.py`, which generates the database and configuration of the measurement IOC. They are under `work/baseline-direct-20261109-01/`, `work/baseline-legacy-20261110-01/run/` and `work/baseline-snmp3-20261110-01/` (`measure/` and `run/`). The interval is the time between the `camonitor` server times of two new values of a counter that advances in whole seconds, so it follows the module's request timing and is not fixed by the poll period: in the first check run on the units the gaps between new values ran from 0.57 s to 3.41 s. The request time of step 1 is taken by the tool itself and that of steps 2 and 3 at the wire through the relay; in two loopback tests against the repository test agent (300 alternating pairs of direct and relayed calls each) the relay added a mean of 0.57 ms per call of two exchanges with a standard error of 0.34 ms (the retained run, `work/baseline-direct-20261109-01/test-runner/relay-overhead-result.txt`) and 0.02 ms with 0.39 ms (an earlier run whose output was not kept), so neither differs from zero by two standard errors; the larger bound, about 1.2 ms per call, is not small against the 2.3 ms median of a single-object GET on PDU2 (T1).
- Stop rules: the tool's own rules in step 1; in the IOC steps the tool stops on a decrease of a unit's `snmpEngineTime` (card restart), no answer from a unit for 120 s outside an owner cut and outside the recovery after its restore (which lasts until the first fresh value or the limit `--recover-seconds`), an exit of the IOC or of a relay, or the owner's request. The loaded record list is checked by hand before the runs (item 5), and no run starts when it shows an output record on PDU1 other than the outlet 8 command record.

Out of scope: any change to the product or to Base, a verdict on improvement (the rows that use these numbers carry the criteria), separating load from card, writes to PDU1 other than outlet 8, performance acceptance.

##### Completion Criteria

- Step 1 produced `phase1.json`, `kstar.json`, `phase2.json` and the SET sender's result for PDU2, each stating its repeat count and the Net-SNMP version.
- Step 1 also produced the outlet 8 result of `outlet_probe` on PDU1 (T6): the time of each SET and of each state change, and the original state restored.
- Step 2 produced both run files with the four phases (the recovery phase is recorded after each cut, so `run.json` lists five phases; or a recorded reason for stopping a phase), the printed module defaults, the loaded record list showing no output record on PDU1 other than the outlet 8 command record, and the PDU2 SET result.
- The response time and the interval are stated for each phase in one table that names the populations (the 358-object set for step 1, and the 163 legacy signals per unit for steps 2 and 3) and, for each row, whether the request time was taken by the tool (step 1) or at the wire through the relay (steps 2 and 3).
- A run of step 2 or 3 is complete only when its phases ran in the order A (both units connected), B (PDU1 cut), D (recovery), C (PDU2 cut) and D (recovery), the names `run.json` gives them, `stopped_by` is empty and every phase check in `run.json` is valid: in phase A both units answered, in a cut phase the cut unit answered no request later than 20 s after the typed cut while the other unit answered, and in a recovery phase a fresh value was seen. Any other run is reported as incomplete with the reasons from `run.json`.
- Each run directory holds `inputs.json` with the SHA-256 of the files the run used; before a result is written into this register, the list is compared with the files on disk, and a result of a changed file is rerun or the difference is stated.
- Step 3 produced the same phases and run files as the operations run of step 2 on the measurement IOC, the loaded record list showing no output record on PDU1 other than the outlet 8 command record, and the check of the Passive-to-2-second assumption against the request interval observed at the wire in step 2.
- The outlet 8 SET results of the legacy IOC (T7) and of the measurement IOC (T9) state the time from the put to the changed device state, and the original state of the outlet is restored after each run.

##### Dependencies And Decisions

- D8 defines the baseline, D9 amends it for outlet 8 of PDU1, and D11 fixes how the legacy credential file is created and removed. The owner's confirmation that no other client reads the two units is needed before step 1.
- The two-unit measurement of the current product (M3) repeats the phases of step 2 on the product as it stands when M3 runs and compares with the results of steps 2 and 3; M3 measures the isolation of one unit with the other disconnected, and step 3 of this row measures `0.0.1` as the baseline.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-10, accepted by the owner after the second-person and third-person passes of this register converged; item 3 gained its run-log sentence afterwards, and the owner accepted the revised plan again on 2026-11-10; item 5 gained its freshness check after that, and the owner accepted the revised plan again on 2026-11-10; items 2 and 5 to 8 were revised for the built run tools after that (item 9 only renumbered), and the owner accepted the revised plan again on 2026-11-10; item 8 gained its handshake and result paragraph after that, and the owner accepted the revised plan again on 2026-11-11; the plan was revised again on 2026-11-11 after reviews against the test framework: Scope (the meaning of the interval and of the request time, and the stop rules, which now leave a unit outside the silence limit during its recovery and stop on an exit of the IOC or of a relay), Dependencies (the new decision D11 on the legacy credential file), Completion Criteria (complete and incomplete runs, `inputs.json`), item 8 (hash flags, the `WARNING` line, signal handling and the exit codes 3, 4 and 5), item 9 (the `inputs.json` comparison) and the Test Plan note; the owner accepted this revision on 2026-11-11 after the second-person passes converged and the third-person passes of the run tool converged, while the document itself had no third-person pass after its last corrections
Implementation Authorization: step 1 only (reads on both units, writes on PDU2, and writes on outlet 8 of PDU1 under D9 after the owner's confirmation of each write run), by the owner instruction of 2026-11-10; steps 2 and 3 are not authorized until the owner authorizes them, except the first check-only run of step 2 on the units (reads only), authorized by the owner instruction of 2026-11-11
Superseded Plan Artifacts: none

1. Owner confirms that no other client reads the units and that PDU1 may be cut from its network for step 2.
2. Run step 1 phases 1 and 2 (T1); write and test the SET sender on PDU2 (T2). The targets file keeps addresses and secret file paths outside the repository in the format of `docs/snmp-performance/test-reference.md` (Format of the targets file), with PDU1 and PDU2 as labels. After building the tools with `docs/snmp-performance/tools/build.sh <bin_dir>`, from the repository root:

    ```bash
    PH=(python3 docs/snmp-performance/tools/run_device_phases.py --targets <targets_file> --oids docs/snmp-performance/inputs/ap8932-oids.tsv --out <results_dir> --tools <bin_dir>)
    "${PH[@]}" phase1
    "${PH[@]}" phase2 --kstar <results_dir>/kstar.json
    ```

3. Run `outlet_probe` on outlet 8 of PDU1 (T6) from the repository root with `python3 -I work/baseline-direct-20261109-01/outlet_probe.py <cycles> <reboot>`, cycles 0 to 5 and reboot 0 or 1. Run the read-only form (`0 0`) first, then one cycle without a reboot (`1 0`), then more cycles and one reboot (`3 1`); every write run needs the owner's confirmation. `outlet_probe.py` passes the write argument only while `private/outlet8.json` holds `{"write": true}`; set it for a run and back to false afterwards. Every run appends its printed line to `work/baseline-direct-20261109-01/outlet_probe.log` with a UTC time, the arguments and the exit code. A run succeeded when the printed line has `"failed":0` and `"restored":1` and the exit code is 0. Exit code 5 means the program refused (no write flag, or another label), 6 an initial state that is neither on nor off with no SET sent, and 2, 3 or 4 a usage, key or session error with no SET sent. Exit code 7 means a failed step or a state that was not restored: do not retry, read the state with `outlet_probe.py 0 0` and tell the owner.
4. Without contacting a device, add the outlet 8 command record to the legacy copy (loaded on PDU1 only) and the outlet 8 `longout` to the measurement IOC (`measure/gen_measure.py` and `st.measure` under `work/baseline-snmp3-20261110-01/`), and load-check both without `iocInit`.
5. Build the legacy IOC from the exported tree, confirm that the loaded record list has no output record on PDU1 other than the outlet 8 command record, and run the two runs of step 2 (T3, T4) through `phase_runner.py` with the startup scripts `st.operations` and `st.defaults` (item 8); the tool starts the relay recorder and runs the interval script. The exported tree names a Base under `/usr/local/epics` in `configure/RELEASE`; a `configure/RELEASE.local` in the export sets `EPICS_BASE` to the distribution Base (`<distribution>/1.5.0/debian-13/7.0.10/base`), which holds the eight modules the application needs. Before the two runs, start the legacy IOC once on the units for a short check with the same command and `--check-only`: `EngineTime` must read without a read alarm on both units and its value must advance (the printed result has `"ok": true` for both units); on a read alarm, stop, and the owner decides the substitute object. The defaults run uses `--cut-seconds 600 --recover-seconds 600`.
6. Run step 3 after step 2, with the owner's authorization and the same cut handshake: create the run configuration with `snmp3_run_config.py` and run `phase_runner.py` with the startup script `st.measure-run` (item 8), which records the same phases as step 2 (T8).
7. Run the SET timings after the read phases of steps 2 and 3, each write run after the owner's confirmation: the outlet 8 SET of the legacy IOC (T7) and of the measurement IOC (T9) with `outlet_set_time.py`, and the `sysLocation` SET of the legacy IOC on the unit without load (T5) with `location_set_time.py`. Keep the IOC and the relays up with `phase_runner.py --check-only --check-seconds 600` and run the timer in a second terminal. `private/outlet8.json` is handled as in item 3, `private/writeflags.json` already allows the unit without load, and each tool prints the put-to-change time and the alarm state of the record, restores the original value and appends its line to its own `.log` file.
8. The commands of items 5 to 7, from the repository root, with `<targets_file>`, `<run_dir>` (a new directory per run), `<repository_root>` and `<run_config>` replaced. Add `--check-only` for the short check, or `--a-seconds 300 --cut-seconds 300 --recover-seconds 300` for a run; the defaults run uses `st.defaults`, `--label legacy-defaults`, `--cut-seconds 600` and `--recover-seconds 600`:

    ```bash
    RUN=(python3 -I work/baseline-direct-20261109-01/phase_runner.py --targets <targets_file> --record PDU1=ALSUCL:PDU1:EngineTime --record PDU2=ALSUCL:PDU106:EngineTime)
    LEG=work/baseline-legacy-20261110-01
    S3=work/baseline-snmp3-20261110-01
    "${RUN[@]}" --run-dir <run_dir> --label legacy-operations --ioc-cwd $LEG/apcpdu-legacy/iocBoot/iocalsucl-pdus --ioc-cmd "../../bin/linux-x86_64/apcpdu <repository_root>/$LEG/run/st.operations" --ioc-env PDU1_ADDR=127.0.0.1:16101 --ioc-env PDU106_ADDR=127.0.0.1:16106 --legacy-credentials --hash-path $LEG/apcpdu-legacy/db --hash-path $LEG/apcpdu-legacy/iocsh
    python3 -I $S3/run/snmp3_run_config.py $S3/private/configuration-measure.json 16101 16106 <run_config>
    "${RUN[@]}" --run-dir <run_dir> --label snmp3 --ioc-cwd $S3/apcpdu-snmp3/iocBoot/ioctestlab-ap8932 --ioc-cmd "../../bin/linux-x86_64/apcpdu <repository_root>/$S3/run/st.measure-run" --ioc-env SNMP_CONFIG=<run_config> --hash-path $S3/apcpdu-snmp3/iocsh --hash-path $S3/snmp3-0.0.1/bin/linux-x86_64/snmp3Worker --hash-path <run_config>
    python3 -I work/baseline-direct-20261109-01/outlet_set_time.py --record ALSUCL:PDU1:OutletCmd8_ --repeat 1
    python3 -I work/baseline-direct-20261109-01/location_set_time.py --record ALSUCL:PDU106:sysLocation_ --repeat 1
    ```

    A run prints one line per phase. For each unit it prints `CUT <unit> NOW`: cut that unit's network link (not the IOC and not the unit's power), then type `cut done`. After the cut phase it prints `RESTORE <unit> NOW`: restore the link, then type `restore done`; the tool then waits for the first fresh value of that unit before it asks for the next unit. The word `stop` at either prompt ends the run. The result is in `<run_dir>`: `run.json` (phases with request time, update interval and a validity check per phase, recovery times, `stopped_by`, `complete` with `incomplete_reasons`, and `credential_file_left`) with `camonitor.log`, `ioc.log` and the relay logs. The tool prints `WARNING` 30 s into a cut when the cut unit still answered after the 20 s grace; Ctrl-C ends the run and still writes the result; further signals during that stop are ignored. The exit code is 0 for a complete run, 3 when a stop rule, `stop`, Ctrl-C or a termination signal ended it (also for `--check-only`), 4 when the run ended but a phase check is invalid (`run.json` has `complete: false` and the reasons), 5 when the credential file could not be removed (the tool prints the directory to remove by hand and records it in `run.json`), and 1 for a failed `--check-only` or for an uncaught error such as a failed write of `run.json` (a traceback and no `run.json`).
9. Compare the `inputs.json` of each run with the files on disk, then write the comparison tables and record the results here, with the complete or incomplete verdict of each run. From the repository root, for each `<run_dir>`, no output means every listed file matches, and a changed or missing file is printed as `FAILED`:

    ```bash
    python3 -I -c "import json,sys;[print(h+'  '+p) for p,h in json.load(open(sys.argv[1]))['files'].items()]" <run_dir>/inputs.json | sha256sum -c --quiet
    ```

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Net-SNMP direct read | `run_device_phases.py` phase 0, 1 and 2 | Both laboratory units, Net-SNMP 5.9.4 | Result files with response times per group size and requests in flight |
| T2 | Net-SNMP direct write | SET sender on `sysLocation` | PDU2 only | SET request times; original value restored; PDU1 refused |
| T3 | Legacy IOC, defaults | Four phases with the module defaults | Both units, installed legacy module | Request time and interval per phase; observed stall of the module on a cut unit |
| T4 | Legacy IOC, operations values | Four phases with the laboratory IOC values | Both units, installed legacy module | Request time and interval per phase |
| T5 | Legacy SET | `stringout` on `sysLocation` of PDU2 | PDU2 only | Time from the put to the changed device value; alarm state on a SET timeout |
| T6 | Net-SNMP direct write | `outlet_probe` on outlet 8: cycles of off and on, then one reboot | PDU1 outlet 8 only | Time of each SET and of each state change; original state restored; any other label refused |
| T7 | Legacy outlet 8 SET | Put to the outlet 8 command record of PDU1, then restore | PDU1 outlet 8 only, installed legacy module | Time from the put to the changed device state; alarm state on a SET timeout; original state restored |
| T8 | snmp3 `0.0.1`, operations values | Four phases with the measurement IOC | Both units, `0.0.1` | Request time and interval per phase |
| T9 | snmp3 outlet 8 SET | Put to the `longout` on the outlet 8 command object of PDU1, then restore | PDU1 outlet 8 only, `0.0.1` | Time from the put to the changed device state; alarm state on a SET timeout; original state restored |

T3, T4 and T8 each end with a run that is complete or incomplete as defined in the Completion Criteria. Not exercised and therefore unresolved: termination of the tool by SIGKILL or loss of the host (`run.json` would then be absent while the incremental logs and `inputs.json` remain, and the run would be incomplete). A run holds on the order of ten thousand relay lines per unit (estimated from 195 lines in the 21 s check run of PDU1), so no evidence-volume rehearsal is planned.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-10 | Both units, Net-SNMP 5.9.4, 358 objects, 5 repeats, tool stop rules | Pass | Phase 0: PDU1 p50 18.4 ms, PDU2 p50 2.3 ms. Phase 1, per-object time at the best K: PDU1 9.46 ms (K 48, K* 16), PDU2 0.344 ms (K 40, K* 40); request p50 at K 48: PDU1 403 ms, PDU2 21.4 ms; no stop and no error status up to K 48; isolated lost requests: PDU1 K 8 and K 12, PDU2 K 20. Phase 2: one pass of 358 objects takes about 3.4 s on PDU1 at K 48 independent of requests in flight (request time 407, 810, 1207 ms for 1, 2, 3 in flight) and 117 to 177 ms on PDU2 (K 40: 161 ms with 1 in flight, 117 ms with 2); no stop; one lost request on PDU1 (2 in flight, K 8). Results under `work/baseline-direct-20261109-01/` |
| T2 | 2026-11-10 | PDU2 only | Pass | `set_probe` refused PDU1 before opening a session (exit 5). On PDU2, 10 SETs on `sysLocation` all succeeded with no error status, every readback matched, and the original value (5 bytes) was restored and verified. SET request times in ms: 150, 469, 735, 737, 732, 732, 733, 725, 800, 793 (about 732 ms after the first two), against 2.3 ms (phase 0 p50) to 6.0 ms (largest phase 1 K 1 p50) for a single-object GET on PDU2. Result in `work/baseline-direct-20261109-01/set_probe.json` |
| T3 | Not run | Both units, installed legacy module | Pending | The module defaults printed by the installed binary: `SessionRetries 5`, `SessionTimeout 10000000`, `PassivePollMSec 2000`; the phases are not run |
| T4 | Not run | Both units, installed legacy module | Pending | none |
| T5 | Not run | PDU2 | Pending | none |
| T6 | 2026-11-10 (read-only form, one cycle, then three cycles and one reboot) | PDU1 outlet 8 | Pass | Read-only run: initial state on, reboot duration 5 s. First run, one cycle without a reboot: off SET 90.7 ms, state read as off after 865.7 ms, on SET 87.2 ms, state read as on after 1324.5 ms, original state restored, exit 0; its printed line was not saved. Second run, three cycles and one reboot (logged as 2026-11-11T06:18:03Z in `work/baseline-direct-20261109-01/outlet_probe.log`): off SET 85.9, 88.9 and 89.1 ms, state read as off after 1367.4, 497.8 and 1252.5 ms, on SET 92.5, 89.5 and 92.6 ms, state read as on after 467.2, 502.4 and 747.2 ms; reboot SET 89.6 ms, state read as off after 496.9 ms and as on again after 5460.8 ms; original state restored, exit 0. A label other than PDU1 and a missing write flag were refused without a datagram when the sender was tested. Each further write run awaits the owner's confirmation |
| T7 | Not run | PDU1 outlet 8 | Pending | none |
| T8 | Not run | Both units, `0.0.1` | Pending | none |
| T9 | Not run | PDU1 outlet 8 | Pending | none |

Preparation observed on 2026-11-10 (no device contacted, no `iocInit`): the legacy IOC exported from the application repository branch `refactor/snmp-db-processing`, built against the distribution Base and linked to `libdevSnmp.so` of `snmp-1.1.0.4ja` and the system `libnetsnmp.so.40`, was started with the planned loader switches and its loaded records listed with `dbl`. Each unit loads 255 records: 148 `longin`, 10 `ai` and 5 `stringin` use the SNMP device support (163 inputs), 27 `bo` use `Soft Channel`, and no output-type record uses the SNMP device support. Before the change to the export, the load template carried one record with an SNMP output link (the master control command), which is why it is removed from the exported copy; the removal is 23 lines of the template and is saved with the results.

Preparation of step 3 observed on 2026-11-10 (no device contacted, no `iocInit`): the tag `0.0.1` of this repository and the application IOC of the application repository commit `3314b1e` were exported with `git archive` and built against the same distribution Base as the legacy IOC, so both steps run on one Base build and the system Net-SNMP 5.9.4. The application snmp3 IOC groups its inputs into polling tiers and group counters: 366 bindings per unit and 396 inputs per unit, of which 133 objects per unit have the same OID as a legacy input; 30 legacy inputs (24 outlet control command values, 5 identity strings of the older MIB, the master control switch value) have no binding in it. Reducing that IOC to the legacy objects would require changing its generator, so it is not used for the baseline. By owner decision of 2026-11-10, step 3 runs a measurement IOC built in this milestone: one snmp3 input per legacy SNMP input, with the same record name and the same OID (163 per unit, 326 in all). Under `work/baseline-snmp3-20261110-01/`, `measure/gen_measure.py` generates `measure/measure.db` and `private/configuration-measure.json` from the loaded legacy record list, the startup script is `apcpdu-snmp3/iocBoot/ioctestlab-ap8932/st.measure`, and the load check output is `measure/check.out`. A legacy record scanned every 10 seconds keeps that scan; a legacy `Passive` record, which the legacy driver polls every `PassivePollMSec` (2000 ms), gets `SCAN` `2 second`; a legacy `ai` conversion is not applied, so values are read as integers. The load check without `iocInit` shows 329 records, all with the snmp3 device support and one of them an output record: the 326 inputs, `longin` (316) or `lsi` (10), with the legacy record names, one `EngineTime` record per unit of type `longin` on `snmpEngineTime` with `SCAN` `2 second` for the interval measurement, and one `longout` named `OutletCmd8_` on the outlet 8 command object of PDU1 through a `set` binding. The configuration uses `timeoutMs` 2000 and `retries` 1 as the laboratory IOC of step 2. The two steps then read the same signals; the polling tiers of the application IOC are not part of the baseline. The Passive-to-2-second equivalence is an assumption to be checked against the request interval observed at the wire in step 2.

Preparation of the outlet 8 write observed on 2026-11-10: `work/baseline-direct-20261109-01/outlet_probe.c` is built to `bin/outlet_probe` and run through `outlet_probe.py`, which passes the write argument as yes for a read-only run (`CYCLES 0`) and otherwise only when `private/outlet8.json` holds `{"write": true}`. Run against the repository test agent `snmp3NativeAgent`, with the object identifiers of the program replaced at build time by a writable object of the agent (the agent registers device identifiers read only), it completed three cycles and restored the original state, ignored a reboot request from an outlet that starts off, refused to write from an unknown initial state, and after a reboot that never settled reported failure and restored the original state. Refusals (another label, no write flag, out-of-range arguments) sent no datagram, counted with a local UDP listener. The reboot of a device that goes off and on is not covered by this agent. A read-only run on PDU1 returned outlet 8 initial state on and a reboot duration of 5 s; no SET was sent.

Preparation of the outlet 8 records observed on 2026-11-10 (no device contacted, no `iocInit`): the legacy copy loads `db/pdu_outlet_write.template` with `CHAN=8` for PDU1 in `iocBoot/iocalsucl-pdus/st.check8` under `work/baseline-legacy-20261110-01/apcpdu-legacy/`, and the load check in `work/baseline-legacy-20261110-01/check8.out` lists 515 records, the 510 of the unchanged export and the 5 of the template, with exactly one SNMP output-type record, the `longout` `OutletCmd8_` of PDU1 on the outlet 8 command object. The snmp3 measurement IOC gets its `longout` from `gen_measure.py`, and `measure/check.out` under `work/baseline-snmp3-20261110-01/` lists 329 records with one output record, the same `OutletCmd8_`. The first write run of `outlet_probe` on outlet 8 of PDU1 on 2026-11-10 was one off and on cycle without a reboot: the initial state on was read again at the end, the off SET took 90.7 ms and the state read as off after 865.7 ms, the on SET took 87.2 ms and the state read as on after 1324.5 ms.

Observation of the legacy read formats on 2026-11-10: the installed legacy module was started with `iocInit` over SNMPv3 against the repository test agent `snmp3NativeAgent`, with test credentials and a test object of the agent in place of a unit object, under `work/baseline-legacy-20261110-01/formatcheck/`, where `run_format_check.bash` starts both, runs the checks and writes `results.txt`. An `INTEGER` input and a `Gauge32` input (value 4294967295) read without alarm, the integer output format `( 40 i` and the string output format `( 40 s` changed the agent objects to the put values (7 and `hello`), and a `Timeticks` object left the value 0 with a read alarm for the `longin` and for the `ai` record under every declared type (`Timeticks`, `TimeTicks`, `Gauge32`, `INTEGER`, `Counter32`, `Unsigned32`). The legacy module therefore cannot read `sysUpTime`. `snmpEngineTime` (`1.3.6.1.6.3.10.2.1.3.0`, `INTEGER` seconds) answers on both units and advances between reads (PDU1 3541906 and 3541908 s, PDU2 3451181 and 3451184 s, in `work/baseline-direct-20261109-01/engine-time.txt`), so both measurement IOCs use it as the freshness record and the stop rule reads it. The legacy read of `snmpEngineTime` itself ran against both units on 2026-11-11 (the short check of item 5, below).

Preparation of the legacy freshness and write records observed on 2026-11-10 (no device contacted, no `iocInit`): `measure-engine.db` (`EngineTime` on both units) and `measure-location.db` (`sysLocation_` `stringout` on the unit without load, record prefix `PDU106`) under `work/baseline-legacy-20261110-01/` are loaded by `st.check9` next to the outlet 8 records, and `check9.out` lists 518 records with exactly two SNMP output-type records, `OutletCmd8_` of PDU1 and `sysLocation_` of the unit without load. The snmp3 measurement IOC loads its `EngineTime` records and lists 329 records in all.

Preparation of the run tools observed from 2026-11-10 to 2026-11-11 (no unit written; the units were read only for `snmpEngineTime` and `sysUpTime`). The tools run against two repository test agents standing for the units, with test credentials. The tests are in `work/baseline-direct-20261109-01/test-runner/`, run from that directory with `python3 -I`: `test_phase_runner.py <scenario>` (the scenarios are listed in its header), `credfail_test.py`, `savefail_test.py`, `doublesignal_test.py`, `mutation_check.py` (the mutation table with the assertion of each row; `mutation_check.py M1 B1` runs rows by id), `doc_command_check.bash`, `doc_command_check_snmp3.bash` and `relay_overhead.py`. Results on the final bytes of the tool (SHA-256 beginning `4e56357c`), with the exit code; each run's record is `run-<scenario>-<time>/` in that directory, and the runs of the final bytes are those whose `inputs.json` lists that hash for `phase_runner.py`:

| Scenario | Outcome | Exit |
| --- | --- | --- |
| `full` | Complete run, phases A, B, D, C, D; recoveries 3.48 s and 3.93 s; update intervals about 1000 ms; interval count 0 on the cut unit; every relayed request answered | 0 |
| `check` | Both records advancing | 0 |
| `nocut` | `cut done` typed without any cut: `complete: false`, both cut phases invalid, warning during each cut | 4 |
| `late`, `slowrec` | Unit answers 2 s and 12 s after the typed restore, silence limit 10 s shorter than the cut: complete runs, recoveries 13.5 s and 12.9 s in `slowrec` | 0 |
| `decrease`, `stop`, `silence`, `term`, `int` | Stop with its reason and a written `run.json`; a phase ended early is marked so, not judged by its replies | 3 |
| `relaydeath`, `relaycut` | Relay of PDU1 killed during phase A or instead of a cut: stop `relay of PDU1 exited` | 3 |
| `doublesignal` | Second signal while the tool stops an IOC that ignores SIGTERM is ignored; `run.json` written (also for an interrupted `--check-only`) | 3 |
| `credfail` | Credential directory not removable: processes stopped, `run.json` written, directory printed | 5 |
| `savefail` | `run.json` not writable: the error, no `run.json`, every process stopped | 1 |

Thirteen mutations of scratch copies, each aimed at one check of the tool, each failed the assertion named for it before the run, while the unchanged copy satisfied it in the same scenario; the 14 baseline rows and 13 mutation rows are in `test-runner/mutation-result.txt`. The exit code 4, the `WARNING` line and the contents of `inputs.json` beyond the relay script have no mutation row.

- The tool writes `inputs.json`, checks each phase and writes `run.json` on a termination signal. The interval script gives 1000.001 ms as the median interval of the test output.
- The startup scripts `st.defaults`, `st.operations` and `st.measure-run`, generated by `make_run_scripts.py`, reach `iocInit` under the tool with their traffic passing the relays; the snmp3 script states loopback isolation of Channel Access and PVA.
- The timers refuse a missing flag, a record name outside the outlet 8 or `sysLocation` pattern and a repeat count above 5 without sending a datagram; against the legacy format-check IOC and the test agent they measured four transitions of 50 to 76 ms (outlet 8) and three of 66 to 71 ms (`sysLocation`), including the 40 ms start of `caput`, and restored the original value (`work/baseline-direct-20261109-01/test-outlet/set_time.log` and `loc_time.log`).
- The credential file, tested with the real tool, the installed legacy module and the test agents, was created with mode 600 in a mode-700 tmpfs directory and removed 0.5 s later at `iocInit`; both units read an integer object through the relays without an alarm (26 paired requests on each relay), and no directory remained after the run, after an IOC command that fails to start, or after termination while the file existed. The test agent does not serve `snmpEngineTime`, so an integer object stood in.
- The item 8 commands, run as written with the test agents, wrote `inputs.json` (51 files including `libdevSnmp.so` for the legacy command; 17 including `snmp3Worker` and `libsnmp3.so` for the snmp3 command, whose run configuration has 329 bindings and loopback endpoints only) and ended with exit code 1, a failed short check, because the test agent does not serve `snmpEngineTime`.
- The short check of item 5 ran on the units on 2026-11-11 (`--check-only` with `st.operations`, with the tool as it was before `inputs.json` and the phase checks; results in `work/baseline-legacy-20261110-01/results/check-operations-1/`): both `EngineTime` records read without an alarm and advanced (10 values on PDU1 and 13 on PDU2 in 20 s), the credential file was removed 0.5 s after its creation, and 97 of 98 requests on PDU1 and 74 of 74 on PDU2 were answered; the open request is the last line of the relay log.

##### Closure Evidence

- none

##### GitHub Projection

Title: Measure the baseline on both laboratory units
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-11T06:36Z; issue updatedAt 2026-11-11T06:32:14Z; title, label, assignee jeonghanlee and milestone verified with `gh issue view` after creation.

#### M3 - Two-device isolation measurement on the current product

Origin: 1.0.0 / M2
Identity History: M2 -> M3 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #3 - https://github.com/jeonghanlee/snmp3/issues/3
Status: Blocked

##### Summary

Guarantee A states that one blocked, slow or absent address changes neither the IOC thread nor another address. The qualification record of `0.0.1` holds loss, late-response and healthy-address observations on fixtures; no run with two physical units exists. This unit measures, on the laboratory IOC with the NMC2 and the NMC3 unit, the record cadence of one unit while the other is disconnected and while it is reconnected, with the existing tools. The result is the reference for M11 and for comparison with the legacy observation of problem 2.

##### Scope

- One IOC, both units, the product as it stands when M3 runs, the monitored record population of the laboratory loaders.
- Three phases of at least 300 s each: both units up, NMC2 disconnected, NMC3 disconnected.
- Record per phase: processing interval and value-change interval of one record per unit, worker containment and relaunch counts, and request outcomes.

Out of scope: performance criteria (M13), a product change, a new tool before an existing tool is shown insufficient (D4).

##### Completion Criteria

- For each phase the healthy unit's processing interval median and 95th percentile are recorded and the ratio to the both-up phase is at most 1.1, or the deviation is recorded with its cause.
- The disconnected unit's records show the configured alarm with its message within one deadline of the disconnection.
- The healthy unit's results are tabulated next to the step 2 and step 3 results of M2 for the same phases.

##### Dependencies And Decisions

- G1 open; resume as Not started.
- D4 selects the tools.
- M2 provides the reference values of the legacy module, of Net-SNMP read directly and of `0.0.1` on its measurement IOC (steps 1 to 3); M3 compares its phases with the results of steps 2 and 3.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Select the record per unit and the capture method from `docs/snmp-performance/module-database-procedure.md`.
2. Run the three phases; disconnect by removing the unit's network link, not by stopping the IOC.
3. Record intervals, outcomes and worker counts per phase under `work/` and summarize here. Put the summary in a table next to the step 2 and step 3 results of M2 for the same phases.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Device cadence | Three 300 s phases with the existing capture tools on the laboratory IOC | Laboratory IOC, both units, the product as it stands when M3 runs | Healthy unit ratio at most 1.1 in each phase |
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

#### M4 - Request sizing with advertised message size and tooBig split

Origin: 1.0.0 / M3
Identity History: M3 -> M4 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #4 - https://github.com/jeonghanlee/snmp3/issues/4
Status: Blocked

##### Summary

Problem 1 and cause B. `maxVarbinds` is a fixed per-profile value, the session does not advertise a maximum message size, and a tooBig reply is a `ProtocolFailure` without a product response. Net-SNMP sets the SNMPv3 header field `msgMaxSize` from the session `rcvMsgMaxSize` (minimum 484); with no setting the library advertises no limit, and the NMC2 card then returns malformed replies that the library drops. This unit advertises a configured size, splits the batch of an address once per tooBig reply down to a floor, and reports a tooBig at the floor as the record alarm message.

##### Scope

- Profile field `maxMessageSize` (default 1472, range 484 to 65507) set on the worker session.
- On `ProtocolFailure` with status tooBig, the scheduler halves the batch of that address for the retry, down to one varbind, and keeps the reduced batch until the worker restarts.
- Alarm message `tooBig` on the records of a batch that reaches the floor.
- Documentation of the field and the behavior in `snmp-rewrite-config.md`, `snmp-record-behavior.md` and `snmp-native-reference.md`.

Out of scope: GETBULK (M8), learning the agent limit across restarts, and changes to other error statuses.

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

#### M5 - SET outcome contract

Origin: 1.0.0 / M4
Identity History: M4 -> M5 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #5 - https://github.com/jeonghanlee/snmp3/issues/5
Status: Not started

##### Summary

Item 9 of cause B. The legacy module reported a SET as successful before sending it and dropped a timed-out SET without alarm. snmp3 has output device support for `ao`, `longout`, `int64out`, `stringout` and `lso`, but the accepted scope of `0.0.1` is GET-only. This unit qualifies the SET path: an output record completes only when the SET result arrives, a timeout or error is an alarm with its message, and consecutive puts keep their order per record.

##### Scope

- Output record completion on the SET result through the same outcome path as M4.
- Alarm messages for SET timeout, tooBig, and error status with index.
- Readback agreement: the written value equals the next GET of the same OID on the fixture agent.

Out of scope: write to a laboratory unit's outlet; the deferred long active client-write cells (M14).

##### Completion Criteria

- `RecordTest` cases for each output record type show completion on result, alarm on timeout and error, and order of consecutive puts.
- The fixture agent shows the written value on the next GET.

##### Dependencies And Decisions

- M4 must be Complete; the SET path uses the same outcome and message additions.

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

#### M6 - Reply time stamp on records

Origin: 1.0.0 / M5
Identity History: M5 -> M6 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #6 - https://github.com/jeonghanlee/snmp3/issues/6
Status: Not started

##### Summary

Problems 4 and 5, cause C. A record's TIME is its processing time, so a client cannot tell when the value was read from the device. Base 7.0.10 lets device support set the time stamp when `TSE=-2`. This unit carries the worker's receive time of each reply through the IPC result and writes it into the record when `TSE=-2`; `TSE=0` records keep Base behavior.

##### Scope

- Receive time (monotonic and wall clock) in the native result and the IPC result frame.
- Device support writes `prec->time` from the wall-clock receive time when `TSE=-2`.
- Documentation of the field in `snmp-record-behavior.md` and `snmp-native-reference.md`.

Out of scope: `I/O Intr` (M7), UTAG, and time synchronization of the IOC host.

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

#### M7 - Reply-driven processing and driver-owned periodic acquisition

Origin: 1.0.0 / M6
Identity History: M6 -> M7 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #7 - https://github.com/jeonghanlee/snmp3/issues/7
Status: Not started

##### Summary

Problems 4 and 5, cause C. In `0.0.1` a record acquires only when it processes, so SCAN sets the acquisition period and processing precedes the value. Base 7.0.10 offers `SCAN=I/O Intr` with `scanIoRequest` and `scanIoSetComplete`. This unit lets a binding declare a period; the module issues the periodic request itself and processes the record when the reply arrives, so the record carries the reply value and time (M6) and SCAN no longer drives acquisition for those records.

##### Scope

- Binding field `period_ms` for input bindings used by `SCAN=I/O Intr` records; `get_ioint_info` in the input DSETs.
- A per-address periodic requester in the scheduler honoring the existing deadline, queue and batch rules (M4).
- `scanIoRequest` on completion and `scanIoSetComplete` to learn when processing finished before the next request.
- Documentation in `snmp-record-behavior.md` and `snmp-rewrite-config.md`.

Out of scope: output records, event-driven SNMP traps, and changing the behavior of `SCAN` periodic records.

##### Completion Criteria

- An `I/O Intr` record processes once per reply at the configured period within one deadline of jitter on the fixture agent, with TIME equal to the receive time.
- A disconnected address stops processing its `I/O Intr` records and the records show the alarm; reconnection resumes without restart.
- `SCAN` periodic records behave as in `0.0.1`.

##### Dependencies And Decisions

- M4 (batch rules) and M6 (receive time) must be Complete.

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

#### M8 - GETBULK table reads

Origin: 1.0.0 / M7
Identity History: M7 -> M8 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #8 - https://github.com/jeonghanlee/snmp3/issues/8
Status: Not started

##### Summary

A 24-outlet PDU exposes its outlets as table columns. `0.0.1` reads each OID with GET in batches of `maxVarbinds`. SNMPv2c and SNMPv3 offer GETBULK with non-repeaters and max-repetitions, so one request can return a column. This unit adds a table binding kind that the worker reads with GETBULK and maps onto the bound records by index.

##### Scope

- Binding kind for a table column with a base OID, first index, count and max-repetitions.
- Worker GETBULK request and response mapping with the existing response checks (position, tag, storage).
- Interaction with M4: a tooBig on GETBULK reduces max-repetitions for that address.

Out of scope: SNMPv1 profiles, sparse tables, and discovery of table size.

##### Completion Criteria

- The fixture agent's outlet table of 24 rows is read with one GETBULK where the advertised size allows and the values match the GET path.
- On both laboratory units the outlet column fills with the request count reported by `snmp3Report`.

##### Dependencies And Decisions

- M4 must be Complete; the response size and reduction rules are shared.

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

#### M9 - SNMPv3 localized keys and secret procedure

Origin: 1.0.0 / M8
Identity History: M8 -> M9 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #9 - https://github.com/jeonghanlee/snmp3/issues/9
Status: Blocked

##### Summary

Cause D. The module reads each passphrase from its own file owned by the IOC's effective user with no group or other permission bits, and the worker derives keys with `generate_Ku` and `netsnmp_extend_kul`. The passphrase bytes stay in worker memory, no procedure defines how the files are created, stored and rotated, and the permitted algorithms per card generation are not recorded. This unit retains only the localized keys in the worker, records the permitted algorithm sets, and documents the creation, storage and rotation procedure. Existing deployments keep passphrases in the legacy `ver3.txt` form; conversion is part of the procedure. By decision D10 the configuration itself carries the localized keys, so a configuration of this form holds no passphrase on the IOC host; the worker's derivation from a passphrase file remains for existing configurations.

##### Scope

- Worker: after `generate_Ku` and localization, keep `securityAuthLocalKey` and `securityPrivLocalKey` only and zero the passphrase buffers.
- Configuration with localized keys (D10): the configuration schema takes a localized authentication key and a localized privacy key per endpoint in place of passphrase files, because the keys depend on the engine ID of each unit; the profile keeps the user name and the algorithms that the endpoints share, and the worker opens each session with the key pair of its own endpoint without any passphrase. The key files get the same owner and mode checks as the passphrase files get today.
- One form per endpoint: an endpoint carries either passphrase files or a localized key pair, and the configuration loader rejects an endpoint that carries both. The passphrase-file form keeps working as it does today.
- Engine ID check: a localized key pair is valid for one engine ID, so the key tool records that engine ID with the pair, and the worker compares the engine ID it discovers at session open with the recorded one and sends no request after the discovery under a mismatch.
- Key tool and procedure: a tool that computes the localized keys from a passphrase, an algorithm set and the unit's engine ID once, off the IOC host, with the passphrase kept in the secret store; the procedure covers an engine ID change (card replacement or reset), which needs new keys.
- Permitted algorithm sets for NMC2 and NMC3 verified against the units.
- Procedure page: host location, creation from a legacy file, rotation (new process), repository and history exclusion, and the scans that verify it.

Out of scope: an external secret store, changing the configuration-loader file checks, and a change of any setting on the units, which access with localized keys does not need.

##### Completion Criteria

- The worker memory holds no passphrase bytes after session open, shown by the worker's own report of zeroization and a test that inspects the process image on the fixture.
- Files created by the documented procedure reach both units with the strongest algorithm set each card accepts, recorded without addresses, user names or secrets.
- A configuration that holds only localized keys reaches both units through the worker and a wrong key is rejected, recorded without addresses, user names or secrets.
- An endpoint that carries both forms is rejected when the configuration loads, and a key pair made for another engine ID is refused at session open, and no GET or SET is sent (only the engine ID discovery exchange takes place).
- Tracked-tree and history scans of this repository find no secret material, or findings have an explicit disposition.

##### Dependencies And Decisions

- G1 open; resume as Not started.
- D10 selects localized keys for the configuration.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Retain localized keys only in the worker and zero passphrases; add the report line and a test.
2. Verify SHA-256 and AES-256 acceptance on the NMC3 unit and the strongest set on the NMC2 unit.
3. Add localized key fields per endpoint to the configuration schema and the worker open path, with the one-form rule and the engine ID check, add the key tool, and test them on the fixture agent and on both units.
4. Write the procedure page, the legacy-file conversion and the engine ID change procedure; run the scans.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Worker | Process image inspection after open on the fixture agent | Linux host, ordinary product | No passphrase bytes |
| T2 | Device | Files created by the procedure, each security level and algorithm set, on both units | Laboratory IOC, both units | Accepted sets recorded |
| T3 | Tracked tree and history | Secret scan of this repository | Linux host | No secret material or disposition recorded |
| T4 | Permissions | Existing wrong-owner and wrong-mode configuration cases, repeated for key files | Linux host | Rejected as before, for key files too |
| T5 | Device and configuration | A configuration holding only localized keys, through the worker, with a wrong key as control | Laboratory IOC, both units | Right keys accepted, wrong key rejected, no passphrase in the configuration |
| T6 | Configuration | An endpoint with both forms, and a key pair recorded for another engine ID, on the fixture agent | Linux host, ordinary product | Both rejected with a message, no GET or SET sent for the mismatch |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Laboratory IOC, both units | Pending | none |
| T3 | Not run | Linux host | Pending | none |
| T4 | Not run | Linux host | Pending | none |
| T5 | Not run | Laboratory IOC, both units | Pending | none |
| T6 | Not run | Linux host | Pending | none |

Observation of localized-key access on 2026-11-11 (reads only, no unit changed): `work/baseline-direct-20261109-01/localized_key_check.py` computed the SHA and AES-128 localized keys of each unit from its existing secret files and its engine ID, read `sysUpTime` through a configuration that held only the keys, and repeated the read with one byte of the authentication key changed. The NMC2 unit and the NMC3 unit both accepted the keys and rejected the changed key by not answering; the results are in `work/baseline-direct-20261109-01/localized-key-check.txt`. AES-192 and AES-256 are not covered by this check. The authentication and the privacy secret files of the laboratory units hold identical content (compared without printing), so both localized keys derive from one passphrase; whether the key tool procedure keeps the two passphrases distinct is for the plan of this row.

##### Closure Evidence

- none

##### GitHub Projection

Title: Keep only localized SNMPv3 keys and document secret handling
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: open
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-11T17:36Z; issue updatedAt 2026-11-11T17:28:55Z; title, label, assignee jeonghanlee and milestone verified with `gh issue view` after the body edit.

#### M10 - Shared-library SONAME and symbolic-link versioning

Origin: 1.0.0 / M9
Identity History: M9 -> M10 (baseline row inserted before it, 2026-11-10)
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

#### M11 - Release 1.0.0

Origin: 1.0.0 / M10
Identity History: M10 -> M11 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #10 - https://github.com/jeonghanlee/snmp3/issues/10
Status: Not started

##### Summary

Release `1.0.0` when every guarantee of the five causes reads Implemented and the combined product is verified on both laboratory units. The release carries the changelog of M1 to M10, the annotated tag and the GitHub release. The operations deployment is a separate decision after the release.

##### Scope

- Integrated re-runs of the checks that later units may have invalidated, on the final combined candidate.
- Two-unit acceptance on the laboratory IOC with the final candidate: M3's three phases, M4's sizing, M7's `I/O Intr` cadence and M9's credentials together.
- Installation on a clean Rocky Linux 8.10 host by the documented procedure.
- Version values, tag, GitHub release, and the next register entry point.

Out of scope: operations deployment, performance acceptance (M13), the deferred cells (M14).

##### Completion Criteria

- Every Release Verification row below is Pass with evidence.
- Tag `1.0.0` and the GitHub release exist on the commit whose `CHANGELOG.md` entry is `1.0.0`.

##### Dependencies And Decisions

- M1 to M10 Complete.

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
| M4 / T1, M4 / T2 | M5, M7, M8 | request outcome and batch rules | Release Verification 2 | Split and message as specified | pending |
| M6 / T2 | M7 | completion path | Release Verification 3 | TIME equals receive time | pending |
| M5 / T1 | M7, M8 | outcome path | Release Verification 4 | SET completion and alarms as specified | pending |
| M10 / T2 | any library change after M10 | installed library identity | Release Verification 5 | Intended libraries load | pending |

##### Production Environment Tests

| Release Verification Label | Timing | System | Version | Architecture | Deployment Path | Method | Expected Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Release Verification 6 | pre-change | Laboratory IOC host with both units | candidate | x86_64 | laboratory IOC startup | M3 three phases plus M4, M7 and M9 criteria on the candidate | All criteria met | pending |
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
| Tools | M12 | Negative-control harness outcome classification | Carry-forward | Open | No | D2 | Scope decided and selected outcomes classified by real execution or retained by dated decision; [detail](#m12---negative-control-harness-outcome-classification) |
| Performance | M13 | Predictable acquisition performance and second-client impact | Carry-forward | Deferred | No | D5 | Existing performance criteria met on their identified scope or changed by an explicit decision; [detail](#m13---predictable-acquisition-performance-and-second-client-impact) |
| Records | M14 | Deferred record verification cells | Carry-forward | Deferred | No | D6 | Each listed cell has a real-path receipt or a dated retirement; [detail](#m14---deferred-record-verification-cells) |

### Backlog Details

#### M12 - Negative-control harness outcome classification

Origin: 1.0.0 / M11
Identity History: M11 -> M12 (baseline row inserted before it, 2026-11-10)
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

#### M13 - Predictable acquisition performance and second-client impact

Origin: 1.0.0 / M12
Identity History: M12 -> M13 (baseline row inserted before it, 2026-11-10)
GitHub Issue: none
Status: Deferred

##### Summary

Remove avoidable software delay and serial-request cost while preserving request ownership and avoiding unnecessary delay to another client. Readiness wakeups and declared-group dispatch are implemented. The once result passed, the sustained normalized second-client ratio failed (1.98 against 1.25), and the baseline variation, lost probe and historical 103 ms difference remain unresolved. The committed criteria, test definitions and receipts are in the [prior register](https://github.com/jeonghanlee/snmp3/blob/ba6c558a1118c792e769aed79ded7581538c3888/docs/milestone-6f015f8.md#m3---predictable-acquisition-performance-and-second-client-impact).

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

#### M14 - Deferred record verification cells

Origin: 1.0.0 / M13
Identity History: M13 -> M14 (baseline row inserted before it, 2026-11-10)
GitHub Issue: none
Status: Deferred

##### Summary

Cells excluded from the accepted record qualification and still unexecuted: Channel Access signed-minimum int64 access, maximum-capacity `lsi`/`lso` `VAL$` access, `ao`/`longout` writes, long active client writes, delayed output-simulation mode changes, and isolated rebuild and reuse beyond the existing `rebuild` clause. Their definitions are in the [prior register](https://github.com/jeonghanlee/snmp3/blob/ba6c558a1118c792e769aed79ded7581538c3888/docs/milestone-6f015f8.md#m5---deferred-record-verification-cells). M5 of this register covers the SET outcome contract; a cell it executes is recorded there and retired here by decision.

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
