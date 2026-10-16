# Request performance results

The reference run measured the request times of two APC AP8932 power distribution unit classes and of the snmp3 module path. This page lists the measured tables. Each table states its conditions, so you can compare a later run with it.

## Scope

This page lists the measurements of the reference run and of the earlier run with a legacy client present. The findings that follow from them are in [Request performance](evaluation.md). The inputs, rules, and tolerance bands are in the [performance test reference](test-reference.md). The raw data is in [results/reference-run.json](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/snmp-performance/results/reference-run.json).

**Out of scope:** the steps that produce these results (see [Measure device request performance](test-procedure.md)).

Abbreviations on this page: UDP is the user datagram protocol, and AOS is the APC operating system of a management card.

In these tables, RTT is the round-trip time, K is the number of objects in one request, and p50, p95, and max are the median, the 95th percentile, and the maximum. An OID is an object identifier. NMC2 and NMC3 are two generations of the vendor network management card (NMC) of the rack power distribution unit (PDU).

## Conditions of the measurements

Net-SNMP 5.9.4 and the module at commit `03e7af45b7a2542da86af0717a78c51edf55319b` produced the results. The `net-snmp-config` tool reports the Net-SNMP version as 5.9.4.pre2. The tables describe devices by class and firmware only.

| Class | Model | Management card | Card firmware (APC operating system, AOS) | Rack PDU firmware |
| --- | --- | --- | --- | --- |
| NMC2 unit | AP8932 | NMC2 | v7.0.8 | 7.0.8 |
| NMC3 unit | AP8932 | NMC3 | v2.5.3.2 | 2.5.2.5 |

The reference run has no other client on the devices (phases 0 to 4 of the procedure, GET only). An earlier run on the same units was made while a legacy IOC built on the previous module was reading them. The legacy IOC used SNMPv3, one session per device, and 2 s polling. The tables mark that run wherever they quote it, and the table "Effect of a concurrent client" compares the two.

The one-object module table was taken with the legacy client present. The record database run was taken without it. The fixture-agent figures do not involve a device.

The monitoring set has 358 object instances (state 38, measurement 18, slow 302). It follows the record count of a database for this model. A complete readable instance set of the model has 754 objects on the measured layout of 24 outlets, 2 banks, and 1 phase.

## Library and module path on a fixture agent

The table shows the time to complete 100 requests of one integer object over SNMPv3 authPriv, as the median of three runs. The added round trip is a fixed delay on each response through an outer UDP relay.

| Added RTT | Net-SNMP, 100 sequential | Net-SNMP, 100 outstanding | Net-SNMP, 1 request with 100 varbinds | Module path, 100 records (interval per record) |
| --- | --- | --- | --- | --- |
| 0 ms | 6 ms | 4 ms | 1 ms | 3.06 s (30 ms) |
| 20 ms | 2.16 s | 30 ms | 23 ms | 5.14 s (51 ms) |
| 100 ms | 10.2 s | 113 ms | 103 ms | 12.4 s (123 ms) |

## Module path with shortened wake-up waits

The table shows the interval per record of 100 records through the module IOC, worker, and native path, as the median of three sweeps. The agent is the fixture agent, and the protocol is SNMPv3 authPriv. The variant is a copy of the module in which the three fixed 10 ms waits were set to 1 ms: the worker poll, the wait of the servicing thread, and the wait of its failure path. Nothing else differs.

| Added RTT | Module as shipped | Waits set to 1 ms |
| --- | --- | --- |
| 0 ms | 30.8 ms | 3.3 ms |
| 20 ms | 51.1 ms | 23.4 ms |

At zero round trip, 27 of the 31 ms per record belong to the wake-up granularity. The per-record processing of the module is therefore at most 3.3 ms, including what remains of the 1 ms waits. Shortening the waits by polling costs wake-ups at 1 kHz per address, so it is not the fix. The table bounds what waking on readiness can recover.

## Module path against the devices, one object per request

The table shows 100 records with distinct integer objects released together, three sweeps. The request timeout was 3000 ms with no retry. The direct reference reads the same objects sequentially with Net-SNMP. The legacy client was polling the units when this table was taken.

| Unit | Interval per record, module | Time for 100 records, module | Direct sequential, per request | Records alarmed |
| --- | --- | --- | --- | --- |
| NMC2 | 362 to 396 ms | 37.4 to 39.5 s | 195 ms | 0 |
| NMC3 | about 32 ms | 3.2 to 3.3 s | 2.0 ms | 0 |

## Module path with a record database

