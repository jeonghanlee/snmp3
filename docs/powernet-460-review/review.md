# PowerNet-MIB 4.6.0 Review for the AP8932 Database

Status: static comparison complete; adoption decision open.
Tracked in `docs/milestone-5dff352.md` (Backlog M13).

## Scope

The AP8932 database of the apcpdu repository is generated from PowerNet-MIB 4.4.6. This review
compares PowerNet-MIB 4.6.0 against the 4.4.6 file for the objects that database reads (the
monitoring set `docs/snmp-performance/inputs/ap8932-oids.tsv`: 358 instances of 91 objects). It does
not regenerate the database, run an IOC, or contact equipment.

## Inputs

| Item | Reference MIB | MIB under review |
| --- | --- | --- |
| File | `PowerNet-MIB` retained by the database repository | `powernet460.mib`, not stored in this repository |
| Version header | 4.4.6 (the file the database pins by hash) | 4.6.0, created 2026-07-06 |
| Size | 2,778,153 bytes | 2,958,837 bytes |
| sha256 | `d6872be58b15e9bad4743e781374219f0758c0b47914069b8e4fb687f8854631` | `5d4d0e4470378082c3dfeb2797d81938d461be6e8a9e2f59002c93564b347e00` |

The 4.6.0 file is the vendor's work and is not stored in this repository. Obtain it from the vendor's
APC PowerNet MIB download and check that its sha256 equals the value above; its own header states
`Version : 4.6.0`. The download page could not be retrieved by the tools used for this review
(access denied), so the publication date shown there is not confirmed here.

## Method

`evidence/compare_mib.py` parses both MIB files itself and reads the monitoring set from the TSV.
For each distinct object it extracts and compares:

- numeric OID (resolved through the parent chain);
- `SYNTAX` including ranges and enumerations;
- `ACCESS`, `STATUS`, `UNITS`, `DESCRIPTION`;
- the parent entry `INDEX` clause;
- a hash of the whitespace-normalized object body.

It also checks that every instance OID of the set lies under its object's OID in both files, and
lists the `rPDU2` objects added, removed or changed between the files.

Re-run, from this directory:

```text
python3 -I evidence/compare_mib.py --old /abs/PowerNet-MIB --new /abs/powernet460.mib --objects ../snmp-performance/inputs/ap8932-oids.tsv --output evidence/compare-result.json
```

## Result for the monitoring set

| Measure | Value |
| --- | --- |
| Monitored instances | 358 |
| Distinct objects | 91 |
| Found in both MIBs | 91 |
| Identical in all compared fields | 91 |
| Changed | 0 |
| Missing in 4.6.0 | 0 |
| Duplicate definitions of a monitored name | 0 |
| Instance OIDs outside their object's OID | 0 |

No monitored object differs in OID, syntax, access, enumeration, units, range, description or index.
The contract that the database derives from the MIB is unchanged between 4.4.6 and 4.6.0.

## rPDU2 changes outside the monitoring set

The `rPDU2` family has 49 added objects, no removed objects, and 4 objects with a changed body. The
revision history in the 4.6.0 file places them as follows; none was introduced by 4.6.0 itself.

| Objects | Count | Revision note | Relation to the AP8932 database |
| --- | --- | --- | --- |
| `rPDU2AdvBank*` (config table, 0.1 A threshold resolution, peak current reset) | 11 | v4.5.0, 2023-07-15 | Parallel to `rPDU2BankConfig*`; sub-ampere threshold resolution |
| `rPDU2AdvPhase*` (config table, 0.1 A threshold resolution, peak current reset) | 11 | v4.5.2 | Parallel to `rPDU2PhaseConfig*` |
| `rPDU2SensorAnalogVoltage*` | 21 | v4.5.6, 2024-11-25 | Sensor family; outside the database scope |
| `rPDU2SensorTempHumidityConfig*` (6 threshold objects added) | 6 | 2026-01-30 entry | Sensor family; outside the database scope |
| `rPDU2SensorTempHumidityConfig*` (4 objects with changed body) | 4 | HumidityLow/Min pair: v4.5.4 description change; TempHigh C/F pair: no revision note names it | Sensor family; outside the database scope |

The only 4.6.0 entry is `upsHighPrecBatteryStateOfHealth`, a UPS object.

The MIB file does not state the minimum PDU firmware that implements the added objects. Objects an
unsupported model lacks respond with `notSupported` according to their descriptions; the firmware
requirement must come from the PDU firmware release notes or from the device.

## Limits

- The comparison is static text and OID resolution of two files, by the comparison tool's own parser.
- The database was not regenerated from 4.6.0, and its integration test was not run against it; the
  passing integration result applies to 4.4.6 only.
- Behavior of physical AP8932 hardware and firmware is not covered.

## Open items

1. Decide whether the database adopts 4.6.0 or keeps 4.4.6. Adoption changes the retained MIB, its
   hash pin, version labels, generated files and the database guide together, and requires
   regeneration, build and a new run of the integration test in a new evidence directory.
2. Decide whether any `rPDU2Adv*` object enters the database scope. This needs a statement of the
   required threshold resolution and the AP8932 firmware that supports the objects.
3. Resolve the monitored numeric OIDs against the 4.6.0 file with an independent translator before
   adoption.
