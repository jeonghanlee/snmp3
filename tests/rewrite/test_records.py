#!/usr/bin/env python3
"""Exercise actual record/DSET/callback/worker/native paths with a loopback SNMP agent."""
import argparse
import json
import re
import subprocess
import time
from pathlib import Path
from test_native import ROOT, ARCH, Runner, digest, write_json

# Deadline-queue and near-deadline trials run one snmp3RecordTest process each.
DEADLINE_SAMPLE_BUDGET_MS = 1000
DEADLINE_TRIALS = 3
NATIVE_TIMEOUT_MS = 10000
NEAR_DELAY_MS = 300
NEAR_MARGINS_MS = (14, 16, 18, 20, 22)
NEAR_REPEATS = 2
NEAR_TRIALS = len(NEAR_MARGINS_MS) * NEAR_REPEATS
QUEUE_CASES = ("deadline-queue", "near-deadline", "stop-queued", "rebuild")
STOP_BUDGET_MS = 300


def queue_counter_lines(path):
    # Every `snmp3 queue:` report line of a record test process: the three counters of admissions behind a retirement.
    found = []
    for line in path.read_text().splitlines():
        match = re.search(r"^snmp3 queue: .* behindRetirement=(\d+) behindAdmitted=(\d+) behindNeverSent=(\d+)$", line)
        if match:
            found.append(tuple(int(value) for value in match.groups()))
    return found


def queue_counters(path):
    # The first report line of a record test process.
    found = queue_counter_lines(path)
    return found[0] if found else None


