#!/usr/bin/env python3
"""Verify the prepared APC candidate through real grouped GET and CA/PVA paths.

The candidate's own hash-checking launcher, startup, DB, QSRV2 mapping, binary,
driver and worker execute unchanged. The shipped APC agent and fixture values
provide the external UDP boundary. Each case starts fresh and retains native
command outcomes, complete client output, request/response times and cleanup.

Baseline covers both units, every input, every mapped field and every declared
wire group. Held replies cover completion gating and unit isolation. Outage
covers cold validity, full public alarm fanout, probe-only traffic and same-value
recovery. Missing required observations are Incomplete (2), observed violations
Failed (1), and SIGINT retains 130. No equipment, deployment, legacy comparison,
v3 wire, expired grouped responses, sustained performance, transfer/replay or
whole-device atomic-sample claim is made. Those safeguards are inapplicable to
this bounded local contract. Complete snapshots retain the actual population;
configured periods are never reported as measured rates.

Partial requests are allowed by the bounded group hold. Every request must
remain within one declared group, without duplicate or unknown members. Member
coverage and whole-request observations are reported separately. A retained
wire replay evaluates captured observations only, not a new IOC execution.
"""

import argparse
from contextlib import contextmanager
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import threading
import time
import traceback

from build_apcpdu import ARCH, ROOT, digest, load_peer, write_json
from test_apcpdu import port
from test_apcpdu_operations import Incomplete, SetupFailure

sys.dont_write_bytecode = True
WAIT_SECONDS = 45
INITIAL_SECONDS = 90
POLL_SECONDS = 0.2
HOLD_SECONDS = 0.8
PROBE_SECONDS = 11
SPLIT_TRIALS = 120
CASES = ("baseline", "held", "split", "outage", "cleanup")
CONTROLS = ("none", "wrong-value", "no-pva")


class Interrupted(KeyboardInterrupt):
    def __init__(self, signum):
        self.signum = signum


def interrupt(signum, frame):
    raise Interrupted(signum)


def decode_delta(text):
    values, kinds = {}, {}
    for line in text.splitlines():
        m = re.fullmatch(r"\s*(\S+)\s+(\S+) = (.*)", line)
        if not m:
            continue
        key, kind, value = m.groups()
        if key in values:
            raise ValueError("Duplicate PVA field: " + key)
        if kind == "string":
            value = json.loads(value)
        elif kind in ("double", "float"):
            value = float(value)
        elif re.fullmatch(r"u?int\d+_t", kind):
            value = int(value)
        elif kind.endswith("[]"):
            array = re.fullmatch(r"\{(\d+)\}(\[.*\])", value)
            if not array:
                raise Incomplete("Unrecognized or truncated PVA array: " + key)
            value = json.loads(array[2])
            if len(value) != int(array[1]):
                raise Incomplete("PVA array count differs from observed elements")
        else:
            continue
        values[key], kinds[key] = value, kind
    return values, kinds


