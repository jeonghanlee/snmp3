# Review record of the legacy module, Net-SNMP, snmp3 and Base 7

This page records the full source review carried out on 2026-11-09 before the next development goal was chosen. It names every source that was read. It states what the reading established about the legacy device support `devSnmp`, the Net-SNMP library, the snmp3 module at tag `0.0.1`, and the facilities of EPICS Base 7.0.10. It closes with the items the review carries into the next goal.

## Scope

This page covers the sources examined and the mechanisms found in the legacy source behind the five operating problems of [The operating problems behind snmp3](why-snmp3.md). It also covers the additional structural defects found in a complete read of that source, the Net-SNMP behavior confirmed from library source, the position of snmp3 `0.0.1` against those findings, and the Base 7.0.10 facilities a new driver can use.

**Out of scope:** the design of the next goal (recorded in the work register), the snmp3 architecture (see [Architecture](snmp-rewrite-architecture.md)), and any measurement on equipment. Every statement below comes from reading source; none was observed on a device.

Abbreviations on this page: SNMP is the Simple Network Management Protocol, OID is an SNMP object identifier, PDU is an SNMP protocol data unit, USM is the SNMP user-based security model, NMC is the network management card of an APC power distribution unit, IOC is an EPICS input/output controller, and SCAN is the scan field of an EPICS record.

## Sources examined

| Source | Identity | Location at review time | What was read |
| --- | --- | --- | --- |
| Legacy module `snmp` (GitHub `jeonghanlee/snmp`) | commit `db9ebf5`, the last state before the 2026-09 rewrite commits | `/home/jeonglee/gitsrc/snmp` | `snmpApp/src/devSnmp.cpp` (5401 lines, complete), `devSnmp.h`, `snmpRegister.cpp`, `snmpSessShow.c`, `snmpMain.cpp`, `devSnmp.dbd`, `Makefile`, `getNetSNMPversion.pl`, `snmpApp/Db/snmp.db`, `iocsh/raritan_emx888.iocsh`, `iocBoot/iocsnmp/st.cmd`, `README.md`; the diff `db9ebf5..68294c2` of `devSnmp.cpp` |
| APC PDU IOC `apcpdu` (internal GitLab) | commits `9e34314`, `cbb9294`, `d9df592`, `43fd0d7` | peer checkout | `iocBoot/iocar08-pdu/st.cmd` at the three startup commits, `apcpduApp/iocsh/pdu_ap7800b.iocsh`, the `Db` templates, `docs/pdu-snmpv3-notes.md` |
| Net-SNMP, Debian 13 | `5.9.4+dfsg-2+deb13u1`, the version linked on the review machine | `work/netsnmp-src/net-snmp-5.9.4+dfsg` (from `apt-get source libsnmp-dev`) | `snmplib/snmp_api.c`: `snmp_open`, `_sess_open`, `snmp_sess_add_ex`, `snmp_sess_copy`, `snmpv3_engineID_probe`, `_sess_async_send`, `snmp_sess_timeout`, `snmp_resend_request`, `snmp_sess_select_info2_flags`, `_sess_read`, `_sess_process_packet_handle_pdu`, `snmp_sess_close`, `snmpv3_parse`, `netsnmp_max_send_msg_size`, `init_snmp`; `snmplib/snmpusm.c`: `usm_discover_engineid`, `usm_build_probe_pdu`; `snmplib/snmp_client.c`: `snmp_sess_synch_response`; `snmplib/mib.c`: `sprint_realloc_integer` |
| Net-SNMP, Rocky Linux 8.10 | source package `net-snmp-5.8-33.el8_10` | `work/netsnmp-src/rocky8` | presence of each API feature in `include/` and `snmplib/`, the probe path in `snmp_api.c`, the 60 Red Hat patches by header, `net-snmp.spec` configure options |
| Net-SNMP, Rocky Linux 9 | source package `net-snmp-5.9.1-21.el9` | `work/netsnmp-src/rocky9` | presence of each API feature, the 42 Red Hat patches by header, `net-snmp.spec` configure options |
| EPICS Base | 7.0.10 | `/home/jeonglee/gitsrc/EPICS-env/epics-base-src` | `aiRecord.c` `process`, `dbScan.h` (`scanIoSetComplete`), `recGbl.h` (`recGblSetSevrMsg`) |
| snmp3 | master `32d2abf`, product source identical to tag `0.0.1` | this repository | every file under `snmp3App/src` and `snmp3App/native`; the Net-SNMP calls of the worker; the Base facilities used by `DeviceSupport.cpp` and `Request.cpp` |

