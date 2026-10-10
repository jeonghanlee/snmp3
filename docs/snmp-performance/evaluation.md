# SNMP Request Performance: Module, Library And Device Selection

## Scope

This document evaluates how fast the snmp3 module can read a monitoring set from SNMP devices,
what part of the time belongs to the module, the library and the device, and what to check when a
device is chosen or sized. The measurement procedure, tools and reference values are in
[test-procedure.md](test-procedure.md); raw reference results are in
[results/reference-run.json](results/reference-run.json).

Measured with Net-SNMP 5.9.4 (`net-snmp-config` reports 5.9.4.pre2) and the module at commit
`03e7af45b7a2542da86af0717a78c51edf55319b`. Devices are described by class and firmware only.

| Class | Model | Management card | Card firmware (AOS) | Rack PDU firmware |
| --- | --- | --- | --- | --- |
| NMC2 unit | AP8932 | NMC2 | v7.0.8 | 7.0.8 |
| NMC3 unit | AP8932 | NMC3 | v2.5.3.2 | 2.5.2.5 |

**Measurement condition.** A legacy IOC built on the previous module (SNMPv3, one session per
device, 2 s polling) was running and reading both units during the device measurements; its traffic
was not excluded. Every device figure below is therefore a figure with a second client present, and
the procedure's prerequisite of no other client was not met. The fixture-agent figures are not
affected. No run with the devices otherwise idle has been made yet.

The monitoring set is 358 object instances (state 38, measurement 18, slow 302), which follows the
record count of a database for this model. A complete readable instance set of the model is
754 objects on the layout measured (24 outlets, 2 banks, 1 phase).

## Terms

- **Request**: one SNMP GET carrying K objects; **K**: objects per request; **pass**: reading the whole
  monitoring set once; **in flight**: requests sent and not yet answered.
- **NMC2, NMC3**: two generations of the vendor's network management card of the rack PDU; NMC3 is
  the newer generation. The units here are told apart by card firmware (AOS v7.0.8 and v2.5.3.2);
  the firmware numbers follow separate schemes, so the lower number belongs to the newer card.
- **Tier**: reading priority group of the monitoring set (state, measurement, slow); see the
  procedure for the definition.

## Findings

1. **The library is not the limit.** One Net-SNMP session handles 100 outstanding requests, and a
   request with 100 varbinds, in about one round trip: 113 ms and
   103 ms at 100 ms added round trip, against
   10.2 s sequentially.
2. **Per-request module overhead dominates fast devices.** The module path takes about 30 ms per
   record at zero round trip, against 0.05 ms for the library; one request is on the wire at a
   time per address, with 20 to 30 ms between a completion and the next request.
3. **Devices of one model differ by 37 to 89 times** for the same object tree: a full sequential
   pass takes 66.5 s on the NMC2 unit and 748 ms on the NMC3 unit (about 89 times); at K = 32
   6.17 s against 168 ms (about 37 times).
4. **Grouping objects into one request gives the largest gain** on both classes: the pass falls to
   5.33 s (NMC2, K=48) and 158 ms (NMC3, K=28).
5. **The group size has a response-size ceiling**, not an object-count ceiling: on the NMC2 unit
   64 objects returned `tooBig` and lost most requests; 48 worked.
6. **Idle time between requests slows the NMC2 unit**: in the idle-time test (100 integer objects
   cycled), back-to-back requests had a median of 44 ms and requests with 10 ms of idle time
   before each had about 280 ms.
7. **A device loses requests when too many are outstanding**: 2 of 10 on the NMC2 unit and 6 of 20
   on the NMC3 unit (single-object requests, no retry); 5 outstanding was answered fully.
8. **Ordering of the objects made no measurable difference**: the five orders differ by 7 percent on
   the NMC2 unit; on the NMC3 unit by up to 27 percent of a 0.4 to 0.5 ms per-object time, with the
   pass-time ranges of the five repeats overlapping. Groups can follow signal priority.
9. **SNMPv3 authPriv (SHA/AES) costs little**: on the fixture, 100 sequential requests took 5.2 ms over
   v2c and 6.3 ms over v3 (about 0.01 ms per request more), small against any device time measured.
10. The sustained schedule (state 5 s, measurement 10 s, slow 60 s) passed on both classes; the NMC2 unit is busy
    about 50 percent of the time with them and the NMC3 unit about 1.4 percent.

