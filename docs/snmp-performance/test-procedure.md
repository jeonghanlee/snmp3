# SNMP Device Performance Test Procedure

## Purpose

Repeatable procedure to find, for an SNMP device class, the number of objects per request, the
number of requests in flight, the request timeout and the monitoring periods at which a
monitoring set is read quickly without slowing the device for other clients. It also measures the
module path against a fast fixture agent. Findings from the first run are in
[evaluation.md](evaluation.md).

All device traffic is SNMP GET. Nothing is written to a device.

## Prerequisites

| Item | Requirement |
| --- | --- |
| Net-SNMP | Development libraries and `net-snmp-config`; the version is recorded with the results |
| Build tools | `gcc`, Python 3 |
| Device access | SNMPv3 user with read access to the monitoring set; the authentication and privacy passphrases each in a file of mode 0600 holding the passphrase as plain bytes with at most one trailing newline |
| Network | Operator's host reaches the device; no other client or module generates traffic to the device during a phase |
| Module path (optional, fixture steps) | The repository built with `make -j4` at its root (`configure/RELEASE.local` names the EPICS Base 7.0.10 install; products land in `bin/linux-x86_64`), and the same Base path in `EPICS_BASE` |
| Operator | One person present for phase 4 and for every device run of the module tool; phases can be stopped at any time |

## Inputs

**Targets file** (JSON, kept outside the repository because it holds addresses and secret paths):

```text
{"targets": [{"label": "unit-a", "address": "<numeric address>", "port": 161, "user": "<name>",
  "authAlgorithm": "SHA", "privAlgorithm": "AES",
  "authSecretFile": "/abs/auth", "privSecretFile": "/abs/priv", "group_size": 28}]}
```

`group_size` is the K used by phase 4 and by `run_module_database.py` for that target; it is
optional (default 28). The module tool also reads `idle_seconds` (optional).

**Object list** (TSV with header `oid`, `mib_object`, `tier`, `value_type`): the monitoring set,
numeric instance OIDs, tier `state`, `meas` or `slow`, value type `integer` or `octets`. Tiers by
reading priority: `state` values that change on an operator action or a fault (on/off states,
load state, power supply alarms), read most often; `meas` electrical measurements (current, power,
energy); `slow` identification, properties and configuration (thresholds, delays), read rarely. The set
used by the first run is [inputs/ap8932-oids.tsv](inputs/ap8932-oids.tsv) (358 objects: 38 state,
18 measurement, 302 slow). For a new model derive the list from its MIB: readable columns times
the rows of the layout, without command and control tables.

## Build

```text
bash docs/snmp-performance/tools/build.sh /abs/bin
```

builds `device_sweep`, `device_monitor` and `fixture_baseline` against the installed Net-SNMP.

## Rules

| # | Rule | Value |
| --- | --- | --- |
| 1 | Operation | GET only |
| 2 | One device at a time; in each repeat the devices are measured one after the other, in the order of the targets file | no overlap |
| 3 | Request timeout, retries | 3000 ms, 0 |
| 4 | Rest between steps, between devices | 3 s, 10 s |
| 5 | Stop on SNMP error status (for example `tooBig`) | always |
| 6 | Stop when two or more requests of a step are lost and they exceed 0.5 percent of the step | always |
| 7 | One isolated lost request | recorded, no stop |
| 8 | Repeats per point | 5 |
| 9 | A difference below the spread of the repeats | not reported as a difference |
| 10 | Record with every result | model, card generation, firmware, Net-SNMP version, tool hashes, run order, date |

## Metrics

| Metric | Definition |
| --- | --- |
| Pass time | Time to read the whole object list once, from the first request sent to the last reply |
| Per-object time | Pass time divided by the number of objects |
| Request time p50, p95, max | Per request, over the requests of one pass |
| Lost requests | Requests that ended in a timeout or send failure |
| Cycle time | Time from the due time of a tier to the reply of its last request |
| Overrun | A tier due again while its previous cycle is unfinished |
| Idle probe | One `sysUpTime` GET per second for 60 s; p50, p95, max |
| Second client p95 ratio | p95 of the probe during a schedule divided by p95 when idle |

## Decision rules

1. **Group size K\*** (per device class): the smallest K whose per-object time is within 10 percent
   of the best per-object time and whose request p95 is at most 1000 ms. If no K qualifies, state
   that and choose by judgement, with the reason recorded (the run with a legacy client present did
   so for NMC2; the reference run gave K\* = 20).
