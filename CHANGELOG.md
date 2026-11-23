# Changelog

## 0.0.10 — Message Size Read From The Unit

### Product

- When a session opens, the worker reads `snmpEngineMaxMessageSize.0` of the unit (SNMP-FRAMEWORK-MIB) with one GET, for every version, with the timeout and the retries of the profile. The session uses the lower of the value read and `maxMessageSize` (default 1472, which stays the upper limit) as its message size, never less than 484. A unit that does not report the value (no reply, an error, an exception, or a value that is not a positive integer) leaves `maxMessageSize`, and the read never fails the open. An SNMPv3 request carries the size used as `msgMaxSize`; a request of SNMPv1 or SNMPv2c has no such field.
- `snmp3RuntimeReport` prints a new line `snmp3 size: id=<endpoint> unit=<value>` after each `snmp3 endpoint:` line. The value is the number the unit reported, `unread` when the unit did not report it, or `unknown` before a request of the endpoint has completed. `snmp3Endpoint.db` has a new record `$(P)MessageSize` (`longin`) with the same number, 0 for the last two.

### Compatibility

- The lines of the report and the records of `0.0.9` are as before; the new line and the new record are added. A configuration, a database and a record link work as before. A session sends one more GET when it opens, and the open of a session of a unit that does not answer waits for the timeout and the retries of the profile once more.
- The worker and the IOC of one release are installed together, as before: the result record that the worker sends to the IOC has a new field and is 52 bytes before its value, not 48.

### Known limits

- The number of values of a request is not limited from the value read; a unit that rejects a request as too large still halves it, as in `0.0.3` and `0.0.9`.
- The read is part of the open of every session; a session of an SNMPv1 or SNMPv2c endpoint is opened once for each start of a worker, and an SNMPv3 session is opened again after repeated timeouts. That a session opened again reads the value again follows from the code and was not shown by a case.
- Observed on 2026-11-23 with the two laboratory units (one APC NMC2 and one NMC3, SNMPv3 only): both report a value larger than 1472, so their sessions use 1472, and the value read equals the one the Net-SNMP client reads. The case that lowers the size was shown with the fixture agent only. SNMPv1 and SNMPv2c were shown with the fixture agent only, because the recorded configuration of the units holds no community.

## 0.0.9 — GETBULK Table Reads

### Product

- A request is sent as one GETBULK when its values are consecutive instances of one column (the same object identifier but the last arc, which rises by one from each binding to the next) of an SNMPv2c or SNMPv3 endpoint. The request names the instance before the first one and asks for as many rows as the request has values. Any other read request, and every read request of an SNMPv1 endpoint, is sent as one GET as before; a write is sent as one SET as before. The records, the databases and `group` do not change.
- Each row of the reply reaches the binding that has its object identifier, not the binding at its position. A row the unit leaves out between two rows it answers is NoSuchInstance for its binding. A row that is not of the column, or the end of the MIB view, ends the rows, and the bindings left are NoSuchInstance. A reply with fewer rows than were asked and neither of these is taken as a tooBig reply: the records show `tooBig` and the request size of the address is halved as for any tooBig reply.
- `snmp3RuntimeReport` prints a new line `snmp3 bulk: address=<n> bulks=<count>` after the `snmp3 batch:` line of each address, and `snmp3Endpoint.db` has a new record `$(P)Bulks` (`longin`) with the same number: the requests handed to the worker as one GETBULK since the IOC started. A request that the worker then does not send, as when an SNMPv3 unit does not answer the request for its engine ID, is counted too.

### Compatibility

- The lines of the report and the records of `0.0.8` are as before; the new line and the new record are added. A configuration, a database and a record link work as before.

### Known limits

- Only a request whose values are all consecutive instances of one column is sent as GETBULK. A group that mixes columns is read with one GET.
- The Net-SNMP agent answers `genErr` when a GETBULK reply would not fit the message size of the request, and does not cut the reply; the module reports that status and does not halve the request size for it. A unit that cuts the reply is handled as described above.
- Observed on 2026-11-21 with the two laboratory units (one APC NMC2 and one NMC3, SNMPv3): both answer GETBULK; a request for 61 rows of an outlet column read with `snmpbulkget` was answered with 52 rows and no error status, so these units cut a reply that does not fit; both report 2000 for `snmpEngineMaxMessageSize.0`. The module read the 24 rows of an outlet column from each unit with one GETBULK, without alarm and with the values of `snmpbulkget`. A request of more rows than one reply holds was not run through the module on the units.

