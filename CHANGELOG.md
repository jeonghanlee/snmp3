# Changelog

## 0.0.1 — Baseline Release

First tagged state of the snmp3 module. It fixes the accepted scope and its limits as the reference point for later versions; it is not a completion claim.

### Product

- EPICS Base 7.0.10 device support for eleven record types, with the IOC linking only Base: Net-SNMP runs in one separate worker process per device address.
- Strict JSON startup configuration with per-file secret references, SNMP v1, v2c and v3 (USM) profiles, and a native capability probe.
- Per-address scheduling with bounded admission, declared request groups, deadlines and owned completion; a blocked device cannot stall IOC servicing or another address.
- Supervised worker lifecycle: bootstrap handshake, framed IPC, deadline containment, exact reap, and a stop sequence that drains Base callbacks before shutdown.

### Accepted scope

- GET-only monitoring of an identified APC PDU application: 385 application checks, 10 selected PVAccess checks and 124 startup checks passed on module `76eee8b`.
- Integration matrix and a 3600-second fixture load on module `c3246fb` across Debian 13, Rocky Linux 8.10 and 10.2, Ubuntu 24.04 LTS and 26.04 LTS; sanitizer products on Debian 13 and Rocky Linux 8.10.
- The product source of this tag is identical to module `76eee8b`.

### Known limits

- Application SET delivery, complete legacy equivalence and physical outage scenarios are not established.
- The single-pass device comparison meets its 1.5 ratio target (observed 1.23); the sustained second-client criterion of 1.25 is not met (observed 1.98). Performance work is deferred.
- The compatibility serial-number LSI record can retain `UDF=1` with a valid value (Soft Channel limitation).
- Shared libraries carry no ABI SONAME yet (#1).

### Documentation

- mdBook with architecture, procedures, references, design decisions and the PowerNet-MIB 4.6.0 review; the work register `docs/milestone-6f015f8.md` holds the product judgment, retrospective and the rule for writing the next goal.
