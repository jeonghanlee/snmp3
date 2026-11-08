# Closed decisions

This document records examined behavior deliberately kept unchanged. Reopening requires a new dated decision and the product requirement that changes the recorded premise. It is not an active work list.

## Blocked-callback shutdown duration

Verdict: Keep
Decision Date: 2026-11-07
Carrying Commit: `3ec3da083424859801d3f094c6a6edb516af60ff` (the examined state)

Premise: accept the current stop sequence with the recorded blocked-callback observations. No shortened or overlapping wait is required for the accepted monitoring scope.

Evidence: the [committed observation and acceptance branches](https://github.com/jeonghanlee/snmp3/blob/3ec3da083424859801d3f094c6a6edb516af60ff/docs/milestone-5dff352.md#m12---stop-duration-with-a-blocked-callback-queue) identify the 2026-10-05 M6 T11 execution, tests `17c494a` and product `2544819`, with observed 2.5 s and 4.0 s stops. The repeated-drain explanation remains source analysis, not a dedicated verified run. No fresh duration measurement or universal bound is claimed.

Disposition: retire the optimization proposal, dedicated repeated-drain investigation and proposed change-only regressions/controls. Preserve their unexecuted status; make no product or Base change.

## PowerNet-MIB monitoring input

Verdict: Keep
Decision Date: 2026-11-07
Carrying Commit: `3ec3da083424859801d3f094c6a6edb516af60ff` (the examined state)

Premise: retain PowerNet-MIB 4.4.6, the current hash pin and monitoring set; add no `rPDU2Adv*` objects.

Evidence: the [static comparison result](https://github.com/jeonghanlee/snmp3/blob/3ec3da083424859801d3f094c6a6edb516af60ff/docs/powernet-460-review/evidence/compare-result.json) records 91 identical selected object definitions covering 358 instances. Result SHA-256: `ad2ff674ee4441ce3bfbce478576c589fefe5ab1ffb3392e7b769c1936ac1d5e`, rechecked on 2026-11-07. No new comparison or device run is claimed.

Disposition: retire conditional 4.6.0 adoption, independent resolution against an adopted 4.6.0 database, and regeneration/integration checks. The existing 4.4.6 evidence does not qualify 4.6.0 or additional device features.
