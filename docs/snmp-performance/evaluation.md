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

**Measurement conditions.** The reference run has no other client on the devices (phases 0 to 4 of
the procedure, GET only). An earlier run on the same units was made while a legacy IOC built on the
previous module (SNMPv3, one session per device, 2 s polling) was reading them; it is kept for
comparison in "Effect of a concurrent client" and is marked wherever it is quoted. The module-path
results (one object per request, and the record database) are from the module at the commit above;
the one-object module table was taken with the legacy client present, the database run without it.
The fixture-agent figures do not involve a device.

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
3. **The two device classes differ by about 16 to 24 times** for the same object tree: with two
   objects per request a pass takes 7.15 s on the NMC2 unit and 457 ms on the NMC3 unit (about 16
   times); at K = 32 3.45 s against 143 ms (about 24 times).
4. **Grouping objects into one request gives the largest gain** on both classes: from K = 2 to the
   best K the pass falls 2.1 times on NMC2 (3.41 s at K = 28) and 3.7 times on NMC3 (124 ms at K =
   40). The single-object pass (K = 1) was not measured in the reference run; with the legacy
   client present it was 66.5 s and 748 ms.
5. **The group size has a response-size ceiling**, not an object-count ceiling: K = 64 lost 5 of 6
   requests on NMC2 and returned `tooBig` with 4 of 6 lost on NMC3; K = 48 worked on both. On NMC2
   the per-object time rises above K = 32 (11.9 ms at K = 40 against 9.5 ms at K = 28).
6. **A concurrent poller changes the NMC2 unit a great deal and the NMC3 unit little.** The fixed
   cost per request of the NMC2 unit fell from 230 ms to 18 ms with the legacy client stopped, its
   pass at K = 32 from 6.17 s to 3.45 s and its busy share under the schedule from about 50 to
   about 32 percent; the NMC3 unit did not change beyond the spread of its repeats.
7. **A device loses requests when too many are outstanding**: 2 of 358 at 10 in flight on the NMC2
   unit, and on the NMC3 unit 1 of 1074 at 10 and 6 of 1074 at 16 (single-object requests, no
   retry); 5 outstanding was answered fully on both.
8. **Ordering of the objects made no measurable difference**: four of the five orders are within 1
   percent on the NMC2 unit (10.7 to 10.8 ms per object); on the NMC3 unit the orders span 0.35 to
   0.48 ms, inside the spread of its repeats. Groups can follow signal priority.
9. **SNMPv3 authPriv (SHA/AES) costs little**: on the fixture, 100 sequential requests took 5.2 ms over
   v2c and 6.3 ms over v3 (about 0.01 ms per request more), small against any device time measured.
10. The sustained schedule (state 5 s, measurement 10 s, slow 60 s) passed on both classes with no
    loss and no overrun; the NMC2 unit is busy about 32 percent of the time with it and the NMC3
    unit about 1.3 percent. The second-client p95 ratio on the NMC2 unit was 2.03, at the limit of 2.
11. **Through the module the same schedule fails on NMC2** (state tier 23 overruns, slow cycle p95
    63 s against its 60 s period) and takes 27 to 91 times the grouped reads on NMC3, with no other
    client; loss stays below 0.5 percent on both.

## Results

### Library and module path against a fast fixture agent (SNMPv3 authPriv)

Time to complete 100 requests of one integer object, median of three runs; the added round trip
is a fixed delay on each response through an outer UDP relay.

| Added RTT | Net-SNMP, 100 sequential | Net-SNMP, 100 outstanding | Net-SNMP, 1 request with 100 varbinds | Module path, 100 records (interval per record) |
| --- | --- | --- | --- | --- |
| 0 ms | 6 ms | 4 ms | 1 ms | 3.06 s (30 ms) |
| 20 ms | 2.16 s | 30 ms | 23 ms | 5.14 s (51 ms) |
| 100 ms | 10.2 s | 113 ms | 103 ms | 12.4 s (123 ms) |

### Module path with the wake-up waits shortened (fixture agent, SNMPv3 authPriv)

Interval per record of 100 records through the module IOC, worker and native path, median of three
sweeps. The variant is a copy of the module in which the three fixed 10 ms waits (the worker poll
and the two waits of the servicing thread) were set to 1 ms; nothing else differs.