## Results

### Library and module path against a fast fixture agent (SNMPv3 authPriv)

Time to complete 100 requests of one integer object, median of three runs; the added round trip
is a fixed delay on each response through an outer UDP relay.

| Added RTT | Net-SNMP, 100 sequential | Net-SNMP, 100 outstanding | Net-SNMP, 1 request with 100 varbinds | Module path, 100 records (interval per record) |
| --- | --- | --- | --- | --- |
| 0 ms | 6 ms | 4 ms | 1 ms | 3.06 s (30 ms) |
| 20 ms | 2.16 s | 30 ms | 23 ms | 5.14 s (51 ms) |
| 100 ms | 10.2 s | 113 ms | 103 ms | 12.4 s (123 ms) |

### Module path against the devices (one object per request)

100 records with distinct integer objects released together, three sweeps; the request timeout was
3000 ms with no retry. Direct reference: the same objects read sequentially with Net-SNMP.

| Unit | Interval per record, module | Time for 100 records, module | Direct sequential, per request | Records alarmed |
| --- | --- | --- | --- | --- |
| NMC2 | 362 to 396 ms | 37.4 to 39.5 s | 195 ms | 0 |
| NMC3 | about 32 ms | 3.2 to 3.3 s | 2.0 ms | 0 |

### Idle time before each request (direct, sequential, single object per request)

The first 100 integer objects of the set, cycled to 358 requests, so the "none" row is not the K = 1
row of the group size table.

| Idle time before each request | NMC2 per request (median request) | NMC3 per request (median request) |
| --- | --- | --- |
| none | 195 ms (44 ms) | 2.0 ms (1.6 ms) |
| 10 ms | 334 ms (286 ms) | 4.9 ms (2.4 ms) |
| 30 ms | 371 ms (310 ms) | 5.9 ms (2.7 ms) |
| 100 ms | 340 ms (276 ms) | 7.0 ms (3.2 ms) |

### Outstanding requests (single object per request, no retry, stop at the first loss)

The first run sent bursts of N requests at once; the kit's ramp (phase 1c) keeps N in flight over the
whole list, which has the same stop condition but is not the identical load shape.

| Unit | Outstanding requests and lost |
| --- | --- |
| NMC2 | 1: 0, 5: 0, 10: 2 |
| NMC3 | 1: 0, 5: 0, 10: 0, 20: 6 |

### Group size, one request in flight (358 objects, five passes per point)

| OIDs per request (K) | Requests per pass | NMC2 pass | NMC2 per OID | NMC2 request p95 | NMC3 pass | NMC3 per OID | NMC3 request p95 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 358 | 66.5 s | 185.9 ms | 570 ms | 748 ms | 2.09 ms | 3 ms |
| 2 | 179 | 38.9 s | 108.8 ms | 598 ms | 464 ms | 1.30 ms | 3 ms |
| 4 | 90 | 23.0 s | 64.3 ms | 638 ms | 328 ms | 0.92 ms | 9 ms |
| 8 | 45 | 14.2 s | 39.6 ms | 940 ms | 274 ms | 0.77 ms | 15 ms |
| 12 | 30 | 10.3 s | 28.9 ms | 1128 ms | 211 ms | 0.59 ms | 14 ms |
| 16 | 23 | 8.90 s | 24.9 ms | 746 ms | 191 ms | 0.53 ms | 19 ms |
| 20 | 18 | 7.97 s | 22.3 ms | 1570 ms | 181 ms | 0.51 ms | 32 ms |
| 24 | 15 | 8.19 s | 22.9 ms | 1588 ms | 180 ms | 0.50 ms | 33 ms |
| 28 | 13 | 6.75 s | 18.9 ms | 1173 ms | 158 ms | 0.44 ms | 33 ms |
| 32 | 12 | 6.17 s | 17.2 ms | 1342 ms | 168 ms | 0.47 ms | 35 ms |
| 40 | 9 | 5.76 s | 16.1 ms | 1362 ms | 180 ms | 0.50 ms | 46 ms |
| 48 | 8 | 5.33 s | 14.9 ms | 1674 ms | 157 ms | 0.44 ms | 39 ms |
| 64 | 6 | stopped: `tooBig` and 4 of 6 requests lost | | | not run | | |