2. **Requests in flight W**: the smallest W after which the pass time improves by less than the
   spread of the repeats, and at which the request p95 stays below 60 percent of the timeout
   (the reference run measured 49 percent at K = 20, W = 3 and 35 percent at K = 28, W = 3 on NMC2).
3. **Timeout**: above the highest request p95 seen at the chosen K and W with a margin; the
   reference run used 3000 ms against a p95 of up to 1.5 s.
4. **Order**: no order is preferred unless it beats the best by more than the spread.
5. **Schedule passes** when all hold: no tier overruns its period; lost requests are below
   0.5 percent; the second client p95 ratio is at most 2.

## Procedure

Run every command from the repository root. Set the variables once (each command below is one
line):

```text
export T=/abs/targets.json O=/abs/objects.tsv OUT=/abs/results BIN=/abs/bin
```

| Phase | Command | What it does | Time per device |
| --- | --- | --- | --- |
| 0 | `python3 docs/snmp-performance/tools/run_device_phases.py phase0 --targets $T --oids $O --out $OUT --tools $BIN` | Idle probe, 30 requests; also writes the object-order files | 1 min |
| 1 | `python3 docs/snmp-performance/tools/run_device_phases.py phase1 --targets $T --oids $O --out $OUT --tools $BIN` | Group size sweep K = 2, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, window 1, 5 repeats; writes `kstar.json` | 10 min NMC2, 1 min NMC3 |
| 1b | same command as phase 1 with `--ks 64,80,96,128 --tag phase1b` | Optional: find the ceiling when K = 48 shows no error; writes `phase1b.json` and `kstar-phase1b.json`, not `phase1.json` | 1 min |
| 1c | same command as phase 1 with `--ks 1x2,1x5,1x10,1x16,1x20 --tag ramp` | Outstanding-request ramp: single-object requests with 2 to 20 in flight over the whole list; stops at the first loss; writes `ramp.json` | 1 to 2 min |
| 2 | `python3 docs/snmp-performance/tools/run_device_phases.py phase2 --targets $T --oids $O --out $OUT --tools $BIN --kstar $OUT/kstar.json` | Window 1, 2, 3 at K\*/2, K\*, and the best K | 7 min NMC2, 1 min NMC3 |
| 3 | same command with `phase3` | Order: inventory, by column, by row, random, without strings, at K\* | 9 min NMC2, 1 min NMC3 |
| 4 | `python3 docs/snmp-performance/tools/run_device_phases.py phase4 --targets $T --oids $O --out $OUT --tools $BIN` | Sustained schedule of 900 s with the second client; periods 5, 10, 60 s | 16 min |

A stop shows on the console as a step with return code 10 and in the phase JSON under `stopped`; the
rows measured before it are kept and the device is skipped for the rest of that phase. Do not rerun a
stopped device without finding the cause.

When the rule gave `kstar: null`, phases 2 and 3 use the best K of phase 1; set `kstar` in
`kstar.json` to override it (the run with a legacy client present set 32 for NMC2). Phase 4 needs a person present; ask the
owner of the device before starting it and announce the hold to any other user of the device.

Options of `run_device_phases.py`: `--repeats` (default 5), `--ks` (phase 1 list), `--timeout-ms`
(default 3000), `--tag` (output name of a phase 1 sweep), `--kstar` (phases 2 and 3), `--duration`
(phase 4 seconds, default 900), `--state-ms`, `--meas-ms`, `--slow-ms` (phase 4 periods, defaults 5000,
10000, 60000) and `--group-size` (phase 4 K for targets without `group_size`, default 28).

**Fixture steps** (module path, optional, no device needed):

```text
python3 docs/snmp-performance/tools/run_fixture_baseline.py --output /abs/fx-base --binary $BIN/fixture_baseline
EPICS_BASE=/abs/base python3 docs/snmp-performance/tools/run_fixture_module.py --output /abs/fx-module
```

The first compares Net-SNMP sequential, concurrent and multi-object requests with the added round
trip 0, 20, 100 ms; the second runs 100 records through the module IOC, worker and native path.
Each prints one JSON row per run and writes `baseline-results.json` and `snmp3-results.json` in its
output directory (new, empty). `--n`, `--delays`, `--versions`, `--repeat` (first) and `--sweeps`
(second) change the matrix.