class InconclusiveWait(Exception):
    """A stop-queued trial that stopped without an observed Base reprocess cannot judge the product."""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--case", choices=("baseline", "shutdown", "queued-shutdown", "edges", "alarms", "active", "policy", "numeric",
                                                   "active-unforced", "deadline-queue", "near-deadline",
                                                   "accounting", "stop-queued", "rebuild", "stop-inflight"), default="baseline")
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
        if args.case in ("alarms", "policy", "stop-inflight"):
            dropped, _ = runner.fault("record-response-drop", 4, peer, "drop-all")
            configuration["endpoints"].append({"id": "Dropped", "address": "127.0.0.1",
                                               "port": int(dropped.rsplit(":", 1)[1]), "profile": "Local"})
            prefix = "Policy" if args.case == "policy" else "Timeout"
            configuration["bindings"] += [dict(binding, id=prefix + binding["id"], endpoint="Dropped")
                                           for binding in list(configuration["bindings"])]
        if args.case in ("active", "active-unforced", "accounting"):
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
        if args.case in ("deadline-queue", "stop-queued", "rebuild"):
            dropped, _ = runner.fault("queue-response-drop", 4, peer, "drop-all")
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "queue-response-drop.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Slow", timeoutMs=NATIVE_TIMEOUT_MS))
            configuration["endpoints"].append({"id": "QueueDropped", "address": "127.0.0.1",
                                               "port": int(dropped.rsplit(":", 1)[1]), "profile": "Slow"})
            configuration["bindings"].append({"id": "QueueFloatWrite", "endpoint": "QueueDropped",
                                              "oid": "1.3.6.1.4.1.53864.4.9.0", "operation": "set",
                                              "valueType": "opaqueFloat", "capacity": 1})
            runner.env["SNMP3_RECORD_QUEUE_BINDING"] = "QueueFloatWrite"
        if args.case == "near-deadline":
            delayed, _ = runner.fault("near-response-delay", 4, peer, "delay", delay_ms=NEAR_DELAY_MS)
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "near-response-delay.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Near", timeoutMs=5 * NEAR_DELAY_MS))
            configuration["endpoints"].append({"id": "NearDelayed", "address": "127.0.0.1",
                                               "port": int(delayed.rsplit(":", 1)[1]), "profile": "Near"})
            configuration["bindings"].append({"id": "NearFloatWrite", "endpoint": "NearDelayed",
                                              "oid": "1.3.6.1.4.1.53864.4.9.0", "operation": "set",
                                              "valueType": "opaqueFloat", "capacity": 1})
            runner.env["SNMP3_RECORD_QUEUE_BINDING"] = "NearFloatWrite"
        config = output / "records.json"
        write_json(config, configuration)
        fixtures = [ROOT / "tests/rewrite/db/records.db", ROOT / "tests/rewrite/db/record-output.db"]
        argv = [str(runner.products / "snmp3RecordTest"), str(config),
                str(runner.products / "snmp3NativeProbe"), str(runner.products / "snmp3Worker"),
                str(ROOT / "dbd/snmp3RecordTest.dbd"), *map(str, fixtures), str(output / "workers.stderr"), args.case]
        def execute(tag, extra=None):
            # One actual snmp3RecordTest process; its stdout events and receipt are kept under the tag.
            environment = dict(runner.env, **(extra or {}))
            started = time.monotonic_ns()
            with (output / (tag + ".stdout")).open("xb") as stdout, (output / (tag + ".stderr")).open("xb") as stderr:
                child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=environment)
                forced = False
                try:
                    code = child.wait(timeout=180 if args.case in ("numeric",) + QUEUE_CASES else 30)
                except subprocess.TimeoutExpired:
                    forced = True
                    child.kill()
                    code = child.wait(timeout=10)
            libraries = runner.loader_identity(output / (tag + ".stderr"))
            write_json(output / (tag + ".receipt.json"), {"argv": argv, "returncode": code,
                       "environment": {key: extra[key] for key in sorted(extra or {})},
                       "pid": child.pid, "child_reaped": child.returncode is not None,
                       "forced_cleanup": forced, "elapsed_ns": time.monotonic_ns() - started,
                       "loaded_libraries": libraries, "product_sha256": digest(Path(argv[0]))})
            runner.check(tag + ":return", code == 0 and not forced)
            return libraries, [json.loads(line) for line in (output / (tag + ".stdout")).read_text().splitlines()
                               if line.startswith("{")]

        def proxy_sets(name):
            path = output / (name + ".stdout")
            return len([line for line in path.read_text().splitlines() if '"event": "fault_request"' in line and
                        '"command": 163' in line]) if path.exists() else 0

        if args.case == "deadline-queue":
            events, samples = [], []
            for trial in range(1, DEADLINE_TRIALS + 1):
                before = proxy_sets("queue-response-drop")
                libraries, trial_events = execute(f"records-sample-{trial}", {"SNMP3_RECORD_BUDGET_MS": str(DEADLINE_SAMPLE_BUDGET_MS),
                                                                              "SNMP3_RECORD_TRIAL": f"sample-{trial}"})
                for event in trial_events:
                    if event.get("event") == "deadline_queue":
                        event["proxy_sets"] = proxy_sets("queue-response-drop") - before
                        interval = event["admission_to_ready_us"] or event["deadline_to_ready_us"]
                        if interval:
                            samples.append(interval)
                events += trial_events
            runner.check("deadline-threshold-sampled", len(samples) == DEADLINE_TRIALS)
            low, high = (min(samples), max(samples)) if samples else (0, 0)
            threshold = {"event": "deadline_threshold", "trials": DEADLINE_TRIALS, "samples_us": samples,
                         "minimum_us": low, "maximum_us": high}
            events.append(threshold)
            budgets = (("below", max(1, low // 2000)), ("above", max(1, high * 2 // 1000)))
            for label, budget in budgets:
                before = proxy_sets("queue-response-drop")
                libraries, trial_events = execute(f"records-{label}", {"SNMP3_RECORD_BUDGET_MS": str(budget),
                                                                       "SNMP3_RECORD_TRIAL": label})
                for event in trial_events:
                    if event.get("event") == "deadline_queue":
                        event["proxy_sets"] = proxy_sets("queue-response-drop") - before
                        event["budget_ms"] = budget
                events += trial_events
            # Every relaunch the case measured, sampling and budget trials alike, bounds the documented threshold.
            relaunches = list(samples)
            for event in events:
                if event.get("event") == "deadline_queue" and event.get("trial") in ("below", "above"):
                    interval = event["admission_to_ready_us"] or event["deadline_to_ready_us"]
                    if interval:
                        relaunches.append(interval)
            events.append({"event": "deadline_threshold_all", "trials": len(relaunches), "samples_us": relaunches,
                           "minimum_us": min(relaunches) if relaunches else 0, "maximum_us": max(relaunches) if relaunches else 0})
        elif args.case == "near-deadline":
            events = []
            for repeat in range(1, NEAR_REPEATS + 1):
                for margin in NEAR_MARGINS_MS:
                    libraries, trial_events = execute(f"records-near-{margin}ms-{repeat}",
                                                      {"SNMP3_RECORD_BUDGET_MS": str(NEAR_DELAY_MS + margin)})
                    for event in trial_events:
                        if event.get("event") == "near_deadline":
                            event["margin_ms"] = margin
                    events += trial_events
        elif args.case in ("stop-queued", "rebuild"):
            before = proxy_sets("queue-response-drop")
            libraries, events = execute("records", {"SNMP3_RECORD_BUDGET_MS": str(STOP_BUDGET_MS)})
            for event in events:
                if event.get("event") == "stop_queued":
                    event["proxy_sets"] = proxy_sets("queue-response-drop") - before
        else:
            libraries, events = execute("records")
        summary = next((event for event in events if event.get("event") == "record_summary"), {})
        # Queue cases skip the baseline and pressure phases; their records are checked per trial below.
        if args.case not in QUEUE_CASES + ("stop-inflight",):
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
            runner.check("actual-normal-mode-native-publication", len([event for event in events if
                         event.get("event") == "simulation_bypass" and event.get("normal_native_published")]) == 4)
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
            switches = [event for event in events if event.get("event") == "input_source_switch"]
            runner.check("actual-six-input-failing-source-switch", len({event["record"] for event in switches}) == 6 and all(
                         event["native_failure_alarm"] and event["siol_selected"] and event["terminal_released_once"] and
                         not event["native_publication"] and event["normal_failure_preserved"] and
                         not event["ownerless_admission"] for event in switches))
            fault_events = [json.loads(line) for line in (output / "record-response-drop.stdout").read_text().splitlines()]
            runner.check("actual-policy-GET-count", len([event for event in fault_events if
                         event.get("event") == "fault_request" and event.get("command") == 160]) == 12)
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
        if args.case == "active-unforced":
            trials = [event for event in events if event.get("event") == "active_unforced"]
            runner.check("unforced-five-outputs-ran", len(trials) == 5)
            for event in trials:
                same = (float(event["wire"]) == float(event["latest"])) if event["record"].endswith("Ao") else event["wire"] == event["latest"]
                runner.check(event["record"] + ":unforced-latest-admitted-and-sent",
                             event["generation"] == 2 and event["completions_delta"] == 2 and
                             event["sevr"] == "NO_ALARM" and event["pact"] == 0 and same)
            exercised = len([event for event in trials if event["branch_exercised"]])
            gaps = [event["retired_us"] - event["result_us"] for event in trials if event["result_us"] and event["retired_us"]]
            events.append({"event": "unforced_summary", "trials": len(trials), "branch_exercised": exercised,
                           "branch_status": "run" if exercised else "not run", "result_to_retired_us": gaps})
        if args.case == "deadline-queue":
            trials = {event["trial"]: event for event in events if event.get("event") == "deadline_queue"}
            summaries = [event for event in events if event.get("event") == "record_summary"]
            runner.check("deadline-trials-completed", len(summaries) == DEADLINE_TRIALS + 2)
            below, above = trials.get("below", {}), trials.get("above", {})
            # The proxy sees the first SET and the follow-up SET; the queued second generation is never sent.
            runner.check("below-threshold-queued-generation-not-sent",
                         below.get("generation") == 2 and not below.get("second_dispatched") and
                         below.get("proxy_sets") == 2 and below.get("stat") == "COMM" and below.get("sevr") == "INVALID")
            runner.check("below-threshold-never-sent-message", below.get("amsg") == "deadline before send")
            # (behindRetirement, behindAdmitted, behindNeverSent): the queued generation was admitted behind its
            # predecessor in both trials and expired unsent only below the threshold.
            runner.check("queue-report-counters",
                         queue_counters(output / "records-below.stdout") == (0, 1, 1) and
                         queue_counters(output / "records-above.stdout") == (0, 1, 0))
            all_relaunches = next((event for event in events if event.get("event") == "deadline_threshold_all"), {})
            listed = all_relaunches.get("samples_us", [])
            measured = [event["admission_to_ready_us"] or event["deadline_to_ready_us"]
                        for event in events if event.get("event") == "deadline_queue"]
            runner.check("threshold-over-every-relaunch",
                         all_relaunches.get("trials") == DEADLINE_TRIALS + 2 and
                         sorted(listed) == sorted(measured) and bool(listed) and
                         all_relaunches.get("minimum_us") == min(listed) and
                         all_relaunches.get("maximum_us") == max(listed) and
                         below.get("budget_ms", 0) * 1000 < all_relaunches.get("minimum_us", 0) and
                         above.get("budget_ms", 0) * 1000 > all_relaunches.get("maximum_us", 0))
            # A successor keeps the deadline of its own admission, so after the operator's put it can be dispatched at
            # most two budgets later, plus the callback delay between the predecessor's deadline and its completion.
            dispatched = [event for event in trials.values() if event.get("second_dispatched")]
            runner.check("put-to-dispatch-within-late-application-bound",
                         len(dispatched) >= 1 and all(
                             event["first_admitted_us"] < event["put_at_us"] and
                             event["put_to_dispatch_us"] == event["dispatched_us"] - event["put_at_us"] and
                             0 < event["put_to_dispatch_us"] <= 2 * event["budget_ms"] * 1000 +
                             max(0, event["admitted_us"] - (event["first_admitted_us"] + event["budget_ms"] * 1000))
                             for event in dispatched))
            runner.check("followup-after-never-sent-has-no-stale-message",
                         below.get("followup_generation") == 3 and below.get("followup_stat") == "COMM" and
                         below.get("followup_amsg") == "")
            runner.check("sent-deadline-without-never-sent-message",
                         len([event for event in trials.values() if event.get("second_dispatched")]) >= 1 and
                         all(event.get("amsg") != "deadline before send" for event in trials.values()
                             if event.get("second_dispatched")))
            runner.check("above-threshold-queued-generation-sent-after-ready",
                         above.get("generation") == 2 and above.get("second_dispatched") and
                         above.get("proxy_sets") == 2 and above.get("stat") == "COMM" and above.get("sevr") == "INVALID")
        if args.case == "near-deadline":
            trials = [event for event in events if event.get("event") == "near_deadline"]
            runner.check("near-deadline-trials-completed", len(trials) == NEAR_TRIALS)
            contained = len([event for event in trials if event["contained_after_result"]])
            exercised = len([event for event in trials if event["grace_exercised"]])
            events.append({"event": "near_deadline_summary", "trials": len(trials), "contained_after_result": contained,
                           "status": "observed" if contained else "not observed", "grace_exercised": exercised,
                           "grace_status": "run" if exercised else "not run"})
        if args.case == "accounting":
            windows = [event for event in events if event.get("event") == "accounting_window"]
            observed = [event for event in windows if event["observed"]]
            runner.check("two-generation-window-observed", bool(observed) and all(
                         event["peak_count"] == 2 and event["peak_bytes"] == 2 * event["single_bytes"] and
                         event["retirement_pending"] == 1 and event["queued"] == 1 for event in observed))
            runner.check("two-generation-trials-latest-sent", len(windows) == 3 and all(
                         event["generations"] == 2 and event["sevr"] == "NO_ALARM" for event in windows))
            limit = next((event for event in events if event.get("event") == "accounting_limit"), {})
            runner.check("count-limit-rejects-reprocess-synchronously", limit.get("limited_generations") == 1 and
                         limit.get("limited_stat") == "WRITE" and limit.get("limited_sevr") == "INVALID" and
                         limit.get("limited_pact") == 0)
            runner.check("raised-limit-admits-explicit-request", limit.get("explicit_sevr") == "NO_ALARM" and
                         limit.get("explicit_completions") == 1)
            events.append({"event": "accounting_summary", "trials": len(windows), "window_observed": len(observed)})
        if args.case == "stop-inflight":
            stop = next((event for event in events if event.get("event") == "stop_inflight"), {})
            rows = stop.get("records", [])
            runner.check("stop-inflight-every-record-completes-with-alarm",
                         len(rows) == 11 and stop.get("stopped") == 11 and stop.get("idle") == 11 and
                         all(row["pact"] == 0 and row["stat"] == "COMM" and row["sevr"] == "INVALID" and
                             row["published"] is False for row in rows))
            runner.check("stop-inflight-waveform-busy-clear", stop.get("waveform_busy") == 0 and
                         any(row["record"].endswith("Waveform") and row["busy"] == 0 for row in rows))
            runner.check("stop-inflight-completion-and-FLNK-once-each",
                         stop.get("completions_delta") == 11 and stop.get("flnk_delta") == 11)
            codes = [item["code"] for item in stop.get("timeline", [])]
            # One generation is on the worker channel and ten wait in the queue; no native result precedes the stop.
            runner.check("stop-inflight-one-sent-and-ten-queued-at-stop",
                         14 in codes and stop.get("pending_at_stop") == 11 and stop.get("held_queued") == 10 and
                         not any(item["code"] == 4 and item["at"] < stop.get("stop_at_us", 0)
                                 for item in stop.get("timeline", [])))
            runner.check("stop-inflight-drain-succeeds-and-worker-reaped",
                         stop.get("drain_failed") is False and stop.get("state") == 4 and stop.get("settled") is True and
                         stop.get("reap_at_us", 0) > 0 and stop.get("retirement_pending_after") == 0)
        if args.case in ("stop-queued", "rebuild"):
            stop = next((event for event in events if event.get("event") == "stop_queued"), {})
            # Product checks apply only when the harness observed the reprocess before stopping. A missing event
            # without a trial failure line means the stop under test itself failed, which the checks judge.
            before_wait = "record test failed after" in (output / "records.stderr").read_text(errors="replace")
            if stop.get("wait_exit") not in ("queued", "rejected") and (stop or before_wait):
                write_json(output / "record-observations.json", events)
                raise InconclusiveWait()
            runner.check("stop-with-retirement-pending-and-queued-successor",
                         stop.get("queued_successor") and stop.get("held_retirement_pending") == 1 and stop.get("held_queued") == 1)
            runner.check("queued-successor-completes-stopping-once",
                         stop.get("generation") == 2 and stop.get("pact") == 0 and stop.get("sevr") == "INVALID" and
                         stop.get("flnk_delta") == 2 and stop.get("completions_delta") == 2 and stop.get("proxy_sets") == 1)
            runner.check("stopping-successor-has-no-never-sent-message", stop.get("amsg") == "")
            runner.check("queue-report-counters", queue_counters(output / "records.stdout") == (0, 1, 0))
            runner.check("drain-succeeds-before-predecessor-reap",
                         stop.get("drain_failed") is False and stop.get("settled") is True)
        if args.case == "rebuild":
            rebuilt = next((event for event in events if event.get("event") == "rebuild"), {})
            counters = queue_counter_lines(output / "records.stdout")
            runner.check("rebuild-second-activation-new-worker",
                         rebuilt.get("second_activation") == rebuilt.get("first_activation", 0) + 1 and
                         rebuilt.get("first_worker_gone") is True and rebuilt.get("second_worker", 0) > 0 and
                         rebuilt.get("second_worker") != rebuilt.get("first_worker"))
            runner.check("rebuild-new-scheduler-counters",
                         len(counters) >= 2 and counters[0] == (0, 1, 0) and counters[-1] == (0, 0, 0))
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
    if args.case == "stop-inflight":
        inputs += [ROOT / "tests/rewrite/db/record-stop.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case in ("active", "active-unforced"):
        inputs += [ROOT / "tests/rewrite/db/record-active.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case in QUEUE_CASES:
        inputs += [ROOT / "tests/rewrite/db/record-queue.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "policy":
        inputs += [ROOT / "tests/rewrite/db/record-policy.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "numeric":
        inputs += [ROOT / "tests/rewrite/db/record-numeric.db"]
    inputs += [ROOT / "tests/rewrite/test_native.py"]
    inputs += [runner.products / name for name in ("snmp3RecordTest", "snmp3NativeProbe", "snmp3Worker", "snmp3NativeAgent")]
    passed = not aborted and bool(runner.checks) and all(check["passed"] for check in runner.checks)
    write_json(output / "results.json", {"passed": passed, "aborted": aborted, "checks": runner.checks,
               "inputs": {str(path): digest(path) for path in inputs}, "loaded_libraries": runner.identities,
               "scope": f"Record case {args.case}; qualifies this invocation only and does not close the full T1-T14 matrix"})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
