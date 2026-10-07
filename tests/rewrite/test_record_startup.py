#!/usr/bin/env python3
"""Verify rejected records and real Runtime preflight failure through IOC exit."""
import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from test_native import ROOT, ARCH, Runner, digest, write_json
from test_record_ca import private_port
from test_record_shutdown import events, wait_for
from run_independence import owns_udp

CASES = ("normal", "production-break", "production-continue", "failure-break", "failure-continue")
PROCESS_SECONDS = 30
INITIAL_VALUES = [71, 19.5, 81, 29.5]
PHASE_FIELDS = ("at_us", "before_free", "restart", "admission", "activation", "created", "exited",
                "joined", "inventory", "active", "pending", "queued", "entered", "completions",
                "entry_open", "detach_allowed", "drain_failed")
CONTEXT_FIELDS = ("record_null", "attached", "handle", "generation", "admission", "published",
                  "native_success", "terminal")


def ints(item, names):
    return isinstance(item, dict) and all(type(item.get(k)) is int for k in names)


def phase_schema(item):
    return (ints(item, PHASE_FIELDS) and isinstance(item.get("state"), str) and
            isinstance(item.get("records"), list) and len(item["records"]) == 4 and all(
                ints(r, ("dpvt_null", "attached", "pact", "stat", "sevr")) and
                type(r.get("value")) in (int, float) for r in item["records"]) and
            isinstance(item.get("contexts"), list) and len(item["contexts"]) in (0, 2) and
            all(ints(c, CONTEXT_FIELDS) for c in item["contexts"]) and
            isinstance(item.get("reservations"), list) and len(item["reservations"]) == 1 and
            all(ints(q, ("count", "bytes", "undelivered", "retirement_pending")) for q in item["reservations"]))


def phase_inventory(observed, failure):
    expected = ["initialized", "started"] + (["attempted", "stop1", "stop2"] if failure else ["completed"])
    expected += ["AtShutdown", "AfterCloseLinks", "AfterStopCallback", "AfterShutdown"]
    if failure:
        expected.append("restart")
    phases = [e for e in observed if e.get("event") == "startup_phase"]
    attempts = [e for e in observed if e.get("event") == "startup_process"]
    return ([e.get("phase") for e in phases] == expected and all(map(phase_schema, phases)) and
            all(a["at_us"] < b["at_us"] for a, b in zip(phases, phases[1:])) and
            [e.get("slot") for e in attempts] == [0, 1] and all(ints(e, ("status",)) for e in attempts) and
            not any(e.get("event") == "startup_observer_error" for e in observed))


