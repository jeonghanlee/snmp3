# Performance test reference

The performance procedures read a monitoring set from SNMP devices and from a fixture agent, and they judge the results against fixed rules. This page lists the inputs, rules, metrics, decision rules, tool options, result fields, stop rules, and reference values that those procedures use.

## Scope

This page covers the values behind three procedures: [Measure device request performance](test-procedure.md), [Measure the module path against a fixture agent](fixture-procedure.md), and [Measure devices through the module database](module-database-procedure.md). It also lists the reference run and its tolerance bands.

**Out of scope:** the steps of each procedure, and the findings of the reference run (see [Request performance](evaluation.md) and [Request performance results](results.md)).

Abbreviations on this page: OID is an object identifier, JSON is the JavaScript Object Notation, and SNMP is the Simple Network Management Protocol.

All device traffic is an SNMP GET. The procedures write nothing to a device.

In this page, NMC2 and NMC3 name the second and third generation of the network management card (NMC) in the APC AP8932 power distribution unit. K is the number of objects in one request. W is the number of requests in flight. The p50, p95, and max values are the median, the 95th percentile, and the maximum. T1, T2, and T3 name three module checks: T1 is the diagnostic on the fixture agent, T2 is the once run on devices, and T3 is the sustained run on devices.

## Inputs of the measurements

### Format of the targets file

The targets file is JSON. Keep it outside the repository, because it holds addresses and secret file paths.

```text
{"targets": [{"label": "unit-a", "address": "<numeric_address>", "port": 161, "user": "<name>",
  "authAlgorithm": "SHA", "privAlgorithm": "AES",
  "authSecretFile": "/abs/auth", "privSecretFile": "/abs/priv", "group_size": 28}]}
```

The `group_size` key is the K that phase 4 and `run_module_database.py` use for that target. It is optional, and its default is 28. The module tool also reads the optional `idle_seconds` key.

### Format of the object list

The object list is a tab-separated values (TSV) file with the header `oid`, `mib_object`, `tier`, `value_type`. It holds the monitoring set as numeric instance object identifiers (OIDs). The tier is `state`, `meas`, or `slow`. The value type is `integer` or `octets`.

The tiers are ordered by reading priority:

- `state` holds values that change on an operator action or a fault, such as on and off states, load state, and power supply alarms. The procedures read it most often.
- `meas` holds electrical measurements: current, power, and energy.
- `slow` holds identification, properties, and configuration such as thresholds and delays. The procedures read it rarely.

The set of the reference run is [inputs/ap8932-oids.tsv](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/snmp-performance/inputs/ap8932-oids.tsv). It has 358 objects: 38 `state`, 18 `meas`, and 302 `slow`. For another model, derive the list from its management information base (MIB): the readable columns times the rows of the layout, without the command and control tables.

## Rules for each measurement

| # | Rule | Value |
| --- | --- | --- |
| 1 | Operation | GET only |
| 2 | Devices | One device at a time; in each repeat the devices are measured one after the other, in the order of the targets file; no overlap |
| 3 | Request timeout, retries | 3000 ms, 0 |
| 4 | Rest between steps, between devices | 3 s, 10 s |
| 5 | Stop on SNMP error status (for example `tooBig`) | Always |
| 6 | Stop when two or more requests of a step are lost and they exceed 0.5 percent of the step | Always |
| 7 | One isolated lost request | Recorded, no stop |
| 8 | Repeats per point | 5 |
| 9 | A difference below the spread of the repeats | Not reported as a difference |
| 10 | Record with every result | Model, card generation, firmware, Net-SNMP version, tool hashes, run order, date |

## Metrics of the measurements

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

## Decision rules for results

1. **Group size K\*** for a device class is the smallest K whose per-object time is within 10 percent of the best per-object time and whose request p95 is at most 1000 ms. If no K qualifies, state that and choose by judgement, and record the reason. The run with a legacy client present did so for NMC2. The reference run gave K\* = 20.
2. **Requests in flight W** is the smallest W after which the pass time improves by less than the spread of the repeats, and at which the request p95 stays below 60 percent of the timeout. The reference run measured 49 percent at K = 20 and W = 3, and 35 percent at K = 28 and W = 3, on NMC2.
3. **Timeout** is above the highest request p95 seen at the chosen K and W, with a margin. The reference run used 3000 ms against a p95 of up to 1.5 s.
4. **Order**: no order is preferred unless it beats the best by more than the spread.
5. **A schedule passes** when all of these hold: no tier overruns its period, lost requests are below 0.5 percent, and the second client p95 ratio is at most 2.