## 0.0.8 — MIB Tools

### Product

- Two Perl programs are installed into `bin/<arch>` beside `snmp3NativeProbe`, which they find through their own location; `--probe` names another probe. They read MIB files and a database and contact no unit.
- `snmp3MibCheck.pl [--probe PATH] [--mib PLACE]... [--system DIR] DATABASE...` checks the records with `DTYP` `snmp3` whose link names an object as `MODULE::name.index`. It resolves the names in one run of the probe and prints `FILE:LINE: RECORD: CLASS: TEXT` for each record the IOC would reject because of its MIB name, its access or its type. The classes are `name`, `module`, `object`, `index`, `type`, `read-only` and `not-readable`. The exit status is 0 for a database without findings, 1 for findings and 2 for a fault of the call or of the probe. Comments are skipped, and names and values written without quotes, with or without macros, are read.
- `snmp3MibDraft.pl [--probe PATH] [--mib PLACE]... [--system DIR] [--index FIRST..LAST] MODULE::name...` writes the draft of a database on the standard output. Each object gets an input record, and an output record `<name>_Set` when its access allows writing; a column of a table gets the records for each index of the range. The record type comes from the declared type, `DESC` is the description cut to 40 bytes, units of at most 15 bytes become `EGU`, longer units and the labels of an enumerated integer stay in comments, and the text is written as UTF-8.
- `snmp3NativeProbe --describe SYSTEM PLACES NAME...` returns, for each `MODULE::name` without an index, whether it is a scalar or a column, its type and access, its description, its units and the labels of an enumerated integer. The output of `--resolve` and `--capabilities` is unchanged.

### Compatibility

- The commands, the records and the output of the probe of `0.0.7` work as before. The release adds the two programs and the probe mode.

### Known limits

- The checker takes over the rules of the IOC for the MIB name, the access and the type only. A record can still be rejected for the fields of its record (`LINR`, `OOPT`, `SIZV`, the waveform), the grammar of its link, `SCAN` or a subscription. A link that holds a macro and a record whose `DTYP` is written as a macro are not checked.
- The draft is a starting text: the names of the records, the scan and everything the MIB does not state are for the author. A column of a table whose rows have more than one index, or an index that is not a number, gets only the one arc given with `--index`, which names no row; the author completes those links.
- The programs were tested with a fixture MIB, the PowerNet MIB and the TRIPPLITE MIBs, and the drafts were loaded in an IOC that reads the fixture agent. No laboratory unit was contacted, because the programs contact none.

## 0.0.7 — Shell Commands, MIB Names and Reply-Driven Records

### Product

- An IOC can be configured by shell commands, without a JSON file: `snmp3ProbePath`, `snmp3Session name address [options]`, `snmp3Options pattern [options]`, `snmp3Subscription name session period`, `snmp3Mib path [system]` and `snmp3Show pattern verbosity`. The options are the fields a profile has in the file, with the same names, defaults and ranges, and every value belongs to one session. A file and commands in one startup script give one configuration.
- A record link can name its object: `@<session or subscription> <object> [type=..] [deadline_ms=..] [group=..] [systemInfo]`. The object is a numeric identifier with `type=`, or `MODULE::name.index`, whose identifier, type and access come from its MIB.
- MIB names are resolved by the probe in one run when `iocInit` begins; the IOC process reads no MIB. A module is taken from the first place that holds it: the places of `snmp3Mib`, the `mibs` directory the module installs, then the directories of the system. `snmp3Show <pattern> 1` prints the place and file of every module that was read.
- A subscription is a set of read objects of one session with a period. The module sends one request for them when the period has passed and Base has processed the records of the request before it. A record with `SCAN=I/O Intr` reads through a subscription and takes one result per processing, with the value, alarm and message a request of the record itself gives. A unit that answers later than the period is asked again as soon as its reply was processed.
- The state records of `DTYP` `snmp3 state` accept `SCAN=I/O Intr`: they process when a state the module shows changes, and once when Base begins to scan.
- A record is rejected at initialization, and named on the standard error, when its name does not resolve, is no object or carries no index, when its record type cannot hold the type, when it writes an object the MIB declares read-only or reads one declared not readable, and when its `SCAN` does not fit its link: `I/O Intr` needs a subscription, and a subscription takes input records with `I/O Intr` and no `group=` label.

