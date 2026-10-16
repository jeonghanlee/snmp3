# Size a new device

You find the group size, the request timeout, and the expected device load for a new device class before you put it into a monitoring schedule. The procedure uses the cost model of two short sweeps and confirms the prediction with a sustained run.

Abbreviations on this page: K is the number of objects in one request.

## Prerequisites

- The tools built and the variables `T`, `O`, `OUT`, and `BIN` set, as in [Measure device request performance](test-procedure.md).
- The permission of the owner of the device for the sustained run in step 5. A person must be present.
- No other client on the device.
- A shell at the repository root, because the commands use paths relative to it.

## Steps

1. To measure the idle request time, run phase 0:

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase0 --targets $T --oids $O --out $OUT --tools $BIN
    ```

2. To measure a few group sizes, run phase 1 with the K values 2, 8, and 32:

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase1 --targets $T --oids $O --out $OUT --tools $BIN --ks 2,8,32
    ```

3. To print the fit of the request time as `fixed + per_object * K`, run:

    ```bash
    python3 docs/snmp-performance/tools/summarize_results.py $OUT
    ```

4. To predict the cycle time of each tier, calculate `ceil(n / K) * (fixed + per_object * K)` with the fit from step 3. Then calculate the device occupancy as the sum over the tiers of the cycle time divided by the period.

5. To measure the schedule, run phase 4:

    ```bash
    python3 docs/snmp-performance/tools/run_device_phases.py phase4 --targets $T --oids $O --out $OUT --tools $BIN
    ```

6. Optional: to lower the occupancy, lengthen the periods, remove objects from the fast tiers, or keep two or three requests in flight, and run step 5 again.

## Verification

The schedule passes when no tier overruns its period, lost requests are below 0.5 percent, and the second client p95 ratio is at most 2. The [decision rules](test-reference.md#decision-rules-for-results) list these conditions.

The measured occupancy is higher than the predicted occupancy, because the model has no queueing between tiers. The reference run predicted 15 percent and measured 32 percent on NMC2, and predicted 0.7 percent and measured 1.3 percent on NMC3. See the [prediction against measurement](results.md#prediction-against-measurement) table.

The reference run accepted an occupancy of about 32 percent on NMC2 (second client p95 ratio 2.03, at the limit) and 1.3 percent on NMC3 (ratio 0.73). No occupancy limit was measured, so an occupancy above these values has no evidence. On NMC2 at K = 10, two or three requests in flight shortened a pass by 12 and 18 percent.