The `work/netsnmp-src` directory is untracked. Recreate it with `apt-get source libsnmp-dev` on Debian 13, and with the two source packages from `https://dl.rockylinux.org/pub/rocky/8/BaseOS/source/tree/Packages/n/` and `https://dl.rockylinux.org/pub/rocky/9/AppStream/source/tree/Packages/n/`, unpacked with `7z x` and `cpio -idm`.

## Legacy module: mechanisms behind the five operating problems

Line numbers refer to `snmpApp/src/devSnmp.cpp` at commit `db9ebf5`. The structure that explains every problem is one send thread, one read thread, and one mutex shared by both for every host of the IOC (`3630-3720`).

| Problem | Mechanism | Lines |
| --- | --- | --- |
| 1. tooBig and OID errors | The only request size control is the fixed per-host `MAXOIDS`. A reply with fewer variables than requested fails the sanity check and no OID of that transaction receives data. An error reply flags the OID at `errindex` as `noSuchName` and excludes it until restart. A tooBig reply has `errindex` 0, so every OID clears `lastPollSent` and the same oversized request is sent again on the next tick. There is no adaptive split. | `1120-1140`, `1174-1186`, `2101-2108`, `2121-2126` |
| 2. The second device and later get no signal | Every transaction opens a new Net-SNMP session with `snmp_open`. For SNMPv3 the open performs an engineID probe synchronously, inside the shared mutex and inside the only send thread (see the Net-SNMP section). The deployed sector IOC at apcpdu `9e34314` defined seven PDUs and loaded six while three existed; each absent PDU blocked every tick for `SessionTimeout x (SessionRetries + 1)`. Live PDUs could not send, and the read thread could not read their replies. apcpdu `cbb9294` removed the absent loaders. | `1429-1460`, `1216-1236`, `3471-3490` |
| 3. SCAN, driver polling and Net-SNMP timing do not meet | A record read returns the cached value regardless of its age, or sets `PACT` and waits for a first reply without a deadline. The OID poll period is the shortest SCAN of its records, measured from the last send. The group builds one transaction per 20 ms tick and the host has one session in flight. The read thread holds the mutex across `select` for up to the next Net-SNMP deadline or 2 s. | `4487-4546`, `1866-1889`, `2224-2233`, `2831-2925`, `3419-3470`, `3662-3690` |
| 4. Signal backlog | The host queue is first-in first-out with one in-flight session. The 61 OIDs of a 24-outlet PDU form four transactions at `MAXOIDS=20`; at the NMC2 reply time of 1.46 to 1.48 s one full poll takes about 6 s while the records process every 2 s. A silent host spends the full timeout on each queued transaction. After `DataStaleTimeoutMSec` (20 s) the cache is cleared and the records go INVALID. | `3419-3470`, `1705-1716` |
| 5. Irregular updates that differ per device | A received reply processes every record of that OID from the read thread through `rset->process`, so the record time is the reply time. SCAN then processes the same cached value again. Clients see an update only when the value changes, so the visible interval is the poll cycle of that host: about 6 s on NMC2 and about 0.2 s on NMC3. Problems 4 and 5 are consequences of problem 3, not separate causes. | `1140-1150`, `3078-3090`, `2501-2512` |

## Legacy module: additional structural defects

These items come from the complete read and were not among the five observed problems. The grouping of all 23 items into five causes, and the guarantee of snmp3 against each cause, are in [The operating problems behind snmp3](why-snmp3.md).

