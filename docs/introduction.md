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
| Procedures | Configure the module at startup, start the IOC and check the workers, verify the worker transport, verify the native transport, measure device request performance, measure the module path against a fixture agent, measure devices through the module database, size a new device |
| Reference | Startup configuration, module contract and record support, worker commands, limits and timers, native result outcomes and value types, performance test reference, request performance results, glossary |
| Appendix | Design decisions and the PowerNet-MIB review |

The book has no tutorial. Its readers already know the system, and they configure, operate, or maintain an IOC and look up one task or one value at a time. The module also needs site hardware to run end to end, so no environment exists in which a reader can follow one lesson and see every result. The pages are organized by task and by data set.

## Verification status of the module

The module is verified on Debian 13 with EPICS Base 7.0.10. The checks run the shipped IOC, the address workers, and the native SNMP library against loopback agents. They include negative checks that fail when a rule is removed. Grouped requests are verified against a fixture agent. The request performance pages record read-only measurements against two device classes.

These items are not verified yet:

- other operating systems: Rocky Linux 8.10 and 10.2, and Ubuntu 24.04 LTS and 26.04 LTS
- thread sanitizer runs
- sustained resource limits
- the comparison of an APC power distribution unit database against its legacy implementation
