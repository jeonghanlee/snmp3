#!/usr/bin/env python3
"""Qualify the shipped R5 owner with real native traffic and outer process faults."""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import signal
import secrets
import socket
import subprocess
import sys
import time
from test_native import ROOT, Runner, USER, VALUE_TYPES, digest, write_json

CASES = ("typed", "fifo", "deadline", "unsent", "ambiguous", "crashes", "high-fd", "live-race",
         "native-expiry", "discovery-expiry", "ipc-partial", "ipc-coalesced", "ipc-stale",
         "ipc-malformed", "ipc-truncated", "ipc-partial-timeout", "ipc-full-channel", "ipc-bad-set",
         "ipc-oversize", "ipc-bootstrap-mismatch", "ipc-bootstrap-secret", "ipc-ready-executable", "ipc-ready-library", "security-conflict", "immutable")


def loss_hook(runner, agent_name, marker):
    def perform(driver, output):
        end = time.monotonic() + 5
        worker = None
        while time.monotonic() < end and driver.poll() is None:
            for line in output.read_text().splitlines():
                event = json.loads(line)
                if event.get("event") == "await_loss":
                    worker = event["pid"]
            log = (runner.output / (agent_name + ".stdout")).read_text()
            committed = any(e.get("event") == "agent_handler" and e.get("mode") == 3
                            for e in (json.loads(line) for line in log.splitlines()))
            if worker and committed:
                status = Path(f"/proc/{worker}/stat").read_text().split()
                if int(status[3]) != driver.pid:
                    raise RuntimeError("fault target is not the driver's owned worker")
                os.kill(worker, signal.SIGKILL)
                write_json(runner.output / "set-loss.json", {"worker": worker, "parent": driver.pid,
                           "actual_agent_commit_observed": True, "signal": signal.SIGKILL,
                           "monotonic_ns": time.monotonic_ns()})
                marker.write_text("ready\n")
                return
            time.sleep(0.001)
        raise RuntimeError("real SET commit not observed before process fault")
    return perform