| No. | Defect | Lines |
| --- | --- | --- |
| 6 | One session and one UDP socket per transaction, with SNMPv3 key localization and the engineID probe repeated every time. No session reuse. | `1429-1460`, `1216-1236`, `1298-1315` |
| 7 | A single `noSuchName` reply excludes the OID permanently; only an IOC restart restores it. | `2121-2126`, `1866-1873` |
| 8 | A record with no cached value stays `PACT` with no deadline. A host that never answers leaves its records neither complete nor in alarm. | `4487-4500`, `3078-3090` |
| 9 | `snmpAoWrite` reports success before anything is sent. A SET that times out is dropped without retry or alarm, and consecutive settings keep only the last one. | `2176-2190`, `4869-4912`, `1805-1836` |
| 10 | All elapsed times use `globalLastTick`, which only the send thread advances. When a probe blocks that thread, stale detection, poll weights and reply statistics stop with it. | `3702-3718`, `754-830` |
| 11 | `SessionTimeout` and `SessionRetries` are IOC-wide. A device that answers in 0.08 s and one that answers in 1.5 s must share one value. Whether a change after `iocInit` takes effect is marked unknown in the source. | `150-170`, `2960-2975` |
| 12 | Output readback writes `pao->val` before taking `dbScanLock`. | `4938-5000` |
| 13 | SNMPv3 defaults are MD5, AES-128 and `noAuthNoPriv`; passphrases stay in plain `char[512]` buffers; the `ver3.txt` file is read without a permission check; only MD5 and SHA-1 authentication are selectable. | `3091-3114`, `3263-3395`, `devSnmp.h:52-62` |
| 14 | The exit hook sleeps 0.2 s, waits at most 3 s per thread, then deletes the host list while the read thread may still be in `select`. | `324-336`, `3597-3628` |
| 15 | After a failed `snmp_send` the PDU belongs to the caller, but `close()` frees it only when `tried_send` is false. Each send failure leaks one PDU, and the failed OIDs retry on the next tick. | `1264-1296`, `1298-1315` |
| 16 | Authentication failures arrive as callback operation `SEC_ERROR` and are handled on the same path as a timeout. Records cannot distinguish a wrong credential from a silent host. | `1189-1207`, `2129-2133` |
| 17 | A malformed reply is dropped by the library without an error code; the driver sees a timeout and resends the identical request. | `1189-1207` |
| 18 | Every reading is stored as the text of `snprint_value` and parsed back through a text mask such as `INTEGER:`. The text depends on `snmp.conf` output options and MIB availability (see the Net-SNMP section). Native numeric values are used only with the `n` flag. | `1608-1652`, `2263-2415` |
| 19 | The 60 s stale-session deletion runs `snmp_close` inside the session destructor, and the library calls back into `replyProcessing` on the object being destroyed. The order of member deletion keeps it working. | `1073-1082`, `1298-1315` |
| 20 | Every waveform converter computes `strchr(sval, ':') + 1` before the NULL test, `snmpWfIntConvert` uses the format `%ul`, and `snmpWfStrConvert` copies without a bound. | `5257-5401` |
| 21 | Passive input records are still polled at `PassivePollMSec`; SCAN `Event` falls to a 200 ms poll. | `2224-2233` |
| 22 | The INP parser uses unbounded `%s` into 128-byte fields, and the `macEnvExpand` result is not freed. | `275-323`, `3836-3843` |
| 23 | Both threads wake every 20 ms whether or not there is work. | `3654-3718` |

## Net-SNMP: behavior confirmed from library source

Line numbers refer to the Debian 5.9.4 source. The Rocky 8.10 (5.8) and Rocky 9 (5.9.1) sources show the same code paths at nearby lines.