For the module path, the module ratio must be at most 1.25 times the direct ratio of the same session. A pair whose ratio lies between 1.15 and 1.35 is repeated once, and the median of the two is used. The tool reports `second_client.p95_ratio`. The direct ratio comes from `phase4.json`.

## Options of the device phase tool

`run_device_phases.py` takes these options:

| Option | Meaning |
| --- | --- |
| `--repeats` | Repeats per point; default 5 |
| `--ks` | The list of K values of a phase 1 sweep |
| `--timeout-ms` | Request timeout; default 3000 |
| `--tag` | Output name of a phase 1 sweep |
| `--kstar` | The K\* file for phases 2 and 3 |
| `--duration` | Seconds of phase 4; default 900 |
| `--state-ms`, `--meas-ms`, `--slow-ms` | Periods of the three tiers in phase 4; defaults 5000, 10000, 60000 |
| `--group-size` | K of phase 4 for targets without `group_size`; default 28 |

When the K\* rule gives `kstar: null`, phases 2 and 3 use the best K of phase 1. Set `kstar` in `kstar.json` to override it. The run with a legacy client present set 32 for NMC2.

A stop shows on the console as a step with return code 10 and in the phase JSON under `stopped`. The tool keeps the rows measured before the stop, and it skips the device for the rest of that phase.

## Terms of the module database tool

| Term | Meaning |
| --- | --- |
| tier | `state`, `meas`, or `slow` of the object list, in that priority order |
| chunk | Up to `--k` objects of one tier in list order, the remainder last. 38 `state` objects at K = 28 give 28 and 10. With `--groups off`, a chunk is K single-object requests, not one request of K objects. |
| pass | All chunks of all tiers, one chunk after another; one request is outstanding at a time with `--groups off` |
| once (`--once`) | `--passes` passes (default 5), each started by one `caput -c` |
| sustained | Without `--once`: the tiers run on their own periods of 5, 10, and 60 s for `--seconds` (default 900) |
| gate | A record that holds every scan until the idle phase has ended |
| second client | `device_sweep probefor`: one GET per second of one object (`sysUpTime` on devices), alone during the idle phase and then alongside the schedule |
| lost | A processing of a record with status COMM and no message |
| unsent | A record that expired with the message `deadline before send` |
| invalid | A run with an unsent record: the deadline is shorter than the queue wait |

### How a run is built

The tool cuts the `--objects` list (default `inputs/ap8932-oids.tsv`) into chunks. The chunks of a pass are chained inside the database: the last record of a chunk completes into the trigger of the next one.

The pass time is the difference of the server time stamps of two marker records. A synchronous marker record starts the pass, and a marker after the last chunk ends it. One persistent `camonitor` reads both.

Records of type integer are `longin` records. Records of type octets are `lsi` records with the capacity `--octets-capacity` (default 64). A longer value is a READ status and stops the run, whereas `device_sweep` counts it as a success.

Each device case writes `oids-by-tier.txt`, the object list in tier order. It is the object file of the direct pass, so the tier cut of `device_sweep` (`tiers=38,18,302`, which must add up to the number of objects) holds the same objects as the chunks.

The `--direct-binary` option runs the direct pass in the same session after the module passes. It uses the same K and the same number of repeats, and it starts at least 3 s after the last module pass (two rests, so about 6 s on a device).

On the fixture, every object is read as the one integer that the agent serves. The counts, tiers, and order are those of the list.

### What a run sends to a device

The numbers assume the object list of 358 objects, K = 28, and `--groups off`. Every request is a GET.

| Run | Requests | In flight |
| --- | --- | --- |
| Module pass (`--once`) | 358 GET requests of one object each; 5 passes: 1790 | 1 |
| Direct pass (`--direct-binary`) | 14 GET requests of at most 28 objects (2 + 1 + 11) and one warm-up request; 5 repeats: 70 | 1 |
| Sustained run, 900 s | About 14.4 requests per second offered (38 every 5 s, 18 every 10 s, 302 every 60 s): about 13000 GET requests, plus 900 probe GETs | 1 module request (the three tier chains queue behind each other at the address worker) and the probe |
| Direct phase 4, 900 s | As described for phase 4 | As described for phase 4 |

The module sends about 25 times as many requests as the direct pass for the same objects. A device that is slow with single-object requests can be saturated by the sustained run. Through a record database, with no other client, NMC2 needed 48 to 204 ms per object and NMC3 needed 48 to 61 ms (2026-10-10). A saturated tier misses its period (`overruns`), its records expire as unsent, and the run ends as invalid. The normal monitoring rate of the devices is not part of these values.

