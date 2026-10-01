#!/usr/bin/env python3
"""Execute the real scheduler/IPC/worker/native path against shipped native agents."""
import argparse
from collections import Counter
import json
import subprocess
import time
from pathlib import Path
from test_native import ROOT, Runner, digest, write_json


def ioc_run(runner, config):
    argv = [str(runner.products / "snmp3RuntimeTest"), str(config),
            str(runner.products / "snmp3NativeProbe"), str(runner.products / "snmp3Worker"),
            str(runner.output / "ioc-native.stderr"), str(ROOT / "dbd/snmp3RuntimeTest.dbd"),
            str(ROOT / "tests/rewrite/db/lifecycle.db")]
    start = time.monotonic_ns()
    with (runner.output / "ioc.stdout").open("xb") as stdout:
        with (runner.output / "ioc.stderr").open("xb") as stderr:
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=runner.env)
            forced = False
            try:
                code = child.wait(timeout=30)
            except subprocess.TimeoutExpired:
                forced = True
                child.kill()
                code = child.wait()
    write_json(runner.output / "ioc.receipt.json", {"argv": argv, "pid": child.pid, "returncode": code,
               "forced_cleanup": forced, "child_reaped": child.returncode is not None,
               "elapsed_ns": time.monotonic_ns() - start, "product_sha256": digest(Path(argv[0]))})
    runner.check("ioc:real-exit", code == 0 and not forced)
    text = (runner.output / "ioc.stdout").read_text()
    events = [json.loads(line) for line in text.splitlines() if line.startswith('{"event":')]
    summary = [e for e in events if e["event"] == "ioc_runtime_summary"]
    runner.check("ioc:two-real-activations-exact-join", len(summary) == 1 and
                 summary[0].get("activations") == 2 and summary[0].get("joined") == 2)
    incomplete = [e for e in events if e["event"] == "incomplete_stop"]
    runner.check("ioc:unconsumed-terminal-retains-full-charge", len(incomplete) == 1 and
                 incomplete[0].get("count") == 1 and incomplete[0].get("bytes") == 4672 and
                 incomplete[0].get("joined") == 1)
    runner.check("ioc:nonblocking-reconciliation-without-second-join", any(
                 e["event"] == "reconciled_stop" and e.get("joined") == 1 for e in events))
    for activation in (1, 2):
        ready = text.index("snmp3 lifecycle: ready activation=" + str(activation))
        process = text.index("dbProcess of 'Runtime_PiniProbe'", ready)
        after = text.index("snmp3 hook: AfterInitialProcess", process)
        runner.check("ioc:ready-before-initial-process:" + str(activation), ready < process < after)
    launches = [e for e in events if e["event"] == "supervision" and e.get("code") == 2]
    reaps = [e for e in events if e["event"] == "supervision" and e.get("code") == 7]
    concurrent = [e for e in events if e["event"] == "concurrent_stop"]
    runner.check("ioc:concurrent-stop-one-join-retained-native-owner", len(concurrent) == 1 and
                 concurrent[0].get("joined") == 2 and concurrent[0].get("consumed_retained") is True)
    forced_pid = concurrent[0]["pid"] if concurrent else None
    runner.check("ioc:every-real-worker-reaped", len(launches) == 4 and len(reaps) == 4 and
                 {e["pid"] for e in launches} == {e["pid"] for e in reaps} and
                 all(e["detail"] == (9 if e["pid"] == forced_pid else 0) for e in reaps))
    runner.loader_identity(runner.output / "ioc.stderr")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ioc", action="store_true", help="Include actual IOC hook/reuse/incomplete-stop execution")
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    runner = Runner(output, args.products.resolve() if args.products else None, args.sanitizers)
    aborted = False
    try:
        profiles, endpoints, bindings = [], [], []
        for family in (4, 6):
            peer, paths = runner.agent(family)
            name = "Address" + str(family)
            profiles.append({"id": name, "version": "2c", "communityFile": str(paths["community"]),
                             "timeoutMs": 100, "retries": 1, "maxVarbinds": 16})
            endpoints.append({"id": name, "address": "127.0.0.1" if family == 4 else "::1",
                              "port": int(peer.rsplit(":", 1)[1]), "profile": name})
            bindings.append({"id": name + "Read", "endpoint": name, "oid": "1.3.6.1.4.1.53864.4.1.0",
                             "operation": "get", "valueType": "integer", "capacity": 1})
        config = output / "two-address.config.json"
        write_json(config, {"schema": 1, "profiles": profiles, "endpoints": endpoints, "bindings": bindings})
        events = runner.invoke("two-address", [str(runner.products / "snmp3SupervisorTest"),
                               str(config), str(runner.products / "snmp3NativeProbe"),
                               str(runner.products / "snmp3Worker"), str(output / "native.stderr")])
        summaries = [e for e in events if e.get("event") == "summary"]
        runner.check("real-two-address-consumption", len(summaries) == 1 and summaries[0].get("consumed") == 4
                     and summaries[0].get("addresses") == 2 and summaries[0].get("settled") is True)
        observed = [e for e in events if e.get("event") == "supervision"]
        ready = [e for e in observed if e.get("code") == 3]
        runner.check("one-ready-worker-per-address", {e["address"] for e in ready} == {1, 2} and len(ready) == 2)
        reaps = [e for e in observed if e.get("code") == 7]
        launches = [e for e in observed if e.get("code") == 2]
        runner.check("every-worker-exact-reap", len(reaps) == 2 and len(launches) == 2 and
                     {e["pid"] for e in reaps} == {e["pid"] for e in launches} and all(e["detail"] == 0 for e in reaps))
        runner.check("no-forced-stop", not any(e.get("code") in (8, 9) for e in observed))
        native = (output / "native.stderr").read_text()
        runner.check("two-real-native-closes", sum(line.startswith("native_event native_close ") for line in native.splitlines()) == 2)
        runner.check("native-events-no-drop", "native_dropped" not in native and runner.tokens and
                     not any(token in native.encode() for token in runner.tokens))
        for name, _, _, _, _ in runner.children:
            handler = [json.loads(line) for line in (output / (name + ".stdout")).read_text().splitlines()
                       if '"event":"agent_handler"' in line]
            counts = Counter(e["request_id"] for e in handler)
            runner.check(name + ":actual-equal-deadline-two-member-batch", len(counts) == 1 and list(counts.values()) == [2])
        if args.ioc:
            ioc_run(runner, config)
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"exception_class": type(error).__name__})
    finally:
        runner.stop()
        runner.scan()
    inputs = sorted((ROOT / "snmp3App").rglob("*.cpp")) + sorted((ROOT / "snmp3App").rglob("*.h"))
    inputs += [Path(__file__).resolve(), ROOT / "tests/rewrite/SupervisorTest.cpp",
               ROOT / "tests/rewrite/test_native.py", ROOT / "tests/rewrite/NativeAgent.cpp"]
    inputs += [runner.products / name for name in ("snmp3SupervisorTest", "snmp3Worker",
                "snmp3NativeAgent", "snmp3NativeProbe")]
    if args.ioc:
        inputs += [ROOT / "tests/rewrite/RuntimeTest.cpp", ROOT / "dbd/snmp3RuntimeTest.dbd",
                   runner.products / "snmp3RuntimeTest", ROOT / "tests/rewrite/db/lifecycle.db"]
    result = {"scope": "Real scheduler/IPC/worker/native path; IOC hook/reuse/incomplete-stop included when requested; fault/sanitizer/full R5 gates remain pending",
              "ioc_requested": args.ioc,
              "checks": runner.checks, "aborted": aborted,
              "sha256": {str(path): digest(path) for path in inputs}, "loaded_libraries": runner.identities,
              "passed": not aborted and all(c["passed"] for c in runner.checks)}
    write_json(output / "results.json", result)
    print(("PASS: " if result["passed"] else "FAIL: ") + str(output / "results.json"))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