def configure(runner, case, family=4):
    profiles, endpoints, bindings = [], [], []
    if case == "security-conflict":
        peer, paths = runner.agent(family)
        different = runner.output / "different-auth"
        runner.secret(different)
        for name in ("Good", "Bad"):
            profiles.append({"id": name, "version": "3", "user": USER, "securityLevel": "authPriv",
                             "authAlgorithm": "SHA", "authSecretFile": str(paths["auth"] if name == "Good" else different),
                             "privAlgorithm": "AES", "privSecretFile": str(paths["privacy"]),
                             "timeoutMs": 100, "retries": 0, "maxVarbinds": 16})
            endpoints.append({"id": name, "address": "127.0.0.1", "port": int(peer.rsplit(":", 1)[1]), "profile": name})
            bindings.append({"id": name + "Read", "endpoint": name, "oid": "1.3.6.1.4.1.53864.4.1.0",
                             "operation": "get", "valueType": "integer", "capacity": 1})
    elif case in ("typed", "immutable"):
        peer, paths = runner.agent(family)
        second, second_paths = runner.agent(family, "8000000001020305")
        choices = (("Alpha", peer, paths, "alpha"), ("Beta", peer, paths, "beta"),
                   ("Port", second, second_paths, None))
        for name, target, material, context in choices:
            profile = {"id": name, "version": "3" if context else "2c", "timeoutMs": 100,
                       "retries": 0, "maxVarbinds": 7}
            if context:
                profile.update(user=USER, securityLevel="authPriv", contextName=context,
                               authAlgorithm="SHA", authSecretFile=str(material["auth"]),
                               privAlgorithm="AES", privSecretFile=str(material["privacy"]))
            else:
                profile["communityFile"] = str(material["community"])
            profiles.append(profile)
            endpoints.append({"id": name, "address": "127.0.0.1" if family == 4 else "::1",
                              "port": int(target.rsplit(":", 1)[1]), "profile": name})
            cells = list(enumerate(VALUE_TYPES, 1)) + [(3, "unsigned32"), (13, "integer"),
                                                       (14, "integer"), (15, "integer")]
            for number, (index, value_type) in enumerate(cells):
                for operation in ("get", "set") if index < 13 else ("get",):
                    bindings.append({"id": f"{name}_{operation}_{number:02}", "endpoint": name,
                                     "oid": f"1.3.6.1.4.1.53864.4.{index}.0", "operation": operation,
                                     "valueType": value_type,
                                     "capacity": 64 if value_type in ("octets", "oid") else 1})
    else:
        for current_family in (4, 6):
            peer, paths = runner.agent(current_family)
            name = "Address" + str(current_family)
            profiles.append({"id": name, "version": "2c", "communityFile": str(paths["community"]),
                             "timeoutMs": 5000, "retries": 0, "maxVarbinds": 16})
            if current_family == 4 and case in ("native-expiry", "discovery-expiry"):
                peer, _ = runner.fault("native-response-drop", 4, peer, "drop-all")
                if case == "discovery-expiry":
                    profiles[-1].pop("communityFile")
                    profiles[-1].update(version="3", user=USER, securityLevel="authPriv", authAlgorithm="SHA",
                                        authSecretFile=str(paths["auth"]), privAlgorithm="AES",
                                        privSecretFile=str(paths["privacy"]))
            endpoints.append({"id": name, "address": "127.0.0.1" if current_family == 4 else "::1",
                              "port": int(peer.rsplit(":", 1)[1]), "profile": name})
            bindings.append({"id": name + "Read", "endpoint": name, "oid": "1.3.6.1.4.1.53864.4.1.0",
                             "operation": "get", "valueType": "integer", "capacity": 1})
            if current_family == 4 and case in ("ipc-full-channel", "ipc-bad-set"):
                bulk = case == "ipc-full-channel"
                bindings.append({"id": "Address4Set", "endpoint": name, "oid": "1.3.6.1.4.1.53864.4.6.0" if bulk else "1.3.6.1.4.1.53864.4.1.0",
                                 "operation": "set", "valueType": "octets" if bulk else "integer",
                                 "capacity": 1048576 if bulk else 1})
            if current_family == 4 and case == "ambiguous":
                proxy, _ = runner.fault("set-response-drop", 4, peer, "drop")
                endpoints.append({"id": "SetPath", "address": "127.0.0.1",
                                  "port": int(proxy.rsplit(":", 1)[1]), "profile": name})
                bindings.append({"id": "Address4Set", "endpoint": "SetPath", "oid": "1.3.6.1.4.1.53864.4.1.0",
                                 "operation": "set", "valueType": "integer", "capacity": 1})
    path = runner.output / "configuration.json"
    write_json(path, {"schema": 1, "profiles": profiles, "endpoints": endpoints, "bindings": bindings})
    return path


