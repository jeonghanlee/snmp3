# Observation Tools

## Scope

Collect and evaluate native SNMP request events, fresh IOC resource reports,
CA values and grouped polling cycles. Python commands require Python 3 and
accept `--help`. Runtime inputs and outputs belong outside this directory.

**Out of scope:** Equipment configuration, write tests, application-specific
legacy conversion rules and deployment. APC-specific comparison and recovery
scenarios belong in the APC application's `tests/` directory.

## Components And Data Flow

| Component | Input | Output |
| --- | --- | --- |
| `build_observer.py` | Explicit module source/install tree and EPICS Base | `observer.so`, compiler output and build receipt |
| `native_observer.cpp` | `SNMP3_OBSERVER_LOG`, an absolute path | Runtime qualification sink selected before IOC startup |
| `native_observation.py` | Actual IOC child, report log and native events | Fresh report samples and request-lifetime observations |
| `evaluate_native.py` | Log, independent SHA-256 and monotonic microsecond window | Native coverage, callback counts and descriptive timing |
| `capture_ca.py` | Installed `libca.so` and requested channel lists | Values, native status/severity and integer timestamps |
| `poll_cycles.py` | CA monitor text and UTC nanosecond window | Paired State/Meas/Slow polling generations |
| `evaluate_cycles.py` | Monitor digest and window JSON | Polling coverage and cycle durations |
| `analyze_observation.py` | Completed run, binding inventory and PV prefix | Separate replay inputs, commands and combined summary |

The native collector reuses `resource_sampler.py`. CA capture reuses
`tests/rewrite/apcpdu_grouped_ca.py`; it does not install another CA framework.
The cycle evaluator imports `poll_cycles.py`, without an equipment runner.
Keep this repository layout when using the CA capture command.

## Interfaces

`build_observer.py` requires `--module-root`, `--base` and a new `--output`
directory. It supports Linux shared-library builds, with `--arch` defaulting
to `linux-x86_64` and `--compiler` to `g++`. Use the module and Base that built
the observed IOC. The receipt identifies direct source/header/library inputs;
it is not a complete compiler or transitive dependency manifest.

For an authorized IOC observation, preload the resulting library with
`LD_PRELOAD` and select an absolute log using `SNMP3_OBSERVER_LOG`.
The constructor calls the existing Runtime qualification API. It removes both
variables before worker execution. It does not override transport symbols.
The IOC and its startup configuration remain the caller's responsibility.

`evaluate_native.py` requires `--native-log`, `--sha256`, `--start-us`,
`--end-us` and a new `--output` JSON. Both window edges are inclusive.
Use `--callback-header` and `--callback-header-sha256` to bind evaluation to
the SDK header used by the capture. The default header is
`/usr/include/net-snmp/library/snmp_api.h`. Unknown or incomplete coverage
returns 2; complete coverage returns 0. A complete timeout observation is
valid evidence and does not by itself mean a successful device operation.

`capture_ca.py` requires `--libca` and `--request`. The request JSON contains
`numeric` and `strings` lists of PV names. The command uses DBR_TIME_DOUBLE
and DBR_TIME_STRING, checks the installed ABI and retains status, severity,
`secondsPastEpoch1990` and `nanoseconds`. Sequential query bounds are monotonic
nanoseconds. Numeric nonfinite values become JSON null. This command only reads.

`evaluate_cycles.py` requires `--input`, `--sha256`, `--window` and a new
`--output`. Window JSON contains integer `start_utc_ns` and `end_utc_ns`.
State, Meas and Slow periods are 5, 10 and 60 seconds. Missing tiers or broken
generation/timestamp evidence return 2. Observed coverage returns 0;
period exceedances remain separate descriptive counts. An absent tier cannot
establish zero overruns. The matcher is scoped to this grouped polling format.

`analyze_observation.py` requires `--run`, a new `--output`, `--inventory`
and `--pv-prefix`. It defaults to `--duration-seconds 900`. The completed run
must contain `result.json`, `observation-window.json`,
`observation-complete.json`, `native-window.json`, `native.stderr`,
`tier-monitor.stdout`, `samples.json` and input-observation snapshots in the
existing grouped observation format. The inventory contains `bindings` with
`rawRecord`, `record` and `tier`. The native callback header must match the
original result's header digest. Selected sends use the requested closed
window; retained terminal observations may extend into the captured drain.

## Fault Helper And Interpretation

`tests/rewrite/helpers/udp_fault.py` owns loopback UDP fault injection.
`--mode drop-protected` forwards requests unchanged and drops only responses
with SNMPv3 authPriv flags. Cleartext discovery REPORTs pass. Controlled mode
reads `SNMP3_RECORD_FAULT_CONTROL`; write `pass` or `drop-protected` to that
file for baseline, loss and recovery. Metadata contains flags and counts,
without decrypted values or credentials. A separate proxy is unnecessary.

Post-submit-to-callback time is not wire RTT. Received callbacks are not
validated-PDU success counts. Resource CPU deltas cover their sampled process
lifetime. Record ages use sequential query-end upper bounds. Unchanged
Busy-suppressed polls, transient alarms and wire packet loss remain unknown.