def companion_checks(check, observed, failure):
    valid = phase_inventory(observed, failure)
    check("startup-phase-inventory", valid)
    if not valid:
        return
    phases = {e["phase"]: e for e in observed if e.get("event") == "startup_phase"}
    initial, started = phases["initialized"], phases["started"]
    check("startup-two-contexts-before-runtime", initial["inventory"] == 2 and len(initial["contexts"]) == 2 and
          all(r["attached"] == 1 and r["pact"] == 0 for r in initial["records"][:2]) and
          all(c["attached"] == 1 and c["handle"] > 0 for c in initial["contexts"]) and
          len({c["handle"] for c in initial["contexts"]}) == 2 and initial["state"] == "Cold" and
          all(initial[k] == 0 for k in ("active", "completions", "admission", "created")))
    check("startup-rejected-records-unbound", all(r["dpvt_null"] == 1 for e in phases.values()
                                                for r in e["records"][2:]))
    check("startup-initial-values", [r["value"] for r in initial["records"]] == INITIAL_VALUES)
    check("startup-no-isolated-cleanup", all(e["before_free"] == 0 for e in phases.values()))
    before = phases["AtShutdown"]
    detached = [phases[k] for k in ("AfterCloseLinks", "AfterStopCallback", "AfterShutdown")]
    if failure:
        detached.append(phases["restart"])
    check("startup-entry-closed-before-detach", before["entry_open"] == 0 and before["entered"] == 0 and
          before["detach_allowed"] == 1 and all(r["attached"] == 1 for r in before["records"][:2]))
    check("startup-record-pointers-cleared", all(r["dpvt_null"] == 1 for e in detached for r in e["records"][:2]))
    check("startup-context-pointers-cleared", all(len(e["contexts"]) == 2 and
          all(c["record_null"] == 1 for c in e["contexts"]) for e in detached))
    check("startup-storage-retained-after-join", phases["AfterStopCallback"]["inventory"] == 2)
    check("startup-storage-retained-after-shutdown", phases["AfterShutdown"]["inventory"] == 2)
    check("startup-context-inventory-preserved", all(e["inventory"] == 2 for e in phases.values()))
    check("startup-handles-preserved", all(len(e["contexts"]) == 2 and
          [c["handle"] for c in e["contexts"]] == [c["handle"] for c in initial["contexts"]]
          for e in phases.values()))
    check("startup-rejected-values-preserved", all([r["value"] for r in e["records"][2:]] == INITIAL_VALUES[2:]
                                                 for e in phases.values()))
    if failure:
        failed = [e for name, e in phases.items() if name != "initialized"]
        check("startup-preflight-failed-before-processing", started["state"] == "Failed" and
              all(started[k] == 0 for k in ("admission", "created", "activation", "active", "completions")))
        check("startup-failed-state-preserved", all(e["state"] == "Failed" and e["admission"] == 0 for e in failed))
        check("startup-no-runtime-thread", all(e[k] == 0 for e in phases.values()
                                               for k in ("activation", "created", "exited", "joined")))
        attempted = phases["attempted"]
        check("startup-admission-refused-with-alarm", [r["stat"] for r in attempted["records"][:2]] == [1, 2] and
              all(r["sevr"] == 3 and r["pact"] == 0 for r in attempted["records"][:2]) and
              len(attempted["contexts"]) == 2 and all(c["generation"] == c["admission"] == 0
                                                     for c in attempted["contexts"]))
        check("startup-no-accepted-work", all(e[k] == 0 for e in phases.values()
              for k in ("active", "pending", "queued", "entered", "completions", "drain_failed")) and
              all(q[k] == 0 for e in phases.values() for q in e["reservations"]
                  for k in ("count", "bytes", "undelivered", "retirement_pending")))
        check("startup-failed-values-preserved", all([r["value"] for r in e["records"]] == INITIAL_VALUES
                                                     for e in phases.values()))
        check("startup-no-native-publication", all(len(e["contexts"]) == 2 and
              all(c[k] == 0 for c in e["contexts"] for k in
                  ("published", "native_success", "terminal", "generation", "admission")) for e in phases.values()))
        final, restart = phases["AfterShutdown"], phases["restart"]
        stable = (*PHASE_FIELDS[3:], "state", "records", "contexts", "reservations")
        check("startup-restart-refused", restart["restart"] == 0 and all(restart[k] == final[k] for k in stable))
    else:
        complete = phases["completed"]
        check("startup-valid-records-complete", started["state"] == "Running" and complete["completions"] == 2 and
              complete["active"] == complete["entered"] == 0 and len(complete["contexts"]) == 2 and
              all(c["generation"] == 1 for c in complete["contexts"]) and
              [c["native_success"] for c in complete["contexts"]] == [1, 0] and
              [c["published"] for c in complete["contexts"]] == [1, 0] and
              all(r["pact"] == r["sevr"] == 0 for r in complete["records"][:2]) and
              [r["value"] for r in complete["records"][:2]] == [-123, 19.5])
        check("startup-normal-stopped", all(e["state"] == "Stopped" and e["completions"] == 2 and
              e["active"] == e["entry_open"] == e["drain_failed"] == 0 and
              e["created"] == e["exited"] == e["joined"] == 1 for e in [before, *detached]))


def final_report(text, kind):
    tail = text.split("snmp3 lifecycle: fallback\n")
    if len(tail) != 2:
        raise ValueError("missing or duplicate fallback")
    lines = re.findall(r"snmp3 " + kind + r": ([^\n]+)", tail[1])
    if len(lines) != 1:
        raise ValueError("missing or duplicate final report")
    return {k: v if k == "state" else int(v) for k, v in (token.split("=", 1) for token in lines[0].split())}