## Result fields to read

`module-results.json` holds one row of `cases` per device or fixture case. Read these fields:

| Judgement | Field |
| --- | --- |
| T2 module pass time p50, valid passes | `pass_p50_ms`, `valid_passes`. A pass with a lost, READ, or unsent record, or one that did not finish, is left out. |
| T2 direct pass time p50, ratio | `direct_p50_ms` (the median of the `total_ms` of `device_sweep`; `p50_ms` of its rows is a request time) and `module_to_direct_ratio`, at most 1.5. There is no ratio when the direct pass stopped, lost a request, or returned fewer passes (`direct.sound` is false and `stopped` is `direct-pass-failed`). |
| Per tier and per chunk | `passes[].tier_ms`, `passes[].chunk_ms` |
| Per record | `per_record_ms` |
| T3 cycles and overruns | `schedule.tiers.<tier>.cycles`, `completed`, `incomplete`, `overruns`, `p50_ms`, `max_ms`, and `schedule.overruns` (0). `incomplete` can be 1 for the cycle that runs when the run ends. |
| T3 loss | `schedule.requests` (requests processed), `schedule.lost`, and `schedule.loss_percent` (below 0.5). The tool counts every processing of every record from its monitor, so the next cycle does not overwrite a loss. |
| T3 second client | `second_client.idle.p95_ms`, `second_client.run.p95_ms`, and `second_client.p95_ratio`. The direct ratio is `probe_during.p95_ms` divided by `idle_probe.p95_ms` of `phase4.json`, which `summarize_results.py` prints. The module run probes for 900 s, and phase 4 probes for 890 s. |
| Stop and invalid | `stopped`, `ended_early`, `invalid`, `passes[].alarms`, `passes[].watch`, `error`, `authorization` |

## Stop rules of the module tool

The tool watches the records while the run goes on: every 0.5 s with `--once`, and every 2 s in the sustained schedule. The exit code is 1 for an error, 10 for a stop, and 12 for an invalid run, in that order of precedence. The `stopped` field names the reason.

| `stopped` or state | Meaning | At the bench |
| --- | --- | --- |
| `read-status` | A member returned status READ: a native failure that is not a transport failure, such as an SNMP error status like `tooBig`, or a value that does not decode or fit. Rule 5 applies. | The run ends at once. Find the cause before a rerun. |
| `loss-above-0.5-percent` | Two or more lost requests above 0.5 percent of the requests processed so far. Rule 6 applies to the requests of the run so far, not of the step, so two losses among the first 400 requests stop a sustained run. | The run ends. One isolated lost request is recorded and the run goes on. |
| `pass-did-not-finish` | The markers of all chunks were not seen within `--pass-wait` (default 120 s). | The tool closes the IOC at once and the run ends. The next pass does not start on top of it. |
| `direct-pass-failed` | The direct pass stopped, lost a request, or gave fewer passes than asked. | The tool gives no ratio. Find the cause. |
| `invalid` (exit 12) | An unsent record. The run ends at once and is not a measurement. | Raise `--deadline-ms` (default 60000) or lower the load, then rerun. |
| `error` | The IOC did not initialize, exited during the run, or an exception occurred. | Read `error` and `ioc.stderr` of the case directory. The passes finished before the failure stay in the row. |

When a rule fires, the tool closes the IOC at once, and the worker answers the requests in flight with Stopping. The tool ends the second client, does not start the direct pass, and writes the document. The tool sees a lost request only after its timeout of 3 s, so a stop cannot come sooner. A request already sent to the device is not recalled.

After a stop or an error, the invocation ends and the later devices of the targets file are not measured. Run them again with a targets file that holds the rest. In a device run, passes are at least 3 s apart (`--pass-rest`) and devices are 10 s apart (`--device-rest`).

If you kill the tool, its child processes end with it (`camonitor`, the IOC, the second client, the direct pass, and the agent and proxy of the fixture), but no results document is written. After an abnormal end, check with `ps` for `snmp3Ioc`, `device_sweep`, and `camonitor`, because a request in flight is not recalled.

## Authorization gate for device runs

The option `--confirm-authorized` makes the tool refuse to start a device run without a text. It checks nothing else: no identity, date, address, or time window. The tool writes the text to `authorization` of every case and of the environment in `module-results.json`, so state who authorized the run and when.

The `environment` holds these values:

- the date (UTC) and the hash of the tool
- the commit of the module, and whether its product sources (`snmp3App`, `configure`, `Makefile`) are modified
- the hashes of `snmp3Ioc` and `snmp3Worker`
- the Net-SNMP version and the kernel
- the Base path and the options of the run

