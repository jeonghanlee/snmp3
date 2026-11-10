# The operating problems behind snmp3

The snmp3 module exists because the legacy Simple Network Management Protocol (SNMP) support module for the Experimental Physics and Industrial Control System (EPICS) cannot keep several devices on one input/output controller (IOC) fresh and predictable. This page states the structure of the legacy module, the problems that structure produces in operation, and the evidence for each problem. It is the premise that the rest of this book answers.

## Scope

This page covers the legacy module `devSnmp` as the APC rack power distribution unit (PDU) IOC runs it, the five operating problems observed with it, and where each observation is recorded. It also covers the five structural causes that the complete source review of 2026-11-09 established behind those problems, and what snmp3 guarantees against each cause.

**Out of scope:** the design of snmp3 itself (see [Architecture](snmp-rewrite-architecture.md)), the measured request performance of snmp3 (see [Request performance](snmp-performance/evaluation.md)), the configuration or database of the PDU IOC, and the line-by-line findings of the source review (see [Review record](legacy-review.md)).

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
| 2. One slow device degrades all devices | When one host does not answer, Net-SNMP blocks `snmp_send` for the full retry sequence and the driver holds the shared session mutex for that time, so every PDU in the IOC stops updating. A shorter timeout cuts off a live PDU that answers in 0.8 s and leaves its records stale. The NMC2 host answers in 1.46 to 1.48 s; an NMC3 host answers in 0.07 to 0.09 s. Operators saw slow updates and a different update state per device. | apcpdu commit `9e34314` (startup-file comment) and `cbb9294`; `docs/APCPDU_PVA_STATUS.md`; legacy commit [`68294c2`](https://github.com/jeonghanlee/snmp/commit/68294c2) |
| 3. Record SCAN, driver polling, and session timing are not aligned | A two-second record scanner does not guarantee a new SNMP reply in each scan interval. Values and timestamps change between scanner passes because the driver processes records on replies. The IOC database changed its processing model three times to work around this. | apcpdu `README.md`, Database Processing; commits `b6b6925`, `e4de5e2`, `00a4b09` |
| 4. Requests accumulate behind the mutex | Operators observed a backlog of requests under load. The repositories record only the send-stall bound, not a queue depth or a request that outlives its poll period. | Operator observation, 2026-09; bound in apcpdu commit `9e34314` |
| 5. The time of a value is unknown | A processed record reports a cached value, so processing does not prove a fresh acquisition. Independently polled records are not a simultaneous sample, and no common timestamp exists. At startup, an output readback callback can run before the first valid response and report `LO read error`; 19 such lines appeared one second after initialization on the deployed IOC. | apcpdu `docs/APCPDU_SIGNAL_ANALYSIS.md`, Alarm Contract; `docs/milestone-e4de5e2.md`, D1 and Accepted Limitations; legacy register, Existing Data Path |

The timeout setting of the deployed IOC moved from the `devSnmp` default of 10 s with 5 retries, which stalls the IOC for 60 s on one unanswered host, to 0.5 s, 1 s, and 2 s with one retry between 2026-09-16 and 2026-09-22. Each value trades problem 2 against stale values: a short timeout drops live replies, and a long timeout lengthens the stop of every device.

## Five structural causes

The complete read of the legacy source at `snmp` commit `db9ebf5` and of the Net-SNMP library source found 23 items: the five observed problems above and eighteen further defects, numbered 6 to 23 in the [Review record](legacy-review.md). They reduce to five causes. The item numbers below refer to that record, which cites the source lines for each.

| Cause | What the legacy source does | Items |
| --- | --- | --- |
| A. Shared execution | One send thread, one read thread, and one mutex serve every host of the IOC. Each transaction opens a new Net-SNMP session, and for SNMPv3 that open probes the engineID synchronously inside the mutex and the send thread. Timeout and retries are IOC-wide. Every elapsed time is measured with a clock that only the send thread advances. | 2, 4, 6, 10, 11, 23 |
| B. Request outcomes without a contract | A tooBig or short reply resends the identical request. One `noSuchName` reply excludes an OID until restart. A record waiting for a first value has no deadline. A SET reports success before it is sent and is dropped on timeout. A failed send leaks the request message it built. An authentication failure and a malformed reply both look like a timeout. | 1, 7, 8, 9, 15, 16, 17 |
| C. A shared text cache as the value path | Record processing reads a cached value of any age, and a reply processes every record of its OID a second time. The cache holds the text of `snprint_value`, parsed back through a mask, so the value depends on the host `snmp.conf` and on MIB availability. Passive input records are polled regardless of demand. No field records the time of the sample. | 3, 5, 18, 20, 21, 22 |
| D. Credential handling | Defaults are MD5, AES-128 and `noAuthNoPriv`; only MD5 and SHA-1 are selectable; passphrases stay in plain buffers; the secret file is read without a permission check. | 13 |
| E. Lifecycle and locking | Output readback writes a record field before taking the record lock. The exit hook deletes the host list while the read thread may still be in `select`. The stale-session deletion relies on member destruction order while the library calls back into the object. | 12, 14, 19 |

## Why a new module instead of a fix

Causes A, B and C are one structure: a shared cache that record processing reads, a shared mutex that serializes all hosts, and polling that no record owns. Changing that structure inside `devSnmp` while it keeps its cached polling path was attempted in the legacy repository in 2026-09. The decision of 2026-09-29 retains the legacy implementation only as a comparator and builds snmp3 on its own contract.

## What snmp3 guarantees against each cause

Each guarantee names the mechanism that implements it, the facility of Net-SNMP 5.8 or later and EPICS Base 7.0.10 it rests on, its state at tag `0.0.1`, and the verification that shows it. The facilities were confirmed present in the Net-SNMP sources of Debian 13, Rocky Linux 8.10 and Rocky Linux 9 and in Base 7.0.10 (see [Review record](legacy-review.md), sections on Net-SNMP and Base 7.0.10).

| Cause | Guarantee | Mechanism and facility | State at `0.0.1` | Verification |
| --- | --- | --- | --- | --- |
| A | A request waits only on its own device address. One blocked, slow or absent address changes neither the IOC thread nor another address. | One worker process and one Net-SNMP single session per address (`snmp_sess_open`, `snmp_sess_async_send`, `snmp_sess_select_info2`, `snmp_sess_read2`); engineID probe controlled with `SNMP_FLAGS_DONT_PROBE`; `timeoutMs` and `retries` per profile; the IOC links Base only. | Implemented. | The prior qualification record (`docs/milestone-5dff352.md` at `6f015f8`) holds the executed loss, late-response, worker-replacement and healthy-address observations; two physical devices on one IOC not yet run. |
| B | Every admitted request ends in exactly one outcome before its own deadline, the record receives the outcome as alarm status and `AMSG`, and no outcome excludes a binding permanently. A tooBig reply is a reported outcome, not a silent retry. | Native outcomes `Complete`, `Timeout`, `SendFailure`, `SecurityFailure`, `ProtocolFailure` with status and index, `ResponseRejected`; `recGblSetSevrMsg`; `deadline_ms` per record. For tooBig: the session `rcvMsgMaxSize` advertised to the agent (Net-SNMP header field `msgMaxSize`, minimum 484) and a bounded split of the batch for that address. | Outcomes, `AMSG` and deadline implemented. `rcvMsgMaxSize` and the split not implemented; `maxVarbinds` is a fixed per-profile value. | `NativeTest` with the fixture agent, which returns `tooBig` on demand; the NMC2 and NMC3 units for the advertised-size behavior. |
| C | A record obtains a value only by its own request, receives it as a wire type converted for its record type, and carries the time of the reply. | Request-driven device support without a cache; `Value` with ASN types and checked `Conversion`; `TSE=-2` with the device support writing `prec->time` from the reply time (Base 7.0.10 `dbCommon`, TSE). | Request path and typed values implemented. Reply time stamp not implemented; records carry the processing time. | `ConversionTest` and `RecordTest`; a time stamp check against the worker's receive time once implemented. |
| D | Secrets enter only from files owned by the IOC's effective user with no group or other permission bits, algorithms are only those the native helper proves by a real hash and encryption call, and SHA-2 and AES-256 are selectable where the agent supports them. | `authSecretFile` and `privSecretFile` checks; `snmp3NativeProbe --capabilities`; `usm_lookup_auth_str`, `generate_Ku`, `netsnmp_extend_kul`; `securityAuthLocalKey` and `securityPrivLocalKey` for retaining localized keys instead of passphrases. | File checks, helper and algorithm lookup implemented. Localized-key retention and the creation and rotation procedure (the work register's secret management unit) not implemented. | `ConfigTest`; the helper output on each target distribution. |
| E | Record fields change only inside Base record processing, and shutdown completes or fails every pending request within a bound before the workers are reaped. | Completion through an `epicsCallback` that takes the record lock, checks `PACT`, and calls `rset->process` (`Request.cpp:140-175`); the runtime drain of pending requests at stop; supervisor reap of worker processes. | Implemented. | `ShutdownTest`, `LifecycleTest`, `RuntimeTest`. |

The [Architecture](snmp-rewrite-architecture.md) page describes the implemented mechanisms. The accepted scope of the current release and its limits are in [Introduction](introduction.md#limits-of-product-acceptance). The rows marked not implemented are the candidates for the next goal in the work register.
