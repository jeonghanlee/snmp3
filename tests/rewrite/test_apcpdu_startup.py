#!/usr/bin/env python3
"""Qualify actual installed APC startup without changing the installation.

Use the candidate's actual generator, installed binary/DBD/DB and both loaders.
All configurations use two loopback endpoints and private disposable secrets.
Valid v1/v2c/v3 configurations reach iocInit and the real interactive shell;
invalid first/later configuration stops before either. A separate Failed-state
case deliberately attempts iocInit with a missing worker and resets script
status before returning, exercising the installed main's state check. The
outer-script control omits on-error-break and must expose continuation.

Cases own fresh processes and UDP sinks. Missing inputs/observations remain
Incomplete (2), observed violations are Failed (1), and success is Passed (0).
Keep real command output, native status, loaded mappings, record registration,
source/product identities and cleanup evidence. No device communication,
CA/PVA value, grouped-read, sustained-load or full M7 acceptance is claimed.
"""

import argparse
import copy
import json
import os
from pathlib import Path
import re
import secrets
import signal
import socket
import subprocess
import sys
import time
import traceback

from build_apcpdu import ARCH, ROOT, digest, write_json
from test_apcpdu import port
from test_apcpdu_operations import Incomplete, private_file


VALID = (("1", None), ("2c", None), ("3", "noAuthNoPriv"), ("3", "authNoPriv"), ("3", "authPriv"))
FAULTS = ("missing-file", "schema", "endpoint", "duplicate-profile", "privacy")
AFTER_INIT = "INSTALLED_AFTER_INIT"
BEFORE_INIT = "INSTALLED_BEFORE_INIT"
SHELL = "INSTALLED_SHELL_REACHED"
RECORDS_BEGIN = "INSTALLED_RECORDS_BEGIN"
RECORDS_END = "INSTALLED_RECORDS_END"


