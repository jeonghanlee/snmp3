# Measure devices through the module database

You read the devices through the shipped module: an input/output controller (IOC) with a record database, one binding and one record per object, and the address worker, all on the operator host. The once run (T2) measures the pass time of the monitoring set. The sustained run (T3) measures the schedule with a second client on the device.

## Prerequisites

- The permission of the owner of the devices. A person must be present, as for phase 4 of [Measure device request performance](test-procedure.md).
- The repository built with `make -j4` at its root, and the tools built with `bash docs/snmp-performance/tools/build.sh <bin_dir>`.
- A targets file and an object list, as described in the [performance test reference](test-reference.md#inputs-of-the-measurements).
- An SNMPv3 user with read access only. The module sends GET requests only.
- No other client on the devices. The tool cannot detect other clients, so stop them first and announce the hold to other users of the device.
- A shell at the repository root, because every command below uses paths relative to it.
- Python 3 and the Base installation that `configure/RELEASE.local` names.

## Steps

1. To set the variables once, run this command. Replace each value with an absolute path.

    ```bash
    export T=<targets_file> O=<objects_file> OUT=<results_dir> BIN=<bin_dir> EPICS_BASE=<base_dir>
    ```

2. To create the results directory, run:

    ```bash
    mkdir -p $OUT
    ```

3. To run T2, the once run with its direct pass for each device, run this command from the repository root. Replace `<authorizing_person_and_date>` with the person who authorized the run and the date.

    ```bash
    python3 docs/snmp-performance/tools/run_module_database.py --output $OUT/mod-t2 --once --targets $T --objects $O --direct-binary $BIN/device_sweep --confirm-authorized "<authorizing_person_and_date>"
    ```

4. To run T3, the sustained run with its idle phase and its second client, run:

    ```bash
    python3 docs/snmp-performance/tools/run_module_database.py --output $OUT/mod-t3 --targets $T --objects $O --probe-binary $BIN/device_sweep --confirm-authorized "<authorizing_person_and_date>"
    ```

5. To get the direct comparison for T3, run phase 4 of the device tool in the same session, with 10 s between devices:

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase4 --targets $T --oids $O --out $OUT --tools $BIN
    ```

6. To print the report tables of the direct phase, run:

    ```bash
    python3 docs/snmp-performance/tools/summarize_results.py $OUT
    ```

Optional: to record the resources of the IOC and the worker during T3, add `--sample-seconds 10` to the command of step 4. The run then writes `resources.csv` and `counters.csv` into each case directory. The [resource sampling](test-reference.md#resource-sampling) section lists the columns.

A T2 run takes about 7 minutes per device with a second-generation network management card (NMC2) and about 3 minutes per device with a third-generation card (NMC3). A T3 run takes about 17 minutes on NMC2 and 21 minutes on NMC3. Phase 4 takes about 16 minutes per device. The [time budget](test-reference.md#time-budget-of-a-full-run) lists the details.

## Verification

Read these fields of `module-results.json` in the output directory of each run. The [result fields](test-reference.md#result-fields-to-read) table of the reference explains each field.

- T2: `pass_p50_ms`, `valid_passes`, `direct_p50_ms`, and `module_to_direct_ratio`. The ratio must be at most 1.5.
- T3: `schedule.overruns` must be 0, `schedule.loss_percent` must be below 0.5, and the `second_client.p95_ratio` must be at most 1.25 times the direct ratio of `phase4.json`.
- Both: `stopped` must be `null` and `invalid` must be `false`. For a device run, `authorization` must name the authorizing person and the date. A fixture run leaves `authorization` as `null`.

The exit code of the tool is 0 for a completed run, 1 for an error, 10 for a stop, and 12 for an invalid run. The [stop rules](test-reference.md#stop-rules-of-the-module-tool) table of the reference lists what to do for each stop.

A module run on a tree before declared groups, with `--groups off`, is the module baseline of T2 and T3. It is expected to exceed the 1.5 ratio. The fixture diagnostic gives a ratio of about 26. Record such a run as the baseline.