**Module database runs.** The fixture runs need no device. A device run with
`run_module_database.py` needs the owner's authorization and a person present, as phase 4 does.
It reads the devices through the shipped module: an IOC with a record database, one binding and
one record per object, and the address worker, on the operator host.

```text
python3 docs/snmp-performance/tools/run_fixture_baseline.py --output /abs/fx-tiers --binary $BIN/fixture_baseline --group 28 --tiers 38,18,302
EPICS_BASE=/abs/base python3 docs/snmp-performance/tools/run_module_database.py --output /abs/fx-db --once --direct-binary $BIN/device_sweep
EPICS_BASE=/abs/base python3 docs/snmp-performance/tools/run_module_database.py --output /abs/fx-flat --shape flat
```

Device runs of the module (T2 is the first command, T3 the second; the direct phase 4 of the same
session gives the comparison of T3):

```text
EPICS_BASE=/abs/base python3 docs/snmp-performance/tools/run_module_database.py --output /abs/mod-t2 --once --targets $T --objects $O --direct-binary $BIN/device_sweep --confirm-authorized "<authorizing person, date>"
EPICS_BASE=/abs/base python3 docs/snmp-performance/tools/run_module_database.py --output /abs/mod-t3 --targets $T --objects $O --probe-binary $BIN/device_sweep --confirm-authorized "<authorizing person, date>"
```

Terms of the tool:

| Term | Meaning |
| --- | --- |
| tier | `state`, `meas` or `slow` of the object list, in that priority order |
| chunk | up to `--k` objects of one tier in list order, the remainder last (38 state objects at K = 28 give 28 and 10); with `--groups off` a chunk is K single-object requests, not one request of K objects |
| pass | all chunks of all tiers, one chunk after another; one request is outstanding at a time with `--groups off` |
| once (`--once`) | `--passes` passes (default 5), each started by one `caput -c` |
| sustained | without `--once`: the tiers run on their own periods of 5, 10 and 60 s for `--seconds` (default 900) |
| gate | a record that holds every scan until the idle phase has ended |
| second client | `device_sweep probefor`, one GET per second of one object (sysUpTime on devices), alone during the idle phase and then alongside the schedule |
| lost | a processing of a record with status COMM and no message |
| unsent | a record expired with the message "deadline before send" |
| invalid | a run with an unsent record: the deadline is shorter than the queue wait |

What is sent to a device (object list of 358 objects, K = 28, `--groups off`; GET only, no SET):

| Run | Requests | In flight |
| --- | --- | --- |
| module pass (`--once`) | 358 GET requests of one object each; 5 passes: 1 790 | 1 |
| direct pass (`--direct-binary`) | 14 GET requests of at most 28 objects (2 + 1 + 11) and one warm-up request; 5 repeats: 70 | 1 |
| sustained run, 900 s | about 14.4 requests per second offered (38 every 5 s, 18 every 10 s, 302 every 60 s): about 13 000 GET requests, plus 900 probe GETs | 1 module request (the three tier chains queue behind each other at the address worker) and the probe |
| direct phase 4, 900 s | as described for phase 4 | as described for phase 4 |

The module sends about 25 times as many requests as the direct pass for the same objects, so a
device that is slow with single-object requests (48 to 204 ms per object on NMC2 and 48 to 61 ms
on NMC3 through a record database, no other client, 2026-10-10) can be saturated by the
sustained run: a tier then misses its period (`overruns`), its records expire as unsent, and the
run ends as invalid. The normal monitoring rate of the devices is not part of these texts.

How a run is built. `--objects` (default `inputs/ap8932-oids.tsv`) is cut into chunks, and the
chunks of a pass are chained inside the database: the last record of a chunk completes into the
trigger of the next one. The pass time is the difference of the server time stamps of a
synchronous marker record at the start and of the marker after the last chunk, read by one
persistent `camonitor`. Records of type integer are `longin` and of type octets `lsi` with
capacity `--octets-capacity` (default 64): a longer value is a READ status and stops the run,
whereas `device_sweep` counts it as a success. Each device case writes `oids-by-tier.txt`, the
object list in tier order: the object file of the direct pass, so that the tier cut of
`device_sweep` (`tiers=38,18,302`, which must add up to the number of objects) holds the same
objects as the chunks. `--direct-binary` runs that direct pass in the same session after the
module passes, with the same K and the same number of repeats, at least 3 s after the last module
pass (two rests, so about 6 s on a device). On the fixture every object is read as the one integer
the agent serves; counts, tiers and order are those of the list.

