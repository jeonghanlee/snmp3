#!/usr/bin/env python3
"""Verify the generated AP8932 IOC against the committed outer SNMP fixture."""

import argparse
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import time
import traceback

from build_apcpdu import ARCH, ROOT, digest, load_peer, write_json


WAIT_SECONDS = 40


def port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def delta(text):
    values, types = {}, {}
    for line in text.splitlines():
        match = re.fullmatch(r"\s*(\S+)\s+(\S+) = (.*)", line)
        if not match:
            continue
        name, kind, value = match.groups()
        if kind == "string":
            value = json.loads(value)
        elif kind in ("double", "float"):
            value = float(value)
        elif re.fullmatch(r"u?int\d+_t", kind):
            value = int(value)
        else:
            continue
        if name in values:
            raise ValueError(f"Duplicate client field: {name}")
        values[name], types[name] = value, kind
    return values, types


class Runner:
    def __init__(self, ioc, output):
        self.ioc, self.output = ioc, output
        self.build = json.loads((ioc / "build.json").read_text())
        self.inventory = json.loads((ioc / "inventory.json").read_text())
        self.prefix = self.build["prefix"]
        self.binary = ioc / "bin" / ARCH / self.build["app"]
        self.base, self.pvxs = Path(self.build["base"]), Path(self.build["pvxs"])
        self.serial, self.checks, self.processes = 0, [], []
        self.env = {k: v for k, v in os.environ.items() if not k.startswith(("EPICS_CA", "EPICS_PVA"))}
        ca, pva, repeater = port(), port(), port()
        while len({ca, pva, repeater}) != 3:
            ca, pva, repeater = port(), port(), port()
        self.env.update({
            "EPICS_CAS_INTF_ADDR_LIST": "127.0.0.1", "EPICS_CA_ADDR_LIST": f"127.0.0.1:{ca}",
            "EPICS_CA_AUTO_ADDR_LIST": "NO", "EPICS_CA_NAME_SERVERS": "",
            "EPICS_CAS_AUTO_BEACON_ADDR_LIST": "NO", "EPICS_CAS_BEACON_ADDR_LIST": "",
            "EPICS_CA_SERVER_PORT": str(ca), "EPICS_CA_REPEATER_PORT": str(repeater),
            "EPICS_PVAS_INTF_ADDR_LIST": "127.0.0.1", "EPICS_PVAS_SERVER_PORT": str(pva),
            "EPICS_PVAS_BROADCAST_PORT": str(pva), "EPICS_PVAS_AUTO_BEACON_ADDR_LIST": "NO",
            "EPICS_PVAS_BEACON_ADDR_LIST": "", "EPICS_PVA_ADDR_LIST": f"127.0.0.1:{pva}",
            "EPICS_PVA_AUTO_ADDR_LIST": "NO", "EPICS_PVA_NAME_SERVERS": "",
            "EPICS_PVA_BROADCAST_PORT": str(pva),
        })
        write_json(output / "environment.json", {k: v for k, v in self.env.items() if k.startswith("EPICS_")})

    def check(self, name, passed, detail=None):
        self.checks.append({"name": name, "passed": bool(passed), "detail": detail})
        print(f"[ {'PASS' if passed else 'FAIL'} ] {name}", flush=True)
        if not passed:
            raise AssertionError(f"{name}: {detail}")

    def client(self, name, *args):
        directory = self.pvxs if name.startswith("pvx") else self.base
        argv = [str(directory / "bin" / ARCH / name), *map(str, args)]
        self.serial += 1
        result = subprocess.run(argv, env=self.env, capture_output=True, text=True, timeout=10)
        path = self.output / f"client-{self.serial:04}"
        path.with_suffix(".out").write_text(result.stdout)
        path.with_suffix(".err").write_text(result.stderr)
        write_json(path.with_suffix(".json"), {"argv": argv, "returncode": result.returncode})
        return result

    def group(self):
        result = self.client("pvxget", "-w", "1", "-F", "delta", self.inventory["group"])
        if result.returncode:
            raise RuntimeError(result.stderr)
        return delta(result.stdout)

    def ca(self, names):
        result = self.client("caget", "-w", "2", "-n", "-F", "\t", *names)
        result.check_returncode()
        values = {}
        for line in result.stdout.splitlines():
            if "\t" in line:
                name, value = line.split("\t", 1)
                values[name] = value
        if set(values) != set(names):
            raise ValueError("Incomplete CA response")
        return values

    def wait_group(self, predicate):
        end, last = time.monotonic() + WAIT_SECONDS, None
        while time.monotonic() < end:
            try:
                last = self.group()
                if predicate(last[0]):
                    return last
            except (RuntimeError, ValueError) as error:
                last = str(error)
            time.sleep(0.2)
        write_json(self.output / "last-group.json", last)
        raise AssertionError("Group condition timed out; see last-group.json and client logs")

    def config(self, folder, snmp_port, version="2c"):
        document = json.loads((self.ioc / "config.template.json").read_text())
        if len(document["endpoints"]) != 1 or document["endpoints"][0]["address"] != "127.0.0.1":
            raise ValueError("Local runner accepts exactly one loopback endpoint")
        document["endpoints"][0]["port"] = snmp_port
        profile = document["profiles"][0]
        profile["version"] = version
        secret = folder / "community.txt"
        secret.write_text("apc-local-test\n")
        secret.chmod(0o600)
        profile["communityFile"] = str(secret)
        if version == "3":
            del profile["communityFile"]
            profile.update(user="local-apc-test", securityLevel="authPriv", authAlgorithm="SHA",
                           privAlgorithm="AES", authSecretFile=str(secret), privSecretFile=str(secret))
        path = folder / "configuration.json"
        write_json(path, document)
        return path

    def header(self):
        app = self.build["app"]
        return f'on error break\ndbLoadDatabase("{self.ioc}/dbd/{app}.dbd")\n{app}_registerRecordDeviceDriver(pdbbase)\n'

    @contextmanager
    def process(self, name, argv):
        path = self.output / (name + ".log")
        with path.open("w") as log:
            child = subprocess.Popen(list(map(str, argv)), cwd=self.ioc, env=self.env,
                                     stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
                                     text=True, start_new_session=True)
            item = {"name": name, "argv": list(map(str, argv)), "pid": child.pid}
            self.processes.append(item)
            try:
                yield child
            finally:
                forced = False
                if child.poll() is None:
                    try:
                        child.communicate("exit\n" if name != "repeater" else "", timeout=8 if name != "repeater" else 0.1)
                    except subprocess.TimeoutExpired:
                        forced = name != "repeater"
                        os.killpg(child.pid, signal.SIGKILL if forced else signal.SIGTERM)
                        child.wait(timeout=5)
                item.update(returncode=child.returncode, forced=forced, stopped=child.poll() is not None)
                write_json(self.output / "processes.json", self.processes)

    @contextmanager
    def fixture(self, name):
        folder = self.output / name
        folder.mkdir()
        peer = load_peer(self.ioc / "source/tests/snmp_peer.py").Pdu(0, folder)
        try:
            config = self.config(folder, peer.server_address[1])
            script = folder / "startup.cmd"
            script.write_text(self.header() +
                              f'iocshLoad("{self.ioc}/iocsh/device.cmd","CONFIG={config}")\n' +
                              'iocInit\npvxsi\nsnmp3ConfigReport\nsnmp3RuntimeReport\necho LOCAL_IOC_READY\n')
            with self.process(name, [self.binary, script]) as child:
                yield peer, child
        finally:
            peer.close()
            write_json(folder / "peer.json", {"errors": peer.errors, "requests": len(peer.requests),
                       "sets": sum(kind == 0xA3 for kind, _ in peer.requests),
                       "socket_closed": peer.fileno() == -1})

    def startup(self):
        for version in ("1", "2c", "3"):
            folder = self.output / ("valid-" + version)
            folder.mkdir()
            config = self.config(folder, port(), version)
            script = folder / "startup.cmd"
            script.write_text(self.header() + f'iocshLoad("{self.ioc}/iocsh/configuration.cmd","CONFIG={config}")\n' +
                              'snmp3ConfigReport\necho CONFIGURATION_ACCEPTED\n')
            name = "valid-" + version
            with self.process(name, [self.binary, script]) as child:
                child.communicate("exit\n", timeout=15)
            log = (self.output / (name + ".log")).read_text()
            self.check(name + ":configuration-only", child.returncode == 0 and
                       "CONFIGURATION_ACCEPTED" in log and "profiles=1 endpoints=1" in log)
        for later in (False, True):
            name = "reject-later" if later else "reject-first"
            folder = self.output / name
            folder.mkdir()
            valid = self.config(folder, port())
            bad = folder / "bad.json"
            bad.write_text('{"schema": 999}\n')
            script = folder / "startup.cmd"
            before = (f'iocshLoad("{self.ioc}/iocsh/configuration.cmd","CONFIG={valid}")\n' +
                      'echo EARLIER_CONFIGURATION_ACCEPTED\n') if later else ""
            script.write_text(self.header() + before +
                              f'iocshLoad("{self.ioc}/iocsh/device.cmd","CONFIG={bad}")\n' +
                              'iocInit\necho UNEXPECTED_STARTUP_CONTINUATION\n')
            with self.process(name, [self.binary, script]) as child:
                child.communicate("echo UNEXPECTED_INTERACTIVE_SHELL\nexit\n", timeout=15)
            log = (self.output / (name + ".log")).read_text()
            self.check(name + ":nonzero-before-init-and-shell", child.returncode == 1 and
                       "startup script failed" in log and "iocInit" not in log and
                       "UNEXPECTED_" not in log and (not later or "EARLIER_CONFIGURATION_ACCEPTED" in log))

    def acceptance(self):
        with self.fixture("acceptance") as (peer, child):
            values, _ = self.wait_group(lambda v: v.get("phase.current.alarm.severity") == 3)
            self.check("cold-start:unavailable", values["phase.current.alarm.severity"] == 3)
            peer.enabled = True
            values, types = self.wait_group(lambda v: v.get("device.model") == "AP8932" and
                                           v.get("device.error.value.index") == 0)
            self.check("identity:raw-octets", values["device.model"] == "AP8932")
            public = self.inventory["public_records"]
            registered = self.ca([name + ".RTYP" for name in public])
            self.check("public-records:names-and-types", len(public) == 400 and
                       all(registered[name + ".RTYP"] == kind for name, kind in public.items()))
            log = (self.output / "acceptance.log").read_text()
            self.check("registration:all-snmp3-bindings", "contexts=178" in log and
                       "frozen=1 profiles=1 endpoints=1 bindings=178" in log)
            expected = {"phase.current.value": 12, "phase.voltage.value": 208,
                        "phase.power.value": 2.5, "phase.apparent_power.value": 2.6,
                        "phase.power_factor.value": 0.96, "bank1.current.value": 6,
                        "bank2.current.value": 6, "limits.device_power.near.value": 3,
                        "limits.device_power.over.value": 3.5}
            for key, value in expected.items():
                self.check(key, math.isclose(values[key], value, abs_tol=1e-9), values[key])
            for channel in range(1, 25):
                key = f"ch{channel:02}.state.value.index"
                self.check(f"outlet-{channel}:decoded", values[key] == 2 and
                           values[f"ch{channel:02}.state.alarm.severity"] == 0)
            mappings = self.inventory["mappings"]
            ca = self.ca([m["+channel"] for m in mappings.values()])
            for key, mapping in mappings.items():
                channel = mapping["+channel"]
                kind = self.inventory["public_records"][channel[:-4]]
                field = key if mapping["+type"] == "plain" else key + ".value" + (".index" if kind in ("mbbi", "bi") else "")
                expected_type = "string" if kind == "stringin" else "double" if kind in ("ai", "calc") else "int32_t"
                self.check(key + ":pva-type", types.get(field) == expected_type, types.get(field))
                equal = ca[channel] == values[field] if expected_type == "string" else math.isclose(float(ca[channel]), values[field], abs_tol=1e-9)
                self.check(key + ":ca-pva-value", equal, {"ca": ca[channel], "pva": values[field]})
            put = self.client("pvxput", "-w", "2", self.inventory["group"], "phase.current.value=999")
            self.check("group:read-only", put.returncode != 0 and self.group()[0]["phase.current.value"] == 12)
            before_time = (values["phase.current.timeStamp.secondsPastEpoch"], values["phase.current.timeStamp.nanoseconds"])
            peer.enabled = False
            failed, _ = self.wait_group(lambda v: v.get("phase.current.alarm.severity") == 3 and
                                       v.get("device.error.value.index") == 3)
            self.check("outage:public-and-summary-invalid", failed["phase.current.alarm.severity"] == 3)
            fields = [self.prefix + suffix for suffix in ("PhaseCurrRaw_.PACT", "PhaseCurrRaw_.UDF", "PhaseCurrRaw_.STAT",
                                                         "PhaseCurrRaw_.SEVR", "PhaseCurr.UDF", "PhaseCurr.SEVR")]
            write_json(self.output / "outage-fields.json", self.ca(fields))
            peer.enabled = True
            recovered, _ = self.wait_group(lambda v: v.get("phase.current.value") == 12 and
                                          v.get("phase.current.alarm.severity") == 0 and
                                          v.get("device.error.value.index") == 0)
            after_time = (recovered["phase.current.timeStamp.secondsPastEpoch"], recovered["phase.current.timeStamp.nanoseconds"])
            self.check("recovery:same-value-new-timestamp", after_time > before_time)
            with peer.lock:
                self.check("wire:no-unintended-set", bool(peer.requests) and not peer.errors and
                           not any(kind == 0xA3 for kind, _ in peer.requests))
            maps = Path(f"/proc/{child.pid}/maps").read_text()
            (self.output / "loaded-maps.txt").write_text(maps)
            loaded = sorted({line.split()[-1] for line in maps.splitlines() if ".so" in line and line.split()[-1].startswith("/")})
            write_json(self.output / "loaded-libraries.json", {name: digest(name) for name in loaded})
            self.check("libraries:snmp3-pvxs-no-legacy", all(any(token in name for name in loaded)
                       for token in ("libsnmp3.so", "libsnmp3Wire.so", "libpvxsIoc.so", "libpvxs.so")) and
                       not any("devSnmp" in name or "freshSnmp" in name for name in loaded))
        self.check("acceptance:normal-exit", self.processes[-1]["returncode"] == 0 and not self.processes[-1]["forced"])
        self.shutdown_evidence("acceptance")

    def shutdown_evidence(self, name):
        log = (self.output / (name + ".log")).read_text()
        self.check(name + ":worker-reaped-and-queues-empty",
                   "state=Stopped admission=0 activation=1 created=1 exited=1 joined=1" in log and
                   "active=0 pending=0 queued=0 running=0 inert=0 entered=0" in log and
                   "count=0 bytes=0" in log and
                   re.search(r"pid=0 ready=0 closing=[01] batch=0 launches=1 reaps=1 forced=0", log) is not None)

    def cleanup_case(self):
        class SetupFailure(Exception):
            pass
        try:
            with self.fixture("cleanup") as (peer, child):
                self.wait_group(lambda v: v.get("phase.current.alarm.severity") == 3)
                raise SetupFailure("Deliberate harness setup failure after IOC start")
        except SetupFailure:
            pass
        self.check("setup-failure:resources-stopped", child.poll() == 0 and peer.fileno() == -1 and
                   not self.processes[-1]["forced"])
        self.shutdown_evidence("cleanup")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ioc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", choices=("all", "startup", "acceptance", "cleanup"), default="all")
    args = parser.parse_args()
    ioc, output = args.ioc.resolve(), args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / "work"):
        parser.error("--output must be a new directory under this checkout's work/")
    output.mkdir(parents=True)
    runner = Runner(ioc, output)
    error = None
    try:
        for path, expected in runner.build["products"].items():
            if digest(path) != expected:
                raise ValueError(f"Build product changed: {path}")
        for name, expected in runner.build["source_hashes"].items():
            if digest(ioc / "source" / name) != expected:
                raise ValueError(f"Exported source changed: {name}")
        runner.check("products:build-identities", True)
        with runner.process("repeater", [runner.base / "bin" / ARCH / "caRepeater"]):
            if args.case in ("all", "startup"):
                runner.startup()
            if args.case in ("all", "acceptance"):
                runner.acceptance()
            if args.case in ("all", "cleanup"):
                runner.cleanup_case()
    except Exception:
        error = traceback.format_exc()
        (output / "error.txt").write_text(error)
        print(error, flush=True)
    finally:
        write_json(output / "result.json", {"case": args.case, "passed": error is None,
                   "checks": runner.checks, "error": error, "processes": runner.processes,
                   "runner_sha256": digest(__file__), "build_sha256": digest(ioc / "build.json"),
                   "limits": ["No hardware or legacy comparator", "No v3 wire qualification",
                              "Sensors and setting write-back disabled", "No command SET qualification",
                              "No delayed/reordered-response matrix", "M7 remains open"]})
    print(f"Evidence: {output}; checks: {len(runner.checks)}; result: {'PASS' if error is None else 'FAIL'}")
    return 0 if error is None else 1


if __name__ == "__main__":
    raise SystemExit(main())
