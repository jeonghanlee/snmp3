# Work Register

Release line: 1.0.0
Milestone index: 1.0.0
Canonical path: `docs/milestone-1.0.0.md`
Canonical branch or ref: master
Git upstream: origin/master
Remote tracker: jeonghanlee/snmp3, GitHub milestone 1.0.0
Recorded date: 2026-11-09
Source baseline: tag `0.0.1` at `8c5bd199f3137bb3c5756c7a2c7d1640122ed788`; register reset from `ba6c558a1118c792e769aed79ded7581538c3888`

Next session entry point: Two units are in progress in `docs/milestone-1.0.0.md`. M16 is In progress: its change is on `origin/master`, T1, T2, T3 and T6 are recorded, and item 7 on the units remains, T4 (link cut and restore) and T5 (power loss and return, with engine ID and boots read before and after), which wait until the owner can cut the link and the power (start `work/m16-units/run_units.sh` before the cut, as its Dependencies And Decisions describe); then its `CHANGELOG.md` entry, the tag with the next free number (D31) and the closure, each under its own authority. M7 is In progress: D34 widened it to the whole configuration by shell command, D36 decided its open points, and its plan was accepted and its implementation authorized on 2026-11-17; parts A and B of its Implementation Plan are done and recorded, part A with seven differences from the accepted plan, of which the first waits for the owner's decision, and part B with seven, one of them a rule the owner added; part A is on `origin/master`, and the work continues at part C (the cell `reply-driven` first). The Ready rows are M8 and M9. M19, two standalone MIB tools, was added on 2026-11-17 by D35 and starts after M7. M6 was deferred to the Backlog by D28 with its measurements. M1 (tag `0.0.2`), M2, M3 (retired by D13), M4 (tag `0.0.3`), M5 (tag `0.0.4`), M10 (tag `0.0.5`), M18 (tag `0.0.6`) and G1 are complete. Commits, pushes, tags and issue actions each need their own authority.

## Scope