The K = 1 row is five sequential single-object passes over the whole set. Losses in these passes
were 3 of 1790 requests on NMC2 and 0 of 1790 on NMC3. The median request was 30 ms on NMC2 and
1.6 ms on NMC3 against means of 186 ms and 2.1 ms: a minority of slow requests carries the pass
time. The 44 ms of the idle-time table is a different sample (integer objects only).

### Requests in flight at selected group sizes

| Unit | K | 1 in flight | 2 in flight | 3 in flight |
| --- | --- | --- | --- | --- |
| NMC2 | 16 | 8.23 s (p95 674 ms) | 6.49 s (p95 1243 ms) | 5.29 s (p95 1413 ms) |
| NMC2 | 32 | 5.89 s (p95 1344 ms) | 5.23 s (p95 1503 ms) | 4.28 s (p95 1639 ms) |
| NMC2 | 48 | 4.93 s (p95 1192 ms) | 4.80 s (p95 1825 ms) | 4.18 s (p95 2357 ms) |
| NMC3 | 14 | 246 ms (p95 28 ms) | 159 ms (p95 20 ms) | 194 ms (p95 51 ms) |
| NMC3 | 28 | 191 ms (p95 33 ms) | 137 ms (p95 42 ms) | 164 ms (p95 64 ms) |
| NMC3 | 48 | 118 ms (p95 24 ms) | 154 ms (p95 77 ms) | 152 ms (p95 68 ms) |

### Object order and string content at the chosen group size

| OID order | NMC2 per OID (K=32) | NMC3 per OID (K=28) |
| --- | --- | --- |
| inventory order | 17.3 ms | 0.53 ms |
| by column | 17.5 ms | 0.42 ms |
| by row | 16.5 ms | 0.48 ms |
| random | 17.1 ms | 0.50 ms |
| inventory order without 37 string objects | 17.7 ms | 0.43 ms |

### Sustained schedule, 15 minutes per unit

Tiers: state 38 objects every 5 s, measurement 18 objects every 10 s, slow 302 objects every 60 s;
one request in flight, state before measurement before slow between requests.

| Unit | Tier (OIDs, period) | Cycle p50 / p95 / max | Overruns | Requests lost |
| --- | --- | --- | --- | --- |
| NMC2, K=32 | state (38, 5 s) | 1077 / 2043 / 3818 ms | 0 | 1 of 360 |
| NMC2, K=32 | measurement (18, 10 s) | 1520 / 2532 / 4485 ms | 0 | 0 of 90 |
| NMC2, K=32 | slow (302, 60 s) | 7785 / 8874 / 8874 ms | 0 | 0 of 150 |
| NMC3, K=28 | state (38, 5 s) | 33 / 74 / 110 ms | 0 | 0 of 360 |
| NMC3, K=28 | measurement (18, 10 s) | 42 / 92 / 122 ms | 0 | 0 of 90 |
| NMC3, K=28 | slow (302, 60 s) | 169 / 243 / 243 ms | 0 | 0 of 165 |

A second client reading `sysUpTime` once per second:

| Unit | Idle p50 / p95 / max | During the schedule p50 / p95 / max | p95 ratio |
| --- | --- | --- | --- |
| NMC2 | 200 / 911 / 1482 ms | 289 / 1213 / 1938 ms | 1.33 |
| NMC3 | 2.5 / 25.4 / 35.5 ms | 2.6 / 26.6 / 69.6 ms | 1.05 |

## Cost model for planning

The time of one request with K objects fits a line over K = 2 to 48:

| Class | Fixed per request | Per object | Worst error of the fit |
| --- | --- | --- | --- |
| NMC2 | 230 ms | 9.9 ms | 15 percent |
| NMC3 | 2.3 ms | 0.39 ms | 18 percent |

The pass time for N objects with K per request and one request in flight is
`ceil(N / K) * (fixed + per_object * K)`. For N = 358 and K = 32 this gives
6.6 s on the NMC2 unit (measured 6.17 s) and
0.18 s on the NMC3 unit (measured 168 ms).
The fit is a planning aid; the tails (request p95 and maximum) are not modelled.

## What limits the module today

