#!/usr/bin/env python3
"""Verify pending and retained record work through actual non-isolated IOC exit."""
import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from test_native import ROOT, ARCH, Runner, digest, write_json
from test_record_ca import private_port
from run_independence import owns_udp

READY_SECONDS = 10
PROCESS_SECONDS = 40
NATIVE_TIMEOUT_MS = 30000
DEADLINE_MS = 60000
PHASES = ("prepared", "queued", "AtShutdown", "AfterCloseLinks",
          "AfterStopCallback", "AfterShutdown", "restart")
REQUEST_FIELDS = ("contexts", "active", "pending", "queued", "running", "inert", "entered",
                  "enqueueFailures", "completions", "entryOpen", "drainFailed", "detachAllowed")
RUNTIME_FIELDS = ("admission", "activation", "created", "exited", "joined")
CONTEXT_FIELDS = ("record_null", "attached", "published", "native_success", "revision", "activation", "binding", "generation",
                  "admission", "terminal", "sent", "identity_match", "outcome")


def events(text):
    result = []
    for line in text.splitlines():
        start = line.find('{"event":')
        if start >= 0:
            result.append(json.loads(line[start:]))
    return result


def integer_fields(value, fields):
    return isinstance(value, dict) and all(type(value.get(k)) is int for k in fields)


def phase_schema(event):
    if not (event.get("case") == "retained" and isinstance(event.get("phase"), str) and
            integer_fields(event, ("at_us", "ready", "watchdog", "before_free", "restart", "queue_status",
                                   "queue_size", "queue_used", "result_batches", "retired_batches")) and
            integer_fields(event.get("runtime"), RUNTIME_FIELDS) and
            isinstance(event["runtime"].get("state"), str) and
            integer_fields(event.get("requests"), REQUEST_FIELDS)):
        return False
    records, contexts, reservations = (event.get(k) for k in ("records", "contexts", "reservations"))
    return (isinstance(records, list) and len(records) == 3 and all(
        integer_fields(r, ("dpvt_null", "attached", "pact")) and
        type(r.get("value")) in (int, float) for r in records) and
        isinstance(contexts, list) and len(contexts) in (0, 2) and
        all(integer_fields(c, CONTEXT_FIELDS) for c in contexts) and
        isinstance(reservations, list) and len(reservations) == 1 and
        all(integer_fields(q, ("address", "count", "bytes", "undelivered", "retirement_pending"))
            for q in reservations))


def phase_inventory(observed):
    phases = [e for e in observed if e.get("event") == "shutdown_phase"]
    return (tuple(e.get("phase") for e in phases) == PHASES and all(map(phase_schema, phases)) and
            all(a["at_us"] < b["at_us"] for a, b in zip(phases, phases[1:])) and
            not any(e.get("event") == "shutdown_observer_error" for e in observed))


