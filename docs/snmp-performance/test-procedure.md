# Measure device request performance

You find four values for one device class of the Simple Network Management Protocol (SNMP): the number of objects per request, the number of requests in flight, the request timeout, and the monitoring periods. At these values, a monitoring set is read quickly without slowing the device for other clients. All traffic is SNMP GET, and the procedure writes nothing to a device.

## Prerequisites

- Net-SNMP development libraries and `net-snmp-config`. Record the version with the results.
- `gcc` and Python 3.
- An SNMPv3 user with read access to the monitoring set. The authentication and privacy passphrases each sit in a file of mode 0600. Each file holds the passphrase as plain bytes with at most one trailing newline.
- A targets file and an object list, as described in the [performance test reference](test-reference.md#inputs-of-the-measurements).
- A shell at the repository root, because the commands use paths relative to it.
- A network path from your host to the device. No other client or module generates traffic to the device during a phase.
- The permission of the owner of the device for phase 4. A person must be present for phase 4. You can stop any phase at any time.

## Steps

1. To set the variables once, run this command. Replace each value with an absolute path. Every command below is one line.

    ```bash
    export T=<targets_file> O=<objects_file> OUT=<results_dir> BIN=<bin_dir>
    ```

    `T` is the targets file, `O` is the object list, `OUT` is the results directory, and `BIN` is the directory that receives the tools.

2. To build the tools `device_sweep`, `device_monitor`, and `fixture_baseline` against the installed Net-SNMP, run this command from the repository root:

    ```bash
    bash docs/snmp-performance/tools/build.sh $BIN
    ```

3. To probe the idle device with 30 requests and write the object-order files, run phase 0. It takes about 1 minute per device.

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase0 --targets $T --oids $O --out $OUT --tools $BIN
    ```

4. To sweep the group size K (the number of objects in one request) over 2, 4, 8, 12, 16, 20, 24, 28, 32, 40, and 48 with a window of 1 and 5 repeats, run phase 1. It writes `kstar.json`, which holds the chosen group size K\*. It takes about 10 minutes on a second-generation network management card (NMC2) and 1 minute on a third-generation card (NMC3).

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase1 --targets $T --oids $O --out $OUT --tools $BIN
    ```

5. Optional: when K = 48 shows no error, find the ceiling with larger K values. This writes `phase1b.json` and `kstar-phase1b.json`, not `phase1.json`. It takes about 1 minute.

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase1 --targets $T --oids $O --out $OUT --tools $BIN --ks 64,80,96,128 --tag phase1b
    ```

6. Optional: to ramp the outstanding requests, send single-object requests with 2 to 20 in flight over the whole list. The ramp stops at the first loss and writes `ramp.json`. It takes 1 to 2 minutes.

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase1 --targets $T --oids $O --out $OUT --tools $BIN --ks 1x2,1x5,1x10,1x16,1x20 --tag ramp
    ```

7. To measure windows 1, 2, and 3 at K\*/2, K\*, and the best K, run phase 2. It takes about 7 minutes on NMC2 and 1 minute on NMC3.

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase2 --targets $T --oids $O --out $OUT --tools $BIN --kstar $OUT/kstar.json
    ```

8. To compare the orders inventory, by column, by row, random, and without strings at K\*, run phase 3. It takes about 9 minutes on NMC2 and 1 minute on NMC3.

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase3 --targets $T --oids $O --out $OUT --tools $BIN --kstar $OUT/kstar.json
    ```

9. To run the sustained schedule of 900 s with the second client at periods of 5, 10, and 60 s, run phase 4. Announce the hold to any other user of the device first. It takes about 16 minutes per device.

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase4 --targets $T --oids $O --out $OUT --tools $BIN
    ```

10. To print the report tables in Markdown, run:

    ```bash
    python3 docs/snmp-performance/tools/summarize_results.py $OUT
    ```

## Verification

List the results directory:

```bash
ls $OUT
```

The directory holds one JavaScript Object Notation (JSON) file for each phase you ran:

```text
kstar.json  phase0.json  phase1.json  phase2.json  phase3-<name>.json  phase4.json
```

The optional steps add `phase1b.json` and `ramp.json`.

A phase that stopped shows a step with return code 10 on the console and a `stopped` entry in its JSON. The tool keeps the rows measured before the stop and skips the device for the rest of that phase. Find the cause before you rerun the stopped device.

Compare the median of the repeats with the tolerance bands of the [performance test reference](test-reference.md#reference-values-and-tolerance-bands). The report from step 10 lists the K\* decision, the pass time by K, the window table, the order table, the sustained table with the second client ratio, and the model fit. Apply the [decision rules](test-reference.md#decision-rules-for-results) to choose K\*, W, and the timeout.
