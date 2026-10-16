# Glossary of terms in this book

The terms below have a specific meaning in this book. Each page also defines an abbreviation at its first use, so you can open any page alone.

## Scope

This page lists each defined term once, in alphabetical order, with the definition that the pages use.

**Out of scope:** record field names of EPICS Base (see the EPICS Record Reference Manual) and the SNMP protocol terms that Net-SNMP defines.

| Term | Definition |
| --- | --- |
| Address worker | The separate native process that serves one numeric device address. It holds the Net-SNMP sessions for that address. |
| Admission | The atomic act of placing a request into the scheduler queue of one address. It publishes all members of a request or none. |
| AMSG | The alarm message field of an EPICS record. The module sets `deadline before send` for a request that expired before transmission. |
| ASan | The address sanitizer, a compiler instrument that detects memory errors. |
| Batch | The set of FIFO queue members that the scheduler sends to a worker as one native request. |
| Binding | An immutable definition in the startup configuration that links a record to a device object set. A record names it with `@binding=<id>`. |
| Burst | A run of admission calls, each less than 5 ms after the previous one. |
| Charge | The storage that the scheduler reserves for a request, written Q. It stays reserved until consumption and retirement both finish. |
| Config | The immutable configuration snapshot of the process. The first API binding, `AfterFinishDevSup`, or `snmp3Stop` freezes it. |
| Declared group | A set of bindings that carry the same `group` label. The scheduler batches its members whatever their deadlines. |
| Deadline | The absolute time by which a request must finish. It covers queueing, relaunch, IPC, and native service. |
| DSET | The device support entry table of an EPICS record type. The module provides eleven. |
| Epoch | The count of launches of one address worker. A new worker gets the next epoch. |
| FIFO | First in, first out. The order of the per-address request queue. |
| Frame | One unit of the IPC channel between the IOC and a worker. It has a 64-byte header. |
| Generation | One admission of a handle. A handle holds at most two generations: a consumed predecessor and a successor. |
| Handle | The registration of one record or component caller against a binding. |
| Hold bound | The 2 ms limit after the admission of the front member of a declared group. The scheduler holds the group at most this long. |
| IOC | The input/output controller, the EPICS process that runs the records. |
| IPC | Inter-process communication. Here it means the socket pair between the IOC and a worker. |
| K | The number of objects in one request. K\* is the group size chosen by the decision rule. |
| MIB | The management information base, the definition of the objects of a device. |
| NMC | The network management card of a power distribution unit. NMC2 and NMC3 are two generations. |
| OID | An object identifier, the numeric name of an SNMP object. |
| PACT | The process-active flag of an EPICS record. It is set while asynchronous work is pending. |
| PDU | Depending on context, a power distribution unit (the device) or a protocol data unit (an SNMP message). Each page states which. |
| Quiet gap | The 0.5 ms interval without a new member after which the scheduler releases a held declared group. |
| Retire grace | The 1000 ms after a deadline in which an answered request can still produce its Retired frame. Without the frame by then, the supervisor contains the worker. |
| Retired frame | The worker message that proves the native call unwound and the worker destroyed its request and result copies. |
| Retirement | The end of native ownership of a request, proved by a Retired frame or an exact reap of the worker process. |
| RPRO | The reprocess flag of an EPICS record. One flag covers several puts during a pending request. |
| RSS | Resident set size, the physical memory that a process uses. |
| Servicing loop | The module thread that handles deadlines, nonblocking IPC, and supervision. |
| SIMM | The simulation mode field of an EPICS record. |
| Successor | A generation admitted behind a retirement-pending predecessor of the same handle. |
| Terminal | The final result of one generation. A consumer borrows it with `take` and completes it with `release`. |
| Tier | A reading priority group of a monitoring set: state, measurement, or slow. |
| TSan | The thread sanitizer, a compiler instrument that detects data races. |
| UBSan | The undefined behavior sanitizer, a compiler instrument that detects undefined behavior. |
| USM | The user-based security model of SNMPv3. |
| Varbind | A variable binding, one object name and value pair in an SNMP message. |