| Added RTT | Module as shipped | Waits set to 1 ms |
| --- | --- | --- |
| 0 ms | 30.8 ms | 3.3 ms |
| 20 ms | 51.1 ms | 23.4 ms |

At zero round trip 27 of the 31 ms per record belong to the wake-up granularity, so the per-record
processing of the module is at most 3.3 ms (including what remains of the 1 ms waits). Shortening
the waits by polling costs wake-ups at 1 kHz per address and is not the fix; the table bounds what
waking on readiness can recover.

### Module path against the devices (one object per request, legacy client present)

100 records with distinct integer objects released together, three sweeps; the request timeout was
3000 ms with no retry. Direct reference: the same objects read sequentially with Net-SNMP. This
table was taken while the legacy client was polling the units.

| Unit | Interval per record, module | Time for 100 records, module | Direct sequential, per request | Records alarmed |
| --- | --- | --- | --- | --- |
| NMC2 | 362 to 396 ms | 37.4 to 39.5 s | 195 ms | 0 |
| NMC3 | about 32 ms | 3.2 to 3.3 s | 2.0 ms | 0 |

### Module path with the monitoring set as a record database (no other client)

The 358 objects loaded as a record database through the module at commit
`03e7af45b7a2542da86af0717a78c51edf55319b`: tiers of 38, 18 and 302 objects with periods of 5, 10
and 60 s, one request chain per tier, the three chains sharing the address queue. Timeout 3000 ms,
no retry, GET only, 900 s per unit, legacy IOC stopped and no other client. Cycle time runs from the
due time of a tier to the reply of its last request; the per-object time is the cycle p50 divided by
the objects of the tier. The direct figure is the grouped-request schedule of the sustained table
below (K = 32 on NMC2, K = 28 on NMC3).

| Unit | Tier | Cycle p50 / p95 / max | Per object p50 | Overruns | Direct grouped p50 | Module / direct |
| --- | --- | --- | --- | --- | --- | --- |
| NMC3 | state | 1.81 / 2.92 / 2.98 s | 47.7 ms | 0 of 181 | 33 ms | 55 |
| NMC3 | measurement | 1.10 / 1.67 / 1.70 s | 61 ms | 0 of 90 | 41 ms | 27 |
| NMC3 | slow | 14.97 / 15.18 / 15.18 s | 49.6 ms | 0 of 15 | 164 ms | 91 |
| NMC2 | state | 1.84 / 6.73 / 12.81 s | 48 ms | 23 (157 cycles of 181 due) | 757 ms | 2.4 |
| NMC2 | measurement | 3.68 / 5.91 / 7.89 s | 204 ms | 0 of 90 | 929 ms | 4.0 |
| NMC2 | slow | 49.2 / 63.2 / 63.2 s | 163 ms | 1 (13 cycles of 15 due) | 4389 ms | 11 |

Requests completed and lost: NMC3 13316 and 0; NMC2 13070 and 6 (0.046 percent, all isolated, no
device-offline event). Objects holding a valid value at the end: NMC2 all 358; NMC3 357, one
enumeration object returned a value outside its enumeration (the outlet layout object, which that
unit answers with -1; the NMC2 unit answers 5). No `tooBig` or protocol error occurred.

On NMC2 the schedule fails through the module: a state request waits behind up to two requests of
the other chains, the state tier overran 23 times and the slow cycle exceeded its period. On NMC3
loss and overruns pass, and the cycles are 27 to 91 times the grouped reads. The per-object time on
NMC3 (48 to 61 ms) is above the 30 ms of the fixture path; the difference is record processing and
10 ms wake-ups, and its share was not isolated. The table is not like-for-like with the
one-object table above: the object mix and the queue sharing differ, and the one-object table was
taken with the legacy client present.

### Outstanding requests (single object per request, no retry, stop on loss)

Single-object requests with N in flight over the whole list, three repeats per point; a repeat stops
at the first point where two or more requests are lost and they exceed 0.5 percent.

| Unit | In flight and requests lost (requests sent) |
| --- | --- |
| NMC2 | 2: 0 (358), 5: 0 (358), 10: 2 (358); stopped |
| NMC3 | 2: 0 (1074), 5: 0 (1074), 10: 1 (1074), 16: 6 (1074), 20: 0 (716, two repeats before the stop) |