### Compatibility

- A configuration from a JSON file and the link `@binding=<id>` work as before.
- `deadline_ms` is optional on both link forms. Without it the budget is `2 x timeoutMs x (retries + 1) + 1000` ms, at most 600000. A link `@binding=<id>` without `deadline_ms` was rejected before and is accepted now.
- The module installs a directory `mibs/` with a note and no MIB file; `make uninstall` removes it.
- The records of `0.0.6` keep their behavior. The macro `SCAN` of the two state databases keeps `1 second` as its default.

### Known limits

- The module ships no MIB file. A MIB that imports standard modules needs them in a place the IOC names or in the directories of the system.
- A subscription with more records than the count limit of the queue of its address is not admitted, and its records carry READ/INVALID with `request not admitted`; `snmp3QueueLimit` raises the limit.
- The reports of the module print to the standard output of the IOC process, which the output redirection of the IOC shell (`command > file`) does not move.
- On the laboratory units the commands, the names and the subscriptions were run read only. A SET through a link that names its object was run against the fixture agent only.

## 0.0.6 — Module State Records

### Product

- The module installs two databases into `db/`. `snmp3Runtime.db` gives `$(P)RuntimeState`, the state of the module. `snmp3Endpoint.db`, loaded once per endpoint with the macro `ENDPOINT`, gives `$(P)PeerState`, the state of the endpoint after a responder showed another engine ID (`normal`, `changed`, `unverified`), and `$(P)BatchLimit` and `$(P)BatchSplits`, the request size of its address. `changed`, `unverified`, `IncompleteStopped` and `Failed` carry MAJOR.
- The records read the state the IOC process holds and send nothing to a unit. They have `PINI=YES` and a periodic scan, 1 second unless the macro `SCAN` sets another period, so a change is visible at most one scan period late.
- `snmp3RuntimeReport` prints one `snmp3 endpoint:` line per endpoint with its `id`, the number of its address, its state and, once the state is settled as `changed` or `unverified`, the time of the change.

### Compatibility

- `dbd/snmp3.dbd` gains the device support `snmp3 state` for `mbbi` and `longin`. The records of `DTYP="snmp3"` are unchanged.
- The lines that `snmp3RuntimeReport` printed before keep their text; the `snmp3 endpoint:` lines follow the worker lines.

### Known limits

- The state records do not offer `SCAN=I/O Intr`.

## 0.0.5 — Shared Library Names

### Product

- The three shared libraries of the module are installed under a versioned name, the way EPICS Base installs its own: `libsnmp3.so.1`, `libsnmp3Wire.so.1` and `libsnmp3Native.so.1` are the libraries, each with its own name as SONAME, and `libsnmp3.so`, `libsnmp3Wire.so` and `libsnmp3Native.so` are links to them.
- The number is the ABI number of the module, `SHRLIB_VERSION` in `snmp3App/src/Makefile` and `snmp3App/native/Makefile`. It is one number for the three libraries, independent of the release number, and raised only for an incompatible change of an interface that a release has promised. It stays 1 until `1.0.0`.

### Compatibility

- A consumer built before this version names `libsnmp3.so` and keeps starting through the link.
- A consumer built from this version on names `libsnmp3.so.1` and does not start against an installation that holds only another ABI number.

## 0.0.4 — SET Outcome

### Product

- A request that the unit answers with an error status other than tooBig sets the alarm message of its records to the name of the status and the error index, as `<status> index <n>` (for example `wrongType index 1`), for a SET and a GET alike.
- The SET path is qualified for `ao`, `longout`, `int64out`, `stringout` and `lso`: an output record stays active until the result of its SET arrives, a refused SET is WRITE/INVALID with the message above, and a SET without a reply is COMM/INVALID with `no reply`.
- Consecutive writes to one record follow EPICS Base: a put that waits for completion is sent on its own, in order, and plain puts that arrive while a request of the record is pending are merged into one further SET with the latest value. The module keeps no queue of values of its own.