class Startup:
    def __init__(self, apc, output):
        self.apc, self.output = apc, output
        self.binary = apc / "bin" / ARCH / "apcpdu"
        self.checks, self.processes, self.commands, self.sentinels = [], [], [], []
        self.identities = {}
        self.required = [self.binary, apc / "dbd/apcpdu.dbd", apc / "db/ap8932.db",
                         apc / "apcpduApp/Db/ap8932.db", apc / "iocsh/snmp3.iocsh",
                         apc / "iocsh/ap8932.iocsh", apc / "apcpduApp/src/apcpduMain.cpp",
                         apc / "scripts/gen_ap8932.py", apc / "docs/ap8932-inventory.json",
                         apc / "mibs/PowerNet-MIB", Path(__file__), ROOT / "snmp3App/src/Register.cpp",
                         ROOT / "tests/rewrite/build_apcpdu.py", ROOT / "tests/rewrite/test_apcpdu.py",
                         ROOT / "tests/rewrite/test_apcpdu_operations.py",
                         ROOT / "bin" / ARCH / "snmp3Worker", ROOT / "bin" / ARCH / "snmp3NativeProbe",
                         ROOT / "lib" / ARCH / "libsnmp3.so", ROOT / "lib" / ARCH / "libsnmp3Native.so"]
        for path in self.required:
            if not path.is_file():
                raise Incomplete("Installed prerequisite missing: " + str(path))
            self.identities[str(path)] = digest(path)
        self.info = json.loads((apc / "docs/ap8932-inventory.json").read_text())
        self.record_templates = re.findall(r'^record\(\w+,\s*"([^"]+)"\)', (apc / "db/ap8932.db").read_text(), re.M)
        self.env = {k: v for k, v in os.environ.items() if not k.startswith(("EPICS_CA", "EPICS_PVA", "LD_"))}
        ca, pva, repeater = port(), port(), port()
        while len({ca, pva, repeater}) != 3:
            ca, pva, repeater = port(), port(), port()
        self.env.update({"EPICS_CAS_INTF_ADDR_LIST": "127.0.0.1", "EPICS_CA_ADDR_LIST": f"127.0.0.1:{ca}",
                         "EPICS_CA_AUTO_ADDR_LIST": "NO", "EPICS_CA_NAME_SERVERS": "",
                         "EPICS_CAS_AUTO_BEACON_ADDR_LIST": "NO", "EPICS_CAS_BEACON_ADDR_LIST": "",
                         "EPICS_CA_SERVER_PORT": str(ca), "EPICS_CA_REPEATER_PORT": str(repeater),
                         "EPICS_PVAS_INTF_ADDR_LIST": "127.0.0.1", "EPICS_PVAS_SERVER_PORT": str(pva),
                         "EPICS_PVAS_BROADCAST_PORT": str(pva), "EPICS_PVAS_AUTO_BEACON_ADDR_LIST": "NO",
                         "EPICS_PVAS_BEACON_ADDR_LIST": "", "EPICS_PVA_ADDR_LIST": f"127.0.0.1:{pva}",
                         "EPICS_PVA_AUTO_ADDR_LIST": "NO", "EPICS_PVA_NAME_SERVERS": ""})
        write_json(output / "environment.json", {k: v for k, v in self.env.items() if k.startswith("EPICS_")})
        for label, binary in (("apc", self.binary), ("probe", ROOT / "bin" / ARCH / "snmp3NativeProbe"),
                              ("worker", ROOT / "bin" / ARCH / "snmp3Worker")):
            linked = self.command(label + "-libraries", ["ldd", binary])
            if linked.returncode or "not found" in linked.stdout:
                raise Incomplete("Installed dependency unavailable: " + label)
            for library in re.findall(r"(/\S+)\s+\(", linked.stdout):
                self.identities[library] = digest(library)

    def check(self, name, condition, detail=None):
        self.checks.append({"name": name, "passed": bool(condition), "detail": detail})
        print(f"[ {'PASS' if condition else 'FAIL'} ] {name}", flush=True)
        if not condition:
            raise AssertionError(f"{name}: {detail}")

    def command(self, name, argv):
        start = time.time_ns()
        result = subprocess.run(list(map(str, argv)), cwd=ROOT, env=self.env, capture_output=True, text=True, timeout=30)
        (self.output / (name + ".stdout")).write_text(result.stdout)
        (self.output / (name + ".stderr")).write_text(result.stderr)
        self.commands.append({"name": name, "argv": list(map(str, argv)), "cwd": str(ROOT),
                              "native_returncode": result.returncode, "start_utc_ns": start, "end_utc_ns": time.time_ns()})
        write_json(self.output / "commands.json", self.commands)
        return result

    def configuration(self, folder, version="2c", level=None):
        sinks = []
        try:
            for _ in range(2):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sinks.append(sock)
                sock.bind(("127.0.0.1", 0))
            profile = {"id": "Local", "version": version, "timeoutMs": 100, "retries": 0}
            community = secrets.token_hex(12)
            self.sentinels.append(community.encode())
            private_file(folder / "community.txt", community + "\n")
            if version == "3":
                profile.update(user="InstalledLocal", securityLevel=level)
                if level != "noAuthNoPriv":
                    auth = secrets.token_hex(12)
                    self.sentinels.append(auth.encode())
                    private_file(folder / "auth.txt", auth + "\n")
                    profile.update(authAlgorithm="SHA", authSecretFile=str(folder / "auth.txt"))
                if level == "authPriv":
                    privacy = secrets.token_hex(12)
                    self.sentinels.append(privacy.encode())
                    private_file(folder / "privacy.txt", privacy + "\n")
                    profile.update(privAlgorithm="AES", privSecretFile=str(folder / "privacy.txt"))
            else:
                profile["communityFile"] = str(folder / "community.txt")
            site = {"profiles": [profile], "endpoints": [{"id": f"PDU{n}", "profile": "Local",
                    "address": "127.0.0.1", "port": sock.getsockname()[1]} for n, sock in enumerate(sinks, 1)]}
            write_json(folder / "site.json", site)
            config = folder / "configuration.json"
            command = self.command(folder.name + "-generate", [sys.executable, "-B", self.apc / "scripts/gen_ap8932.py",
                                   "config", "--site", folder / "site.json", "--units", "PDU1,PDU2", "--output", config])
            self.check(folder.name + ":generator", command.returncode == 0)
            document = json.loads(config.read_text())
            self.check(folder.name + ":complete-local-configuration", len(document["endpoints"]) == 2 and
                       len(document["bindings"]) == 2 * len(self.info["bindings"]) and
                       all(e["address"] == "127.0.0.1" for e in document["endpoints"]) and
                       all(b["operation"] == "get" for b in document["bindings"]))
            return config, sinks
        except BaseException:
            for sock in sinks:
                sock.close()
            raise

    def header(self, on_error=True):
        return (("on error break\n" if on_error else "") + f'epicsEnvSet("SNMP3","{ROOT}")\n' +
                f'dbLoadDatabase("{self.apc}/dbd/apcpdu.dbd")\napcpdu_registerRecordDeviceDriver(pdbbase)\n')

    def module(self, config):
        return f'iocshLoad("{self.apc}/iocsh/snmp3.iocsh","SNMP_CONFIG={config}")\n'

    def unit(self, number):
        return f'iocshLoad("{self.apc}/iocsh/ap8932.iocsh","P=APCSTART:,R=PDU{number}:,U=PDU{number},DATABASE_TOP={self.apc}/db")\n'

    @staticmethod
    def marker(log, value):
        return value in [line.strip() for line in log.splitlines()]

    def run_ioc(self, name, folder, script, valid=False):
        path = folder / "startup.cmd"
        path.write_text(script)
        argv = [str(self.binary), str(path)]
        started = time.time_ns()
        log_path = folder / "ioc.log"
        stderr_path = folder / "ioc.stderr"
        with log_path.open("x") as log, stderr_path.open("x") as stderr:
            child = subprocess.Popen(argv, cwd=folder, env=self.env, stdin=subprocess.PIPE,
                                     stdout=log, stderr=stderr, text=True, start_new_session=True)
            item = {"name": name, "argv": argv, "cwd": str(folder), "pid": child.pid, "start_utc_ns": started}
            self.processes.append(item)
            forced = False
            try:
                if valid:
                    child.stdin.write("echo " + SHELL + "\n")
                    child.stdin.flush()
                    deadline = time.monotonic() + 20
                    while child.poll() is None and time.monotonic() < deadline:
                        if self.marker(log_path.read_text(), SHELL):
                            break
                        time.sleep(0.05)
                    if child.poll() is None:
                        maps = Path(f"/proc/{child.pid}/maps").read_text()
                        (folder / "loaded-maps.txt").write_text(maps)
                        libraries = sorted({line.split()[-1] for line in maps.splitlines()
                                            if ".so" in line and line.split()[-1].startswith("/")})
                        write_json(folder / "loaded-libraries.json", {p: digest(p) for p in libraries})
                        for library in libraries:
                            expected = digest(library)
                            if self.identities.get(library, expected) != expected:
                                raise Incomplete("Loaded library changed: " + library)
                            self.identities[library] = expected
                    child.communicate("snmp3RuntimeReport\nexit\n", timeout=12)
                else:
                    child.communicate("echo " + SHELL + "\nexit\n", timeout=15)
            finally:
                if child.poll() is None:
                    child.send_signal(signal.SIGTERM)
                    try:
                        child.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        forced = True
                        child.kill()
                        child.wait(timeout=5)
                item.update(native_returncode=child.returncode, stopped=child.poll() is not None,
                            forced=forced, end_utc_ns=time.time_ns())
                write_json(self.output / "processes.json", self.processes)
        return child.returncode, log_path.read_text() + "\n" + stderr_path.read_text()

    def records(self, name, log, units):
        expected = {record.replace("$(P)", "APCSTART:").replace("$(R)", f"PDU{unit}:")
                    for unit in units for record in self.record_templates}
        lines = [line.strip() for line in log.splitlines()]
        actual = {line for line in lines if line.startswith("APCSTART:") and not any(c.isspace() for c in line)}
        self.check(name + ":actual-record-registration", actual == expected,
                   {"expected": len(expected), "actual": len(actual), "missing": sorted(expected - actual)[:10],
                    "extra": sorted(actual - expected)[:10]})

    def valid(self):
        for version, level in VALID:
            name = "valid-" + (level or version)
            folder = self.output / name
            folder.mkdir()
            config, sinks = self.configuration(folder, version, level)
            try:
                script = self.header() + self.module(config) + self.unit(1) + self.unit(2)
                script += f'echo {RECORDS_BEGIN}\ndbl\necho {RECORDS_END}\necho {BEFORE_INIT}\niocInit\necho {AFTER_INIT}\nsnmp3RuntimeReport\n'
                code, log = self.run_ioc(name, folder, script, valid=True)
                self.check(name + ":native-success", code == 0, code)
                self.check(name + ":iocinit-and-shell", self.marker(log, BEFORE_INIT) and
                           self.marker(log, AFTER_INIT) and self.marker(log, SHELL) and
                           "iocRun: All initialization complete" in log)
                self.check(name + ":runtime-running", "state=Running" in log)
                self.records(name, log, (1, 2))
            finally:
                for sock in sinks:
                    sock.close()

    def invalid(self, control=False):
        cases = ((False, "schema"),) if control else tuple((later, kind) for later in (False, True) for kind in FAULTS)
        for later, kind in cases:
            name = ("later-" if later else "first-") + kind
            folder = self.output / name
            folder.mkdir()
            config, sinks = self.configuration(folder, "3" if kind == "privacy" else "2c",
                                               "authPriv" if kind == "privacy" else None)
            try:
                bad = folder / "invalid.json"
                document = copy.deepcopy(json.loads(config.read_text()))
                if kind == "schema":
                    document["schema"] = 999
                elif kind == "endpoint":
                    document["bindings"][0]["endpoint"] = "Absent"
                elif kind == "duplicate-profile":
                    document["profiles"].append(copy.deepcopy(document["profiles"][0]))
                elif kind == "privacy":
                    del document["profiles"][0]["privSecretFile"]
                if kind != "missing-file":
                    write_json(bad, document)
                script = self.header(on_error=not control)
                if later:
                    script += self.module(config) + self.unit(1)
                    script += 'echo EARLIER_DEVICE_LOADED\ndbl\nsnmp3ConfigReport\n'
                script += self.module(bad) + self.unit(2 if later else 1)
                script += f'echo {BEFORE_INIT}\niocInit\necho {AFTER_INIT}\nsnmp3RuntimeReport\n'
                code, log = self.run_ioc(name, folder, script)
                self.check(name + ":native-rejection", code == 1, code)
                self.check(name + ":before-iocinit", not self.marker(log, BEFORE_INIT) and
                           not self.marker(log, AFTER_INIT) and "Starting iocInit" not in log and
                           "iocRun: All initialization complete" not in log and
                           "snmp3 hook: AfterFinishDevSup" not in log)
                self.check(name + ":before-interactive-shell", not self.marker(log, SHELL))
                self.check(name + ":actual-module-error", "snmp3:" in log and "startup script failed" in log)
                if later:
                    self.check(name + ":earlier-device-observed", self.marker(log, "EARLIER_DEVICE_LOADED"))
                    self.records(name, log, (1,))
            finally:
                for sock in sinks:
                    sock.close()

    def failed_state(self):
        name = "failed-state"
        folder = self.output / name
        folder.mkdir()
        config, sinks = self.configuration(folder)
        try:
            script = self.header(on_error=False) + self.module(config) + self.unit(1)
            script += f'on error continue\nsnmp3WorkerPath("{folder}/missing-worker")\niocInit\nsnmp3RuntimeReport\necho SCRIPT_RETURNED_ZERO\n'
            code, log = self.run_ioc(name, folder, script)
            self.check(name + ":failure-exposed", "state=Failed" in log and self.marker(log, "SCRIPT_RETURNED_ZERO"))
            self.check(name + ":main-rejects", code == 1 and "startup script failed" in log, code)
            self.check(name + ":no-interactive-shell", not self.marker(log, SHELL))
        finally:
            for sock in sinks:
                sock.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apcpdu", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", choices=("all", "valid", "invalid", "failed-state"), default="all")
    parser.add_argument("--control", choices=("none", "continue-after-error"), default="none")
    args = parser.parse_args()
    if args.control != "none" and args.case != "invalid":
        parser.error("--control requires --case invalid")
    output, apc = args.output.resolve(), args.apcpdu.resolve()
    if output.exists() or not output.is_relative_to(ROOT / "work") or apc == ROOT:
        parser.error("--output must be new under snmp3 work/ and --apcpdu must select the installed candidate")
    os.umask(0o077)
    output.mkdir(parents=True, mode=0o700)
    runner, error, code = None, None, 0
    started = time.time_ns()
    try:
        runner = Startup(apc, output)
        result = runner.command("generator-check", [sys.executable, "-B", apc / "scripts/gen_ap8932.py", "db", "--check"])
        runner.check("candidate:generator-files-current", result.returncode == 0)
        runner.check("candidate:installed-db-current", digest(apc / "db/ap8932.db") == digest(apc / "apcpduApp/Db/ap8932.db"))
        if args.case in ("all", "valid"):
            runner.valid()
        if args.case in ("all", "invalid"):
            runner.invalid(args.control != "none")
        if args.case in ("all", "failed-state"):
            runner.failed_state()
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
            for path in output.rglob("*"):
                if path.is_file() and path.suffix in (".log", ".stdout", ".stderr"):
                    data = path.read_bytes()
                    if any(sentinel in data for sentinel in runner.sentinels):
                        error = (error or "") + "\nDisposable secret appeared in captured output"
                        code = 1
            if any(check["passed"] is False for check in runner.checks):
                code = 1
            if code == 0 and any(not p.get("stopped") or p.get("forced") for p in runner.processes):
                code, error = 2, "Owned process did not stop normally"
            write_json(output / "identities.json", runner.identities)
        report = {"argv": sys.argv, "cwd": str(Path.cwd()), "case": args.case, "control": args.control,
                  "status": "PASS" if code == 0 else "FAIL" if code == 1 else "INCOMPLETE", "returncode": code,
                  "checks": runner.checks if runner else [], "processes": runner.processes if runner else [],
                  "error": error, "start_utc_ns": started, "end_utc_ns": time.time_ns(),
                  "limits": ["Installed startup only; no successful remote SNMP communication claim",
                             "No candidate rebuild or modification", "No hardware or full M7 acceptance"]}
        write_json(output / "result.json", report)
    print(f"Evidence: {output}; checks: {len(report['checks'])}; result: {report['status']}")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