With the legacy client present the first run found the NMC2 unit losing at 10 and the NMC3 unit at
20 (6 of 20 in a burst of 20 at once).

### Group size, one request in flight (358 objects, five passes per point)

| OIDs per request (K) | Requests per pass | NMC2 pass | NMC2 per OID | NMC2 request p95 | NMC3 pass | NMC3 per OID | NMC3 request p95 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2 | 179 | 7.15 s | 20.0 ms | 68 ms | 457 ms | 1.28 ms | 4 ms |
| 4 | 90 | 5.58 s | 15.6 ms | 118 ms | 319 ms | 0.89 ms | 7 ms |
| 8 | 45 | 4.96 s | 13.8 ms | 222 ms | 242 ms | 0.68 ms | 12 ms |
| 12 | 30 | 4.62 s | 12.9 ms | 324 ms | 201 ms | 0.56 ms | 13 ms |
| 16 | 23 | 4.38 s | 12.2 ms | 426 ms | 194 ms | 0.54 ms | 16 ms |
| 20 | 18 | 3.64 s | 10.2 ms | 600 ms | 181 ms | 0.51 ms | 32 ms |
| 24 | 15 | 3.50 s | 9.8 ms | 628 ms | 176 ms | 0.49 ms | 40 ms |
| 28 | 13 | 3.41 s | 9.5 ms | 652 ms | 160 ms | 0.45 ms | 34 ms |
| 32 | 12 | 3.45 s | 9.6 ms | 680 ms | 143 ms | 0.40 ms | 33 ms |
| 40 | 9 | 4.26 s | 11.9 ms | 1191 ms | 124 ms | 0.35 ms | 23 ms |
| 48 | 8 | 3.99 s | 11.2 ms | 1023 ms | 157 ms | 0.44 ms | 39 ms |
| 64 | 6 | stopped: 5 of 6 requests lost | | | stopped: `tooBig`, 4 of 6 lost | | |

No request was lost in the K = 2 to 48 passes on either unit (0 of 2,210 requests on each). The
rule of the procedure gives K\* = 20 for NMC2 (best K = 28) and K\* = 40 for NMC3. Individual passes
vary: on NMC2 the pass of one K ranges up to 26 percent above its minimum (K = 32: 3.39 to 4.27 s).
The single-object pass was not part of this run; the legacy-present run measured 66.5 s and 748 ms
at K = 1 (median request 30 ms and 1.6 ms, means 186 ms and 2.1 ms).

### Requests in flight at selected group sizes

| Unit | K | 1 in flight | 2 in flight | 3 in flight |
| --- | --- | --- | --- | --- |
| NMC2 | 10 | 4.72 s (p95 272 ms) | 4.13 s (p95 459 ms) | 3.85 s (p95 545 ms) |
| NMC2 | 20 | 4.42 s (p95 1030 ms) | 4.49 s (p95 1369 ms) | 4.41 s (p95 1472 ms) |
| NMC2 | 28 | 3.58 s (p95 769 ms) | 3.42 s (p95 874 ms) | 3.51 s (p95 1063 ms) |
| NMC3 | 20 | 184 ms (p95 33 ms) | 161 ms (p95 52 ms) | 174 ms (p95 59 ms) |
| NMC3 | 40 | 123 ms (p95 23 ms) | 156 ms (p95 54 ms) | 117 ms (p95 53 ms) |

More requests in flight shortened the NMC2 pass by 12 and 18 percent at K = 10 and by 5 percent or
less at K = 20 and 28, which is inside its pass-to-pass spread; the NMC3 pass did not improve.

### Object order and string content at the chosen group size

Per-object time at K\* (K = 20 on NMC2, K = 40 on NMC3).

| OID order | NMC2 per OID | NMC3 per OID |
| --- | --- | --- |
| inventory order | 10.7 ms | 0.35 ms |
| by column | 10.8 ms | 0.38 ms |
| by row | 10.8 ms | 0.46 ms |
| random | 10.7 ms | 0.48 ms |
| inventory order without 37 string objects | 12.4 ms | 0.42 ms |

### Sustained schedule, 15 minutes per unit (no other client)