Each case holds `started` and, from optional keys of the target, `model`, `card`, and `firmware`. These values complete the record that rule 10 asks for.

## Targets file keys of the module tool

| Key | Meaning |
| --- | --- |
| `label` | The case directory is `device-<label>`, so the label must be a safe path name |
| `group_size` | Wins over `--k` |
| `idle_seconds` | Optional. The idle phase of T3 is 60 s on NMC2 and 300 s on NMC3, so a target sets it. Without it, `--idle-seconds` (60 s) applies and `idle_seconds_from_target` is false. |
| `model`, `card`, `firmware` | Optional; copied to the case row |
| `authAlgorithm`, `privAlgorithm` | Passed to the module as given. `device_sweep` maps anything other than MD5 to SHA1 and anything other than DES to AES. |

The object of the second client is `--probe-oid` (`sysUpTime` on devices, as in phase 4). The second client records at most 4096 samples, so `--seconds` is at most 4072 with it. Phase 4 uses a fixed idle phase of 60 s. Each ratio of T3 is taken against the idle phase of its own run.

## Module tool options

- `--tier state|meas|slow` runs one tier alone.
- `--shape flat` is the 100-record measurement of `run_fixture_module.py`. It uses `--n`, `--sweeps`, `timeoutMs` 5000, no `maxVarbinds`, and the same `interval_ms` per record.
- `--timeout-ms` and `--max-varbinds` default to 3000 and 32.
- `--sample-seconds <n>` samples the resources of the IOC and of every worker every `<n>` seconds during a sustained run. The default 0 switches sampling off.
- `--groups on` writes one group per chunk. A product without declared groups rejects the `group` field (`unknown configuration field`), and the case ends with an `error` row.

The output directory must not exist. It receives `module-results.json` and one directory per case. Each case directory holds `config.json` (addresses and secret file paths: keep it outside the repository), `records.db`, `startup.cmd`, `ioc.stdout`, `ioc.stderr`, and `monitor.out`.

### Resource sampling

With `--sample-seconds`, the sustained run writes two CSV files into the case directory and records `resources` in the case row (`interval_s`, `samples`, `missed_reports`, and the two file names). A sample asks the IOC for `snmp3RuntimeReport` and reads `/proc` for the IOC and for each worker PID that the report names.

`resources.csv` has one row per process and sample:

| Column | Meaning |
| --- | --- |
| `t_s` | Seconds since the sampler started |
| `phase` | `idle` before the schedule, `schedule` during it |
| `process`, `pid` | `ioc` or `worker-<address>`, and its process identifier |
| `rss_kb` | Resident set size in kilobytes |
| `threads`, `fds` | Threads and open file descriptors |
| `sockets` | Open socket descriptors, including the IPC channel. It stands in for the native session count, which the runtime report does not print. |
| `cpu_s` | User plus system CPU seconds since the process started |

`counters.csv` has one row per address and sample. It holds the queue counters (`count`, `bytes`, `queued`, `active`, `undelivered`, `retirement_pending`, `behind_retirement`), the record counters (`records_active`, `records_pending`, `records_completions`, `records_enqueue_failures`), the servicing-loop passes (`loop_passes`), and the runtime `state`.

### Group counters of a chunk

With a product that accepts declared groups, a chunk ends when its counter record reaches the number of its members. The members complete on parallel callback threads, so the last member of the list is not the last to finish.

In `--once` mode, the tool reads the counters and the end markers of every pass. It lists a violation in `counter_problems` of the pass. A violation is one of these:

- a count sequence other than 1 to K
- a member that completes after the last count, or before the previous chunk ended
- an end marker posted before the last count

A pass with a problem is not a valid pass.

## Fixture result variability

The module tool runs SNMP v3 by default (`--versions 3`). `run_fixture_baseline.py` runs 2c and 3 and repeats three times, so compare its v3 rows.

The `interval_ms` of a sweep is the span between the first and the last update time stamp of the 100 records, divided by 99. The procedure compares the runners by the medians of their sweeps.

At 0 ms, the interval is 0.1 to 0.8 ms, and the sweeps of one runner spread by more than 15 percent. A ratio of the medians therefore reproduces within that spread only: twelve sweeps give about 1.0, and four sweeps gave 0.5 to 1.1.

The 0 ms module pass of the T1 diagnostic varies between runs on one host: p50 of 108 to 190 ms, and a ratio to the `device_sweep` direct pass of 5 to 13. It is not a device number. All objects are the same OID on the loopback fixture, so a 0 ms pass measures the module and local overhead only.