The table shows the 358 objects loaded as a record database through the module at commit `03e7af45b7a2542da86af0717a78c51edf55319b`. The tiers have 38, 18, and 302 objects with periods of 5, 10, and 60 s. Each tier has one request chain, and the three chains share the address queue. The timeout was 3000 ms with no retry, GET only, 900 s per unit. The legacy IOC was stopped, and no other client was present.

The cycle time runs from the due time of a tier to the reply of its last request. The per-object time is the cycle p50 divided by the objects of the tier. The direct figure is the grouped-request schedule of the table "Sustained schedule results" (K = 32 on NMC2, K = 28 on NMC3).

| Unit | Tier | Cycle p50 / p95 / max | Per object p50 | Overruns | Direct grouped p50 | Module / direct |
| --- | --- | --- | --- | --- | --- | --- |
| NMC3 | state | 1.81 / 2.92 / 2.98 s | 47.7 ms | 0 of 181 | 33 ms | 55 |
| NMC3 | measurement | 1.10 / 1.67 / 1.70 s | 61 ms | 0 of 90 | 41 ms | 27 |
| NMC3 | slow | 14.97 / 15.18 / 15.18 s | 49.6 ms | 0 of 15 | 164 ms | 91 |
| NMC2 | state | 1.84 / 6.73 / 12.81 s | 48 ms | 23 (157 cycles of 181 due) | 757 ms | 2.4 |
| NMC2 | measurement | 3.68 / 5.91 / 7.89 s | 204 ms | 0 of 90 | 929 ms | 4.0 |
| NMC2 | slow | 49.2 / 63.2 / 63.2 s | 163 ms | 1 (13 cycles of 15 due) | 4389 ms | 11 |

NMC3 completed 13316 requests and lost 0. NMC2 completed 13070 requests and lost 6 (0.046 percent). All six losses were isolated, and no device-offline event occurred. At the end, all 358 objects of NMC2 held a valid value. On NMC3, 357 objects did. One enumeration object returned a value outside its enumeration: the outlet layout object, which NMC3 answers with -1 and NMC2 answers with 5. No `tooBig` or protocol error occurred.

On NMC2, the schedule fails through the module. A state request waits behind up to two requests of the other chains, the state tier overran 23 times, and the slow cycle exceeded its period. On NMC3, loss and overruns pass, and the cycles are 27 to 91 times the grouped reads.

The per-object time on NMC3 (48 to 61 ms) is above the 30 ms of the fixture path. The difference is record processing and 10 ms wake-ups, and its share was not isolated. This table is not like-for-like with the one-object table above: the object mix and the queue sharing differ, and the one-object table was taken with the legacy client present.

## Outstanding requests per device

The table shows single-object requests with N in flight over the whole list, three repeats per point. A repeat stops at the first point where two or more requests are lost and they exceed 0.5 percent.

| Unit | In flight and requests lost (requests sent) |
| --- | --- |
| NMC2 | 2: 0 (358), 5: 0 (358), 10: 2 (358); stopped |
| NMC3 | 2: 0 (1074), 5: 0 (1074), 10: 1 (1074), 16: 6 (1074), 20: 0 (716, two repeats before the stop) |

With the legacy client present, the first run found NMC2 losing requests at 10 in flight and NMC3 at 20 (6 of 20 in a burst of 20 at once).

## Group size with one request in flight

The table shows 358 objects and five passes per point.

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

No request was lost in the K = 2 to 48 passes on either unit (0 of 2,210 requests on each). The rule of the procedure gives K\* = 20 for NMC2 (best K = 28) and K\* = 40 for NMC3.

Individual passes vary. On NMC2, the pass of one K ranges up to 26 percent above its minimum (K = 32: 3.39 to 4.27 s). The single-object pass was not part of this run. The run with a legacy client present measured 66.5 s on NMC2 and 748 ms on NMC3 at K = 1 (median request 30 ms and 1.6 ms, means 186 ms and 2.1 ms).

## Requests in flight at selected group sizes

| Unit | K | 1 in flight | 2 in flight | 3 in flight |
| --- | --- | --- | --- | --- |
| NMC2 | 10 | 4.72 s (p95 272 ms) | 4.13 s (p95 459 ms) | 3.85 s (p95 545 ms) |
| NMC2 | 20 | 4.42 s (p95 1030 ms) | 4.49 s (p95 1369 ms) | 4.41 s (p95 1472 ms) |
| NMC2 | 28 | 3.58 s (p95 769 ms) | 3.42 s (p95 874 ms) | 3.51 s (p95 1063 ms) |
| NMC3 | 20 | 184 ms (p95 33 ms) | 161 ms (p95 52 ms) | 174 ms (p95 59 ms) |
| NMC3 | 40 | 123 ms (p95 23 ms) | 156 ms (p95 54 ms) | 117 ms (p95 53 ms) |

