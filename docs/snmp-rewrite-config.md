# Startup configuration reference

The module loads complete JSON definitions before any binding or IOC initialization. This reference lists the schema 1 fields, the file validation rules, the commands, and the ownership boundary.

## Scope

This page covers the startup configuration of the module: the JSON schema, the secret files, the native capability helper, the publication and freeze conditions, the IOC commands that load and report the configuration, and the shell commands and record links that give the same definitions without a file.

**Out of scope:** the behavior of a record request over time (see [Record request behavior](snmp-record-behavior.md)); the record types and alarms (see [Module contract and record support](snmp-rewrite-contract.md)); the queue limits and timers (see [Worker commands, limits and timers](snmp-worker-reference.md)).

Abbreviations on this page: IOC is the input/output controller, the process of the Experimental Physics and Industrial Control System (EPICS) that runs the records. DSET is the device support entry table of a record type. INP and OUT are the input and output link fields of a record. PINI is the process-at-initialization field. JSON is the JavaScript Object Notation, and YAJL is the Yet Another JSON Library that EPICS Base bundles. UTF-8 is the 8-bit Unicode transformation format. USM is the user-based security model of SNMPv3. SNMP is the Simple Network Management Protocol. GET and SET are its read and write operations.

## Runtime boundary of the configuration

The module implements configuration, immutable bindings, owned values, native capability observation, address queues, private inter-process communication (IPC), and supervised native workers. Eleven record DSETs and checked conversion are available through the owned worker path.

Loading the configuration and initializing a device perform no request. The `iocInit` command starts the configured workers. Record and component admission send the application GET and SET requests.

A record selects an existing JSON definition with `DTYP="snmp3"` and `@binding=<id> [deadline_ms=<1..600000>]` on INP or OUT. The key `binding` is mandatory and `deadline_ms` is optional (see [Module contract and record support](snmp-rewrite-contract.md) for the budget without it); both are frozen at initialization.

The `deadline_ms` budget covers queueing, worker relaunch, IPC, and native service from the admission of each request. A request queued behind another keeps the deadline of its own admission. The profile fields `timeoutMs` and `retries` set the native service time of a lost response.

