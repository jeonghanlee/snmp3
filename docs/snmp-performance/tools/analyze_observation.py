#!/usr/bin/env python3
"""Evaluate a closed observation window separately from startup and bounded drain."""

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import sys

NATIVE_TOOL = Path(__file__).with_name("evaluate_native.py")
CYCLE_TOOL = Path(__file__).with_name("evaluate_cycles.py")
EPICS_EPOCH_SECONDS = 631152000


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    return json.loads(path.read_text())


def save(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def distribution(values):
    ordered = sorted(values)
    return {"samples": len(ordered), "p50": statistics.median(ordered) if ordered else None,
            "p95": ordered[max(0, (95 * len(ordered) + 99) // 100 - 1)] if ordered else None,
            "max": max(ordered) if ordered else None}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--pv-prefix", required=True)
    parser.add_argument("--duration-seconds", type=int, default=900)
    parser.add_argument("--callback-header", type=Path, default=Path("/usr/include/net-snmp/library/snmp_api.h"))
    args = parser.parse_args()
    if args.duration_seconds <= 0:
        parser.error("A positive observation duration is required")
    duration_ns = args.duration_seconds * 1000000000
    run, output = args.run.resolve(), args.output.resolve()
    assert not output.exists()
    original = load(run / "result.json")
    assert original["status"] == "Passed" and original["native_returncode"] == 0
    original_result_sha = sha(run / "result.json")
    window = load(run / "observation-window.json")
    assert window["intended_seconds"] == args.duration_seconds and load(run / "observation-complete.json")["elapsed_seconds"] >= args.duration_seconds
    output.mkdir(mode=0o700)
    inputs = output / "inputs"
    inputs.mkdir(mode=0o700)
    for name in ("native.stderr", "tier-monitor.stdout", "observation-window.json", "native-window.json"):
        shutil.copy2(run / name, inputs / name)
    start_us = (window["start_ns"] + 999) // 1000
    stop_us = start_us + duration_ns // 1000
    cohort = set()
    for row in original["native_summary"]["requests"]:
        sent = row["send"]
        if start_us <= sent["at_us"] < stop_us:
            cohort.add((sent["pid"], sent["address"], sent["epoch"], sent["session"], sent["token"]))
    assert cohort
    retained = []
    for line in (inputs / "native.stderr").read_text().splitlines(True):
        fields = line.split()
        if not fields or fields[0] != "native_event":
            retained.append(line)
            continue
        assert len(fields) == 10
        session, token, operation, detail, stamp, address, epoch, pid = map(int, fields[2:])
        key = (pid, address, epoch, session, token)
        if key in cohort or fields[1] in ("native_open", "native_close"):
            retained.append(line)
    (inputs / "native-cohort.stderr").write_text("".join(retained))
    tier_window = {"start_utc_ns": window["start_utc_ns"],
                   "end_utc_ns": window["start_utc_ns"] + duration_ns - 1}
    save(inputs / "tier-window.json", tier_window)
    members = {p.name: sha(p) for p in inputs.iterdir()}
    tool_paths = [Path(__file__), NATIVE_TOOL, NATIVE_TOOL.with_name("native_observation.py"),
                  CYCLE_TOOL, CYCLE_TOOL.with_name("poll_cycles.py"),
                  Path(__file__).with_name("resource_sampler.py"), args.inventory, args.callback_header]
    tools = {str(p): sha(p) for p in tool_paths}
    save(output / "input-manifest.json", {"members": members, "tools": tools})

    def execute(label, argv):
        assert {p.name for p in inputs.iterdir()} == set(members)
        assert all(sha(inputs / name) == expected for name, expected in members.items())
        assert all(sha(Path(name)) == expected for name, expected in tools.items())
        child = subprocess.run(argv, capture_output=True)
        (output / (label + ".stdout")).write_bytes(child.stdout)
        (output / (label + ".stderr")).write_bytes(child.stderr)
        save(output / (label + ".command.json"), {"argv": argv, "native_returncode": child.returncode})
        assert child.returncode == 0, label

    drain_end = load(inputs / "native-window.json")["end_us"]
    execute("native", [sys.executable, "-B", str(NATIVE_TOOL), "--native-log", str(inputs / "native-cohort.stderr"),
        "--sha256", members["native-cohort.stderr"], "--start-us", str(start_us), "--end-us", str(drain_end),
        "--output", str(output / "native.json"), "--callback-header", str(args.callback_header),
        "--callback-header-sha256", original["native_summary"]["callback_header_sha256"]])
    execute("cycles", [sys.executable, "-B", str(CYCLE_TOOL), "--input", str(inputs / "tier-monitor.stdout"),
        "--sha256", members["tier-monitor.stdout"], "--window", str(inputs / "tier-window.json"),
        "--output", str(output / "cycles.json")])
    native, cycles = load(output / "native.json"), load(output / "cycles.json")
    assert native["attempted"] == len(cohort) and native["coverage"] == "COMPLETE"
    assert all(cycles["tiers"][tier]["observed_cycles"] for tier in ("State", "Meas", "Slow"))
    samples = load(run / "samples.json")
    observed = [s for s in samples if window["start_ns"] <= s["start_ns"] < window["start_ns"] + duration_ns]
    processes = {}
    for name in {p["process"] for s in samples for p in s["processes"]}:
        series = [(s, p) for s in samples for p in s["processes"] if p["process"] == name]
        first_s, first_p = series[0]
        last_s, last_p = series[-1]
        assert len({p["pid"] for _, p in series}) == 1
        elapsed = (last_s["start_ns"] - first_s["start_ns"]) / 1e9
        cpu = last_p["cpu_s"] - first_p["cpu_s"]
        processes[name] = {"pid": first_p["pid"], "sampled_seconds": elapsed,
            "rss_kb_start": first_p["rss_kb"], "rss_kb_end": last_p["rss_kb"],
            "rss_kb_max": max(p["rss_kb"] for _, p in series), "cpu_seconds_delta": cpu,
            "mean_one_core_cpu_percent": cpu / elapsed * 100,
            "thread_range": [min(p["threads"] for _, p in series), max(p["threads"] for _, p in series)],
            "fd_range": [min(p["fds"] for _, p in series), max(p["fds"] for _, p in series)]}
    entries = load(args.inventory)["bindings"]
    roles = {args.pv_prefix + e[key] + ".SEVR": e["tier"] + ":" + role
             for e in entries for key, role in (("rawRecord", "raw"), ("record", "public"))}
    ages, missing = defaultdict(list), defaultdict(int)
    for path in run.glob("*-input-observation.json"):
        snapshot = load(path)
        begin = snapshot["start_utc_ns"]
        if not tier_window["start_utc_ns"] <= begin <= tier_window["end_utc_ns"]:
            continue
        end = begin + snapshot["end_ns"] - snapshot["start_ns"]
        for pv, value in snapshot["channels"].items():
            if value["secondsPastEpoch1990"] == 0:
                missing[roles[pv]] += 1
                continue
            stamp = (value["secondsPastEpoch1990"] + EPICS_EPOCH_SECONDS) * 1000000000 + value["nanoseconds"]
            ages[roles[pv]].append(max(0, end - stamp) / 1e9)
    gaps = [(b["start_ns"] - a["start_ns"]) / 1e9 for a, b in zip(observed, observed[1:])]
    result = {"unit": original["unit"], "run": str(run), "original_result_sha256": original_result_sha,
        "intended_window_seconds": args.duration_seconds, "actual_observed_seconds": load(run / "observation-complete.json")["elapsed_seconds"],
        "native_send_window_monotonic_us": {"inclusive_start": start_us, "exclusive_stop": stop_us,
                                           "terminal_capture_through": drain_end},
        "native": {k: native[k] for k in ("accepted", "received_callback", "timeout", "send_failed", "dropped_events", "timeout_percent")},
        "cycles": {k: {n: v[n] for n in ("observed_cycles", "p50_ms", "p95_ms", "max_ms", "period_exceedances", "unfinished_generations")}
                   for k, v in cycles["tiers"].items()},
        "resource_interval_seconds": distribution(gaps), "resource_samples_in_window": len(observed),
        "process_lifetime_resources": processes,
        "record_age_upper_bound_seconds": {k: distribution(v) for k, v in ages.items()},
        "missing_record_timestamps": dict(missing), "input_manifest_sha256": sha(output / "input-manifest.json"),
        "limits": ["Native requests selected by send timestamp; their retained terminals may occur during bounded drain",
                   "Resource CPU deltas cover the full sampled process lifetime, including startup/drain",
                   "Record ages use sequential query-end upper bounds, not an atomic group timestamp",
                   "Busy-suppressed due ticks, unobserved transient READ alarms and wire packet loss/RTT are not established",
                   "No legacy-equivalence or write result"]}
    assert sha(run / "result.json") == original_result_sha
    save(output / "summary.json", result)
    print(json.dumps({k: result[k] for k in ("unit", "native", "cycles", "resource_samples_in_window")}, indent=2))


if __name__ == "__main__":
    main()
