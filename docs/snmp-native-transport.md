# Native SNMP transport

The separate `snmp3Native` library implements SNMP v1, v2c, and v3 GET and SET through Net-SNMP. It has a local single-owner API and returns owned results. Native calls run in the separate worker process, never in the IOC (input/output controller).

## Scope

This page explains the ownership, the session lifetime, the capability check, the handling of security material, and the event service of the native library. The verification driver loads the shipped schema-1 Config and immutable bindings, then calls the library through real UDP transport.

**Out of scope:** the result outcomes and value types (see [Native result outcomes and value types](snmp-native-reference.md)); the steps that run the verification (see [Verify the native transport](snmp-native-verification.md)); worker IPC, address queues, and IOC servicing (see [Address worker supervision](snmp-worker-supervision.md)). Record conversion, FLNK, hardware acceptance, runs on a second operating system, and sustained qualification are separate work.

Abbreviations on this page: FLNK is the forward link of a record, IPC is inter-process communication, PDU is a protocol data unit of SNMP, SHA256 is the secure hash algorithm with 256 bits, UDP is the user datagram protocol, and SNMP is the Simple Network Management Protocol.

## Products and dependencies

| Product | Purpose | Native dependency |
| --- | --- | --- |
| `libsnmp3Native.so` | Session, PDU, and event-service ownership | Net-SNMP |
| `snmp3NativeProbe` | Shared callable capability catalogue | Native library and Base Com |
| `snmp3NativeTest` | Actual Config, binding, and adapter verification | Native library, snmp3, and Com |
| `snmp3NativeApiProbe` | Direct library ownership and security preflight | Net-SNMP |
| `snmp3NativeAgent` | Disposable protocol and typed-value fixture | Net-SNMP agent library |

Production native flags are confined to `snmp3App/native` and its tests. The separate application binary interface (ABI) InventoryTest uses native headers only as a test input. The IOC and `libsnmp3` keep their Base-only dependency boundary.

The selected installed headers and libraries govern native behavior. Catalogue agreement alone does not prove binary identity, so test receipts also identify the actual loaded library paths and their SHA256 values.

The public API is declared in [Native.h](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/snmp3App/native/Native.h) and [Worker.h](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/snmp3App/native/Worker.h). The file [Native.cpp](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/snmp3App/native/Native.cpp) implements compatibility, discovery, PDU ownership, and response checks. The file [Worker.cpp](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/snmp3App/native/Worker.cpp) implements deadline service, completion delivery, and native shutdown. The installed headers `session_api.h`, `library/snmp_api.h`, `library/keytools.h`, `library/large_fd_set.h`, and `library/snmp_transport.h` supply the selected API declarations.

## Owner and session lifetime

Create one `Worker` on its servicing thread, once per process. Native initialization precedes session creation. Owner operations and destruction must use that same thread.

The library disables ordinary user and system configuration, persistent state, and native alarm signals. The owner closes sessions before native shutdown. Closing one session does not shut down other sessions.

The call `Worker::open(profile, endpoint, capabilities)` returns a distinct `Native` session. The session identity includes the numeric address and port, the immutable profile, the version, the security level, the user, the algorithms, the security and context engine IDs, and the context name. Secrets stay private owned profile data, outside the reported identity.

The call `Native::submit(bindings, payload, completion)` admits one GET or SET batch and returns its opaque token. The bindings must match the session endpoint and the complete immutable profile, and they must share one operation. A GET has no payload. A SET has one typed value per binding. The library rejects empty and oversized batches before the send.

The request storage owns the binding handles, the SET values, and the callback context. A caller mutation of the submitted vectors does not alter the accepted operation.

The call `Worker::service(maximumWaitMs)` services native sockets and deadlines. The call `Worker::service(maximumWaitUs, channel, channelWrite)` adds one caller descriptor to the same wait. That descriptor is the IPC channel of the worker, and it is in the write set while bytes are unsent. The call returns in one of these cases:

- the channel or a library socket is ready;
- a library timer is due;
- the bound passes (in microseconds; `UINT64_MAX` is no bound);
- the wait is interrupted or fails.

The caller reads the channel itself.

The call `takeEvents()` transfers accumulated diagnostic events to the caller. Events include native operations, session and token identity, monotonic times, and the actual socket file descriptor. Callers must consume events. The default local Worker keeps its component behavior. The address worker selects bounded event insertion (4096 slots and a numeric drop counter) and a separate native session and security ledger.

Completions receive owned `NativeResult` data after the native call stack unwinds. A completion can submit another request or close a session. Keep the Worker alive throughout API calls and completion delivery. Destroying the Worker from its own completion is unsupported. The library records callback exceptions as `completion_exception`, and they do not escape through native frames.

The call `close()` first prevents dispatch. It keeps pending contexts during the real native close callbacks and produces one Cancelled result for each remaining request. A repeated close produces no second native close and no second terminal result. Sessions can outlive owner shutdown as closed handles, and a submit then fails locally. Destroying the owner from the wrong thread terminates the process.

## Capability and USM association

The probe and the adapter use one callable capability implementation. Opening a session compares the native version and every canonical algorithm name, type, and object identifier (OID) against the immutable Config observation. A mismatch fails before discovery or profile-session open. There is no algorithm fallback.

For v3, the process pins private security material by the resolved security engine ID and the security user. Compatibility requires an equal security level, applicable algorithm metadata, and owned secret bytes. The native-derived keys and the existing native user entries must also agree. Profile names, context identity, and transport settings do not change this compatibility rule. Compatible contexts keep separate sessions.

The check order depends on the security engine ID:

- For an explicit security engine ID, check the tuple before registering candidate credentials.
- For an omitted ID, open a discovery-only native session with an empty user and no authentication or privacy. Resolve and close it, check compatibility, then derive keys and open the credential-bearing session.

Discovery dispatches no application GET or SET and preserves existing shared user keys.

The first association stays pinned until process exit, including after its last session closes. A conflict returns SecurityConflict without replacing native users or keys and without dispatching candidate application operations. Native timeliness updates stay enabled. Different engines or users have independent tuples. Replacing credentials at runtime needs a new process.

Native APIs derive master and localized keys and perform privacy-key extension. The module implements no custom user-based security model (USM) or crypto path. An omitted context engine ID inherits the resolved security engine ID. Explicit security and context IDs stay independent. Responses must keep the declared engine and context identity.

Native timeliness REPORT and retry handling can recover from a same-engine agent restart. The library supplies no application SET replay and no production worker restart policy.

## Event service and retry behavior

The owner initializes and separately zeros a dynamic large file-descriptor (FD) set. It merges the interests and deadlines from every active single-session handle, then calls the native large-FD select and read APIs.

Each session reports its own pending timer, so an idle session never discards the timer of another. The earliest timer and the bound of the caller set the wait. A failed wait other than EINTR sleeps 10 ms, so a repeating failure cannot become a busy loop. After each return from the wait, the owner services the native timeout of every active session. A return follows socket activity, a due timer, EINTR, or a failed wait.

Tests exercise successful traffic and timeout with an actual socket descriptor above `FD_SETSIZE`. They also exercise mixed deadlines while another session continues receiving traffic.

The immutable timeout and retry settings govern native behavior. Net-SNMP alone owns retransmission and discovery. Packet and callback timestamps and counts describe the actual retries, and the library adds no application retry loop. Native discovery and open can block. The test parent bounds the duration of its child. Production process containment is described in [Address worker supervision](snmp-worker-supervision.md#containment-of-worker-failures), and native open stays blocking within that separate process.