| Behavior | Evidence |
| --- | --- |
| `snmp_open` probes the engineID at open time for an SNMPv3 session. | `snmp_api.c:1869-1876` in `snmp_sess_add_ex`; `1851` in 5.8 |
| The USM probe is synchronous: `usm_discover_engineid` calls `snmp_sess_synch_response`, which loops on `select` until a REPORT arrives or the last retry expires. | `snmpusm.c:3690-3760`; `snmp_client.c` `snmp_sess_synch_response`; `1365` in 5.8 |
| A request makes `retries + 1` attempts; the total wait is `timeout x (retries + 1)`. | `snmp_api.c:6868` (`rp->retries >= sp->retries`) |
| The discovered engineID is stored in the library's internal session copy, not in the caller's session structure. A caller that opens a new session from the same template probes again every time. | `snmp_api.c:5818-5831` (write to `sp`), `1192-1228` (`in_session` read only) |
| After a failed `snmp_send` the caller owns the PDU. | `snmp_api.c` comment above `_sess_async_send`; `5339-5350` |
| A REPORT that is not `notInTimeWindow` reaches the callback as `NETSNMP_CALLBACK_OP_SEC_ERROR` after the retries. | `snmp_api.c:5795-5812` |
| A reply whose ScopedPDU does not parse is logged as `security service 3 error parsing ScopedPDU` and dropped; no callback runs and the request later times out. | `snmp_api.c:4017-4047` |
| `snmp_sess_close` calls the callback with `NETSNMP_CALLBACK_OP_TIMED_OUT` for every pending request before freeing it. | `snmp_api.c:1990-2006` |
| `init_snmp` reads the host `snmp.conf` files and the application configuration before any session exists. `sprint_realloc_integer` omits the `INTEGER:` prefix under `quickPrinting` and prints an enumeration label or number according to `printNumericEnums` and MIB availability. | `snmp_api.c:925-930`; `mib.c:1384-1420` |
| The SNMPv3 header field `msgMaxSize` is the smaller of `netsnmp_max_send_msg_size()` and the session `rcvMsgMaxSize`. With no configuration the function returns `SNMP_MAX_PACKET_LEN` (`0x7fffffff`). Upstream 5.8 initializes `rcvMsgMaxSize` to 1472; the Red Hat patch `net-snmp-5.8-bulk.patch` and the 5.9 line initialize it to `netsnmp_max_send_msg_size()`. On all three distributions the default therefore tells the agent that any response size is acceptable. The minimum a session may advertise is `SNMP_MIN_MAX_LEN` (484). | `snmp_api.c:2384-2385`, `787`, `1808`; `snmp.h:48-49`; `rocky8/net-snmp-5.8-bulk.patch` |
| The following features exist in 5.8, 5.9.1 and 5.9.4 alike: SHA-256 and SHA-512 authentication (`usmHMAC192SHA256AuthProtocol`, `usmHMAC384SHA512AuthProtocol`), AES-192 and AES-256 (`usmAES192PrivProtocol`, `usmAES256PrivProtocol`), `netsnmp_extend_kul`, `usm_lookup_auth_str`, `usm_lookup_priv_str`, `securityAuthLocalKey`, `rcvMsgMaxSize`, `netsnmp_large_fd_set`, `snmp_sess_select_info2_flags`, `snmp_sess_read2`, `netsnmp_get_monotonic_clock`, `SNMP_FLAGS_DONT_PROBE`, `snmp_pdu_add_variable`, `set_enginetime`, and the callback operations `SEC_ERROR` and `SEND_FAILED`. | presence check over `include/` and `snmplib/` of the three trees |
| Distribution build options: Rocky 8.10 and Rocky 9 build with `--enable-blumenthal-aes`, security modules USM and TSM, and transports including DTLSUDP and TLSTCP. Rocky 9 adds `--disable-des`; Rocky 8.10 keeps DES. Debian 13 builds with `--enable-blumenthal-aes`, security module TSM and transport TLSTCP only, does not disable DES, and its `net-snmp-config --version` reports `5.9.4.pre2` for package 5.9.4. | `rocky8/net-snmp.spec:298-321`, `rocky9/net-snmp.spec:289-313`, `net-snmp-config --configure-options` on the review machine |

## snmp3 0.0.1: position against the findings

The Net-SNMP calls of the worker, from `snmp3App/native`, are `snmp_sess_init`, `snmp_sess_open`, `snmp_sess_async_send`, `snmp_sess_select_info2`, `snmp_sess_read2`, `snmp_sess_timeout`, `snmp_sess_close`, `snmp_sess_session`, `snmp_sess_transport`, `netsnmp_large_fd_set_init`, `netsnmp_large_fd_set_select`, `netsnmp_large_fd_set_cleanup`, `snmp_pdu_create`, `snmp_add_null_var`, `snmp_pdu_add_variable`, `snmp_free_pdu`, `generate_Ku`, `netsnmp_extend_kul`, `usm_get_user`, `usm_lookup_auth_str`, `usm_lookup_priv_str`, `netsnmp_ds_set_boolean`, `netsnmp_get_version`, `init_snmp`, `snmp_shutdown`.

