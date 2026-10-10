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
| Operator | One person present for phase 4; phases can be stopped at any time |

## Inputs

**Targets file** (JSON, kept outside the repository because it holds addresses and secret paths):

```text
{"targets": [{"label": "unit-a", "address": "<numeric address>", "port": 161, "user": "<name>",
  "authAlgorithm": "SHA", "privAlgorithm": "AES",
  "authSecretFile": "/abs/auth", "privSecretFile": "/abs/priv", "group_size": 28}]}
```

`group_size` is the K used by phase 4 for that target; it is optional (default 28).

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
   that and choose by judgement, with the reason recorded (the first run did so for NMC2).
2. **Requests in flight W**: the smallest W after which the pass time improves by less than the
   spread of the repeats, and at which the request p95 stays below 60 percent of the timeout
   (the first run measured 55 percent at K = 32, W = 3 on NMC2).
3. **Timeout**: above the highest request p95 seen at the chosen K and W with a margin; the first
   run used 3000 ms against a p95 of up to 1.6 s.
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
| 1 | `python3 docs/snmp-performance/tools/run_device_phases.py phase1 --targets $T --oids $O --out $OUT --tools $BIN` | Group size sweep K = 2, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, window 1, 5 repeats; writes `kstar.json` | 15 min NMC2, 2 min NMC3 |
| 1b | same command as phase 1 with `--ks 64,80,96,128 --tag phase1b` | Optional: find the ceiling when K = 48 shows no error; writes `phase1b.json` and `kstar-phase1b.json`, not `phase1.json` | 2 min |
| 1c | same command as phase 1 with `--ks 1x2,1x5,1x10,1x16,1x20 --tag ramp` | Outstanding-request ramp: single-object requests with 2 to 20 in flight over the whole list; stops at the first loss; writes `ramp.json` | 1 min |
| 2 | `python3 docs/snmp-performance/tools/run_device_phases.py phase2 --targets $T --oids $O --out $OUT --tools $BIN --kstar $OUT/kstar.json` | Window 1, 2, 3 at K\*/2, K\*, and the best K | 10 min NMC2, 2 min NMC3 |
| 3 | same command with `phase3` | Order: inventory, by column, by row, random, without strings, at K\* | 10 min NMC2, 2 min NMC3 |
| 4 | `python3 docs/snmp-performance/tools/run_device_phases.py phase4 --targets $T --oids $O --out $OUT --tools $BIN` | Sustained schedule of 900 s with the second client; periods 5, 10, 60 s | 17 min |

A stop shows on the console as a step with return code 10 and in the phase JSON under `stopped`; the
rows measured before it are kept and the device is skipped for the rest of that phase. Do not rerun a
stopped device without finding the cause.

When the rule gave `kstar: null`, phases 2 and 3 use the best K of phase 1; set `kstar` in
`kstar.json` to override it (the first run set 32 for NMC2). Phase 4 needs a person present; ask the
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

## Output

Each phase writes JSON into the output directory (`phase0.json`, `phase1.json`, `kstar.json`,
`phase2.json`, `phase3-*.json`, `phase4.json`, and `phase1b.json` or `ramp.json` for the optional steps). Print the report tables (Markdown) with:

```text
python3 docs/snmp-performance/tools/summarize_results.py $OUT
```

Report, per device class: the K\* decision, the
pass time table by K, the window table, the order table, the sustained table with the second client
ratio, and the model fit (`fixed + per_object * K`, least squares over K = 2 to 48). The layout of
the first run is [results/reference-run.json](results/reference-run.json).

## Reference values and tolerance bands

First run, 358 objects, SNMPv3 authPriv, taken while a legacy IOC was polling the same units (the
no-other-client prerequisite was not met); treat the values as figures with a second client present
until a run with the devices otherwise idle replaces them. A rerun on the same class and firmware is expected inside
the bands, which are the observed spread of five repeats widened to a round number; a result
outside a band is a finding to investigate (firmware change, device load, network path).

| Quantity | NMC2 reference | NMC2 tolerance band | NMC3 reference | NMC3 tolerance band |
| --- | --- | --- | --- | --- |
| Pass, K=1 | 66.5 s | 56.6 s to 76.5 s | 748 ms | 561 ms to 935 ms |
| Pass, K=8 | 14.2 s | 12.0 s to 16.3 s | 274 ms | 206 ms to 343 ms |
| Pass, K=32 | 6.17 s | 5.24 s to 7.09 s | 168 ms | 126 ms to 210 ms |
| Pass, K=48 | 5.33 s | 4.53 s to 6.13 s | 157 ms | 117 ms to 196 ms |
| Request p95, K=32 | 1342 ms | +/- 30 percent | 35 ms | +/- 50 percent |
| Idle probe p50 (1 per s) | 200 ms | +/- 50 percent | 2.5 ms | +/- 50 percent |
| State cycle p50 (38 objects, 5 s) | 1077 ms | +/- 30 percent | 33 ms | +/- 50 percent |

## Time budget

A full run of phases 0 to 4 on one NMC2-class and one NMC3-class unit takes about 1 hour 15 minutes
without phase 1b; the fixture steps add about 10 minutes.

## Limits of the procedure

- It measures one monitoring set per run and one SNMPv3 profile; other sets and SNMPv1/v2c are
  not covered.
- The second client reads only `sysUpTime`; other services of the device are not measured.
- The tolerance bands come from one day on one pair of units, with a legacy client polling them.
- It cannot detect another client by itself: check for other pollers of the device (running IOCs,
  monitoring systems) before a run and record what was found with the results.
- The first run had no 10 s rest between the two devices in phases 1 to 3 (they are different
  hardware); the tool now applies it.
