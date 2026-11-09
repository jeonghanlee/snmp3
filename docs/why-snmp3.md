# The operating problems behind snmp3

The snmp3 module exists because the legacy Simple Network Management Protocol (SNMP) support module for the Experimental Physics and Industrial Control System (EPICS) cannot keep several devices on one input/output controller (IOC) fresh and predictable. This page states the structure of the legacy module, the problems that structure produces in operation, and the evidence for each problem. It is the premise that the rest of this book answers.

## Scope

This page covers the legacy module `devSnmp` as the APC rack power distribution unit (PDU) IOC runs it, the five operating problems observed with it, and where each observation is recorded.

**Out of scope:** the design of snmp3 itself (see [Architecture](snmp-rewrite-architecture.md)), the measured request performance of snmp3 (see [Request performance](snmp-performance/evaluation.md)), and the configuration or database of the PDU IOC.

Other abbreviations on this page: OID is an object identifier of SNMP, NMC is the network management card of an APC PDU, and SCAN is the scan field of an EPICS record.

## Where the legacy module runs

The APC PDU IOC loads the `devSnmp` device support, which links the Net-SNMP library into the IOC process. Each PDU is one SNMP host. A 24-outlet PDU exposes 61 to 63 monitored OIDs, or 163 with the setting readbacks loaded. One IOC serves several PDUs: the deployed sector IOC serves three AP8932 units behind two card generations, NMC2 and NMC3, and the laboratory IOC serves one unit of each generation.

## Three clocks that do not meet

The legacy module has three independent timing sources, and no one of them owns a record's value.

| Clock | Owner | What it decides |
| --- | --- | --- |
| Record SCAN | EPICS Base | When a record processes. Processing reads the cached OID value; it does not send a request. |
| OID polling period | `devSnmp` | When the driver sends a GET for an OID. The period derives from SCAN at record creation, and a Passive record still polls at `PassivePollMSec`. A received reply processes every record attached to that OID, whether or not it requested anything. |
| Session timing | Net-SNMP | How long a send blocks and how often it retries: `SessionTimeout` times `SessionRetries + 1` per unanswered host. |

The driver serializes all hosts of one IOC behind one session mutex. The read task holds that mutex across select, read, and timeout, and a blocked send holds it for the whole retry sequence. The driver's poll-rate counters are averages since reset, and reply accounting advances on some no-value and mismatched-OID paths. No field records the time of the last valid sample.

These are the facts of the legacy source at its baseline, recorded in the legacy register, [Existing Data Path](https://github.com/jeonghanlee/snmp/blob/895b990/docs/milestone-db9ebf5.md#existing-data-path).

## Observed problems

The five problems below are what operators and the IOC author observed between 2026-09-15 and 2026-10-09. Each row names the observation and where it is recorded. The apcpdu repository is the IOC application; its documents are cited at commit `b2c4aba`.

| Problem | Observation | Record |
| --- | --- | --- |
| 1. OID request errors | Each card generation rejects an SNMPv3 response above its size limit. The client logs `security service 3 error parsing ScopedPDU`; the NMC3 host also logs `tooBig`, and the NMC2 host times out instead. The NMC2 limit is far lower: 30 OIDs per request fails, 20 passes. After tuning `MAXOIDS` per host, about five messages per hour remain with no confirmed host or cause (47 in 8.05 h on the deployed IOC). | apcpdu `docs/pdu-snmpv3-notes.md`; commits `cbb9294`, `d9df592`; `docs/milestone-e4de5e2.md`, G1 |
| 2. One slow device degrades all devices | When one host does not answer, Net-SNMP blocks `snmp_send` for the full retry sequence and the driver holds the shared session mutex for that time, so every PDU in the IOC stops updating. A shorter timeout cuts off a live PDU that answers in 0.8 s and leaves its records stale. The NMC2 host answers in up to 1.46 s; an NMC3 host answers in 0.08 s. Operators saw slow updates and a different update state per device. | apcpdu commit `9e34314` (startup-file comment) and `cbb9294`; `docs/APCPDU_PVA_STATUS.md`; legacy commit [`68294c2`](https://github.com/jeonghanlee/snmp/commit/68294c2) |
| 3. Record SCAN, driver polling, and session timing are not aligned | A two-second record scanner does not guarantee a new SNMP reply in each scan interval. Values and timestamps change between scanner passes because the driver processes records on replies. The IOC database changed its processing model three times to work around this. | apcpdu `README.md`, Database Processing; commits `b6b6925`, `e4de5e2`, `00a4b09` |
| 4. Requests accumulate behind the mutex | Operators observed a backlog of requests under load. The repositories record only the send-stall bound, not a queue depth or a request that outlives its poll period. | Operator observation, 2026-09; bound in apcpdu commit `9e34314` |
| 5. The time of a value is unknown | A processed record reports a cached value, so processing does not prove a fresh acquisition. Independently polled records are not a simultaneous sample, and no common timestamp exists. At startup, an output readback callback can run before the first valid response and report `LO read error`; 19 such lines appeared one second after initialization on the deployed IOC. | apcpdu `docs/APCPDU_SIGNAL_ANALYSIS.md`, Alarm Contract; `docs/milestone-e4de5e2.md`, D1 and Accepted Limitations; legacy register, Existing Data Path |

The timeout setting of the deployed IOC moved from the Net-SNMP default of 1 s with 5 retries to 0.5 s, 1 s, and 2 s with one retry between 2026-09-16 and 2026-09-22. Each value trades problem 2 against stale values: a short timeout drops live replies, and a long timeout lengthens the stop of every device.

## Why a new module instead of a fix

Problems 2 through 5 are consequences of one structure: a shared cache that record processing reads, a shared mutex that serializes all hosts, and polling that no record owns. Changing that structure inside `devSnmp` while it keeps its cached polling path was attempted in the legacy repository in 2026-09. The decision of 2026-09-29 retains the legacy implementation only as a comparator and builds snmp3 on its own contract.

snmp3 answers each problem with one rule:

- A record's processing admits one owned request and completes with that request's result or an explicit failure. Problems 3 and 5.
- Net-SNMP runs in a separate worker process per device address, so a blocked device cannot hold the IOC or another address. Problems 2 and 4.
- Each request has an admission deadline, bounded batch size, and bounded queue depth per address. Problems 1 and 4.

The [Architecture](snmp-rewrite-architecture.md) page describes these rules as the module implements them. The accepted scope of the current release and its limits are in [Introduction](introduction.md#limits-of-product-acceptance).
