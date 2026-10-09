# Startup configuration reference

The independent module loads complete JSON definitions before binding or IOC
initialization. This reference describes schema 1, file validation, commands,
and the ownership boundary.

## Scope and runtime boundary

Configuration, immutable bindings, owned values, native capability observation,
address queues, private IPC and supervised native workers are implemented.
Eleven record DSETs and checked conversion are available through the owned
worker path. Loading configuration and device initialization
perform no request; iocInit starts configured workers, and record/component
admission sends application GET/SET. Records select an existing JSON definition
with `DTYP="snmp3"` and `@binding=<id> deadline_ms=<1..600000>` on INP/OUT.
Both keys are mandatory and frozen at initialization. The
[record contract](snmp-rewrite-contract.md#record-support-contract)
defines supported records, conversion and live-field behavior. See the
[worker reference](snmp-worker-supervision.md) for queue and lifecycle ownership.

The `deadline_ms` budget covers queueing, worker relaunch, IPC and native
service from the admission of each request, and a request queued behind another
keeps the deadline of its own admission. A request ends at the earlier of its
native outcome and its deadline: the native service time of a lost response is
set by the profile's `timeoutMs` and `retries`, and a `deadline_ms` below it ends
the request as a Deadline instead of a native timeout.

Also allow for the time a worker needs to come back after a lost request. After
a Deadline or worker loss the address worker is contained, reaped and relaunched,
and a request queued behind the lost one is dispatched only if its own budget
lasts until the new worker is Ready. For a production (ordinary) build that time
is about 0.4 s (362 to 403 ms over the recorded runs), including the first 250 ms
restart backoff step; an instrumented (ASan/UBSan qualification) build needs about
0.7 s (710 to 745 ms); a worker that does not react to its closed channel needs
about 1.8 s on an ordinary build and longer on an instrumented one. Each further consecutive
failure, without a matching Retired frame in between, lengthens the backoff to
500, 1000, 2000 and then 4000 ms (see the
[worker reference](snmp-worker-supervision.md)), so a budget must also cover the
backoff step in effect. In the measurement, a budget of 1000 ms dispatched the
queued request in each of three runs of the deadline-queue case on both build
types, covering only the first backoff step and leaving about 260 ms to spare on
instrumented builds. A smaller budget lets the queued request expire unsent, which
the record reports as COMM/INVALID with AMSG `deadline before send`.

A plain put to a Passive record, or a write to PROC, made while a request is
pending is held as RPRO and reaches the device after that request ends, up to
about twice the budget after the put when the request ends in a Deadline. A
Channel Access put-callback is held by Base until the record completes, then
sends its own request, so a single put-callback completes within the same bound
and each further put-callback queued on the same record adds up to one budget;
read the outcome from the record's STAT, SEVR and AMSG fields. The measurements
are in the [record contract](snmp-rewrite-contract.md#deadline-path). Queue limits
must cover two generations of each record handle that can hold a successor; the
[worker reference](snmp-worker-supervision.md) gives the sizing rule.

## Strict JSON and root fields

The parser uses YAJL bundled with EPICS Base 7.0.10. It explicitly disables
JSON5 and comments. Duplicate keys, trailing data, invalid UTF-8, malformed
input, unknown fields, and wrong types fail the entire load.
Input bytes and decoded string values/keys must form valid Unicode scalar
encodings. Overlong sequences, encoded surrogate code points, unpaired
surrogate escapes, and values above U+10FFFF are rejected. Valid escaped
surrogate pairs are accepted.

Integer options require unsigned decimal JSON integers. Values such as `1.0`,
`1e3`, `true`, and `"1"` are rejected. JSON strings have no macro or environment
expansion. Strings used by the schema reject bytes below 32 and byte 127.

| Root field | Required type | Constraint |
| --- | --- | --- |
| `schema` | Integer | Exactly `1` |
| `profiles` | Array of objects | At most 4096 definitions |
| `endpoints` | Array of objects | At most 4096 definitions |
| `bindings` | Array of objects | At most 4096 definitions |

All arrays are required; empty arrays are valid. The input limit is 1 MiB
(1048576 bytes), including whitespace. Nesting is limited to 32 containers;
each JSON array or object is limited to 4096 entries.

Definition IDs contain 1-64 ASCII bytes from `A-Z`, `a-z`, `0-9`, `_`, `-`, and
`.`. IDs must be unique within their definition array. References resolve only
within the complete candidate; a missing referenced ID rejects the candidate.

## Profile fields and defaults

| Field | Type | Default or requirement |
| --- | --- | --- |
| `id` | String | Required definition ID |
| `version` | String | Required: `"1"`, `"2c"`, or `"3"` |
| `timeoutMs` | Integer | Default 1000; range 1-60000 |
| `retries` | Integer | Default 3; range 0-10 |
| `maxVarbinds` | Integer | Default 32; range 1-1024 |
| `checkRanges` | Boolean | Default `true` |

Version 1 and 2c require `communityFile`. They reject every v3 field listed
below. The native adapter uses timeout, retry and batch settings. Configuration
loading itself executes no request and converts no record field.

Version 3 rejects `communityFile`. It requires `user` and `securityLevel`.
The `user` string contains 1-32 UTF-8 bytes; `contextName` defaults to empty
and permits at most 255 UTF-8 bytes.

| `securityLevel` | Required fields | Rejected unused fields |
| --- | --- | --- |
| `noAuthNoPriv` | `user`, `securityLevel` | All auth and privacy fields |
| `authNoPriv` | Also `authAlgorithm`, `authSecretFile` | Privacy fields |
| `authPriv` | Also `authAlgorithm`, `authSecretFile`, `privAlgorithm`, `privSecretFile` | None |

Algorithm strings must exactly match a name advertised by the selected native
helper. There is no fallback to another algorithm. The helper is observed for
every successful load, including version 1, version 2c, and empty configurations.

Optional v3 fields are `contextName`, `securityEngineId`, and `contextEngineId`.
An explicit engine ID is 5-32 bytes encoded as contiguous hexadecimal digits.
Hexadecimal case is ignored; `0x` and `0X` prefixes are accepted. Empty,
odd-length, non-hexadecimal, all-zero, and all-FF IDs are rejected.

Omitting `securityEngineId` declares native discovery when the separate adapter
opens a session. Omitting `contextEngineId` declares inheritance from the native
resolved security engine ID. Security and context IDs remain distinct owned
byte arrays. Loading these declarations performs no discovery exchange.

## Endpoint fields and references

| Field | Type | Default or requirement |
| --- | --- | --- |
| `id` | String | Required definition ID |
| `address` | String | Required numeric IPv4 or IPv6 address |
| `port` | Integer | Default 161; range 1-65535 |
| `profile` | String | Required existing profile ID |

The module normalizes numeric addresses with `inet_pton` and `inet_ntop`.
Hostnames, IPv6 scope suffixes, and transport-prefixed strings are rejected.
Address and port are stored separately. Multiple endpoints can retain distinct
profile and context identities for the same address.

## Binding fields and value types

| Field | Type | Default or requirement |
| --- | --- | --- |
| `id` | String | Required definition ID |
| `endpoint` | String | Required existing endpoint ID |
| `oid` | String | Required numeric dotted OID |
| `operation` | String | Required `get` or `set` |
| `valueType` | String | Required type from the table below |
| `capacity` | Integer | Default 1; bounded by value type |

An object identifier (OID) has 2-128 unsigned 32-bit arcs. A leading dot is
optional. The first arc is 0, 1, or 2; the second is at most 39 when the first
is 0 or 1. Empty arcs, negative values, and symbolic names are rejected.

| `valueType` | Capacity limit | Owned value representation |
| --- | --- | --- |
| `integer` | 1 | Signed 64-bit integer |
| `unsigned32`, `counter32`, `gauge32`, `timeticks` | 1 | Unsigned 32-bit value with distinct tag |
| `counter64` | 1 | Unsigned 64-bit value |
| `octets` | 1-1048576 bytes | Owned byte vector, including NUL |
| `oid` | 1-128 arcs | Owned unsigned 32-bit arc vector |
| `ipAddress` | 1 | Four-byte IPv4 value |
| `opaqueFloat` | 1 | Float32 with retained bit pattern |
| `opaqueDouble` | 1 | Float64 with retained bit pattern |

Capacity is binding metadata; it does not create an EPICS waveform or define
record conversion policy. The Value API also represents `NoSuchObject`,
`NoSuchInstance`, and `EndOfMibView` exception tags. These response tags are
not configurable binding value types. Checked accessors reject mismatched tags.

## Secret files and permissions

`communityFile`, `authSecretFile`, and `privSecretFile` contain raw secret bytes,
not JSON strings. A relative path resolves against the directory of the
configuration file after `realpath` resolution. An absolute path is accepted.
The configuration file can be a symlink; the secret file's final path component
cannot be a symlink.

Each secret is opened once with `O_NOFOLLOW`, `O_CLOEXEC`, and `O_NONBLOCK`.
The module checks the opened descriptor with `fstat` and reads that descriptor.
The file must be regular, owned by the effective IOC user, and readable by that
user. Group and other permission bits must all be zero; mode `0600` satisfies
this requirement. Directories, FIFOs, missing files, and unreadable files fail.

The file size limit is 1026 bytes, allowing a 1024-byte passphrase plus CRLF.
One final LF, with an optional preceding CR, is removed. Remaining control
bytes, NUL, and DEL are rejected. Community length is 1-255 bytes; authentication
and privacy passphrases contain 8-1024 bytes after newline removal.

The immutable configuration owns the actual secret bytes. Reports and module
errors do not include secrets or parser input snippets. Locked memory and
complete zeroization are outside this implementation's guarantees.

## Native capability helper boundary

`snmp3NativeProbe --capabilities` emits schema-1 JSON containing the native
version and authentication/privacy entries with `name`, `type`, and `oid`.
The separate executable links Net-SNMP; the IOC and support library remain
Base-only. Build-time selection uses `NET_SNMP_CONFIG`, defaulting to
`net-snmp-config`.

The helper enumerates native APIs after `sc_init`. It advertises authentication
only after a real `sc_hash` succeeds, and privacy only after a real `sc_encrypt`
succeeds. It reads no Net-SNMP user configuration and opens no SNMP session.
Its advertised catalogue depends on the selected installed native library.

`snmp3Load` requires an absolute helper path. It launches the helper with
`posix_spawn`, without a shell or secret arguments, and bounds stdout to 64 KiB
and execution to five seconds. Child failure, invalid output, excessive output,
or timeout rejects the complete candidate; the module waits for child cleanup.
Helper stdin and stderr use `/dev/null`.

The configuration retains the helper path, version, algorithm types, and OIDs.
The separate native adapter compares that complete canonical catalogue with its
shared native implementation before discovery/session open. Actual binary
identity is an additional verification observation, not a schema field. The
[native transport reference](snmp-native-transport.md) defines process-lifetime
USM compatibility and native representation restrictions.

## Publication and freeze conditions

`snmp3Load(config, nativeProbe)` validates a candidate outside the Config
state mutex. It publishes the entire candidate and increments revision under
one mutex after checking frozen state again. Failure preserves the exact
published snapshot and revision. Successful loads before freeze replace all
definitions; they never merge them.

The initial snapshot is empty with revision 0 and `frozen=0`. The first successful
module API `Config::bind(id)` freezes all settings and returns a binding that
owns its configuration. A failed lookup does not freeze. The Base
`AfterFinishDevSup` hook also freezes before module readiness and initial PINI.
Both `iocBuild` and `iocInit` reach this hook.

`snmp3Stop` freezes configuration even before IOC initialization, then closes admission, contains workers
and joins the servicing thread. Unsettled ownership remains IncompleteStopped. Stop, isolated database cleanup, and another database
activation never permit reload. A fresh IOC process is required to use different
definitions. No operator reset, bind, freeze, or replacement command exists.
Record device initialization uses the same immutable Config binding and freeze;
the DSET performs no request until actual record processing.

## IOC commands and reports

| Command | Result |
| --- | --- |
| `snmp3Load(config, nativeProbe)` | Whole-file load or IOC shell error |
| `snmp3ConfigReport` | Revision, frozen flag, and definition counts |
| `snmp3Report` | `owned-worker-transport; recordSupport=available` capability |
| `snmp3RuntimeReport` | Lifecycle, pending reservation and worker state/counters |
| `snmp3Stop` | Freeze, admission closure, worker containment and one thread join |
| `snmp3WorkerPath(path)` | Absolute actual worker product before freeze |
| `snmp3QueueLimit(address,count,bytes)` | Live count 1-16384 and bytes 1-67108864; preserve accepted work |

`recordSupport=available` identifies a compiled feature, not configuration
validity, runtime readiness or device reachability.

The configuration report contains numbers only:

```
snmp3 config: revision=1 frozen=1 profiles=1 endpoints=1 bindings=1
```

Load failure calls `iocshSetError(1)`. A startup script beginning with `on error
break` stops at that failed command and makes the IOC exit with status 1.
Base's default Continue policy can execute later commands and return success;
that exit status does not make the rejected load successful.

## Configuration and startup examples

The shipped [v2c example](../tests/rewrite/config/v2c.json) uses a disposable
community file in its own directory. Supply an existing mode-0600 secret file
owned by the IOC user. The JSON contains no actual credential:

```json
{
    "schema": 1,
    "profiles": [
        {"id": "Community", "version": "2c", "communityFile": "community.txt"}
    ],
    "endpoints": [
        {"id": "Local", "address": "127.0.0.1", "profile": "Community"}
    ],
    "bindings": [
        {"id": "Read", "endpoint": "Local", "oid": "1.3.6.1.2.1.1.3.0", "operation": "get", "valueType": "timeticks"}
    ]
}
```

The shipped [startup example](../tests/rewrite/config/startup.cmd) runs from the
checkout root. Set `CONFIG_JSON` to the prepared configuration path and
`NATIVE_PROBE` to the absolute helper path and `WORKER_PATH` to the absolute
actual worker product through the IOC environment.
Ordinary Base expansion applies to these IOC arguments:

```
on error break
dbLoadDatabase("dbd/snmp3Ioc.dbd")
snmp3Ioc_registerRecordDeviceDriver(pdbbase)
snmp3Load("$(CONFIG_JSON)", "$(NATIVE_PROBE)")
snmp3WorkerPath("$(WORKER_PATH)")
snmp3ConfigReport
dbLoadRecords("tests/rewrite/db/lifecycle.db", "P=Config_")
iocInit
snmp3ConfigReport
```

The standard longin PINI fixture processes constant 42. It demonstrates Base
startup ordering, not SNMP I/O. Reports show revision 1 with one profile,
endpoint, and binding, changing from unfrozen to frozen before PINI.
The [verification runner](../tests/rewrite/README.md) executes this exact shipped
startup file and JSON example with generated disposable secrets.