Tiers: state 38 objects every 5 s, measurement 18 objects every 10 s, slow 302 objects every 60 s;
one request in flight, state before measurement before slow between requests.

| Unit | Tier (OIDs, period) | Cycle p50 / p95 / max | Overruns | Requests lost |
| --- | --- | --- | --- | --- |
| NMC2, K=32 | state (38, 5 s) | 757 / 1700 / 1763 ms | 0 | 0 of 360 |
| NMC2, K=32 | measurement (18, 10 s) | 929 / 1916 / 1924 ms | 0 | 0 of 90 |
| NMC2, K=32 | slow (302, 60 s) | 4389 / 4462 / 4462 ms | 0 | 0 of 150 |
| NMC3, K=28 | state (38, 5 s) | 33 / 70 / 100 ms | 0 | 0 of 360 |
| NMC3, K=28 | measurement (18, 10 s) | 41 / 78 / 113 ms | 0 | 0 of 90 |
| NMC3, K=28 | slow (302, 60 s) | 164 / 282 / 282 ms | 0 | 0 of 165 |

A second client reading `sysUpTime` once per second:

| Unit | Idle p50 / p95 / max | During the schedule p50 / p95 / max | p95 ratio |
| --- | --- | --- | --- |
| NMC2 | 19 / 342 / 719 ms | 20 / 694 / 1609 ms | 2.03 |
| NMC3 | 2.5 / 19 / 27 ms | 2.5 / 14 / 71 ms | 0.73 |

The idle probe of phase 0 (30 requests, one per second) gave p50 19.1 ms and p95 364 ms on NMC2
and p50 2.5 ms and p95 7.3 ms on NMC3.

### Effect of a concurrent client (same units, legacy IOC polling, against no other client)

| Quantity | NMC2 legacy present | NMC2 none | NMC3 legacy present | NMC3 none |
| --- | --- | --- | --- | --- |
| Fixed cost per request (fit) | 230 ms | 18 ms | 2.3 ms | 2.6 ms |
| Cost per object (fit) | 9.9 ms | 9.9 ms | 0.39 ms | 0.33 ms |
| Pass at K = 32 | 6.17 s | 3.45 s | 168 ms | 143 ms |
| Best per-object time (K) | 14.9 ms (48) | 9.5 ms (28) | 0.44 ms (48) | 0.35 ms (40) |
| Request p95 at K = 32 | 1342 ms | 680 ms | 35 ms | 33 ms |
| Idle probe p50 | 200 ms | 19 ms | 2.5 ms | 2.5 ms |
| Second client idle p95 | 911 ms | 342 ms | 25 ms | 19 ms |
| State tier cycle p50 | 1077 ms | 757 ms | 33 ms | 33 ms |
| Slow tier cycle p50 | 7785 ms | 4389 ms | 169 ms | 164 ms |
| Busy share under the schedule | about 50 percent | about 32 percent | 1.4 percent | 1.3 percent |
| Requests in flight with first loss | 10 | 10 | 20 | 10 |

With the legacy client present the idle-time test (100 integer objects cycled, 358 requests) gave
per-request times on NMC2 of 195 ms back-to-back and 334, 371 and 340 ms with 10, 30 and 100 ms of
idle time before each request, and on NMC3 of 2.0, 4.9, 5.9 and 7.0 ms. That effect was not
reproduced or explained without the legacy client; the clean idle probe at one request per second
answered with p50 19 ms on NMC2.

## Cost model for planning

The time of one request with K objects fits a line over K = 2 to 48 (reference run, no other
client):

| Class | Fixed per request | Per object | Worst error of the fit |
| --- | --- | --- | --- |
| NMC2 | 18 ms | 9.9 ms | 16 percent |
| NMC3 | 2.6 ms | 0.33 ms | 29 percent |

The pass time for N objects with K per request and one request in flight is
`ceil(N / K) * (fixed + per_object * K)`. For N = 358 and K = 32 this gives
4.0 s on the NMC2 unit (measured 3.45 s) and
0.16 s on the NMC3 unit (measured 143 ms).
The fit is a planning aid; the tails (request p95 and maximum) are not modelled. With a
concurrent poller present the fixed cost of the NMC2 unit was 230 ms; size with the figure that
matches the clients the device will have.