Choose `deadline_ms` with worker recovery in mind. [Record request behavior](snmp-record-behavior.md#recovery-time-after-a-containment) explains recovery conditions, expiry before dispatch, and the limits of a pending put. Queue limits must cover two generations of each record handle that can hold a successor. The [charge formulas](snmp-worker-reference.md#charge-formulas-for-a-request) give the sizing rule.

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
| `maxMessageSize` | Integer | Default 1472; range 484-65507 |
| `checkRanges` | Boolean | Default `true` |

Version 1 and 2c require `communityFile`. They reject every v3 field listed
below. The native adapter uses timeout, retry and batch settings. Configuration
loading itself executes no request and converts no record field.

`maxMessageSize` is the largest message, in bytes, that the worker session
accepts. An SNMPv3 request for objects carries it as `msgMaxSize`, and the unit sizes its
reply against it. A version 1 or 2c message has no size field, so the value
has no effect on the wire for those versions. The request itself must fit:
with the Net-SNMP agent of the tests and a size of 484, requests of 484 bytes
and more got no reply while requests of 467 bytes and less were answered; the
APC units were not run this way. `maxVarbinds` is the
upper bound of one request; the scheduler lowers the number of values per
request of an address when the unit rejects a request as too large, as
described in [Record behavior](snmp-record-behavior.md).

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
| `group` | String | Optional group label, 1-64 characters from the definition ID alphabet |
| `oid` | String | Required numeric dotted OID |
| `operation` | String | Required `get` or `set` |
| `valueType` | String | Required type from the table below |
| `capacity` | Integer | Default 1; bounded by value type |
| `systemInfo` | Boolean | Default `false`; allowed on a `get` binding only. Marks system information of the unit (firmware revision, name, MAC address, status), the only values still read and shown after the unit at the address shows another engine ID; see [Record behavior](snmp-record-behavior.md) |

An object identifier (OID) has 2-128 unsigned 32-bit arcs. A leading dot is
optional. The first arc is 0, 1, or 2; the second is at most 39 when the first
is 0 or 1. Empty arcs, negative values, and symbolic names are rejected.

A group is the set of bindings that carry the same `group` label. The loader
rejects a configuration in which a group:

- contains a `set` binding;
- spans two endpoints;
- has more bindings than the `maxVarbinds` of the profile of its endpoint;
- would need a request or a largest response above the 2 MiB IPC data limit.

The label is kept with the binding definition. The worker protocol does not
carry it.

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

The helper runs with the environment list of the worker and with no other variable: `PATH=/usr/bin:/bin`, `LANG=C`,
`LC_ALL=C` and `LD_LIBRARY_PATH`, whose value is the sorted, unique directories of the worker's libraries as `ldd`
reports them, joined by `:`. A sanitizer qualification run adds two sanitizer options to the worker only. Call
`snmp3WorkerPath` before `snmp3Load`. A load while no worker path is set, or with a
worker for which `ldd` fails, is rejected before the helper starts. After a successful load a different worker path is
rejected, so the catalogue always comes from the worker that starts.

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
| `snmp3RuntimeReport` | Lifecycle, pending reservation, worker state/counters and the servicing-loop pass count (`snmp3 loop:`; fields in [Runtime report lines](snmp-worker-reference.md#runtime-report-lines)) |
| `snmp3Stop` | Freeze, admission closure, worker containment and one thread join |
| `snmp3WorkerPath(path)` | Absolute actual worker product; call it before `snmp3Load`; a different path after a successful load is rejected |
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

## Configuration by shell commands

The definitions of a file can be given by shell commands instead, or beside a file. A unit is a session under a name; the values a profile holds in a file are options of the session; and a record names its object in its link, so no binding is written anywhere else.

```text
snmp3WorkerPath("<worker_path>")
snmp3ProbePath("<native_probe>")
snmp3Session PDU1 192.0.2.1 version=2c communityFile=<file>
snmp3Session PDU2 [2001:db8::1]:1161 version=3 user=<user> securityLevel=authPriv authAlgorithm=SHA authSecretFile=<file> privAlgorithm=AES privSecretFile=<file>
snmp3Options PDU* timeoutMs=2000 retries=1
snmp3Subscription PDU1Fast PDU1 1000
snmp3Show PDU* 1
```

| Command | Contract |
| --- | --- |
| `snmp3ProbePath(absolutePath)` | The native probe, named before the first session. A second call with another path, and a `snmp3Load` with another probe, are rejected |
| `snmp3Session name address [options]` | Defines a session. `name` follows the rules of a definition ID. `address` is numeric, alone or with a port: `192.0.2.1`, `192.0.2.1:161`, `2001:db8::1`, `[2001:db8::1]:161`; without a port it is 161. The options are those of `snmp3Options` |
| `snmp3Options pattern [options]` | Sets options of every session whose name matches the pattern (`*` and `?`) and exists when the command runs; a session defined later is not touched. A pattern that matches no session is rejected |
| `snmp3Subscription name session period` | Defines a subscription: a set of read objects of one session that the module reads by itself every `period` milliseconds, 100 to 3600000. A name is defined once among sessions and subscriptions. See [Records that process on a reply](snmp-record-behavior.md#records-that-process-on-a-reply) |
| `snmp3Mib path [system]` | Names a directory or one file of MIB modules of the IOC. With the word `system` as second argument the path is a directory that replaces the directories of the system for this IOC. See [Objects by MIB name](#objects-by-mib-name) |
| `snmp3Show pattern verbosity` | Prints one `snmp3 session:` line per matching session and one `snmp3 subscription:` line per matching subscription; with verbosity 1 the lines hold the values, one `snmp3 object:` line follows a session for each object a record defined on it, with `subscription=<name>` when the record reads through one, and the `snmp3 mib:` lines follow. No secret content is printed |

An option is one word `name=value`. The names, defaults, ranges and rules are those of the profile fields above: `version`, `timeoutMs`, `retries`, `maxVarbinds`, `maxMessageSize`, `checkRanges` (`true` or `false`), `communityFile`, `user`, `securityLevel`, `authAlgorithm`, `authSecretFile`, `privAlgorithm`, `privSecretFile`, `contextName`, `securityEngineId`, `contextEngineId`. A secret file is named by its path and must meet the rules of [Secret files and permissions](#secret-files-and-permissions). Every value belongs to one session; none applies to the whole IOC.

Each option is checked when its command runs: an unknown name, a value outside its range, an algorithm the probe does not report, or a secret file that is rejected fails the command. What a version needs as a whole is checked when `iocInit` begins: a session without `version`, a community session with a field of SNMPv3, or an SNMPv3 session without the fields of its security level is left out and the IOC prints `snmp3: session <name>: <reason>`. `iocInit` completes without that session and counts as a failed command: in a startup file that begins with `on error break` no later line runs and the IOC exits with status 1. A failed command prints `snmp3: <reason>`; in a startup file that begins with `on error break` no later line runs.

A file and commands can be used together. `snmp3Load` gives the endpoints of the file as sessions under their `id`, so `snmp3Options` acts on them as well: an endpoint that takes an option gets a profile of its own, and another endpoint that shared its profile keeps the values of the file. An option that the profile of a file endpoint cannot take is rejected at the command and leaves the values of the file. A name is defined once: a session under the name of an endpoint of the file, or a file with an endpoint under the name of a session, is rejected with `name defined twice`. Every definition by command or file is closed when `iocInit` begins; a command after that is rejected with `configuration frozen`.

A record names a session and its object, by a numeric identifier or by a name of a MIB:

```text
record(longin, "<record_name>") {
    field(DTYP, "snmp3")
    field(INP, "@PDU1 1.3.6.1.2.1.1.3.0 type=timeticks")
}
```

| Word of the link | Meaning |
| --- | --- |
| first word | Name of a session, or of a subscription for an input record with `SCAN` set to `I/O Intr` |
| second word | Numeric dotted object identifier, or `MODULE::name.index` (see [Objects by MIB name](#objects-by-mib-name)) |
| `type=<type>` | A value type of the table in [Binding fields and value types](#binding-fields-and-value-types). Required with a numeric identifier. With a name it is optional and then decides the type in place of the MIB |
| `deadline_ms=<ms>` | Optional deadline budget, 1 to 600000 ms. Without it the budget is `2 x timeoutMs x (retries + 1) + 1000` ms of the session, at most 600000 ms |
| `group=<label>` | Optional group label of an input record; the rules of a declared group apply. Not accepted on a record of a subscription |
| `systemInfo` | Optional mark of an input record as system information of the unit |

An input record reads the object and an output record writes it; the size of a value comes from the record, the 40 characters of a `stringin` or `stringout`, the `SIZV` of an `lsi` or `lso`, and the `NELM` of a `waveform`. A record that is rejected at initialization, for a link the rules above do not allow, a session that does not exist, or a value type its record type cannot hold, defines no object.

### Objects by MIB name

A link can name its object as `MODULE::name.index`. The index is written out: `.0` for a scalar, and the index of the row for a column of a table:

```text
snmp3Mib /path/to/site/mibs
```

```text
record(longin, "<record_name>") {
    field(DTYP, "snmp3")
    field(INP, "@PDU1 PowerNet-MIB::rPDULoadStatusLoad.1")
}
```

The MIB gives the numeric identifier, the value type and the access of the object when the IOC initializes. It is used for nothing else: a value is never kept or compared as text, and an IOC whose links use numbers reads no MIB.

| Declared in the MIB | Value type |
| --- | --- |
| `INTEGER`, `Integer32` | `integer` |
| `Unsigned32` | `unsigned32` |
| `Counter32` | `counter32` |
| `Gauge32` | `gauge32` |
| `TimeTicks` | `timeticks` |
| `Counter64` | `counter64` |
| `OCTET STRING`, `BITS`, and a textual convention on them | `octets` |
| `OBJECT IDENTIFIER` | `oid` |
| `IpAddress` | `ipAddress` |
| anything else | none; the link must carry `type=` |

A module is looked for in three places, and the first place that holds it is used:

1. the places the IOC names with `snmp3Mib`, in the order of the commands;
2. the `mibs` directory of the module installation, which holds a note and no MIB file as shipped;
3. the directories of the system: the ones built into the Net-SNMP library, or the one directory given with `snmp3Mib <path> system`.

A MIB that imports other modules finds them in the same places. No environment variable chooses a place. Of the directories built into the library only the absolute ones are used, so a directory under the home of the user is not read.

A record is rejected at initialization, with `snmp3: record initialization rejected: <record>`, when its name does not resolve, names no object with a value (a table, a group, a notification, an identity), or carries no index, when its record type cannot hold the type, when it is an output record on an object the MIB declares read-only, or when it is an input record on an object the MIB declares not readable (`not-accessible`, `accessible-for-notify` or `write-only`). A unit that does not follow its MIB in this is reached with the numeric identifier, which reads no MIB. Before that the IOC prints `snmp3: MIB module <MODULE> not found` once for a module that no place holds, `snmp3: MIB name <text> not resolved` for a name its module does not hold, `snmp3: MIB name <text> is no object with a value` for a name of that kind, and `snmp3: MIB name <text> carries no index of an instance` for a scalar without `.0` or a column without the index of its row. The IOC continues with the other records.

All names of an IOC are resolved in one run of the probe when `iocInit` begins. `snmp3Show <pattern> 1` reports what was used:

```text
snmp3 object: session=PDU1 id=link.1 oid=1.3.6.1.4.1.318.1.1.12.2.3.1.1.2.1 type=gauge32 operation=get group=none systemInfo=0 name=PowerNet-MIB::rPDULoadStatusLoad.1 typeSource=mib
snmp3 mib: place=ioc path=/path/to/site/mibs
snmp3 mib: place=module path=<install>/mibs
snmp3 mib: place=system path=<directories>
snmp3 mib: module=PowerNet-MIB place=ioc file=/path/to/site/mibs/PowerNet-MIB
snmp3 mib: probeStarts=1
```

| Line or field | Meaning |
| --- | --- |
| `name=`, `typeSource=` | On the line of an object given by name: the name, and `mib` when the MIB gave the type or `link` when `type=` of the link decided. An object given by number has neither field |
| `snmp3 mib: place=<place> path=` | The places in the order they are searched |
| `snmp3 mib: module=<MODULE> place=<place> file=` | One line per module that was read, a named one or one it imports, with the place and file it came from |
| `snmp3 mib: probeStarts=<n>` | How many times the probe was started for names since the IOC started: 0 for an IOC without names |

## Configuration and startup examples

The shipped [v2c example](https://github.com/jeonghanlee/snmp3/blob/master/tests/rewrite/config/v2c.json) uses a disposable
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

The shipped [startup example](https://github.com/jeonghanlee/snmp3/blob/master/tests/rewrite/config/startup.cmd) runs from the
checkout root. Set `CONFIG_JSON` to the prepared configuration path and
`NATIVE_PROBE` to the absolute helper path and `WORKER_PATH` to the absolute
actual worker product through the IOC environment.
Ordinary Base expansion applies to these IOC arguments:

```
on error break
dbLoadDatabase("dbd/snmp3Ioc.dbd")
snmp3Ioc_registerRecordDeviceDriver(pdbbase)
snmp3WorkerPath("$(WORKER_PATH)")
snmp3Load("$(CONFIG_JSON)", "$(NATIVE_PROBE)")
snmp3ConfigReport
dbLoadRecords("tests/rewrite/db/lifecycle.db", "P=Config_")
iocInit
snmp3ConfigReport
```

The standard longin PINI fixture processes constant 42. It demonstrates Base
startup ordering, not SNMP I/O. Reports show revision 1 with one profile,
endpoint, and binding, changing from unfrozen to frozen before PINI.
The [verification runner](https://github.com/jeonghanlee/snmp3/blob/master/tests/rewrite/README.md) executes this exact shipped
startup file and JSON example with generated disposable secrets.