The two direct tools differ at 0 ms: `device_sweep` takes 13 to 21 ms with asynchronous requests and a select loop, and `run_fixture_baseline` takes 1.4 ms with synchronous requests. The cause is not analysed. At 20 and 100 ms they agree.

## Phase output files

Each phase writes JSON into the output directory: `phase0.json`, `phase1.json`, `kstar.json`, `phase2.json`, `phase3-*.json`, `phase4.json`, and `phase1b.json` or `ramp.json` for the optional steps.

For each device class, the report has these parts: the K\* decision, the pass time table by K, the window table, the order table, and the sustained table with the second client ratio. It also has the model fit (`fixed + per_object * K`, least squares over K = 2 to 48). The layout of the reference run is [results/reference-run.json](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/snmp-performance/results/reference-run.json).

## Reference values and tolerance bands

The reference run used 358 objects, SNMPv3 authPriv, GET only, and no other client on the devices. An earlier run with a legacy IOC polling the same units is kept in [results/reference-run.json](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/snmp-performance/results/reference-run.json) under `legacy_client_present`.

The reference is the median of five repeats. The pass-time bands are a fixed share of the reference: 15 percent on NMC2 and 25 percent on NMC3. They are not the observed spread. The other rows state their share.

Compare the median of the repeats of a rerun on the same class and firmware with the band, not a single pass. Single passes of the reference run fell outside the band at three points: NMC2 at K = 8 (3.9 to 5.1 s) and K = 32 (3.4 to 4.3 s), and NMC3 at K = 48 (117 to 243 ms). A median outside a band is a finding to investigate. Possible causes are a firmware change, device load, the network path, or another client on the device.

| Quantity | NMC2 reference | NMC2 tolerance band | NMC3 reference | NMC3 tolerance band |
| --- | --- | --- | --- | --- |
| Pass, K=2 | 7.15 s | 6.08 s to 8.22 s | 457 ms | 343 ms to 572 ms |
| Pass, K=8 | 4.96 s | 4.21 s to 5.70 s | 242 ms | 182 ms to 303 ms |
| Pass, K=32 | 3.45 s | 2.93 s to 3.97 s | 143 ms | 107 ms to 179 ms |
| Pass, K=48 | 3.99 s | 3.39 s to 4.59 s | 157 ms | 118 ms to 196 ms |
| Request p95, K=32 | 680 ms | +/- 30 percent | 33 ms | +/- 50 percent |
| Idle probe p50 (1 per s) | 19 ms | +/- 50 percent | 2.5 ms | +/- 50 percent |
| State cycle p50 (38 objects, 5 s) | 757 ms | +/- 30 percent | 33 ms | +/- 50 percent |

The single-object pass (K = 1) is not part of phase 1. With a legacy client present, it was 66.5 s on NMC2 and 748 ms on NMC3.

## Time budget of a full run

A full run of phases 0 to 4 with 1b and 1c on one NMC2-class unit and one NMC3-class unit took about 1 hour 10 minutes. Phase 4 took about 33 minutes of that for both units. The fixture steps add about 10 minutes.

The module runs of both units add about 50 minutes. That figure covers T2 and T3 without the direct phase 4.

A device run of the module takes the following time, from the defaults of the tool and the single-object times above:

- T2 takes about 7 minutes per NMC2-class device (5 passes of up to about 73 s, and the direct pass about 40 s). It takes about 3 minutes per NMC3-class device (5 passes of about 22 s).
- T3 takes about 17 minutes on NMC2 (60 s idle, 900 s, and about 20 s of start and close) and 21 minutes on NMC3 (300 s idle).
- The direct phase 4 takes about 16 minutes per device.

For two units, that is about 1 hour 20 minutes. It is not the 35 minutes of the plan sentence, which does not include the module runs.

The T1 module command with five passes at three delays takes about 5 minutes. The baseline takes about 15 s.

## Limits of the procedures

- They measure one monitoring set per run and one SNMPv3 profile. Other sets and SNMPv1 or v2c are not covered.
- The second client reads only `sysUpTime`. Other services of the device are not measured.
- The reference values come from one day on one pair of units, with no other client on them. The bands are chosen shares of the reference, not a measured distribution.
- They cannot detect another client by themselves. Before a run, check for other pollers of the device, such as running IOCs and monitoring systems, and record what you found with the results.
- The reference run had no 10 s rest between the two devices in phases 1 to 3, because they are different hardware. The tool applies the rest.