The goal of this release line is that every guarantee in [What snmp3 guarantees against each cause](why-snmp3.md#what-snmp3-guarantees-against-each-cause) reads Implemented, and that the implemented product is verified on one NMC2 unit and one NMC3 unit sharing one IOC. The five causes and the 23 items behind them are recorded in [The operating problems behind snmp3](why-snmp3.md) and the [Review record](legacy-review.md). The release line also includes the table-read request (GETBULK), the SET outcome contract, reply-driven record processing, and shared-library SONAME versioning (D1, D3).

Work is ordered by the code surface each unit shares with the next; the row order is the execution order and the local IDs follow it. Each completed M row adds a `CHANGELOG.md` entry and an annotated tag with the version named in its detail; the final release is `1.0.0`.

Out of scope: performance criteria and acceptance (Backlog M13), the deferred record verification cells (Backlog M14), the negative-control harness classification (Backlog M12), a new measurement tool before an existing one is shown insufficient (D4), and any deployment to the operations network before `1.0.0`.

## Milestone

### Work

| Group | ID | Work unit | Type | Status | Ready | Deps | Done when / Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Gate | G1 | Laboratory NMC2 and NMC3 units reachable through snmp3 | External gate | Complete | No | D12 | Both units answer a GET through the laboratory IOC with snmp3 `0.0.1`; [detail](#g1---laboratory-nmc2-and-nmc3-units-reachable-through-snmp3) |
| E | M1 | Probe spawn environment equal to the worker environment | Milestone | Complete | No | D14, D15 | Probe and worker receive the same environment list built from the worker's libraries; the changed tests and the ordinary matrix stage pass; tag `0.0.2`; [detail](#m1---probe-spawn-environment-equal-to-the-worker-environment) |
| Baseline | M2 | Baseline measurement on the two laboratory units | Milestone | Complete | No | D8, D9, D11 | Net-SNMP 5.9.4 read directly and the installed legacy module measured on both units, with the response time of a request and the interval of new values recorded; no product change; [detail](#m2---baseline-measurement-on-the-two-laboratory-units) |
| A | M3 | Two-device isolation measurement on the current product | Milestone | Complete | No | G1, D4, M2, D13 | Measured cadence of the healthy unit with the other unit disconnected; [detail](#m3---two-device-isolation-measurement-on-the-current-product) |
| B | M4 | Request sizing with advertised message size and tooBig split | Milestone | Complete | No | G1, D16 | At `maxVarbinds=61` the records of both units fill without manual tuning; a rejection, read or unreadable, halves the batch and shows on the record; tag `0.0.3`; [detail](#m4---request-sizing-with-advertised-message-size-and-toobig-split) |
| Recovery | M16 | Recovery when a unit returns after a power or network loss | Milestone | In progress | No | G1, D18, D22, D23, D24, D25, D31 | With the IOC left running, the records of a unit return to normal by themselves when the unit comes back with its engine ID unchanged; a changed engine ID is shown and recorded, and until the IOC is restarted no SET is sent and no value is taken except system information; tag: the next free number at its closing; [detail](#m16---recovery-when-a-unit-returns-after-a-power-or-network-loss) |
| B | M5 | SET outcome contract | Milestone | Complete | No | M4, D26, D27 | Output records complete only on the SET result; a refused SET and a SET without a reply are alarms with a message; consecutive writes follow Base; tag `0.0.4`; [detail](#m5---set-outcome-contract) |
| C | M7 | Shell-command configuration, MIB names and reply-driven processing | Milestone | In progress | No | M4, D28, D30, D32, D34, D36 | An IOC is configured by shell commands and record links without a file, objects are given by MIB name, and `SCAN=I/O Intr` records process on the reply of a subscription; tag: the next free number at its closing; [detail](#m7---shell-command-configuration-mib-names-and-reply-driven-processing) |
| Performance | M8 | GETBULK table reads | Milestone | Not started | Yes | M4 | A table read uses one GETBULK request where the profile allows it; tag: the next free number at its closing; [detail](#m8---getbulk-table-reads) |
| D | M9 | SNMPv3 localized keys and secret procedure | Carry-forward | Not started | Yes | G1, D10 | Worker retains localized keys only; the configuration carries localized keys and a documented procedure creates the files that reach both units; tag: the next free number at its closing; [detail](#m9---snmpv3-localized-keys-and-secret-procedure) |
| Release | M10 | Shared-library SONAME and symbolic-link versioning | Carry-forward | Complete | No | D3, D29, D31 | Installed libraries carry SONAMEs and links per policy; consumers load them; tag `0.0.5`; [detail](#m10---shared-library-soname-and-symbolic-link-versioning) |
| Records | M18 | Module state as records | Milestone | Complete | No | D30, D32, D33 | The runtime state and, per endpoint, the engine ID state and the request size are readable as records of databases the module installs, and the runtime report names each endpoint; [detail](#m18---module-state-as-records) |
| Tools | M19 | MIB tools: database check and database draft | Milestone | Not started | No | M7, D35 | A standalone program checks a database against MIB files for names, types and access before an IOC starts, and one writes a draft of records from MIB files for the author to finish; [detail](#m19---mib-tools-database-check-and-database-draft) |
| Release | M11 | Release 1.0.0 | Milestone | Not started | No | M1, M2, M3, M4, M5, M7, M8, M9, M10, M16, M18 | Integrated re-runs and the two-unit acceptance pass; tag `1.0.0` and GitHub release exist; [detail](#m11---release-100) |

Current work tally: 7 Complete, 2 In progress, 4 Not started, 0 Blocked; G1 Complete. Ready rows: M8, M9. Backlog contains 1 Open and 4 Deferred rows and is excluded from this tally.

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
| D12 | Accept the baseline run of the snmp3 `0.0.1` measurement IOC on both units (every request answered, the `EngineTime` records advancing) as the observation of G1, in place of a separate `caget` on the laboratory IOC of the application repository. | 2026-11-13 |
| D13 | Retire the two-device isolation measurement on the current product: the baseline run of the snmp3 `0.0.1` measurement IOC on both units already records the cadence of the healthy unit while the other is cut, and a further run on the laboratory IOC would not change the reference. The three phases and criteria of that row stay the acceptance check of the release row. | 2026-11-13 |
| D14 | Give the probe and the worker one environment list built by one function from the libraries of the worker executable: the worker path must be set before `snmp3Load`, the probe runs with that list, a load without a worker path or with a worker whose `ldd` fails is rejected, and a change of the worker path after a successful load is rejected. The sanitizer entries are not passed to the probe, which has no qualification, and stay unproven. | 2026-11-13 |
| D15 | Accept the differences between the accepted plan of M1 and the work as run, as listed in its detail: the order of test writing and code change, the helper design, the scope of the startup-order search, the construction of the four preflight cases of `record-startup`, and the verification Base (the public 1.5.0 tree, after the earlier Base tree was removed). The ordinary-stage criterion is met by 40 PASS and 1 NOT APPLICABLE (`tsan`) cells whose names and statuses are equal on the unchanged and the changed tree. The plan text stays as accepted. Add the `0.0.2` entry of `CHANGELOG.md` to the commit that carries the change, so that tag `0.0.2` names it. | 2026-11-13 |
| D16 | Plan of M4 within its scope: the batch is halved rounded down with a minimum of 1; NMC3 is judged unchanged by a split count of 0 and no `tooBig` message on its records at `maxVarbinds=50`, and the interval of its `snmpEngineTime` record is recorded next to the T8 figures of M2 (PDU2, phase A: p50 4936, p95 10432, maximum 11403 ms) with no acceptance threshold, because performance criteria are out of the release scope (M13); the split count is the cumulative number since the IOC started and `snmp3RuntimeReport` shows it; the advertised size is checked on the wire for an SNMPv3 profile; T3 runs through the measurement IOC of the baseline rebuilt against the new module with the two startup lines swapped, and its worker-log check is replaced by the record state because production redirects the worker log to `/dev/null`. | 2026-11-14 |
| D17 | M4 also adjusts on a timeout, after the diagnosis of both units on 2026-11-14: a timed-out request during which the library counted an unparsable reply (`snmpInASNParseErrs`) is a rejection that could not be read and halves the batch like a tooBig reply; a timed-out request with no such count is no reply from the unit and leaves the batch unchanged. The record message names which of the three it was. T3 runs both units with the bindings of the measurement IOC (164 read bindings per unit) at `maxVarbinds=61`. This revises the plan accepted under D16, whose rounding, split count and NMC3 comparison stay. | 2026-11-14 |
| D18 | Add the recovery of a unit that returns as work of release 1.0.0 (M16): an IOC that stays up while a unit loses power or its network link must return to normal by itself when the unit answers again. The first step is to observe whether the current product already does. | 2026-11-14 |
| D19 | New tests are written in bash, which starts and stops the processes, and Perl, which reads the records and judges; they join the matrix as new cells beside the existing Python cells, and T1 of M4 is the first. Moving the existing cells to bash and Perl is separate work (M17) in Backlog and starts by the owner's decision. | 2026-11-14 |
| D20 | M4: the `tooBig` message is set on the records of every request that the unit rejects with tooBig, not only on a batch that has reached the floor, and `unreadable rejection` and `no reply` are set the same way. The record did fail for that reason in that round, and a message at the floor only would need the batch limit passed to the record side. This accepts difference 2 of M4; the Scope line of M4 keeps its earlier wording. | 2026-11-14 |
| D21 | M4: differences 1, 3, 4, 5 and 6 between the accepted plan and the work as run are accepted as listed in the detail of M4. Difference 7, the batch limit shared by endpoints of one host address, is left as it is; the units in use have different addresses. | 2026-11-14 |
| D22 | M16 runs next, before M5. Its tag becomes `0.0.4`, and the tags of M5, M6, M7, M8, M9 and M10 each move up by one, to `0.0.5` through `0.0.10`, so that tags rise in the order of execution; the M16 row moves to follow M4. The IDs do not change. | 2026-11-14 |
| D23 | M16, the owner's policy for a unit that returns: the IOC is never left dead, and it tells apart what came back. A unit whose engine ID is unchanged is recovered fully, also when its engine time went back. A changed engine ID at the same address is suspicious, because the network gives an address only to a registered MAC address: the IOC keeps basic communication, asks whether it is the same unit, never returns to normal operation by itself, shows the change on the records and records it so that the user can restart the IOC and trace what happened. In that state a SET is never sent, values read from the unit are not put into records, and only bindings marked as system information (firmware, name, MAC address, status) are read and shown, with an alarm. A responder that fails authentication is reported as well. A profile with a passphrase and a profile with localized keys only must both work; with localized keys only the changed unit cannot be authenticated and is reported as unverified. | 2026-11-14 |
| D24 | M16, rules the policy of D23 left open, filled in at the owner's instruction and open to change in the plan review. (1) An SNMPv3 endpoint asks the address for its engine ID again after 2 consecutive failed requests; the least interval between two questions follows the beacon of the Channel Access server of EPICS Base (`rsrv/online_notify.c`): it starts at 0.02 s, doubles after each question and stays at 15 s, the default of `EPICS_CAS_BEACON_PERIOD`, and it starts again after a recovery. (2) In the fixture, recovery and the entry into a changed state are due within 30 s of the agent's return. (3) In a changed state every record of the endpoint is COMM/INVALID: the records whose values are withheld keep the value they had, and the system information records take their values. (4) The messages are `no reply`, `engine ID changed: restart IOC` and `engine ID unverified: restart IOC`. (5) One log line is written when a state is entered, none per failed request. (6) A changed state lasts until the IOC is restarted, also when the earlier engine ID answers again. (7) A profile that cannot authenticate the new engine ID, because it holds localized keys only, has no authentication, or names its engine ID in the configuration, is unverified; with the engine ID named in the configuration a restart does not accept another engine ID, the configuration must be changed. (8) The tests of this unit are limited to what the purpose needs; what is left out is listed with its reason in the detail. | 2026-11-14 |
| D25 | M16: the seven differences between the accepted plan and the work as run are accepted as listed in the detail of M16. With difference 2 an endpoint that has no system information binding is unverified at once, which adds a case to D24 (7). | 2026-11-15 |
| D26 | M5: consecutive writes to one output record follow EPICS Base as it is. A put that waits for completion is sent on its own, in order; plain puts that arrive while a request of the record is pending are merged by Base into one further processing with the latest value. The module does not queue values of its own, so that no old value reaches a unit late. The criterion of issue #5 that every consecutive put is sent is replaced by this. | 2026-11-15 |
| D27 | M5: differences 1 and 2 between its accepted plan and the work as run are accepted as listed in its detail. Tags follow the order of closing: M5 closes first and takes `0.0.4`, and M16, which waits for the runs on the units, takes `0.0.5`; the tags of M6 to M10 stay as D22 set them. | 2026-11-16 |
| D28 | M6 is deferred and moved to the Backlog: measured on 2026-11-16 with the fixture agent and on both units, the TIME of a record with `TSE=0` already lies within 1 ms of the arrival of its reply, so the fault that M6 answered is absent and what remains is an exactly equal time stamp for the values of one reply. M7 no longer depends on M6, because a record that processes on the arrival of a reply takes its time there. The measurements are kept in the detail of M6 for a later look. The tags of the units that follow are taken in the order of closing; with M6 out, M7, M8, M9 and M10 are planned as `0.0.6` to `0.0.9`. | 2026-11-16 |
| D29 | M10: the shared libraries are versioned the way EPICS Base does it, with `SHRLIB_VERSION` in the Makefiles: the real file is `lib<name>.so.<ABI>`, its SONAME is the same name, and `lib<name>.so` is a link to it, two levels and not the three of the earlier wording. One ABI number serves the three libraries and starts at 1. It is independent of the release number and is raised only for an incompatible change of an interface that a release has promised; until `1.0.0` no interface is promised, so it stays 1 through the units that still change the worker protocol. | 2026-11-16 |
| D30 | An IOC application uses snmp3 the way it uses the other support modules: it adds the database definition file and the library, runs the registered shell commands, and reads the state of the module from records. It includes no header of the module and reads no C++ object of it. The module therefore offers its state as records (M18), and what `1.0.0` promises to an application is the database definition file, the shell commands and those records. The application `apcpdu`, whose main reads `snmp3::Runtime` through a header taken from the module's source directory, is written again in its own repository with the IOC template generator. | 2026-11-16 |
| D31 | Tags follow the order of closing (D27, D28): M10 closes before M16 and takes `0.0.5`. Only the next number is fixed: every other open unit, M16, M7, M8, M9 and M18, takes the next free number when it closes, and the numbers that D22 and D28 planned for them no longer hold. | 2026-11-16 |
| D32 | M18: the module offers four values as records, the runtime state, and per endpoint the state after a change of engine ID and the two values of the request size of its address. A record names an endpoint by its `id` in the configuration. The records are scanned periodically with `PINI=YES` and read the state the IOC process holds; `SCAN=I/O Intr` comes with M7. The states are `mbbi` records whose severities the database sets, the numbers are `longin` records, and the databases are installed into `db/` and loaded with `dbLoadRecords`. Records for the worker, an archive tag and a PVA group are left to the application. `snmp3RuntimeReport` gains one line per endpoint with its `id`, its address number, its state and the time of a change, chosen by the implementer on the owner's word; the lines that exist keep their text. | 2026-11-16 |
| D33 | M18: differences 1 to 5 between its accepted plan and the work as run are accepted as listed in its detail. | 2026-11-16 |
| D34 | M7 is widened to the whole configuration of the module by shell command and is finished as one unit: sessions, options and subscriptions by command in the form of the `opcua` module of the site distribution (not of the earlier `snmp` module), the object in the record link, object names resolved through MIB files by a function of the module, and processing on the arrival of a reply. The JSON file stays as the form that gives the same definitions at once. A MIB is looked for in the place of the IOC, then of the module, then of the system, and the report names where each came from. The period of acquisition belongs to a subscription, not to a binding of the file and not to the record link. | 2026-11-17 |
| D35 | A unit of its own, M19, makes two standalone programs out of the owner's experience with SNMP IOC databases, where the type, the access and the name of every object were copied from the MIB by hand: one checks a database against MIB files for names, types and read or write access, before an IOC is started, and one writes a draft of database records from MIB files. The draft is a starting text that the author of the database finishes, not a finished database; the check is the main program of the two. It is independent of M7 in purpose and uses the link form and the MIB resolution that M7 makes. Inside the IOC the type of an object given by name comes from the MIB with no entry by hand, and the record is checked against the MIB at initialization (M7). | 2026-11-17 |
| D36 | M7, six points of its plan: (1) the repository of the module carries no standard MIB file at first; its `mibs` directory is installed empty with a note, and a MIB the IOC and the module do not hold is looked for in the system; files can be added by a later decision. (2) The period of a subscription is 100 to 3600000 ms; the lower end is the fastest periodic scan of Base. (3) The first request of a subscription leaves at once when its first record joins the `I/O Intr` list. (4) An object given by number is shown with its number and the type of its link, with no MIB name, so that an IOC that uses numbers reads no MIB. (5) An IOC without a file names the probe with the command `snmp3ProbePath`, in the form of `snmp3WorkerPath`. (6) A name may carry `type=`, which then decides the type in place of the MIB, for a unit that does not follow its MIB; `snmp3Show` marks the object as overridden. | 2026-11-17 |

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

### Assignment History

| Work Identity | From Canonical | To Canonical | Target Commit | Authority Moved At |
| --- | --- | --- | --- | --- |
| 1.0.0 / M5, current ID M6 | `docs/milestone-1.0.0.md`, Milestone | `docs/milestone-1.0.0.md`, Backlog | this synchronization commit | this synchronization commit |

### Milestone Details

#### G1 - Laboratory NMC2 and NMC3 units reachable through snmp3

Origin: 1.0.0 / G1
GitHub Issue: none
Status: Complete

##### Summary

M3, M4 and M9 need one AP8932 unit with an NMC2 card and one with an NMC3 card, both reachable from the laboratory IOC host and both answering snmp3 `0.0.1` requests with the laboratory IOC's existing SNMPv3 secret files. The laboratory IOC of the apcpdu repository (commit `3314b1e`) is the known startup. The owner confirms the units are powered, on the laboratory network and configured.

##### Completion Criteria

- One GET per unit completes through the laboratory IOC with `0.0.1`, observed with `caget` on one input record of each unit, and the observation time, unit card generation and module tag are recorded here. No address, host name or credential is recorded.

##### Verification Results

| Observed At | Result | Evidence |
| --- | --- | --- |
| 2026-11-12 | Pass | Observed in the complete run of the snmp3 `0.0.1` measurement IOC of the baseline (the T8 run of the baseline measurement, started 2026-11-13T00:49Z): both units answered every request, 6895 to the NMC2 unit and 4498 to the NMC3 unit in the connected phase, and the `EngineTime` record of each unit advanced. The observation was made on the measurement IOC with `camonitor`, not with `caget` on the laboratory IOC; D12 accepts it in place of that observation. Unit cards: NMC2 and NMC3; module tag `0.0.1` |

##### Closure Evidence

- Decision of 2026-11-13 (D12): the baseline run of the measurement IOC is the observation of this gate. The owner chose it over a separate `caget` on the laboratory IOC.

#### M1 - Probe spawn environment equal to the worker environment

Origin: 1.0.0 / M1
Identity History: none
GitHub Issue: #2 - https://github.com/jeonghanlee/snmp3/issues/2
Status: Complete

##### Summary

`snmp3Load` launches `snmp3NativeProbe --capabilities` with the IOC process environment (`Config.cpp`, `posix_spawn` with `environ`), while the supervisor launches the worker with a fixed list (`PATH`, `LANG`, `LC_ALL`, `LD_LIBRARY_PATH`). A catalogue produced under the IOC environment can name a library set the worker never loads. This unit gives both spawns the same environment list.

##### Scope

- One function builds the environment list from the worker executable path and a sanitizer flag: `PATH=/usr/bin:/bin`, `LANG=C`, `LC_ALL=C`, `LD_LIBRARY_PATH` (the sorted, unique directories of the worker's libraries as `ldd` reports them, joined by `:`, the rule of `Supervisor.cpp` today) and the two sanitizer entries when the flag is set. The probe spawn in `Config.cpp` and the worker spawn in `Supervisor.cpp` both use it.
- `snmp3WorkerPath` is called before `snmp3Load`. `Config::load` rejects a load while the worker path is unset, and rejects it when `ldd` of the worker fails, before any probe starts. A change of the worker path after a successful load is rejected, so the list that the probe received is the list of the worker that starts (setting the same path again stays accepted). The probe gets the list with the sanitizer flag off, because the load has no qualification.
- Every tracked startup script, document, tool and test that calls `snmp3Load` sets the worker path first (20 tracked files by `grep` on 2026-11-13, calls and mentions together: `Register.cpp`, the C++ tests under `tests/rewrite`, the Python tests and `config/startup.cmd`, `docs/snmp-performance/tools`, and the documents that show the startup order), and `docs/legacy-review.md`, whose open finding about the probe environment (the rows at lines 101 and 122) is updated to the resolved behavior.
- Tests: the environment of a real probe process and of a real worker process compared, and the two rejections.

Out of scope: the catalogue format, the capability comparison in the supervisor, any change to what the worker loads, the `ldd` spawn of `Supervisor.cpp`, which keeps its own three-entry list, and the startup scripts of other repositories.

##### Completion Criteria

- With the sanitizer flag off, the probe and the worker are spawned with byte-identical environment lists built by one function from the worker executable; the test compares the two environments observed from the running processes, not the return value of that function.
- A load with the worker path unset, or with a worker for which `ldd` fails, is rejected with a clear error and starts no probe, and a change of the worker path after a successful load is rejected.
- Every tracked file that calls `snmp3Load` sets the worker path first, the startup examples run as written, and the open finding about the probe environment in `docs/legacy-review.md` states the resolved behavior.
- The checks of the changed tests were seen failing on the unchanged code and on each mutation of the table in the Test Plan, for the intended assertion.
- The ordinary matrix stage passes on the changed product with the same cells as on the unchanged tree.
- Tag `0.0.2` names the commit that carries the change; it is created and pushed only under separate tag authority.
- Unproven by these checks: the sanitizer entries (the probe never receives them and the worker receives them only in a qualification run), and termination of the helper by a signal during the probe.
- Compatibility fact recorded at closure: the startup of the application repository (`snmp3.iocsh` of its laboratory IOC, commit `3314b1e`) calls `snmp3Load` before `snmp3WorkerPath` and is rejected after this change until the two lines are swapped; that change is outside this repository. `tests/rewrite/test_apcpdu_startup.py` and `test_apcpdu_startup.pl` run that application startup, are not cells of the ordinary stage, and fail against an application whose startup keeps the old order; they stay unrun until the swap.

##### Dependencies And Decisions

- D14 fixes one list from one function built from the worker's libraries, the worker path before the load, the three rejections (unset path, failing `ldd`, path change after a load), and the sanitizer entries left unproven.
- Not part of this unit: the swap of the two startup lines in the application repository; the owner decides whether it becomes work of its own.
- The body of issue #2 states the earlier acceptance criteria (a `ConfigTest` case, one function); it is updated at closure under separate issue authority.
- D15 accepts the differences between the accepted plan and the work as run, which are listed here and recorded 2026-11-13; the plan text below is unchanged:
  1. Order: the product code was edited before T1, T2 and the search of T4 were written (items 3 and 4 of the plan). The observation on the unchanged code was made afterwards, on a clone of the commit with only the new test files added, and it gives the same failing checks on the two Bases that were used.
  2. Helper: the plan asked for an extra test library in a separate directory; the probe role of `snmp3EnvHelper` links Base `Com` instead (`tests/rewrite/Makefile`), and the earlier session observed directly that its set of `ldd` directories then differs from the worker's, which is the purpose of that library. The worker environment is read from `/proc/<pid>/environ` of the real `snmp3Worker` (the fallback the plan names), and the supervisor was not run once with the helper as the worker.
  3. Search scope of T4: a Markdown file is searched per code block, any other file by a worker-path call earlier in the file; several scripts inside one code file are not told apart, and scripts generated at run time are covered by the cells of T3.
  4. Wording of T3: the plan says all cells PASS, and both trees give 40 PASS and 1 NOT APPLICABLE (`tsan`, by design on every environment); D15 accepts that count as meeting the criterion.
  5. Test change outside the listed scope: the four preflight cases of `tests/rewrite/test_record_startup.py` (`production-break`, `production-continue`, `failure-break`, `failure-continue`) gave `snmp3WorkerPath` a path that does not exist so that Supervisor construction fails. After this unit `Config::load` rejects such a path at load (`ldd` fails), so the Runtime preflight failure was no longer reached and the cell failed. The cases now give a link to the real worker and the runner removes it after `snmp3Load` returns, while the startup script waits on a FIFO; every check and expected value is unchanged. The ten cases of the cell pass, and a mutation of the new code (the removal left out) fails six checks, among them `startup-real-preflight-failure`, as predicted before the run. `tests/rewrite/README.md` describes the new construction in the paragraph on missing-worker cases.
  6. Verification Base: the baseline of item 1 and the first run of T3 used the Base of the internal 1.3.0 tree, which the owner has since removed from this host. All results recorded here were produced again against the Base of the public `EPICS-env-distribution` 1.5.0 tree for Debian 13 (EPICS Base 7.0.10, the version the product requires); `configure/RELEASE.local`, which git ignores, names it. The products built against the earlier Base are kept under `work/products-alsu-1.3.0-base/`.
  7. Unproven by these checks, unchanged from the criteria above: the sanitizer entries, and termination of the helper by a signal during the probe. The ordinary stage does not run the instrumented product.
  8. Second-person pass on the changed documents (item 8 of the plan), run 2026-11-13 over the changed text of `docs/snmp-rewrite-config.md`, `docs/snmp-worker-reference.md`, `docs/configure-procedure.md`, `docs/legacy-review.md` and `tests/rewrite/README.md`: the first pass found one minor finding, that the sentence on the helper environment did not say that a sanitizer qualification run adds two options to the worker only; the owner had it applied, and the second pass, from the seat of a reader of the configuration reference alone, found nothing above the severity floor. The record is the report of that session. The SHA-256 manifest of the evidence directory was taken before that added sentence, so `docs/snmp-rewrite-config.md` is the one entry of it that differs from the file.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-13, accepted by the owner after the second-person and third-person passes of this detail converged
Implementation Authorization: 2026-11-13, by the owner: items 1 to 9 of the accepted plan, that is edits and test runs in this repository; every commit, push, tag and issue action needs its own authority
Superseded Plan Artifacts: none

1. On the unchanged tree run `python3 tests/rewrite/run_matrix.py --stages environment,ordinary --output work/matrix-m1-baseline` from the repository root (the output directory must not exist) and keep the result as the baseline; the series below starts only when every cell passes.
2. Write a helper `snmp3EnvHelper` (source `tests/rewrite/EnvHelper.cpp`, target in `tests/rewrite/Makefile`) that writes its environment, one entry per line in order, to a file named from its own executable path and its mode (`--capabilities` or `--ipc-fd`). As the probe it also prints capability output that the load accepts, and it is linked to one extra test library in a separate directory so that the library directories of the probe helper and of the worker differ. Before writing T1, run the real supervisor once with the helper as the worker; if the supervisor refuses it for lack of the IPC start, T1 reads the environment of the real `snmp3Worker` from `/proc/<pid>/environ` instead, with the pid taken from `WorkerSnapshot::pid` of the supervisor.
3. Write T1, T2 and the search of T4 before any code change. T1 runs the shipped load and launch paths from a test process whose environment holds a marker variable and a different `LD_LIBRARY_PATH`, with the worker path set first; the expected list is the three fixed entries as literals and the library path from the directories of the worker's `ldd` under the rule above (decided rule; source: the existing worker behavior in `Supervisor.cpp`). T2 loads with the worker path unset, loads with a worker that `ldd` rejects (a script), and changes the worker path after a successful load.
4. Run T1, T2 and T4 on the unchanged code and require T1 to fail on the assertion that the probe environment holds no variable outside the list, T2 to fail because no rejection happens, and the search of T4 to fail because it finds the calls in the old order. Record the failing output.
5. Implement the shared function, use it for both spawns, make `Config::load` require the worker path and reject a failing `ldd`, make `Config::setWorkerPath` reject a different path once a load has succeeded, keep the phrase `nativeProbe is absolute` in the help text of `snmp3Load` (`test_config.py` expects it), and update every file of the scope that calls `snmp3Load`.
6. Run T1, T2 and T4 until all pass, then run each mutation of the table in the Test Plan on a scratch copy and require the predicted failing assertion.
7. Run the ordinary stage again on the changed tree (T3), with `--output work/matrix-m1-changed`, and compare the name and status columns of its `cells.tsv` with those of the baseline of item 1.
8. The documents that show the startup order changed, and the open finding in `docs/legacy-review.md` is updated: run a second-person pass on the changed text before the commit.
9. Keep the helper, the test outputs and the SHA-256 of every test and product file the runs used under `work/`.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Configuration and launch | Observe the environment of the real probe process and of the real worker process started through the shipped load and launch paths, with a marker variable and a different library path in the test process | Linux host, ordinary product | The two lists are byte-identical, hold no variable outside the list, and the library path equals the sorted unique directories from the worker's `ldd` |
| T2 | Configuration failure | Load with the worker path unset, load with a worker for which `ldd` fails, and change the worker path after a successful load | Linux host, ordinary product | Each of the first two loads is rejected with an error and starts no probe, and the path change is rejected with the earlier path kept |
| T3 | Regression | `python3 tests/rewrite/run_matrix.py --stages environment,ordinary --output <new directory>` from the repository root, once on the unchanged tree and once on the changed tree, each with its own output directory | Linux host, ordinary product | All cells PASS on both trees and the name and status columns of `cells.tsv` equal |
| T4 | Documents and startup scripts | Run the startup example of `docs/snmp-rewrite-config.md` and of `docs/configure-procedure.md` as written against the shipped IOC with the configuration that the document prepares, and search the files of the scope for a `snmp3Load` call that is not preceded by `snmp3WorkerPath` in the same script or example | Linux host, ordinary product | Both examples reach `iocInit` without an error, and the search finds no call without the worker path before it |

The search of T4 reads static text; the startup scripts that the Python and C++ tests generate are exercised by the cells of T3, where a missing worker path rejects the load.

The mutations below are applied one at a time to scratch copies of the changed tree; the exact old and new text of each is written when the code exists, the failing assertion of each is fixed before the series, and the series starts only when the changed tree passes T1, T2 and T4:

- V1: the probe spawn passes `environ` again; expected failing assertion: the probe environment holds no variable outside the list.
- V2: the shared function leaves out `LD_LIBRARY_PATH`; expected failing assertion: the library path equals the directories from the worker's `ldd`.
- V3: the probe path appends one extra entry; expected failing assertion: the two lists are byte-identical.
- V4: the probe path reorders two entries; expected failing assertion: the two lists are byte-identical, order included.
- V5: an `ldd` failure is ignored and an empty library path is used; expected failing assertion: the load with a failing `ldd` is rejected.
- V6: the probe list is built from the probe's own `ldd`; expected failing assertion: the library path equals the directories from the worker's `ldd`.
- V7: the load no longer rejects an unset worker path; expected failing assertion: the load with the worker path unset is rejected.
- V8: the rejection of a worker path change after a successful load is removed; expected failing assertion: the path change after a load is rejected.
- V9: the two startup lines of one example are put back in the old order; expected failing assertion: the search finds no call without the worker path before it.

Not covered and listed as unproven: the sanitizer entries, because the probe never receives them and the planned stages do not run the instrumented product, and termination of the helper by a signal during the probe.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-14T01:44Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The 6 checks of `snmp3EnvTest` that belong to T1 pass (probe and worker observed, no variable outside the list, fixed entries equal the literals, probe and worker library paths equal the directories of the worker's `ldd`, probe and worker environments byte-identical); the worker environment is read from `/proc/<pid>/environ` of the real `snmp3Worker`. The same binary on the unchanged tree with the same test files (observed 2026-11-14T01:57Z) fails 4 of these 6; mutations V1 to V4 and V6 each fail the predicted checks. Evidence: `work/m1-evidence-base150/envtest.out`, `work/tmp/unchanged-base150/envtest.out`, `work/tmp/mutation-base150/` |
| T2 | 2026-11-14T01:44Z | Same | Pass | The 7 checks of `snmp3EnvTest` that belong to T2 pass (load without a worker path, load with a worker whose `ldd` fails, each rejected with its reason and starting no probe; path change after a load rejected with the earlier path kept; the same path accepted again). On the unchanged tree 6 of these 7 fail (observed 2026-11-14T01:57Z); mutations V5 to V8 each fail the predicted checks. Evidence: same files as T1 |
| T3 | 2026-11-14T01:26Z on the unchanged tree, 2026-11-14T01:17Z on the changed tree | Same host, one build per tree against the same public 1.5.0 Base | Pass, with a wording difference | Both runs end `PASS=40, NOT APPLICABLE=1`; the 41 rows of the two `cells.tsv` are equal in name and status (`diff` of the first two columns is empty); the only NOT APPLICABLE cell is `tsan`, which the matrix writes NOT APPLICABLE on every environment. The plan text says "all cells PASS"; the wording is listed under Dependencies And Decisions for the owner. Earlier run on the changed tree against the Base of the internal 1.3.0 tree (2026-11-13T21:28Z) ended with one FAIL, `record-startup`, for the reason recorded under Dependencies And Decisions; that Base tree is gone from this host, so both runs were redone on the public tree. Evidence: `work/matrix-m1-baseline-base150/`, `work/matrix-m1-changed-base150/`, `work/matrix-m1-changed/` (the earlier FAIL run) |
| T4 | 2026-11-14T01:44Z to 2026-11-14T01:45Z | Same as T1 | Pass | The search checked 17 `snmp3Load` calls and found 0 without a worker path before it; on the unchanged tree it finds 11 of 17 (2026-11-14T01:57Z); mutation V9 makes it fail with exit 1. The block of `docs/snmp-rewrite-config.md` is identical to the shipped `tests/rewrite/config/startup.cmd`; run as written through the steps of `docs/configure-procedure.md` it reaches `iocInit`, prints the three `snmp3 config:` lines and exits 0. The block of `docs/snmp-worker-reference.md` also runs, from `Running` to `Stopped` with the worker reaped and not forced. Evidence: `work/m1-evidence-base150/order.out`, `example1.out`, `example2.out` |

Mutation series V1 to V9 (rerun on the public 1.5.0 Base, observed 2026-11-14T01:57Z to 2026-11-14T01:59Z): each copy builds, the library the run loads is the copy's own, the SHA-256 of the mutated file equals the recorded `mutated_sha256`, and the set of failing checks equals the predicted `intended` plus `also` set in every row; V9 fails its search with exit 1. Evidence: `work/tmp/mutation-base150/` (table, copies, results). SHA-256 of the test and product files that the runs used: `work/m1-evidence-base150/SHA256SUMS` (23 files; the six product hashes equal those in `environment.json` of the changed matrix run).

##### Closure Evidence

- Completion Criteria checked on 2026-11-13 against the Verification Results: byte-identical environment lists of the probe and the worker (T1), the three rejections (T2), every tracked `snmp3Load` call preceded by the worker path and the startup examples run as written (T4), the checks seen failing on the unchanged code and on each of V1 to V9, and the ordinary stage with the same 41 cells on both trees (T3, accepted wording D15). The differences from the plan are accepted by D15.
- Landing: the change and its `CHANGELOG.md` entry `0.0.2` are commit `6b89552f028542f06b5198b3fee0df3f275edb16`, and the register record of the plan, decisions and results is commit `c3f9dcac5fc9e616861704d3ed4a6fa4e984790e`; both were pushed to `origin/master`. After the push, `git rev-parse HEAD '@{upstream}'` printed `c3f9dca` twice and `git diff --name-only 513929d origin/master` listed the 29 paths of the two commits (observed 2026-11-14T04:57Z); `git fetch` and `git status -sb` showed no difference again at 2026-11-14T05:08Z.
- Tag: the annotated tag `0.0.2` (object `9db6471d0337273b72ad0514c7c5ef69c14d233b`) names `6b89552`, created and pushed under the owner's tag authority of 2026-11-13. Recheck with `git ls-remote --tags origin refs/tags/0.0.2 'refs/tags/0.0.2^{}'`, which printed that object and the peeled commit at 2026-11-14T05:08Z.
- Issue #2: under the owner's Issue authority of 2026-11-13 its body was rewritten to the final behavior with the five acceptance items checked, a closing comment was posted and the issue was closed as completed. Recheck with `gh issue view 2 --json state,stateReason,closedAt`, which printed CLOSED, COMPLETED and 2026-11-14T05:03:27Z at 2026-11-14T05:08Z.
- Unproven and left listed: the sanitizer entries, termination of the helper by a signal during the probe, and the application repository, whose startup keeps the old order of the two lines until the owner decides whether the swap becomes work of its own.

##### GitHub Projection

Title: Spawn the native probe with the worker environment
Labels: bug
GitHub Milestone: 1.0.0
Observed State: closed (completed)
Observed Labels: bug
Observed Milestone: 1.0.0
Last Compared: 2026-11-14T05:08Z; issue updatedAt 2026-11-14T05:03:28Z; title, label, assignee jeonghanlee and milestone unchanged; body carries the final behavior and the checked acceptance criteria.

#### M2 - Baseline measurement on the two laboratory units

Origin: 1.0.0 / M15
Identity History: M15 -> M2 (created as the next free ID, then placed second so the row order is the execution order, 2026-11-10)
GitHub Issue: #11 - https://github.com/jeonghanlee/snmp3/issues/11
Status: Complete

##### Summary

The release goal compares snmp3 with the module it replaces, so the starting point is measured first, with no product change. Two layers are measured on one NMC2 unit that carries a load and one NMC3 unit without load: Net-SNMP 5.9.4 read directly with the tools of `docs/snmp-performance`, and the installed legacy module `snmp-1.1.0.4ja` (library string `devSnmp 1.1.0.3`) running in the laboratory IOC of the application repository. The legacy module is used as installed in the Debian 13 distribution tree on Base 7.0.10; EPICS Base and the module are not modified. The slower response of the NMC2 unit is attributed to the card by the owner; load and card are not separated.

##### Scope

- Units: PDU1 (NMC2, loaded, reads, and writes on outlet 8 only under D9) and PDU2 (NMC3, no load, reads and writes). Addresses, user names and secrets appear only in files under an ignored `work/` directory.
- Step 1, Net-SNMP directly: `run_device_phases.py` phases 1 and 2 on the 358 objects of `docs/snmp-performance/inputs/ap8932-oids.tsv` (5 repeats, tool stop rules), after the read-only phase 0 probe. The `sysLocation` write is on PDU2 only, through a sender that refuses any label other than PDU2, on `sysLocation` with the original value restored.
- Step 1, outlet 8 of PDU1 (D9): `outlet_probe` switches outlet 8 off and on in cycles and reboots it once, times each SET and the time until the state reads as set, and restores the original state. The object identifiers are constants of the program, a label other than PDU1 or a missing write flag is refused before a session opens, and `CYCLES 0` reads only. Every write run needs the owner's confirmation; the first write run is one cycle without a reboot.
- Step 2, legacy IOC: the legacy laboratory IOC of the application repository branch `refactor/snmp-db-processing`, exported with `git archive` and built against the distribution Base. Loader switches: no outlet command records (`WRITE_EN=#`) and no limit enforcement (`LIMIT_EN` unset) on either unit. The load template still loads the master control command records (`MasterControlCmd_` and `MasterControlCmd`), the only records of the loaded templates with an SNMP output link, so they are removed from the exported copy and the difference to the branch is recorded with the results; the read population is unchanged. Each unit also gets one `snmpEngineTime` read record, and PDU2 one `stringout` on `sysLocation`. Two runs: the module defaults (10 s timeout, 5 retries, and 32 objects per request, passed as `MAXOIDS=32` because the loader's own default is 24) and the laboratory IOC operations values (2 s, 1 retry, 20 objects for PDU1 and 50 for PDU2). Phases per run: both units connected 300 s, PDU1 cut 300 s, PDU2 cut 300 s, reconnect. The owner cuts and restores a unit on a typed handshake. The defaults run holds the cut phases for 600 s instead of 300 s. The legacy module reads its SNMPv3 credentials from a file `ver3.txt` (keys `SecurityName`, `AuthType`, `PrivType`, `AuthPassphrase`, `PrivPassphrase` and `SecurityLevel`, one file for both hosts), which holds the passphrases in plain text; the module reads it once when its configuration call runs and keeps the credentials in memory. The run tool therefore (D11) creates the file only with the owner's permission, from the existing secret files of the targets file (both units hold the same user, algorithms and secrets, which the tool checks), with mode 600 in a new mode-700 directory on tmpfs, and removes it as soon as the IOC log shows `iocInit`, when the freshness records are connected, when the run stops or when the tool is terminated; nothing is written under the repository. Its values are not written in this register.
- Step 2, outlet 8 of PDU1 (D9): of the outlet command records of the legacy write template, only the one of outlet 8 loads on PDU1; it is the write template loaded for channel 8 alone, and the load check lists exactly one SNMP output record, `OutletCmd8_` of PDU1, where the unchanged export lists none. Its SET is timed from the put to the changed device state (T7), and the original state is restored after each run. Every write run needs the owner's confirmation.
- Step 3, snmp3 `0.0.1`: the measurement IOC described in the paragraph on the preparation of step 3 (one snmp3 input per legacy SNMP input, plus one `snmpEngineTime` read record per unit), on the same two units, with the laboratory IOC values (`timeoutMs` 2000, `retries` 1), run through the same phases, relay recorder and interval script as the operations run of step 2 and under the same PDU1 cut handshake. Outlet 8 of PDU1 also gets one `longout` record on its command object (D9), so that step 3 measures the SET of outlet 8 as step 2 does (T9); that record is built, and every write run needs the owner's confirmation.
- Two quantities in every step: the response time of one request (p50, p95, maximum) and the interval between new values of the `snmpEngineTime` record (p50, p95, maximum, from `camonitor -tsc`). The request time of steps 2 and 3 is read at the wire with a small user-space UDP relay recorder; the interval is analyzed with a short script. Twelve scratch tools are written because no existing tool does these jobs: the relay recorder (`udp_relay.py`), the interval script (`interval_stats.py`), the direct SET senders (`set_probe` and `outlet_probe`), the run tools (`phase_runner.py`, `make_run_scripts.py` and `snmp3_run_config.py`), the timers of a SET through an IOC record (`outlet_watch` with `outlet_set_time.py`, and `location_set_time.py`), `gen_measure.py`, which generates the database and configuration of the measurement IOC, and `make_tables.py`, which builds the results tables from the run records. They are under `work/baseline-direct-20261109-01/`, `work/baseline-legacy-20261110-01/run/` and `work/baseline-snmp3-20261110-01/` (`measure/` and `run/`). The interval is the time between the `camonitor` server times of two new values of a counter that advances in whole seconds, so it follows the module's request timing and is not fixed by the poll period: in the first check run on the units the gaps between new values ran from 0.57 s to 3.41 s. The request time of step 1 is taken by the tool itself and that of steps 2 and 3 at the wire through the relay; in two loopback tests against the repository test agent (300 alternating pairs of direct and relayed calls each) the relay added a mean of 0.57 ms per call of two exchanges with a standard error of 0.34 ms (the retained run, `work/baseline-direct-20261109-01/test-runner/relay-overhead-result.txt`) and 0.02 ms with 0.39 ms (an earlier run whose output was not kept), so neither differs from zero by two standard errors; the larger bound, about 1.2 ms per call, is not small against the 2.3 ms median of a single-object GET on PDU2 (T1).
- Stop rules: the tool's own rules in step 1; in the IOC steps the tool stops on a decrease of a unit's `snmpEngineTime` (card restart), no answer from a unit for 120 s outside an owner cut and outside the recovery after its restore (which lasts until the first fresh value or the limit `--recover-seconds`), an exit of the IOC or of a relay, or the owner's request. The loaded record list is checked by hand before the runs (item 5), and no run starts when it shows an output record on PDU1 other than the outlet 8 command record. The run of step 3 uses a silence limit of 700 s instead of 120 s: in a first attempt snmp3 sent no request to PDU2 from the moment PDU1 was cut, and the 120 s limit stopped the run 110 s into the cut, so the attempt is not used.

Out of scope: any change to the product or to Base, a verdict on improvement (the rows that use these numbers carry the criteria), separating load from card, writes to PDU1 other than outlet 8, performance acceptance.

##### Completion Criteria

- Step 1 produced `phase1.json`, `kstar.json`, `phase2.json` and the SET sender's result for PDU2, each stating its repeat count and the Net-SNMP version.
- Step 1 also produced the outlet 8 result of `outlet_probe` on PDU1 (T6): the time of each SET and of each state change, and the original state restored.
- Step 2 produced both run files with the four phases (the recovery phase is recorded after each cut, so `run.json` lists five phases; or a recorded reason for stopping a phase), the printed module defaults, the loaded record list showing no output record on PDU1 other than the outlet 8 command record, and the PDU2 SET result.
- The defaults run of step 2 records the stall of the module on a cut unit, so the 60 s derived in `docs/why-snmp3.md` is confirmed or corrected.
- The response time and the interval are stated for each phase in one table that names the populations (the 358-object set for step 1, and the 163 legacy signals per unit for steps 2 and 3) and, for each row, whether the request time was taken by the tool (step 1) or at the wire through the relay (steps 2 and 3).
- A run of step 2 or 3 is complete only when its phases ran in the order A (both units connected), B (PDU1 cut), D (recovery), C (PDU2 cut) and D (recovery), the names `run.json` gives them, `stopped_by` is empty and every phase check in `run.json` is valid: in phase A both units answered, in a cut phase the cut unit answered no request later than 20 s after the typed cut while the other unit answered, and in a recovery phase a fresh value was seen. Any other run is reported as incomplete with the reasons from `run.json`.
- Each run directory holds `inputs.json` with the SHA-256 of the files the run used; before a result is written into this register, the list is compared with the files on disk, and a result of a changed file is rerun or the difference is stated.
- Step 3 produced the same phases and run files as the operations run of step 2 on the measurement IOC, the loaded record list showing no output record on PDU1 other than the outlet 8 command record, and the check of the Passive-to-2-second assumption against the request interval observed at the wire in step 2.
- The outlet 8 SET results of the legacy IOC (T7) and of the measurement IOC (T9) state the time from the put to the changed device state, and the original state of the outlet is restored after each run.

##### Dependencies And Decisions

- D8 defines the baseline, D9 amends it for outlet 8 of PDU1, and D11 fixes how the legacy credential file is created and removed. The owner's confirmation that no other client reads the two units is needed before step 1.
- The two-unit measurement of the current product (M3, retired 2026-11-13 by D13) was to repeat the phases of step 2 on the product as it stands when M3 runs and compares with the results of steps 2 and 3; M3 measures the isolation of one unit with the other disconnected, and step 3 of this row measures `0.0.1` as the baseline.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-10, accepted by the owner after the second-person and third-person passes of this register converged; item 3 gained its run-log sentence afterwards, and the owner accepted the revised plan again on 2026-11-10; item 5 gained its freshness check after that, and the owner accepted the revised plan again on 2026-11-10; items 2 and 5 to 8 were revised for the built run tools after that (item 9 only renumbered), and the owner accepted the revised plan again on 2026-11-10; item 8 gained its handshake and result paragraph after that, and the owner accepted the revised plan again on 2026-11-11; the plan was revised again on 2026-11-11 after reviews against the test framework: Scope (the meaning of the interval and of the request time, and the stop rules, which now leave a unit outside the silence limit during its recovery and stop on an exit of the IOC or of a relay), Dependencies (the new decision D11 on the legacy credential file), Completion Criteria (complete and incomplete runs, `inputs.json`, and the stall of the module on a cut unit, which the owner added on 2026-11-11 to match the linked issue), item 8 (hash flags, the `WARNING` line, signal handling and the exit codes 3, 4 and 5), item 9 (the `inputs.json` comparison) and the Test Plan note; on 2026-11-11 the owner moved the SET timings T5 and T7 ahead of the read phases of step 2 (item 7), because the cut phases cannot be run yet; the owner accepted this revision on 2026-11-11 after the second-person passes converged and the third-person passes of the run tool converged, while the document itself had no third-person pass after its last corrections
Implementation Authorization: step 1 only (reads on both units, writes on PDU2, and writes on outlet 8 of PDU1 under D9 after the owner's confirmation of each write run), by the owner instruction of 2026-11-10; step 2 (legacy IOC) is authorized by the owner instruction of 2026-11-11, starting with T5 and T7, one write run each after the owner's confirmation; the owner confirmed on 2026-11-12 that the units can be cut, and the cut phases run on the typed handshake; step 3 (snmp3 `0.0.1`) is authorized by the owner instruction of 2026-11-12: the T8 run on the typed handshake, and the T9 write run after the owner's confirmation
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
7. Run the SET timings after the read phases of steps 2 and 3 (T5 and T7 ran first, by the owner's decision of 2026-11-11, because the cut phases could not be run yet), each write run after the owner's confirmation: the outlet 8 SET of the legacy IOC (T7) and of the measurement IOC (T9) with `outlet_set_time.py`, and the `sysLocation` SET of the legacy IOC on the unit without load (T5) with `location_set_time.py`. Keep the IOC and the relays up with `phase_runner.py --check-only --check-seconds 600` and run the timer in a second terminal. `private/outlet8.json` is handled as in item 3, `private/writeflags.json` already allows the unit without load, and each tool prints the put-to-change time and the alarm state of the record, restores the original value and appends its line to its own `.log` file.
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
| T3 | 2026-11-12 | Both units, installed legacy module, `st.defaults` | Pass | Complete run, `work/baseline-legacy-20261110-01/results/defaults-20261112T204054Z/` (tool SHA-256 beginning `4e56357c`; the 51 input files match); the module defaults printed by the installed binary are `SessionRetries 5`, `SessionTimeout 10000000` and `PassivePollMSec 2000`. The phases A, B, D, C and D are valid and the summary line reports `complete: true`. Request time in A: PDU1 p50 35.4 ms, p95 900.1 ms, maximum 1789.0 ms (1002 requests, all answered); PDU2 p50 5.5 ms, p95 36.1 ms, maximum 122.1 ms (1418 requests, all answered). The cut unit answered no request after the 20 s grace (PDU1 cut: 60 requests, none answered; PDU2 cut: 60 requests, none answered) while the other unit answered (92 and 20 requests). Stall of the module on a cut unit: it sent one request to the cut unit every 10 s, and its requests to the other unit paused in nine gaps of 50 s or more in each 600 s cut, the largest being 60.1 s (PDU1 cut) and 60.6 s (PDU2 cut), which confirms the 60 s derived in `docs/why-snmp3.md`. Recoveries: PDU1 1.617 s, PDU2 11.536 s. Interval of new `EngineTime` values in A: PDU1 p50 1835 ms (150 values), PDU2 p50 1442 ms (204 values); during the PDU1 cut the PDU2 p50 was 10048 ms and the p95 120207 ms (11 values); during the PDU2 cut the PDU1 had one interval, 361702 ms. The IOC log holds 19 `session send failed: Timeout` lines. The phase tables follow in item 9 |
| T4 | 2026-11-12 | Both units, installed legacy module, `st.operations` | Pass | Complete run, `work/baseline-legacy-20261110-01/results/operations-20261112T193800Z/` (tool SHA-256 beginning `4e56357c`; the 51 input files match): the phases A, B, D, C and D are valid and the summary line reports `complete: true`. Request time in A: PDU1 p50 36.4 ms, p95 587.7 ms, maximum 1623.8 ms (1464 requests, all answered); PDU2 p50 9.4 ms, p95 45.1 ms, maximum 94.6 ms (1060 requests, all answered). The cut unit answered no request after the 20 s grace (PDU1 cut: 148 requests, none answered; PDU2 cut: 138 requests, none answered) while the other unit answered (148 and 138 requests). Recoveries: PDU1 1.656 s, PDU2 4.443 s. Interval of new `EngineTime` values in A: PDU1 p50 1889 ms (142 values), PDU2 p50 1212 ms (223 values); during the PDU1 cut the PDU2 p50 was 12090 ms (28 values); during the PDU2 cut the PDU1 p50 was 30205 ms (9 values). The IOC log holds 172 `session send failed: Timeout` lines and 2 `error parsing ScopedPDU` lines. The phase tables follow in item 9 |
| T5 | 2026-11-11 | PDU2, installed legacy module, `st.operations` | Pass | One run through `ALSUCL:PDU106:sysLocation_`: the probe SET returned from `caput` in 40.5 ms and the device value changed 361.4 ms after the put, with no alarm; the restoring SET (original value, 5 bytes) changed it after 751.4 ms, with `INVALID` `READ` at that sample and `NO_ALARM` when read afterwards; the original value was restored and verified; no SET timed out. Logged as 2026-11-12T07:23:32Z in `work/baseline-direct-20261109-01/location_set_time.log`; run record `work/baseline-legacy-20261110-01/results/sets-1/` |
| T6 | 2026-11-10 (read-only form, one cycle, then three cycles and one reboot) | PDU1 outlet 8 | Pass | Read-only run: initial state on, reboot duration 5 s. First run, one cycle without a reboot: off SET 90.7 ms, state read as off after 865.7 ms, on SET 87.2 ms, state read as on after 1324.5 ms, original state restored, exit 0; its printed line was not saved. Second run, three cycles and one reboot (logged as 2026-11-11T06:18:03Z in `work/baseline-direct-20261109-01/outlet_probe.log`): off SET 85.9, 88.9 and 89.1 ms, state read as off after 1367.4, 497.8 and 1252.5 ms, on SET 92.5, 89.5 and 92.6 ms, state read as on after 467.2, 502.4 and 747.2 ms; reboot SET 89.6 ms, state read as off after 496.9 ms and as on again after 5460.8 ms; original state restored, exit 0. A label other than PDU1 and a missing write flag were refused without a datagram when the sender was tested. Each further write run awaits the owner's confirmation |
| T7 | 2026-11-11 | PDU1 outlet 8, installed legacy module, `st.operations` | Pass | One run through `ALSUCL:PDU1:OutletCmd8_`, initial state on: the off SET returned from `caput` in 39.5 ms and the state read as off 2740.6 ms after the put; the restoring on SET returned in 41.4 ms and the state read as on 9987.4 ms after the put; no alarm at either sample; the original state was restored and read again as on, and `private/outlet8.json` was set back to false. Logged as 2026-11-12T07:24:29Z in `work/baseline-direct-20261109-01/outlet_set_time.log`; run record `work/baseline-legacy-20261110-01/results/sets-1/` |
| T8 | 2026-11-12 | Both units, `0.0.1` measurement IOC, `st.measure-run`, silence limit 700 s | Pass | Complete run, `work/baseline-snmp3-20261110-01/results/snmp3-run-20261113T004952Z/` (tool SHA-256 beginning `4e56357c`; the 17 input files match): the phases A, B, D, C and D are valid and the summary line reports `complete: true`. Request time in A: PDU1 p50 40.4 ms, p95 47.8 ms, maximum 901.6 ms (6895 requests, all answered); PDU2 p50 1.9 ms, p95 7.9 ms, maximum 69.9 ms (4498 requests, all answered). The cut unit answered no request after the 20 s grace (PDU1 cut: 111 requests; PDU2 cut: 40 requests; none answered). The other unit received 18 requests in the PDU1 cut, all 270.0 s after the typed cut, and 271 requests in the PDU2 cut with four gaps of 30 s or more (largest 43.8 s); all were answered. Both `EngineTime` records were in alarm (`READ INVALID` or `COMM INVALID`) for the whole of each cut and had no new value; in A they were in `READ INVALID` for 29 % (PDU1) and 31 % (PDU2) of the time. Interval of new `EngineTime` values in A: p50 4866 ms (PDU1) and 4936 ms (PDU2), p95 10424 ms and 10432 ms (54 values each). Recoveries: PDU1 3.212 s, PDU2 6.364 s. Three earlier attempts are not used: one interrupted by a network failure at the first cut prompt, one stopped by the 120 s silence limit 110 s into the PDU1 cut (no request to PDU2 after the cut), and one started with the 120 s limit and stopped before the cut. The phase tables follow in item 9 |
| T9 | 2026-11-12 | PDU1 outlet 8, `0.0.1` measurement IOC | Pass | One run through `ALSUCL:PDU1:OutletCmd8_` (`longout`, `snmp3`), initial state on: the off SET returned from `caput` in 35.9 ms and the state read as off 10330.9 ms after the put; the restoring on SET returned in 35.6 ms and the state read as on 7181.6 ms after the put; no alarm at either sample; the original state was restored and read again as on, and `private/outlet8.json` was set back to false. Logged as 2026-11-13T02:38:34Z in `work/baseline-direct-20261109-01/outlet_set_time.log`; IOC run record `work/baseline-snmp3-20261110-01/results/snmp3-check-20261113T023804Z/` |

Results tables (item 9), built by `work/baseline-direct-20261109-01/make_tables.py` from the run records, the step 1 result files and the SET logs. The request time of steps 2 and 3 is read at the wire through the relay; that of step 1 is taken by the tool itself. The populations are the 358 objects of step 1 and the 163 legacy signals per unit plus the `EngineTime` record of steps 2 and 3.

Units measured (reported by the owner on 2026-11-12; serial numbers, board numbers and names are not recorded here): PDU1 is an AP8932 with an NMC2 card (hardware revision 05; application and AOS 7.0.8, boot monitor 4.1.0); PDU2 is an AP8932 with an NMC3 card (hardware revision 21; application 2.5.2.5, AOS 2.5.3.2, boot monitor 4.2.9). The two units differ in card generation and firmware, so a difference between them is not attributed to the card alone.

Verdict of each run (the `inputs.json` column compares the recorded SHA-256 with the files on disk):

| Run | Settings | Phases | Verdict | `inputs.json` | Tool SHA-256 | Record directory |
| --- | --- | --- | --- | --- | --- | --- |
| T4 legacy, operations values | 300 s cuts | A, B, D, C, D | Complete | 51 of 51 match | `4e56357c` | `work/baseline-legacy-20261110-01/results/operations-20261112T193800Z/` |
| T3 legacy, module defaults | 600 s cuts | A, B, D, C, D | Complete | 51 of 51 match | `4e56357c` | `work/baseline-legacy-20261110-01/results/defaults-20261112T204054Z/` |
| T8 snmp3 0.0.1 | 300 s cuts, silence limit 700 s | A, B, D, C, D | Complete | 17 of 17 match | `4e56357c` | `work/baseline-snmp3-20261110-01/results/snmp3-run-20261113T004952Z/` |

Step 1, Net-SNMP read directly (358 objects, request time taken by the tool; a phase 1 entry holds 8 or 9 requests, so its p95 equals its maximum and is not listed separately). The step 1 result files carry the repeat counts (phase 0: 30 requests; phases 1 and 2: runs 0 to 4; SET sender: 10 SETs) but not the Net-SNMP version; the installed `snmpget --version` reports 5.9.4.pre2 (read on 2026-11-12), and the owner accepted this separate record of the version on 2026-11-12.

| Unit | Measurement | Requests | p50 (ms) | p95 (ms) | Maximum (ms) |
| --- | --- | --- | --- | --- | --- |
| PDU1 | Phase 0, one object per request | 30 | 18.4 | 253.7 | 416.7 |
| PDU2 | Phase 0, one object per request | 30 | 2.3 | 15.9 | 26.5 |
| PDU1 | Phase 1, 48 objects per request, one request in flight (median of the p50 of 5 repeats, maximum of their maxima) | 40 | 403.0 | - | 1756.9 |
| PDU2 | Phase 1, 48 objects per request, one request in flight (median of the p50 of 5 repeats, maximum of their maxima) | 40 | 21.4 | - | 86.9 |
| PDU2 | Phase 1, 40 objects per request, one request in flight (median of the p50 of 5 repeats, maximum of their maxima) | 45 | 11.5 | - | 52.3 |

Steps 2 and 3, per phase and unit (requests as counted at the wire in the phase window, with the number answered in parentheses; request time over the answered requests; interval between new values of the `EngineTime` record from `camonitor -tsc`, with the number of new values):

| Run | Phase | Unit | Requests (answered) | Request time p50 / p95 / maximum (ms) | New values | Interval p50 / p95 / maximum (ms) |
| --- | --- | --- | --- | --- | --- | --- |
| T4 | A | PDU1 | 1464 (1464) | 36.4 / 587.7 / 1623.8 | 142 | 1889 / 2942 / 4249 |
| T4 | A | PDU2 | 1060 (1060) | 9.4 / 45.1 / 94.6 | 223 | 1212 / 2206 / 3353 |
| T4 | B (PDU1 cut) | PDU1 | 148 (0) | - | 0 | - |
| T4 | B (PDU1 cut) | PDU2 | 148 (148) | 14.6 / 42.0 / 87.7 | 28 | 12090 / 12182 / 16121 |
| T4 | D (PDU1 restored) | PDU1 | 8 (8) | 60.1 / 528.4 / 528.4 | 0 | - |
| T4 | D (PDU1 restored) | PDU2 | 6 (6) | 5.0 / 68.7 / 68.7 | 0 | - |
| T4 | C (PDU2 cut) | PDU1 | 138 (138) | 138.2 / 1060.3 / 1467.2 | 9 | 30205 / 35302 / 35302 |
| T4 | C (PDU2 cut) | PDU2 | 138 (0) | - | 0 | - |
| T4 | D (PDU2 restored) | PDU1 | 4 (3) | 42.4 / 527.3 / 527.3 | 0 | - |
| T4 | D (PDU2 restored) | PDU2 | 4 (3) | 51.4 / 2053.3 / 2053.3 | 0 | - |
| T3 | A | PDU1 | 1002 (1002) | 35.4 / 900.1 / 1789.0 | 150 | 1835 / 3007 / 3511 |
| T3 | A | PDU2 | 1418 (1418) | 5.5 / 36.1 / 122.1 | 204 | 1442 / 2471 / 3327 |
| T3 | B (PDU1 cut) | PDU1 | 60 (0) | - | 0 | - |
| T3 | B (PDU1 cut) | PDU2 | 92 (92) | 9.3 / 31.2 / 48.2 | 11 | 10048 / 120207 / 120207 |
| T3 | D (PDU1 restored) | PDU1 | 2 (2) | 261.3 / 517.8 / 517.8 | 0 | - |
| T3 | D (PDU1 restored) | PDU2 | 6 (6) | 5.1 / 29.7 / 29.7 | 0 | - |
| T3 | C (PDU2 cut) | PDU1 | 20 (20) | 36.5 / 784.0 / 784.0 | 1 | 361702 / 361702 / 361702 |
| T3 | C (PDU2 cut) | PDU2 | 60 (0) | - | 0 | - |
| T3 | D (PDU2 restored) | PDU1 | 0 (0) | - | 0 | - |
| T3 | D (PDU2 restored) | PDU2 | 3 (2) | 10.4 / 19.1 / 19.1 | 0 | - |
| T8 | A | PDU1 | 6895 (6895) | 40.4 / 47.8 / 901.6 | 54 | 4866 / 10424 / 11403 |
| T8 | A | PDU2 | 4498 (4498) | 1.9 / 7.9 / 69.9 | 54 | 4936 / 10432 / 11403 |
| T8 | B (PDU1 cut) | PDU1 | 111 (0) | - | 0 | - |
| T8 | B (PDU1 cut) | PDU2 | 18 (18) | 1.9 / 3.4 / 3.4 | 0 | - |
| T8 | D (PDU1 restored) | PDU1 | 55 (55) | 41.8 / 200.7 / 337.2 | 0 | - |
| T8 | D (PDU1 restored) | PDU2 | 63 (63) | 1.9 / 2.2 / 23.6 | 0 | - |
| T8 | C (PDU2 cut) | PDU1 | 271 (271) | 41.5 / 104.8 / 464.2 | 0 | - |
| T8 | C (PDU2 cut) | PDU2 | 40 (0) | - | 0 | - |
| T8 | D (PDU2 restored) | PDU1 | 73 (73) | 41.0 / 49.2 / 103.2 | 0 | - |
| T8 | D (PDU2 restored) | PDU2 | 19 (19) | 1.9 / 17.3 / 17.3 | 0 | - |

Effect of a cut on the other unit (the cut window runs from the typed cut to the end of the cut phase; the longest silence is the longest time in the window without a request to the other unit):

| Run | Cut unit | Requests to the cut unit (answered) | Requests to the other unit (answered) | Longest silence toward the other unit (s) | New `EngineTime` values of the other unit | `EngineTime` alarm time of the other unit (%) |
| --- | --- | --- | --- | --- | --- | --- |
| T4 | PDU1 | 148 (0) | 148 (148) | 4.1 | 28 | 0 |
| T4 | PDU2 | 138 (0) | 138 (138) | 5.4 | 9 | 0 |
| T3 | PDU1 | 60 (0) | 92 (92) | 60.1 | 11 | 0 |
| T3 | PDU2 | 60 (0) | 20 (20) | 60.6 | 1 | 0 |
| T8 | PDU1 | 111 (0) | 18 (18) | 270.0 | 0 | 100 |
| T8 | PDU2 | 40 (0) | 271 (271) | 43.8 | 0 | 100 |

SET timings (the direct rows give the time of the SET request and the time until the state read as set; the IOC rows give the time the `caput` took to return and the time from the put to the changed device value or state; times in ms):

| Layer | Object and transition | Calls | Call returned | Device changed after | Alarm at the sample |
| --- | --- | --- | --- | --- | --- |
| Net-SNMP direct (T2) | PDU2 `sysLocation`, ten SETs | 10 | p50 732.6, 150.2 to 799.6 | readback matched every SET, not timed | - |
| Net-SNMP direct (T6) | PDU1 outlet 8 off | 3 | 85.9 to 89.1 | 497.8 to 1367.4 | - |
| Net-SNMP direct (T6) | PDU1 outlet 8 on | 3 | 89.5 to 92.6 | 467.2 to 747.2 | - |
| Net-SNMP direct (T6) | PDU1 outlet 8 reboot | 1 | 89.6 | off after 496.9, on again after 5460.8 | - |
| Legacy IOC (T5) | PDU2 `sysLocation`, probe value | 1 | 40.5 | 361.4 | NO_ALARM / NO_ALARM |
| Legacy IOC (T5) | PDU2 `sysLocation`, restore | 1 | 46.9 | 751.4 | INVALID / READ |
| Legacy IOC (T7) | PDU1 outlet 8 off | 1 | 39.5 | 2740.6 | NO_ALARM / NO_ALARM |
| Legacy IOC (T7) | PDU1 outlet 8 on | 1 | 41.4 | 9987.4 | NO_ALARM / NO_ALARM |
| snmp3 0.0.1 IOC (T9) | PDU1 outlet 8 off | 1 | 35.9 | 10330.9 | NO_ALARM / NO_ALARM |
| snmp3 0.0.1 IOC (T9) | PDU1 outlet 8 on | 1 | 35.6 | 7181.6 | NO_ALARM / NO_ALARM |

Check of the Passive-to-2-second assumption against the wire of step 2 (phase A of the two legacy runs; requests grouped into bursts separated by more than 0.5 s): T4: the requests come in bursts that start every 1.29 s on PDU1 and 1.07 s on PDU2 (p50), 4.9 and 3.5 requests per second, and the Passive `EngineTime` record gets a new value every 1.89 s and 1.21 s (p50); T3: the requests come in bursts that start every 0.95 s on PDU1 and 1.06 s on PDU2 (p50), 3.3 and 4.7 requests per second, and the Passive `EngineTime` record gets a new value every 1.83 s and 1.44 s (p50). The legacy module therefore reads its Passive records every 1.2 to 1.9 s, not every 2.0 s as `PassivePollMSec` suggests (the bursts start every 0.95 to 1.29 s), so the 2 s assumption holds only approximately and the `2 second` scan of the measurement IOC reads the equivalent records slightly less often than the legacy module.

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
- The read-only checks of step 3 on the units (`work/baseline-snmp3-20261110-01/results/snmp3-check-*`): both units answered every request (2765 of 2765 on PDU1 and 1721 of 1721 on PDU2 in the 120 s check), the `EngineTime` records advanced, and in the 120 s check both records were in `READ INVALID` at the same moments for about 35 % of the 116 s observed.
- The short check of item 5 ran on the units on 2026-11-11 (`--check-only` with `st.operations`, with the tool as it was before `inputs.json` and the phase checks; results in `work/baseline-legacy-20261110-01/results/check-operations-1/`): both `EngineTime` records read without an alarm and advanced (10 values on PDU1 and 13 on PDU2 in 20 s), the credential file was removed 0.5 s after its creation, and 97 of 98 requests on PDU1 and 74 of 74 on PDU2 were answered; the open request is the last line of the relay log.

##### Closure Evidence

- Completion Criteria checked on 2026-11-12 against the Verification Results: step 1 files and T1, T2, T6; step 2 run files and T3, T4, T5, T7 with the loaded record list showing one output record on PDU1; the stall of the module on a cut unit measured at 60.1 s and 60.6 s (T3); the tables of item 9; T3, T4 and T8 each a complete run with every `inputs.json` matching the files on disk; the Passive-to-2-second check; T7 and T9 with the original state of outlet 8 restored. The owner accepted the record of the Net-SNMP version and asked for the two units' generation and firmware to be recorded (2026-11-12).
- Landing: the results and tables are in commits 7eab1ab to 89842da of `docs/milestone-1.0.0.md`, pushed to `origin/master`; after the push of 89842da, `git rev-parse HEAD origin/master` printed the same commit (observed 2026-11-12). The commit that carries this closure edit is the next commit of the file in `git log`.
- Unproven and left listed: exit code 4, the `WARNING` line and the contents of `inputs.json` beyond the relay script have no mutation row; termination by SIGKILL was not exercised.
- Issue #11: under the owner's Issue authority of 2026-11-12 its body gained a Result section, a closing comment was posted and the issue was closed as completed; `gh issue view 11` read back state CLOSED, reason COMPLETED, at 2026-11-13T03:03Z.

##### GitHub Projection

Title: Measure the baseline on both laboratory units
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: closed (completed)
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-13T03:03Z; issue updatedAt 2026-11-13T03:03:28Z; title, label, assignee jeonghanlee and milestone unchanged; body carries the Result section.

#### M3 - Two-device isolation measurement on the current product

Origin: 1.0.0 / M2
Identity History: M2 -> M3 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #3 - https://github.com/jeonghanlee/snmp3/issues/3
Status: Complete

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

- G1 complete (2026-11-13); status restored to Not started.
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

- Retired by the owner's decision of 2026-11-13 (D13). Waived checks: T1 (three 300 s phases on the laboratory IOC, ratio of the healthy unit's interval at most 1.1) and T2 (alarm on loss observed on the laboratory IOC); neither was run.
- What replaces them: the baseline run of the snmp3 `0.0.1` measurement IOC (the T8 run of the baseline measurement) shows the records of both units in `COMM INVALID` about 1.2 s after the PDU1 cut and no new value of either unit for the rest of the cut, and no request to the healthy unit for 270.0 s in the PDU1 cut, so the ratio criterion is not met on `0.0.1` and the alarm of the disconnected unit appeared within 1.2 s. These values are the reference for the reply-driven processing work and for the release acceptance, which keeps the three phases and the criteria of this row; the laboratory IOC population has no measurement on the product before those changes.
- Issue #3: under the owner's Issue authority of 2026-11-13 its body gained a Result section and a rewritten first paragraph, a closing comment was posted and the issue was closed as completed; `gh issue view 3` read back state CLOSED, reason COMPLETED, at 2026-11-13T18:14Z.

##### GitHub Projection

Title: Measure one unit while the other unit is disconnected
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: closed (completed)
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-13T18:14Z; issue updatedAt 2026-11-13T18:14:55Z; title, label, assignee jeonghanlee and milestone unchanged; body carries the Result section.

#### M4 - Request sizing with advertised message size and tooBig split

Origin: 1.0.0 / M3
Identity History: M3 -> M4 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #4 - https://github.com/jeonghanlee/snmp3/issues/4
Status: Complete

##### Summary

Problem 1 and cause B. In the product as it stands (code read on 2026-11-14) `maxVarbinds` is a fixed per-profile value, the worker session sets no `rcvMsgMaxSize`, a reply with an error status ends every value of its request as a `ProtocolFailure` with an empty record message, and the scheduler builds the next request with the same number of values, so a request the unit rejects is sent again unchanged. The library advertises 65507 when nothing is set, the configured value from 484 to 65507 otherwise, refuses a session below 484 and caps a larger value at 65507 (observed on the wire with the fixture agent, Net-SNMP 5.9.4, `work/tmp/m4-verify/advertised.txt`). This unit advertises a configured size, halves the batch of an address when the unit rejects a request, whether the rejection can be read or not, and reports the cause as the record message.

Diagnosis of both laboratory units on 2026-11-14, read only, with a single-request tool, the baseline relay and the library debug output (`work/tmp/m4-diagnose/`): the limit is the size of the reply, not the number of values. Both cards answer a reply of 1341 bytes on the wire (measured three times on each) and reject, three times of three on each, the request whose reply would be 1342 bytes; that size is calculated from the replies of the single objects, and the same calculation gave 1341 for the request measured at 1341 (`wire-limit2.txt`, `wire-limit3.txt`); 61 short values pass, and the 358-object set passes with 51 or 50 values depending on the order. A rejection is always a 118-byte tooBig reply with an empty binding list. A share of these rejections cannot be decrypted: the card encrypts with a time one second earlier than the engine time it writes in the header, and every captured unreadable message (3 of NMC2, 3 of NMC3) decrypts to the same tooBig reply with that earlier time. The library drops such a message, counts it in `snmpInASNParseErrs`, and the request ends as a timeout. Observed shares: NMC2 10 of 24 and 3 of 8, NMC3 3 of 150; the NMC2 card takes 0.3 to 0.5 s per reply and the NMC3 card about 0.02 s. No reply within the limit was unreadable in about 70 reads. Advertising a size is honored by both cards and does not change the unreadable share. Every statement of this detail about the units rests on these observations; earlier records of other units are not used.

##### Scope

- Profile field `maxMessageSize` (default 1472, range 484 to 65507) set on the worker session.
- On `ProtocolFailure` with status tooBig, the scheduler halves the batch of that address for the retry, rounded down (61, 30, 15, 7, 3, 1), down to one varbind, and keeps the reduced batch until the worker restarts.
- Alarm message `tooBig` on the records of a batch that reaches the floor.
- A timeout is classified by the library count of unparsable replies, read by the worker before and after the request: raised means a rejection arrived that could not be read, which halves the batch like a tooBig reply; unchanged means no reply from the unit, which leaves the batch as it is. Each case has its own record message. The count rises for an unparsable reply on an SNMPv3 session; on SNMPv2c it was observed not to rise, so such a reply is not told apart from no reply; SNMPv1 was not run.
- Documentation of the field and the behavior in `snmp-rewrite-config.md`, `snmp-record-behavior.md` and `snmp-native-reference.md`.

Out of scope: GETBULK (M8), learning the agent limit across restarts, and changes to other error statuses.

Limits stated in the documentation of the field, with no work attached: an SNMPv1 or SNMPv2c message has no size field, so the setting has an effect on SNMPv3 sessions only; a request larger than `maxMessageSize` gets no reply at all, not even a tooBig, so the size must exceed the request of `maxVarbinds` values (observed 2026-11-14 with the fixture agent, `work/tmp/m4-verify/boundary.txt`).

##### Completion Criteria

- With `maxVarbinds=61` and the bindings of the measurement IOC (164 read bindings per unit; a request of their first 61 is rejected by both units and one of 30 or 32 is answered with 809 to 856 bytes, `work/tmp/m4-diagnose/wire-ioc-bindings.txt`) the records of both units fill without manual tuning, no request times out once the batch has settled, and the split count per address (cumulative since the IOC started) is visible in `snmp3RuntimeReport`.
- The fixture agent's tooBig cell produces the split and, at the floor, the `tooBig` message on the record; another error status of the fixture agent changes neither the batch limit nor the message.
- A timed-out request with a raised count of unparsable replies halves the batch like a tooBig reply; a timed-out request without it leaves the batch unchanged; the record message tells tooBig, an unreadable rejection and no reply apart.
- For an SNMPv3 profile the advertised size seen on the wire equals the configured `maxMessageSize`.
- The NMC3 unit's behavior is unchanged at `maxVarbinds=50`: its split count stays 0 and no record shows a `tooBig` message; the interval between new values of its `snmpEngineTime` record is recorded next to the T8 figures of M2, with no threshold.
- The checks were seen failing on the unchanged code and on each mutation of the table in the Test Plan, for the intended assertion.
- The 41 cells of the ordinary matrix stage on the unchanged tree keep their name and status on the changed tree (40 PASS and 1 NOT APPLICABLE, `tsan`, as accepted for M1 in D15), and the cells that M4 adds are PASS.

##### Dependencies And Decisions

- G1 complete (2026-11-13); status restored to Not started.
- D16 fixes the rounding, the NMC3 comparison, the meaning of the split count, the wire check of the advertised size, and the route and expected result of T3.
- T3 needs an IOC built against the new module: the measurement IOC of the baseline (D12) rebuilt with the two startup lines swapped. The swap in the application repository stays the owner's question recorded under M1.
- D17 adds the timeout as a signal and fixes how it is classified, from the diagnosis recorded in the Summary.
- D19 fixes the T1 harness: bash starts and stops the processes, Perl reads the records and judges, and it joins the matrix as a new cell; the existing cells stay as they are.
- D20 fixes where the three record messages are set: on every request that fails for that reason.
- D21 accepts the other differences listed below and leaves difference 7 as it is.
- Differences between the accepted plan and the work as run, recorded 2026-11-14 and accepted by D20 and D21; the plan text is unchanged:
  1. Order: the T2 cells `unreadable-halves`, `no-reply-keeps` and `split-count` and the whole of T1 were written after the product code. T1 was then run on the unchanged code with only the test files added, and mutations V11 (an unreadable rejection does not halve) and V12 (the split is not counted) were added to the table to prove the three cells.
  2. Message (D20): the plan sets the `tooBig` message on the records of a batch that reaches the floor. The code sets it on the records of every request that the unit rejects with tooBig, so the records of a larger rejected request show it for one round as well; `unreadable rejection` and `no reply` are set the same way.
  3. Product fix outside the list of the plan, found by T1: Base keeps `NAMSG` when it equals `AMSG`, so a record that failed twice with the same message kept the message after its request succeeded. A completion without a message now clears `NAMSG` first (`Request.cpp`).
  4. T3: the bindings of the measurement IOC carry no group and its requests hold one value, so at `maxVarbinds` 61 no request is rejected and the split cannot show. The split on the units was observed with 61 integer bindings of each unit in one group through the production `snmp3Ioc`, read only. The runtime report gives a new line `snmp3 batch:` after the queue line, because a check of `test_records.py` fixes the end of the `snmp3 queue:` line.
  5. Test helper: `tests/rewrite/helpers/ipc_fault.py` walks the profile of the Bootstrap frame by fixed offsets and was adjusted by the 4 bytes of `maxMessageSize`.
  6. Wire check (c): the engine discovery request of the Net-SNMP library carries 65507 whatever the session sets; the check covers the requests that ask for objects.
  7. Observed and not acted on: two endpoints with the same host address and different ports share one address in the scheduler, and so one batch limit (seen when both units were reached through loopback relays).

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-14, accepted by the owner after the second-person passes of this detail were closed
Implementation Authorization: 2026-11-14, by the owner: items 1 to 6 of the accepted plan, that is edits and test runs in this repository and the read-only run against the two units in item 6; every commit, push, tag and issue action needs its own authority
Superseded Plan Artifacts: the plan accepted on 2026-11-14 under D16, revised by D17 after the diagnosis of both units; its item 1 is partly done (the proxy records the advertised size, and the T2 cells for the tooBig rule were seen failing on the unchanged code, `work/m4-evidence/t2-unchanged.txt`)

1. Write the checks before the code: make the fault proxy of the tests record the `msgMaxSize` of an SNMPv3 request (`header[1]` of the message header, which it parses but does not keep), write T1 and T2 and the table of mutations from D16 and D17, rehearse (d) on the worker path, run them on the unchanged code and record the failing output, and start the mutation series only after the changed tree passes every check.
2. Add `maxMessageSize` to the configuration schema and the worker open path (`rcvMsgMaxSize`).
3. Add the per-address batch reduction in the scheduler on tooBig and on a timeout that the worker reports with a raised count of unparsable replies (`snmp_get_statistic(STAT_SNMPINASNPARSEERRS)`, read before and after the request), and expose the split count in `snmp3RuntimeReport`.
4. Add the three record messages (tooBig, unreadable rejection, no reply) in the request completion, and document the field and its limits.
5. Run T1 and T2 until they pass, then each mutation of the table on a scratch copy against the same Base as the unchanged run.
6. Build the measurement IOC of the baseline against the new module with the two startup lines swapped, run both units with `maxVarbinds=61` (NMC3 with 50) and record the results with T4.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | IOC | A new matrix cell in bash and Perl (D19), with the real IOC, worker and fixture agent behind the fault proxy, in place of `NativeTest`, which includes only `Config.h`, `Capabilities.h` and `Worker.h` and cannot observe the split or the record message. (a) The agent cell that always answers tooBig, on one SNMPv2c and one SNMPv3 profile: the batch limit goes 8, 4, 2, 1 and only that record shows `tooBig`, the others fill. (b) No tooBig, and the agent cell that answers another error status: the batch limit does not change and no `tooBig` message shows. (c) SNMPv3 profile: the advertised size on the wire equals the configured one. (d) SNMPv3 profile with the engine ID set, so that the worker skips the engine discovery at session open: the proxy cuts the last byte of the reply, which the library cannot parse (observed: timeout and the count raised by one on SNMPv3, timeout and no count on SNMPv2c, `work/tmp/m4-verify/malformed.txt`), so the request times out with the count of unparsable replies raised: the batch halves and the records show the message of an unreadable rejection. (e) SNMPv2c profile: the proxy drops every reply (on SNMPv3 the engine discovery fails first, which is not a request timeout): the batch does not change and the records show the message of no reply | Linux host, ordinary product | Expected values from D16, not from the implementation; evidence is the fault proxy record (request length, advertised size, error status of the reply) |
| T2 | Scheduler | `SchedulerTest` cases: the halving rule of D16 (61, 30, 15, 7, 3, 1), the minimum of 1, independence of addresses, reset when the worker restarts, no change on other error statuses, the split counter | Linux host | Behavior as specified in D16 |
| T3 | Device | Both units with the bindings of the measurement IOC at `maxVarbinds=61`, then NMC3 at 50, through the measurement IOC rebuilt against the new module | Measurement IOC, both units | At 61 the records of both units fill with no alarm once the splits settle, and no request times out after that; at 50 NMC3 keeps a split count of 0 and shows no `tooBig` message, and its `snmpEngineTime` interval is recorded next to the T8 figures; the split count shows in `snmp3RuntimeReport`; times in UTC |
| T4 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` on the unchanged and the changed tree against the same Base | Linux host | The 41 cells of the unchanged tree keep their name and status (40 PASS and 1 NOT APPLICABLE, `tsan`), and the cells that M4 adds are PASS |

The mutations below are applied one at a time to scratch copies of the changed tree; the exact old and new text of each is written when the code exists, the failing assertion of each is fixed before the series, and the series starts only when the changed tree passes T1 and T2:

- V1: the session no longer sets `rcvMsgMaxSize`; expected failing assertion: T1 (c).
- V2: a tooBig reply leaves the batch limit unchanged; expected failing assertion: T2 halving rule, and T1 (a) within its bounded wait.
- V3: the halving rounds up; expected failing assertion: T2 halving rule.
- V4: the minimum of 1 is removed; expected failing assertion: T2 minimum.
- V5: the reset when the worker restarts is removed; expected failing assertion: T2 reset.
- V6: the batch limit applies to every address; expected failing assertion: T2 independence of addresses.
- V7: every error status is taken as tooBig; expected failing assertion: T1 (b).
- V8: the `tooBig` message is not set at the floor; expected failing assertion: T1 (a).
- V9: the count of unparsable replies is ignored, so a rejection that could not be read is taken as no reply; expected failing assertion: T1 (d).
- V10: a timeout with no reply halves the batch; expected failing assertion: T1 (e).

Not covered and listed as unproven: SNMPv1 sessions, the default 1472 and the range 484 to 65507 of `maxMessageSize`, a unit whose rejection differs from the two units measured on 2026-11-14, and the production worker log, which is redirected to `/dev/null`.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-14T21:36Z to 2026-11-14T21:45Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The matrix cell `request-sizing` (`tests/rewrite/test_request_sizing.bash` with `request_sizing.pl`) passes its 217 checks in 6 cases, each with its own IOC: tooBig on SNMPv2c and on SNMPv3 (limit 4, 2, 1, 1 and split count 1, 2, 3, 3 over four rounds of an 8-member group; all 8 records show `tooBig` after the first round and only the eighth after the fourth), another error status and plain answers (limit 0, count 0, no `tooBig`), an unreadable reply on SNMPv3 with the engine ID set (limit 4, count 1, message `unreadable rejection`, proxy mutation observed), and dropped replies on SNMPv2c (limit 0, count 0, message `no reply`, drops observed). For (c) every SNMPv3 request for objects carries 2000, the configured value; the engine discovery request of the library carries 65507 and no object. On the unchanged code with only the new test files (2026-11-14T21:36Z) the cell exits 2: five cases fail on the limit, the count and the messages, and the SNMPv3 tooBig case cannot be judged because the loader rejects `maxMessageSize`. Mutations V1, V2, V4, V7, V8, V9, V10, V11 and V12 fail the predicted checks. Evidence: `work/matrix-m4-changed-2/o-request-sizing/`, `work/m4-evidence/t1-unchanged.log`, `work/tmp/m4-mutation/results/`, `work/m4-evidence/mutation-table.json`, `work/m4-evidence/sources.sha256` |
| T2 | 2026-11-14T21:26Z to 2026-11-14T21:36Z | Same as T1 | Pass | The seven cells of `snmp3SchedulerTest` pass on the changed tree (`toobig-halving` with 61, 30, 15, 7, 3, 1, 1; `toobig-per-address`; `toobig-reset`; `toobig-other-status`; `unreadable-halves`; `no-reply-keeps`; `split-count`), and the whole suite passes inside the matrix. On the unchanged code `toobig-halving`, `toobig-per-address` and `toobig-reset` fail on the first expected size (2026-11-14T18:55Z). Mutations V2 to V7 and V10 to V12 fail the predicted cells with the predicted sizes (V3: 31 for 30; V4: 61 for 1; V5: 4 for 8 after the restart; V6: peer 4 for 8). Evidence: `work/m4-evidence/t2-unchanged.txt`, `work/tmp/m4-mutation/results/*/t2-rerun.txt` |
| T3 | 2026-11-14T21:46Z to 2026-11-14T22:02Z | Both units, read only (GET requests; no write); measurement IOC rebuilt against the changed module with the two startup lines swapped, and the production `snmp3Ioc` of the changed tree for the grouped runs | Pass with a changed method (difference 4) | Measurement IOC, 180 s each at `maxVarbinds` 61, 120 and 50: split count 0 and limit 0 on both addresses in all 19 reports of each run, all 328 read records without alarm from the second sample on, no `tooBig` message. A relay on the wire at 120 shows why: the bindings carry no group, so a request holds one value (in 120 s 2778 of 2780 requests to the NMC2 unit and 1795 of 1796 to the NMC3 unit are 140 bytes or less; the others are 160 bytes, and the largest reply is 162 bytes), and `maxVarbinds` has no effect on this IOC. With 61 integer bindings of each unit in one group (6 runs, 3 rounds each): the first request is rejected every time, on the NMC2 unit 4 times as `unreadable rejection` after the 4 s of the two timeouts and 2 times as `tooBig`, on the NMC3 unit 6 times as `tooBig`; the limit of that address becomes 30 with split count 1, the other address is untouched, and in rounds 2 and 3 all 61 records fill without alarm in every run. These 6 runs had no relay and give no sizes. Sizes come from one more run of 4 rounds through loopback relays (`group-61`), in which both units share one address and so one limit: the NMC2 unit got one request of 1460 bytes and answered with 117 bytes (`tooBig`), then per round requests of 778, 787 and 140 bytes answered with 809, 817 and 141 bytes; the NMC3 unit, already under the limit of 30, got no request of 61 values and per round requests of 778, 787 and 140 bytes answered with 808, 817 and 141 bytes. With 50 in one group (1 run): no rejection, limit 0 and count 0 on both units. Interval between new values of the `snmpEngineTime` record at 50: NMC3 unit p50 2000, p95 2191, max 2279 ms (90 intervals); NMC2 unit p50 5847, p95 8700, max 9433 ms (33 intervals); no threshold. Evidence: `work/m4-t3/runs/` |
| T4 | 2026-11-14T21:36Z to 2026-11-14T21:45Z | Same as T1 | Pass | `run_matrix.py --stages environment,ordinary` on the changed tree: 41 PASS and `tsan` NOT APPLICABLE; the 41 cells of the M1 run on the same Base (`work/matrix-m1-changed-base150/cells.tsv`, the product code of commit 746a498) keep name and status, and the added cell `request-sizing` is PASS. A first run before the helper fix of difference 5 had `qualification` FAIL (`work/matrix-m4-changed-1/`). Evidence: `work/matrix-m4-changed-2/cells.tsv` |

##### Closure Evidence

- Completion Criteria checked on 2026-11-14 against the Verification Results: the split and the `tooBig` message with the fixture agent and no change on another error status (T1, T2), the two kinds of timeout told apart with their effect on the batch and their messages (T1, T2), the advertised size on the wire for the requests that ask for objects (T1, difference 6), the checks seen failing on the unchanged code and on each of V1 to V12, and the ordinary stage with the same 41 cells and the added cell (T4). On the units the records fill at `maxVarbinds=61` and the split count shows in the report; the split itself was observed with a group of 61 values and not with the ungrouped bindings of the measurement IOC, and the NMC3 unit at 50 keeps a split count of 0 with no `tooBig` message (T3, difference 4). The differences from the plan are accepted by D20 and D21.
- Landing: the change is commit `1a0ff9b4426967189df68466f134728a5dfa5a9f`, the register record of the plan, decisions and results is commit `62cc494729df97009c3e205e6071280e54112509`, and the `CHANGELOG.md` entry `0.0.3` is commit `d2919b6749bbdce36e5d2ab336095c071ccfb223`; all three were pushed to `origin/master`. After `git fetch`, `git rev-parse HEAD origin/master` printed `d2919b6` twice and `git diff --name-only 746a498 origin/master` listed the 26 paths of the three commits (observed 2026-11-15T01:04Z).
- Tag: the annotated tag `0.0.3` (object `4601368588689c6adfb7ea6e3bb9fc53fb4dff9c`) names `d2919b6`, the commit that holds the `CHANGELOG.md` entry, created and pushed under the owner's tag authority of 2026-11-14. Recheck with `git ls-remote --tags origin refs/tags/0.0.3 'refs/tags/0.0.3^{}'`, which printed that object and the peeled commit at 2026-11-15T01:04Z.
- Issue #4: under the owner's Issue authority of 2026-11-14 its body was rewritten to the final behavior with the three acceptance items checked and the known limits, a closing comment was posted and the issue was closed as completed. Recheck with `gh issue view 4 --json state,stateReason,closedAt`, which printed CLOSED, COMPLETED and 2026-11-15T00:46:22Z at 2026-11-15T00:46Z.
- The `CHANGELOG.md` entry was committed without a second-person pass, by the owner's decision of 2026-11-14.
- Unproven and left listed: SNMPv1 sessions, the default 1472 and the range of `maxMessageSize`, a unit whose rejection differs from the two units measured, and the limit shared by endpoints of one host address (difference 7).

##### GitHub Projection

Title: Advertise a message size and split the batch on tooBig
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: closed (completed)
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-15T00:46Z; issue updatedAt 2026-11-15T00:46:22Z; title, label, assignee jeonghanlee and milestone unchanged; body carries the final behavior, the checked acceptance criteria and the known limits.

#### M5 - SET outcome contract

Origin: 1.0.0 / M4
Identity History: M4 -> M5 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #5 - https://github.com/jeonghanlee/snmp3/issues/5
Status: Complete

##### Summary

Item 9 of cause B. The legacy module reported a SET as successful before sending it and dropped a timed-out SET without alarm. snmp3 has output device support for `ao`, `longout`, `int64out`, `stringout` and `lso`, but the accepted scope of `0.0.1` is GET-only. This unit qualifies the SET path.

Observed on 2026-11-15 with the product of commit `5ad4e82`, the production `snmp3Ioc` and worker and the fixture agent (`work/tmp/m5-observe/run-1/`):

| Case | Observed |
| --- | --- |
| One put to each of the five output record types | No alarm; the agent's log holds 5 SET commits; the next GET returns 2.5, 41, 123456789012 and the written text |
| A SET whose reply is delayed by 750 ms | The record stays active (PACT 1) until the reply and completes without alarm after it |
| A SET while the agent is gone | COMM/INVALID with `no reply`; the record keeps the written value in VAL |
| A SET that the agent refuses (an integer written to an octet string object) | WRITE/INVALID with an empty message |
| Three plain puts to one record in one burst (7, 8, 9) | The agent commits 2 values, the next GET returns 9, the record holds 9 |

The two faults of the legacy module are absent. What is missing is the reason on the record when the unit refuses a request, and the merging of plain puts is the behavior of Base that `snmp-record-behavior.md` describes (D26).

##### Scope

- An alarm message on the records of a request that the unit answers with an error status other than tooBig: the name of the status and the error index, as `<name> index <n>`, for the statuses of RFC 3416 (for example `wrongType index 1`). The message of tooBig stays as M4 set it.
- Checks that fix the SET contract as observed: completion on the result, the written value on the next GET for each output record type, the alarm and message of a refused SET and of a SET without a reply, and the two routes of consecutive writes of D26.
- Documentation of the SET contract and the message in `snmp-record-behavior.md`.

Out of scope: a write to a laboratory unit's outlet; values queued by the module (D26); the deferred long active client-write cells (M14).

##### Completion Criteria

- For each of `ao`, `longout`, `int64out`, `stringout` and `lso` a put completes without alarm, the agent's log holds its SET, and the next GET of the same object returns the written value.
- An output record whose reply is delayed stays active until the reply arrives.
- A SET that the agent refuses gives WRITE/INVALID with the status name and index in the message; a SET without a reply gives COMM/INVALID with `no reply`.
- Three puts that each wait for completion give three SETs at the agent in the order written; three plain puts in one burst give the first and the last value at the agent, and the last one on the next GET.
- The checks were seen failing on the unchanged code or on a mutation of the table in the Test Plan, for the intended assertion, and the cells of the ordinary matrix stage keep their name and status.

##### Dependencies And Decisions

- M4 is Complete (tag `0.0.3`): the SET path uses its outcome and message additions.
- D26 fixes the behavior of consecutive writes.
- D22 gave this unit the tag `0.0.5`; D27 changes it to `0.0.4`, because this unit closes before M16.
- New tests are written in bash and Perl as a matrix cell (D19), and are limited to what the purpose needs, as D24 (8) set for M16.
- Differences between the accepted plan and the work as run, recorded 2026-11-16 and decided by D27; the plan text is unchanged:
  1. The message is set for every request that the unit answers with an error status, a GET as well as a SET, because both take one path. The cell of M4 expected an empty message for the object that answers genErr; its expectation is now `genErr index 8`, the eighth value of that request (`tests/rewrite/request_sizing.pl`). The batch limit is still unchanged by such a status.
  2. Under V3 more checks fail than the delayed record: with the record no longer active the result is never applied, so the refused put and the put without an agent show no alarm, and the consecutive writes change. The row of the table named the merged puts and the readings before the result; the rest follows from the same mechanism and was checked in the output.
  3. The closing order of the tags: D22 gave M16 the tag `0.0.4` and this unit `0.0.5`, and M16 waits for the runs on the units. D27 gives this unit `0.0.4` and M16 `0.0.5`.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-15, accepted by the owner
Implementation Authorization: 2026-11-15, by the owner: items 1 to 4 of the accepted plan, that is edits and test runs in this repository; no write to a unit; every commit, push, tag and issue action needs its own authority
Superseded Plan Artifacts: the draft that named `RecordTest` cases and a review of the completion path; the observation of 2026-11-15 replaces the review

1. Write the checks first: a matrix cell in bash and Perl with the real IOC, worker and fixture agent, from one table of checks kept with the cell; puts that wait for completion go through the Channel Access client of Base on private loopback ports, as `test_record_ca.py` does. Run it on the unchanged code and record the failing output: the message of the refused SET is expected to fail, the other checks to pass.
2. Add the message for an error status with its index in `DeviceSupport.cpp`.
3. Document the SET contract and the message in `snmp-record-behavior.md`.
4. Run the cell, the mutations and the ordinary matrix.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Fixture, one write | One put to each of the five output record types, a GET of each object after it; one put through a proxy that delays the reply by 750 ms; one put that the agent refuses; one put after the agent is stopped | Linux host, ordinary product | No alarm and the written value on the GET for each type; the delayed record active before the reply and without alarm after it; WRITE/INVALID with `wrongType index 1` for the refused put; COMM/INVALID with `no reply` for the put without an agent |
| T2 | Fixture, consecutive writes | Three puts that wait for completion (7, 8, 9) to one record; three plain puts in one burst (7, 8, 9) to another | Linux host, ordinary product | The agent's log holds the SETs 7, 8, 9 in that order for the first record, and the SETs 7 and 9 for the second, with 9 on the next GET |
| T3 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` on the unchanged and the changed tree | Linux host | The cells of the unchanged tree keep their name and status, and the added cell is PASS |

The status and index that the fixture agent returns for the refused put are read from its reply in the first run on the unchanged code, with a client other than the module, before the expected text is fixed.

The mutations are applied one at a time to scratch copies of the changed tree; the exact old and new text of each is written when the code exists, and the failing assertion of each is fixed before the series:

- V1: the message of an error status is not set; expected failing assertion: T1, refused put.
- V2: the message carries the wrong index; expected failing assertion: T1, refused put.
- V3: an output record completes when its request is sent; expected failing assertion: T1, delayed reply.
- V4: a SET without a reply raises no alarm; expected failing assertion: T1, put without an agent.
- V5: the value sent is not the value written; expected failing assertion: T1, GET after the put.

Left out, with the reason:

- tooBig on a SET: it takes the path of M4, and the fixture agent has no object that answers a SET with tooBig.
- A mutation for the order of the puts that wait for completion and for the merged plain puts: both are behavior of Base, which this unit does not change; the checks of T2 guard against a later change of the module that would alter it.
- A check on a laboratory unit: no write to a unit is in scope.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-16T03:19Z to 2026-11-16T03:32Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The matrix cell `set-contract` (`tests/rewrite/test_set_contract.bash` with `set_contract.pl`), 21 checks of one write: for `ao`, `longout`, `int64out`, `stringout` and `lso` no alarm, the SET in the agent's log (41 and 123456789012 as logged values) and 2.5, 41, 123456789012, `abc` and `longer text` on the next GET; the delayed record active (PACT 1) right after the put and complete without alarm after the reply; the refused put WRITE/INVALID with `wrongType index 1`; the put without an agent COMM/INVALID with `no reply`. The status 7 and index 1 of the refusal were read from the agent's reply with `snmpset` beforehand (`work/m5-evidence/refused-set.txt`). On the product before the change (2026-11-16T03:17Z) the refused put fails with an empty message and the other checks pass. Mutations V1 and V2 fail the refused put, V3 the delayed record, V4 the put without an agent, V5 the logged value and the GET of `longout`. Evidence: `work/matrix-m5-1/o-set-contract/`, `work/m5-evidence/unchanged-1.log`, `work/tmp/m5-mutation/results/`, `work/m5-evidence/mutation-table.json`, `sources.sha256` |
| T2 | Same run as T1 | Same as T1 | Pass | 4 checks of consecutive writes in the same cell: three `caput -c` to one record end with status 0 each and the agent's log holds 7, 8, 9 in that order; three plain puts in one write to a record behind the delayed reply give 7 and 9 in the agent's log, and the record and the next GET hold 9. The same on the product before the change. V3 and V5 also change these values, as their rows of the table predicted for the merged puts. Same evidence as T1 |
| T3 | 2026-11-16T03:19Z to 2026-11-16T03:32Z | Same as T1 | Pass | `run_matrix.py --stages environment,ordinary` on the changed tree: 43 PASS and `tsan` NOT APPLICABLE; the 43 cells of the M16 run on the same Base (`work/matrix-m16-2/cells.tsv`) keep name and status, and the added cell `set-contract` is PASS with 25 checks. Evidence: `work/matrix-m5-1/cells.tsv` |

##### Closure Evidence

- Completion Criteria checked on 2026-11-16 against the Verification Results: the five output record types with their SET at the agent and the written value on the next GET, the delayed record active until its reply, the refused SET and the SET without a reply with their alarms and messages (T1), the two routes of consecutive writes (T2), the check of the refused SET seen failing on the unchanged code and the checks of T1 on the mutations V1 to V5, and the ordinary stage with the same 43 cells and the added cell (T3). The differences from the plan are decided by D27.
- Landing: the change is commit `4d47b302e3eec291db13336ddf95e6039b81e264`, the register record of the plan, decisions and results is commit `82394731c0f23b77b639fc7af989fd12901b9877`, and the `CHANGELOG.md` entry `0.0.4` is commit `4f4ab5e20455d4994bda72277d195a53e0e5f3ba`; all three were pushed to `origin/master`. After `git fetch`, `git rev-parse HEAD origin/master` printed `4f4ab5e` twice and `git diff --name-only 5ad4e82 origin/master` listed the 10 paths of the three commits (observed 2026-11-16T08:38Z).
- Tag: the annotated tag `0.0.4` (object `7fb67c62988fe5f21db1d338ab328819017e5140`) names `4f4ab5e`, the commit that holds the `CHANGELOG.md` entry, created and pushed under the owner's tag authority of 2026-11-16. Recheck with `git ls-remote --tags origin refs/tags/0.0.4 'refs/tags/0.0.4^{}'`, which printed that object and the peeled commit at 2026-11-16T08:38Z. The tagged tree also holds the code of M16, which is not closed; the `CHANGELOG.md` entry `0.0.4` names it as included and not yet verified on a unit.
- Issue #5: under the owner's Issue authority of 2026-11-16 its body was rewritten to the final behavior with the four acceptance items checked, the criterion on consecutive puts replaced by the behavior of Base (D26) and the known limits; a closing comment was posted and the issue was closed as completed. Recheck with `gh issue view 5 --json state,stateReason,closedAt`, which printed CLOSED, COMPLETED and 2026-11-16T08:37:54Z at 2026-11-16T08:38Z.
- The `CHANGELOG.md` entry was committed without a second-person pass, by the owner's decision of 2026-11-16.
- Unproven and left listed: a SET that a unit answers with tooBig, and any write to a laboratory unit.

##### GitHub Projection

Title: Complete output records on the SET result
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: closed (completed)
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-16T08:38Z; issue updatedAt 2026-11-16T08:37:54Z; title, label, assignee jeonghanlee and milestone unchanged; body carries the final behavior, the checked acceptance criteria and the known limits.

#### M7 - Shell-command configuration, MIB names and reply-driven processing

Origin: 1.0.0 / M6
Identity History: M6 -> M7 (baseline row inserted before it, 2026-11-10); title changed from "Reply-driven processing and driver-owned periodic acquisition" when D34 widened the scope (2026-11-17)
GitHub Issue: #7 - https://github.com/jeonghanlee/snmp3/issues/7
Status: In progress

##### Summary

An IOC application uses a support module through its database definition file, its library, its shell commands and its records (D30). snmp3 takes its whole configuration from one JSON file that `snmp3Load` reads: profiles, endpoints and, for every value, a binding with its numeric object identifier and type. A record names a binding of that file and nothing else, so one more value means an edit of the file and of the database, a period of acquisition cannot be set at all, and an object is written as a number. The `opcua` module of the site distribution (0.11.2) shows the form this unit follows: `opcuaSession name URL [options]`, `opcuaSubscription name session interval [options]`, `opcuaOptions pattern [options]`, `opcuaShow pattern verbosity`, and a record link `@<subscription or session> <object> [options]` with `SCAN=I/O Intr`. D34 puts the whole of that into this unit: sessions, options and subscriptions by shell command, the object in the record link, object names resolved through MIB files, and processing on the arrival of a reply. The JSON file stays as the form that gives the same definitions at once.

Observed on 2026-11-17 with the product of commit `e65b032`:

Acquisition, production `snmp3Ioc` and worker with the fixture agent, 20 samples at 1 s each (`work/m7-evidence/observe/`, runs `run-2` and `run-4`; `RUNS.txt` names the two discarded runs and why):

| Record | Reply | Processed in about 22 s | Requests at the agent | Largest LCNT | Alarm |
| --- | --- | --- | --- | --- | --- |
| `SCAN=1 second` | prompt | 22 | 22 | not read | none |
| `SCAN=.5 second` | delayed 750 ms | 21 | 22 | 1 | none |
| `SCAN=.1 second` | delayed 1000 ms | 19 | 20 | 9 | none |
| `SCAN=I/O Intr` | - | 0 | 0 | - | rejected at initialization (`snmp3: record initialization rejected:`), SCAN falls to `Passive`, the record stays UDF |

- A reply that takes longer than the scan period lowers the rate to the next multiple of the scan period, with no sign on the record. Base raises SCAN/INVALID (`Async in progress`) once a record was scanned more than 10 times while active (`MAX_LOCK` in `dbAccess.c` of Base 7.0.10); the fixture proxy delays at most 1000 ms, which reached 9, so that alarm is read from the source and was not seen.
- On the laboratory units (M4, T3) a record of the NMC2 unit took a new value every 5.8 s at the median while the NMC3 unit gave one every 2.0 s.

Configuration, read from the source:

- `snmp3Load` builds one configuration from the file, and the first record that takes a binding fixes it (`Config::bind` sets `frozen`); the runtime and the workers start at the hook `AfterFinishDevSup`, after every record was initialized. A value defined by a record can therefore reach the worker, once the configuration stays open until that hook.
- The module has one command of the form this unit adds, `snmp3QueueLimit(address, count, bytes)`. A command that fails calls `iocshSetError`, so a startup script with `on error break` stops.
- The object identifier parser takes digits and dots only (`Config.cpp`); the module reads no MIB file. The IOC process links no Net-SNMP; the worker and the probe do.

MIB names, a scratch program on the Net-SNMP library with the PowerNet MIB of the application (`work/m7-evidence/mib/`):

- With the standard MIB files of the host in reach, `PowerNet-MIB::rPDULoadStatusLoad.1` resolves to `.1.3.6.1.4.1.318.1.1.12.2.3.1.1.2.1` with the declared type Gauge and read-only access, a serial number object to an octet string, an outlet command object to an integer with read-write access; a name the MIB does not hold, and a name without its module, do not resolve. Six names take 0.02 s.
- Without the standard MIB files no name of the PowerNet MIB resolves: it imports `RFC1155-SMI`, `RFC1213-MIB` and `SNMP-FRAMEWORK-MIB`. On this Debian 13 host those files come from `snmp-mibs-downloader`, not from the base package, which holds 13 MIB files of Net-SNMP itself.
- In the probe of a scratch copy of the tree, run with the environment the module gives the probe (`PATH`, `LANG`, `LC_ALL`, `LD_LIBRARY_PATH` and nothing else), the same names resolve and the file each MIB was read from is known (`work/m7-evidence/verify/`). A MIB without imports parses. When several directories hold one module, the library takes the file of the directory listed last: listed as IOC, module, system it took the system's, listed in reverse it took the IOC's. Its read call returns the same for a module that no directory holds; `which_module` returns -1 for it.
- The license text of that package says that an RFC published before 2008-11-10 may be copied with its notice and its MIB extracted as it is for separate use, and may not be modified; MIB modules of later RFCs are under a BSD license (`debian-snmp-mibs-downloader-copyright.txt`).

Order of processing, a scratch IOC with five records on one `I/O Intr` list, three runs of three passes each (`work/m7-evidence/verify/order/`): the records of one PRIO process in the order of PHAS, and two of equal PHAS in the order they were loaded, in every pass; a record of another PRIO processes before, between or after them. `dbScan.c` of Base 7.0.10 keeps one list per priority and inserts by PHAS.

Record link, Base 7.0.10: the string after `@` is stored at its full length (`dbStaticLib.c`, `malloc(len + 1)`); a database file line of 3000 characters loads and one of 20000 ends the IOC with `scanner input buffer overflow`; `dbgf` and `dbDumpRecord` show at most 275 characters of it. A link stays well below 275 characters when the values that can be long sit in the options of a command.

The review of the earlier module (`docs/legacy-review.md`) names three faults that this unit must not bring back: a value kept by the module and read at any age, with a record processed twice for one reply; a value that depends on MIB files being present, because it was kept as text; and a timeout and a retry count shared by the whole IOC.

##### Scope

- Shell commands in the form of the `opcua` module, each with a usage text for `help`:
  - `snmp3ProbePath path`: the probe executable, as `snmp3WorkerPath` names the worker. Today the probe path is the second argument of `snmp3Load` and nothing else gives it, so an IOC without a file could not run the probe; `snmp3Load` keeps its argument. A probe path given a second time with another value, by either command, is rejected, as a changed worker path is today.
  - `snmp3Session name address [options]`: a unit under a name. `address` is a numeric address, alone or with a port: `192.0.2.1`, `192.0.2.1:161`, `2001:db8::1`, `[2001:db8::1]:161`; without a port it is 161. The options are those of `snmp3Options`, given at once.
  - `snmp3Options pattern [options]`: values of the sessions whose names match, as `name=value`: the sixteen fields a profile has in the file beside its `id` (`version`, `timeoutMs`, `retries`, `maxVarbinds`, `maxMessageSize`, `checkRanges`, `communityFile`, `user`, `securityLevel`, `authAlgorithm`, `authSecretFile`, `privAlgorithm`, `privSecretFile`, `contextName`, `securityEngineId`, `contextEngineId`) with the same names, defaults, ranges and rules. The command acts on the sessions that exist when it runs; a session defined later is not touched by it. A value is per session; none is IOC-wide.
  - `snmp3Subscription name session period`: a set of read objects of one session that the module reads by itself every `period` milliseconds, 100 to 3600000; it has no option; the first request leaves at once when the first record of the subscription joins the `I/O Intr` list.
  - `snmp3Mib path [system]`: a MIB file or directory of the IOC; with the word `system` as second argument, the path replaces the directories of the system for this IOC. Without such a line the directories of the system are the ones the Net-SNMP library has built in; its list begins with a directory under the home of the user, which the probe does not see because its environment holds no `HOME`. No environment variable is read.
  - `snmp3Show pattern verbosity`: for the sessions and subscriptions whose names match, at verbosity 0 one line per name, and at verbosity 1 also the values of each session, without the content of a secret file, the objects of the records with their links, the MIB places in their order and the path of the `mibs` directory of the module. At verbosity 1 it shows what was defined and, per object given by name, the numeric identifier, the type and the file its MIB came from; an object given by number is shown with its number and the type of its link, and no MIB is read for it. It also shows how many times the probe was started for names since the IOC started.
  - The commands that exist (`snmp3WorkerPath`, `snmp3Load`, `snmp3QueueLimit`, `snmp3Stop`, `snmp3Report`, `snmp3ConfigReport`, `snmp3RuntimeReport`) stay as they are.
- A record link that names its object: `@<session or subscription> <object> [options]`, where the object is a numeric identifier or `MODULE::name.index`, for input and output records. With a name, the type comes from the MIB, and an output record on a read-only object is rejected; with a number, the option `type=` gives it. A name may carry `type=` as well, for a unit that answers with another type than its MIB declares: the option then decides, and `snmp3Show` marks the object as overridden. Further options: `deadline_ms`, `group`, `systemInfo`. The size of a value is not an option: it comes from the record, the length of a text record and the number of elements of a waveform, as the device support takes it today. The link `@binding=<id> ...` of a binding of the file keeps working.
- MIB loading as a function of the module, run by the probe so that the IOC process reads no MIB: a module name is looked for in the places the IOC named with `snmp3Mib`, then in the `mibs` directory the module installs, which is empty at first and holds a note, then in the directories of the system. The path of that directory is fixed when the module is built, from its install location, and `snmp3Show` prints it. The first place that holds it is used, and the report names the place and the file; the library takes the directory listed last, so the function gives it the places in reverse, and it tells a missing module by `which_module`, not by the result of the read call. A MIB is used only to turn a name into an identifier, a type and an access at initialization; a value is never kept or read as text. The probe runs for names once for an IOC start, not once per record.
- Reply-driven processing: a record with `SCAN=I/O Intr` whose link names a subscription processes when the result of a request of that subscription has arrived, once per result and with the value, alarm and message a record-driven request gives. The module sends the next request of a subscription when the period has passed and the earlier request has ended and its records have been given to processing (`scanIoSetComplete`); it keeps no value of its own. The objects of a subscription leave in as few requests as the request size rules allow (M4), and the rules of M16 hold for them: the restart after requests without a reply, and no request to an endpoint whose unit shows another engine ID except system information in the state `changed`.
- The rules that keep the two ways apart: a record with `SCAN=I/O Intr` whose link names a session, and a record with another SCAN whose link names a subscription, are rejected at initialization. A subscription holds read objects only: the link of an output record names a session, and an output record whose link names a subscription is rejected.
- The order in which the records of one reply process, stated so that an application can put a group trigger on the last one: by PHAS, then by the order of loading, among records of one PRIO. Records of one reply with different PRIO have no order between them, and the documentation says so.
- `SCAN=I/O Intr` for the state records of M18: they process when the state they read has changed.
- Names: sessions and subscriptions share one set of names, so that the first word of a link means one thing; a command that defines a name that exists, of either kind and from a command or from the file, is rejected.
- The file and the commands together: `snmp3Load` gives the definitions of the file as the commands would, an endpoint of the file being a session under its `id`.
- Documentation: the commands, the link, the MIB search and its report, the table of who schedules a record processing, a request, a value and the completion of a SET, the scope of every setting, and the rule that one processing of an `I/O Intr` record is one result.

Out of scope: SNMP traps; GETBULK (M8); localized keys (M9); a PVA group made by the module; any change to a record with a periodic or passive SCAN on a binding of the file; the application repository.

##### Completion Criteria

- An IOC whose startup script holds only shell commands and whose records name their objects, with no JSON file, reads and writes the fixture agent: the twelve checks of T1, taken from the rules of the cells `set-contract`, `request-sizing` and `recovery`, hold as they do for bindings of the file.
- A session takes its values from `snmp3Options`: two sessions with different `timeoutMs` keep their own, and a pattern sets several sessions at once. A wrong command (an unknown option, a value outside its range, a name defined twice, an unknown session) fails, and a startup script with `on error break` ends there with the message of a failed startup: neither a line after it nor `iocInit` runs, whether the wrong line is the first definition or follows a valid one, and no secret reaches the output.
- A record whose link gives a name through a MIB reads the same value as one whose link gives the number, takes its type from the MIB, and is rejected at initialization with its name when the name does not resolve, when its record type cannot hold the declared type, or when it is an output record on a read-only object.
- A MIB is found in the three places in their order, the nearer place wins when two hold the same module, and `snmp3Show` names the place and file of each; with a MIB missing in all three, the records that need it are rejected and the IOC says which module is missing.
- The start of an IOC with 500 objects by name is measured beside the same IOC with objects by number, and the probe runs once for the names.
- With a prompt agent a record with `SCAN=I/O Intr` on a subscription of 500 ms processes once per reply, and the agent receives one request per period: 40 in 20 s, give or take 1. With a reply delayed 750 ms the next request leaves when the reply has been processed: about 26 in 20 s, where the same record with `SCAN=.5 second` gives about 20; the record never shows SCAN/INVALID.
- When the agent stops, the record carries COMM/INVALID with `no reply` at each attempt; when it returns with the same value, the record is without alarm within 30 s, with no restart of the IOC. While one agent is away, a subscription of another address keeps its period. When it returns with another engine ID, the record carries COMM/INVALID with the message of the state and takes no value, no further request for it reaches the responder, and a system information record of the subscription is still read in the state `changed`.
- The objects of one subscription leave as one request while the request size rules allow, and their records, given one PRIO, process in the order of PHAS.
- The three record cases the rules reject are rejected, and so is a period outside its range.
- A state record with `SCAN=I/O Intr` shows a change of the endpoint state and of the runtime state without a periodic scan.
- A configuration from a file behaves as before: the cells of the ordinary matrix stage keep their name and status. A file and commands in one startup script give one configuration.
- On the two laboratory units, read only, an IOC configured by commands, with objects given by name through the PowerNet MIB, reads the values that the IOC configured by the file reads, and one record per unit with `SCAN=I/O Intr` processes once per reply for 300 s without alarm.
- The documentation states what the Scope lists.

##### Dependencies And Decisions

- M4 (request size rules) is Complete. D28 removes the dependency on M6.
- The code of M16 is on `origin/master`; this unit keeps its rules for a request the module sends by itself and does not wait for its runs on the units.
- D30 sets the rule this unit serves, and D32 leaves `SCAN=I/O Intr` of the state records to it.
- D34 widens the scope to the whole configuration by shell command, with the `opcua` module as the model, the JSON file kept, MIB loading inside the module and the search order IOC, module, system.
- The tag of this unit is the next free number at its closing (D31).
- New tests are written in bash and Perl as matrix cells (D19), and are limited to what the purpose needs.
- D36 decides the six points that were open: no standard MIB file in the repository at first, a period of 100 to 3600000 ms, the first request at once, no MIB name for an object given by number, the command `snmp3ProbePath`, and `type=` beside a name as an override.
- Part A is done (2026-11-17): the cell `shell-config` with 23 checks (T1, the lines of T2 that need no subscription, and the IOC of T9 with both forms), seen failing on the unchanged tree and passing on the changed one, and the ordinary matrix with 46 PASS and `tsan` NOT APPLICABLE, the 46 cells of the M18 run unchanged and `shell-config` added (`work/matrix-m7-a5/cells.tsv`). The pages `docs/snmp-rewrite-config.md`, `docs/snmp-rewrite-contract.md` and `docs/snmp-worker-reference.md` describe the four commands and the second link form. T2 and T9 are recorded when their other parts have run.
- Differences between the accepted plan and the work as run. Item 1 waits for the owner's decision, item 2 is decided, and items 3 to 7 report what was done:
  1. T2 asked that no output of a wrong IOC holds the address of a session. The IOC shell prints every line of the startup script, so the address is in the standard output by the script itself. The cell checks that no line of the module (`snmp3...` on the standard error) holds the address and that no output holds the content of a secret file.
  2. Decided by the owner on 2026-11-17: `deadline_ms` is optional on both link forms. Without it the budget is `2 x timeoutMs x (retries + 1) + 1000` ms, at most 600000, with the values of the session, or of the profile of the endpoint for a binding of the file. A link `@binding=<id>` without `deadline_ms` was rejected before and is accepted now, so `ConversionTest` and the reject cases of `tests/rewrite/db/record-contract.db` and `record-edges.db` changed: the two database cases now carry `deadline_ms=` without a value, which is still rejected.
  3. A binding now carries its own number of the worker protocol. The scheduler and the worker list each numbered the bindings by the order of their IDs, which holds only while the set of bindings does not grow; a binding that a record defines takes the next number, and the bindings of a file keep the numbers they had (`Binding.h`, `Scheduler.cpp`, `Ipc.cpp`, `Supervisor.cpp`).
  4. The ordinary matrix was run twice during part A, beyond the one run at the end that the size names. The first run found four existing cells failing (`config`, `lifecycle`, `qualification`, `supervisor-ioc`) on two faults of the change: the configuration object was copied at `iocInit` although no session existed, and a binding that was not found fixed the definitions. Both were corrected and the second run passed (`work/matrix-m7-a1/`, `work/matrix-m7-a2/`); item 7 names the run that is recorded.
  5. The first run of the cell on the changed code found a fault of the change: the IOC took a binding by its place, not by its number, where it reserves the size of a result, and the worker was contained on the writes of `int64out` and `lso` (`changed-1.log`).
  6. A third-person review of part A found two faults of the change, each with a check added to the cell that failed before the correction (`before-review-fix-1.log`) and passes after it: `snmp3Options` on an endpoint of the file that left it incomplete ended the IOC with a segmentation fault at `iocInit`, and is now rejected at the command (`wrong-file-option`); a record that was rejected after its link defined a binding left that binding behind, and the binding is now removed (`orphan-object`). The third added check is `default-deadline`. The review also noted that the scheduler did not count the 256 bytes of fixed storage for a binding defined after its construction; `Scheduler::registerBinding` now adds them, and no check observes that sum.
  7. Three more matrix runs followed the review: `work/matrix-m7-a3/` (44 PASS, `rec-contract` and `rec-edges` FAIL on the reject cases of item 2), `work/matrix-m7-a4/` (45 PASS, `rec-edges` FAIL on the second of them) and `work/matrix-m7-a5/`, the one recorded.
- Part B is done (2026-11-17): the cell `mib-names` with 12 checks (T3 and T11), seen failing on the unchanged tree and passing on the changed one, and the ordinary matrix with 47 PASS and `tsan` NOT APPLICABLE, the 47 cells of the part A run unchanged and `mib-names` added (`work/matrix-m7-b6/cells.tsv`, after the review corrections and the rule of item 7). The three pages of part A describe `snmp3Mib`, the object by name, the search order and the `snmp3 mib:` lines.
- Differences between the accepted plan and part B as run. Item 7 is a rule the owner added; the others report what was done:
  1. Of the 12 checks, `none-rejected` passes on the unchanged tree as well: that product rejects every link that carries a name, so the check cannot fail there. It guards the changed code against a name that is taken although no place holds its module.
  2. The case `place-module` copies the fixture MIB into the `mibs` directory of the tree under test for the time of the case and removes it afterwards, because the path of that directory is fixed when the module is built.
  3. A run with the PowerNet MIB of the application repository, in one IOC without an agent, was made beside the cell and found a fault of the change: `snmp3Show` listed all 380 modules the library had found in the system directories, read or not. The number of module lines was added to the three shown checks, and two of them, `ioc-shown` and `module-shown`, failed on that code (`changed-2.log`; the third replaces the system directories by a directory with one module), and the probe now reports only the modules that were read (`observe/powernet/`). The ordinary matrix was started once before that correction and stopped; item 6 names the complete runs and the one that is recorded.
  4. The three pages were written in this part, as in part A, and not in part E.
  5. A third-person review of part B found two gaps of the change in the probe. A name without the index of an instance, a scalar without `.0`, was taken, and its record failed only when it was read; such a name is now rejected at initialization, with a record of that kind added to the case `wrong-records`, which failed before the correction (`before-review-fix-1.log`). The list of directories built into the library begins with an entry under the home of the user, which the probe, having no `HOME`, read as a directory below the working directory of the IOC; only the absolute entries are used now (`observe/review-fix.txt`). A name that is no object with a value, a table, a group, a notification or an identity, is rejected as well, with or without `type=`. The review also had the text of a failed probe run for names separated from the one of the capability query, and the `mibs` directory added to the directories that `make uninstall` removes.
  6. The ordinary matrix was run six times in part B, beyond the one run at the end that the size names: `work/matrix-m7-b1/` before the review and `work/matrix-m7-b2/` after its corrections, each with 47 PASS; `work/matrix-m7-b3/` after the rule for a name that is no object, with `rec-stop-enqueue-failed` FAIL on its check `drain-returns-on-completion-wake`, which allows 5 ms, while a second tree was being built and the cell was running on this host; `work/matrix-m7-b4/` on the same product with nothing else started from this work, with 47 PASS; `work/matrix-m7-b5/` after that rule took in notifications, groups and identities, with 47 PASS; and `work/matrix-m7-b6/` after the rule of item 7, with 47 PASS, the one recorded. The cause of the one failure was not established beyond that.
  7. Decided by the owner on 2026-11-17, beyond the access rule of the plan (an output record on a read-only object is rejected): an input record on an object that the MIB declares not readable, `not-accessible`, `accessible-for-notify` or `write-only`, is rejected at initialization as well. No option overrides it; a unit that does not follow its MIB is read through the numeric identifier. A record of that kind was added to the case `wrong-records`, which failed before the rule (`before-access-rule-1.log`).
- Issue #7 still carries the title and body of the scope before D34 (see GitHub Projection). Its body is rewritten after the acceptance of this plan, under Issue authority.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-17, accepted by the owner after five third-person and three second-person passes of this detail
Implementation Authorization: 2026-11-17, by the owner: parts A to E of the accepted plan, that is edits and test runs in this repository and the read-only runs on the units; every commit, push, tag and issue action needs its own authority
Superseded Plan Artifacts: the draft of 2026-11-10, written before M4, M16 and M18; the draft of 2026-11-17 that put a field `periodMs` on a binding of the file, before D34

The work is done in five parts, each closed by its cell before the next begins, so that a fault is found in the part that made it. Every new cell has a check design table in the header of its Perl file, written before its code. A case that changes the state of an IOC or of an agent has its own IOC and its own agents and sets its own first state, and a cell runs one case alone on request, so that no case leans on what an earlier one left.

1. Part A, definitions by command and by record. Check first: a matrix cell `shell-config` in bash and Perl that starts an IOC from commands only and holds the twelve checks of T1, the checks of T2 that need no subscription and the IOC of T9 with both forms, no more; run on the unchanged code and the failing output recorded. Then: the configuration stays open until `AfterFinishDevSup` and takes sessions, options and record-defined objects (`snmp3App/src/Binding.h`, `Config.h`, `Config.cpp`); the commands `snmp3ProbePath`, `snmp3Session`, `snmp3Options`, `snmp3Show` with usage texts (`Register.cpp`); the link grammar and the definition of an object at record initialization (`Request.h`, `DeviceSupport.cpp`); `snmp3Load` through the same path.
2. Part B, MIB names. Check first: a cell `mib-names` with the checks of T3 and T11: a small MIB of the fixture objects written without imports, which the observation showed to parse, the three places, a missing module, the three rejections and the two start times. Then: the resolution in the probe (`snmp3App/native/NativeProbe.cpp`, `Capabilities.cpp` or a new source), the command `snmp3Mib`, the search order and its report, the `mibs` directory of the module (`snmp3App/Makefile`, a new directory), the type and access checks at record initialization.
3. Part C, subscriptions and processing on a reply. Check first: a cell `reply-driven` with the checks of T4, T5, T6 and T7 and the four lines of T2 that name a subscription. Then: `snmp3Subscription`; the periodic requester through the admission path a record uses, with the rules of M4 and M16 (`Request.cpp`, `Scheduler.cpp`, the servicing loop in `Runtime.cpp`); `get_ioint_info`, `scanIoRequest` and `scanIoSetComplete` in the input device supports, the three rejected record cases, and the order of processing of one reply (`DeviceSupport.cpp`).
4. Part D, `SCAN=I/O Intr` of the state records (`StateSupport.cpp`, `Peer.cpp`, `Runtime.cpp`, `Scheduler.cpp`), checked in the cell `module-state`.
5. Part E, documentation (`docs/snmp-rewrite-config.md`, `docs/snmp-record-behavior.md`, `docs/snmp-worker-reference.md`, `docs/configure-procedure.md`), the mutations of every part, the ordinary matrix, and the run on the two units, GET only, with the addresses, names and secrets kept in a private directory under `work/` and out of every recorded text.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Fixture, commands and record links | Cell `shell-config`: an IOC started from `snmp3ProbePath`, `snmp3Session`, `snmp3Options` and records that name their objects by number, with no file, against the fixture agent; twelve checks taken from the rules of three cells | Linux host, ordinary product | From `set-contract`: for `ao`, `longout`, `int64out`, `stringout` and `lso` the written value reaches the agent and the next GET returns it (5); a record behind a reply delayed 750 ms is active right after the put and without alarm after the reply (1); a refused SET is WRITE/INVALID with `wrongType index 1` (1); a SET after the agent stopped is COMM/INVALID with `no reply` (1). From `request-sizing`, under SNMPv2c and under SNMPv3: a group of eight whose eighth object answers tooBig ends at the limit 1 with 3 reductions, the eighth record with `tooBig` (2). From `recovery`, under SNMPv3: the input records carry COMM/INVALID with `no reply` while the agent is stopped, and are without alarm within 30 s of its return with the same engine ID (2) |
| T2 | Fixture, wrong commands | Separate IOCs, each with one wrong line under `on error break`, followed by a line that writes a marker file and by `iocInit`: an unknown option, `timeoutMs` 0, a session defined twice, a subscription under the name of a session, options for an unknown session, a subscription on an unknown session, a period of 99 and one of 3600001; the unknown option once as the first definition of the script and once after a valid session with its options; one wrong IOC names a secret file whose content is a known random text. And one IOC with two sessions whose `timeoutMs` differ, one of them set through a pattern | Linux host, ordinary product | Each wrong IOC ends with `snmp3Ioc: startup script failed` and exit status 1; the marker file is absent and the line of complete initialization is absent, in the first position and after a valid session alike; the output names the fault of the line, and neither the standard output nor the standard error holds the random text of the secret file or the address of a session; in the last IOC `snmp3Show` prints the two values |
| T3 | Fixture, MIB names | Cell `mib-names`: a fixture MIB placed in turn in the place of the IOC, of the module and of a stand-in for the system (a directory given with `snmp3Mib path system`), then in two places with different content, then in none; records by name beside records by number; an input record whose type cannot hold the declared type; an output record on a read-only object; a name the MIB does not hold | Linux host, ordinary product | By name and by number read the same value; `snmp3Show` names the place and file, the nearer place when two hold the module; the three wrong records and, with no MIB, every record by name are rejected at initialization and named, and the missing module is named |
| T4 | Fixture, period | Cell `reply-driven`: a record with `SCAN=I/O Intr` on a subscription of 500 ms with a prompt agent, and one on a second address behind the proxy that delays each reply 750 ms; a counter record on the forward link of each; 20 s between two readings | Linux host, ordinary product | Prompt: the agent logs 40 requests, give or take 1, and the counter rises by the same number. Delayed: both numbers lie between 24 and 28 and equal each other give or take 1; SEVR is NO_ALARM in every reading and LCNT is 0. The configured period, timeout and retry count with their scope, and the agent's log of every request, are kept as evidence beside the counts |
| T5 | Fixture, loss and return on a subscription | The same cell: the prompt agent is stopped and started again with its engine ID and the same value, while a subscription of 500 ms on a second address keeps running against its own agent; an agent under SNMPv3 returns with another engine ID, with one value record and one system information record of a subscription on it | Linux host, ordinary product | While away: COMM/INVALID with `no reply`, with the value, PACT, UDF, STAT and SEVR of the record kept in each reading. After the return with the same engine and the same value: NO_ALARM within 30 s, although the value did not change. During the 20 s in which the first agent is away, the agent of the second address logs 40 requests, give or take 1, and its record's counter rises by the same number. After the return with another engine: the value record is COMM/INVALID with `engine ID changed: restart IOC` and keeps its value, the system information record carries the responder's value with that alarm, and the agent logs no request for the value object after the state was settled |
| T6 | Fixture, one request and the order | The same cell: three objects of one subscription through the proxy in mode `pass`, which logs every datagram; their records have one PRIO and PHAS 2, 0 and 1, and a record on the forward link of each takes the next number of a shared counter | Linux host, ordinary product | The number of request datagrams equals the number of processing passes of one record; in every pass the numbers rise in the order PHAS 0, 1, 2 |
| T7 | Initialization | A record with `SCAN=I/O Intr` whose link names a session, a record with `SCAN=1 second` whose link names a subscription, and an output record whose link names a subscription | Linux host, ordinary product | The three are rejected at initialization and named |
| T8 | Fixture, state records | The cell `module-state` with the state databases loaded with `SCAN=I/O Intr`: the endpoint that returns with another engine ID, then `snmp3Stop` | Linux host, ordinary product | `$(P)PeerState` reads `changed` and `$(P)RuntimeState` reads `Stopped` when read at once after the log line and after the command, with no periodic scan on the records |
| T9 | Regression and both forms | `tests/rewrite/run_matrix.py --stages environment,ordinary` on the unchanged and the changed tree; in `shell-config` one IOC with a file and commands in one script | Linux host | The cells of the unchanged tree keep their name and status, and the added cells are PASS; the IOC with both forms reads the objects of both |
| T10 | Units, read only | Two IOCs on the two laboratory units, GET only: one configured by the private file `work/ioc-new/private/config.json` with the records of `work/ioc-new/apcpdu_snmp3/apcpduApp/Db/ap8932_min.template`, one by commands with the same objects given by name through the PowerNet MIB of the application repository (`mibs/PowerNet-MIB` there, read and not changed), the secrets given as the same private files; then, by commands, one record per unit with `SCAN=I/O Intr` on a subscription of 2000 ms with a counter on its forward link, 300 s | Both laboratory units | The two IOCs read the same values for the identity objects and values without alarm for the others; `snmp3Show` names the place and file of the PowerNet MIB and of each MIB it imports; each `I/O Intr` record is NO_ALARM in every reading and its counter rises by the number of replies, recorded with the interval between values beside the 2.0 s and 5.8 s that M4 recorded |
| T11 | Startup time | Two IOCs with 500 input records on 500 objects that the fixture agent serves from a list given to it as its fifth argument (`OBJECTS`, whose row form the source of the agent states), one IOC with the objects by number and one by name through a MIB written for the test with those 500 objects, which lie below `1.3.6.1.4.1.318` because the agent takes listed objects only there; the time from the start of the process to the line of complete initialization, three starts each | Linux host, ordinary product | The two times are recorded beside each other; by name `snmp3Show` reports one start of the probe for names, and by number none |

The mutations are applied one at a time to scratch copies of the changed tree; the exact old and new text of each is written when the code of its part exists:

- V1: a session ignores its own `timeoutMs` and uses the default; expected failing assertion: T2, the two values.
- V2: a failed command does not set the shell error; expected failing assertion: T2, the wrong IOCs.
- V3: an output record takes a read-only object; expected failing assertion: T3.
- V4: the type of a record by name is not checked against the MIB; expected failing assertion: T3, the wrong input record.
- V5: the search takes the system before the IOC; expected failing assertion: T3, two places.
- V6: the next request leaves on the period without waiting for the earlier one; expected failing assertion: T4, the delayed counts.
- V7: no scan request when a result is staged; expected failing assertion: T4, the counters.
- V8: the refusal rule of the endpoint state is not applied to a request of a subscription; expected failing assertion: T5, no request after the state was settled.
- V9: each object of a subscription is sent alone; expected failing assertion: T6, the datagram count.
- V10: a record with `SCAN=I/O Intr` on a session is accepted; expected failing assertion: T7, that record.
- V11: the range of the period is not checked; expected failing assertion: T2, the two periods.
- V12: no scan request when the state of an endpoint changes; expected failing assertion: T8, `changed`.
- V13: the requester of one address waits while a request of another address is unanswered; expected failing assertion: T5, the count of the second address.

Size of the test, stated before it is built and accepted by the owner on 2026-11-17: three new cells (`shell-config`, `mib-names`, `reply-driven`) and an extension of `module-state`; about 75 checks (T1 12, T2 16, T3 10, T4 6, T5 14, T6 2, T7 3, T8 2, T9 1, T10 7, T11 2); 13 mutation rows; for each part one run on the unchanged code and one on the changed code; the mutation series with one build per row; one run of the ordinary matrix at the end; on the units two read-only IOCs and one run of 300 s. A check, a series or a tool beyond this is not built; a finding outside it is reported.

Left out, with the reason:

- A checker script for a PVA group: this unit makes no group.
- The SCAN alarm of Base after more than 10 scans of an active record: a rule of Base, read from its source; the proxy cannot delay long enough, and the new way does not scan an active record.
- An exact interval between two requests: the criterion is the count over 20 s, which the agent's log gives without a clock of the test.
- Every option of `snmp3Options` in T2: the options are the fields of a profile, whose defaults and ranges the cell `config` asserts for the file; T2 shows that the command reaches the same check.
- A MIB of another vendor: the fixture MIB and the PowerNet MIB on the units cover a MIB without and with imports.
- A link cut or a power cut on the laboratory units: that is M16.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-17T06:04Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The cell `shell-config` (`tests/rewrite/test_shell_config.bash` with `shell_config.pl`), IOCs started from `snmp3ProbePath`, `snmp3Session` and records that name their objects by number, with no file: the five output record types reach the agent and read back (2.5, 41, 123456789012, `abc`, `longer text`); the record behind the reply delayed 750 ms is active right after the put and without alarm after it; the refused SET is WRITE/INVALID with `wrongType index 1`; the SET after the agent stopped is COMM/INVALID with `no reply`; the group of eight ends at `limit=1 splits=3` with `tooBig` on its eighth record under SNMPv2c and under SNMPv3; the inputs carry COMM/INVALID with `no reply` while the agent is away and are without alarm within 30 s of its return. These are the twelve checks; the cell passes its 23 checks (2026-11-17T07:41Z, after the review corrections). On the unchanged tree the 20 checks of the first form of the cell fail, the IOCs ending at `Command 'snmp3ProbePath' not registered`. Evidence: `work/m7-evidence/part-a/unchanged-3.log`, `changed-3.log`, `before-review-fix-1.log`, `changed-5.log`, `RUNS.txt`, `sources.sha256` (the hashes of the run of 07:41Z; `sources-changed-3.sha256` holds those of the run of 06:04Z) |
| T2 | Not run | Linux host, ordinary product | Pending | none |
| T3 | 2026-11-17T20:39Z to 2026-11-17T20:53Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The cell `mib-names` (`tests/rewrite/test_mib_names.bash` with `mib_names.pl`), 10 checks. With the fixture MIB, written without imports, in the place of the IOC, in the `mibs` directory of the module and in a directory given with `snmp3Mib <path> system`, the record by name and the record by number both read 4321 without alarm after 4321 was written, and `snmp3Show` names the place, the file, one module, the identifier and the type `integer` with `typeSource=mib`; an object whose link carries a name and `type=` shows `typeSource=link`. With the MIB as a file of the IOC and a MIB of the same module with another identifier in the system directory, the record by name reads 4321 and the place shown is the IOC's. A text record on an integer object, an output record on a read-only object, a name the MIB does not hold, a name without its index and an input record on an object declared `not-accessible` are rejected and named, and a valid record by name beside them reads without alarm. With the MIB in no place, the two records by name are rejected, the record by number reads without alarm, and the IOC prints `snmp3: MIB module SNMP3-FIXTURE-MIB not found` once. On the unchanged tree 9 of the 10 fail, with the cell in its last form (see the differences of part B). Evidence: `work/m7-evidence/part-b/unchanged-3.log`, `changed-2.log`, `before-review-fix-1.log`, `before-access-rule-1.log`, `changed-10.log`, `RUNS.txt`, `sources.sha256` |
| T4 | Not run | Linux host, ordinary product | Pending | none |
| T5 | Not run | Linux host, ordinary product | Pending | none |
| T6 | Not run | Linux host, ordinary product | Pending | none |
| T7 | Not run | Linux host, ordinary product | Pending | none |
| T8 | Not run | Linux host, ordinary product | Pending | none |
| T9 | Not run | Linux host | Pending | none |
| T10 | Not run | Both laboratory units | Pending | none |
| T11 | Same run as T3 | Same as T3 | Pass | The same cell, 2 checks: two IOCs with 500 input records on 500 objects that the fixture agent serves from a list, three starts each. By number the time from the start of the process to the line of complete initialization is 0.661 s, 0.646 s and 0.655 s and `snmp3Show` reports `probeStarts=0`; by name through a MIB of those 500 objects it is 0.722 s, 0.749 s and 0.736 s with `probeStarts=1`; in the last start of each the first record reads its value without alarm. On the unchanged tree both checks fail. Evidence: as T3 |

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
Status: Not started

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

- G1 complete (2026-11-13); status restored to Not started.
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
Status: Complete

##### Summary

Give the snmp3 shared libraries on Linux explicit ABI-versioned SONAMEs and reproducible symbolic-link installation. `1.0.0` asserts a stable library interface, so this work precedes the release (D3).

Inventory of 2026-11-16 on the product of commit `dcca500`:

| Library | SONAME today | Exported symbols | Consumers |
| --- | --- | --- | --- |
| `libsnmp3.so` | `libsnmp3.so` | 249 | `snmp3Ioc`, and every IOC application that links the module |
| `libsnmp3Wire.so` | `libsnmp3Wire.so` | 102 | `libsnmp3.so`, `snmp3Worker`, and the measurement IOC application `apcpdu`, whose Makefile links it beside `snmp3` |
| `libsnmp3Native.so` | `libsnmp3Native.so` | 98 | `snmp3Worker`, `snmp3NativeProbe` |

The module installs no header files, and `dbd/snmp3.dbd` names the registrar and eleven device support entries. A consumer outside the module can use more than that: the measurement IOC application `apcpdu` takes `Runtime.h` from the module's source directory (`USR_INCLUDES += -I$(SNMP3)/snmp3App/src`), calls `snmp3::Runtime::instance()` and `snmp3::Runtime::snapshot()`, and links `libsnmp3Wire.so` beside `libsnmp3.so`. `libsnmp3Native.so` is used only by the module's own executables.

Base 7.0.10 has the mechanism: with `SHRLIB_VERSION` set, its build rules write the real file with the version in its name, give it that name as SONAME, and make the unversioned link, as for its own `libCom.so -> libCom.so.3.25.0`. Tried in a scratch copy with `SHRLIB_VERSION = 1` in `snmp3App/src/Makefile` and `snmp3App/native/Makefile`: the three libraries are installed as `lib<name>.so.1` with the link `lib<name>.so`, their SONAMEs and the NEEDED entries of `snmp3Ioc`, `snmp3Worker` and `snmp3NativeProbe` carry `.so.1`, and the `set-contract` cell passes on that build (`work/tmp/m10-probe/`).

##### Scope

- `SHRLIB_VERSION` in the two Makefiles, with the ABI number of D29.
- The rule of D29 for when the number changes, and what an installation looks like, in the documentation, with the move from an installation without versions: the build replaces the unversioned file by the link of the same name, a consumer built before the change names the unversioned library and keeps starting through that link, and a rebuild of the consumer records the versioned name.
- A check of the installed names: real files, links, SONAMEs and the NEEDED entries of the consumers.

Out of scope: a policy for all EPICS modules, device behavior, a three-level link chain (D29), and installed header files.

##### Completion Criteria

- After a normal build each of the three libraries is a real file `lib<name>.so.<ABI>` with that name as its SONAME and a link `lib<name>.so` to it, and a second build leaves the links valid.
- `snmp3Ioc`, `snmp3Worker` and `snmp3NativeProbe` name the versioned SONAMEs, and the real IOC, worker and probe paths run: the cells of the ordinary matrix stage keep their name and status.
- A consumer linked against one ABI number does not start against an installation that holds only another number, and says which library it misses; a consumer linked before the change, which names `libsnmp3.so`, still starts through the link.
- The documentation states the names, the rule for the number and the three facts of the move from an installation without versions that the Scope lists.

##### Dependencies And Decisions

- D3 includes this work in `1.0.0`.
- D29 fixes the two-level form of Base, the single ABI number starting at 1, and the rule for raising it.
- The tag of this unit is taken in the order of closing (D27, D28): D31 gives it `0.0.5`, because it closes before M16.
- New tests are written in bash and Perl as a matrix cell (D19), and are limited to what the purpose needs.
- The work as run follows the accepted plan; no difference was recorded (2026-11-16).

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-16, accepted by the owner after the third-person and second-person passes of this detail were closed
Implementation Authorization: 2026-11-16, by the owner: items 1 to 4 of the accepted plan, that is edits and test runs in this repository; every commit, push, tag and issue action needs its own authority
Superseded Plan Artifacts: the draft that began with the inventory and proposed the rules afterwards; the inventory is done and recorded in the Summary, and D29 holds the rules

1. Write the check first: a matrix cell in bash and Perl that reads, for the tree it runs in, the real files, links, SONAMEs and NEEDED entries, with the expected names built from the ABI number 1 and the library names as literals. Run it on the unchanged code and record the failing output.
2. Set `SHRLIB_VERSION` in `snmp3App/src/Makefile` and `snmp3App/native/Makefile`.
3. Document the names, the rule of D29 and the move from an installation without versions, as the Scope lists it, in `docs/configure-procedure.md`.
4. Run the cell, the two checks of T2 in scratch copies, the mutations and the ordinary matrix.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Build and install | The new cell on the built tree, run twice with a build between the runs | Debian 13 host, ordinary product | For `snmp3`, `snmp3Wire` and `snmp3Native`: a real file `lib<name>.so.1`, SONAME `lib<name>.so.1`, a link `lib<name>.so` to it; `snmp3Ioc` names `libsnmp3.so.1`, `libsnmp3.so.1` names `libsnmp3Wire.so.1`, `snmp3Worker` names `libsnmp3Native.so.1` and `libsnmp3Wire.so.1`, `snmp3NativeProbe` names `libsnmp3Native.so.1`; the same after the second build |
| T2 | Another number and an earlier consumer | In scratch copies: the `snmp3Ioc` of a build with the number 1 started against the libraries of a build with the number 2; and a `snmp3Ioc` built before the change, which names `libsnmp3.so`, started against the changed libraries | Debian 13 host | The first does not start and the loader names `libsnmp3.so.1` as missing; the second starts and runs a startup script to its end |
| T3 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` on the unchanged and the changed tree | Debian 13 host | The cells of the unchanged tree keep their name and status, and the added cell is PASS |

The mutations are applied one at a time to scratch copies of the changed tree; the exact old and new text of each is written when the code exists:

- V1: `SHRLIB_VERSION` is removed from `snmp3App/src/Makefile`; expected failing assertion: T1, names of `snmp3` and `snmp3Wire`.
- V2: `SHRLIB_VERSION` is removed from `snmp3App/native/Makefile`; expected failing assertion: T1, names of `snmp3Native`.

Left out, with the reason:

- A build on Rocky Linux 8.10: the names come from the build rules of Base, which are the same file on both systems (`RULES_BUILD`, `CONFIG.Common.UnixCommon` and `CONFIG_COMMON` of the local Debian 13 and Rocky Linux 8.10 trees of Base 7.0.10 have equal hashes); Release Verification 5 and 7 of M11 inspect the installed libraries and install on Rocky Linux 8.10.
- A replacement of a library by a compatible one of the same number: the consumer names the SONAME, and a file of the same name is loaded whatever build wrote it; T2 shows the two cases in which the name decides.
- A check of the exported symbols against a list: no interface is promised before `1.0.0` (D29).

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-16T18:51Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The cell `library-names` (`tests/rewrite/test_library_names.bash` with `library_names.pl`) passes its 13 checks on the built tree, and again after a second build: for `snmp3`, `snmp3Wire` and `snmp3Native` a regular file `lib<name>.so.1` with that SONAME and a link `lib<name>.so` to it; `snmp3Ioc` names `libsnmp3.so.1`, `libsnmp3.so.1` names `libsnmp3Wire.so.1`, `snmp3Worker` names `libsnmp3Native.so.1` and `libsnmp3Wire.so.1`, `snmp3NativeProbe` names `libsnmp3Native.so.1`. In the working tree the first build replaced the three unversioned files by links. On the unchanged tree all 13 checks fail. Mutation V1 fails the six checks of `snmp3` and `snmp3Wire` and the three consumer checks that name them; V2 fails the three checks of `snmp3Native` and the two consumer checks that name it. Evidence: `work/m10-evidence/unchanged-1.log`, `changed-1.log`, `changed-2.log`, `mutation-table.json`, `sources.sha256`, `work/tmp/m10-mutation/results/` |
| T2 | 2026-11-16T18:51Z | Debian 13 host | Pass | A `snmp3Ioc` built before the change, which names `libsnmp3.so`, placed beside the changed libraries loads `libsnmp3.so` through the link and `libsnmp3Wire.so.1` behind it, runs a startup script that loads the database definition, registers the support and prints the report, and exits 0. The `snmp3Ioc` of the changed tree, which names `libsnmp3.so.1`, placed beside the libraries of a scratch build with `SHRLIB_VERSION = 2` (`libsnmp3.so.2` and its link) does not start: `error while loading shared libraries: libsnmp3.so.1: cannot open shared object file`, exit 127. Evidence: `work/m10-evidence/t2/` |
| T3 | 2026-11-16T18:52Z to 2026-11-16T19:05Z | Same as T1 | Pass | `run_matrix.py --stages environment,ordinary` on the changed tree: 44 PASS and `tsan` NOT APPLICABLE; the 44 cells of the M5 run on the same Base (`work/matrix-m5-1/cells.tsv`) keep name and status, and the added cell `library-names` is PASS with 13 checks. Evidence: `work/matrix-m10-1/cells.tsv` |

##### Closure Evidence

- Completion Criteria checked on 2026-11-16 against the Verification Results: the real file, the SONAME and the link of each of the three libraries after a build and after a second build, and the versioned names in `snmp3Ioc`, `snmp3Worker` and `snmp3NativeProbe` (T1); the consumer that does not start against another number and names the missing library, and the consumer built before the change that starts through the link (T2); the ordinary stage with the same 44 cells and the added cell (T3); the names, the rule for the number and the three facts of the move in `docs/configure-procedure.md`. No difference from the plan was recorded.
- Landing: the change is commit `14bcb99`, the register record of the results and of D31 is commit `9b010dd`, and the `CHANGELOG.md` entry `0.0.5` is commit `a1a66d675ba8ae5cb04d98ad045f956d6c70f45b`; all three were pushed to `origin/master`. After `git fetch`, `git rev-parse HEAD origin/master` printed `a1a66d6` twice and `git diff --name-only cdfbefe origin/master` listed the 8 paths of the three commits (observed 2026-11-16T20:57Z).
- Tag: the annotated tag `0.0.5` (object `6aaa4b82572927a2571ab9a0776e12914a54f0a2`) names `a1a66d6`, the commit that holds the `CHANGELOG.md` entry, created and pushed under the owner's tag authority of 2026-11-16. Recheck with `git ls-remote --tags origin refs/tags/0.0.5 'refs/tags/0.0.5^{}'`, which printed that object and the peeled commit at 2026-11-16T20:57Z. The tagged tree also holds the code of M16, which is not closed; the `CHANGELOG.md` entry `0.0.4` names it as included and not yet verified on a unit.
- Issue #1: under the owner's Issue authority of 2026-11-16 its body was rewritten to the final form with the four acceptance items checked, the three-level chain replaced by the two-level form of Base (D29), the Rocky Linux build and the package path named as not run with the reason, and the known limits; the issue was closed as completed. Recheck with `gh issue view 1 --json state,stateReason,closedAt`, which printed CLOSED, COMPLETED and 2026-11-16T20:56:56Z at 2026-11-16T20:57Z.
- The document changes of the first two commits had one second-person pass before commit; the `CHANGELOG.md` entry was committed without one, by the owner's decision of 2026-11-16.
- Unproven and left listed: a build on Rocky Linux 8.10 (Release Verification 5 and 7 of M11), a replacement by a compatible library of the same number, and a check of the exported symbols.

##### GitHub Projection

Title: Version SNMP3 shared libraries with ABI SONAMEs
Labels: enhancement
GitHub Milestone: 1.0.0
Observed State: closed (completed)
Observed Labels: enhancement
Observed Milestone: 1.0.0
Last Compared: 2026-11-16T20:57Z; issue updatedAt 2026-11-16T20:56:56Z; title, label, assignee jeonghanlee and milestone unchanged; body carries the final form, the checked acceptance criteria and the known limits.

#### M18 - Module state as records

Origin: 1.0.0 / M18
Identity History: none
GitHub Issue: none
Status: Complete

##### Summary

An IOC application uses a support module through its database definition file, its library, its shell commands and its records (D30). The site modules do the same: autosave reports its state to the records of `save_restoreStatus.db`, and iocStats offers its values as records. The state of snmp3 is visible today only where a person looks at the IOC: the state of an endpoint after a responder showed another engine ID (M16) is on the alarm message of the records of that endpoint, in one line of the IOC log and in the shell report, and a request size that the module lowered for an address (M4) is only in the shell report. An operator screen or an alarm system has no PV for either.

Observed on 2026-11-16 with the product of commit `14bcb99` and the production `snmp3Ioc`: at 19:01Z with two fixture agents (the scenario `two` of the cell `recovery`, `work/matrix-m10-1/o-recovery/two/v3d/ioc.stdout`), and for `snmp3Report` at 18:58Z (the cell `r5-operator`, `work/matrix-m10-1/o-operator/operator.stdout`):

- `snmp3Report` prints one line, `snmp3: Base 7.0.10; capability=owned-worker-transport; recordSupport=available`.
- `snmp3RuntimeReport` prints `snmp3 runtime: state=Running ...` once and, per address, the lines `snmp3 queue:`, `snmp3 batch: address=N limit=0 splits=0`, `snmp3 peer: address=N state=normal`, `snmp3 admit:` and `snmp3 worker:`. A `limit` of 0 means that no rejection has lowered it, or that the worker of the address has ended or was restarted since and the limit is forgotten while `splits` stays; `docs/snmp-worker-reference.md` and `docs/snmp-record-behavior.md` state both, and the report of a stopped module in the grouped run of M4 shows `limit=0 splits=1`.
- Every line names an address by its number. No line says which endpoint of the configuration has that number, so with two units a reader cannot tell from the report which unit `address=2` is.
- When an endpoint changes its state, the time, the previous and the current engine ID are written once, in the `snmp3 engine:` line of the IOC log. The report shows the state and not since when.
- The module has no `Db` directory and installs nothing into `db/`. The code has no support for `SCAN=I/O Intr`.

##### Scope

- Two databases that the module installs into `db/` (D32):
  - `snmp3Runtime.db`, macro `P`: `$(P)RuntimeState`, an `mbbi` with the seven states that the report prints (`Cold`, `Starting`, `Running`, `Stopping`, `Stopped`, `IncompleteStopped`, `Failed`); `IncompleteStopped` and `Failed` carry MAJOR.
  - `snmp3Endpoint.db`, macros `P` and `ENDPOINT` (the `id` of an endpoint in the configuration): `$(P)PeerState`, an `mbbi` with `normal`, `changed` and `unverified`, the last two with MAJOR; an endpoint whose responder is still being asked to authenticate (the state `Pending` of the code) reads `unverified`, as the `snmp3 peer:` line shows it; `$(P)BatchLimit` and `$(P)BatchSplits`, `longin` records with the values of the `snmp3 batch:` line of the address of that endpoint.
  - Every record has `PINI=YES` and a periodic `SCAN` (macro `SCAN`, default `1 second`) and reads the state the IOC process holds; no request goes to a unit. The severities are set in the database by the state severity fields of `mbbi`.
- Device support with its own `DTYP`, `snmp3 state`, for `mbbi` and `longin`, in `dbd/snmp3.dbd`. Its link names what is read: the runtime state, or one of the three values of an endpoint by the endpoint `id`. A link that names an unknown endpoint fails the record at initialization.
- In `snmp3RuntimeReport`, one added line per endpoint: `snmp3 endpoint: id=<id> address=N state=<normal|changed|unverified>`, with ` since=<UTC time>` when the state is not `normal`: the time in the form and with the value of `time=` in the `snmp3 engine:` line that the IOC log holds for that change (for example `2026-11-16T19:01:00Z`). While the responder is still being asked to authenticate, the line shows `unverified` without `since`, because the log line and its time exist only from the moment the state is settled. The state is the one the record of that endpoint reads. The lines that exist keep their text.
- Documentation of the records, with the two meanings of 0 on `$(P)BatchLimit` (not lowered, or forgotten with the worker) and the delay of at most one scan period, of how an application loads them with `dbLoadRecords`, and of the added report line.

Out of scope: records for the worker, queue and admission lines of the report, an `info(archive, ...)` tag and a PVA group, which an application can add by itself; `SCAN=I/O Intr`, which belongs to M7; a process exit status that follows the module state; and any change of an application repository.

##### Completion Criteria

- With the two databases loaded by `dbLoadRecords` from `db/`, `$(P)RuntimeState` shows `Running` while the module runs and `Stopped` after `snmp3Stop`, and for each endpoint `$(P)PeerState` shows `normal`, then `changed` with MAJOR when the responder returns with another engine ID and answers an authenticated request, and `unverified` with MAJOR when it does not.
- `$(P)BatchLimit` and `$(P)BatchSplits` show 0 and 0 before a rejection and the lowered limit and the number of reductions after a unit rejected a request as too large, equal to the `snmp3 batch:` line of that address.
- `snmp3RuntimeReport` prints one `snmp3 endpoint:` line per endpoint with its `id`, its address number and its state, and, for an endpoint settled as `changed` or `unverified`, the time since when, equal to the time of the `snmp3 engine:` line of that change; the lines that existed keep their text.
- A record whose link names an unknown endpoint does not initialize, and the IOC says which record.
- The documentation states the records, their states and severities, the two meanings of 0 on `$(P)BatchLimit`, the delay of at most one scan period, the load lines and the added report line.
- On the two laboratory units, read only and with records that read each unit, the state records show `Running` and `normal` and the report names both endpoints; with a group of 61 values on each unit, which both reject, the two request size records of each endpoint show the lowered limit and the reduction.
- The cells of the ordinary matrix stage keep their name and status, and the added cell is PASS.

##### Dependencies And Decisions

- D30 sets the rule this unit serves.
- D32 fixes which values become records, how a record names an endpoint, how the records are kept current, and what the report gains.
- D33 accepts the differences between the plan and the work as run.
- The code of M16 is on `origin/master`; this unit reads the state it keeps and does not wait for its runs on the units. The state records are read on the two units in two read-only runs of this unit (T5), which need no cut of a link or of power.
- The tag of this unit is the next free number at its closing (D31).
- New tests are written in bash and Perl as a matrix cell (D19), and are limited to what the purpose needs.
- Differences between the accepted plan and the work as run (2026-11-16), accepted by D33:
  1. One more document was changed than item 6 names: `docs/snmp-rewrite-contract.md` gained one sentence under "Supported record types" that points at the state records, because its table reads as the whole record support of the module.
  2. Item 1 found that one IOC holds the three endpoints of T1 on one address with three ports, so T1, T2 and the three cases ran in one IOC and T3 in a second.
  3. The first run of the cell on the unchanged tree showed a fault of the cell, not of the product: its configuration did not mark the `Sys` bindings of B and C as system information, so the responder of B could not be asked to authenticate and B ended `unverified`. The check `applied-log-B` failed for it. The configuration was corrected and the unchanged tree run again; both logs are kept (`work/m18-evidence/unchanged-1.log`, `unchanged-2.log`).
  4. In the second run of T5 the state records were read after each of the three rounds, not only after the third.
  5. The link text of the device support is `runtime`, `peer=<id>`, `batchLimit=<id>` and `batchSplits=<id>`; it is stated in the source of the support and used only by the two installed databases.
- After the third-person passes of the change the owner had three items applied (2026-11-16): two include lines in `StateSupport.cpp`, after which the three product binaries have the hashes they had before, so the runs of T5 stand; the check `batch-shared-B` in the cell, for which the cell, the committed tree, the mutations and the matrix were run again; and the first paragraph and the Scope of `docs/snmp-record-behavior.md`, which now name every section of the page.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-16, accepted by the owner after eight third-person and three second-person passes of this detail
Implementation Authorization: 2026-11-16, by the owner: items 1 to 8 of the accepted plan, that is edits and test runs in this repository and the two read-only runs on the units; every commit, push, tag and issue action needs its own authority
Superseded Plan Artifacts: the draft of 2026-11-16 whose first item left open which values become records; D32 holds that decision

1. Write the check first: a matrix cell `module-state` in bash (`tests/rewrite/test_module_state.bash`, starts and stops the agents and the IOC) and Perl (`module_state.pl`, reads and judges), with a check design table in the header of the Perl file. It loads the two databases from `db/` of the tree it runs in and reads the records through the IOC shell. The cell `recovery` ran the two changed cases in separate IOCs and its two agents on `127.0.0.1` and `::1`; this cell first shows whether one IOC holds the three endpoints of T1 on one address with three ports, and runs the three cases in separate IOCs when it does not. Run it on the unchanged code and record the failing output.
2. Keep the time of the state change, the one the `snmp3 engine:` line prints, with the state of an endpoint (`snmp3App/src/Peer.h`, `Peer.cpp`) and offer the state of one endpoint, the batch values of its address and the runtime state to the device support.
3. Add the device support `snmp3 state` for `mbbi` and `longin` (a new source file in `snmp3App/src/`, its entries in the database definition file, `snmp3App/src/Makefile`, and the object list of `tests/rewrite/run_independence.py`).
4. Add `snmp3App/Db/` with its Makefile (`DB += snmp3Runtime.db snmp3Endpoint.db`), name it in `DIRS` of `snmp3App/Makefile`, and add the two databases, written to the database conventions of the site (no separator after a macro, a DESC of at most 40 characters on every record).
5. Add the `snmp3 endpoint:` line to `snmp3RuntimeReport` (`snmp3App/src/Runtime.cpp`).
6. Document the records, with the two meanings of 0 on `$(P)BatchLimit` and the delay of at most one scan period, and the load lines in `docs/snmp-record-behavior.md`, and the report line in `docs/snmp-worker-reference.md`.
7. Run the cell, the mutations and the ordinary matrix.
8. Run the two runs of T5 on the two units, GET only, with the addresses, names and secrets kept in the private directories under `work/` that T5 names and out of every recorded text.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Fixture, endpoint state | The real IOC with both databases loaded from `db/`, against fixture agents under SNMPv3: one agent that keeps its engine ID, one that returns with another engine ID and the same secrets, one that returns with another engine ID and other secrets, in one IOC or in three as item 1 of the plan finds; the records and `snmp3RuntimeReport` are read before and after the return | Linux host, ordinary product | `$(P)PeerState` is `normal` with NO_ALARM before; after the return `normal`, `changed` with MAJOR and `unverified` with MAJOR for the three endpoints; the `snmp3 endpoint:` lines carry the same states, the `id` and address number of each endpoint, and a `since` time only on the two that changed, equal to the `time` of the `snmp3 engine:` line of that endpoint in the IOC log; the `snmp3 peer:` lines keep their text |
| T2 | Fixture, request size and runtime state | The same IOC with, on the endpoint of the agent that keeps its engine ID, a group of eight values whose eighth is the object that the agent answers with tooBig (`.53864.4.11.0`), the group of the case `toobig-v3` of the cell `request-sizing`: `$(P)BatchLimit` and `$(P)BatchSplits` are read before and after the rejection and compared with the `snmp3 batch:` line; `$(P)RuntimeState` is read while the module runs and after `snmp3Stop` | Linux host, ordinary product | 0 and 0 before; after the rejections 1 and 3, the values the halving rule gives for eight values (8 to 4 to 2 to 1), and the report line carries the same; these are read before `snmp3Stop`; `Running`, then `Stopped` |
| T3 | Initialization | An IOC whose database names an endpoint `id` that the configuration does not hold | Linux host, ordinary product | The record does not initialize and the IOC output names it |
| T4 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` on the unchanged and the changed tree | Linux host | The cells of the unchanged tree keep their name and status, and the added cell is PASS |
| T5 | Units, read only | The production `snmp3Ioc` with both databases, the private configuration of the two laboratory units and records that read each unit, GET only. First run, 60 s, bindings without a group (the configuration `work/ioc-new/private/config.json` with the records of `work/ioc-new/apcpdu_snmp3/apcpduApp/Db/ap8932_min.template`): the read records, the state records and `snmp3RuntimeReport` are read at the end. Second run, 3 rounds, with 61 integer bindings of each unit in one group per unit, the grouped run of M4 that both units reject (configuration and `group.db` made by `work/m4-t3/t3_group.pl config`, as `work/m4-t3/run_group.sh` does, without its relays): the same is read after the third round and before the IOC is stopped | Both laboratory units | First run: the read records of both units carry values with NO_ALARM; `$(P)RuntimeState` is `Running`; for each of the two endpoints `$(P)PeerState` is `normal` with NO_ALARM and `$(P)BatchLimit` and `$(P)BatchSplits` are 0; the two `snmp3 endpoint:` lines carry the two `id`s, `normal` and no `since` time. Second run: for each of the two endpoints `$(P)BatchLimit` is 30 and `$(P)BatchSplits` is 1, the values M4 recorded for both addresses with that group on 2026-11-14, before the change of M16 was in the product; the `snmp3 batch:` lines carry the same; the 61 records fill without alarm in the third round |

The mutations are applied one at a time to scratch copies of the changed tree; the exact old and new text of each is written when the code exists:

- V1: the endpoint state support always reads `normal`; expected failing assertion: T1, `changed` and `unverified`.
- V2: the severity of `changed` is removed from `snmp3Endpoint.db`; expected failing assertion: T1, MAJOR on `changed`.
- V3: the batch support reads 0; expected failing assertion: T2, the limit and the reductions after the rejection.
- V4: the runtime state support reads a constant; expected failing assertion: T2, `Stopped`.
- V5: the `since` time is left out of the report line; expected failing assertion: T1, the `since` time.
- V6: an unknown endpoint is accepted at initialization; expected failing assertion: T3.

Left out, with the reason:

- A change of engine ID on the laboratory units: a unit cannot be made to change its engine ID on request, and the fixture shows it (T1). A rejected request can be provoked on a unit, as M4 did, and T5 does it.
- The states `Cold`, `Starting`, `Stopping`, `IncompleteStopped` and `Failed` of the runtime: the record reads the same value as the report, and T2 shows that it follows a change.
- The delay of a periodic scan: a state is visible at most one scan period late, which the documentation states; `SCAN=I/O Intr` is M7.
- SNMPv1 and SNMPv2c: they carry no engine ID, so the endpoint state stays `normal` (M16).

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-17T00:03Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The cell `module-state` (`tests/rewrite/test_module_state.bash` with `module_state.pl`), one IOC with the endpoints A, B and C on one address and three ports: `$(P)PeerState` is `normal` with NO_ALARM for the three before; after the agents of B and C returned with another engine ID, A is `normal` with NO_ALARM, B is `changed` with MAJOR and C, whose agent has other secrets, is `unverified` with MAJOR. The three `snmp3 endpoint:` lines read `id=<id> address=1 state=normal` before; after the return the line of A is unchanged and the lines of B and C carry their state and `since=2026-11-17T00:03:45Z`, the `time` of the `snmp3 engine:` line of each in the IOC log. The `snmp3 peer:` line reads `address=1 state=normal` before and `state=unverified` after. The returned agents report the engine IDs they were given, and the log holds one line per endpoint, `changed` authenticated 1 for B and `unverified` authenticated 0 for C. The cell passes its 28 checks. On a copy of the committed tree with the cell added, the 18 checks of the new behavior fail and the 10 checks of the fixture and of the lines that existed pass. Mutation V1 fails `peer-after-B` and `peer-after-C`, V2 fails `peer-after-B` with `changed NO_ALARM`, V5 fails `endpoint-after-B` and `endpoint-after-C`; each mutation fails no other check. Evidence: `work/m18-evidence/unchanged-3.log`, `changed-2.log`, `mutation-table.json`, `mutation-2/`, `sources.sha256`. An earlier series of the cell with 27 checks, before the check `batch-shared-B` was added, is kept beside it (`unchanged-2.log`, `changed-1.log`, `mutation/`, `sources-first-series.sha256`) |
| T2 | 2026-11-17T00:03Z | Same as T1 | Pass | The same run, with the group of eight on endpoint A: `$(P)BatchLimit` and `$(P)BatchSplits` read 0 and 0 before and 1 and 3 after four passes of the group, and the two records of endpoint B, which shares the address, read 1 and 3 as well; the eighth record carries `tooBig`, and the report prints `snmp3 batch: address=1 limit=1 splits=3`. `$(P)RuntimeState` reads `Running` before and `Stopped` after `snmp3Stop`, where the report prints `state=Stopped`. Mutation V3 fails `batch-after` and `batch-shared-B` with `0 0`, and V4 fails `runtime-stopped` with `Running`. Evidence: as T1 |
| T3 | 2026-11-17T00:03Z | Same as T1 | Pass | The same run, second IOC, with `snmp3Endpoint.db` loaded for an endpoint the configuration does not hold: the IOC completes its initialization and prints `snmp3: record initialization rejected:` for `X_PeerState`, `X_BatchLimit` and `X_BatchSplits`. Mutation V6 fails the three checks. Evidence: as T1 |
| T4 | 2026-11-17T00:07Z to 2026-11-17T00:20Z | Same as T1 | Pass | `run_matrix.py --stages environment,ordinary` on the changed tree: 45 PASS and `tsan` NOT APPLICABLE; the 45 cells of the M10 run on the same Base (`work/matrix-m10-1/cells.tsv`) keep name and status, and the added cell `module-state` is PASS with 28 checks. Evidence: `work/matrix-m18-2/cells.tsv`; the run before the check was added is `work/matrix-m18-1/` |
| T5 | 2026-11-16T23:04Z to 2026-11-16T23:06Z | Both laboratory units, read only (GET requests; no write); production `snmp3Ioc`, worker and `libsnmp3.so.1` of the changed tree, whose hashes in `sources-first-series.sha256` and `sources.sha256` are equal | Pass | First run, 60 s, bindings without a group: the 12 read records of the two units hold a value with NO_ALARM; `$(P)RuntimeState` is `Running`; both endpoints read `normal` with NO_ALARM and 0 and 0; the report prints `snmp3 endpoint: id=<id> address=N state=normal` for the two endpoints, with the address numbers 1 and 2 and no `since`. Second run, 3 rounds, 61 integer bindings of each unit in one group per unit, no relay: both units answer the first request with `tooBig` (61 records each INVALID with that message); after each of the three rounds both endpoints read `$(P)BatchLimit` 30 and `$(P)BatchSplits` 1 and the report prints `limit=30 splits=1` for both addresses; in rounds 2 and 3 the 61 records of each unit hold a value with NO_ALARM. The values are those M4 recorded before the change of M16. Evidence: `work/m18-units/private/first-1/`, `group-1/`, with the scripts `work/m18-units/run_first.sh` and `run_group.sh` |

##### Closure Evidence

- Completion Criteria checked on 2026-11-16 against the Verification Results: the runtime state `Running` and `Stopped` and the three endpoint states with their severities (T1, T2); the request size records before and after the rejections, on the endpoint of the group and on one that shares its address (T2); the `snmp3 endpoint:` lines with `id`, address number, state and the time of the change, and the unchanged text of the lines that existed (T1, T4); the rejection of a record that names an unknown endpoint (T3); the two read-only runs on the laboratory units (T5); the ordinary stage with the same 45 cells and the added cell (T4); the records, their states and severities, the two meanings of 0, the delay of one scan period, the load lines and the report line in `docs/snmp-record-behavior.md` and `docs/snmp-worker-reference.md`. The differences from the plan are decided by D33.
- Review: the change had four third-person passes and two second-person passes before its commit; the items they raised were applied, and the last pass of each kind returned no finding.
- Landing: the change is commit `bbe30e2`, the register record of the plan, decisions and results is commit `6db2a13`, and the `CHANGELOG.md` entry `0.0.6` is commit `bbe462269f7f7d2ac75b9ceee4478ad86c5c104f`; all three were pushed to `origin/master`. After `git fetch`, `git rev-parse HEAD origin/master` printed `bbe4622` twice and `git diff --name-only cdae1e3 origin/master` listed the 19 paths of the three commits (observed 2026-11-17T02:18Z).
- Tag: the annotated tag `0.0.6` (object `1e0088d288879cb7b96809b4eeeb872352e3f817`) names `bbe4622`, the commit that holds the `CHANGELOG.md` entry, created and pushed under the owner's tag authority of 2026-11-16. Recheck with `git ls-remote --tags origin refs/tags/0.0.6 'refs/tags/0.0.6^{}'`, which printed that object and the peeled commit at 2026-11-17T02:18Z. The tagged tree also holds the code of M16, which is not closed; the `CHANGELOG.md` entry `0.0.4` names it as included and not yet verified on a unit.
- This unit has no linked issue.
- The `CHANGELOG.md` entry was committed without a second-person pass, by the owner's decision of 2026-11-16.
- Unproven and left listed: the runtime states other than `Running` and `Stopped` on the record; what the record and the report line show while a responder is still being asked to authenticate; a change of engine ID on a laboratory unit; and the measured delay of the periodic scan.

#### M16 - Recovery when a unit returns after a power or network loss

Origin: 1.0.0 / M16
Identity History: none
GitHub Issue: none
Status: In progress

##### Summary

An IOC stays up for months while a unit can lose its power or its network link. For the IOC the two look the same: requests time out. When the unit answers again, the IOC must start to communicate again by itself, with no restart of the IOC and no operator action. The owner met the opposite on another device support: the device lost power, came back with its network, and the IOC kept waiting. SNMP runs over UDP and has no connection to restore; SNMPv1 and SNMPv2c keep no state per unit, while an SNMPv3 client remembers the engine ID, the engine boots and the engine time of the unit.

Observed on 2026-11-15 (UTC) with the unchanged product, the production `snmp3Ioc` and worker, and the fixture agent stopped and started again while four IOCs kept running (SNMPv1, SNMPv2c, SNMPv3 with engine discovery, SNMPv3 with the engine ID set); 16 request rounds in about 50 s after the return (`work/tmp/m16-observe/`; `RUNS.txt` there names the runs that count and the ones discarded):

| The agent returns as | SNMPv1 | SNMPv2c | SNMPv3, both profiles |
| --- | --- | --- | --- |
| Paused 20 s and continued, so its engine ran on (stands for a link cut) | recovers | recovers | recovers |
| Started for the first time 20 s after the IOC | recovers | recovers | recovers |
| Restarted with higher engine boots | recovers | recovers | recovers |
| Restarted with the same boots after the IOC ran 20 s | recovers | recovers | recovers |
| Restarted with the same boots after the IOC ran 200 s | recovers | recovers | stays INVALID with `no reply` |
| Restarted with another engine ID (IOC ran 20 s; also with higher boots after 200 s) | recovers | recovers | stays INVALID with `no reply` |

Recovers means normal from the first round after the return. In the two cases that do not recover, all 16 rounds fail with the message `no reply` and the worker is not restarted. What the agent sent back in these rounds was not recorded. When the IOC starts while no agent runs, the records of the SNMPv3 profile with engine discovery are INVALID with an empty message until the agent starts; after an agent that had answered is stopped, they carry `no reply` like the others. The fixture agent stands for a unit here only as a server that disappears and returns; what the two laboratory units do to their engine ID and boots over a power loss has not been observed. Read from both units on 2026-11-15T10:42Z, read only: the engine ID of each is 11 bytes in the format of RFC 3411 with enterprise number 318 and format octet 3, which is a MAC address of 6 bytes, so an unchanged engine ID means the same network card; the engine boots are 125 on the NMC2 unit and 16 on the NMC3 unit, and the engine time equals `sysUpTime` in seconds on both (212995 s and 106229 s). The values themselves are kept in `work/m16-evidence/private/`. In the baseline run of snmp3 `0.0.1` (M2 / T8) the network link of each unit was cut for 300 s and restored, and the unit was answered again.

##### Scope

- Full recovery by itself when the unit returns with its engine ID unchanged, including engine time that went back with the same boots: an SNMPv3 endpoint asks the address for its engine ID again after 2 consecutive failed requests, with a least interval that doubles from 0.02 s to 15 s like the Channel Access beacon (D24), and synchronizes again when the engine ID is the same.
- A state per SNMPv3 endpoint, entered when the responder shows another engine ID than the one first used and kept until the IOC is restarted (D23, D24):
  - changed: the responder answers an authenticated request under the profile's passphrase with its new engine ID;
  - unverified: that request fails, or the profile cannot authenticate another engine ID (localized keys only, no authentication, or the engine ID named in the configuration).
- In either state no SET is sent and no value read from the unit is put into a record: those records are COMM/INVALID with the message of the state and keep the value they had. In the changed state the bindings marked as system information are read and shown, also with COMM/INVALID and the message.
- A binding field that marks system information (read only), for objects such as firmware revision, name, MAC address and status.
- Messages: `no reply` whenever a unit does not answer, also for an SNMPv3 profile with engine discovery whose unit has not answered since the IOC started, where the message is empty today; `engine ID changed: restart IOC`; `engine ID unverified: restart IOC`.
- One log line when a state is entered, with the UTC time, the endpoint, the earlier and the new engine ID and whether the request was authenticated, and a line per address in `snmp3RuntimeReport` with the state.
- Documentation of the states, the messages, the field, the log and report lines, and the limits: SNMPv1 and SNMPv2c carry no engine ID, so a replaced unit cannot be told apart there.

Out of scope: a restart of the IOC itself, the application repository, performance criteria (M13), receiving traps, a probe for endpoints that no record reads periodically, reading the MAC address from the host's neighbor table, a record of a recovery with the engine ID unchanged, a message of its own for the seconds between the first failed request and the outcome of the new question, and the change of the configuration to localized keys (M9).

##### Completion Criteria

- Recovery: in the fixture the records are normal again within 30 s after the agent returns as paused and continued, started after the IOC, restarted with higher boots, and restarted with the same boots after 200 s of IOC run time; while the agent is away the message is `no reply`, also before its first start.
- Changed engine ID: within 30 s the endpoint is in the changed state with the same secrets and in the unverified state with other secrets; no SET reaches the agent, no withheld record takes a value of the returned agent, the system information records show the returned agent's values with COMM/INVALID in the changed state, every record carries the message, the log holds exactly one line for the event and the report shows the state; the state is still held after further rounds.
- Another address is unaffected: its records stay normal and its SET is still sent while the first address is in a changed state.
- On the laboratory units: after a link cut and after a power loss the records return to normal by themselves if the engine ID is unchanged; the engine ID, the boots and the time without power are recorded.
- The checks were seen failing on the unchanged code or on a mutation of the table in the Test Plan, for the intended assertion, and the cells of the ordinary matrix stage keep their name and status.

##### Dependencies And Decisions

- G1 complete (2026-11-13).
- D18 places this unit in release 1.0.0 and sets the observation as its first step. The observation with the fixture agent is done (Summary); the observation on the units remains.
- D22 runs this unit next, after M4, and its row follows M4, so the row order stays the execution order while the ID does not follow it. D22 gave it the tag `0.0.4`; D27 changes it to `0.0.5`, because M5 closes first. D31 gives `0.0.5` to M10, which closes first, and this unit the next free number at its closing.
- D23 is the policy this detail implements, and D24 fills the rules it left open and limits the tests to what the purpose needs.
- D25 accepts the differences listed below.
- M9 changes the configuration to localized keys. This unit defines the behavior for that form (unverified after a change); its check runs when M9 provides the form.
- The power loss of a unit is an action on equipment: each run needs the owner's permission, and the owner or an operator the owner names performs the cut; the owner plans it for the week after 2026-11-14. Outlet 8 of PDU1 is the only output that may be written (D9); what it powers is not recorded here.
- New tests are written in bash and Perl as a matrix cell (D19).
- Preparation of T4 and T5, observed 2026-11-15T22:01Z to 22:04Z, read only: the measurement IOC rebuilt against the module of commit `dfbf10e` ran 180 s against both units. From the fourth sample (20 s) on all 164 read records of each unit are without alarm, both addresses show `snmp3 peer:` `normal` and a batch limit of 0, the IOC log holds no `snmp3 engine:` line, and the engine ID digest and the boots of both units (125 and 16) are the same before and after. `work/m16-units/run_units.sh LABEL SECONDS` starts such a run, reads engine ID digest and boots before and after, samples the report and the record states every 5 s with their UTC time, and writes `summary.txt`; it ends early when a file `stop` appears in its run directory. Evidence: `work/m16-units/runs/normal-1/`.
- Differences between the accepted plan and the work as run, recorded 2026-11-15 and accepted by D25; the plan text is unchanged:
  1. The new question for the engine ID is a new start of the session: after 2 consecutive requests without a reply the worker closes the SNMPv3 session of the endpoint and drops the remembered engine time, and the next request opens a session, which asks for the engine ID where the profile discovers it. A profile that names its engine ID is asked by a separate question at that moment.
  2. The state is decided in two steps. The worker answers a request of an endpoint with another engine ID unsent, with native outcome 13 and both engine IDs; the IOC then lets only the system information bindings through, and the first of them that is answered under authentication gives `changed`, a security failure gives `unverified`. Until then the endpoint is shown as `unverified` and nothing is logged. An endpoint without a system information binding cannot be asked and is `unverified` at once; D24 (7) did not name this case.
  3. A unit that does not answer the question for its engine ID now ends as the native outcome Timeout, where it was OpenFailure, so that its records carry `no reply`.
  4. The SET and the values are held back in more than one place (the start of a request in the IOC, the worker, and the handling of a result), so no single change sends a SET or takes a value. V3 changes the IOC and the worker together, V4 the IOC in two places and the worker. The predictions that did not hold were corrected after the run and the mechanism checked in the output: under V3 the output record still alarms because the result is withheld, and the mutant is seen by the SET in the agent's log and the value 9 that it wrote; under V4 the state also arrives late or never, because the unsent answer is the IOC's only notice; under V8 the state is entered again at every reading, which gives 59 log lines; under V11 the empty message also shows after a session start again.
  5. The worker and the IOC must come from the same build: the binding of the worker bootstrap carries the system information mark, and a result may carry the native outcome 13 with a note of at most 66 bytes.
  6. `tests/rewrite/run_independence.py` holds the list of the objects of the IOC and was given the new `Peer.o`.
  7. The fixture controls are two environment variables of `snmp3NativeAgent`, `SNMP3_FIXTURE_ENGINE_BOOTS` and `SNMP3_FIXTURE_INTEGER`.

##### Implementation Plan

Plan Status: accepted
Plan Acceptance: 2026-11-15, accepted by the owner after the third-person passes of this detail were closed
Implementation Authorization: 2026-11-15, by the owner: items 1 to 7 of the accepted plan, that is edits and test runs in this repository and read-only runs against the two units; the power loss of a unit in item 7 needs the owner's permission for each run and is performed by the owner or an operator the owner names; every commit, push, tag and issue action needs its own authority
Superseded Plan Artifacts: the draft of 2026-11-14 that began with an observation on the units, and the first rewrite of the same day, which D24 narrows

1. Fixture: let `snmp3NativeAgent` start with a given engine boots value, with its own time list entry set to the same value (an override after initialization that leaves that entry behind gives an inconsistent agent, observed 2026-11-15), and with a given start value of its objects so that a returned agent holds other values than before. The engine ID argument exists.
2. Write the checks first, from one table of checks kept with the cell (claim, input, state read, source of the expected value): a matrix cell in bash and Perl for T1 to T3, with waits on completion signals and declared bounds as in the request sizing cell, and with the agent's ready line (engine ID, boots) and the measured run time as the evidence that each case was applied. Run it on the unchanged code and record the failing output.
3. Worker: the new question for the engine ID by the rule of D24, the new synchronization for the same engine ID, the authenticated request for another engine ID where the profile allows it, and the report of the outcome to the IOC.
4. IOC: the state per endpoint until the IOC stops, no SET and no value in a state, the system information bindings in the changed state, the alarms and messages of D24, the log line and the report line, and `no reply` in place of the empty message.
5. Configuration: the binding field for system information, with its loader checks.
6. Documentation.
7. Run the cell and the mutations, then the units: link cut and restore, and a power loss and return by the owner, with engine ID and boots read before and after.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Fixture, engine ID unchanged | The real IOC and worker against the fixture agent. SNMPv3 with engine discovery and SNMPv3 with the engine ID set: paused 20 s and continued; started 20 s after the IOC; restarted with higher boots after 200 s of measured IOC run time; restarted with the same boots after 200 s. SNMPv2c: paused and continued; started after the IOC; restarted once | Linux host, ordinary product | `no reply` while the agent is away, also before its first start; records normal within 30 s of the return in every case |
| T2 | Fixture, changed engine ID | The agent restarted with another engine ID and other object values, once with the same secrets and once with other secrets, under SNMPv3 with engine discovery; input records, one output record written before and after the change, and system information records; rounds continue for 30 s after the state is seen | Linux host, ordinary product | Changed state with the same secrets, unverified with other secrets, within 30 s; the SET before the change is seen in the agent's log and the SET after it is not; withheld records keep their earlier values with COMM/INVALID and the message; system information records show the new values with COMM/INVALID in the changed state; exactly one log line; the report line; the state still held at the end |
| T3 | Fixture, two addresses | Two agents; the first restarted with another engine ID, the second left running | Linux host, ordinary product | The records of the second address stay normal and its SET is seen in its agent's log while the first address is in the changed state |
| T4 | Device, link | Cut and restore the network link of one unit while the IOC runs | Measurement IOC, both units | The unit's records return to normal by themselves; the other unit stays normal |
| T5 | Device, power | One unit loses power and returns while the IOC runs; engine ID and boots read before and after, and the time without power recorded | Measurement IOC, both units | With the engine ID unchanged the records return to normal by themselves; the other unit stays normal. If the engine ID is unchanged, the changed state is not exercised on a unit and stays a fixture result |
| T6 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` on the unchanged and the changed tree | Linux host | The cells of the unchanged tree keep their name and status, and the added cell is PASS |

The pause of the fixture agent stands for a link cut with one difference: requests sent during the pause are answered when the agent continues, and the client drops them as late. The cut of a real link is T4.

The mutations are applied one at a time to scratch copies of the changed tree; the exact old and new text of each is written when the code exists, and the failing assertion of each is fixed before the series:

- V1: the engine ID is not asked again after failed requests; expected failing assertion: T1, same boots after 200 s.
- V2: another engine ID is accepted as normal operation; expected failing assertion: T2 state and message.
- V3: a SET is sent in a state; expected failing assertion: T2, SET not seen after the change.
- V4: read values are put into records in a state; expected failing assertion: T2, withheld records keep their earlier values.
- V5: the system information bindings are withheld in the changed state; expected failing assertion: T2, new values shown.
- V6: the system information records carry no alarm; expected failing assertion: T2, COMM/INVALID on the system information records.
- V7: a failed authentication is taken as authenticated; expected failing assertion: T2 with other secrets.
- V8: the state ends when requests succeed again; expected failing assertion: T2, state still held at the end.
- V9: the log line is written per failed request, or not at all; expected failing assertion: T2, exactly one log line.
- V10: the state of one address is applied to every address; expected failing assertion: T3.
- V11: the empty message stays for a unit that has not answered since the IOC started, under engine discovery; expected failing assertion: T1, `no reply` before the first start of the agent.

Left out, with the reason (D24):

- SNMPv1 in the cell: it shares the stateless path of SNMPv2c, and the observation of 2026-11-15 in the Summary ran it in every case.
- A restart with the same boots after 20 s: the 200 s case contains it, and the unchanged product already passes it.
- Normal operation after a restart of the IOC: the state lives in the memory of the IOC process, so a new process cannot hold it.
- The earlier engine ID answering again during a state: the state is latched by one rule, which T2 checks by the rounds after the state is seen (V8).
- A profile with no authentication, and one with the engine ID named in the configuration: they take the same branch as a failed authentication, which T2 checks with other secrets.
- The request count of the other address: the isolation of two addresses was measured on the units in M2, and T3 checks what this change could break, the state reaching another address.

Not covered and listed as unproven until its condition holds: a profile with localized keys only (M9), and a unit whose return differs from the cases above.

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | 2026-11-15T10:01Z to 2026-11-15T10:14Z | Debian 13 host, ordinary product built against Base 7.0.10 of the public 1.5.0 tree | Pass | The matrix cell `recovery` (`tests/rewrite/test_recovery.bash` with `recovery.pl`), scenarios `pause`, `late`, `boots` and `same`: 48 checks pass. SNMPv2c, SNMPv3 with engine discovery and SNMPv3 with the engine ID set carry `no reply` while the agent is away and are normal within 30 s of its return in every scenario; the run time before the loss is at least 200 s in `boots` and `same`, and the returned agent reports boots 2 and 1. On the product before the change (2026-11-15T09:25Z) the same cell fails `same` for both SNMPv3 profiles (never normal, `no reply` to the end) and `late` for engine discovery (empty message). Mutation V1 fails `same`, and V11 fails the `no reply` check of engine discovery in `late` and, after a session start again, in `pause`, `boots` and `same`. Evidence: `work/matrix-m16-2/o-recovery/`, `work/m16-evidence/unchanged-1.log`, `work/tmp/m16-mutation/results/`, `work/m16-evidence/mutation-table.json`, `mutation-combined.json`, `sources.sha256` |
| T2 | Same run as T1 | Same as T1 | Pass | Scenarios `changed` and `unverified`: 22 checks pass. Within 30 s of the return of an agent with another engine ID the report shows `changed` with the same secrets and `unverified` with other secrets, and holds it for the 30 s of rounds that follow; the SET of 5 before the loss is in the first agent's log and the SET of 9 in the state is not in the returned agent's; the two input records keep 5 with COMM/INVALID and the message of the state; the system information record shows 77 in `changed` and keeps 5 in `unverified`, with COMM/INVALID and the message; the output record is INVALID with the message; the IOC log holds exactly one `snmp3 engine:` line with both engine IDs and `authenticated` 1 or 0. On the product before the change 14 of these checks fail. Mutations V2, V5, V6, V7, V8 and V9 fail the checks named for them; V3 and V4 are combined mutants (difference 4). Same evidence as T1 |
| T3 | Same run as T1 | Same as T1 | Pass | Scenario `two`: 5 checks pass. The first address, on 127.0.0.1, is `changed` within 30 s and the second, on ::1 with its own agent and engine ID, stays normal in every sample; its SET of 6 during the state adds a commit to its agent's log; the report shows one `changed` and one `normal`. On the product before the change 2 checks fail (no state). Mutation V10 fails the two checks of the second address. Same evidence as T1 |
| T4 | Not run | Measurement IOC, both units | Pending | none |
| T5 | Not run | Measurement IOC, both units | Pending | none |
| T6 | 2026-11-15T10:01Z to 2026-11-15T10:14Z | Same as T1 | Pass | `run_matrix.py --stages environment,ordinary` on the changed tree: 42 PASS and `tsan` NOT APPLICABLE; the 42 cells of the M4 run on the same Base (`work/matrix-m4-changed-2/cells.tsv`) keep name and status, and the added cell `recovery` is PASS with 75 checks. A first run had `config` and `lifecycle` FAIL on the object list of difference 6 (`work/matrix-m16-1/`). Evidence: `work/matrix-m16-2/cells.tsv` |

##### Closure Evidence

- none

#### M19 - MIB tools: database check and database draft

Origin: 1.0.0 / M19
Identity History: none
GitHub Issue: none
Status: Not started

##### Summary

A MIB file states, for every object, its name, its type, whether it can be read or written, its description, and for an enumerated integer the meaning of each value. An EPICS database for an SNMP unit repeats all of that by hand: the record type, the direction, the link, the description, the states. The owner's experience with SNMP IOC databases is that this copying is where the time and the mistakes are, and that a mistake shows only when the IOC runs against the unit. D35 makes two standalone programs: one that checks a database against MIB files before an IOC is started, and one that writes a draft of records from MIB files, which the author then finishes. M7 already takes the type of an object given by name from the MIB and checks the record at initialization; this unit moves the same knowledge to the time the database is written.

##### Scope

- A checker, the main program: from a database file and MIB files, it reports for every record of the module whether the name resolves, whether the record type can hold the declared type, and whether the direction agrees with the access; it ends with a status that tells a clean database from one with findings, so that it can run before an IOC starts and in a pipeline.
- A draft program: from MIB files and a choice of objects or tables, it writes a draft of database records with the link form of M7 by name, the record type and direction from the declared type and access, the description from the MIB cut to the DESC length, the units where the MIB gives them, and the states of an enumerated integer. The draft is for the author to finish: PV names, the choice of what to keep and everything the MIB does not state stay with the author, and the finished database goes through the checker.
- Both resolve names the way the IOC does, through the MIB function and the search order of M7, so that a database the checker passes is one the IOC accepts.
- Documentation of both programs.

Out of scope: the IOC runtime; any change to the device support; a graphical tool; the naming of PVs beyond what the caller gives.

##### Completion Criteria

- For a database with seeded faults (a name the MIB does not hold, a record type that cannot hold the declared type, an output record on a read-only object, a missing MIB), the checker names each fault and its record and ends with the status of a database with findings; for a clean database it reports none.
- For the fixture MIB and for the PowerNet MIB, the draft program writes records for chosen objects with the record type, direction, description and states the MIB declares; the draft passes the checker, and an IOC of M7 loads it and reads the fixture agent.
- A database the checker passes initializes in the IOC without a rejected record, and a record the checker names is one the IOC rejects.
- The documentation states how each program is run and what its findings mean.

##### Dependencies And Decisions

- D35 creates this unit.
- M7 gives the link form and the MIB resolution these programs use; the unit starts after M7 is Complete.
- Open for the owner's decision: whether release `1.0.0` waits for this unit (it is not among the dependencies of M11 until the owner says so); the language of the two programs (Perl, as the owner named, with the helper of the module for the MIB reading, is proposed); and what the draft holds for a read-write object (an input and an output record, proposed).

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. Observe first, after M7: what the MIB function of the module returns for an object (type, access, description, units, enumeration) on the fixture MIB and the PowerNet MIB, and which of it a record needs.
2. Write the checks first, as a matrix cell in bash and Perl (D19).
3. Write the checker, then the draft program, and the documentation.
4. Run the cell, the mutations and the ordinary matrix.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Checker | The checker on a database with the seeded faults and on a clean one, with the fixture MIB | Linux host | Each fault named with its record; the status of findings for the first and of a clean database for the second |
| T2 | Draft | The draft program on the fixture MIB and on the PowerNet MIB for chosen objects; the draft given to the checker and loaded in the production IOC against the fixture agent | Linux host, ordinary product | Records with the declared type, direction, description and states; the checker reports none; no record rejected; the fixture values read |
| T3 | Agreement | The seeded database loaded in the production IOC | Linux host, ordinary product | The IOC rejects exactly the records the checker named |
| T4 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` on the unchanged and the changed tree | Linux host | The cells of the unchanged tree keep their name and status, and the added cell is PASS |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host | Pending | none |
| T2 | Not run | Linux host, ordinary product | Pending | none |
| T3 | Not run | Linux host, ordinary product | Pending | none |
| T4 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

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
| latest changelog entry | `CHANGELOG.md` | the number of the last closed unit | `1.0.0` | read the first entry heading | Release Verification 8 | read the first entry heading | Release Verification 9 |

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
| Release Verification 8 | Version | pre-change | read `CHANGELOG.md` first heading | repository | the number of the last closed unit | command output |
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
| Tools | M17 | Matrix cells moved from Python to bash and Perl | Milestone | Deferred | No | D19 | Each moved cell kills the same mutations as the Python cell it replaces, and the matrix gives the same cell names and statuses; [detail](#m17---matrix-cells-moved-from-python-to-bash-and-perl) |
| Records | M14 | Deferred record verification cells | Carry-forward | Deferred | No | D6 | Each listed cell has a real-path receipt or a dated retirement; [detail](#m14---deferred-record-verification-cells) |
| Records | M6 | Reply time stamp on records | Milestone | Deferred | No | D28 | Record TIME equals the worker receive time with `TSE=-2`, when an exactly equal time stamp for the values of one reply is asked for; [detail](#m6---reply-time-stamp-on-records) |

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

#### M17 - Matrix cells moved from Python to bash and Perl

Origin: 1.0.0 / M17
Identity History: none
GitHub Issue: none
Status: Deferred

##### Summary

The cells of `tests/rewrite/run_matrix.py` are driven by Python harnesses (25 harness files of 10,442 lines and 5 helper files of 1,134 lines, counted on 2026-11-14). The owner writes new tests in bash and Perl (D19). This unit moves the existing cells to the same form, one cell at a time, without losing what each cell proves.

##### Scope

- Move each Python cell to bash, which starts and stops the processes, and Perl, which reads the records and judges.
- For each moved cell, run the old and the new cell on the same inputs and the same mutation table and compare the mutations each kills.
- Decide whether the matrix runner itself and the fault proxy move as well.

Out of scope: the C++ test bodies, product changes, and new checks.

##### Completion Criteria

- A mutation killed by the Python cell and surviving in the moved cell is resolved before the move of that cell is accepted; a mutation surviving in both is recorded and not fixed inside the move.
- The matrix gives the same cell names and statuses before and after the move.

##### Dependencies And Decisions

- D19 places this work in Backlog. Transition date: 2026-11-14. It starts by the owner's decision, which also names the order of the cells.

##### Implementation Plan

Plan Status: draft
Plan Acceptance: none
Implementation Authorization: none
Superseded Plan Artifacts: none

1. List the cells with their mutation tables, or write the table of a cell that has none, before its move.
2. Move one cell, compare the killed sets, and repeat.

##### Test Plan

| Label | Layer | Method | Environment | Expected Result |
| --- | --- | --- | --- | --- |
| T1 | Conversion | The old and the new cell on the same inputs and the same mutation table | Linux host, ordinary product | The same mutations are killed |
| T2 | Regression | `tests/rewrite/run_matrix.py --stages environment,ordinary` before and after | Linux host | The same cell names and statuses |

##### Verification Results

| Label | Observed At | Environment | Result | Evidence |
| --- | --- | --- | --- | --- |
| T1 | Not run | Linux host, ordinary product | Pending | none |
| T2 | Not run | Linux host | Pending | none |

##### Closure Evidence

- none

#### M6 - Reply time stamp on records

Origin: 1.0.0 / M5
Identity History: M5 -> M6 (baseline row inserted before it, 2026-11-10)
GitHub Issue: #6 - https://github.com/jeonghanlee/snmp3/issues/6
Status: Deferred

##### Summary

Problems 4 and 5, cause C. A record's TIME is its processing time, so a client cannot tell when the value was read from the device. Base 7.0.10 lets device support set the time stamp when `TSE=-2`. This unit carries the worker's receive time of each reply through the IPC result and writes it into the record when `TSE=-2`; `TSE=0` records keep Base behavior.

Deferred by D28 on 2026-11-16 after the delay was measured on the product of commit `37008cb`, with `TSE=0`: the time from the arrival of a reply at the IOC host to the TIME of the record that takes its value, the TIME read through a Channel Access monitor.

| Where | Case | Updates | Min | p50 | p95 | Max |
| --- | --- | --- | --- | --- | --- | --- |
| Fixture agent, 40 s | Record with its own request | 41 | 31 us | 132 us | 397 us | 441 us |
| Fixture agent, 40 s | Eight records of one group, all | 328 | 78 us | 257 us | 586 us | 685 us |
| Fixture agent, 40 s | First and last record of the group | 41 each | 78 and 87 us | 238 and 271 us | 504 and 599 us | 590 and 685 us |
| NMC2 unit, 60 s | Record with its own request | 61 | 112 us | 227 us | 398 us | 556 us |
| NMC2 unit, 60 s | Eight records of one group, all | 488 | 159 us | 334 us | 627 us | 773 us |
| NMC3 unit, 60 s | Record with its own request | 61 | 40 us | 121 us | 269 us | 495 us |
| NMC3 unit, 60 s | Eight records of one group, all | 488 | 92 us | 209 us | 468 us | 588 us |

How it was measured, for a later look: on the fixture the arrival is the instant the fault proxy in `pass` mode logs the reply before it forwards it, converted from the monotonic to the wall clock with an offset read before and after the run (drift 0 us); on the units (2026-11-16T16:06Z to 16:07Z, read only) it is the wall time a loopback relay logs right after it forwarded the reply, so the true delay is longer than the figure by the time of that forward. Each record is paired with the latest reply of its kind before its TIME; the one-value and the eight-value reply differ in length (140 and 299 bytes on the units, 63 each). Both runs use a small IOC of 9 and 18 records processed once a second; the delay under a burst of some hundred records was not measured. The scripts, the monitor output and the relay and proxy logs are in `work/m6-evidence/` (`observe.sh` and `analyze.pl` with `run-1/` for the fixture, `units.sh` and `analyze_units.pl` with `units-1/` for the units); that directory is local and untracked, and its `private/` part holds the unit addresses.

##### Scope

- Receive time (monotonic and wall clock) in the native result and the IPC result frame.
- Device support writes `prec->time` from the wall-clock receive time when `TSE=-2`.
- Documentation of the field in `snmp-record-behavior.md` and `snmp-native-reference.md`.

Out of scope: `I/O Intr` (M7), UTAG, and time synchronization of the IOC host.

##### Completion Criteria

- With `TSE=-2`, record TIME equals the worker receive time recorded in the result within the IPC transfer latency, for every input record type.
- With `TSE=0`, TIME is unchanged from `0.0.1` behavior.

##### Dependencies And Decisions

- D28 defers this unit and moves it to the Backlog. Its issue #6 stays open: under the owner's Issue authority of 2026-11-16 its body was given a section on the current state with the deferral and the measurements, and the issue was taken out of the GitHub milestone `1.0.0`.

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
GitHub Milestone: none
Observed State: open
Observed Labels: enhancement
Observed Milestone: none
Last Compared: 2026-11-16T17:12Z; issue updatedAt 2026-11-16T17:12:08Z; state open, label and assignee jeonghanlee unchanged, no milestone; body carries the current state with the deferral and the measurements.

## History

| Reset Date | Prior State Commit |
| --- | --- |
| 2026-11-09 | ba6c558a1118c792e769aed79ded7581538c3888 |