def retained_checks(check, observed):
    valid = phase_inventory(observed)
    check("retained-phase-inventory", valid)
    if not valid:
        return
    phases = {e["phase"]: e for e in observed if e.get("event") == "shutdown_phase"}
    prepared, queued, stop, close, joined, final, restart = (phases[k] for k in PHASES)
    check("retained-no-watchdog-or-isolated-free", all(not e["watchdog"] and not e["before_free"]
                                                    for e in phases.values()))
    check("retained-fixture-prepared", prepared["ready"] == 1 and prepared["requests"]["contexts"] == 2 and
          prepared["requests"]["active"] == 0 and prepared["requests"]["completions"] == 0 and
          [r["value"] for r in prepared["records"]] == [71, 19.5, 0] and
          all(r["attached"] for r in prepared["records"][:2]))
    check("retained-native-retired-before-exit", queued["ready"] == 1 and
          queued["result_batches"] == queued["retired_batches"] == 2 and
          len(queued["contexts"]) == 2 and all(c["terminal"] == c["sent"] == c["identity_match"] == 1
          and c["outcome"] == 1 for c in queued["contexts"]) and
          all(q["count"] == q["undelivered"] == 2 and q["bytes"] > 0 and q["retirement_pending"] == 0
              for q in queued["reservations"]))
    check("retained-two-queued-before-exit", all(queued["requests"][k] == v for k, v in
          {"contexts": 2, "active": 2, "queued": 2, "pending": 0, "running": 0,
           "entered": 0, "completions": 0, "entryOpen": 1}.items()) and
          all(r["pact"] == 1 for r in queued["records"][:2]) and queued["queue_used"] == 2)
    check("retained-drain-expiry-recorded", stop["requests"]["drainFailed"] == 1)
    check("retained-entry-closed-before-detach", all(stop["requests"][k] == v for k, v in
          {"contexts": 2, "active": 2, "queued": 2, "entryOpen": 0, "entered": 0,
           "detachAllowed": 1, "completions": 0}.items()))
    check("retained-record-dpvt-detached", all(r["dpvt_null"] == 1
          for e in (close, joined, final, restart) for r in e["records"][:2]))
    check("retained-context-record-detached", all(len(e["contexts"]) == 2 and
          all(c["record_null"] == 1 for c in e["contexts"]) for e in (close, joined, final, restart)))
    check("retained-contexts-after-callback-join", joined["requests"]["contexts"] == 2)
    check("retained-contexts-after-shutdown", final["requests"]["contexts"] == 2)
    check("retained-late-callbacks-inert", all(e["requests"]["inert"] == 2 for e in (joined, final, restart)))
    check("retained-terminal-preserved", all(len(e["contexts"]) == 2 and
          all(c["terminal"] == c["sent"] == c["identity_match"] == 1 and c["outcome"] == 1
              for c in e["contexts"]) for e in (queued, stop, close, joined, final, restart)))
    check("retained-ownership-after-detach", all(all(e["requests"][k] == v for k, v in
          {"contexts": 2, "active": 2, "completions": 0}.items()) and
          e["reservations"] == queued["reservations"] for e in (stop, close, joined, final)))
    check("retained-no-record-publication", all([r["value"] for r in e["records"]] ==
          [r["value"] for r in queued["records"]] == [71, 19.5, 0] and len(e["contexts"]) == 2 and
          all(c["published"] == c["native_success"] == 0 for c in e["contexts"])
          for e in (queued, stop, close, joined, final)))
    identities = lambda e: [(c["revision"], c["activation"], c["binding"], c["generation"], c["admission"]) for c in e["contexts"]]
    check("retained-generation-identities", len(identities(queued)) == 2 and
          len(set(identities(queued))) == 2 and all(all(n > 0 for n in ident) for ident in identities(queued)) and
          all(identities(e) == identities(queued) for e in (stop, close, joined, final)))
    check("retained-joined-consumer-with-live-queue", all(e["queue_status"] == 0 and e["queue_size"] >= 3 and
          e["queue_used"] == 0 and all(e["requests"][k] == 0 for k in ("pending", "queued", "running", "entered"))
          for e in (joined, final)))
    check("retained-incomplete-stopped", all(e["runtime"]["state"] == "IncompleteStopped" and
          all(e["runtime"][k] == v for k, v in {"admission": 0, "created": 1, "exited": 1, "joined": 1}.items())
          for e in (stop, final)))
    check("retained-restart-rejected-without-activation", restart["restart"] == 0 and
          restart["runtime"] == final["runtime"] and restart["requests"] == final["requests"] and
          restart["reservations"] == final["reservations"])
    transport = [e for e in observed if e.get("event") == "shutdown_transport"]
    results = {(e["address"], e["epoch"], e["batch"]) for e in transport if e["code"] == 4}
    retired = {(e["address"], e["epoch"], e["batch"]) for e in transport if e["code"] == 5}
    check("retained-exact-transport-retirement", len(results) == 2 and results == retired and
          all(e["at_us"] <= queued["at_us"] for e in transport if e["code"] in (4, 5)) and
          not any(e["code"] == 1 and e["at_us"] > final["at_us"] for e in transport))


def report(text):
    result = {}
    fields = {"runtime": ("state", *RUNTIME_FIELDS), "records": REQUEST_FIELDS,
              "worker": ("address", "epoch", "pid", "ready", "closing", "batch", "launches", "reaps", "forced")}
    for kind, required in fields.items():
        lines = re.findall(r"snmp3 " + kind + r": ([^\n]+)", text)
        if len(lines) != 1:
            raise ValueError("missing or duplicate report: " + kind)
        pairs = dict(token.split("=", 1) for token in lines[0].split())
        if set(pairs) != set(required):
            raise ValueError("incomplete report: " + kind)
        result[kind] = {k: v if k == "state" else int(v) for k, v in pairs.items()}
    return result


def wait_for(predicate, seconds=READY_SECONDS):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return True
        time.sleep(0.01)
    return False


def worker_identities(ioc, products):
    children = set()
    for task in Path(f"/proc/{ioc.pid}/task").iterdir():
        try:
            children.update(map(int, (task / "children").read_text().split()))
        except FileNotFoundError:
            continue
    result = []
    for pid in sorted(children):
        proc = Path(f"/proc/{pid}")
        if (proc / "exe").resolve() == (products / "snmp3Worker").resolve():
            result.append({"pid": pid, "start_time": (proc / "stat").read_text().rsplit(")", 1)[1].split()[19]})
    return result