class Grouped:
    def __init__(self, candidate, output, control):
        self.candidate, self.output, self.control = candidate, output, control
        self.checks, self.processes, self.clients = [], [], 0
        self.client_processes, self.tool_hashes = [], {}
        self.split_gate = threading.Lock()
        self.split_armed, self.split_selected = False, None
        self.info = json.loads((candidate / "ioc/docs/ap8932-inventory.json").read_text())
        self.handoff = json.loads((candidate / "handoff.json").read_text())
        self.config = json.loads((candidate / "local/configuration.json").read_text())
        self.groups = json.loads((candidate / "groups.json").read_text())
        self.manifest = json.loads((candidate / "manifest.json").read_text())
        self.entries = self.info["bindings"]
        self.units = self.handoff["units"]
        self.mapping = next(iter(json.loads((candidate / "ioc/db/ap8932_group.json").read_text()).values()))
        self.peer_module = load_peer(candidate / "ioc/tests/snmp_peer.py")
        sys.path.insert(0, str(candidate / "ioc/tests"))
        spec = importlib.util.spec_from_file_location("apc_candidate_fixture", candidate / "ioc/tests/test_ap8932.py")
        self.fixture = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.fixture)
        self.base = Path(self.fixture.Runner.read_release("EPICS_BASE"))
        self.pvxs = Path(self.fixture.Runner.read_release("PVXS"))
        self.ca_port, self.pva_port, repeater = port(), port(), port()
        if len({self.ca_port, self.pva_port, repeater}) != 3:
            raise Incomplete("Port allocation collision")
        self.env = {k: v for k, v in os.environ.items()
                    if not k.startswith(("EPICS_CA", "EPICS_PVA", "LD_"))}
        self.env.update({"PYTHONDONTWRITEBYTECODE": "1", "EPICS_CA_SERVER_PORT": str(self.ca_port),
                         "EPICS_CA_REPEATER_PORT": str(repeater), "EPICS_CA_ADDR_LIST": f"127.0.0.1:{self.ca_port}",
                         "EPICS_CA_AUTO_ADDR_LIST": "NO", "EPICS_CA_NAME_SERVERS": "",
                         "EPICS_PVA_ADDR_LIST": f"127.0.0.1:{self.pva_port}", "EPICS_PVA_AUTO_ADDR_LIST": "NO",
                         "EPICS_PVA_NAME_SERVERS": "", "MIBS": ""})
        write_json(output / "environment.json", {k: v for k, v in self.env.items() if k.startswith("EPICS_")})
        self.blackhole = None
        if control == "no-pva":
            self.blackhole = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.blackhole.bind(("127.0.0.1", 0))
            self.blackhole.setblocking(False)

    def check(self, name, passed, detail=None):
        self.checks.append({"name": name, "passed": bool(passed), "detail": detail})
        print(f"[ {'PASS' if passed else 'FAIL'} ] {name}", flush=True)
        if not passed:
            raise AssertionError(f"{name}: {detail}")

    def until(self, predicate, name, seconds=WAIT_SECONDS):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            result = predicate()
            if result:
                return result
            time.sleep(POLL_SECONDS)
        raise Incomplete("Required observation unavailable: " + name)

    def client(self, name, *args):
        root = self.pvxs if name.startswith("pvx") else self.base
        if name == "grouped-ca":
            helper = Path(__file__).with_name("apcpdu_grouped_ca.py")
            argv = [sys.executable, "-B", str(helper), "--libca", str(self.base / "lib" / ARCH / "libca.so"), *map(str, args)]
            self.tool_hashes[str(helper)] = digest(helper)
        else:
            argv = [str(root / "bin" / ARCH / name), *map(str, args)]
        self.clients += 1
        path = self.folder / f"client-{self.clients:05}"
        self.tool_hashes.setdefault(argv[0], digest(argv[0]))
        receipt = {"argv": argv, "start_ns": time.monotonic_ns()}
        with path.with_suffix(".out").open("wb") as out, path.with_suffix(".err").open("wb") as err:
            child = subprocess.Popen(argv, env=self.env, stdout=out, stderr=err)
            receipt["pid"] = child.pid
            self.client_processes.append(receipt)
            try:
                child.wait(timeout=12)
            except BaseException as e:
                receipt["execution_error"] = type(e).__name__
                if child.poll() is None:
                    child.terminate()
                    try:
                        child.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        child.kill()
                        child.wait(timeout=3)
                if isinstance(e, subprocess.TimeoutExpired):
                    raise Incomplete("Client process timed out: " + name) from e
                raise
            finally:
                receipt.update(returncode=child.returncode, stopped=child.poll() is not None,
                               end_ns=time.monotonic_ns())
                write_json(path.with_suffix(".json"), receipt)
        return subprocess.CompletedProcess(argv, child.returncode, path.with_suffix(".out").read_bytes(),
                                           path.with_suffix(".err").read_bytes())

    def ca(self, names, strings=False):
        values = {}
        for i in range(0, len(names), 45):
            chunk = names[i:i + 45]
            r = self.client("caget", "-w", "2", "-n", *(["-S"] if strings else []), "-F", "\t", *chunk)
            if r.returncode:
                raise Incomplete("CA query failed; raw output retained")
            for line in r.stdout.decode().splitlines():
                if "\t" in line:
                    key, value = line.split("\t", 1)
                    if key in values:
                        raise Incomplete("Duplicate CA record")
                    values[key] = value
            if not set(chunk).issubset(values):
                raise Incomplete("CA query omitted required records")
        return values

    def fields(self, unit, *suffixes):
        prefix = "APCLOCAL:" + unit + ":"
        got = self.ca([prefix + x for x in suffixes])
        return {x: got[prefix + x] for x in suffixes}

    def put(self, unit, suffix, value):
        r = self.client("caput", "-w", "2", f"APCLOCAL:{unit}:{suffix}", value)
        if r.returncode:
            raise Incomplete("CA put failed")

    def group(self, unit):
        if self.control == "no-pva":
            env = self.env.copy()
            self.env["EPICS_PVA_ADDR_LIST"] = "127.0.0.1:" + str(self.blackhole.getsockname()[1])
        try:
            r = self.client("pvxget", "-w", "2", "-#", "0", "-F", "delta", f"APCLOCAL:{unit}:group")
        finally:
            if self.control == "no-pva":
                self.env = env
        if r.returncode:
            raise Incomplete("Required PVA group unavailable")
        return decode_delta(r.stdout.decode())

    def identify(self):
        for relative, expected in self.manifest["files"].items():
            self.check("identity:" + relative, digest(self.candidate / relative) == expected)
        for path, expected in self.manifest["externalDependencies"].items():
            self.check("dependency:" + Path(path).name, digest(path) == expected)
        self.check("scope:two-loopback-get-units", len(self.config["endpoints"]) == 2
                   and all(x["address"] == "127.0.0.1" for x in self.config["endpoints"])
                   and all(x["operation"] == "get" for x in self.config["bindings"]))
        write_json(self.output / "candidate-manifest.json", self.manifest)

    def make_peer(self, endpoint):
        module, folder, owner = self.peer_module, self.folder, self
        unit = endpoint["id"]

        class Peer(module.Pdu):
            def server_bind(self):
                self.server_address = ("127.0.0.1", endpoint["port"])
                super().server_bind()

        peer = Peer(unit, folder)
        peer.events, peer.held, peer.hold = [], [], False
        peer.split_delivered = set()
        measurement = next(g for g in self.groups if g["unit"] == unit and g["tier"] == "meas")
        measurement_oids = {e["mib"] + "." + e["oid"].rsplit(".", 1)[1]
                           for e in self.entries if e["record"] in measurement["members"]}
        peer.event_lock = threading.Lock()
        for entry in self.entries:
            kind, value = self.fixture.fixture_value(entry, self.info["layout"])
            if entry["kind"] in ("text", "text40"):
                value = unit + ":" + value
            if self.control == "wrong-value" and unit == self.units[0] and entry["record"] == "Phase1Current":
                value = 999
            peer.set(entry["mib"], int(entry["oid"].rsplit(".", 1)[1]), value, kind)

        def metadata(data):
            outer = list(module.items(next(module.items(data))[1]))
            fields = list(module.items(outer[2][1]))
            pairs = [list(module.items(v)) for _, v, _ in module.items(fields[3][1])]
            names = [peer.names.get(p[0][1], p[0][1].hex()) for p in pairs]
            values = {name: int.from_bytes(p[1][1], "big", signed=True)
                      for name, p in zip(names, pairs) if p[1][0] in (module.INTEGER, module.GAUGE)}
            return {"pdu": outer[2][0], "request_id": int.from_bytes(fields[0][1], "big", signed=True),
                    "oids": names, "integer_values": values, "phase_raw": values.get("rPDU2PhaseStatusCurrent.1")}

        def event(kind, data, address):
            row = {"event": kind, "ns": time.monotonic_ns(), "client_port": address[1], **metadata(data)}
            with peer.event_lock:
                peer.events.append(row)
                with (folder / f"timeline-{unit}.jsonl").open("a") as stream:
                    stream.write(json.dumps(row) + "\n")
            return row

        class Reply:
            def __init__(self, sock):
                self.sock = sock

            def sendto(self, data, address):
                meta = event("response", data, address)
                members = set(meta["oids"])
                with owner.split_gate:
                    if owner.split_armed and members and members < measurement_oids:
                        if peer.split_delivered and owner.split_selected is None:
                            peer.held.append((self.sock, data, address, meta))
                            owner.split_selected = {"unit": unit, "group": measurement["group"], "response": meta,
                                                    "delivered_oids": sorted(peer.split_delivered)}
                            return len(data)
                        sent = self.sock.sendto(data, address)
                        event("delivery", data, address)
                        peer.split_delivered.update(members)
                        return sent
                if peer.hold and len(meta["oids"]) > 1:
                    with peer.event_lock:
                        peer.held.append((self.sock, data, address, meta))
                    return len(data)
                sent = self.sock.sendto(data, address)
                event("delivery", data, address)
                return sent

        class Handler(module.Handler):
            def handle(self):
                data, sock = self.request
                event("request", data, self.client_address)
                self.request = data, Reply(sock)
                super().handle()

        peer.RequestHandlerClass = Handler
        peer.deliver = lambda packet: (packet[0].sendto(packet[1], packet[2]), event("delivery", packet[1], packet[2]))
        return peer

    @contextmanager
    def application(self, name):
        self.folder = self.output / name
        self.folder.mkdir()
        peers, child, repeater = {}, None, None
        streams = []
        case = {"name": name, "start_ns": time.monotonic_ns()}
        self.processes.append(case)
        try:
            for endpoint in self.config["endpoints"]:
                peers[endpoint["id"]] = self.make_peer(endpoint)
                peers[endpoint["id"]].enabled = name not in ("baseline", "cleanup")
            for filename in ("repeater.log", "ioc.log", "ioc.stderr"):
                streams.append((self.folder / filename).open("w"))
            repeater = subprocess.Popen([str(self.base / "bin" / ARCH / "caRepeater")], env=self.env,
                                        stdout=streams[0], stderr=subprocess.STDOUT, start_new_session=True)
            case["repeater_pid"] = repeater.pid
            argv = [sys.executable, str(self.candidate / "start.py"), "--ca-port", str(self.ca_port),
                    "--pva-port", str(self.pva_port)]
            write_json(self.folder / "command.json", {"argv": argv, "cwd": str(self.folder)})
            child = subprocess.Popen(argv, cwd=self.folder, env=self.env, stdin=subprocess.PIPE,
                                     stdout=streams[1], stderr=streams[2], text=True, start_new_session=True)
            case["ioc_pid"] = child.pid
            self.until(lambda: "AP8932_GROUPED_READY" in (self.folder / "ioc.log").read_text()
                       or child.poll() is not None, "IOC startup")
            self.check(name + ":actual-startup", child.poll() is None, child.returncode)
            case["initialized"] = True
            child.stdin.write("dbl\nsnmp3RuntimeReport\necho GROUPED_CENSUS_DONE\n")
            child.stdin.flush()
            self.until(lambda: "GROUPED_CENSUS_DONE" in (self.folder / "ioc.log").read_text(), "record census")
            records = re.findall(r'^record\(\w+,\s*"([^"]+)"', (self.candidate / "ioc/db/ap8932.db").read_text(), re.M)
            expected = {r.replace("$(P)", "APCLOCAL:").replace("$(R)", u + ":") for u in self.units for r in records}
            actual = set(re.findall(r"^APCLOCAL:\S+$", (self.folder / "ioc.log").read_text(), re.M))
            self.check(name + ":actual-record-census", bool(expected) and actual == expected,
                       {"expected": len(expected), "actual": len(actual), "missing": sorted(expected - actual)[:5]})
            maps = Path(f"/proc/{child.pid}/maps").read_text()
            (self.folder / "loaded-maps.txt").write_text(maps)
            libs = {s.split()[-1] for s in maps.splitlines() if ".so" in s and s.split()[-1].startswith("/")}
            write_json(self.folder / "loaded-libraries.json", {s: digest(s) for s in sorted(libs)})
            yield peers, child
        finally:
            forced = False
            if child is not None:
                if child.poll() is None:
                    try:
                        child.communicate("snmp3RuntimeReport\nexit\n", timeout=12)
                    except subprocess.TimeoutExpired:
                        forced = True
                        os.killpg(child.pid, signal.SIGKILL)
                        child.wait(timeout=5)
                case.update(ioc_returncode=child.returncode, forced=forced, stopped=child.poll() is not None)
            if repeater is not None:
                repeater.terminate()
                try:
                    repeater.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    repeater.kill()
                    repeater.wait(timeout=5)
                case.update(repeater_returncode=repeater.returncode, repeater_stopped=repeater.poll() is not None)
            for unit, peer in peers.items():
                peer.close()
                write_json(self.folder / f"peer-{unit}.json", {"errors": peer.errors, "requests": peer.requests,
                           "events": peer.events, "socket_closed": peer.fileno() == -1})
            for stream in streams:
                stream.close()
            if child is not None and case.get("initialized"):
                text = (self.folder / "ioc.log").read_text()
                case["worker_terminal"] = ("state=Stopped" in text and "launches=1 reaps=1 forced=0" in text
                                            and "created=1 exited=1 joined=1" in text and "queued=0" in text)
            case["end_ns"] = time.monotonic_ns()
            write_json(self.output / "processes.json", self.processes)
        self.check(name + ":normal-exit", child.returncode == 0 and not forced)
        log = (self.folder / "ioc.log").read_text()
        self.check(name + ":worker-reaped", "launches=1 reaps=1" in log and "queued=0" in log)

    def healthy(self, label="startup"):
        for unit in self.units:
            self.until(lambda: self.fields(unit, "Online", "Outlet01State.SEVR", "Phase1Current.SEVR") ==
                       {"Online": "1", "Outlet01State.SEVR": "0", "Phase1Current.SEVR": "0"},
                       unit + ":healthy state and measurement")
            self.until(lambda: float(self.fields(unit, "_PollSlowDone")["_PollSlowDone"]) > 0,
                       unit + ":first slow cycle")
            prefix = "APCLOCAL:" + unit + ":"
            expected = {}
            for e in self.entries:
                _, raw = self.fixture.fixture_value(e, self.info["layout"])
                expected[prefix + e["record"] + ".SEVR"] = "3" if e["kind"] == "scaled" and raw < 0 else "0"
            samples = []

            def ready():
                actual = self.ca(list(expected))
                samples.append({"ns": time.monotonic_ns(), "severities": actual})
                write_json(self.folder / (unit + "-" + label + "-readiness.json"), samples)
                return actual == expected

            self.until(ready, unit + ":all input validity after cold recovery", INITIAL_SECONDS)

    def enable(self, peers):
        for peer in peers.values():
            peer.enabled = True
        self.healthy()

    def pause(self):
        for unit in self.units:
            for head in ("_PollState", "_PollMeas", "_PollSlowDivider"):
                self.put(unit, head + ".SCAN", "Passive")
        self.until(lambda: all(all(float(v) == 0 for v in self.fields(u, "_BusyState", "_BusyMeas", "_BusySlow").values())
                              for u in self.units), "all cycles idle")

    def wire(self, peers=None, replay=None):
        for unit in self.units:
            if replay is None:
                events, errors = peers[unit].events, peers[unit].errors
            else:
                saved = json.loads((replay / ("peer-" + unit + ".json")).read_text())
                events, errors = saved["events"], saved["errors"]
            requests = [e for e in events if e["event"] == "request"]
            if not requests:
                raise Incomplete(unit + ": required wire observations are absent")
            allowed, observations, invalid = {}, {}, []
            for g in [g for g in self.groups if g["unit"] == unit]:
                allowed[g["group"]] = {e["mib"] + "." + e["oid"].rsplit(".", 1)[1]
                                       for e in self.entries if e["record"] in g["members"]}
                observations[g["group"]] = {"full": [], "partial": [], "members": set()}
            canary = self.entries[0]["mib"] + "." + self.entries[0]["oid"].rsplit(".", 1)[1]
            for request in requests:
                members = set(request["oids"])
                if request["oids"] == [canary]:
                    continue
                owners = [name for name, group in allowed.items() if members and members <= group]
                if len(owners) != 1 or len(members) != len(request["oids"]):
                    invalid.append(request)
                    continue
                group = owners[0]
                row = observations[group]
                row["members"].update(members)
                row["full" if members == allowed[group] else "partial"].append(request)
            summary = {name: {"full_requests": row["full"], "partial_requests": row["partial"],
                             "observed_members": sorted(row["members"]),
                             "missing_members": sorted(allowed[name] - row["members"])}
                       for name, row in observations.items()}
            write_json(self.folder / (unit + "-wire-summary.json"),
                       {"groups": summary, "invalid_requests": invalid, "requests": len(requests)})
            self.check(unit + ":GET-only", all(e["pdu"] == 0xA0 for e in requests))
            self.check(unit + ":peer-no-errors", not errors, errors)
            self.check(unit + ":no-unknown-duplicate-or-mixed-members", not invalid, invalid)
            self.check(unit + ":multi-varbind-observed", any(len(e["oids"]) > 1 for e in requests))
            for name, row in summary.items():
                if row["missing_members"]:
                    raise Incomplete(name + ": required member observations are absent")
                self.check(name + ":member-coverage", True,
                           {"full": len(row["full_requests"]), "partial": len(row["partial_requests"]),
                            "missing": row["missing_members"]})

    def replay_wire(self, folder, expected_hash):
        folder = folder.resolve()
        manifest = folder.parent / "input-manifest.json"
        if expected_hash is None or digest(manifest) != expected_hash:
            raise Incomplete("Retained-input manifest identity is unavailable or changed")
        document = json.loads(manifest.read_text())
        actual = {str(p.relative_to(folder)) for p in folder.rglob("*") if p.is_file()}
        if actual != set(document["files"]):
            raise Incomplete("Retained-input membership differs from the frozen manifest")
        for name, expected in document["files"].items():
            path = (folder / name).resolve()
            if not path.is_relative_to(folder) or path.stat().st_size != expected["bytes"] or digest(path) != expected["sha256"]:
                raise Incomplete("Retained-input member differs from the frozen manifest")
        for saved, current in (("groups.json", "groups.json"), ("inventory.json", "ioc/docs/ap8932-inventory.json")):
            if digest(folder / saved) != digest(self.candidate / current):
                raise Incomplete("Retained wire data and candidate inventory are different")
        self.folder = self.output / "wire-replay"
        self.folder.mkdir()
        self.wire(replay=folder)

    def baseline(self):
        with self.application("baseline") as (peers, child):
            for unit in self.units:
                raw = self.entries[0]["rawRecord"] + ".SEVR"
                self.check(unit + ":cold-invalid", self.fields(unit, raw)[raw] == "3")
            self.enable(peers)
            self.pause()
            for unit in self.units:
                prefix = "APCLOCAL:" + unit + ":"
                values = self.ca([prefix + e["record"] for e in self.entries], strings=True)
                severities = self.ca([prefix + e["record"] + ".SEVR" for e in self.entries])
                write_json(self.folder / (unit + "-ca.json"), {"values": values, "severities": severities})
                for entry in self.entries:
                    _, raw = self.fixture.fixture_value(entry, self.info["layout"])
                    name, kind = entry["record"], entry["kind"]
                    invalid = kind == "scaled" and raw < 0
                    self.check(unit + ":" + name + ":alarm", severities[prefix + name + ".SEVR"] == ("3" if invalid else "0"))
                    if kind in ("text", "text40"):
                        expected = unit + ":" + raw
                        have = self.ca([prefix + name + ".VAL$"], strings=True)[prefix + name + ".VAL$"] if kind == "text" else values[prefix + name]
                        same = have == expected
                    elif kind == "enum":
                        expected = list(entry["states"]).index(str(raw))
                        same = float(values[prefix + name]) == expected
                    elif invalid:
                        continue
                    else:
                        expected = raw * entry.get("scale", 1)
                        same = math.isclose(float(values[prefix + name]), expected, rel_tol=1e-6, abs_tol=1e-6)
                    self.check(unit + ":" + name + ":value", same, {"expected": expected, "actual": values[prefix + name]})
                pva, kinds = self.group(unit)
                write_json(self.folder / (unit + "-pva.json"), {"values": pva, "types": kinds})
                channels = {m["+channel"].replace("$(P)", "APCLOCAL:").replace("$(R)", unit + ":").removesuffix(".VAL")
                            for k, m in self.mapping.items() if not k.startswith("+")}
                mapped_ca = self.ca(sorted(channels), strings=True)
                mapped_sevr = self.ca(sorted({r.split(".", 1)[0] + ".SEVR" for r in channels}))
                write_json(self.folder / (unit + "-mapped-ca.json"), {"values": mapped_ca, "severities": mapped_sevr})
                for key, mapping in self.mapping.items():
                    if key.startswith("+"):
                        continue
                    channel = mapping["+channel"].replace("$(P)", "APCLOCAL:").replace("$(R)", unit + ":")
                    record = channel.removesuffix(".VAL")
                    ca_value = mapped_ca[record]
                    plain = mapping.get("+type") == "plain"
                    field = key if plain else key + ".value"
                    if field not in pva and field + ".index" in pva:
                        field += ".index"
                    self.check(unit + ":" + key + ":PVA-present", field in pva)
                    expected = ca_value
                    if kinds[field].endswith("[]"):
                        octets = pva[field]
                        self.check(unit + ":" + key + ":terminating-NUL", bool(octets) and octets[-1] == 0)
                        same = bytes(x & 255 for x in octets[:-1]).decode("ascii") == ca_value
                    elif isinstance(pva[field], str):
                        same = pva[field] == expected
                    else:
                        ca_number = float(ca_value)
                        same = (math.isnan(ca_number) and math.isnan(pva[field])) or math.isclose(ca_number, pva[field], rel_tol=1e-6, abs_tol=1e-6)
                    self.check(unit + ":" + key + ":CA-PVA", same, {"ca": ca_value, "pva": pva[field]})
                    if plain:
                        continue
                    alarm = key + ".alarm.severity"
                    base_record = record.split(".", 1)[0]
                    ca_alarm = mapped_sevr[base_record + ".SEVR"]
                    self.check(unit + ":" + key + ":alarm-PVA", pva.get(alarm) == int(ca_alarm))
                    stamp = [pva.get(key + ".timeStamp." + s) for s in ("secondsPastEpoch", "nanoseconds")]
                    self.check(unit + ":" + key + ":timestamp-present", all(isinstance(x, int) for x in stamp)
                               and stamp[0] > 0 and 0 <= stamp[1] < 1000000000)
                    entry = next((e for e in self.entries if prefix + e["record"] == base_record), None)
                    if entry and "egu" in entry:
                        self.check(unit + ":" + key + ":units", pva.get(key + ".display.units") == entry["egu"])
                    if entry and entry["kind"] == "enum":
                        self.check(unit + ":" + key + ":enum-choices", pva.get(key + ".value.choices") == list(entry["states"].values()))
                target = next(k for k, m in self.mapping.items() if not k.startswith("+")
                              and m["+channel"].endswith("Phase1Current.VAL"))
                result = self.client("pvxput", "-w", "2", f"APCLOCAL:{unit}:group", target + ".value=99")
                refusal = result.stderr.decode().lower()
                self.check(unit + ":PVA-write-refused", result.returncode != 0 and "timeout" not in refusal
                           and any(s in refusal for s in ("writ", "read-only", "not permitted", "not allowed")), refusal)
                unchanged, _ = self.group(unit)
                self.check(unit + ":refused-write-value-unchanged", unchanged[target + ".value"] == pva[target + ".value"])
            self.wire(peers)

    def held(self):
        with self.application("held") as (peers, child):
            self.enable(peers)
            self.pause()
            unit, other = self.units
            peer = peers[unit]
            before = self.fields(unit, "_PollState", "_PollStateDone")
            first, following = [g for g in self.groups if g["unit"] == unit and g["tier"] == "state"]
            counters = (first["counter"], following["counter"])
            prior = self.fields(unit, *counters)
            before_pva, _ = self.group(unit)
            changed = {}
            for entry in self.entries:
                if entry["record"] not in first["members"]:
                    continue
                kind, value = self.fixture.fixture_value(entry, self.info["layout"])
                if entry["kind"] == "enum":
                    value = next(int(s) for s in entry["states"] if int(s) != value)
                else:
                    value += 1
                changed[entry["record"]] = value
                peer.set(entry["mib"], int(entry["oid"].rsplit(".", 1)[1]), value, kind)
            peer.hold = True
            self.put(unit, "_PollState.PROC", 1)
            self.until(lambda: peer.held, "held grouped response")
            if len(peer.held) != 1:
                raise Incomplete("More than one group held before observation")
            packet = peer.held[0]
            entry = next(e for e in self.entries if e["mib"] + "." + e["oid"].rsplit(".", 1)[1] in packet[3]["oids"])
            key = next(k for k, m in self.mapping.items() if not k.startswith("+")
                       and m["+channel"].endswith(entry["record"] + ".VAL"))
            value_field = key + (".value.index" if entry["kind"] == "enum" else ".value")
            prior_other = self.fields(other, entry["record"])[entry["record"]]
            expected = (list(entry["states"]).index(str(changed[entry["record"]])) if entry["kind"] == "enum"
                        else changed[entry["record"]] * entry.get("scale", 1))
            write_json(self.folder / "held-exposure.json", {"ns": time.monotonic_ns(), "unit": unit,
                       "response": packet[3], "record": entry["record"], "expected_value": expected,
                       "prior_counters": prior})
            held_pva, _ = self.group(unit)
            self.check("held:undelivered-value-not-published", held_pva[value_field] == before_pva[value_field])
            start = time.monotonic_ns()
            samples = []
            while (time.monotonic_ns() - start) / 1e9 < HOLD_SECONDS:
                samples.append(self.fields(unit, "_BusyState", "_PollState", "_PollStateDone", *counters))
                time.sleep(POLL_SECONDS)
            write_json(self.folder / "held-samples.json", {"start_ns": start, "end_ns": time.monotonic_ns(), "samples": samples})
            self.check("held:cycle-not-completed", all(x["_BusyState"] == "1" and x["_PollStateDone"] == before["_PollStateDone"] for x in samples))
            self.check("held:next-group-not-issued", len(peer.held) == 1)
            self.check("held:incomplete-group-does-not-advance", all(
                0 <= float(s[first["counter"]]) < first["size"]
                and s[following["counter"]] == prior[following["counter"]] for s in samples))
            poll = samples[-1]["_PollState"]
            self.put(unit, "_PollState.PROC", 1)
            self.check("held:busy-cycle-suppressed", self.fields(unit, "_PollState")["_PollState"] == poll)
            other_before = self.fields(other, "_PollStateDone")["_PollStateDone"]
            self.put(other, "_PollState.PROC", 1)
            self.check("held:other-unit-remains-valid", self.fields(other, "Online", "Phase1Current.SEVR") ==
                       {"Online": "1", "Phase1Current.SEVR": "0"})
            peer.hold = False
            peer.deliver(peer.held[0])
            self.until(lambda: self.fields(unit, "_PollStateDone")["_PollStateDone"] != before["_PollStateDone"], "held cycle resumes")
            self.check("held:delivery-completes-chain", self.fields(unit, "_BusyState")["_BusyState"] == "0")
            completed = self.fields(unit, *counters)
            self.check("held:every-member-counted-once", completed == {first["counter"]: str(first["size"]),
                       following["counter"]: str(following["size"])}, completed)
            after_pva, _ = self.group(unit)
            write_json(self.folder / "held-pva.json", {"field": key, "before": before_pva, "held": held_pva, "after": after_pva})
            self.check("held:delivered-value-published", math.isclose(after_pva[value_field], expected))
            stamp = lambda v: (v[key + ".timeStamp.secondsPastEpoch"], v[key + ".timeStamp.nanoseconds"])
            self.check("held:timestamp-follows-delivery", stamp(held_pva) == stamp(before_pva)
                       and stamp(after_pva) > stamp(before_pva))
            self.check("held:other-unit-value-isolated", self.fields(other, entry["record"])[entry["record"]] == prior_other)
            self.until(lambda: self.fields(other, "_PollStateDone")["_PollStateDone"] != other_before, "other unit completes")
            self.check("held:other-unit-completes", True)
            self.wire(peers)

    def ca_probe(self, mode, names):
        result = self.client("grouped-ca", mode, *names)
        if result.returncode:
            raise Incomplete("Native CA probe did not provide required observations")
        return json.loads(result.stdout)["observations"]

    def runtime_count(self, child, label):
        marker = "GROUPED_RUNTIME_" + label
        child.stdin.write("snmp3RuntimeReport\necho " + marker + "\n")
        child.stdin.flush()
        self.until(lambda: re.search(r"^" + marker + r"$", (self.folder / "ioc.log").read_text(), re.M), marker)
        text = (self.folder / "ioc.log").read_text().split("\n" + marker + "\n")[0]
        rows = re.findall(r"snmp3 records: ([^\n]+)", text)
        if not rows:
            raise Incomplete("Required runtime completion count is absent")
        fields = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", rows[-1])}
        write_json(self.folder / (label + "-runtime.json"), fields)
        return fields["completions"]

    def split(self):
        with self.application("split") as (peers, child):
            self.enable(peers)
            self.pause()
            members = [e for e in self.entries if e["tier"] == "meas"]
            raw = {u: [f"APCLOCAL:{u}:" + e["rawRecord"] for e in members] for u in self.units}
            names = [pv for u in self.units for pv in raw[u]]
            heads = [f"APCLOCAL:{u}:_PollMeas.PROC" for u in self.units]
            trials = []
            for trial in range(SPLIT_TRIALS):
                before = self.ca_probe("snapshot", names)
                native_before = self.runtime_count(child, f"trial{trial}_before")
                counts = {u: self.fields(u, "_PollMeas", "_PollMeasDone") for u in self.units}
                with self.split_gate:
                    self.split_selected = None
                    for peer in peers.values():
                        peer.split_delivered.clear()
                    self.split_armed = True
                pulse = self.ca_probe("pulse", heads)

                def finished_or_held():
                    return self.split_selected or all(float(self.fields(u, "_PollMeasDone")["_PollMeasDone"]) >
                                                     float(counts[u]["_PollMeasDone"]) for u in self.units)

                self.until(finished_or_held, "actual split or completed simultaneous cycles")
                selected = self.split_selected
                if selected is None:
                    self.split_armed = False
                    trials.append({"trial": trial, "split_held": False, "pulse": pulse})
                    write_json(self.folder / "split-trials.json", trials)
                    continue
                selected = dict(selected)
                unit = selected["unit"]
                group = next(g for g in self.groups if g["group"] == selected["group"])
                counter = group["counter"]
                expected_completed = len(selected["delivered_oids"])
                self.until(lambda: float(self.fields(unit, counter)[counter]) == expected_completed,
                           "earlier split responses complete", seconds=1.0)
                during = self.ca_probe("snapshot", names)
                held_state = self.fields(unit, counter, "_PollMeasDone", "_BusyMeas")
                native_during = self.runtime_count(child, f"trial{trial}_held")
                stamp = lambda row: (row["secondsPastEpoch1990"], row["nanoseconds"])
                completed = {pv for pv in names if stamp(during[pv]) != stamp(before[pv])}
                selected_completed = {pv for pv in raw[unit] if pv in completed}
                expected_pvs = {f"APCLOCAL:{unit}:" + e["rawRecord"] for e in members
                                if e["mib"] + "." + e["oid"].rsplit(".", 1)[1] in selected["delivered_oids"]}
                write_json(self.folder / "split-exposure.json", {"trial": trial, "selection": selected,
                           "before": before, "during": during, "state": held_state, "pulse": pulse,
                           "native_before": native_before, "native_during": native_during})
                self.check("split:actual-partial-completions", 0 < expected_completed < group["size"]
                           and selected_completed == expected_pvs and float(held_state[counter]) == expected_completed)
                self.check("split:no-early-cycle-completion", held_state["_BusyMeas"] == "1"
                           and held_state["_PollMeasDone"] == counts[unit]["_PollMeasDone"])
                self.check("split:completion-count-matches-records", native_during - native_before == len(completed))
                with self.split_gate:
                    self.split_armed = False
                peers[unit].deliver(peers[unit].held[0])
                self.until(lambda: all(float(self.fields(u, "_PollMeasDone")["_PollMeasDone"]) ==
                                       float(counts[u]["_PollMeasDone"]) + 1 for u in self.units), "split cycles finish once")
                after = self.ca_probe("snapshot", names)
                native_after = self.runtime_count(child, f"trial{trial}_after")
                final = {u: self.fields(u, "_PollMeas", "_PollMeasDone", "_BusyMeas", counter) for u in self.units}
                write_json(self.folder / "split-final.json", {"trial": trial, "after": after, "state": final,
                           "native_after": native_after, "expected_members": len(names)})
                self.check("split:every-record-completes", all(stamp(after[pv]) > stamp(before[pv]) for pv in names))
                self.check("split:exact-native-completion-delta", native_after - native_before == len(names))
                self.check("split:one-cycle-per-unit", all(float(final[u]["_PollMeas"]) == float(counts[u]["_PollMeas"]) + 1
                           and final[u]["_BusyMeas"] == "0" and float(final[u][counter]) == group["size"] for u in self.units))
                trials.append({"trial": trial, "split_held": True, "selection": selected, "pulse": pulse})
                write_json(self.folder / "split-trials.json", trials)
                self.wire(peers)
                return
            raise Incomplete("No actual partial-response completion window was exposed")

    def outage(self):
        with self.application("outage") as (peers, child):
            self.enable(peers)
            unit, other = self.units
            peer = peers[unit]
            peer.enabled = False
            peers[other].set("rPDU2PhaseStatusCurrent", 1, 145)
            self.until(lambda: self.fields(unit, "Online")["Online"] == "0", "offline flag")
            names = [f"APCLOCAL:{unit}:{e['record']}.SEVR" for e in self.entries]
            self.until(lambda: all(v == "3" for v in self.ca(names).values()), "all input alarms offline")
            self.check("outage:all-public-inputs-invalid", True)
            phase_key = next(k for k, m in self.mapping.items() if not k.startswith("+")
                             and m["+channel"].endswith("Phase1Current.VAL"))
            offline_pva, _ = self.group(unit)
            self.check("outage:PVA-invalid", offline_pva[phase_key + ".alarm.severity"] == 3)
            self.check("outage:other-unit-valid", self.fields(other, "Online", "Phase1Current.SEVR") == {"Online": "1", "Phase1Current.SEVR": "0"})
            self.until(lambda: math.isclose(float(self.fields(other, "Phase1Current")["Phase1Current"]), 14.5),
                       "fresh data on the other unit during outage")
            self.check("outage:other-unit-fresh-data", self.fields(other, "Phase1Current.SEVR")["Phase1Current.SEVR"] == "0")
            other_pva, _ = self.group(other)
            self.check("outage:PVA-other-unit-fresh-data", math.isclose(other_pva[phase_key + ".value"], 14.5)
                       and other_pva[phase_key + ".alarm.severity"] == 0)
            time.sleep(3.5)
            start = time.monotonic_ns()
            time.sleep(PROBE_SECONDS)
            end = time.monotonic_ns()
            probes = [e for e in peer.events if e["event"] == "request" and start <= e["ns"] <= end]
            write_json(self.folder / "probe-window.json", {"start_ns": start, "end_ns": end, "requests": probes})
            canary = self.entries[0]["mib"] + "." + self.entries[0]["oid"].rsplit(".", 1)[1]
            self.check("outage:probe-only", bool(probes) and all(e["oids"] == [canary] for e in probes))
            state_period = float(re.search(r'epicsEnvSet\("STATE_SCAN","([\d.]+) second"\)',
                                          (self.candidate / "local/startup.cmd").read_text())[1])
            self.check("outage:probe-count-bounded", len(probes) <= math.ceil((end - start) / 1e9 / state_period) + 1,
                       {"requests": len(probes), "seconds": (end - start) / 1e9, "source_period": state_period})
            peer.enabled = True
            self.until(lambda: self.fields(unit, "Online", "Phase1Current.SEVR") == {"Online": "1", "Phase1Current.SEVR": "0"}, "same-value recovery")
            self.until(lambda: self.fields(unit, "Outlet01ConfigNumber.SEVR")["Outlet01ConfigNumber.SEVR"] == "0", "slow recovery")
            self.healthy("recovery")
            self.check("outage:same-value-all-tier-recovery", True)
            recovered_pva, _ = self.group(unit)
            phase = next(e for e in self.entries if e["record"] == "Phase1Current")
            expected = self.fixture.fixture_value(phase, self.info["layout"])[1] * phase["scale"]
            self.check("outage:PVA-same-value-recovery", math.isclose(recovered_pva[phase_key + ".value"], expected)
                       and recovered_pva[phase_key + ".alarm.severity"] == 0)
            write_json(self.folder / "outage-pva.json", {"offline": offline_pva, "other_during_outage": other_pva,
                                                       "recovered": recovered_pva})
            self.wire(peers)

    def cleanup(self):
        try:
            with self.application("cleanup"):
                raise SetupFailure("Intentional failure after actual IOC initialization")
        except SetupFailure:
            self.check("cleanup:setup-failure-detected", True)
        self.check("cleanup:all-owned-processes-stop", all(p.get("stopped") and p.get("repeater_stopped") and not p.get("forced") for p in self.processes))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", choices=(*CASES, "all"), default="all")
    parser.add_argument("--control", choices=CONTROLS, default="none")
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("--replay-wire", type=Path)
    parser.add_argument("--proof-sha256")
    args = parser.parse_args()
    if args.replay_wire and (args.control != "none" or args.reverse):
        parser.error("Retained wire evaluation cannot run live controls or reverse cases")
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / "work"):
        parser.error("--output must be a new directory under work/")
    output.mkdir(parents=True, mode=0o700)
    run, error, status, rc = None, None, "INCOMPLETE", 2
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, interrupt)
    try:
        run = Grouped(args.candidate.resolve(), output, args.control)
        run.identify()
        if args.replay_wire:
            run.replay_wire(args.replay_wire, args.proof_sha256)
        else:
            cases = CASES if args.case == "all" else (args.case,)
            for name in reversed(cases) if args.reverse else cases:
                getattr(run, name)()
        status, rc = "PASS", 0
    except KeyboardInterrupt as e:
        error, rc = "Interrupted", 128 + getattr(e, "signum", signal.SIGINT)
    except (Incomplete, FileNotFoundError, OSError, json.JSONDecodeError) as e:
        error = str(e)
    except Exception:
        error, status, rc = traceback.format_exc(), "FAIL", 1
    finally:
        if run:
            checks, processes = run.checks, run.processes
            if run.blackhole is not None:
                discarded = 0
                try:
                    while True:
                        run.blackhole.recvfrom(65535)
                        discarded += 1
                except BlockingIOError:
                    pass
                finally:
                    run.blackhole.close()
                write_json(output / "pva-control.json", {"searches_discarded": discarded, "socket_closed": True})
            if any(p.get("initialized") and (not p.get("worker_terminal") or p.get("forced") or p.get("ioc_returncode") != 0)
                   for p in processes):
                checks.append({"name": "terminal-resources", "passed": False, "detail": processes})
            if any(not p.get("stopped") for p in run.client_processes):
                checks.append({"name": "client-resources", "passed": False, "detail": run.client_processes})
            if any(c["passed"] is False for c in checks):
                status = "FAIL"
                if rc in (0, 2):
                    rc = 1
        else:
            checks, processes = [], []
        write_json(output / "result.json", {"status": status, "checks": checks, "error": error,
                   "native_returncode": rc, "processes": processes, "runner_sha256": digest(__file__),
                   "mode": "retained-wire-evaluation" if args.replay_wire else "actual-IOC-execution",
                   "client_processes": run.client_processes if run else [], "tools": run.tool_hashes if run else {}})
        if error:
            print(error, flush=True)
        print(f"Evidence: {output}; checks: {len(checks)}; result: {status}; exit: {rc}", flush=True)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