### Included and not yet verified on a unit

This version also holds the recovery of a unit that returns, which is verified with the fixture agent and waits for the runs on the laboratory units before it is closed:

- After 2 consecutive requests without a reply the worker starts the SNMPv3 session of the endpoint again and drops the remembered engine time, so a unit that returns with its engine time behind the remembered one is synchronized again. A unit that has not answered since the IOC started carries `no reply`.
- A responder with another engine ID at the address puts the endpoint into the state `changed` or `unverified` until the IOC is restarted: no SET is sent and no value is taken, except bindings marked `systemInfo` in the state `changed`; the records carry `engine ID changed: restart IOC` or `engine ID unverified: restart IOC`, the IOC log holds one `snmp3 engine:` line, and `snmp3RuntimeReport` prints a `snmp3 peer:` line per address.
- The binding field `systemInfo` marks system information of a unit.

### Compatibility

- A sent request that fails with an error status now carries an alarm message where the field was empty before.
- The worker and the IOC must come from the same build: the binding of the worker bootstrap carries the system information mark, and a result may carry the native outcome 13.
- A unit that does not answer the question for its engine ID ends as a timeout, where it was an open failure.

### Known limits

- A SET that a unit answers with tooBig was not run; it takes the path of a read request.
- SNMPv1 and SNMPv2c carry no engine ID, so a replaced unit cannot be told apart there.

## 0.0.3 — Request Sizing

### Product

- When a unit rejects a request as too large, the scheduler halves the number of values per request of that address, rounded down and never below one, and keeps the lower limit until the worker of the address restarts. A tooBig reply and a timeout during which the library counted a reply it could not parse both count as a rejection; a timeout with no reply and every other error status leave the limit unchanged.
- A failed request sets the alarm message of its records to `tooBig`, `unreadable rejection`, or `no reply`.
- The profile field `maxMessageSize` (default 1472, range 484 to 65507) sets the message size that an SNMPv3 session advertises to the unit.
- `snmp3RuntimeReport` prints a `snmp3 batch:` line per address with the current limit and the number of reductions since the IOC started.
- A record that failed twice in a row with the same alarm message no longer keeps that message after its request succeeds.

### Compatibility

- The worker and the IOC must come from the same build: the profile of the worker bootstrap carries the new field.
- A sent request that fails with tooBig, an unreadable rejection or no reply now carries an alarm message where the field was empty before.

### Known limits

- In the device run, bindings without a `group` travelled one value per request in 4571 of 4574 object requests, and no request was rejected; a declared group of 61 values was rejected and then split.
- The count of unparsable replies rises on SNMPv3 sessions only; on SNMPv2c such a reply is reported as `no reply`, and SNMPv1 was not run.
- Endpoints with the same host address and different ports share one limit.

## 0.0.2 — Probe Environment

### Product

- `snmp3Load` launches the native probe with the same environment list as the worker, built by one function from the libraries of the worker executable, so the capability catalogue no longer depends on the IOC environment.
- `snmp3WorkerPath` must be called before `snmp3Load`. A load while no worker path is set, or with a worker for which `ldd` fails, is rejected before the probe starts, and a different worker path after a successful load is rejected.

### Compatibility

- A startup script that calls `snmp3Load` before `snmp3WorkerPath` is rejected: swap the two lines. The startup of an application repository must be changed the same way.

### Known limits

- The sanitizer options of a qualification run reach the worker only; the probe never receives them.
- Termination of the probe by a signal during the load is not covered by the checks.

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

- mdBook with architecture, procedures, references, design decisions and the PowerNet-MIB 4.6.0 review; the work register of that release ([`docs/milestone-6f015f8.md` at `ba6c558`](https://github.com/jeonghanlee/snmp3/blob/ba6c558a1118c792e769aed79ded7581538c3888/docs/milestone-6f015f8.md)) holds the product judgment, retrospective and the rule for writing the next goal.
