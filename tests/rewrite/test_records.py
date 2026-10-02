#!/usr/bin/env python3
"""Exercise actual record/DSET/callback/worker/native paths with a loopback SNMP agent."""
import argparse
import json
import subprocess
import time
from pathlib import Path
from test_native import ROOT, ARCH, Runner, digest, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--case", choices=("baseline", "shutdown", "queued-shutdown", "edges", "alarms", "active", "policy", "numeric"), default="baseline")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    runner = Runner(output, args.products.resolve() if args.products else None, args.sanitizers)
    aborted = False
    try:
        peer, material = runner.agent(4)
        specs = [("IntegerRead", 1, "get", "integer", 1),
                 ("Counter32Read", 2, "get", "counter32", 1),
                 ("Counter64Read", 5, "get", "counter64", 1),
                 ("OctetsRead", 6, "get", "octets", 1024),
                 ("OidRead", 7, "get", "oid", 128),
                 ("FloatRead", 9, "get", "opaqueFloat", 1),
                 ("DoubleRead", 10, "get", "opaqueDouble", 1),
                 ("IpRead", 8, "get", "ipAddress", 1),
                 ("IntegerWrite", 1, "set", "integer", 1),
                 ("Counter64Write", 5, "set", "counter64", 1),
                 ("OctetsWrite", 6, "set", "octets", 1024),
                 ("MaxOctetsRead", 6, "get", "octets", 32766),
                 ("MaxOctetsWrite", 6, "set", "octets", 32766),
                 ("FloatWrite", 9, "set", "opaqueFloat", 1)]
        configuration = {"schema": 1, "profiles": [{"id": "Local", "version": "2c",
                         "communityFile": str(material["community"]), "timeoutMs": 100,
                         "retries": 0, "maxVarbinds": 16}],
                         "endpoints": [{"id": "Local", "address": "127.0.0.1",
                         "port": int(peer.rsplit(":", 1)[1]), "profile": "Local"}],
                         "bindings": [{"id": name, "endpoint": "Local",
                         "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0",
                         "operation": operation, "valueType": tag, "capacity": capacity}
                         for name, index, operation, tag, capacity in specs]}
        if args.case in ("alarms", "policy"):
            dropped, _ = runner.fault("record-response-drop", 4, peer, "drop-all")
            configuration["endpoints"].append({"id": "Dropped", "address": "127.0.0.1",
                                               "port": int(dropped.rsplit(":", 1)[1]), "profile": "Local"})
            prefix = "Timeout" if args.case == "alarms" else "Policy"
            configuration["bindings"] += [dict(binding, id=prefix + binding["id"], endpoint="Dropped")
                                           for binding in list(configuration["bindings"])]
        if args.case == "active":
            delayed, _ = runner.fault("active-response-delay", 4, peer, "delay", delay_ms=750)
            dropped, _ = runner.fault("active-response-drop", 4, peer, "drop-all")
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "active-response-delay.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Delayed", timeoutMs=1500))
            configuration["profiles"].append(dict(configuration["profiles"][0], id="NativeRetry", retries=1))
            configuration["endpoints"] += [{"id": "Delayed", "address": "127.0.0.1",
                                            "port": int(delayed.rsplit(":", 1)[1]), "profile": "Delayed"},
                                           {"id": "Dropped", "address": "127.0.0.1",
                                            "port": int(dropped.rsplit(":", 1)[1]), "profile": "Local"},
                                           {"id": "NativeRetry", "address": "127.0.0.1",
                                            "port": int(dropped.rsplit(":", 1)[1]), "profile": "NativeRetry"}]
            outputs = [binding for binding in configuration["bindings"] if binding["operation"] == "set"]
            configuration["bindings"] += [dict(binding, id="Active" + binding["id"], endpoint="Delayed")
                                           for binding in outputs]
            configuration["bindings"].append(dict(next(binding for binding in outputs if binding["id"] == "FloatWrite"),
                                                   id="RetryFloatWrite", endpoint="Dropped"))
            configuration["bindings"].append(dict(next(binding for binding in outputs if binding["id"] == "FloatWrite"),
                                                   id="NativeRetryFloatWrite", endpoint="NativeRetry"))
        if args.case == "numeric":
            tags = (("Integer", 1, "integer"), ("Unsigned32", 3, "unsigned32"),
                    ("Counter32", 2, "counter32"), ("Gauge32", 3, "gauge32"),
                    ("TimeTicks", 4, "timeticks"), ("Counter64", 5, "counter64"),
                    ("OpaqueFloat", 9, "opaqueFloat"), ("OpaqueDouble", 10, "opaqueDouble"))
            for name, index, tag in tags:
                for operation, suffix in (("get", "Read"), ("set", "Write")):
                    configuration["bindings"].append({"id": "Matrix" + name + suffix, "endpoint": "Local",
                        "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0", "operation": operation,
                        "valueType": tag, "capacity": 1})
            fixed = (("WideHigh", 19, "counter64"), ("WideMax", 20, "counter64"),
                     ("FloatNaN", 21, "opaqueFloat"), ("FloatInfinity", 22, "opaqueFloat"),
                     ("FloatNegativeInfinity", 23, "opaqueFloat"), ("DoubleNaN", 24, "opaqueDouble"),
                     ("DoubleInfinity", 25, "opaqueDouble"), ("DoubleNegativeInfinity", 26, "opaqueDouble"))
            configuration["bindings"] += [{"id": "Matrix" + name + "Read", "endpoint": "Local",
                "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0", "operation": "get",
                "valueType": tag, "capacity": 1} for name, index, tag in fixed]
        config = output / "records.json"
        write_json(config, configuration)
        fixtures = [ROOT / "tests/rewrite/db/records.db", ROOT / "tests/rewrite/db/record-output.db"]
        argv = [str(runner.products / "snmp3RecordTest"), str(config),
                str(runner.products / "snmp3NativeProbe"), str(runner.products / "snmp3Worker"),
                str(ROOT / "dbd/snmp3RecordTest.dbd"), *map(str, fixtures), str(output / "workers.stderr"), args.case]
        started = time.monotonic_ns()
        with (output / "records.stdout").open("xb") as stdout, (output / "records.stderr").open("xb") as stderr:
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=runner.env)
            forced = False
            try:
                code = child.wait(timeout=180 if args.case == "numeric" else 30)
            except subprocess.TimeoutExpired:
                forced = True
                child.kill()
                code = child.wait(timeout=10)
        libraries = runner.loader_identity(output / "records.stderr")
        write_json(output / "records.receipt.json", {"argv": argv, "returncode": code,
                   "pid": child.pid, "child_reaped": child.returncode is not None,
                   "forced_cleanup": forced, "elapsed_ns": time.monotonic_ns() - started,
                   "loaded_libraries": libraries, "product_sha256": digest(Path(argv[0]))})
        runner.check("records:return", code == 0 and not forced)
        events = [json.loads(line) for line in (output / "records.stdout").read_text().splitlines()
                  if line.startswith("{")]
        summary = next((event for event in events if event.get("event") == "record_summary"), {})
        runner.check("actual-eleven-record-path", summary.get("records") == 11 and summary.get("checks", 0) > 100)
        completions = [event for event in events if event.get("event") == "record_completed"]
        runner.check("actual-record-identities", bool(completions) and all(event["activation"] == 1 and
                     event["revision"] == 1 and event["handle"] > 0 for event in completions))
        runner.check("actual-callback-pressure", any(event.get("event") == "callback_pressure" and
                     event.get("enqueue_failures", 0) > 0 and event.get("completed_once") for event in events))
        runner.check("actual-GET-and-SET-pressure", {event.get("record") for event in events if
                     event.get("event") == "callback_pressure"} == {"Records_Longin", "Records_Ao"})
        if args.case == "shutdown":
            runner.check("actual-blocked-shutdown", any(event.get("event") == "blocked_shutdown" and
                         event.get("waited_for_entered") and event.get("restart_rejected") for event in events))
        if args.case == "queued-shutdown":
            runner.check("actual-queued-shutdown", any(event.get("event") == "queued_shutdown" and
                         event.get("storage_retained_until_cleanup") and event.get("abandoned_finalized_once")
                         and event.get("restart_rejected") for event in events))
        if args.case == "edges":
            runner.check("actual-input-edges", any(event.get("event") == "input_edges" and
                         event.get("precision_rejected") and event.get("binary_exact") for event in events))
            runner.check("actual-Base-FLNK-order", any(event.get("event") == "base_order" and
                         event.get("flnk_source_pact") == 1 and event.get("target_completions") == 1 for event in events))
            runner.check("actual-six-input-simulation", len([event for event in events if
                         event.get("event") == "simulation_bypass" and event.get("terminal_released")]) == 6)
            runner.check("actual-output-edges", any(event.get("event") == "output_edges" and
                         event.get("rounding_modes") == 4 and event.get("wire_bit_fixtures") == 36 and
                         event.get("invalid_set_rejected") for event in events))
            runner.check("actual-max-payload-queue", any(event.get("event") == "max_payload_queue" and
                         event.get("data_bytes") == 32766 and event.get("rejected_without_admission") and
                         event.get("retained_before_release") for event in events))
        if args.case == "alarms":
            runner.check("actual-native-timeout-terminal", len([event for event in events if
                         event.get("event") == "native_terminal" and event.get("outcome") == 6 and
                         event.get("native_code") == 2 and event.get("error_status") == 0 and
                         event.get("error_index") == 0]) == 11)
            runner.check("actual-eleven-native-timeouts", len([event for event in events if
                         event.get("event") == "native_timeout" and event.get("communication_alarm")]) == 11)
            runner.check("actual-record-deadline", any(event.get("event") == "record_deadline" and
                         event.get("communication_alarm") for event in events))
            runner.check("actual-deadline-terminal", any(event.get("event") == "native_terminal" and
                         event.get("record") == "Records_Deadline" and event.get("outcome") == 2 for event in events))
            fault_events = [json.loads(line) for line in
                            (output / "record-response-drop.stdout").read_text().splitlines()]
            runner.check("actual-UDP-responses-dropped", len([event for event in fault_events if
                         event.get("event") == "fault_dropped"]) >= 11)
        if args.case == "active":
            active_events = [event for event in events if event.get("event") == "active_output"]
            runner.check("actual-five-output-RPRO", len(active_events) == 5 and all(
                         event.get("first_payload_owned") and event.get("latest_requested_preserved") and
                         event.get("generations") == 2 for event in active_events))
            delay_events = [json.loads(line) for line in
                            (output / "active-response-delay.stdout").read_text().splitlines()]
            requests = [event for event in delay_events if event.get("event") == "fault_request" and
                        event.get("command") == 163]
            runner.check("actual-ten-delayed-SET-packets", len(requests) == 10)
            runner.check("changes-after-real-SET-transmission", len(requests) == 10 and len(active_events) == 5 and all(
                         requests[index * 2]["monotonic_ns"] <= event["changed_at_us"] * 1000
                         for index, event in enumerate(active_events)))
            drop_events = [json.loads(line) for line in
                           (output / "active-response-drop.stdout").read_text().splitlines()]
            dropped_requests = [event for event in drop_events if event.get("event") == "fault_request" and
                                event.get("command") == 163]
            runner.check("explicit-only-same-value-retry", len(dropped_requests) == 4 and
                         dropped_requests[0]["request_id"] != dropped_requests[1]["request_id"] and any(
                         event.get("event") == "explicit_retry" and event.get("generations") == 2 for event in events))
            runner.check("actual-configured-native-retry", len(dropped_requests) == 4 and
                         dropped_requests[2]["request_id"] == dropped_requests[3]["request_id"] and any(
                         event.get("event") == "native_retry" and event.get("generations") == 1 for event in events))
            dropped_responses = [event for event in drop_events if event.get("event") == "fault_response"]
            runner.check("actual-successful-SET-responses-lost", len(dropped_responses) == 4 and all(
                         event.get("command") == 162 and event.get("status") == 0 and event.get("index") == 0
                         for event in dropped_responses))
        if args.case == "policy":
            policies = [event for event in events if event.get("event") == "output_policy"]
            runner.check("actual-five-output-ivoa-completion", len([event for event in policies if
                         not event["simulation"] and event["oopt"] == 0]) == 15 and all(
                         event["terminal_released_once"] and event["latest_requested_preserved"] for event in policies))
            runner.check("actual-five-output-simulation-bypass", len([event for event in policies if event["simulation"]]) == 5)
            runner.check("actual-all-live-oopt-choices", {event["oopt"] for event in policies if event["oopt"]} == set(range(1, 6)))
            runner.check("actual-five-output-sync-and-delayed-simulation", len([event for event in events if
                         event.get("event") == "output_simulation" and not event["native_admission"]]) == 10)
            runner.check("actual-five-first-pass-ivov", len([event for event in events if
                         event.get("event") == "output_ivov" and event["wire_readback_exact"]]) == 5)
            runner.check("actual-five-first-pass-continue-and-no-drive", len([event for event in events if
                         event.get("event") == "output_first_policy"]) == 10)
            runner.check("actual-ao-drive-oval-wire", any(event.get("event") == "output_drive" and
                         event["drive_clipped"] and event["oval_wire_exact"] for event in events))
            runner.check("actual-lso-dol-and-Base-capacity-boundary", any(event.get("event") == "output_dol" and
                         event["supervisory_ignores_dol"] and event["closed_loop_data_bytes"] == 200 and
                         event["source_data_bytes"] == 300 and event["base_prepared_data_bytes"] == 255 and
                         event["wire_readback_exact"] for event in events))
            fault_events = [json.loads(line) for line in (output / "record-response-drop.stdout").read_text().splitlines()]
            runner.check("actual-policy-SET-count", len([event for event in fault_events if
                         event.get("event") == "fault_request" and event.get("command") == 163]) == 38)
        if args.case == "numeric":
            inputs_observed = [event for event in events if event.get("event") == "numeric_input"]
            outputs_observed = [event for event in events if event.get("event") == "numeric_output"]
            summary_numeric = next((event for event in events if event.get("event") == "numeric_summary"), {})
            runner.check("actual-68-advertised-numeric-GET-pairs", len({event["record"] for event in inputs_observed
                         if not event["fixed"]}) == 68)
            runner.check("actual-48-waveform-tag-FTVL-pairs", len({event["record"] for event in inputs_observed
                         if not event["fixed"] and "Wave" in event["record"]}) == 48)
            runner.check("all-advertised-numeric-inputs-have-success", len({event["record"] for event in inputs_observed
                         if not event["fixed"] and event["accepted"]}) == 68)
            runner.check("actual-20-advertised-numeric-SET-pairs", len({event["record"] for event in outputs_observed
                         if event["accepted"]}) == 20)
            runner.check("actual-60-fixed-high-or-nonfinite-GET-pairs", len({event["record"] for event in inputs_observed
                         if event["fixed"]}) == 60)
            runner.check("failure-after-native-success-preserves-input", summary_numeric.get("failed_after_good", 0) > 50)
            runner.check("numeric-rejections-send-no-native-SET", summary_numeric.get("rejected_outputs", 0) >= 30)
            runner.check("numeric-output-wire-readback", all(event["separate_native_readback"] for event in outputs_observed))
            agent_events = [json.loads(line) for line in (output / "agent-ipv4-1.stdout").read_text().splitlines()
                            if line.startswith("{")]
            runner.check("actual-numeric-SET-actions", len([event for event in
                         agent_events if event.get("event") == "agent_handler" and
                         event.get("mode") == 2]) == summary_numeric.get("accepted_outputs", -1) + summary_numeric.get("stimuli", -1) + 6)
        runner.check("IOC-native-free", bool(libraries) and not any("netsnmp" in path for path in libraries))
        write_json(output / "record-observations.json", events)
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"exception_class": type(error).__name__})
    finally:
        runner.stop()
        runner.scan()
    inputs = sorted((ROOT / "snmp3App/src").glob("*.cpp")) + sorted((ROOT / "snmp3App/src").glob("*.h"))
    inputs += [Path(__file__).resolve(), ROOT / "tests/rewrite/RecordTest.cpp",
               ROOT / "tests/rewrite/NativeAgent.cpp", ROOT / "tests/rewrite/db/records.db",
               ROOT / "tests/rewrite/db/record-output.db", ROOT / "dbd/snmp3RecordTest.dbd"]
    if args.case == "edges":
        inputs += [ROOT / "tests/rewrite/db/record-edges.db", ROOT / "tests/rewrite/db/record-order.db"]
    if args.case == "alarms":
        inputs += [ROOT / "tests/rewrite/db/record-alarms.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "active":
        inputs += [ROOT / "tests/rewrite/db/record-active.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "policy":
        inputs += [ROOT / "tests/rewrite/db/record-policy.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "numeric":
        inputs += [ROOT / "tests/rewrite/db/record-numeric.db"]
    inputs += [ROOT / "tests/rewrite/test_native.py"]
    inputs += [runner.products / name for name in ("snmp3RecordTest", "snmp3NativeProbe", "snmp3Worker", "snmp3NativeAgent")]
    passed = not aborted and bool(runner.checks) and all(check["passed"] for check in runner.checks)
    write_json(output / "results.json", {"passed": passed, "aborted": aborted, "checks": runner.checks,
               "inputs": {str(path): digest(path) for path in inputs}, "loaded_libraries": runner.identities,
               "scope": "Initial eleven-record integration and isolated shutdown; does not close the full T1-T14 matrix"})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