More requests in flight shortened the NMC2 pass by 12 and 18 percent at K = 10. At K = 20 and 28, the gain was 5 percent or less, which is inside the pass-to-pass spread. The NMC3 pass did not improve.

## Object order and string content

The table shows the per-object time at K\* (K = 20 on NMC2, K = 40 on NMC3).

| OID order | NMC2 per OID | NMC3 per OID |
| --- | --- | --- |
| inventory order | 10.7 ms | 0.35 ms |
| by column | 10.8 ms | 0.38 ms |
| by row | 10.8 ms | 0.46 ms |
| random | 10.7 ms | 0.48 ms |
| inventory order without 37 string objects | 12.4 ms | 0.42 ms |

## Sustained schedule results

The schedule reads three tiers for 15 minutes per unit with no other client: state (38 objects every 5 s), measurement (18 objects every 10 s), and slow (302 objects every 60 s). One request is in flight, and requests go in the order state, measurement, slow.

| Unit | Tier (OIDs, period) | Cycle p50 / p95 / max | Overruns | Requests lost |
| --- | --- | --- | --- | --- |
| NMC2, K=32 | state (38, 5 s) | 757 / 1700 / 1763 ms | 0 | 0 of 360 |
| NMC2, K=32 | measurement (18, 10 s) | 929 / 1916 / 1924 ms | 0 | 0 of 90 |
| NMC2, K=32 | slow (302, 60 s) | 4389 / 4462 / 4462 ms | 0 | 0 of 150 |
| NMC3, K=28 | state (38, 5 s) | 33 / 70 / 100 ms | 0 | 0 of 360 |
| NMC3, K=28 | measurement (18, 10 s) | 41 / 78 / 113 ms | 0 | 0 of 90 |
| NMC3, K=28 | slow (302, 60 s) | 164 / 282 / 282 ms | 0 | 0 of 165 |

A second client reading `sysUpTime` once per second gave these times:

| Unit | Idle p50 / p95 / max | During the schedule p50 / p95 / max | p95 ratio |
| --- | --- | --- | --- |
| NMC2 | 19 / 342 / 719 ms | 20 / 694 / 1609 ms | 2.03 |
| NMC3 | 2.5 / 19 / 27 ms | 2.5 / 14 / 71 ms | 0.73 |

The idle probe of phase 0 (30 requests, one per second) gave p50 19.1 ms and p95 364 ms on NMC2, and p50 2.5 ms and p95 7.3 ms on NMC3.

## Effect of a concurrent client

The table compares the same units with the legacy IOC polling and with no other client.

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

With the legacy client present, the idle-time test cycled 100 integer objects in 358 requests. It gave per-request times on NMC2 of 195 ms back-to-back, and 334, 371, and 340 ms with 10, 30, and 100 ms of idle time before each request. On NMC3 it gave 2.0, 4.9, 5.9, and 7.0 ms. Without the legacy client, the effect was not reproduced or explained. The clean idle probe at one request per second answered with p50 19 ms on NMC2.

## Cost model fit

The time of one request with K objects fits a line over K = 2 to 48 (reference run, no other client):

| Class | Fixed per request | Per object | Worst error of the fit |
| --- | --- | --- | --- |
| NMC2 | 18 ms | 9.9 ms | 16 percent |
| NMC3 | 2.6 ms | 0.33 ms | 29 percent |

## Prediction against measurement

The table compares the predicted and the measured median cycle times of the reference run. The prediction uses `ceil(n / K) * (fixed + per_object * K)` for each tier. The occupancy is the sum over tiers of the cycle time divided by the period.

| Unit | Cycle per tier | Occupancy predicted | Occupancy measured |
| --- | --- | --- | --- |
| NMC2, K = 32 | state: 0.41 s predicted, 0.76 s measured; measurement: 0.20 s predicted, 0.93 s measured; slow: 3.17 s predicted, 4.39 s measured | 15 percent | 32 percent |
| NMC3, K = 28 | state: 0.02 s predicted, 0.03 s measured; measurement: 0.01 s predicted, 0.04 s measured; slow: 0.13 s predicted, 0.16 s measured | 0.7 percent | 1.3 percent |