What to read in `module-results.json` (one row of `cases` per device or fixture case):

| Judgement | Field |
| --- | --- |
| T2 module pass time p50, valid passes | `pass_p50_ms`, `valid_passes` (a pass with a lost, READ or unsent record, or that did not finish, is left out) |
| T2 direct pass time p50, ratio | `direct_p50_ms` (median of the `total_ms` of `device_sweep`; `p50_ms` of its rows is a request time), `module_to_direct_ratio`, at most 1.5; no ratio when the direct pass stopped, lost a request or returned fewer passes (`direct.sound` false, `stopped` is `direct-pass-failed`) |
| per tier and per chunk | `passes[].tier_ms`, `passes[].chunk_ms` |
| per record | `per_record_ms` |
| T3 cycles and overruns | `schedule.tiers.<tier>.cycles`, `completed`, `incomplete`, `overruns`, `p50_ms`, `max_ms`, and `schedule.overruns` (0; `incomplete` may be 1 for the cycle still running when the run ends) |
| T3 loss | `schedule.requests` (requests processed), `schedule.lost`, `schedule.loss_percent` (below 0.5): every processing of every record is counted from its monitor, so a loss is not overwritten by the next cycle |
| T3 second client | `second_client.idle.p95_ms`, `second_client.run.p95_ms`, `second_client.p95_ratio`; the direct ratio is `probe_during.p95_ms` divided by `idle_probe.p95_ms` of `phase4.json` (`summarize_results.py` prints it); the module run probes for 900 s and phase 4 for 890 s |
| stop and invalid | `stopped`, `ended_early`, `invalid`, `passes[].alarms`, `passes[].watch`, `error`, `authorization` |

Stop rules and what to do. The records are watched while the run goes on (every 0.5 s in `--once`,
every 2 s in the sustained schedule). The exit code is 1 for an error, 10 for a stop and 12 for an
invalid run, in that order of precedence; `stopped` names the reason.

| `stopped` or state | Meaning | At the bench |
| --- | --- | --- |
| `read-status` | a member returned status READ (a native failure that is not a transport failure, for example an SNMP error status such as `tooBig`, or a value that does not decode or fit): rule 5 | the run ends at once; find the cause before a rerun |
| `loss-above-0.5-percent` | two or more lost requests above 0.5 percent of the requests processed so far: rule 6, applied to the requests of the run so far and not of the step, so two losses among the first 400 requests stop a sustained run | the run ends; one isolated lost request is recorded and the run goes on |
| `pass-did-not-finish` | the markers of all chunks were not seen within `--pass-wait` (default 120 s) | the IOC is closed at once and the run ends; the next pass is not started on top of it |
| `direct-pass-failed` | the direct pass stopped, lost a request or gave fewer passes than asked | no ratio is given; find the cause |
| `invalid` (exit 12) | an unsent record: the run ends at once and is not a measurement | raise `--deadline-ms` (default 60000) or lower the load, then rerun |
| `error` | the IOC did not initialize, exited during the run, or an exception occurred | read `error` and `ioc.stderr` of the case directory; the passes finished before the failure stay in the row |

When a rule fires, the IOC is closed at once (its worker answers the requests in flight with
Stopping), the second client is ended and the direct pass is not started; the document is written.
A lost request is seen only after its timeout (3 s), so a stop cannot come sooner than that. A
request already sent to the device is not recalled. After a stop or an error the invocation ends
and the later devices of the targets file are not measured; run them again with a targets file that
holds the rest. In a device run passes are at least 3 s apart (`--pass-rest`) and devices 10 s apart
(`--device-rest`).

If the tool itself is killed, its child processes (`camonitor`, the IOC, the second client, the
direct pass, the agent and proxy of the fixture) end with it, but no results document is written;
check with `ps` for `snmp3Ioc`, `device_sweep` and `camonitor` after an abnormal end, because a
request in flight is not recalled.

The authorization gate. `--confirm-authorized` refuses to start a device run without a text; it
checks nothing else (no identity, date, address or time window). The text is written to
`authorization` of every case and of the environment in `module-results.json`, so state who
authorized the run and when. The same `environment` holds the date (UTC), the hash of the tool, the
commit of the module, whether its product sources (`snmp3App`, `configure`, `Makefile`) are
modified, the hashes of `snmp3Ioc` and `snmp3Worker`, the Net-SNMP version, the kernel, the Base
path and the options of the run; each case holds `started` and, from optional keys of the target, `model`, `card` and
`firmware`, which complete the record that rule 10 asks for.

