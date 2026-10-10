# ADR: Single-owner native transport and process-pinned USM material

Date: 2026-09-30
Status: accepted for the verified R4 component boundary
Source Session: standalone work in `~/gitsrc/snmp3`
Source Decisions: User-approved `<local>/plan-r4-20260930.md`, SHA256 `6a707681a94607ad520f3d14de99989e7802232b7d28c5a7d6c915789ec8e14a`; R4.1-R4.5
Note: `<local>/...` names a directory retained outside the repository, not tracked.

## Scope

Separate native library, local event service and executable component tests.
Out of scope: production worker IPC/supervisor, address admission queues, IOC
record integration/conversion, hardware and sustained/two-OS qualification.

## Context

The Base-only IOC publishes immutable profiles/bindings and owned typed values.
Native sessions must preserve these identities and return owned response data
without exposing record pointers. Net-SNMP's single-session APIs isolate opaque
session handles, but USM user/key state remains shared within a native process.
Authenticated open can register security material; checking compatibility only
after that call cannot protect a previously associated tuple.

Native callbacks borrow PDU storage and can run synchronously during failed
send or pending close. Native retry/REPORT behavior and large-FD/deadline service
must follow actual library behavior, rather than an application retry model.

## Decision

Keep native code in a separate library, with one owner thread and one Worker
instance for the process lifetime. Initialize native state before sessions,
disable ordinary configuration/persistent state, close all sessions before
shutdown and preserve the Base-only IOC/support boundary.

Use a shared callable capability implementation for the probe and adapter.
Compare version and canonical algorithm name/type/OID entries before open.
Retain separate complete session identities for each immutable profile,
endpoint, engine and context. Verify actual binary selection separately from
catalogue agreement.

Pin private security material by resolved security engine ID and user until
process exit. Compatibility includes level, applicable native algorithm
metadata and owned secret bytes, with native-derived/cached key agreement.
Contexts and transport settings do not alter compatibility. Closing the last
session does not release the association. Reject conflicts without replacing
users/keys, clearing shared timeliness or dispatching candidate GET/SET.

Resolve omitted engines through a native discovery-only session with empty user
and no authentication/privacy before checking the tuple and registering
credentials. Preserve explicit security/context IDs independently; omitted
context IDs inherit the resolved security engine. Native APIs own master-key
derivation, localization, privacy-key extension, protocol encoding and retries.

Own immutable request context and payload until native access ends. Transfer
PDUs only on actual native send acceptance; free failed-send PDUs as caller.
Copy responses before borrowed PDU storage expires, validate positional OIDs,
counts, tags and storage, and retain status/index separately. Queue exactly one
terminal completion and deliver only after native-stack unwind. Permit submit
and session-close reentry, while requiring the Worker to outlive API calls and
completion delivery. Retain pending contexts through real close callbacks.

Merge all sessions' large-FD interests and native deadlines. Service native
timeouts even during another session's traffic or EINTR. Do not add application
retries or replay SET. Apply native wire/storage limits independently of the
configuration's checkRanges setting or broader owned Value representation.

## Consequences

Compatible contexts can progress through distinct sessions sharing safe USM
material. Credential replacement requires a new process. Secrets and derived
keys remain private owned memory; locked memory and complete zeroization are
not guaranteed. Identity/events report no secret-derived fingerprint.

Native discovery/open may block. Production containment and worker restart
policy require later integration. Local in-flight maps and diagnostic events
are not a bounded address queue or resource qualification. Event consumers
must retrieve accumulated events. Worker destruction inside its own completion
is unsupported, and wrong-thread destruction terminates the process.

Integer32/native OID representability can be narrower than owned Value storage.
Gauge32/Unsigned32 share a wire tag; immutable binding metadata selects the
owned tag. Lost native integer bits cannot be reconstructed. EPICS conversion
and precision rules remain separate work.

## Alternatives Considered

Linking native transport into the IOC would remove the chosen process and
dependency boundary. A custom protocol/USM/crypto implementation would replace
native behavior without satisfying the actual-library contract.

Checking tuple compatibility after authenticated open permits prior cache
mutation. Deleting/replacing USM entries on conflict or last-close invalidates
peers and pending work. Silently adopting a cached key can make invalid
credentials appear successful. These approaches violate process-pinned identity.

Delivering caller completions from native callbacks permits reentry to destroy
borrowed storage. Adding application retry or SET replay obscures native packet
ownership. Fixed FD sets cannot safely service sockets above FD_SETSIZE.

## Verification Or Enforcement

`tests/rewrite/test_native.py` uses shipped products, real native libraries, a
separate native agent and an external UDP fault process. Direct preflight checks
native ownership/callback/discovery sequencing; adapter executions separately
check all advertised algorithms, versions/security levels, engine/context
identity, credential conflicts in both orders, compatible close/reopen/pending
peers, native retries, typed ownership, large FD/EINTR, native failures and
same-engine timeliness recovery after an actual agent restart.

Separate ASan/UBSan products instrument the new native library/helper/drivers
and fixtures. Dependencies remain uninstrumented and leak detection is disabled.
Receipts identify sources, products, selected headers, actual loaded libraries,
redacted wire metadata, timing, terminal tokens and every owned child's reap
status. Secret sentinels must be absent from diagnostic output. Required
unexecuted cells block full component acceptance.

`test_config.py` runs complete current foundation/lifecycle/configuration
regressions. The independence audit keeps IOC source/object/command/link/loader
checks strict and audits separately declared native products. Privilege-dependent
wrong-owner secret cells can remain explicit NOT RUN; old owner receipts do not
transfer to changed products. The native transport reference supplies executable
operator commands and exact API/representation limits.