def run_case(case, output, products, sanitizers):
    output.mkdir(mode=0o700)
    runner = Runner(output, products, sanitizers)
    failure, production = case != "normal", case.startswith("production")
    policy = "continue" if case.endswith("continue") else "break"
    executable = "snmp3Ioc" if production else "snmp3StartupTest"
    base = Path(next(line.split("=", 1)[1].strip() for line in
                    (ROOT / "configure/RELEASE.local").read_text().splitlines()
                    if line.strip().startswith("EPICS_BASE") and "=" in line))
    children, receipts, observed = [], [], []
    aborted = False

    def launch(name, argv):
        streams = [(output / (name + suffix)).open("xb") for suffix in (".stdout", ".stderr")]
        try:
            child = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=streams[0], stderr=streams[1], env=runner.env)
        except Exception:
            for stream in streams:
                stream.close()
            raise
        item = {"name": name, "argv": argv, "child": child, "streams": streams, "done": False}
        children.append(item)
        return item

    def finish(item):
        if item["done"]:
            return
        child = item["child"]
        if item["name"] == "repeater" and child.poll() is None:
            child.terminate()
        forced = False
        try:
            code = child.wait(timeout=PROCESS_SECONDS)
        except subprocess.TimeoutExpired:
            forced = True
            child.kill()
            code = child.wait(timeout=PROCESS_SECONDS)
        for stream in item["streams"]:
            stream.close()
        expected = (0, -15) if item["name"] == "repeater" else ((1,) if failure else (0,))
        receipt = {"name": item["name"], "argv": item["argv"], "pid": child.pid, "returncode": code,
                   "child_reaped": child.returncode is not None, "forced_cleanup": forced,
                   "expected_exit": code in expected and not forced,
                   "loaded_libraries": runner.loader_identity(output / (item["name"] + ".stderr"))}
        write_json(output / (item["name"] + ".receipt.json"), receipt)
        receipts.append(receipt)
        runner.check(item["name"] + ":expected-exit", receipt["expected_exit"])
        item["done"] = True

    try:
        peer, secrets = runner.agent(4)
        proxy, _ = runner.fault("startup-proxy", 4, peer, "pass")
        server, repeater = private_port(), private_port()
        while server == repeater:
            repeater = private_port()
        runner.env.update(EPICS_CA_SERVER_PORT=str(server), EPICS_CA_REPEATER_PORT=str(repeater),
                          EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST="127.0.0.1:" + str(server),
                          EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
                          EPICS_CAS_BEACON_ADDR_LIST="127.0.0.1:" + str(repeater))
        write_json(output / "ports.json", {"server": server, "repeater": repeater})
        config = output / "records.json"
        write_json(config, {"schema": 1, "profiles": [{"id": "Local", "version": "2c",
                   "communityFile": str(secrets["community"]), "timeoutMs": 1000, "retries": 0}],
                   "endpoints": [{"id": "Local", "address": "127.0.0.1", "port": int(proxy.rsplit(":", 1)[1]),
                                  "profile": "Local"}], "bindings": [
                   {"id": name, "endpoint": "Local", "operation": op, "valueType": tag, "capacity": 1,
                    "oid": f"1.3.6.1.4.1.53864.4.{index}.0"}
                   for name, op, tag, index in (("StartupRead", "get", "integer", 1),
                                                ("StartupWrite", "set", "opaqueFloat", 9))]})
        worker = output / "missing-worker" if failure else products / "snmp3Worker"
        if failure and worker.exists():
            raise RuntimeError("failure boundary already exists")
        script = output / "startup.cmd"
        setup = "" if production else f'snmp3StartupSetup({int(failure)},"{output}/worker.stderr",{int(sanitizers)})\n'
        script.write_text(f'on error {policy}\n' + f'dbLoadDatabase("{ROOT}/dbd/{executable}.dbd")\n' +
                          f'{executable}_registerRecordDeviceDriver(pdbbase)\n' + setup +
                          f'snmp3Load("{config}","{products}/snmp3NativeProbe")\n' +
                          f'snmp3WorkerPath("{worker}")\n' +
                          f'dbLoadRecords("{ROOT}/tests/rewrite/db/record-startup.db","P=Startup_")\n' +
                          'iocInit\n' + ('snmp3StartupExercise\n' if not failure else '') +
                          'echo STARTUP_SCRIPT_COMPLETED\nsnmp3RuntimeReport\n')
        rep = launch("repeater", [str(base / "bin" / ARCH / "caRepeater")])
        if not wait_for(lambda: owns_udp(rep["child"].pid, repeater)):
            raise RuntimeError("private repeater unavailable")
        finish(launch("ioc", [str(products / executable), str(script)]))
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"class": type(error).__name__, "message": str(error)})
    finally:
        for item in reversed(children):
            finish(item)
        runner.stop()
        runner.scan()
    try:
        text = (output / "ioc.stdout").read_text()
        stderr = (output / "ioc.stderr").read_text()
        observed = events(text)
        final, requests = final_report(text, "runtime"), final_report(text, "records")
        write_json(output / "observations.json", {"events": observed, "final": final, "requests": requests})
        hooks = re.findall(r"snmp3 hook: (AtShutdown|AfterStopScan|AfterStopCallback|AfterShutdown)", text)
        runner.check("startup-real-shutdown-order", hooks == ["AtShutdown", "AfterStopScan", "AfterStopCallback", "AfterShutdown"])
        runner.check("startup-script-policy", ("STARTUP_SCRIPT_COMPLETED\n" in text) == (policy == "continue" or not failure))
        runner.check("startup-rejection-diagnostics", all(stderr.count("record initialization rejected: Startup_" + name) == 1
                     for name in ("RejectedInput", "RejectedOutput")) and
                     "record initialization rejected: Startup_Input" not in stderr and
                     "record initialization rejected: Startup_Output" not in stderr)
        wire = events((output / "startup-proxy.stdout").read_text())
        wire_requests = [e for e in wire if e.get("event") == "fault_request"]
        if failure:
            runner.check("startup-real-preflight-failure", text.count("preflight-failed activation=0") == 1 and
                         stderr.count("runtime startup failed; admission closed") == 1 and
                         stderr.count("snmp3Ioc: startup script failed") == 1)
            runner.check("startup-fallback-failed", final == {"state": "Failed", "admission": 0, "activation": 0,
                         "created": 0, "exited": 0, "joined": 0})
            runner.check("startup-fallback-idle-retained", requests["contexts"] == 2 and requests["detachAllowed"] == 1 and
                         all(v == 0 for k, v in requests.items() if k not in ("contexts", "detachAllowed")))
            runner.check("startup-no-worker-or-wire", "snmp3 worker:" not in text and not wire_requests)
        else:
            agent = events((output / "agent-ipv4-1.stdout").read_text())
            runner.check("startup-real-GET-SET", sorted(e["command"] for e in wire_requests) == [160, 163] and
                         len([e for e in wire if e.get("event") == "fault_response"]) == 2 and
                         any(e.get("event") == "agent_set_value" and e.get("index") == 9 and
                             e.get("value") == "19.5" for e in agent))
            worker = final_report(text, "worker")
            runner.check("startup-worker-reaped", worker["launches"] == worker["reaps"] == 1 and
                         worker["forced"] == worker["pid"] == worker["ready"] == worker["closing"] == 0)
        if not production:
            companion_checks(runner.check, observed, failure)
    except (ValueError, KeyError, TypeError, OSError) as error:
        runner.check("startup-observations-complete", False)
        write_json(output / "parse-error.json", {"class": type(error).__name__, "message": str(error)})
    fixture_receipts = [json.loads((output / (entry[0] + ".receipt.json")).read_text()) for entry in runner.children]
    cleanup = len(receipts) == len(children) and all(r["child_reaped"] and r["expected_exit"] for r in receipts) and all(
        r["child_reaped"] and not r["forced_cleanup"] and r["returncode"] == 0 for r in fixture_receipts)
    runner.check("startup-all-child-receipts-clean", cleanup)
    libraries = next((r["loaded_libraries"] for r in receipts if r["name"] == "ioc"), {})
    runner.check("startup-IOC-native-free", bool(libraries) and not any("netsnmp" in p for p in libraries))
    sources = list((ROOT / "snmp3App/src").glob("*.[ch]*"))
    sources += [ROOT / "tests/rewrite" / name for name in ("StartupTest.cpp", "db/record-startup.db",
                "snmp3StartupTestRegistrar.dbd", "Makefile", "build_r5_sanitizers.py", "test_native.py",
                "NativeAgent.cpp", "helpers/udp_fault.py", "test_record_shutdown.py", "test_record_ca.py", "run_independence.py")]
    sources += [Path(__file__).resolve(), ROOT / "dbd" / (executable + ".dbd")]
    sources += [products / name for name in (executable, "snmp3Worker", "snmp3NativeAgent", "snmp3NativeProbe")]
    result = {"case": case, "passed": not aborted and all(c["passed"] for c in runner.checks),
              "aborted": aborted, "checks": runner.checks, "observations": observed, "cleanup_passed": cleanup,
              "children": receipts + fixture_receipts, "ioc_libraries": libraries,
              "inputs": {str(p): digest(p) for p in sources if p.is_file()},
              "scope": "Actual ai/ao initialization and Runtime preflight failure; no isolated cleanup or thread-creation claim"}
    write_json(output / "results.json", result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--case", choices=(*CASES, "all"), default="all")
    args = parser.parse_args()
    products = args.products.resolve() if args.products else ROOT / "bin" / ARCH
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    cases = CASES if args.case == "all" else (args.case,)
    results = [run_case(case, output / case, products, args.sanitizers) for case in cases]
    passed = all(r["passed"] for r in results)
    write_json(output / "results.json", {"passed": passed, "argv": sys.argv, "cases": [
        {"case": r["case"], "passed": r["passed"], "result": str(output / r["case"] / "results.json")} for r in results]})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