Targets file keys used by the module tool: `label` (the case directory is `device-<label>`, so it
must be a safe path name), `group_size` (wins over `--k`), `idle_seconds` (optional; the idle phase
of T3 is 60 s on NMC2 and 300 s on NMC3, so a target sets it; without it `--idle-seconds`, 60 s,
applies and `idle_seconds_from_target` is false), `model`, `card` and `firmware` (optional, copied to the case row), `authAlgorithm` and `privAlgorithm` (passed to
the module as given; `device_sweep` maps anything other than MD5 to SHA1 and anything other than DES
to AES). The object of the second client is `--probe-oid` (sysUpTime on devices, as in phase 4); the second
client records at most 4096 samples, so `--seconds` is at most 4072 with it.
Phase 4 uses a fixed idle phase of 60 s; each ratio of T3 is taken against the idle phase of its
own run. Use a SNMPv3 user with read access only, announce the hold to other users of the device
and stop other clients first, as for phase 4: the tool cannot detect them.

Order of a device session: module `--once` run with its direct pass for each device (T2), then
the sustained run with its idle phase and second client, then the direct phase 4 of
`run_device_phases.py` for the comparison of T3, with 10 s between devices. The first run of the
tool on the tree before declared groups (the tree after Stage A, recorded in `environment` of the
output) in `--groups off` mode is the module baseline of T2 and T3: it is expected to exceed the
1.5 ratio (the fixture diagnostic gives about 26), and it is recorded as the baseline. `--groups on` writes one group per chunk; a product without declared groups rejects
the `group` field ("unknown configuration field") and the case ends with an `error` row. With a
product that accepts it, a chunk ends when its counter record reaches the number of its members
(the members complete on parallel callback threads, so the last member of the list is not the
last to finish). In `--once` mode the tool reads the counters and the end markers of every pass and
lists a violation in `counter_problems` of the pass: a count sequence other than 1 to K, a member
that completes after the last count or before the previous chunk ended, or an end marker posted
before the last count. A pass with a problem is not a valid pass.
Decision rule 5 above states an absolute ratio for the direct phases; T3 compares the module
ratio with the direct ratio of the same session: the module ratio is at most 1.25 times the direct
ratio, and a pair whose ratio lies between 1.15 and 1.35 is repeated once and the median of the two
is used (the tool reports `second_client.p95_ratio`; the direct ratio is read from `phase4.json`).

Time of a device run, from the defaults of the tool and the single-object times above: T2 about 7
minutes per NMC2-class device (5 passes of up to about 73 s, the direct pass about 40 s) and
about 3 minutes per NMC3-class device (5 passes of about 22 s); T3 about 17 minutes (60 s idle,
900 s and about 20 s of start and close) on NMC2 and 21 minutes (300 s idle) on NMC3; the direct
phase 4 about 16 minutes per device. For two units that is about 1 hour 20 minutes, not the 35
minutes of the plan sentence, which does not include the module runs.

Reproduction of the fixture results of the register (T1 diagnostic and the 100-record interval):
the module tool runs SNMP v3 by default (`--versions 3`), `run_fixture_baseline.py` runs 2c and 3
and repeats three times (compare its v3 rows), and the 100-record interval is taken in one
session with both runners:

```text
EPICS_BASE=/abs/base python3 docs/snmp-performance/tools/run_module_database.py --output /abs/fx-flat --shape flat --n 100 --sweeps 12 --delays 0 --versions 2c,3
EPICS_BASE=/abs/base python3 docs/snmp-performance/tools/run_fixture_module.py --output /abs/fx-old --n 100 --sweeps 12 --delays 0 --versions 2c,3
```

`interval_ms` of a sweep is the span between the first and the last update time stamp of the 100
records divided by 99; the runners are compared by the medians of their sweeps. At 0 ms the
interval is 0.1 to 0.8 ms and the sweeps of one runner spread by more than 15 percent, so a ratio
of the medians is reproduced within that spread only (twelve sweeps give about 1.0; four sweeps
gave 0.5 to 1.1). The 0 ms module pass of the T1 diagnostic varies between runs on one host (p50
108 to 190 ms, ratio to the `device_sweep` direct pass 5 to 13). It is not a device number: all
objects are the same OID on the loopback fixture, so a 0 ms pass measures the module and local
overhead only. The two direct tools differ at 0 ms (`device_sweep` 13 to 21 ms, asynchronous
requests with a select loop; `run_fixture_baseline` 1.4 ms, synchronous requests): the cause is
not analysed here, and at 20 and 100 ms they agree. Running time: the T1 module command with five
passes at three delays about 5 minutes, the baseline about 15 s.

