# Module contract and record support

The snmp3 module guarantees the product and commands, the IOC startup behavior, the record support, the capacity and lifecycle rules, and a stated verification boundary. This page lists those guarantees as exact values.

## Scope

This page covers the module as built and shipped: the commands, the startup behavior of the input/output controller (IOC), the eleven record types, the alarm mapping, the capacity contract, the lifecycle contract, and the verification boundary.

**Out of scope:** the behavior of a record request over time (see [Record request behavior](snmp-record-behavior.md)); the startup configuration fields (see [Startup configuration](snmp-rewrite-config.md)); the worker protocol and timers (see [Worker commands, limits and timers](snmp-worker-reference.md)); the migration of an existing IOC database.

Abbreviations on this page: FLNK is the forward link of a record, PID is a process identifier, JSON is the JavaScript Object Notation, SNMP is the Simple Network Management Protocol, and DTYP is the device type field of a record.

## Product and commands

The default build produces `libsnmp3` and `snmp3Ioc` against EPICS Base 7.0.10. Compilation rejects any other Base major, revision, or modification version. The native-free `libsnmp3Wire` is shared with the separate worker.

The support library registers seven commands:

- `snmp3Report`
- `snmp3RuntimeReport`
- `snmp3Stop`
- `snmp3Load`
- `snmp3ConfigReport`
- `snmp3WorkerPath`
- `snmp3QueueLimit`

The capability line is `owned-worker-transport; recordSupport=available`. It describes the compiled module capability. It is not a record-enable switch, and it does not claim that configuration, startup, or device communication has succeeded. Use the lifecycle report for the runtime state and the record values and alarms for request outcomes.

The module implements owned startup definitions, address admission queues, framed inter-process communication (IPC), and native worker supervision. Component callers use the actual scheduler, worker, and native path. Eleven record device support entry tables (DSETs), checked conversion, and terminal callbacks provide the record support through that same path.

The servicing thread owns deadlines, IPC, and reap, and it never calls native session APIs. The separate capability probe opens no session. The IOC, support, and wire products exclude the native dependencies and the legacy production sources. The standard longin fixture verifies Base processing, not an SNMP request.

