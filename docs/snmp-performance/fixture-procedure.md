# Measure the module path against a fixture agent

You compare the module path of snmp3 with the Net-SNMP library on a loopback fixture agent that adds a controlled round-trip delay. The fixture needs no device. The first command set measures Net-SNMP sequential, concurrent, and multi-object requests with an added round trip of 0, 20, and 100 ms. The second set runs 100 records through the input/output controller (IOC) of the module, the worker, and the native path. The tools write the JavaScript Object Notation (JSON) result files, and the module needs the Experimental Physics and Industrial Control System (EPICS) Base.

## Prerequisites

- The repository built with `make -j4` at its root. The file `configure/RELEASE.local` names the EPICS Base 7.0.10 installation, and the products land in `bin/linux-x86_64`.
- The tools built with `bash docs/snmp-performance/tools/build.sh <bin_dir>`, as in [Measure device request performance](test-procedure.md).
- Python 3.
- Output directories that do not exist yet.

## Steps

1. To set the variables once, run this command. Replace each value with an absolute path.

    ```bash
    export OUT=<results_dir> BIN=<bin_dir> EPICS_BASE=<base_dir>
    ```

    `OUT` receives the results, `BIN` holds the built tools, and `EPICS_BASE` is the Base installation that `configure/RELEASE.local` names.

2. To create the results directory, run:

    ```bash
    mkdir -p $OUT
    ```

3. To measure Net-SNMP sequential, concurrent, and multi-object requests, run this command from the repository root. It writes `baseline-results.json` and prints one JSON row per run.

    ```bash
    python3 docs/snmp-performance/tools/run_fixture_baseline.py --output $OUT/fx-base --binary $BIN/fixture_baseline
    ```

4. To measure 100 records through the module, run this command. It writes `snmp3-results.json` and prints one JSON row per run.

    ```bash
    python3 docs/snmp-performance/tools/run_fixture_module.py --output $OUT/fx-module
    ```

5. Optional: to measure the Net-SNMP baseline with the tier cut of the object list, run:

    ```bash
    python3 docs/snmp-performance/tools/run_fixture_baseline.py --output $OUT/fx-tiers --binary $BIN/fixture_baseline --group 28 --tiers 38,18,302
    ```

6. Optional: to read the fixture through the module database with one record per object, run:

    ```bash
    python3 docs/snmp-performance/tools/run_module_database.py --output $OUT/fx-db --once --direct-binary $BIN/device_sweep
    ```

7. Optional: to measure the 100-record shape with the module database tool, run:

    ```bash
    python3 docs/snmp-performance/tools/run_module_database.py --output $OUT/fx-flat --shape flat
    ```

8. Optional: to reproduce the interval comparison of the two runners, run the module database tool with the flat shape and 12 sweeps at 0 ms:

    ```bash
    python3 docs/snmp-performance/tools/run_module_database.py --output $OUT/fx-flat12 --shape flat --n 100 --sweeps 12 --delays 0 --versions 2c,3
    ```

9. Optional: to run the second runner of the interval comparison in the same session, run:

    ```bash
    python3 docs/snmp-performance/tools/run_fixture_module.py --output $OUT/fx-old --n 100 --sweeps 12 --delays 0 --versions 2c,3
    ```

You can change the matrix of step 3 with `--n`, `--delays`, `--versions`, and `--repeat`. You can change the matrix of step 4 with `--n`, `--delays`, `--versions`, and `--sweeps`.

## Verification

List each output directory:

```bash
ls $OUT/fx-base/baseline-results.json $OUT/fx-module/snmp3-results.json
```

The command prints both paths without an error:

```text
<results_dir>/fx-base/baseline-results.json
<results_dir>/fx-module/snmp3-results.json
```

The optional module database runs write `module-results.json` in their output directories. Each results file holds one row per run. The `interval_ms` of a sweep is the span between the first and the last update time stamp of the 100 records, divided by 99. Compare the two runners by the medians of their sweeps. The [fixture result variability](test-reference.md#fixture-result-variability) section of the reference explains how much the ratio of the medians can vary at 0 ms.