| Finding | snmp3 `0.0.1` |
| --- | --- |
| Net-SNMP inside the IOC process; one mutex and one send thread for all hosts (problems 2, 3; items 6, 10, 11, 23) | Answered. Net-SNMP runs in one worker process per address; the IOC links only Base. |
| Session per transaction and repeated probe (item 6) | Answered in structure: the single-session API and `SNMP_FLAGS_DONT_PROBE` control are used (`Native.cpp:118`). How long one discovered engineID is reused is to be confirmed. |
| Value stored as text (item 18) | Answered. Values cross the worker boundary as wire types. |
| Alarm without a cause (item 16) | Answered. `recGblSetSevrMsg` carries a message (`Request.cpp:166`). |
| 40-character string limit worked around with waveform (item 20) | Answered. `lsi`, `lso` and `int64in` are supported. |
| Record waits without a deadline (item 8) | Answered. Every request has a deadline. |
| Typed SET and dropped SET (item 9) | Typed SET through `snmp_pdu_add_variable`. GET-only is the accepted scope of `0.0.1`; SET delivery is out of that scope. |
| tooBig and request sizing (problem 1) | Not answered. `maxVarbinds` is a fixed per-device value as `MAXOIDS` was. `rcvMsgMaxSize` is not set. No adaptive split. |
| Reply time as record time (problems 4, 5) | Not answered. No device timestamp is written; the record time is the processing time. |
| Processing on reply arrival (problems 4, 5) | Not answered. The DSETs report `noInterrupt`; `SCAN=I/O Intr` is not offered. |
| Localized keys instead of passphrases (item 13) | Not answered. `securityAuthLocalKey` and `securityPrivLocalKey` are not used. |
| Table reads in one request | Not answered. `SNMP_MSG_GETBULK` is not used. |
| Probe process environment | Resolved after `0.0.1`: the native probe runs with the same environment list as the worker, built by one function from the libraries of the worker, so the catalogue cannot depend on the IOC environment. The load requires the worker path first. The finding came from the snmp3 source read of this review: the probe was spawned with the IOC environment while the worker was spawned with a scrubbed environment. |

## EPICS Base 7.0.10: facilities for a new driver

| Legacy practice | Base 7.0.10 facility | Source |
| --- | --- | --- |
| `rset->process` called directly from the read thread | `callbackRequestProcessCallback` for asynchronous completion | `modules/database/src/ioc/db/callback.h` |
| `SCAN=I/O Intr` rejected; records poll a cache | `scanIoRequest` with `scanIoSetComplete` so a record processes when the reply arrives and the driver learns when processing finished | `dbScan.h:117-133` |
| Record time is the processing time | `TSE=-2` with the device support writing `prec->time` from the reply arrival time | `dbCommon.dbd.pod`, TSE |
| Alarm severity only; the cause goes to `printf` | `recGblSetSevrMsg` writes the cause to `AMSG` | `recGbl.h:30-78` |
| String longer than 40 characters stored in a `waveform` of `CHAR` | `lsi` and `lso` records; `int64in` and `int64out` for 64-bit counters | Base record set |
| 20 ms sleep loops for every timer | `epicsTimerQueueActive` and per-request deadlines | `epicsTimer.h` |
| Positional INP string `host community oid mask length flags` | JSON link syntax (`jlink`) or a structured configuration file | `dbJLink.h` |
| `ai` process reads the device support in both the start and the completion phase and takes the time stamp after the read | 7.0.10 keeps this order; a device support that sets `PACT` completes through a callback that processes the record again | `aiRecord.c` `process` |

## Items carried into the next goal

- Confirm on an NMC2 and an NMC3 unit whether a session `rcvMsgMaxSize` of 1472 or less makes the agent answer `tooBig` instead of a malformed reply (problem 1).
- Confirm with two devices on one IOC that a silent or slow device leaves the other device's cadence unchanged under snmp3 (problem 2).
- Decide whether the next driver processes records on reply arrival (`I/O Intr`) and stamps them with the reply time (problems 4 and 5).
- Decide the SNMPv3 credential handling: localized keys, permitted authentication and privacy protocols per card generation, and the secret file procedure (item 13; the work register carries this as its secret management unit).
- Decided: the probe spawn uses the worker's environment list (the probe process environment row above).
- Keep `GETBULK` and adaptive request splitting as design options for problem 1 and for performance.
