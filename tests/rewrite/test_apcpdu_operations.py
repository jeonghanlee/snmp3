#!/usr/bin/env python3
"""Exercise APC commands, scanner pause/resume and SNMPv3 through the real generated IOC.

Contract: the pinned APC DB, scheduler, MIB and PDU values feed the actual IOC,
driver, worker and CA/PVA clients. Only the outer agent/UDP boundary is simulated.
Commands cover every outlet and the master, a notWritable reply and explicit retry;
SET acknowledgement and independent GET are distinct from physical confirmation.
Scanner pause changes only SCANNER.SCAN; independent 10-second timing records keep
running. Pause exceeds two information periods. Resume must publish changed inputs
without replaying missed information scans. v3 runs every security level with the
same APC OIDs and independent expected values; incorrect auth/privacy secrets must
reach the wire without publishing a healthy value. SHA/AES are the selected algorithms.

Evidence: bounded local runs retain original client bytes, packet metadata, peer
outcomes, actual intervals, source/product identities and process cleanup. Required
missing exposure is INCOMPLETE (2); an observed violation is FAIL (1), success is 0.
No sustained performance claim or large-volume/replay-transfer claim is made, so
those lifecycle safeguards are inapplicable. Counts describe actual requests, not
configured rates. Every case starts fresh, can run alone and retains failed output.
"""

import argparse
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import secrets
import shlex
import signal
import subprocess
import threading
import time
import traceback

from build_apcpdu import ARCH, ROOT, digest, load_peer, write_json
from test_apcpdu import Runner, WAIT_SECONDS, port

SCAN_SECONDS = 2
INFO_SECONDS = 10
PAUSE_SECONDS = 2 * INFO_SECONDS + SCAN_SECONDS
RESUME_SECONDS = INFO_SECONDS + 2 * SCAN_SECONDS
QUIET_SECONDS = 2 * SCAN_SECONDS
POLL_SECONDS = 0.2
ENGINE_ID = "8000000001020304"
USER = "localApcUser"
LEVELS = {"noAuthNoPriv": 1, "authNoPriv": 2, "authPriv": 3}
EXPECTED = {"phase.current.value": 12, "phase.voltage.value": 208,
            "phase.power.value": 2.5, "phase.apparent_power.value": 2.6,
            "phase.power_factor.value": 0.96, "bank1.current.value": 6,
            "bank2.current.value": 6, "limits.device_power.near.value": 3,
            "limits.device_power.over.value": 3.5}
CASES = ("commands", "scanner", "v3", "cleanup")


class Incomplete(Exception):
    pass


class SetupFailure(Exception):
    pass


def private_file(path, text):
    with open(path, "x", opener=lambda p, flags: os.open(p, flags, 0o600)) as stream:
        stream.write(text)