def execute(output, case, family, products=None, sanitizers=False):
    output.mkdir(mode=0o700)
    runner = Runner(output, products, sanitizers)
    aborted = False
    sockets = []
    try:
        if sanitizers:
            runner.invoke("instrumented-capabilities", [str(runner.products / "snmp3NativeProbe"), "--capabilities"])
        config = configure(runner, case, family)
        argv = [str(runner.products / "snmp3QualificationTest"), str(config),
                str(runner.products / "snmp3NativeProbe"), str(runner.products / "snmp3Worker"),
                str(output / "native.stderr"), case]
        hook = None
        passed = ()
        if case == "immutable":
            marker = output / "immutable.release"
            argv.append(str(marker))
            def mutate_owned_files(driver, log):
                end = time.monotonic() + 5
                while time.monotonic() < end and driver.poll() is None:
                    if '"event":"immutable_snapshot"' in log.read_text():
                        for path in output.iterdir():
                            if path.name.endswith(("-community", "-auth", "-privacy")):
                                token = secrets.token_hex(24)
                                path.write_text(token + "\n")
                                runner.tokens.append(token.encode())
                        marker.write_text("ready\n")
                        return
                    time.sleep(0.001)
                raise RuntimeError("immutable snapshot not observed")
            hook = mutate_owned_files
        if case.startswith("ipc-"):
            parent, left = socket.socketpair()
            child, right = socket.socketpair()
            sockets = [parent, left, child, right]
            proxy_argv = [sys.executable, str(ROOT / "tests/rewrite/helpers/ipc_fault.py"),
                          "--mode", case[4:], "--parent-fd", str(left.fileno()), "--worker-fd", str(right.fileno())]
            stdout = (output / "ipc-proxy.stdout").open("wb")
            stderr = (output / "ipc-proxy.stderr").open("wb")
            process = subprocess.Popen(proxy_argv, stdout=stdout, stderr=stderr, env=runner.env,
                                       pass_fds=(left.fileno(), right.fileno()))
            runner.children.append(("ipc-proxy", process, stdout, stderr, proxy_argv))
            left.close()
            right.close()
            passed = (parent.fileno(), child.fileno())
            argv += [str(descriptor) for descriptor in passed]
            def close_parent_copies(_driver, _output):
                parent.close()
                child.close()
            hook = close_parent_copies
        if case == "ambiguous":
            marker = output / "loss.release"
            argv.append(str(marker))
            hook = loss_hook(runner, "agent-ipv4-1", marker)
        events = runner.invoke("qualification", argv, ready_hook=hook, pass_fds=passed)
        summaries = [e for e in events if e.get("event") == "qualification_summary"]
        runner.check("real-qualification-complete", len(summaries) == 1 and
                     summaries[0].get("settled") is True and summaries[0].get("mode") == case)
        observed = [e for e in events if e.get("event") == "supervision"]
        launches, reaps = ([e for e in observed if e.get("code") == code] for code in (2, 7))
        runner.check("every-worker-exact-reap", Counter(e["pid"] for e in launches) ==
                     Counter(e["pid"] for e in reaps) and bool(launches))
        live = {}
        for event in observed:
            if event["code"] == 2:
                runner.check("no-overlap:" + str(event["pid"]), not live.get(event["address"]))
                live[event["address"]] = event["pid"]
            elif event["code"] == 7:
                runner.check("exact-reap:" + str(event["pid"]), live.get(event["address"]) == event["pid"])
                live[event["address"]] = None
        if case in ("fifo", "high-fd"):
            handlers = [json.loads(line) for line in (output / "agent-ipv4-1.stdout").read_text().splitlines()
                        if '"event":"agent_handler"' in line]
            groups = Counter(e["request_id"] for e in handlers)
            runner.check("actual-native-unequal-FIFO-and-equal-batch", list(groups.values()) == [1, 1, 2])
        if case == "ambiguous":
            packets = [json.loads(line) for line in (output / "set-response-drop.stdout").read_text().splitlines()]
            runner.check("one-actual-SET-no-replay", sum(e.get("event") == "fault_request" and
                         e.get("command") == 163 for e in packets) == 1)
        if case == "high-fd":
            runner.check("actual-high-IOC-channel-FD", any(e.get("code") == 17 and e.get("detail", 0) > 1100 for e in observed))
        if case == "live-race":
            races = [e for e in events if e.get("event") == "live_race"]
            runner.check("actual-concurrent-64-owned-completions", len(races) == 1 and
                         races[0].get("accepted") == 64 and races[0].get("consumed") == 64 and races[0].get("rejected", 0) > 0)
            for name, _, _, _, _ in runner.children:
                handlers = [json.loads(line) for line in (output / (name + ".stdout")).read_text().splitlines()
                            if '"event":"agent_handler"' in line]
                runner.check(name + ":32-real-native-requests", len(handlers) == 32)
        if case in ("native-expiry", "discovery-expiry"):
            packets = [json.loads(line) for line in (output / "native-response-drop.stdout").read_text().splitlines()]
            requests = [e for e in packets if e.get("event") == "fault_request"]
            runner.check("actual-blocked-native-stage", bool(requests) and
                         any(e.get("discovery") is (case == "discovery-expiry") for e in requests))
            runner.check("deadline-does-not-dispatch-next-batch", sum(e.get("code") == 13 and e.get("address") == 1 for e in observed) == 1)
        if case.startswith("ipc-"):
            proxy = [json.loads(line) for line in (output / "ipc-proxy.stdout").read_text().splitlines()]
            required = {"ipc-partial": "partial_frame", "ipc-coalesced": "coalesced_actual_frames",
                        "ipc-stale": "stale_actual_frame", "ipc-malformed": "malformed_actual_header",
                        "ipc-truncated": "truncated_actual_frame", "ipc-partial-timeout": "truncated_actual_frame",
                        "ipc-full-channel": "full_channel_reader_stopped", "ipc-bad-set": "unsupported_actual_set_tag",
                        "ipc-oversize": "oversized_actual_header", "ipc-bootstrap-mismatch": "bootstrap_mismatch_actual_frame",
                        "ipc-bootstrap-secret": "bootstrap_secret_actual_frame",
                        "ipc-ready-executable": "ready_identity_actual_frame", "ipc-ready-library": "ready_identity_actual_frame"}[case]
            runner.check("real-IPC-outer-fault-observed", any(e.get("event") == required for e in proxy))
            if case == "ipc-stale":
                runner.check("old-generation-result-rejected", any(e.get("code") == 16 for e in observed))
                runner.check("old-admission-batch-delivered-to-real-worker", any(e.get("field") == "admission" for e in proxy))
            if case in ("ipc-bad-set", "ipc-full-channel", "ipc-bootstrap-mismatch", "ipc-bootstrap-secret", "ipc-ready-executable", "ipc-ready-library"):
                runner.check("invalid-or-unsent-work-never-reached-agent", '"event":"agent_handler"' not in
                             (output / "agent-ipv4-1.stdout").read_text())
        if case in ("typed", "immutable"):
            for name, _, _, _, _ in runner.children:
                handler = [json.loads(line) for line in (output / (name + ".stdout")).read_text().splitlines()
                           if '"event":"agent_handler"' in line]
                expected_contexts = {0} if name.endswith("-2") else {1, 2}
                runner.check(name + ":actual-contexts", {e["context"] for e in handler} == expected_contexts)
                groups = Counter(e["request_id"] for e in handler if e["mode"] == 160)
                runner.check(name + ":actual-maxvarbinds", bool(groups) and max(groups.values()) <= 7)
            native = (output / "native.stderr").read_text()
            runner.check("one-worker-three-real-sessions", len(launches) == 1 and
                         sum(line.startswith("native_event native_open ") for line in native.splitlines()) == 3)
        runner.check("native-events-no-drop", "native_dropped" not in (output / "native.stderr").read_text())
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"exception_class": type(error).__name__})
    finally:
        for transport in sockets:
            transport.close()
        runner.stop()
        runner.scan()
    inputs = [ROOT / "tests/rewrite/QualificationTest.cpp", Path(__file__).resolve(),
              ROOT / "tests/rewrite/NativeAgent.cpp", ROOT / "tests/rewrite/helpers/udp_fault.py",
              ROOT / "tests/rewrite/helpers/ipc_fault.py",
              runner.products / "snmp3QualificationTest", runner.products / "snmp3Worker"]
    inputs += sorted((ROOT / "snmp3App/src").glob("*.cpp"))
    inputs += sorted((ROOT / "snmp3App/native").glob("*.cpp"))
    inputs += sorted((ROOT / "snmp3App/src").glob("*.h"))
    inputs += sorted((ROOT / "snmp3App/native").glob("*.h"))
    inputs += [ROOT / "tests/rewrite/test_native.py", runner.products / "snmp3NativeProbe",
               runner.products / "snmp3NativeAgent"]
    result = {"passed": not aborted and all(c["passed"] for c in runner.checks), "aborted": aborted,
              "case": case, "family": family, "checks": runner.checks, "loaded_libraries": runner.identities,
              "sources_and_products": {str(path): digest(path) for path in inputs}}
    write_json(output / "results.json", result)
    return result["passed"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cases", nargs="+", choices=CASES, default=list(CASES))
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    results = {}
    for case in args.cases:
        for family in (4, 6) if case == "typed" else (4,):
            name = case + "-" + str(family)
            results[name] = execute(output / name, case, family,
                                    args.products.resolve() if args.products else None, args.sanitizers)
    passed = all(results.values())
    write_json(output / "results.json", {"passed": passed, "cases": results,
               "scope": "Real owner/native path; full V5 acceptance additionally requires IPC fault, IOC, sanitizer, regression and documentation evidence"})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
