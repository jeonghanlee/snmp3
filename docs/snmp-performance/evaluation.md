# Request performance of the module and devices

The speed at which the snmp3 module reads a monitoring set from an SNMP device depends on three parts: the module, the Net-SNMP library, and the device. This page explains the share of each part, the findings of the reference run, and the factors that decide how many objects to put in one request.

## Scope

This page explains what the measurements show. It covers the findings, the cost model, the limits of the measured module build, the factors that matter when you choose or size a device, and the limits of the evidence.

**Out of scope:** the measured tables (see [Request performance results](results.md)); the inputs, rules, and reference values (see the [performance test reference](test-reference.md)); the steps that produce the measurements (see [Measure device request performance](test-procedure.md)); the steps that size a device (see [Size a new device](sizing-procedure.md)).

Abbreviations on this page: SNMP is the Simple Network Management Protocol.

## Terms used on this page

- A **request** is one SNMP GET that carries K objects. K is the number of objects per request.
- A **pass** reads the whole monitoring set once.
- **In flight** describes requests that were sent and not yet answered.
- A **tier** is a reading priority group of the monitoring set: state, measurement, or slow. The [performance test reference](test-reference.md#format-of-the-object-list) defines the tiers.
- **NMC2** and **NMC3** are two generations of the vendor network management card (NMC) of the rack power distribution unit (PDU). NMC3 is the newer generation. Card firmware tells the units apart: APC operating system (AOS) v7.0.8 and v2.5.3.2. The two firmware numbers follow separate schemes, so the lower number belongs to the newer card.

## Build used for the measurements

The module results come from the module at commit `03e7af45b7a2542da86af0717a78c51edf55319b`. That build sent one object per request and woke on fixed 10 ms waits. The current code differs in both points: it forms grouped requests from declared groups, and it waits on readiness. The section "Limits of the measured module build" lists the differences. The device tables in this book do not yet measure the current build.

## Findings of the reference run

1. **The library is not the limit.** One Net-SNMP session handles 100 outstanding requests, and a request with 100 variable bindings (varbinds), in about one round trip. At 100 ms of added round-trip time, it took 113 ms and 103 ms, against 10.2 s sequentially.
2. **Per-request module overhead dominates fast devices.** In the measured build, the module path takes about 30 ms per record at zero round trip, against 0.05 ms for the library. One request is on the wire at a time per address, with 20 to 30 ms between a completion and the next request.
3. **The two device classes differ by about 16 to 24 times** for the same object tree. With two objects per request, a pass takes 7.15 s on NMC2 and 457 ms on NMC3 (about 16 times). At K = 32 it takes 3.45 s against 143 ms (about 24 times).
4. **Grouping objects into one request gives the largest gain** on both classes. From K = 2 to the best K, the pass falls 2.1 times on NMC2 (3.41 s at K = 28) and 3.7 times on NMC3 (124 ms at K = 40). The reference run did not measure the single-object pass (K = 1). With the legacy client present, it took 66.5 s and 748 ms.
5. **The group size has a response-size ceiling**, not an object-count ceiling. K = 64 lost 5 of 6 requests on NMC2 and returned `tooBig` with 4 of 6 lost on NMC3. K = 48 worked on both. On NMC2, the per-object time rises above K = 32 (11.9 ms at K = 40 against 9.5 ms at K = 28).
6. **A concurrent poller changes NMC2 a great deal and NMC3 little.** With the legacy client stopped, the fixed cost per request of NMC2 fell from 230 ms to 18 ms. Its pass at K = 32 fell from 6.17 s to 3.45 s, and its busy share under the schedule fell from about 50 to about 32 percent. NMC3 did not change beyond the spread of its repeats.
7. **A device loses requests when too many are outstanding.** NMC2 lost 2 of 358 at 10 in flight. NMC3 lost 1 of 1074 at 10 and 6 of 1074 at 16. These runs used single-object requests with no retry. Five outstanding requests were answered fully on both.
8. **The order of the objects made no measurable difference.** Four of the five orders are within 1 percent on NMC2 (10.7 to 10.8 ms per object). On NMC3 the orders span 0.35 to 0.48 ms, inside the spread of its repeats. Groups can therefore follow signal priority.
9. **SNMPv3 authPriv (SHA/AES) costs little.** On the fixture, 100 sequential requests took 5.2 ms over v2c and 6.3 ms over v3. That is about 0.01 ms more per request, small against any device time measured.
10. **The sustained schedule passed on both classes** with no loss and no overrun (state 5 s, measurement 10 s, slow 60 s). NMC2 is busy about 32 percent of the time with it, and NMC3 about 1.3 percent. The second-client p95 ratio on NMC2 was 2.03, at the limit of 2.
11. **Through the measured module build, the same schedule fails on NMC2.** The state tier overran 23 times, and the slow cycle p95 was 63 s against its 60 s period. On NMC3 the cycles take 27 to 91 times the grouped reads. Loss stays below 0.5 percent on both. No other client was present.

## Cost model for planning

The time of one request with K objects fits a line over K = 2 to 48. The line has a fixed cost per request and a cost per object. The [results page](results.md#cost-model-fit) lists the fit for both classes.

The pass time for N objects with K per request and one request in flight is `ceil(N / K) * (fixed + per_object * K)`. For N = 358 and K = 32, this gives 4.0 s on NMC2 (measured 3.45 s) and 0.16 s on NMC3 (measured 143 ms).

The fit is a planning aid. It does not model the tails (request p95 and maximum). With a concurrent poller present, the fixed cost of NMC2 was 230 ms, so size with the figure that matches the clients the device will have. The prediction is a lower bound, because the model has no queueing between tiers.

## Limits of the measured module build

| Limit of the measured build | Effect | Current code |
| --- | --- | --- |
| One object per request: record requests carry different deadlines, and batching needs identical ones | The device fixed per-request cost is paid per object | A declared group batches its members whatever their deadlines. The device tables do not yet measure it. |
| One active request per address with a completion handshake before the next | At least 20 to 30 ms between requests | Still one active batch per address. The servicing loop and the worker wait on readiness. The device tables do not yet measure it. |
| Fixed 10 ms wake-up granularity in the worker and in the servicing thread | About 27 of the 31 ms per record on the fixture path; with 1 ms waits the interval was 3.3 ms | The loops wait on readiness. The idle poll is a 100 ms backstop. |
| Deadline containment restarts the whole address worker | One late reply costs a worker restart and SNMPv3 rediscovery (about 0.4 s) | Unchanged. Keep the deadline above the device request p95 at the chosen size. |

The quantities that follow for a device profile are the objects per request (bounded by response bytes), the requests in flight, the request timeout above the request p95 at that size, and the retries. The profile already carries `maxVarbinds`, `timeoutMs`, and `retries`.

## Factors for choosing or sizing a device

- **Other clients.** One concurrent poller changed the fixed cost of NMC2 by more than 10 times and its busy share under the schedule by a third. Measure with no other client on the device.
- **Group size.** The fixed per-request cost sets the best size, so measure a full pass at several group sizes.
- **Response-size ceiling.** The ceiling is hit by bytes, not by object count. Find it with the largest string objects of the set (phase 1b of the procedure), then stay well below it.
- **Requests in flight.** Find the count at which the device starts to lose requests (phase 1c) and stay below it.
- **Timeout.** Set the request timeout above the request p95 at the chosen size. The module default timeout of 1000 ms is above the NMC2 request p95 at K = 32 without a concurrent poller (0.68 s). It is below the p95 at K = 40 (1.19 s) and below the 1.3 s measured with a concurrent poller present, where it would retransmit constantly.
- **Second client.** The p95 ratio of a second client depends on the idle baseline. It was 2.03 at about a third busy with an idle p95 of 342 ms. It was 1.33 at about half busy with a legacy client present (idle p95 911 ms).
- **Card generation.** Units of one model are not interchangeable for timing, so record the management card generation and firmware with every result.
- **Instances, not definitions.** The readable definitions of the subtree are 264, and the instances on a 24-outlet unit are 754. The monitoring set measured here has 358.
- **Retries.** A retry repeats a lost request and lengthens the pass by its timeout, so include it in the deadline.

## Limits of the evidence

- The reference run covers one day, one pair of units, and five passes per point. The NMC2 passes vary at one point (K = 32: 3.39 to 4.27 s, K = 20: 3.50 to 4.51 s). The same K differs between phases by up to about 20 percent (K = 20 with one in flight: 3.64 s in phase 1, 4.42 s in phase 2). NMC3 varied from 117 to 243 ms at K = 48.
- The legacy-present run and the reference run were made on different dates and in different sequences on the same units. The differences quoted as the effect of the concurrent client do not come from an interleaved comparison.
- The measurements cover only the 358 objects of the monitoring set. Sensors and metered outlets were not attached.
- The idle-gap effect seen with the legacy client present was not reproduced without it. It is neither confirmed nor explained.
- The single-object pass (K = 1) and the idle-time sweep were not repeated without the legacy client.
- Grouped requests were measured with Net-SNMP direct. The module path on the devices is the one-object-per-request figure of the measured build.
- The second client reads only `sysUpTime`. The module-path database run has no second-client probe.