Options: `--tier state|meas|slow` runs one tier alone, `--shape flat` is the 100-record
measurement of `run_fixture_module.py` (`--n`, `--sweeps`, `timeoutMs` 5000, no `maxVarbinds`,
the same `interval_ms` per record), `--timeout-ms` and `--max-varbinds` default to 3000 and 32.
The output directory must not exist; it receives `module-results.json` and one directory per case
with `config.json` (addresses and secret file paths: keep it outside the repository), `records.db`,
`startup.cmd`, `ioc.stdout`, `ioc.stderr` and `monitor.out`.

## Output

Each phase writes JSON into the output directory (`phase0.json`, `phase1.json`, `kstar.json`,
`phase2.json`, `phase3-*.json`, `phase4.json`, and `phase1b.json` or `ramp.json` for the optional steps). Print the report tables (Markdown) with:

```text
python3 docs/snmp-performance/tools/summarize_results.py $OUT
```

Report, per device class: the K\* decision, the
pass time table by K, the window table, the order table, the sustained table with the second client
ratio, and the model fit (`fixed + per_object * K`, least squares over K = 2 to 48). The layout of
the reference run is [results/reference-run.json](results/reference-run.json).

## Reference values and tolerance bands

Reference run: 358 objects, SNMPv3 authPriv, GET only, with no other client on the devices. An
earlier run with a legacy IOC polling the same units is kept in
[results/reference-run.json](results/reference-run.json) under `legacy_client_present`.

The reference is the median of five repeats. The pass-time bands are a fixed share of the reference
(15 percent on NMC2, 25 percent on NMC3), not the observed spread; the other rows state their share.
Compare the median of the repeats of a rerun on the same class and firmware with the band, not a
single pass: single passes of the reference run fell outside the band at three points (NMC2 K=8 3.9
to 5.1 s and K=32 3.4 to 4.3 s; NMC3 K=48 117 to 243 ms). A median outside a band is a finding to
investigate (firmware change, device load, network path, another client on the device).

| Quantity | NMC2 reference | NMC2 tolerance band | NMC3 reference | NMC3 tolerance band |
| --- | --- | --- | --- | --- |
| Pass, K=2 | 7.15 s | 6.08 s to 8.22 s | 457 ms | 343 ms to 572 ms |
| Pass, K=8 | 4.96 s | 4.21 s to 5.70 s | 242 ms | 182 ms to 303 ms |
| Pass, K=32 | 3.45 s | 2.93 s to 3.97 s | 143 ms | 107 ms to 179 ms |
| Pass, K=48 | 3.99 s | 3.39 s to 4.59 s | 157 ms | 118 ms to 196 ms |
| Request p95, K=32 | 680 ms | +/- 30 percent | 33 ms | +/- 50 percent |
| Idle probe p50 (1 per s) | 19 ms | +/- 50 percent | 2.5 ms | +/- 50 percent |
| State cycle p50 (38 objects, 5 s) | 757 ms | +/- 30 percent | 33 ms | +/- 50 percent |

The single-object pass (K = 1) is not part of phase 1; with a legacy client present it was 66.5 s
on NMC2 and 748 ms on NMC3.

## Time budget

A full run of phases 0 to 4 with 1b and 1c on one NMC2-class and one NMC3-class unit took about 1
hour 10 minutes (phase 4 about 33 minutes of it for both units); the fixture steps add about 10
minutes. The module runs of both units add about 50 minutes (T2 and T3 without the direct phase 4, see
'Module database runs').

## Limits of the procedure

- It measures one monitoring set per run and one SNMPv3 profile; other sets and SNMPv1/v2c are
  not covered.
- The second client reads only `sysUpTime`; other services of the device are not measured.
- The reference values come from one day on one pair of units, with no other client on them; the
  bands are chosen shares of the reference, not a measured distribution.
- It cannot detect another client by itself: check for other pollers of the device (running IOCs,
  monitoring systems) before a run and record what was found with the results.
- The first run had no 10 s rest between the two devices in phases 1 to 3 (they are different
  hardware); the tool now applies it.
