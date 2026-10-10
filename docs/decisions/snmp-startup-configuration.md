# ADR: Owned startup configuration and native capability observation

Date: 2026-09-30
Status: accepted
Source Session: `<local>/review_sessions/20260923_104501_snmp-architecture`
Source Decisions: conv20260930_015254 D001, D002; plan20260930_014806 R3.1-R3.5
Scope: schema-1 startup configuration and typed component boundary
Note: `<local>/...` names a directory retained outside the repository, not tracked.

## Context

The IOC needs validated security, endpoint, and binding definitions before
device initialization. Readers must retain the same owned settings after
binding. The selected Base includes YAJL with JSON5 enabled by default, while
the configuration contract requires strict JSON. Native cryptographic support
depends on the selected installed Net-SNMP product.

## Decision

Use bounded strict JSON with both JSON5 and comments explicitly disabled.
Validate UTF-8 input and decoded string values/keys, including scalar bounds.
Validate the whole candidate, including references, secret files, and the
selected native catalogue, before atomic publication. Require owned secret
bytes read from one checked descriptor. Reject unknown fields and duplicate
keys instead of accepting ambiguous input.

Publish a process-owned immutable snapshot under a mutex. Freeze all settings
after the first successful binding, at Base AfterFinishDevSup, or on explicit
snmp3Stop, including cold stop. Failed candidates preserve the exact
snapshot and revision. Database cleanup does not reset configuration.

Observe native capabilities through a separately linked, explicitly selected
absolute helper. Use actual native enumeration and callable crypto operations.
Bound process duration and output; reap children on every observed outcome.
Keep Net-SNMP dependencies outside the IOC and support library.

## Consequences

Operators prepare a complete configuration and private secret files before
startup. A fresh IOC process is required to replace frozen settings. Numeric
reports omit identifiers and secrets. Secrets remain owned in process memory;
locked memory and complete zeroization are not guaranteed.

Binding metadata and owned Value tags define the component interface without
choosing EPICS record conversion policy. Native discovery, actual sessions,
record binding, queues, and helper/native consistency enforcement require later
integration. Native catalogue observation does not qualify those operations.

## Alternatives Considered

JSON5 and comment acceptance would make the language depend on Base parser
defaults and contradict the selected strict schema. An additional parser would
introduce an unnecessary installed dependency.

Incremental setters or partial publication could mix security and endpoint
revisions. Runtime credential replacement would require session and in-flight
request contracts absent from this checkpoint.

A hardcoded algorithm list or persisted capability file would not demonstrate
the selected native product's callable transforms. Linking native code into
the IOC would remove the selected process and dependency boundary.

## Verification Or Enforcement

The shipped `tests/rewrite/test_config.py` runs the real Config/YAJL library,
native helper, generated IOC registration, and standard PINI fixture. It checks
strict syntax, schema limits, security levels, descriptor-checked secret files,
atomic replacement, binding/freeze races, owned values, cold/post-start stop,
Continue-policy preservation, helper fault deadlines, and child reaping.

The runner first executes the complete foundation and lifecycle regressions.
It records actual loaded libraries, source/product/fixture hashes, command
outcomes, and cleanup. Wrong-owner secret testing is explicitly not-run when
the environment cannot create an independently owned file. It never substitutes
a mock owner or internal native/Config implementation.

`run_independence.py` separately audits the Base-only IOC and the declared native
helper. `test_lifecycle.py` runs two actual isolated database cycles and requires
the process-owned configuration to remain frozen across cleanup. The configuration
guide and command help define the operator contract enforced by these tests.