The [work register](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/milestone-5dff352.md#current-required-verification-results-2026-10-08) owns the accepted qualification, the observed coverage, and the remaining limitations.

## IOC startup behavior

The IOC uses Base `iocsh` and `epicsExit`. Its exit status follows these rules:

| Situation | Result |
| --- | --- |
| A startup file returns a nonzero `iocsh` result | Exit status 1 |
| More than one file argument | Exit status 2 |
| A startup file returns zero | The IOC accepts interactive commands |
| No startup file | The IOC accepts interactive commands |
| The interactive shell returns | Main calls `epicsExit` |

A startup file must begin with `on error break` to propagate command errors, because the Base default script policy is `continue`. The IOC does not change that policy. A returning startup script also fails when the module retains the Failed state, even under `continue`.

An `exit` command returns from the current `iocsh` invocation. In a startup file, Main checks that return and the retained Failed state. On success it enters the interactive shell.

## Explicit configuration and record bindings

The SNMP interface uses explicit configuration and record bindings. You migrate the startup files and databases of existing IOCs deliberately. The module excludes a legacy syntax translation adapter, and legacy record links need deliberate migration to the explicit grammar. Verification of the legacy sources qualifies only those sources, never this module.

## Record binding syntax

The shipped Value API owns these exact types: signed 64-bit, unsigned 32-bit, Counter64, binary octets, object identifier (OID) arcs, IPv4, Float32, Float64, and exception tags. Accessors check tags. Binding handles own immutable profile and endpoint definitions and their configuration snapshot.

A record uses the device type field `DTYP="snmp3"` and an `INST_IO` link on its input link (INP) or output link (OUT):

```text
@binding=<id> deadline_ms=<ms>
```

Replace `<id>` with a binding identifier from the configuration, and `<ms>` with the deadline budget.

This record reads the binding `IntegerRead` into a longin record with a deadline budget of 5000 ms:

```text
record(longin, "<record_name>") {
    field(DTYP, "snmp3")
    field(INP, "@binding=IntegerRead deadline_ms=5000")
}
```

The binding `IntegerRead` must exist in the loaded configuration, and its value type must fit the record type.

- Exactly these two keys are required.
- The explicit deadline budget ranges from 1 to 600000 ms.
- Device initialization resolves one immutable JSON binding and registers a separate handle for each record. It performs no network I/O.
- The module refuses a live INP or OUT replacement. Admission and completion check the original DTYP, link, DSET, and effective storage identity.
- A direct Base DTYP write can succeed, but a mismatch produces LINK/INVALID and admits no snmp3 request.

## Supported record types

| Record | Native operation and representation |
| --- | --- |
| ai | Numeric GET to direct double VAL; LINR NO CONVERSION; exact integer conversion required |
| longin | Integer-tag GET to signed 32-bit VAL; overflow rejected |
| int64in | Integer-tag GET to signed 64-bit VAL without a double intermediate; Counter64 above INT64_MAX rejected |
| stringin | Octets, OID, or IPv4 GET to at most 39 data bytes; embedded NUL or overflow rejected |
| lsi | The same text GET to effective SIZV storage; LEN includes the terminating NUL |
| waveform | One numeric scalar or octet, OID, or IPv4 array GET; numeric LONG, ULONG, INT64, UINT64, FLOAT, or DOUBLE; octet and IP UCHAR; OID ULONG |
| ao | Numeric SET from Base-prepared OVAL; LINR NO CONVERSION; finite OpaqueFloat rounding uses software roundTiesToEven |
| longout | Integer-tag SET from signed 32-bit VAL; OOPT must remain Every Time |
| int64out | Integer-tag SET from signed 64-bit VAL without a double intermediate |
| stringout | Bounded Octets SET from a terminated 40-byte buffer |
| lso | Octets SET of LEN-1 bytes from effective SIZV storage; terminating NUL excluded |

The table uses these EPICS record fields: VAL (value), LINR (linearization), OVAL (output value), SIZV (string buffer size), LEN (string length), and OOPT (output execute option).

## Conversion and storage rules

- Record conversion rejects range, precision, and capacity loss before it publishes any input or admits an unusable SET.
- For ao-to-OpaqueFloat only, the module accepts finite binary32 rounding without changing the requested VAL. All other precision and capacity loss is an error.
- Input rejection preserves the last complete VAL or buffer pointer (BPTR) and the element count (NORD) or LEN.
- Base clamps the lsi and lso SIZV to 16 to 32767 before binding. The module freezes that effective capacity and the storage pointer.
- Base or client truncation before the DSET is separate from the SNMP conversion.

## Mapping of failures to alarms

| Condition | Alarm |
| --- | --- |
| Local admission or conversion error on an input | READ/INVALID |
| Local admission or conversion error on an output | WRITE/INVALID |
| Native protocol, security, or exception failure | The same READ or WRITE alarm of the operation, with INVALID severity |
| Native timeout, session-open failure, send failure, or cancellation | COMM/INVALID |
| Record deadline, worker loss, or Stopping | COMM/INVALID |
| Invalid identity | LINK/INVALID |

Base owns competing record alarms, monitors, and simulation.

A failed native input keeps the prior undefined flag (UDF) for ai, longin, int64in, stringin, and lsi. A successful input clears it. A waveform follows the UDF processing of Base and separately keeps a native-publication success flag. The BUSY flag is FALSE at initialization, at admission, and at a valid completion before Base record processing. The process-active flag (PACT) represents pending asynchronous work.

## Startup configuration contract

- Schema-1 strict JSON defines complete owned security, profile, endpoint, and binding snapshots.
- A failed load preserves the exact published snapshot and revision.
- A successful load before the freeze replaces the whole configuration.
- A successful first API binding, `AfterFinishDevSup`, or `snmp3Stop` freezes every setting.
- Isolated database cleanup does not permit a reload.
- SNMPv3 configuration accepts native discovery and inheritance declarations and explicit security and context engine IDs.
- Algorithms must be advertised and callable by the selected native helper.
- The separate native library performs discovery and sessions. Loading the configuration performs no network I/O.
- Secret files require ownership by the effective user ID and private permissions. Reports omit secret bytes and identifiers.

The [configuration reference](snmp-rewrite-config.md) defines all fields, limits, commands, and startup examples.

## Capacity limits of the contract

- The defaults are 1024 commands per address and 1048576 charged bytes.
- The atomic live command `snmp3QueueLimit` accepts a count of 1 to 16384 and bytes of 1 to 67108864.
- A decrease preserves accepted work. A later admission must fit both totals.
- All pending states, including consumed terminals that await real retirement or reap, keep the full queue charge (Q) and count.
- Checked frame, capacity, identity, and storage validation precedes publication.

The [worker reference](snmp-worker-reference.md#charge-formulas-for-a-request) gives the exact charge, wire, storage, and timer rules.

## Lifecycle rules of the contract

The runtime states are Cold, Starting, Running, Stopping, Stopped, IncompleteStopped, and Failed. The [worker supervision page](snmp-worker-supervision.md#lifecycle-and-stop) explains them. These rules are part of the contract:

- A start returns after servicing readiness. A duplicate start while Running creates nothing.
- A later isolated activation starts only after the old terminal, native, and process ownership settles.
- A failed preflight or thread creation closes admission and retains the failure. An unexpected service failure also closes admission.
- A stop retains the old PID, context, and reservations when it cannot prove cleanup. IncompleteStopped blocks a start and retains tracking after the join.
- A repeated or concurrent stop does not create a second join.
- A cold stop creates no thread and freezes Config. An empty foundation activation creates no native worker.
- The module has no operator start, credential replacement, or rate reset command.
- One worker per address and one active native batch prevent following work from using a live old slot. A restart requires the exact reap of the old PID and the next epoch.
- The conservative first-Batch-byte recovery rule prevents an ambiguous SET replay. Native retry remains distinct.
- Reports separate the actual lifecycle, the queue ownership, and the worker state. Running does not promise device availability.
- Process-owned Config and Runtime survive isolated cleanup and never permit a reload.
- Readiness waits and joins retain no servicing state lock.
- Base init hooks return void, so the DSET rejects closed admission. Main also checks the retained Failed state after a startup script returns.
- A stop services record completions while Base is alive. Then it closes the atomic completion-entry gate after a 2000 ms drain attempt. The module retains an unsuccessful attempt and blocks a restart.
- Callbacks that have already entered, including those that wait for a record lock, must finish before `AtShutdown` permits link closure. This safety wait can exceed the attempt budget. Total IOC shutdown is not bounded when external processing stalls.
- Shutdown `del_record` detaches pointers without freeing callback storage. A Base callback join alone is insufficient. Isolated cleanup releases abandoned contexts only after the callback queue is actually destroyed. Non-isolated shutdown retains any context that Base can reference.
- Waits hold no operation or record lock that a callback needs.

## Boundary of the verification

The verification runners cover these areas:

- **Foundation.** The shipped foundation runner checks the real build inputs, the generated registration, the actual loader identity, standard-record processing, the startup failures, and the cleanup of owned processes on Linux x86_64. It retains logs and source and product hashes.
- **Lifecycle.** The lifecycle runner adds the real idle-thread, process-initialization (PINI), concurrent stop, isolated reuse, and operating system limit failure cases.
- **Configuration.** The configuration runner adds the shipped Config, Value, and Binding paths, the actual native catalogue and fault children, publication races, the startup freeze, and diagnostic secret checks.
- **Native.** Separate native API and adapter runners exercise actual sessions, ownership, typed GET and SET, discovery and user-based security model (USM) handling, large file-descriptor deadlines, and external faults. The [native transport reference](snmp-native-transport.md) defines that component boundary.
- **Owner, IPC, worker, and IOC.** Separate component, IOC, and qualification runners cover owner, IPC, worker, and native traffic, external faults, and the actual IOC lifecycle.
- **Records.** The separate record runner executes the actual DSET, the Base callbacks, worker and native traffic, and isolated cleanup, including the initial queue-pressure and blocked-shutdown cases. Its executed subset does not close the full record matrix.

Separate address sanitizer (ASan) and undefined behavior sanitizer (UBSan) products qualify the native code for its executed paths. Targeted ASan and UBSan runs also instrument the address queue, IPC, and worker code. Dependencies stay uninstrumented, and leak detection is disabled.

The [record verification reference](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/tests/rewrite/README.md#record-verification) provides executable cases through real Base records, DSET, scheduler, worker, native transport, and agents. The required record qualification covers the eleven record kinds, the checked conversions, the output capture, the alarms, FLNK, reprocessing, simulation, charged ownership, callback pressure, shutdown, and lifecycle. The [qualification record](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/docs/milestone-5dff352.md#current-required-verification-results-2026-10-08) identifies the actual products, receipts, accepted results, and evidence limits.

## Items that are not verified

These items are outside the accepted verification scope:

- Additional Channel Access variants: signed-minimum values, maximum-capacity long strings, and ao and longout access.
- Long active client writes.
- Delayed output simulation-mode changes.
- Broader isolated database reuse.
- Hardware acceptance.
- Qualification on other operating systems.
- Thread sanitizer (TSan) runs, leak checking, and sustained resource acceptance.
- The APC migration.

Available support does not imply that these cases have run or that a particular device is reachable. The work register owns the exact deferred cases and dated limitations.