class Operations(Runner):
    def __init__(self, ioc, output, control="none"):
        super().__init__(ioc, output)
        self.control = control
        self.identities = {}
        self.intervals = []
        self.env["MIBS"] = ""
        for key in ("LD_PRELOAD", "LD_AUDIT", "LD_DEBUG", "LD_DEBUG_OUTPUT", "LD_LIBRARY_PATH"):
            self.env.pop(key, None)

    def missing(self, name, detail):
        self.checks.append({"name": name, "passed": None, "detail": detail})
        raise Incomplete(f"{name}: {detail}")

    def until(self, predicate, label, seconds=WAIT_SECONDS):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            time.sleep(POLL_SECONDS)
        self.missing(label, f"Required observation absent after {seconds} seconds")

    def observe(self, label, seconds, sample):
        start = time.monotonic_ns()
        end = start + int(seconds * 1e9)
        rows = []
        try:
            while time.monotonic_ns() < end:
                rows.append({"monotonic_ns": time.monotonic_ns(), "value": sample()})
                time.sleep(POLL_SECONDS)
        finally:
            finish = time.monotonic_ns()
            interval = {"name": label, "start_ns": start, "end_ns": finish,
                        "elapsed_seconds": (finish - start) / 1e9, "samples": rows}
            self.intervals.append(interval)
            write_json(self.output / "intervals.json", self.intervals)
        return interval

    def put(self, suffix, value):
        result = self.client("caput", "-w", "2", self.prefix + suffix, value)
        if result.returncode:
            self.missing("caput:" + suffix, result.returncode)

    def fields(self, *suffixes):
        values = self.ca([self.prefix + s for s in suffixes])
        return {s: values[self.prefix + s] for s in suffixes}

    @contextmanager
    def service(self, name, argv):
        with self.process(name, argv) as child:
            item = self.processes[-1]
            try:
                yield child
            finally:
                if child.poll() is None:
                    child.send_signal(signal.SIGTERM)
                    try:
                        child.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        item["service_forced"] = True
                        child.kill()
                        child.wait(timeout=5)
                        self.missing(name + ":normal-stop", "SIGTERM did not stop owned service")

    @contextmanager
    def application(self, name, folder, config):
        script = folder / "startup.cmd"
        script.write_text(self.header() +
                          f'iocshLoad("{self.ioc}/iocsh/device.cmd","CONFIG={config}")\n' +
                          'iocInit\npvxsi\nsnmp3ConfigReport\nsnmp3RuntimeReport\necho LOCAL_IOC_READY\n')
        with self.process(name, [self.binary, script]) as child:
            self.until(lambda: "LOCAL_IOC_READY" in (self.output / (name + ".log")).read_text(),
                       name + ":ioc-ready")
            maps = Path(f"/proc/{child.pid}/maps").read_text()
            (folder / "loaded-maps.txt").write_text(maps)
            libraries = sorted({line.split()[-1] for line in maps.splitlines()
                                if ".so" in line and line.split()[-1].startswith("/")})
            write_json(folder / "loaded-libraries.json", {p: digest(p) for p in libraries})
            yield child
        self.check(name + ":normal-exit", child.returncode == 0 and not self.processes[-1]["forced"])
        self.shutdown_evidence(name)

    @contextmanager
    def recorded_peer(self, name):
        folder = self.output / name
        folder.mkdir()
        module = load_peer(self.ioc / "source/tests/snmp_peer.py")
        peer = module.Pdu(0, folder)
        peer.timeline = []
        gate = threading.Lock()

        class Reply:
            def __init__(self, sock, row):
                self.sock, self.row = sock, row

            def sendto(self, data, address):
                fields = list(module.items(list(module.items(next(module.items(data))[1]))[2][1]))
                self.row.update(reply_ns=time.monotonic_ns(),
                                status=int.from_bytes(fields[1][1], "big", signed=True))
                return self.sock.sendto(data, address)

        class Handler(module.Handler):
            def handle(self):
                data, sock = self.request
                row = {"arrival_ns": time.monotonic_ns(), "reply_ns": None}
                try:
                    message = list(module.items(next(module.items(data))[1]))
                    fields = list(module.items(message[2][1]))
                    pairs = [list(module.items(v)) for _, v, _ in module.items(fields[3][1])]
                    row.update(pdu=message[2][0], oids=[peer.names.get(p[0][1], p[0][1].hex()) for p in pairs],
                               values=[int.from_bytes(p[1][1], "big", signed=True)
                                       if p[1][0] == module.INTEGER else None for p in pairs])
                    self.request = data, Reply(sock, row)
                    super().handle()
                except Exception:
                    row["error"] = traceback.format_exc()
                finally:
                    with gate:
                        peer.timeline.append(row)

        peer.RequestHandlerClass = Handler
        peer.enabled = True
        try:
            config = self.config(folder, peer.server_address[1])
            with self.application(name, folder, config) as child:
                yield peer, child
        finally:
            peer.close()
            write_json(folder / "timeline.json", peer.timeline)
            write_json(folder / "peer.json", {"errors": peer.errors, "requests": len(peer.requests),
                       "sets": sum(pdu == 0xA3 for pdu, _ in peer.requests),
                       "socket_closed": peer.fileno() == -1})
        self.check(name + ":fixture-clean", peer.fileno() == -1 and not peer.errors and
                   not any("error" in r for r in peer.timeline))

    @staticmethod
    def sets(peer):
        return [row for row in list(peer.timeline) if row.get("pdu") == 0xA3]

    def healthy(self):
        values, types = self.wait_group(lambda v: v.get("device.model") == "AP8932" and
                                       v.get("device.error.value.index") == 0)
        for field, expected in EXPECTED.items():
            self.check(field, math.isclose(values[field], expected, abs_tol=1e-9), values[field])
        for number in range(1, 25):
            field = f"ch{number:02}.state.value.index"
            self.check(field, values[field] == 2)
        return values, types

    def command(self, peer, public, raw, readback, oid, value, success=True):
        before = len(self.sets(peer))
        self.put(public, value)
        self.until(lambda: len(self.sets(peer)) > before, public + ":wire-set")
        self.until(lambda: self.fields(raw + ".PACT")[raw + ".PACT"] == "0", raw + ":settled")
        fields = self.fields(raw + ".VAL", raw + ".SEVR", readback + ".VAL")
        rows = self.sets(peer)[before:]
        self.check(public + ":set-value-order", len(rows) == 1 and rows[0]["oids"] == [oid]
                   and rows[0]["values"] == [value] and rows[0].get("status") == (0 if success else 17), rows)
        self.check(raw + ":completion-alarm", int(fields[raw + ".VAL"]) == value and
                   int(fields[raw + ".SEVR"]) == (0 if success else 3), fields)
        expected = value if success else 1
        self.until(lambda: int(self.fields(readback + ".VAL")[readback + ".VAL"]) == expected,
                   readback + ":independent-value")
        self.until(lambda: any(row.get("pdu") == 0xA0 and oid in row.get("oids", []) and
                               row["arrival_ns"] > rows[0]["reply_ns"] for row in list(peer.timeline)),
                   readback + ":independent-get")

    def commands(self):
        with self.recorded_peer("commands") as (peer, _):
            self.healthy()
            self.check("startup:no-set", not self.sets(peer))
            peer.writable.update(("rPDU2OutletSwitchedControlCommand", "sPDUMasterControlSwitch"))
            for number in range(1, 25):
                self.command(peer, f"OutletCmd{number}", f"OutletCmd{number}_", f"OutletCmdReadback{number}",
                             f"rPDU2OutletSwitchedControlCommand.{number}", 2)
            for value in (1, 3):
                self.command(peer, "OutletCmd1", "OutletCmd1_", "OutletCmdReadback1",
                             "rPDU2OutletSwitchedControlCommand.1", value)
            self.command(peer, "MasterControlCmd", "MasterControlCmd_", "MasterControlReadback",
                         "sPDUMasterControlSwitch.0", 1)
            # The fixture stores command OIDs; no outlet status or physical action follows a SET.
            self.check("commands:no-physical-confirmation", all(
                self.fields(f"OutletStatus{n}")[f"OutletStatus{n}"] == "2" for n in range(1, 25)))
            peer.set("rPDU2OutletSwitchedControlCommand", 1, 1)
            peer.writable.remove("rPDU2OutletSwitchedControlCommand")
            if self.control == "accept-rejected":
                peer.writable.add("rPDU2OutletSwitchedControlCommand")
            self.command(peer, "OutletCmd1", "OutletCmd1_", "OutletCmdReadback1",
                         "rPDU2OutletSwitchedControlCommand.1", 2, success=False)
            before = len(self.sets(peer))
            self.observe("failed-set-no-retry", QUIET_SECONDS, lambda: len(self.sets(peer)))
            self.check("failed-set:no-automatic-retry", len(self.sets(peer)) == before)
            peer.writable.add("rPDU2OutletSwitchedControlCommand")
            self.command(peer, "OutletCmd1", "OutletCmd1_", "OutletCmdReadback1",
                         "rPDU2OutletSwitchedControlCommand.1", 2)
            before = len(self.sets(peer))
            self.observe("command-idle", QUIET_SECONDS, lambda: len(self.sets(peer)))
            self.check("idle:no-unintended-set", len(self.sets(peer)) == before == 29, before)

    def scanner(self):
        with self.recorded_peer("scanner") as (peer, _):
            self.healthy()
            info = "sPDUIdentSerialNumber.0"
            phase = "rPDU2PhaseStatusCurrent.1"
            timing = "rPDU2OutletSwitchedConfigRebootDuration.1"
            self.until(lambda: sum(info in r.get("oids", []) for r in peer.timeline) >= 2,
                       "scanner:baseline-information-cycles")
            self.put("SCANNER.SCAN", "Passive")
            # Drain existing asynchronous work before defining the paused interval.
            self.until(lambda: self.fields("PhaseCurrRaw_.PACT", "SerialNo.PACT") ==
                       {"PhaseCurrRaw_.PACT": "0", "SerialNo.PACT": "0"}, "scanner:drained")
            count = self.fields("SCANNER")["SCANNER"]
            peer.set("rPDU2PhaseStatusCurrent", 1, 135)
            peer.set("sPDUIdentSerialNumber", 0, "SIM-PAUSED", 4)
            interval = self.observe("scanner-paused", PAUSE_SECONDS,
                                    lambda: self.fields("SCANNER", "PhaseCurr", "SerialNo"))
            rows = [r for r in peer.timeline if interval["start_ns"] <= r["arrival_ns"] < interval["end_ns"]]
            self.check("scanner:paused-counter-and-values", all(r["value"] ==
                       {"SCANNER": count, "PhaseCurr": "12", "SerialNo": "SIM-0"}
                       for r in interval["samples"]), interval)
            self.check("scanner:no-phase-or-info-get", not any(
                phase in r.get("oids", []) or info in r.get("oids", []) for r in rows))
            timing_rows = [r for r in rows if timing in r.get("oids", [])]
            if len(timing_rows) < 2:
                self.missing("scanner:independent-timing-scan", timing_rows)
            self.check("scanner:timing-scan-continues", len(timing_rows) >= 2)
            start = time.monotonic_ns()
            self.put("SCANNER.SCAN", "2 second")
            self.until(lambda: self.fields("PhaseCurr", "SerialNo") ==
                       {"PhaseCurr": "13.5", "SerialNo": "SIM-PAUSED"}, "scanner:resumed-values")
            self.observe("scanner-resumed", RESUME_SECONDS,
                         lambda: self.fields("SCANNER", "PhaseCurrRaw_.PACT"))
            rows = [r for r in peer.timeline if r["arrival_ns"] >= start and info in r.get("oids", [])]
            if len(rows) < 2:
                self.missing("scanner:resumed-information-cycles", rows)
            gaps = [(b["arrival_ns"] - a["arrival_ns"]) / 1e9 for a, b in zip(rows, rows[1:])]
            self.check("scanner:no-catchup-burst", min(gaps) >= INFO_SECONDS - SCAN_SECONDS, gaps)
            self.check("scanner:resumed-counter", float(self.fields("SCANNER")["SCANNER"]) > float(count))
            self.check("scanner:no-set", not self.sets(peer))

    def build_agent(self):
        source = ROOT / "tests/rewrite/NativeAgent.cpp"
        binary = self.output / "snmp3ApcAgent"
        if binary.exists():
            if self.identities.get(str(binary)) != digest(binary):
                raise Incomplete("Native agent product changed")
            return binary
        flags = shlex.split(subprocess.check_output(["net-snmp-config", "--cflags"], text=True))
        libraries = shlex.split(subprocess.check_output(["net-snmp-config", "--libs"], text=True))
        argv = ["g++", "-std=c++11", "-O2", *flags, str(source), "-lnetsnmpagent", *libraries, "-o", str(binary)]
        result = subprocess.run(argv, capture_output=True, text=True, timeout=60)
        (self.output / "agent-build.out").write_text(result.stdout)
        (self.output / "agent-build.err").write_text(result.stderr)
        write_json(self.output / "agent-build.json", {"argv": argv, "returncode": result.returncode,
                   "source_sha256": digest(source)})
        if result.returncode:
            self.missing("agent:build", result.returncode)
        self.identities[str(source)] = digest(source)
        self.identities[str(binary)] = digest(binary)
        return binary

    def export_objects(self, folder):
        module = load_peer(self.ioc / "source/tests/snmp_peer.py")
        peer = module.Pdu(0, folder)
        try:
            rows, mapping = [], {}
            for index, (oid, (kind, value)) in enumerate(peer.values.items(), 1):
                name, suffix = peer.names[oid].rsplit(".", 1)
                numeric = ".".join(map(str, peer.resolve(name) + (int(suffix),)))
                text = value.encode("ascii").hex() if isinstance(value, str) else str(value)
                rows.append(f"{numeric} {kind} {text}\n")
                mapping[numeric] = index
            path = folder / "objects.txt"
            private_file(path, "".join(rows))
            write_json(folder / "object-index.json", mapping)
            return path, mapping
        finally:
            peer.close()

    @staticmethod
    def events(path):
        return [json.loads(line) for line in path.read_text().splitlines() if line.startswith("{")]

    def v3_case(self, binary, level, fault=None):
        name = "v3-" + (fault or level)
        folder = self.output / name
        folder.mkdir()
        objects, mapping = self.export_objects(folder)
        material = {key: secrets.token_hex(12) for key in ("auth", "priv")}
        agent_config = folder / "agent.conf"
        private_file(agent_config, f"createUser {USER} SHA {material['auth']} AES {material['priv']}\n"
                     f"group apcGroup usm {USER}\nview apcView included .1.3.6.1.4.1.318\n"
                     'access apcGroup "" usm noauth prefix apcView none none\n')
        agent_port = port()
        with self.service(name + "-agent", [binary, agent_port, "4", agent_config, ENGINE_ID, objects]) as agent:
            self.until(lambda: any(e.get("event") == "agent_ready" for e in
                       self.events(self.output / (name + "-agent.log"))), name + ":agent-ready")
            maps = Path(f"/proc/{agent.pid}/maps").read_text()
            (folder / "agent-loaded-maps.txt").write_text(maps)
            libraries = sorted({line.split()[-1] for line in maps.splitlines()
                                if ".so" in line and line.split()[-1].startswith("/")})
            self.identities.update({p: digest(p) for p in libraries})
            write_json(folder / "agent-loaded-libraries.json", {p: digest(p) for p in libraries})
            proxy_args = [os.sys.executable, ROOT / "tests/rewrite/helpers/udp_fault.py", "--family", "4",
                          "--agent-port", str(agent_port), "--mode", "pass"]
            with self.service(name + "-proxy", proxy_args):
                ready = self.until(lambda: next((e for e in self.events(self.output / (name + "-proxy.log"))
                                   if e.get("event") == "fault_ready"), None), name + ":proxy-ready")
                config = self.config(folder, ready["port"], "3")
                document = json.loads(config.read_text())
                profile = document["profiles"][0]
                for key in ("authAlgorithm", "privAlgorithm", "authSecretFile", "privSecretFile"):
                    profile.pop(key, None)
                profile.update(user=USER, securityLevel=level)
                if level != "noAuthNoPriv":
                    auth = folder / "auth.txt"
                    private_file(auth, (secrets.token_hex(12) if fault == "bad-auth" else material["auth"]) + "\n")
                    profile.update(authAlgorithm="SHA", authSecretFile=str(auth))
                if level == "authPriv":
                    privacy = folder / "priv.txt"
                    private_file(privacy, (secrets.token_hex(12) if fault == "bad-privacy" else material["priv"]) + "\n")
                    profile.update(privAlgorithm="AES", privSecretFile=str(privacy))
                write_json(config, document)
                with self.application(name, folder, config):
                    if fault == "setup-failure":
                        raise SetupFailure("Deliberate setup failure after native agent, proxy and IOC start")
                    if fault:
                        interval = self.observe(name + "-rejected", QUIET_SECONDS,
                                                lambda: self.fields("PhaseCurrRaw_.SEVR", "Model.SEVR"))
                        self.check(name + ":no-healthy-publication", all(row["value"] ==
                                   {"PhaseCurrRaw_.SEVR": "3", "Model.SEVR": "3"}
                                   for row in interval["samples"]))
                    else:
                        values, types = self.healthy()
                        mappings = self.inventory["mappings"]
                        ca = self.ca([m["+channel"] for m in mappings.values()])
                        for key, item in mappings.items():
                            channel = item["+channel"]
                            kind = self.inventory["public_records"][channel[:-4]]
                            field = key if item["+type"] == "plain" else key + ".value" + (
                                ".index" if kind in ("mbbi", "bi") else "")
                            expected_type = "string" if kind == "stringin" else "double" if kind in ("ai", "calc") else "int32_t"
                            equal = ca[channel] == values[field] if expected_type == "string" else math.isclose(
                                float(ca[channel]), values[field], abs_tol=1e-9)
                            self.check(name + ":" + key, equal and types.get(field) == expected_type)
                        required = {mapping[b["oid"]] for b in self.inventory["bindings"] if b["operation"] == "get"}
                        self.until(lambda: required <= {e["index"] for e in
                                   self.events(self.output / (name + "-agent.log"))
                                   if e.get("event") == "agent_handler" and e.get("mode") == 160},
                                   name + ":all-get-bindings")
        events = self.events(self.output / (name + "-agent.log"))
        handled = [e for e in events if e.get("event") == "agent_handler"]
        packets = self.events(self.output / (name + "-proxy.log"))
        applications = [e for e in packets if e.get("event") == "fault_request" and not e.get("discovery")]
        if not applications:
            self.missing(name + ":wire-exposure", packets[-5:])
        self.check(name + ":wire-version", all(e.get("version") == 3 for e in applications))
        if level == "authPriv":
            self.check(name + ":encrypted", all(e.get("encrypted") for e in applications))
        self.check(name + ":handler-security", not handled if fault else bool(handled) and all(
            e["version"] == 3 and e["security_level"] == LEVELS[level] and e["mode"] == 160 for e in handled))
        self.check(name + ":agent-stopped", agent.returncode == 0 and
                   any(e.get("event") == "agent_stopped" for e in events))

    def v3(self):
        binary = self.build_agent()
        for level in LEVELS:
            self.v3_case(binary, level)
        for fault in ("bad-auth", "bad-privacy"):
            self.v3_case(binary, "authPriv", fault)

    def cleanup(self):
        try:
            with self.recorded_peer("cleanup"):
                self.healthy()
                raise SetupFailure("Deliberate setup failure after real resources start")
        except SetupFailure:
            pass
        self.check("cleanup:owned-processes-stopped", all(p.get("stopped") and not p.get("forced")
                   and p["returncode"] == 0 for p in self.processes if p["name"] != "repeater"))
        self.shutdown_evidence("cleanup")
        try:
            self.v3_case(self.build_agent(), "authPriv", "setup-failure")
        except SetupFailure:
            pass
        self.check("cleanup:native-agent-proxy-and-ioc-stopped", all(
            p.get("stopped") and not p.get("forced") and not p.get("service_forced") and
            p["returncode"] == 0 for p in self.processes if p["name"] != "repeater"))
        self.shutdown_evidence("v3-setup-failure")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ioc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", choices=("all", *CASES), default="all")
    parser.add_argument("--control", choices=("none", "accept-rejected"), default="none")
    args = parser.parse_args()
    if args.control != "none" and args.case not in ("all", "commands"):
        parser.error("--control accept-rejected requires --case commands or all")
    ioc, output = args.ioc.resolve(), args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / "work"):
        parser.error("--output must be a new directory under this checkout's work/")
    output.mkdir(parents=True, mode=0o700)
    runner, error, code = None, None, 0
    started = time.time_ns()
    try:
        runner = Operations(ioc, output, args.control)
        for path, expected in runner.build["products"].items():
            if digest(path) != expected:
                raise Incomplete("Build product changed: " + path)
        for name, expected in runner.build["source_hashes"].items():
            if digest(ioc / "source" / name) != expected:
                raise Incomplete("Exported APC source changed: " + name)
        runner.identities.update(runner.build["products"])
        runner.identities.update({str(ioc / "source" / name): expected
                                  for name, expected in runner.build["source_hashes"].items()})
        paths = [Path(__file__), ROOT / "tests/rewrite/test_apcpdu.py", ROOT / "tests/rewrite/build_apcpdu.py",
                 ROOT / "tests/rewrite/helpers/udp_fault.py", ROOT / "bin" / ARCH / "snmp3Worker",
                 ROOT / "lib" / ARCH / "libsnmp3Native.so", ioc / "build.json"]
        runner.identities.update({str(p): digest(p) for p in paths})
        write_json(output / "identities.json", runner.identities)
        with runner.process("repeater", [runner.base / "bin" / ARCH / "caRepeater"]):
            for case in CASES if args.case == "all" else (args.case,):
                getattr(runner, case)()
    except BaseException as exc:
        error = traceback.format_exc()
        code = 130 if isinstance(exc, KeyboardInterrupt) else 1 if isinstance(exc, AssertionError) else 2
        (output / "error.txt").write_text(error)
        print(error, flush=True)
    finally:
        if runner:
            for path, expected in runner.identities.items():
                if not Path(path).is_file() or digest(path) != expected:
                    error = (error or "") + "\nIdentity changed: " + path
                    code = code or 2
            write_json(output / "identities.json", runner.identities)
            if any(check["passed"] is False for check in runner.checks):
                code = 1
            if code == 0 and any(not p.get("stopped") or p.get("forced") or p.get("service_forced") or
                                 (p["name"] != "repeater" and p.get("returncode") != 0)
                                 for p in runner.processes):
                error = "Owned process did not stop normally; see processes.json"
                code = 2
        result = {"case": args.case, "control": args.control, "passed": code == 0,
                  "status": "PASS" if code == 0 else "INCOMPLETE" if code == 2 else "FAIL",
                  "returncode": code, "checks": runner.checks if runner else [],
                  "processes": runner.processes if runner else [], "error": error,
                  "start_utc_ns": started, "end_utc_ns": time.time_ns(),
                  "limits": ["Loopback AP8932 only", "No physical command confirmation or hardware",
                             "No legacy comparator, declared binding groups, reordered replies or optional variants",
                             "SNMPv3 SHA/AES only", "M7 remains open"]}
        write_json(output / "result.json", result)
    print(f"Evidence: {output}; checks: {len(result['checks'])}; result: {result['status']}")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