## What limits the module today

| Limit | Effect | Direction |
| --- | --- | --- |
| One object per request: record requests carry different deadlines and batching needs identical ones | The fixed per-request cost of the device is paid per object | Build requests from several objects: coalescing or a multi-object binding |
| One active request per address with a completion handshake before the next | At least 20 to 30 ms between requests | Send the next request without waiting for the handshake; keep a small number in flight |
| Fixed 10 ms wake-up granularity in the worker and in the servicing thread (read from the source) | About 27 of the 31 ms per record on the fixture path (see the wake-up table); with 1 ms waits the interval was 3.3 ms | Wake on socket and pipe readiness |
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

Prediction and measurement of the reference run (median cycle times):

| Unit | Cycle per tier | Occupancy predicted | Occupancy measured |
| --- | --- | --- | --- |
| NMC2, K = 32 | state: 0.41 s predicted, 0.76 s measured; measurement: 0.20 s predicted, 0.93 s measured; slow: 3.17 s predicted, 4.39 s measured | 15 percent | 32 percent |
| NMC3, K = 28 | state: 0.02 s predicted, 0.03 s measured; measurement: 0.01 s predicted, 0.04 s measured; slow: 0.13 s predicted, 0.16 s measured | 0.7 percent | 1.3 percent |

4. Accept a schedule when the sustained test passes. The reference run accepted an occupancy of
   about 32 percent on NMC2 (second client p95 ratio 2.03, at the limit) and 1.3 percent on NMC3
   (ratio 0.73); no occupancy limit was measured, so an occupancy above these has no evidence. To
   lower it, lengthen periods, remove objects from the fast tiers, or keep two or three requests in
   flight (on the NMC2 unit at K = 10 a pass was 12 and 18 percent shorter).

## Considerations when choosing or sizing a device

1. Measure with no other client on the device; one concurrent poller changed the fixed cost of the
   NMC2 unit by more than 10 times and its busy share under the schedule by a third.
2. Measure a full pass at several group sizes; the fixed per-request cost sets the best size.
3. Find the response-size ceiling with the largest string objects of the set (phase 1b of the
   procedure), then stay well below it; the ceiling is hit by bytes, not by object count.
4. Find the in-flight count at which the device starts to lose requests (phase 1c) and stay below it.
5. Set the request timeout above the request p95 at the chosen size; the module's default timeout of
   1000 ms is above the NMC2 unit's request p95 at K = 32 without a concurrent poller (0.68 s), below
   it at K = 40 (1.19 s) and below the 1.3 s measured with one present, where it would retransmit
   constantly.
6. Measure what a second client sees while the schedule runs; the p95 ratio depends on the idle
   baseline: 2.03 at about a third busy with an idle p95 of 342 ms, 1.33 at about half busy with a
   legacy client present (idle p95 911 ms).
7. Record the management card generation and firmware with every result; units of one model are
   not interchangeable for timing.
8. Count instances, not definitions: the readable definitions of the subtree are 264 and
   the instances on a 24-outlet unit are 754; the monitoring set measured here has 358.
9. A retry repeats a lost request and lengthens the pass by its timeout; include it in the
   deadline.

## Limits of the evidence

- One day, one pair of units, five passes per point. The NMC2 unit's passes vary at one point (K =
  32: 3.39 to 4.27 s, K = 20: 3.50 to 4.51 s), and the same K differs between phases by up to about
  20 percent (K = 20 one in flight: 3.64 s in phase 1, 4.42 s in phase 2). The NMC3 unit varied from
  117 to 243 ms at K = 48.
- The legacy-present run and the reference run were made on different dates and sequences on the
  same units; the differences quoted as the effect of the concurrent client are not from an
  interleaved comparison.
- Only the 358 objects of the monitoring set; sensors and metered outlets were not attached.
- The idle-gap effect seen with the legacy client present was not reproduced without it and is
  neither confirmed nor explained.
- The single-object pass (K = 1) and the idle-time sweep were not repeated without the legacy
  client.
- Grouped requests were measured with Net-SNMP direct; the module cannot yet form them, so the
  module path on the devices is the one-object-per-request figure.
- The second client reads only `sysUpTime`; the module-path database run has no second-client probe.