def worker_gone(worker):
    try:
        start = Path(f"/proc/{worker['pid']}/stat").read_text().rsplit(")", 1)[1].split()[19]
        return start != worker["start_time"]
    except FileNotFoundError:
        return True


def run_case(case, output, products, sanitizers):
    output.mkdir(mode=0o700)
    runner = Runner(output, products, sanitizers)
    base = Path(next(line.split("=", 1)[1].strip() for line in
                    (ROOT / "configure/RELEASE.local").read_text().splitlines()
                    if line.strip().startswith("EPICS_BASE") and "=" in line))
    children, receipts, workers, stimulus = [], [], [], []
    aborted = False
    ioc = None
    exit_ns = 0
    executable = "snmp3Ioc" if case == "inflight" else "snmp3ShutdownTest"
    inputs = sorted((ROOT / "snmp3App/src").glob("*.cpp")) + sorted((ROOT / "snmp3App/src").glob("*.h"))
    inputs += sorted((ROOT / "snmp3App/native").glob("*.cpp")) + sorted((ROOT / "snmp3App/native").glob("*.h"))
    inputs += [ROOT / "tests/rewrite" / name for name in ("ShutdownTest.cpp", "NativeAgent.cpp", "test_native.py",
               "test_record_ca.py", "run_independence.py", "helpers/udp_fault.py", "db/record-shutdown.db")]
    registrar_dir = ROOT / ("snmp3App/src" if case == "inflight" else "tests/rewrite") / ("O." + ARCH)
    inputs += [ROOT / "dbd" / (executable + ".dbd"), Path(__file__).resolve(),
               registrar_dir / (executable + "_registerRecordDeviceDriver.cpp")]
    inputs += [ROOT / "tests/rewrite" / name for name in
               ("Makefile", "build_r5_sanitizers.py", "snmp3ShutdownTestRegistrar.dbd")]
    inputs += [products / name for name in (executable, "snmp3Worker", "snmp3NativeAgent", "snmp3NativeProbe")]
    inputs += [base / "bin" / ARCH / name for name in ("caget", "caput", "caRepeater")]

    def launch(name, argv, interactive=False):
        stdout, stderr = [(output / (name + suffix)).open("xb") for suffix in (".stdout", ".stderr")]
        try:
            child = subprocess.Popen(argv, stdin=subprocess.PIPE if interactive else subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr, env=runner.env)
        except Exception:
            stdout.close()
            stderr.close()
            raise
        entry = {"name": name, "argv": argv, "child": child, "streams": (stdout, stderr),
                 "started_ns": time.monotonic_ns(), "done": False}
        children.append(entry)
        return entry

    def finish(entry, terminate=False):
        if entry["done"]:
            return
        child, forced = entry["child"], False
        if terminate and child.poll() is None:
            child.terminate()
        try:
            code = child.wait(timeout=PROCESS_SECONDS)
        except subprocess.TimeoutExpired:
            forced = True
            child.kill()
            code = child.wait(timeout=PROCESS_SECONDS)
        if child.stdin:
            child.stdin.close()
        for stream in entry["streams"]:
            stream.close()
        item = {"name": entry["name"], "argv": entry["argv"], "pid": child.pid, "returncode": code,
                "child_reaped": child.returncode is not None, "forced_cleanup": forced,
                "started_ns": entry["started_ns"], "exit_ns": time.monotonic_ns(),
                "loaded_libraries": runner.loader_identity(output / (entry["name"] + ".stderr"))}
        item["normal_exit"] = not forced and code in ((0, -15) if entry["name"] == "repeater" else (0,))
        receipts.append(item)
        write_json(output / (entry["name"] + ".receipt.json"), item)
        runner.check(entry["name"] + ":normal-exit", item["normal_exit"])
        entry["done"] = True
        return item

    def client(tool, arguments):
        name = f"ca-{len(children):03d}-{tool}"
        entry = launch(name, [str(base / "bin" / ARCH / tool), "-w", "5", "-t", *arguments])
        outcome = finish(entry)
        if not outcome["normal_exit"]:
            raise RuntimeError("CA client failed")
        return (output / (name + ".stdout")).read_text().strip()

    def shell(command):
        ioc.stdin.write((command + "\n").encode())
        ioc.stdin.flush()

    def stdout():
        return (output / "ioc.stdout").read_text(errors="replace")

    try:
        peer, secrets = runner.agent(4)
        proxy, _ = runner.fault("shutdown-proxy", 4, peer, "drop-all" if case == "inflight" else "pass")
        server, repeater = private_port(), private_port()
        while repeater == server:
            repeater = private_port()
        runner.env.update(EPICS_CA_SERVER_PORT=str(server), EPICS_CA_REPEATER_PORT=str(repeater),
                          EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST="127.0.0.1:" + str(server),
                          EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
                          EPICS_CAS_BEACON_ADDR_LIST="127.0.0.1:" + str(repeater))
        write_json(output / "ports.json", {"server": server, "repeater": repeater})
        config = output / "records.json"
        write_json(config, {"schema": 1, "profiles": [{"id": "Local", "version": "2c",
                   "communityFile": str(secrets["community"]), "timeoutMs": NATIVE_TIMEOUT_MS, "retries": 0}],
                   "endpoints": [{"id": "Local", "address": "127.0.0.1", "port": int(proxy.rsplit(":", 1)[1]),
                                  "profile": "Local"}], "bindings": [
                   {"id": name, "endpoint": "Local", "operation": op, "valueType": tag, "capacity": 1,
                    "oid": f"1.3.6.1.4.1.53864.4.{index}.0"}
                   for name, op, tag, index in (("ShutdownRead", "get", "integer", 1),
                                                ("ShutdownWrite", "set", "opaqueFloat", 9))]})
        script = output / "startup.cmd"
        setup = (f'snmp3ShutdownSetup("{output}/worker.stderr",{int(sanitizers)})\n' if case == "retained" else "")
        prepare = "snmp3ShutdownPrepare\n" if case == "retained" else ""
        script.write_text('on error break\n' + f'dbLoadDatabase("{ROOT}/dbd/{executable}.dbd")\n' +
                          f'{executable}_registerRecordDeviceDriver(pdbbase)\n' + setup +
                          f'snmp3Load("{config}","{products}/snmp3NativeProbe")\n' +
                          f'snmp3WorkerPath("{products}/snmp3Worker")\n' +
                          f'dbLoadRecords("{ROOT}/tests/rewrite/db/record-shutdown.db","P=Shutdown_")\n' +
                          'iocInit\n' + prepare)
        rep = launch("repeater", [str(base / "bin" / ARCH / "caRepeater")])
        if not wait_for(lambda: owns_udp(rep["child"].pid, repeater)):
            raise RuntimeError("private repeater not ready")
        entry = launch("ioc", [str(products / executable), str(script)], True)
        ioc = entry["child"]
        ready = wait_for(lambda: ioc.poll() is None and
                         b"iocRun: All initialization complete" in (output / "ioc.stderr").read_bytes() and
                         (case == "inflight" or any(e.get("phase") == "prepared" for e in events(stdout()))))
        runner.check(case + "-actual-IOC-ready", ready)
        if not ready:
            raise RuntimeError("IOC not ready")
        for pv in ("Shutdown_Input.PROC", "Shutdown_Output.PROC"):
            started = time.monotonic_ns()
            client("caput", [pv, "1"])
            stimulus.append({"pv": pv, "started_ns": started, "finished_ns": time.monotonic_ns()})
        pact = [int(client("caget", [pv + ".PACT"])) for pv in ("Shutdown_Input", "Shutdown_Output")]
        runner.check(case + "-both-CA-records-active", pact == [1, 1])
        if case == "retained":
            shell("snmp3ShutdownAwait")
            if not wait_for(lambda: any(e.get("phase") == "queued" for e in events(stdout())), READY_SECONDS + 2):
                raise RuntimeError("queued observation missing")
        wire_ready = wait_for(lambda: any(e.get("event") == "fault_request" for e in
                                         events((output / "shutdown-proxy.stdout").read_text())))
        runner.check(case + "-actual-request-before-exit", wire_ready)
        workers = worker_identities(ioc, products)
        runner.check(case + "-actual-owned-worker", len(workers) == 1)
        shell("snmp3RuntimeReport")
        if not wait_for(lambda: "snmp3 worker:" in stdout()):
            raise RuntimeError("runtime report missing")
        before = report(stdout())
        write_json(output / "before-exit.json", before)
        runner.check(case + "-pending-before-exit", before["runtime"]["state"] == "Running" and
                     before["records"]["active"] == 2 and before["records"]["completions"] == 0)
        exit_ns = time.monotonic_ns()
        runner.check(case + "-exit-before-deadlines", 0 < exit_ns - stimulus[0]["started_ns"] <
                     min(NATIVE_TIMEOUT_MS, DEADLINE_MS) * 1000000)
        shell("exit")
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"exception_class": type(error).__name__, "message": str(error)})
    finally:
        if ioc is not None and ioc.poll() is None and aborted:
            try:
                shell("exit")
            except BrokenPipeError:
                pass
        for entry in reversed(children):
            finish(entry, terminate=entry["name"] == "repeater")
        runner.stop()
        runner.scan()
    observed = []
    try:
        text = stdout()
        observed = events(text)
        tail = text.split("snmp3 lifecycle: fallback\n")
        runner.check(case + "-single-fallback", len(tail) == 2)
        final = report(tail[-1])
        write_json(output / "fallback.json", final)
        hooks = re.findall(r"snmp3 hook: (AtShutdown|AfterStopScan|AfterStopCallback|AfterShutdown)", text)
        runner.check(case + "-actual-Base-shutdown-order", hooks ==
                     ["AtShutdown", "AfterStopScan", "AfterStopCallback", "AfterShutdown"])
        runner.check(case + "-worker-reaped", len(workers) == 1 and all(map(worker_gone, workers)) and
                     final["worker"]["launches"] == final["worker"]["reaps"] == 1 and
                     final["worker"]["forced"] == 0)
        if case == "inflight":
            runner.check("inflight-stopped", final["runtime"]["state"] == "Stopped" and
                         all(final["runtime"][k] == v for k, v in
                             {"admission": 0, "activation": 1, "created": 1, "exited": 1, "joined": 1}.items()))
            runner.check("inflight-completed-before-link-close", final["records"] == dict.fromkeys(REQUEST_FIELDS, 0) |
                         {"contexts": 2, "completions": 2, "detachAllowed": 1})
        else:
            retained_checks(runner.check, observed)
            runner.check("retained-fallback-incomplete", final["runtime"]["state"] == "IncompleteStopped" and
                         final["records"]["contexts"] == final["records"]["active"] == final["records"]["inert"] == 2 and
                         final["records"]["completions"] == 0 and final["records"]["drainFailed"] == 1)
            wire = events((output / "shutdown-proxy.stdout").read_text())
            requests = [e for e in wire if e["event"] == "fault_request"]
            responses = [e for e in wire if e["event"] == "fault_response"]
            agent = events((output / "agent-ipv4-1.stdout").read_text())
            runner.check("retained-real-GET-SET-responses", sorted(e["command"] for e in requests) == [160, 163] and
                         len(responses) == 2 and all(e["monotonic_ns"] < exit_ns for e in requests + responses) and
                         any(e["event"] == "agent_set_value" and e.get("index") == 9 and
                             e.get("value") == "19.5" for e in agent))
    except (ValueError, KeyError, TypeError, OSError) as error:
        runner.check(case + "-parse-complete-observations", False)
        write_json(output / "parse-error.json", {"class": type(error).__name__, "message": str(error)})
    fixture_receipts = [json.loads((output / (entry[0] + ".receipt.json")).read_text()) for entry in runner.children]
    cleanup = len(receipts) == len(children) and all(r["child_reaped"] and r["normal_exit"] for r in receipts) and all(
        r["child_reaped"] and not r["forced_cleanup"] and r["returncode"] == 0 for r in fixture_receipts)
    runner.check(case + "-all-child-receipts-clean", cleanup)
    ioc_receipt = next((r for r in receipts if r["name"] == "ioc"), {})
    libraries = ioc_receipt.get("loaded_libraries", {})
    runner.check(case + "-IOC-native-free", bool(libraries) and not any("netsnmp" in p for p in libraries))
    passed = not aborted and all(c["passed"] for c in runner.checks)
    result = {"case": case, "passed": passed, "aborted": aborted, "checks": runner.checks,
              "cleanup_passed": cleanup, "children": receipts + fixture_receipts,
              "workers": workers, "stimulus": stimulus, "exit_requested_ns": exit_ns,
              "observations": observed, "inputs": {str(p): digest(p) for p in inputs},
              "loaded_libraries": runner.identities, "ioc_libraries": libraries,
              "scope": "Real non-isolated Base shutdown; ai/ao; dependencies uninstrumented; leaks disabled"}
    write_json(output / "results.json", result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--case", choices=("inflight", "retained", "all"), default="all")
    args = parser.parse_args()
    products = args.products.resolve() if args.products else ROOT / "bin" / ARCH
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    cases = ("inflight", "retained") if args.case == "all" else (args.case,)
    outcomes = [run_case(case, output / case, products, args.sanitizers) for case in cases]
    passed = all(r["passed"] for r in outcomes)
    write_json(output / "results.json", {"passed": passed, "argv": sys.argv, "cases": [
        {"case": r["case"], "passed": r["passed"], "result": str(output / r["case"] / "results.json")}
        for r in outcomes]})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
