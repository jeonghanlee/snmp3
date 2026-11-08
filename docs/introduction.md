# Documentation for the snmp3 module

snmp3 is a module for the Experimental Physics and Industrial Control System (EPICS). It reads and writes objects of the Simple Network Management Protocol (SNMP) from records of an input/output controller (IOC). It runs the Net-SNMP library in separate worker processes, one per device address, and keeps the IOC free of Net-SNMP code. This book explains how the module is built, configured, supervised, and verified.

## Scope

This book covers the architecture of the module, the startup configuration, the record support contract, worker supervision, the native SNMP transport, and the measured request performance. It also holds the design decisions behind them.

**Out of scope:** the migration of an existing IOC database to snmp3, which the work register tracks outside this book, and the legacy SNMP support module.

## Readers of this book

- **IOC engineers** configure the module and write records against it. Start with the procedures [Configure the module at startup](configure-procedure.md) and [Start the IOC and check the workers](start-ioc-procedure.md). Then read [Startup configuration](snmp-rewrite-config.md) and [Module contract and record support](snmp-rewrite-contract.md).
- **Module developers** change the module. Start with [Architecture](snmp-rewrite-architecture.md) and [Address worker supervision](snmp-worker-supervision.md), then read the design decisions in the appendix.
- **Performance engineers** measure request times against devices. Start with [Request performance](snmp-performance/evaluation.md) and [Measure device request performance](snmp-performance/test-procedure.md).

## How the book is organized

| Part | Pages |
| --- | --- |
| Concepts | Architecture, address worker supervision, record request behavior, native SNMP transport, request performance |
| Procedures | Configure the module at startup, start the IOC and check the workers, verify the worker transport, verify the native transport, measure device request performance, measure the module path against a fixture agent, measure devices through the module database, size a new device, build the documentation book |
| Reference | Startup configuration, module contract and record support, worker commands, limits and timers, native result outcomes and value types, performance test reference, request performance results, glossary |
| Appendix | Design decisions and the PowerNet-MIB review |

The book has no tutorial. Its readers already know the system, and they configure, operate, or maintain an IOC and look up one task or one value at a time. The module also needs site hardware to run end to end, so no environment exists in which a reader can follow one lesson and see every result. The pages are organized by task and by data set.

## Verification status of the module

Verification applies to identified builds and executed paths. The checks exercise the shipped IOC, address workers, native SNMP library, and Base records. Negative controls demonstrate failures when selected rules are removed.

| Qualified product | Environment | Verified scope |
| --- | --- | --- |
| Module `c3246fb`, EPICS Base 7.0.10 | Debian 13, Rocky Linux 8.10 and 10.2, Ubuntu 24.04 LTS and 26.04 LTS | Defined integration matrix and a 3600-second fixture load after 300 seconds of idle observation; functional requirements met and resource trends recorded |
| Separate instrumented products of `c3246fb` | Debian 13 and Rocky Linux 8.10 | Address sanitizer and undefined behavior sanitizer paths; these products do not supply performance measurements |
| Module `76eee8b`, APC application `96fa207`/`3314b1e` | Identified APC monitoring application with EPICS Base 7.0.10 | Accepted GET-only scope: 385 application checks, 10 selected PVAccess checks, and 124 startup checks |

The [qualification record](https://github.com/jeonghanlee/snmp3/blob/6f015f8aa1d6b8c7c92811d5a87ded0bac9228bf/docs/milestone-5dff352.md) identifies the products, executed cases, controls, and accepted limits. A qualified snapshot does not establish that every later build ran the same matrix.

Current unfinished work and the next entry point are in the [work register](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/milestone-6f015f8.md).

## Limits of product acceptance

- APC acceptance covers GET-only monitoring. It does not establish application SET delivery, complete legacy equivalence, or every physical outage scenario. Module-level SET support has separate verification.
- The APC compatibility serial-number long string input (LSI) record can retain `UDF=1` despite a valid value, timestamp, and alarm state. This observed Soft Channel limitation is distinct from the module's native LSI support; see [record validity](snmp-rewrite-contract.md#mapping-of-failures-to-alarms).
- The [grouped performance comparison](snmp-performance/results.md#grouped-module-comparison-on-a-device) meets the single-pass target and fails the sustained second-client target. It does not establish a production speedup or predictable latency and CPU use.
- Thread sanitizer runs are excluded from qualification. Leak checking and resource acceptance thresholds are not established by the recorded resource baseline.
