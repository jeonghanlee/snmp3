#!/usr/bin/env python3
"""Verify old UDP responses through the real generated AP8932 IOC and CA/PVA.

The pinned outer PDU constructs every response. Only its UDP delivery is held.
After a valid cold-start baseline the original scanner is made Passive. One
manual raw-record GET expires without a reply. A second GET of the same OID
must use the same client port and a different request ID. The old response is
delivered either while the second GET is pending or after its successful reply.
It must not complete the newer request, clear its alarm, or replace its value
or timestamp. A subsequent GET and duplicate old response check recovery.

Every case owns a fresh IOC and agent. Required missing exposure is Incomplete
(2), an observed violation is Failed (1), and a completed success is Passed (0).
Controls alter only outer-agent delivery or the real response request ID.
Raw client bytes, packet identities, delivery bounds, snapshots and process
outcomes are retained. No equipment, v3, grouped-binding, long-duration,
large-volume, cross-host replay or simultaneous-device-sample claim is made;
the corresponding execution safeguards are outside this bounded contract.
"""

import argparse
from contextlib import contextmanager
import hashlib
import json
import math
import os
from pathlib import Path
import socket
import sys
import threading
import time
import traceback

from build_apcpdu import ARCH, ROOT, digest, load_peer, write_json
from test_apcpdu_operations import Incomplete, Operations, SetupFailure


OID = "rPDU2PhaseStatusCurrent.1"
RAW = "PhaseCurrRaw_"
PUBLIC = "PhaseCurr"
TIMEOUT_MS = 2000
PENDING_SECONDS = 0.35
STABLE_SECONDS = 0.6
CASES = ("old-first", "new-first", "cleanup")
CONTROLS = ("none", "accept-stale", "no-stale-delivery", "no-pva")


