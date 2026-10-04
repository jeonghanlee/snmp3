#!/usr/bin/env python3
"""Qualify real CA clients through the production IOC, records and native agent."""
import argparse
import json
import socket
import subprocess
import time
from pathlib import Path
from test_native import ROOT, ARCH, Runner, digest, write_json
from run_independence import owns_udp

READY_SECONDS = 10
PROCESS_SECONDS = 15
DELAY_MS = 1000


def private_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as selected:
        selected.bind(("127.0.0.1", 0))
        return selected.getsockname()[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()
    products = args.products.resolve() if args.products else ROOT / "bin" / ARCH
    if not (products / "snmp3Ioc").is_file():
        parser.error("selected products must include the production snmp3Ioc")
    base = Path(next(line.split("=", 1)[1].strip() for line in
                    (ROOT / "configure/RELEASE.local").read_text().splitlines()
                    if line.strip().startswith("EPICS_BASE") and "=" in line))
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    runner = Runner(output, products, args.sanitizers)
    ioc = repeater = None
    logs = []
    aborted = False
    observations = []
    # PINI input completes once; every admitted CA operation below adds its own completion.
    expected_completions = 1
    contexts = (ROOT / "tests/rewrite/db/record-ca.db").read_text().count('field(DTYP, "snmp3")')
    workers = []
    input_files = sorted((ROOT / "snmp3App/src").glob("*.cpp")) + sorted((ROOT / "snmp3App/src").glob("*.h"))
    input_files += sorted((ROOT / "snmp3App/native").glob("*.cpp")) + sorted((ROOT / "snmp3App/native").glob("*.h"))
    input_files += [Path(__file__).resolve(), ROOT / "tests/rewrite/test_native.py",
                    ROOT / "tests/rewrite/run_independence.py",
                    ROOT / "tests/rewrite/db/record-ca.db", ROOT / "tests/rewrite/NativeAgent.cpp",
                    ROOT / "tests/rewrite/helpers/udp_fault.py",
                    ROOT / "dbd/snmp3Ioc.dbd", ROOT / "snmp3App/src" / ("O." + ARCH) /
                    "snmp3Ioc_registerRecordDeviceDriver.cpp"]
    input_files += [products / name for name in ("snmp3Ioc", "snmp3Worker", "snmp3NativeProbe", "snmp3NativeAgent")]
    input_files += [base / "bin" / ARCH / name for name in ("caget", "caput", "caRepeater")]
    try:
        peer, secrets = runner.agent(4)
        server_port, repeater_port = private_port(), private_port()
        while repeater_port == server_port:
            repeater_port = private_port()
        runner.env.update(EPICS_CA_SERVER_PORT=str(server_port), EPICS_CA_REPEATER_PORT=str(repeater_port),
                          EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST="127.0.0.1:" + str(server_port),
                          EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
                          EPICS_CAS_BEACON_ADDR_LIST="127.0.0.1:" + str(repeater_port))
        write_json(output / "ports.json", {"server": server_port, "repeater": repeater_port})
        delayed, _ = runner.fault("ca-response-delay", 4, peer, "delay", delay_ms=DELAY_MS)
        specs = [("WideRead", 5, "get", "counter64", 1, "Local"), ("WideWrite", 5, "set", "counter64", 1, "Local"),
                 ("TextRead", 6, "get", "octets", 1024, "Local"), ("TextWrite", 6, "set", "octets", 1024, "Local"),
                 ("IntegerRead", 1, "get", "integer", 1, "Local"), ("IntegerWrite", 1, "set", "integer", 1, "Local"),
                 ("ActiveFloatWrite", 9, "set", "opaqueFloat", 1, "Delayed"),
                 ("ActiveIntegerWrite", 1, "set", "integer", 1, "Delayed"),
                 ("ActiveCounter64Write", 5, "set", "counter64", 1, "Delayed"),
                 ("ActiveOctetsWrite", 6, "set", "octets", 1024, "Delayed")]
        profile = {"id": "Local", "version": "2c", "communityFile": str(secrets["community"]),
                   "timeoutMs": 100, "retries": 0}
        config = output / "records.json"
        write_json(config, {"schema": 1, "profiles": [profile, dict(profile, id="Delayed", timeoutMs=3 * DELAY_MS)],
                   "endpoints": [{"id": "Local", "address": "127.0.0.1",
                   "port": int(peer.rsplit(":", 1)[1]), "profile": "Local"},
                   {"id": "Delayed", "address": "127.0.0.1",
                   "port": int(delayed.rsplit(":", 1)[1]), "profile": "Delayed"}],
                   "bindings": [{"id": name, "endpoint": endpoint, "operation": operation,
                   "valueType": tag, "capacity": capacity, "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0"}
                   for name, index, operation, tag, capacity, endpoint in specs]})
        script = output / "startup.cmd"
        script.write_text('on error break\n'
                          f'dbLoadDatabase("{ROOT}/dbd/snmp3Ioc.dbd")\n'
                          'snmp3Ioc_registerRecordDeviceDriver(pdbbase)\n'
                          f'snmp3Load("{config}","{products}/snmp3NativeProbe")\n'
                          f'snmp3WorkerPath("{products}/snmp3Worker")\n'
                          f'dbLoadRecords("{ROOT}/tests/rewrite/db/record-ca.db","P=Ca_")\n'
                          'iocInit\nsnmp3Report\n')
        for name in ("repeater", "ioc"):
            logs += [(output / (name + ".stdout")).open("xb"), (output / (name + ".stderr")).open("xb")]
        repeater_argv = [str(base / "bin" / ARCH / "caRepeater")]
        repeater = subprocess.Popen(repeater_argv, stdout=logs[0], stderr=logs[1], env=runner.env)
        end = time.monotonic() + READY_SECONDS
        while time.monotonic() < end and repeater.poll() is None and not owns_udp(repeater.pid, repeater_port):
            time.sleep(0.01)
        runner.check("private-repeater-owned", repeater.poll() is None and owns_udp(repeater.pid, repeater_port))
        ioc_argv = [str(products / "snmp3Ioc"), str(script)]
        ioc = subprocess.Popen(ioc_argv, stdin=subprocess.PIPE, stdout=logs[2], stderr=logs[3], env=runner.env)
        end = time.monotonic() + READY_SECONDS
        def initialized():
            return b"iocRun: All initialization complete" in (output / "ioc.stderr").read_bytes()
        while time.monotonic() < end and ioc.poll() is None:
            if initialized():
                break
            time.sleep(0.01)
        ready = ioc.poll() is None and initialized()
        runner.check("actual-production-IOC-ready", ready)
        if not ready:
            raise RuntimeError("actual IOC startup failed")
        ordinal = 0

        def start_client(tool, arguments):
            # Background CA client whose exit time is compared with proxy delivery timestamps.
            nonlocal ordinal
            ordinal += 1
            name = f"ca-{ordinal:03d}-{tool}"
            argv = [str(base / "bin" / ARCH / tool), *arguments]
            stdout = (output / (name + ".stdout")).open("xb")
            stderr = (output / (name + ".stderr")).open("xb")
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=runner.env)
            return {"name": name, "argv": argv, "child": child, "streams": (stdout, stderr),
                    "started_ns": time.monotonic_ns()}

        def finish_client(entry):
            child, forced = entry["child"], False
            try:
                code = child.wait(timeout=PROCESS_SECONDS)
            except subprocess.TimeoutExpired:
                forced = True
                child.kill()
                code = child.wait(timeout=PROCESS_SECONDS)
            exit_ns = time.monotonic_ns()
            for stream in entry["streams"]:
                stream.close()
            result = {"argv": entry["argv"], "pid": child.pid, "returncode": code, "forced_cleanup": forced,
                      "child_reaped": child.returncode is not None, "started_ns": entry["started_ns"],
                      "exit_ns": exit_ns, "loaded_libraries": runner.loader_identity(output / (entry["name"] + ".stderr"))}
            write_json(output / (entry["name"] + ".receipt.json"), result)
            return result

        def client(tool, arguments, expected=0):
            nonlocal ordinal
            ordinal += 1
            name = f"ca-{ordinal:03d}-{tool}"
            argv = [str(base / "bin" / ARCH / tool), "-w", "5", *arguments]
            started = time.monotonic_ns()
            forced = False
            with (output / (name + ".stdout")).open("xb") as stdout:
                with (output / (name + ".stderr")).open("xb") as stderr:
                    child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=runner.env)
                    try:
                        code = child.wait(timeout=PROCESS_SECONDS)
                    except subprocess.TimeoutExpired:
                        forced = True
                        child.kill()
                        code = child.wait(timeout=PROCESS_SECONDS)
            libraries = runner.loader_identity(output / (name + ".stderr"))
            write_json(output / (name + ".receipt.json"), {"argv": argv, "pid": child.pid,
                       "returncode": code, "expected_returncode": expected, "forced_cleanup": forced,
                       "child_reaped": child.returncode is not None, "elapsed_ns": time.monotonic_ns() - started,
                       "loaded_libraries": libraries})
            runner.check(name + ":return", code == expected and not forced)
            if forced or code != expected:
                raise RuntimeError("actual CA client failed")
            return (output / (name + ".stdout")).read_text().strip()

        def value(name, long_string=False):
            return client("caget", ["-t", "-S" if long_string else "-s", name])

        def process(name):
            nonlocal expected_completions
            client("caput", ["-c", "-t", name + ".PROC", "1"])
            expected_completions += 1
            runner.check(name + ":PACT-cleared", value(name + ".PACT") == "0")
            runner.check(name + ":NO_ALARM", value(name + ".SEVR") == "NO_ALARM")

        end = time.monotonic() + READY_SECONDS
        while value("Ca_Pini.PACT") != "0" and time.monotonic() < end:
            pass
        runner.check("PINI-native-input", value("Ca_Pini") == "-123")
        runner.check("PINI-Base-FLNK", value("Ca_Completed") == "1")
        process("Ca_UnsignedWide")
        runner.check("Counter64-UINT64_MAX-decimal-CA", value("Ca_UnsignedWide") == "18446744073709551615")
        for number in ("9007199254740993", "9223372036854775807"):
            client("caput", ["-c", "-t", "Ca_WideOut", number])
            expected_completions += 1
            process("Ca_WideIn")
            runner.check("int64-decimal-CA-SET-GET:" + number,
                         value("Ca_WideOut") == number and value("Ca_WideIn") == number)
            observations.append({"event": "ca_wide", "decimal": number, "exact_native_GET": True})
        for length in (0, 39, 40, 200, 255):
            text = "T" * length
            client("caput", ["-c", "-t", "-S", "Ca_TextOut.VAL$", text])
            expected_completions += 1
            process("Ca_TextIn")
            runner.check("long-string-CHAR-CA-SET-GET:" + str(length),
                         value("Ca_TextOut.VAL$", True) == text and value("Ca_TextIn.VAL$", True) == text and
                         value("Ca_TextOut.LEN") == str(length + 1) and value("Ca_TextIn.LEN") == str(length + 1))
            observations.append({"event": "ca_long_string", "data_bytes": length, "exact_native_GET": True})
        short_view = value("Ca_TextIn")
        runner.check("CA-DBR_STRING-short-view", short_view == "T" * 39)
        client("caput", ["-c", "-t", "-S", "Ca_TextOut.VAL$", "B" * 300], expected=1)
        diagnostic = (output / f"ca-{ordinal:03d}-caput.stderr").read_text()
        process("Ca_TextIn")
        runner.check("CA-client-capacity-rejection", "Invalid element count requested" in diagnostic and
                     value("Ca_TextOut.LEN") == "256" and value("Ca_TextOut.VAL$", True) == "T" * 255 and
                     value("Ca_TextIn.VAL$", True) == "T" * 255)
        observations.append({"event": "ca_client_capacity", "client_data_bytes": 300,
                             "rejected_before_put": True, "preserved_data_bytes": 255, "exact_native_GET": True})
        client("caput", ["-c", "-t", "Ca_ShortOut", "short-wire"])
        expected_completions += 1
        process("Ca_ShortIn")
        runner.check("short-string-CA-SET-GET", value("Ca_ShortIn") == "short-wire")

        def proxy_events(kind, command=None):
            return [event for event in (json.loads(line) for line in
                    (output / "ca-response-delay.stdout").read_text().splitlines() if line.startswith("{"))
                    if event.get("event") == kind and (command is None or event.get("command") == command)]

        def agent_values(index):
            return [event for event in (json.loads(line) for line in
                    (output / "agent-ipv4-1.stdout").read_text().splitlines() if line.startswith("{"))
                    if event.get("event") == "agent_set_value" and event.get("index") == index and
                    event.get("context") == 0]

        def wait_for(predicate, seconds=PROCESS_SECONDS):
            end = time.monotonic() + seconds
            while time.monotonic() < end and not predicate():
                time.sleep(0.01)
            return predicate()

        def stored(event, text):
            # Agent-side record of the value a SET stored: decimal, or octets length plus hex prefix.
            if "value" in event:
                return event["value"]
            return f"{event['length']}:{event['prefix_hex']}" if text else None

        def expected_stored(kind, payload):
            if kind == "Ao":
                return "%.9g" % float(payload)
            if kind in ("Stringout", "Lso"):
                data = payload.encode()
                return f"{len(data)}:{data[:32].hex()}"
            return payload

        def settle(name, sets, deliveries):
            # Phase sentinel: the first delayed response has reached the worker and the record is idle;
            # when a second SET appears it must also have been delivered before reading the outcome.
            delivered = wait_for(lambda: len(proxy_events("fault_delayed_delivery")) >= deliveries + 1)
            wait_for(lambda: len(proxy_events("fault_request", 163)) >= sets + 2, 3 * DELAY_MS / 1000)
            second = len(proxy_events("fault_request", 163)) >= sets + 2
            if second:
                wait_for(lambda: len(proxy_events("fault_delayed_delivery")) >= deliveries + 2)
            wait_for(lambda: value(name + ".PACT") == "0")
            return delivered, second

        # Each output receives a second CA write while its first SET response is held by the outer UDP
        # delay; Base then reprocesses the record. Outcome is judged only from observed wire, agent and
        # record state: two SETs on the wire, the agent storing the first and then the latest value,
        # and the record idle without alarm. The plain put uses RPRO; the put-callback write is
        # deferred by Base until the first generation completes.
        actives = [("Ao", 9, ("1.5", "2.5"), ("3.5", "4.5")),
                   ("Longout", 1, ("31", "32"), ("33", "34")),
                   ("Int64out", 5, ("9007199254740993", "9007199254740995"),
                    ("9007199254740997", "9007199254740999")),
                   ("Stringout", 6, ("first-short", "latest-short"), ("third-short", "fourth-short")),
                   ("Lso", 6, ("F" * 200, "L" * 250), ("T" * 120, "U" * 230))]
        callback_bound = 3 * DELAY_MS / 1000 + 5
        flnk = 0
        for kind, index, plain, notify in actives:
            name = "Ca_Active" + kind
            long_string = kind == "Lso"
            field = name + ".VAL$" if long_string else name
            mode = ["-S"] if long_string else []
            text = kind in ("Stringout", "Lso")
            for route, (first, latest) in (("plain-put", plain), ("put-callback", notify)):
                label = f"{name}:{route}"
                sets = len(proxy_events("fault_request", 163))
                deliveries = len(proxy_events("fault_delayed_delivery"))
                stored_before = len(agent_values(index))
                client("caput", ["-t", *mode, field, first])
                applied = wait_for(lambda: len(proxy_events("fault_response", 162)) >= sets + 1)
                request = proxy_events("fault_request", 163)[sets] if applied else None
                notifier = None
                if route == "plain-put":
                    client("caput", ["-t", *mode, field, latest])
                else:
                    notifier = start_client("caput", ["-c", "-w", str(callback_bound), "-t", *mode, field, latest])
                state = {"PACT": value(name + ".PACT"), "RPRO": value(name + ".RPRO")}
                delivered, second = settle(name, sets, deliveries)
                notify_result = finish_client(notifier) if notifier else None
                requests = proxy_events("fault_request", 163)[sets:]
                first_delivery = min((event["monotonic_ns"] for event in proxy_events("fault_delayed_delivery")
                                      if request and event["monotonic_ns"] > request["monotonic_ns"]), default=0)
                values = [stored(event, text) for event in agent_values(index)[stored_before:]]
                outcome = {field_name: value(name + "." + field_name) for field_name in ("STAT", "SEVR", "AMSG", "PACT")}
                outcome["VAL"] = value(field, long_string)
                flnk_now = int(float(value("Ca_ActiveCompleted")))
                observation = {"event": "ca_active_output", "record": name, "route": route,
                               "state_after_second_write": state, "first_delivered": delivered,
                               "set_requests": len(requests), "agent_stored": values,
                               "outcome": outcome, "flnk_delta": flnk_now - flnk,
                               "second_set_after_first_delivery": second and len(requests) >= 2 and
                               requests[1]["monotonic_ns"] > first_delivery}
                if notify_result:
                    observation["put_callback"] = notify_result
                observations.append(observation)
                flnk = flnk_now
                expected_completions += 2
                runner.check(label + ":write-while-active", applied and state["PACT"] == "1")
                runner.check(label + ":two-SETs-second-after-first-delivery",
                             observation["second_set_after_first_delivery"] and len(requests) == 2)
                runner.check(label + ":agent-stored-first-then-latest",
                             values == [expected_stored(kind, first), expected_stored(kind, latest)])
                runner.check(label + ":idle-without-alarm",
                             outcome["SEVR"] == "NO_ALARM" and outcome["PACT"] == "0" and
                             (float(outcome["VAL"]) == float(latest) if kind == "Ao" else outcome["VAL"] == latest))
                if notify_result:
                    runner.check(label + ":callback-after-second-delivery",
                                 notify_result["returncode"] == 0 and not notify_result["forced_cleanup"] and
                                 len(proxy_events("fault_delayed_delivery")) >= deliveries + 2 and
                                 notify_result["exit_ns"] >= max(event["monotonic_ns"] for event in
                                                                 proxy_events("fault_delayed_delivery")[deliveries:]))
        client("caput", ["-c", "-t", "Ca_SignedOut", "-2147483648"])
        process("Ca_IntegerIn")
        process("Ca_SignedWideIn")
        expected_completions += 1
        runner.check("INT32_MIN-CA-SET-GET", value("Ca_SignedOut") == "-2147483648" and
                     value("Ca_IntegerIn") == "-2147483648" and value("Ca_SignedWideIn") == "-2147483648")
        process("Ca_WideIn")
        baseline = value("Ca_WideIn")
        client("caput", ["-c", "-t", "Ca_WideOut", "-9223372036854775808"])
        rejected = (value("Ca_WideOut"), value("Ca_WideOut.STAT"), value("Ca_WideOut.SEVR"), value("Ca_WideOut.PACT"))
        process("Ca_WideIn")
        after = value("Ca_WideIn")
        runner.check("INT64_MIN-CA-Counter64-rejected-without-SET",
                     rejected == ("-9223372036854775808", "WRITE", "INVALID", "0") and after == baseline)
        observations.append({"event": "ca_signed_minimum", "int32_min": value("Ca_IntegerIn"),
                             "int64_min_record": rejected, "native_before": baseline, "native_after": after})
        child_pids = set()
        for task in Path(f"/proc/{ioc.pid}/task").iterdir():
            try:
                child_pids.update(int(pid) for pid in (task / "children").read_text().split())
            except FileNotFoundError:
                continue
        for pid in sorted(child_pids):
            process_path = Path(f"/proc/{pid}")
            if (process_path / "exe").resolve() == (products / "snmp3Worker").resolve():
                started = (process_path / "stat").read_text().rsplit(")", 1)[1].split()[19]
                workers.append({"pid": pid, "start_time": started})
        runner.check("actual-owned-worker-PID", len(workers) == 1)
        write_json(output / "worker-identities.json", workers)
        ioc.stdin.write(b"snmp3Report\nexit\n")
        ioc.stdin.flush()
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"exception_class": type(error).__name__})
    finally:
        for name, child, argv in (("ioc", ioc, locals().get("ioc_argv", [])),
                                  ("repeater", repeater, locals().get("repeater_argv", []))):
            if child is None:
                continue
            forced = False
            if name == "ioc" and child.poll() is None and aborted:
                try:
                    child.stdin.write(b"exit\n")
                    child.stdin.flush()
                except BrokenPipeError:
                    pass
            if name == "repeater" and child.poll() is None:
                child.terminate()
            try:
                code = child.wait(timeout=PROCESS_SECONDS)
            except subprocess.TimeoutExpired:
                forced = True
                child.kill()
                code = child.wait(timeout=PROCESS_SECONDS)
            if name == "ioc" and child.stdin:
                child.stdin.close()
            runner.check(name + ":normal-exit", not forced and (code == 0 if name == "ioc" else code in (0, -15)))
            libraries = runner.loader_identity(output / (name + ".stderr"))
            if name == "ioc":
                runner.check("IOC-native-free", bool(libraries) and not any("netsnmp" in path for path in libraries))
                final = (output / "ioc.stdout").read_text()
                runner.check("Base-normal-shutdown", "snmp3 hook: AfterShutdown" in final and
                             "state=Stopped admission=0" in final and "created=1 exited=1 joined=1" in final)
                runner.check("native-worker-reaped", bool(workers) and all(not Path(f"/proc/{worker['pid']}").exists()
                             for worker in workers) and "launches=1 reaps=1 forced=0" in final)
                runner.check("exact-native-completion-count-and-closed-gate",
                             f"contexts={contexts} active=0 pending=0 queued=0 running=0 inert=0 entered=0" in final and
                             f"completions={expected_completions} entryOpen=0 drainFailed=0 detachAllowed=1" in final)
            write_json(output / (name + ".receipt.json"), {"argv": argv, "pid": child.pid,
                       "returncode": code, "forced_cleanup": forced, "child_reaped": child.returncode is not None,
                       "loaded_libraries": libraries})
        for stream in logs:
            stream.close()
        runner.stop()
        runner.scan()
    passed = not aborted and bool(runner.checks) and all(check["passed"] for check in runner.checks)
    write_json(output / "record-observations.json", observations)
    write_json(output / "results.json", {"passed": passed, "aborted": aborted, "checks": runner.checks,
               "inputs": {str(path): digest(path) for path in input_files}, "loaded_libraries": runner.identities,
               "scope": "Actual production IOC/CA/record/native path; selected numeric/text/PINI/active-output/signed-minimum/non-isolated shutdown subset"})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
