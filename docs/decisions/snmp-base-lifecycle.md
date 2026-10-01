# ADR: Base Lifecycle Ownership

Date: 2026-09-30
Status: accepted
Source Session: `/home/jeonglee/gitsrc/snmp/work/review_sessions/20260923_104501_snmp-architecture`
Source Decisions: conv20260930_002546 Decisions; plan20260930_001514 R2.1-R2.2
Scope: idle-thread lifecycle checkpoint only

## Context

Base 7.0.10 owns record processing, callback services, init hooks and exit
handling. The independent module must finish startup before initial PINI and
join module threads before Base removes callback services. Actual Base source
announces AfterFinishDevSup before initialProcess and AtShutdown before
scanStop/callbackStop. Exit callbacks run in reverse registration order.

## Decision

Use process-owned Runtime storage initialized through epicsThreadOnce, one
joinable Base thread, readiness/stop events, an operation mutex and a separate
state mutex. Start at AfterFinishDevSup and stop at AtShutdown. Register the
hook at each real registrar invocation and one process fallback at Runtime
initialization. The worker never acquires the operation mutex; joins and
readiness waits hold no state lock. Preserve Failed after creation failure.

## Consequences

Runtime storage deliberately lasts until process exit. Admission permission
is not an implemented queue. This decision qualifies idle-thread lifecycle
only. In-flight request and callback drain, shutdown FLNK, retained callback
storage and native-session/process shutdown remain later integration gates.
The known Base callback-queue limitation remains unresolved; no installed
Base modification or callback-bearing acceptance follows from this decision.

## Alternatives Considered

The legacy shared exit flag, polling loop and detached-thread ownership are
excluded by the independent rewrite contract. They do not establish the
selected event-based readiness and exactly-once join requirements.

A process-exit fallback alone cannot establish module stop before Base
callback shutdown: exitDatabase runs before the earlier module fallback.
The early AtShutdown hook establishes that ordering; the later fallback
covers cold registration and repeated cleanup without a second join.

## Verification Or Enforcement

The shipped tests/rewrite/test_lifecycle.py fixture observes actual ready
before PINI and actual worker return/join before Base callback shutdown.
Direct module tests exercise concurrent/repeated stop and event reuse.
Two actual isolated Base database cycles require restored hooks and
cumulative joins. A real non-root OS-limit failure must leave no created
or joined handle. The test invokes the actual Main.cpp implementation;
a successful returning startup script must not hide retained Failed state.
This direct Main test is supplemental module coverage, not a full IOC
startup thread-failure integration test.

## Authority

EPICS Base 7.0.10: modules/database/src/ioc/misc/iocInit.c startup and shutdown;
modules/libcom/src/osi/epicsThread.h joinable-thread contract;
modules/libcom/src/misc/epicsExit.c process callback order;
modules/database/src/ioc/db/dbUnitTest.c isolated cleanup and hook removal.