class Reordered(Operations):
    def config(self, folder, snmp_port, version="2c"):
        path = super().config(folder, snmp_port, version)
        document = json.loads(path.read_text())
        document["profiles"][0].update(timeoutMs=TIMEOUT_MS, retries=0)
        write_json(path, document)
        return path

    @contextmanager
    def peer(self, name):
        folder = self.output / name
        folder.mkdir()
        module = load_peer(self.ioc / "source/tests/snmp_peer.py")
        peer = module.Pdu(0, folder)
        peer.enabled = True
        peer.held, peer.timeline, peer.holding = [], [], False
        gate = threading.RLock()

        def parse(data):
            outer = list(module.items(next(module.items(data))[1]))
            fields = list(module.items(outer[2][1]))
            pairs = [list(module.items(value)) for _, value, _ in module.items(fields[3][1])]
            names = [peer.names.get(pair[0][1], pair[0][1].hex()) for pair in pairs]
            values = {key: int.from_bytes(pair[1][1], "big", signed=True)
                      for key, pair in zip(names, pairs) if pair[1][0] in (2, 0x42)}
            return outer, fields, {"pdu": outer[2][0], "request_id": int.from_bytes(fields[0][1], "big", signed=True),
                                   "oids": names, "phase_value": values.get(OID)}

        def send(packet, label, current=None):
            data = packet["bytes"]
            if current is not None:
                outer, fields, _ = parse(data)
                _, current_fields, _ = parse(current["bytes"])
                response = module.tlv(outer[2][0], current_fields[0][2] + b"".join(f[2] for f in fields[1:]))
                data = module.tlv(0x30, outer[0][2] + outer[1][2] + response)
            _, _, meta = parse(data)
            row = {"event": "delivery", "label": label, **meta,
                   "client_port": packet["address"][1], "sha256": hashlib.sha256(data).hexdigest(),
                   "length": len(data), "start_ns": time.monotonic_ns()}
            if self.control == "no-stale-delivery" and label.endswith(":old"):
                row.update(event="withheld", end_ns=time.monotonic_ns())
                with gate:
                    peer.timeline.append(row)
                return row
            row["sent_bytes"] = packet["socket"].sendto(data, packet["address"])
            row["end_ns"] = time.monotonic_ns()
            with gate:
                peer.timeline.append(row)
            return row

        peer.deliver = send

        class Reply:
            def __init__(self, sock, address):
                self.sock, self.address = sock, address

            def sendto(self, data, address):
                _, _, meta = parse(data)
                packet = {"bytes": data, "socket": self.sock, "address": address, "metadata": meta}
                with gate:
                    peer.timeline.append({"event": "response", "captured_ns": time.monotonic_ns(),
                                          "client_port": address[1], **meta})
                    if peer.holding and OID in meta["oids"]:
                        peer.held.append(packet)
                        return len(data)
                send(packet, "ordinary")
                return len(data)

        class Handler(module.Handler):
            def handle(self):
                data, sock = self.request
                try:
                    _, _, meta = parse(data)
                    with gate:
                        peer.timeline.append({"event": "request", "arrival_ns": time.monotonic_ns(),
                                              "client_port": self.client_address[1], **meta})
                    self.request = data, Reply(sock, self.client_address)
                    super().handle()
                except Exception:
                    with gate:
                        peer.errors.append(traceback.format_exc())

        peer.RequestHandlerClass = Handler
        blackhole = None
        try:
            if self.control == "no-pva":
                blackhole = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                blackhole.setblocking(False)
                blackhole.bind(("127.0.0.1", 0))
                self.env["EPICS_PVA_ADDR_LIST"] = "127.0.0.1:" + str(blackhole.getsockname()[1])
                write_json(folder / "pva-control.json", {"search_address": self.env["EPICS_PVA_ADDR_LIST"],
                           "fault": "Owned UDP socket does not answer actual client searches"})
                write_json(self.output / "environment.json", {k: v for k, v in self.env.items()
                           if k.startswith("EPICS_")})
            config = self.config(folder, peer.server_address[1])
            with self.application(name, folder, config) as child:
                yield peer, child
        finally:
            discarded = 0
            if blackhole is not None:
                try:
                    while True:
                        blackhole.recvfrom(65535)
                        discarded += 1
                except OSError as error:
                    if not isinstance(error, BlockingIOError):
                        peer.errors.append("PVA fault socket cleanup: " + repr(error))
                finally:
                    blackhole.close()
            peer.close()
            write_json(folder / "timeline.json", peer.timeline)
            write_json(folder / "peer.json", {"errors": peer.errors, "held_responses": len(peer.held),
                       "socket_closed": peer.fileno() == -1, "requests": len(peer.requests),
                       "discarded_pva_searches": discarded,
                       "pva_fault_socket_closed": blackhole is None or blackhole.fileno() == -1})
        self.check(name + ":fixture-clean", peer.fileno() == -1 and not peer.errors, peer.errors)

    def snapshot(self):
        started = time.monotonic_ns()
        fields = self.fields(RAW + ".VAL", RAW + ".PACT", RAW + ".SEVR", PUBLIC + ".VAL", PUBLIC + ".SEVR")
        group, _ = self.group()
        keys = ("phase.current.value", "phase.current.alarm.severity", "phase.current.timeStamp.secondsPastEpoch",
                "phase.current.timeStamp.nanoseconds")
        if not all(key in group for key in keys):
            self.missing("phase:complete-pva-observation", sorted(group))
        selected = {key: group[key] if not isinstance(group[key], float) or math.isfinite(group[key]) else None
                    for key in keys}
        return {"start_ns": started, "end_ns": time.monotonic_ns(), "ca": fields, "pva": selected}

    def observe_phase(self, label, seconds):
        started = time.monotonic_ns()
        samples = []
        try:
            while time.monotonic_ns() - started < int(seconds * 1e9):
                samples.append(self.snapshot())
                time.sleep(0.04)
        finally:
            write_json(self.output / (label + "-snapshots.json"), {"start_ns": started,
                       "end_ns": time.monotonic_ns(), "samples": samples})
        if len(samples) < 2:
            self.missing(label + ":sample-coverage", len(samples))
        return samples

    def ready(self, peer, name):
        try:
            self.healthy()
        except AssertionError as error:
            if any(check["passed"] is False for check in self.checks):
                raise
            self.missing(name + ":valid-baseline", str(error))
        self.put("SCANNER.SCAN", "Passive")
        self.until(lambda: self.fields(RAW + ".PACT")[RAW + ".PACT"] == "0", name + ":initial-drain")
        baseline = self.snapshot()
        self.check(name + ":baseline-value", float(baseline["ca"][PUBLIC + ".VAL"]) == 12 and
                   baseline["pva"]["phase.current.value"] == 12, baseline)
        peer.holding = True

    def capture(self, peer, number, value, name):
        peer.set("rPDU2PhaseStatusCurrent", 1, value)
        self.put(RAW + ".PROC", 1)
        self.until(lambda: len(peer.held) >= number, name + ":captured-response")
        packet = peer.held[number - 1]
        self.check(name + ":fixture-value", packet["metadata"]["phase_value"] == value, packet["metadata"])
        self.check(name + ":request-active", self.fields(RAW + ".PACT")[RAW + ".PACT"] == "1")
        return packet

    def completed(self, name, raw):
        self.until(lambda: self.fields(RAW + ".PACT")[RAW + ".PACT"] == "0", name + ":completed")
        sample = self.snapshot()
        self.check(name + ":current-value", float(sample["ca"][RAW + ".VAL"]) == raw and
                   math.isclose(float(sample["ca"][PUBLIC + ".VAL"]), raw / 10, abs_tol=1e-9) and
                   sample["pva"]["phase.current.value"] is not None and
                   math.isclose(sample["pva"]["phase.current.value"], raw / 10, abs_tol=1e-9), sample)
        self.check(name + ":healthy", sample["ca"][RAW + ".SEVR"] == "0" and
                   sample["ca"][PUBLIC + ".SEVR"] == "0" and sample["pva"]["phase.current.alarm.severity"] == 0,
                   sample)
        return sample

    def stable(self, name, reference):
        samples = self.observe_phase(name, STABLE_SECONDS)
        self.check(name + ":current-value-and-timestamp-preserved", all(
            sample["ca"] == reference["ca"] and sample["pva"] == reference["pva"] for sample in samples), samples)

    def old_delivery(self, peer, old, current, label):
        event = peer.deliver(old, label, current if self.control == "accept-stale" else None)
        deliveries = [row for row in list(peer.timeline) if row["event"] == "delivery" and row.get("label") == label]
        if len(deliveries) != 1:
            self.missing(label + ":actual-delivery", {"observed_deliveries": len(deliveries), "event": event})
        if event["sent_bytes"] != event["length"]:
            self.missing(label + ":actual-delivery", event)
        return event

    def order(self, name):
        with self.peer(name) as (peer, _):
            self.ready(peer, name)
            old = self.capture(peer, 1, 135, name + ":old")
            self.until(lambda: self.fields(RAW + ".PACT", RAW + ".SEVR") ==
                       {RAW + ".PACT": "0", RAW + ".SEVR": "3"}, name + ":expired")
            timeout = self.snapshot()
            self.check(name + ":timeout-unavailable", timeout["ca"][PUBLIC + ".SEVR"] == "3" and
                       timeout["pva"]["phase.current.alarm.severity"] == 3, timeout)
            current = self.capture(peer, 2, 147, name + ":new")
            if old["address"] != current["address"]:
                self.missing(name + ":same-session-exposure", [old["address"], current["address"]])
            self.check(name + ":distinct-request-identities", old["metadata"]["request_id"] !=
                       current["metadata"]["request_id"])
            if name == "old-first":
                old_event = self.old_delivery(peer, old, current, name + ":old")
                pending = self.observe_phase(name + "-pending", PENDING_SECONDS)
                self.check(name + ":pending-remains-invalid", all(
                    sample["ca"][RAW + ".PACT"] == "1" and sample["ca"][RAW + ".SEVR"] == "3" and
                    sample["ca"][PUBLIC + ".SEVR"] == "3" and
                    sample["pva"]["phase.current.alarm.severity"] == 3 for sample in pending), pending)
                new_event = peer.deliver(current, name + ":new")
                self.check(name + ":observed-delivery-order", old_event["end_ns"] <
                           pending[0]["start_ns"] < pending[-1]["end_ns"] < new_event["start_ns"])
                reference = self.completed(name + ":new", 147)
            else:
                new_event = peer.deliver(current, name + ":new")
                reference = self.completed(name + ":new", 147)
                old_event = self.old_delivery(peer, old, current, name + ":old")
                self.check(name + ":observed-delivery-order", new_event["end_ns"] <
                           reference["start_ns"] < reference["end_ns"] < old_event["start_ns"])
            self.stable(name + "-after-old", reference)
            peer.holding = False
            peer.set("rPDU2PhaseStatusCurrent", 1, 159)
            self.put(RAW + ".PROC", 1)
            reference = self.completed(name + ":recovery", 159)
            peer.deliver(old, name + ":old-duplicate")
            self.stable(name + "-after-duplicate", reference)
            self.check(name + ":get-only", bool(peer.requests) and not any(pdu == 0xA3 for pdu, _ in peer.requests))

    def cleanup(self):
        try:
            with self.peer("cleanup") as (peer, _):
                self.ready(peer, "cleanup")
                self.capture(peer, 1, 135, "cleanup:old")
                raise SetupFailure("Deliberate setup failure with a real response held at the outer agent")
        except SetupFailure:
            pass
        self.check("cleanup:normal-resource-stop", all(p.get("stopped") and not p.get("forced") and
                   p["returncode"] == 0 for p in self.processes if p["name"] != "repeater"))
        self.shutdown_evidence("cleanup")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ioc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", choices=("all", *CASES), default="all")
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("--control", choices=CONTROLS, default="none")
    args = parser.parse_args()
    if args.control != "none" and args.case != "old-first":
        parser.error("Controls require --case old-first")
    ioc, output = args.ioc.resolve(), args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / "work"):
        parser.error("--output must be a new directory under this checkout's work/")
    os.umask(0o077)
    output.mkdir(parents=True, mode=0o700)
    runner, error, code = None, None, 0
    started = time.time_ns()
    selected = list(CASES if args.case == "all" else (args.case,))
    if args.reverse:
        selected.reverse()
    try:
        runner = Reordered(ioc, output, args.control)
        identities = {**runner.build["products"], **{
            str(ioc / "source" / path): expected for path, expected in runner.build["source_hashes"].items()}}
        for path, expected in identities.items():
            if digest(path) != expected:
                raise Incomplete("Build identity changed: " + path)
        runner.identities.update(identities)
        helpers = (Path(__file__), ROOT / "tests/rewrite/test_apcpdu_operations.py",
                   ROOT / "tests/rewrite/test_apcpdu.py", ROOT / "tests/rewrite/build_apcpdu.py",
                   ROOT / "bin" / ARCH / "snmp3Worker", ROOT / "lib" / ARCH / "libsnmp3Native.so", ioc / "build.json")
        runner.identities.update({str(path): digest(path) for path in helpers})
        write_json(output / "identities.json", runner.identities)
        with runner.process("repeater", [runner.base / "bin" / ARCH / "caRepeater"]):
            for name in selected:
                runner.cleanup() if name == "cleanup" else runner.order(name)
    except BaseException as exc:
        code = 130 if isinstance(exc, KeyboardInterrupt) else 1 if isinstance(exc, AssertionError) else 2
        error = traceback.format_exc()
        (output / "error.txt").write_text(error)
        print(error, flush=True)
    finally:
        if runner:
            for path, expected in runner.identities.items():
                if not Path(path).is_file() or digest(path) != expected:
                    error = (error or "") + "\nIdentity changed: " + path
                    code = code or 2
            if any(check["passed"] is False for check in runner.checks):
                code = 1
            if code == 0 and any(not p.get("stopped") or p.get("forced") or
                                 (p["name"] != "repeater" and p.get("returncode") != 0)
                                 for p in runner.processes):
                code, error = 2, "Owned process did not stop normally"
        result = {"passed": code == 0, "status": "PASS" if code == 0 else "FAIL" if code == 1 else "INCOMPLETE",
                  "returncode": code, "cases": selected, "control": args.control,
                  "argv": sys.argv, "cwd": str(Path.cwd()),
                  "checks": runner.checks if runner else [], "processes": runner.processes if runner else [],
                  "error": error, "start_utc_ns": started, "end_utc_ns": time.time_ns(),
                  "limits": ["Loopback SNMPv2c only", "Single selected phase-current OID",
                             "No hardware, legacy comparator or full M7 acceptance"]}
        write_json(output / "result.json", result)
    print(f"Evidence: {output}; checks: {len(result['checks'])}; result: {result['status']}")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