| Limit | Effect | Direction |
| --- | --- | --- |
| One object per request: record requests carry different deadlines and batching needs identical ones | The fixed per-request cost of the device is paid per object | Build requests from several objects: coalescing or a multi-object binding |
| One active request per address with a completion handshake before the next | At least 20 to 30 ms between requests; on an idle-sensitive device the device also answers slower | Send the next request without waiting for the handshake; keep a small number in flight |
| Fixed 10 ms wake-up granularity in the worker and in the servicing thread (read from the source) | Estimated 5 to 10 ms added per request; its share was not isolated by measurement | Wake on socket and pipe readiness |
| Deadline containment restarts the whole address worker | One late reply costs a worker restart and SNMPv3 rediscovery (about 0.4 s) | Keep the deadline above the device's request p95 at the chosen size |

Quantities that follow for a device profile: objects per request (bounded by response bytes),
requests in flight, request timeout above the request p95 at that size, and retries; the profile
already carries `maxVarbinds`, `timeoutMs` and `retries`.


## Sizing a new device

1. Run phase 0 and phase 1 of the procedure with a few group sizes (for example K = 2, 8, 32) to get
   `fixed` and `per_object` of the request time (`tools/summarize_results.py` prints the fit).
2. Predict each tier's cycle time as `ceil(n / K) * (fixed + per_object * K)` and the occupancy of the
   device as the sum over tiers of cycle time divided by period.
3. Run phase 4 to measure it. The prediction is a lower bound: the model has no queueing between
   tiers.

Prediction and measurement of the first run (median cycle times):

| Unit | Cycle per tier | Occupancy predicted | Occupancy measured |
| --- | --- | --- | --- |
| NMC2, K = 32 | state: 0.84 s predicted, 1.08 s measured; measurement: 0.41 s predicted, 1.52 s measured; slow: 5.29 s predicted, 7.78 s measured | 30 percent | 50 percent |
| NMC3, K = 28 | state: 0.02 s predicted, 0.03 s measured; measurement: 0.01 s predicted, 0.04 s measured; slow: 0.14 s predicted, 0.17 s measured | 0.7 percent | 1.4 percent |

4. Accept a schedule when the sustained test passes. The first run accepted an occupancy of about
   50 percent (second client p95 ratio 1.33) and 1.4 percent (ratio 1.05); no occupancy limit was
   measured, so an occupancy between or above these has no evidence either way. To lower it, lengthen
   periods, remove objects from the fast tiers, or keep two or three requests in flight (on the NMC2
   unit at K = 32 a pass was 11 and 27 percent shorter).

## Considerations when choosing or sizing a device

1. Measure the idle single-request time and the back-to-back time before sizing; both can differ
   by more than 5 times on one device.
2. Measure a full pass at several group sizes; the fixed per-request cost sets the best size.
3. Find the response-size ceiling with the largest string objects of the set (phase 1b of the
   procedure), then stay well below it; the ceiling is hit by bytes, not by object count.
4. Find the in-flight count at which the device starts to lose requests (phase 1c) and stay below it.
5. Set the request timeout above the request p95 at the chosen size; the module's default timeout of
   1000 ms is below the NMC2 unit's request p95 at K = 32 (1.3 s) and would retransmit constantly.
6. Measure what a second client sees while the schedule runs; occupancy near half of the time
   raised another client's p95 by about a third.
7. Record the management card generation and firmware with every result; units of one model are
   not interchangeable for timing.
8. Count instances, not definitions: the readable definitions of the subtree are 264 and
   the instances on a 24-outlet unit are 754; the monitoring set measured here has 358.
9. A retry repeats a lost request and lengthens the pass by its timeout; include it in the
   deadline.

## Limits of the evidence

- One day, one pair of units, five passes per point; the NMC2 unit varies by about 10 percent
  between passes and has sporadic timeouts.
- A legacy IOC was polling the same units throughout, so every device figure was taken with a
  second client present; its effect on idle time, idle-gap sensitivity, request loss with several
  requests outstanding and the second-client ratio is not separated from the device's own behavior.
- Only the 358 objects of the monitoring set; sensors and metered outlets were not attached.
- The idle-gap effect is measured, not explained.
- Grouped requests were measured with Net-SNMP direct; the module cannot yet form them, so the
  module path on the devices is the one-object-per-request figure above.
- The second client reads only `sysUpTime`.
