#!/usr/bin/env python3
"""Execute isolated native API preflight with the actual installed agent library."""

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import secrets
import shlex
import signal
import socket
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
ARCH = "linux-x86_64"
ENGINE_ID = "8000000001020304"
USER = "fixtureUser"
VALUE_TYPES = ("integer", "counter32", "gauge32", "timeticks", "counter64", "octets",
               "oid", "ipAddress", "opaqueFloat", "opaqueDouble")
PROCESS_TIMEOUT = 8
READY_TIMEOUT = 5


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def build_sanitizers(output):
    products = output / "products"
    products.mkdir(mode=0o700)
    release = (ROOT / "configure/RELEASE.local").read_text().splitlines()
    base = Path(next(line.split("=", 1)[1].strip() for line in release
                     if line.strip().startswith("EPICS_BASE") and "=" in line))
    common = ["g++", "-std=c++11", "-O1", "-g", "-fPIC", "-fsanitize=address,undefined",
              "-fno-omit-frame-pointer", "-fno-sanitize-recover=all"]
    common += ["-I" + str(path) for path in (ROOT / "snmp3App/src", ROOT / "snmp3App/native",
               base / "include", base / "include/os/Linux", base / "include/compiler/gcc")]
    native_flags = shlex.split(subprocess.check_output(["net-snmp-config", "--cflags"], text=True))
    native_libs = shlex.split(subprocess.check_output(["net-snmp-config", "--libs"], text=True))
    link = ["-L" + str(products), "-L" + str(ROOT / "lib" / ARCH), "-L" + str(base / "lib" / ARCH),
            "-Wl,-rpath,$ORIGIN:" + str(ROOT / "lib" / ARCH) + ":" + str(base / "lib" / ARCH)]
    jobs = [("libsnmp3Native.so", [ROOT / "snmp3App/native" / (name + ".cpp")
             for name in ("Capabilities", "Native", "Worker")], ["-shared"], native_libs),
            ("snmp3NativeProbe", [ROOT / "snmp3App/native/NativeProbe.cpp"], [],
             ["-lsnmp3Native", "-lCom"] + native_libs),
            ("snmp3NativeAgent", [ROOT / "tests/rewrite/NativeAgent.cpp"], [],
             ["-lnetsnmpagent"] + native_libs),
            ("snmp3NativeApiProbe", [ROOT / "tests/rewrite/NativeApiProbe.cpp"], [], native_libs),
            ("snmp3NativeTest", [ROOT / "tests/rewrite/NativeTest.cpp"], [],
             ["-lsnmp3Native", "-lsnmp3", "-lCom"] + native_libs)]
    receipts = []
    for name, sources, flags, libraries in jobs:
        argv = common + native_flags + flags + list(map(str, sources)) + link + libraries + ["-o", str(products / name)]
        with (output / (name + ".build.stdout")).open("wb") as stdout:
            with (output / (name + ".build.stderr")).open("wb") as stderr:
                child = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
                code = child.wait()
        receipt = {"argv": argv, "returncode": code, "pid": child.pid, "child_reaped": True,
                   "sources": {str(path): digest(path) for path in sources}}
        if not code:
            receipt["product_sha256"] = digest(products / name)
            symbols = subprocess.check_output(["nm", "-D", str(products / name)], text=True)
            receipt["asan_imports"] = "__asan_" in symbols
            receipt["ubsan_imports"] = "__ubsan_" in symbols
        receipts.append(receipt)
        write_json(output / "sanitizer-build.json", {"products": receipts,
                   "coverage": "New native library and native drivers/fixtures; dependencies uninstrumented; leak detection disabled"})
        if code or not receipt.get("asan_imports") or not receipt.get("ubsan_imports"):
            raise RuntimeError("instrumented native product build failed")
    return products


class Runner:
    def __init__(self, output, products=None, sanitizers=False):
        self.output = output
        self.checks = []
        self.children = []
        self.tokens = []
        self.env = dict(os.environ, MIBS="", LD_DEBUG="libs")
        for key in ("LD_PRELOAD", "LD_AUDIT", "LD_DEBUG_OUTPUT", "LD_LIBRARY_PATH"):
            self.env.pop(key, None)
        if sanitizers:
            self.env.update(ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
                            UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        self.products = products or ROOT / "bin" / ARCH
        self.identities = {}
        self.catalogue = None
        self.agent_count = 0
        self.stopped = set()

    def check(self, name, observed):
        self.checks.append({"name": name, "passed": bool(observed)})

    def secret(self, path):
        token = secrets.token_hex(24)
        with open(path, "x", opener=lambda p, flags: os.open(p, flags, 0o600)) as stream:
            stream.write(token + "\n")
        self.tokens.append(token.encode())
        return token

    def invoke(self, name, argv, expected=0, ready_hook=None, pass_fds=()):
        start = time.monotonic_ns()
        with (self.output / (name + ".stdout")).open("wb") as stdout:
            with (self.output / (name + ".stderr")).open("wb") as stderr:
                child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=self.env, pass_fds=pass_fds)
                forced = False
                error_class = None
                try:
                    if ready_hook:
                        ready_hook(child, self.output / (name + ".stdout"))
                    code = child.wait(timeout=PROCESS_TIMEOUT)
                except subprocess.TimeoutExpired:
                    forced = True
                    child.kill()
                    code = child.wait(timeout=PROCESS_TIMEOUT)
                except Exception as error:
                    error_class = type(error).__name__
                    forced = True
                    if child.poll() is None:
                        child.kill()
                    code = child.wait(timeout=PROCESS_TIMEOUT)
        receipt = {"argv": list(map(str, argv)), "returncode": code,
                   "passed_descriptors": list(pass_fds),
                   "elapsed_ns": time.monotonic_ns() - start, "pid": child.pid,
                   "child_reaped": child.returncode is not None, "forced_cleanup": forced,
                   "environment_overrides": {"MIBS": "", "LD_DEBUG": "libs"}, "hook_error_class": error_class}
        receipt["loaded_libraries"] = self.loader_identity(self.output / (name + ".stderr"))
        receipt["environment_removed"] = ["LD_PRELOAD", "LD_AUDIT", "LD_DEBUG_OUTPUT", "LD_LIBRARY_PATH"]
        receipt["sanitizer_options"] = {key: self.env[key] for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS") if key in self.env}
        write_json(self.output / (name + ".receipt.json"), receipt)
        self.check(name + ":return", code == expected and not forced and not error_class)
        raw = (self.output / (name + ".stdout")).read_text()
        events = []
        for line in raw.splitlines():
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                self.check(name + ":json-output", False)
        self.loader_identity(self.output / (name + ".stderr"))
        if error_class:
            raise RuntimeError("test-parent coordination failed")
        return events

    def restart_hook(self, entry, marker):
        def perform(driver, output):
            deadline = time.monotonic() + READY_TIMEOUT
            while time.monotonic() < deadline and driver.poll() is None:
                if b'"event":"restart_ready"' in output.read_bytes():
                    break
                time.sleep(0.01)
            else:
                raise RuntimeError("driver restart marker not observed")
            self.stop_entry(entry)
            old_name, _, _, _, argv = entry
            with open(argv[3], "a") as config:
                engine = argv[4] if len(argv) == 5 else ENGINE_ID
                config.write(f"oldEngineID 0x{engine}\nengineBoots 2\n")
            name = old_name + "-restart"
            stdout = (self.output / (name + ".stdout")).open("wb")
            stderr = (self.output / (name + ".stderr")).open("wb")
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=self.env)
            self.children.append((name, child, stdout, stderr, argv))
            deadline = time.monotonic() + READY_TIMEOUT
            while time.monotonic() < deadline and child.poll() is None:
                if b'"event":"agent_ready"' in (self.output / (name + ".stdout")).read_bytes():
                    marker.write_text("ready\n")
                    return
                time.sleep(0.01)
            raise RuntimeError("actual restarted agent unavailable")
        return perform

    def loader_identity(self, path):
        identities = {}
        for line in path.read_text(errors="replace").splitlines():
            if "calling init:" in line:
                library = Path(line.split("calling init:", 1)[1].strip()).resolve()
                if library.is_file():
                    self.identities[str(library)] = digest(library)
                    identities[str(library)] = self.identities[str(library)]
        return identities

    def agent(self, family, engine=ENGINE_ID):
        protocol = socket.AF_INET if family == 4 else socket.AF_INET6
        address = "127.0.0.1" if family == 4 else "::1"
        with socket.socket(protocol, socket.SOCK_DGRAM) as reserved:
            reserved.bind((address, 0))
            port = reserved.getsockname()[1]
        self.agent_count += 1
        name = "agent-ipv" + str(family) + "-" + str(self.agent_count)
        paths = {key: self.output / (name + "-" + key) for key in ("community", "auth", "privacy")}
        material = {key: self.secret(path) for key, path in paths.items()}
        config = self.output / (name + ".conf")
        text = (
            f"rwcommunity {material['community']} 127.0.0.1 .1.3.6.1.4.1.53864\n"
            f"rwcommunity6 {material['community']} ::1 .1.3.6.1.4.1.53864\n"
            f"createUser {USER} SHA {material['auth']} AES {material['privacy']}\n"
            f"group nativeGroup usm {USER}\n"
            "view nativeView included .1.3.6.1.4.1.53864\n"
            "access nativeGroup \"\" usm noauth prefix nativeView nativeView none\n"
        )
        text += (f"createUser independentFixtureUser SHA {material['auth']} AES {material['privacy']}\n"
                 "group nativeGroup usm independentFixtureUser\n")
        if self.catalogue:
            for ai, auth in enumerate(self.catalogue["authentication"]):
                for pi, privacy in enumerate(self.catalogue["privacy"]):
                    user = f"algorithmUser{ai}_{pi}"
                    text += (f"createUser {user} {auth['name']} {material['auth']} "
                             f"{privacy['name']} {material['privacy']}\n"
                             f"group nativeGroup usm {user}\n")
        with open(config, "x", opener=lambda p, flags: os.open(p, flags, 0o600)) as stream:
            stream.write(text)
        argv = [str(self.products / "snmp3NativeAgent"), str(port), str(family), str(config)]
        if engine != ENGINE_ID:
            argv.append(engine)
        stdout = (self.output / (name + ".stdout")).open("wb")
        stderr = (self.output / (name + ".stderr")).open("wb")
        child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=self.env)
        entry = (name, child, stdout, stderr, argv)
        self.children.append(entry)
        deadline = time.monotonic() + READY_TIMEOUT
        ready = False
        while time.monotonic() < deadline and child.poll() is None:
            if b'"event":"agent_ready"' in (self.output / (name + ".stdout")).read_bytes():
                ready = True
                break
            time.sleep(0.01)
        self.check(name + ":bind-ready", ready)
        if not ready:
            raise RuntimeError("native agent did not become ready")
        socket_inodes = set()
        for fd in (Path("/proc") / str(child.pid) / "fd").iterdir():
            try:
                target = os.readlink(fd)
            except FileNotFoundError:
                continue
            if target.startswith("socket:["):
                socket_inodes.add(target[8:-1])
        listeners = []
        for table in ("udp", "udp6", "tcp", "tcp6"):
            for line in (Path("/proc") / str(child.pid) / "net" / table).read_text().splitlines()[1:]:
                fields = line.split()
                if fields[9] in socket_inodes:
                    listeners.append({"table": table, "local": fields[1],
                                      "state": fields[3], "inode": fields[9]})
        wanted_address = "0100007F" if family == 4 else "00000000000000000000000001000000"
        wanted_local = f"{wanted_address}:{port:04X}"
        self.check(name + ":only-owned-loopback-listener", len(listeners) == 1 and
                   listeners[0]["table"] == ("udp" if family == 4 else "udp6") and
                   listeners[0]["local"] == wanted_local)
        write_json(self.output / (name + ".sockets.json"), {"pid": child.pid, "sockets": listeners})
        peer = f"udp:{address}:{port}" if family == 4 else f"udp6:[{address}]:{port}"
        return peer, paths

    def probe(self, name, mode, peer, version, paths, user=USER, expected=0):
        argv = [str(self.products / "snmp3NativeApiProbe"), mode, peer, str(version),
                str(paths["community"]), str(paths["auth"]), str(paths["privacy"]), user]
        return self.invoke(name, argv, expected)

    def fault(self, name, family, peer, mode, delay_ms=None):
        argv = [sys.executable, str(ROOT / "tests/rewrite/helpers/udp_fault.py"), "--family", str(family),
                "--agent-port", peer.rsplit(":", 1)[1], "--mode", mode]
        if delay_ms is not None:
            argv += ["--delay-ms", str(delay_ms)]
        stdout = (self.output / (name + ".stdout")).open("wb")
        stderr = (self.output / (name + ".stderr")).open("wb")
        child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=self.env)
        entry = (name, child, stdout, stderr, argv)
        self.children.append(entry)
        end = time.monotonic() + READY_TIMEOUT
        port = None
        while time.monotonic() < end and child.poll() is None:
            for line in (self.output / (name + ".stdout")).read_text().splitlines():
                event = json.loads(line)
                if event.get("event") == "fault_ready":
                    port = event["port"]
            if port:
                break
            time.sleep(0.01)
        self.check(name + ":ready", bool(port))
        if not port:
            raise RuntimeError("UDP fault process unavailable")
        host = "127.0.0.1" if family == 4 else "::1"
        target = f"udp:{host}:{port}" if family == 4 else f"udp6:[{host}]:{port}"
        return target, entry

    def stop(self):
        for entry in reversed(self.children):
            self.stop_entry(entry)

    def stop_entry(self, entry):
        name, child, stdout, stderr, argv = entry
        if name in self.stopped:
            return
        forced = False
        if child.poll() is None:
            child.send_signal(signal.SIGTERM)
        try:
            code = child.wait(timeout=PROCESS_TIMEOUT)
        except subprocess.TimeoutExpired:
            forced = True
            child.kill()
            code = child.wait(timeout=PROCESS_TIMEOUT)
        stdout.close()
        stderr.close()
        write_json(self.output / (name + ".receipt.json"), {
            "argv": argv, "pid": child.pid, "returncode": code,
            "child_reaped": child.returncode is not None, "forced_cleanup": forced,
            "environment_overrides": {"MIBS": "", "LD_DEBUG": "libs"}})
        self.check(name + ":normal-stop", code == 0 and not forced)
        self.loader_identity(self.output / (name + ".stderr"))
        self.stopped.add(name)

    def scan(self):
        recorded = [p for p in self.output.iterdir()
                    if p.suffix in (".stdout", ".stderr", ".json")]
        leaks = [p.name for p in recorded if any(token in p.read_bytes() for token in self.tokens)]
        self.check("secret-sentinels-absent", not leaks)
        sanitizer_errors = [p.name for p in recorded if p.suffix == ".stderr" and any(
            marker in p.read_bytes() for marker in (b"ERROR: AddressSanitizer", b"runtime error:",
                                                   b"AddressSanitizer:DEADLYSIGNAL"))]
        self.check("sanitizer-diagnostics-absent", not sanitizer_errors)


def summary(events):
    return next((entry for entry in events if entry.get("event") == "summary"), {})


def preflight(runner):
    for family in (4, 6):
        peer, paths = runner.agent(family)
        for version in (1, 2, 3):
            name = f"ipv{family}-v{version}-get"
            events = runner.probe(name, "get", peer, version, paths)
            observed = summary(events)
            runner.check(name + ":real-value", observed.get("received") == 1 and observed.get("value") == -123)
            runner.check(name + ":normal-close", any(e.get("event") == "native_close" and e.get("return") == 1 for e in events))
            if version == 3:
                runner.check(name + ":discovery-before-credentials", any(
                    e.get("event") == "discovery" and e.get("candidate_user_absent") is True
                    and e.get("engine_length") == 8 for e in events))
        for mode in ("send-fail", "pending-close", "return-zero", "high-fd"):
            name = f"ipv{family}-{mode}"
            events = runner.probe(name, mode, peer, 2, paths)
            observed = summary(events)
            if mode == "send-fail":
                runner.check(name + ":caller-ownership", any(e.get("event") == "caller_pdu_freed" for e in events))
                runner.check(name + ":native-failure", observed.get("failed") == 1 and observed.get("received") == 0)
            elif mode == "pending-close":
                runner.check(name + ":close-callback", observed.get("timeout") == 1 and observed.get("received") == 0)
            elif mode == "return-zero":
                runner.check(name + ":callback-retains-request", observed.get("received") == 2 and observed.get("resend", 0) >= 1)
            else:
                runner.check(name + ":real-high-socket", any(e.get("event") == "socket_interest"
                    and e.get("nfds", 0) > e.get("fd_set_size", 0) for e in events))
                runner.check(name + ":large-fd-value", observed.get("received") == 1 and observed.get("value") == -123)
        name = f"ipv{family}-warm-discovery"
        events = runner.probe(name, "warm-discovery", peer, 3, paths)
        runner.check(name + ":keys-preserved", any(e.get("event") == "warm_discovery" and
                     e.get("cached_material_preserved") is True for e in events))
        runner.check(name + ":real-value", summary(events).get("value") == -123 and
                     summary(events).get("received") == 1)
        name = f"ipv{family}-security-report"
        events = runner.probe(name, "get", peer, 3, paths, user="absentFixtureUser")
        runner.check(name + ":real-security-callback", summary(events).get("security") == 1)
        runner.check(name + ":report-is-not-response", any(e.get("event") == "native_callback" and
                     e.get("command") == 168 for e in events))
        for key in ("auth", "privacy"):
            name = f"ipv{family}-invalid-{key}"
            altered = dict(paths)
            altered[key] = runner.output / (name + ".secret")
            runner.secret(altered[key])
            events = runner.probe(name, "get", peer, 3, altered)
            runner.check(name + ":no-successful-response", not any(e.get("event") == "native_callback"
                         and e.get("command") == 162 for e in events))
            runner.check(name + ":native-terminal", summary(events).get("timeout", 0) +
                         summary(events).get("security", 0) + summary(events).get("failed", 0) >= 1)
        unused_family = socket.AF_INET if family == 4 else socket.AF_INET6
        unused_host = "127.0.0.1" if family == 4 else "::1"
        with socket.socket(unused_family, socket.SOCK_DGRAM) as unused:
            unused.bind((unused_host, 0))
            unused_port = unused.getsockname()[1]
            unreachable = (f"udp:{unused_host}:{unused_port}" if family == 4 else
                           f"udp6:[{unused_host}]:{unused_port}")
            name = f"ipv{family}-timeout"
            events = runner.probe(name, "get", unreachable, 2, paths)
            runner.check(name + ":native-retry-timeout", summary(events).get("timeout") == 1 and
                         summary(events).get("resend") == 1 and summary(events).get("received") == 0)
            name = f"ipv{family}-discovery-failure"
            events = runner.probe(name, "get", unreachable, 3, paths, expected=1)
            runner.check(name + ":native-open-failure", any(e.get("event") == "discovery_open" and
                         e.get("success") is False for e in events))
        name = f"ipv{family}-open-failure"
        events = runner.probe(name, "get", "unavailable-transport:127.0.0.1:1", 2, paths, expected=1)
        runner.check(name + ":native-open-failure", any(e.get("event") == "session_open" and
                     e.get("success") is False for e in events))


def adapter_configuration(runner, name, family, peer, paths, version=2, level="authPriv", context=""):
    port = int(peer.rsplit(":", 1)[1])
    profile = {"id": "Main", "version": str(version) if version != 2 else "2c",
               "timeoutMs": 100, "retries": 1, "maxVarbinds": 16}
    if version != 3:
        profile["communityFile"] = str(paths["community"])
    else:
        profile.update(user=USER, securityLevel=level, contextName=context)
        if level != "noAuthNoPriv":
            profile.update(authAlgorithm="SHA", authSecretFile=str(paths["auth"]))
        if level == "authPriv":
            profile.update(privAlgorithm="AES", privSecretFile=str(paths["privacy"]))
    document = {"schema": 1, "profiles": [profile], "endpoints": [
        {"id": "Main", "address": "127.0.0.1" if family == 4 else "::1", "port": port, "profile": "Main"}],
        "bindings": []}
    for index, value_type in enumerate(VALUE_TYPES + ("integer",) * 8, 1):
        for operation in ("get", "set"):
            document["bindings"].append({"id": "Main_" + ("G" if operation == "get" else "S") + str(index),
                "endpoint": "Main", "oid": f"1.3.6.1.4.1.53864.4.{index}.0", "operation": operation,
                "valueType": value_type, "capacity": 64 if value_type in ("octets", "oid") else 1})
    path = runner.output / (name + ".config.json")
    write_json(path, document)
    return path, document


def second_endpoint(document, context=None):
    profile = copy.deepcopy(document["profiles"][0])
    profile["id"] = "Second"
    if context is not None:
        profile["contextName"] = context
    document["profiles"].append(profile)
    endpoint = copy.deepcopy(document["endpoints"][0])
    endpoint.update(id="Second", profile="Second")
    document["endpoints"].append(endpoint)
    bindings = copy.deepcopy(document["bindings"])
    for binding in bindings:
        binding.update(id=binding["id"].replace("Main_", "Second_"), endpoint="Second")
    document["bindings"].extend(bindings)
    return profile


def expected_values(context=0):
    data = [-123 - context, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
            [65, 0, 66, 255, context], [1, 3, 6, 1, 4, 1, 53864], [127, 0, 0, 1],
            struct.unpack("=I", struct.pack("=f", -2.25))[0],
            struct.unpack("=Q", struct.pack("=d", 1 / 3))[0]]
    return [{"type": index if index == 0 else index + 1, "data": value}
            for index, value in enumerate(data)]


def adapter(runner):
    helper = str(runner.products / "snmp3NativeProbe")
    driver = str(runner.products / "snmp3NativeTest")
    runner.catalogue = runner.invoke("adapter-capabilities", [helper, "--capabilities"])[0]
    for family in (4, 6):
        peer, paths = runner.agent(family)
        for version, level in ((1, ""), (2, ""), (3, "noAuthNoPriv"), (3, "authNoPriv"), (3, "authPriv")):
            name = f"adapter-ipv{family}-v{version}-{level or 'community'}"
            config, _ = adapter_configuration(runner, name, family, peer, paths, version, level)
            events = runner.invoke(name, [driver, "get", str(config), helper, "Main"])
            results = [e for e in events if e.get("event") == "result"]
            runner.check(name + ":owned-value", len(results) == 1 and results[0].get("outcome") == 0 and
                         results[0].get("values") == [{"type": 0, "data": -123}])
            runner.check(name + ":one-terminal", sum(e.get("event") == "terminal" for e in events) == 1)
            if version == 3:
                runner.check(name + ":resolved-identity", any(e.get("event") == "identity" and
                    e.get("security_engine") == [128, 0, 0, 0, 1, 2, 3, 4] and
                    e.get("context_engine") == e.get("security_engine") for e in events))
        for mode in ("pending-close", "reentry", "duplicate-oid", "invalid", "high-fd", "typed"):
            name = f"adapter-ipv{family}-{mode}"
            config, _ = adapter_configuration(runner, name, family, peer, paths)
            events = runner.invoke(name, [driver, mode, str(config), helper, "Main"])
            results = [e for e in events if e.get("event") == "result"]
            if mode == "pending-close":
                runner.check(name + ":cancelled-once", len(results) == 1 and results[0].get("outcome") == 9)
                runner.check(name + ":native-close-once", sum(e.get("event") == "native_close" for e in events) == 1)
            elif mode == "reentry":
                runner.check(name + ":two-owned-completions", len(results) == 2 and all(e.get("outcome") == 0 for e in results))
            elif mode == "duplicate-oid":
                runner.check(name + ":positional-values", len(results) == 1 and
                    results[0].get("values") == [{"type": 0, "data": -123}] * 2)
            elif mode == "invalid":
                runner.check(name + ":bounds-rejected", sum(e.get("event") == "batch_rejected" and
                    e.get("rejected") is True for e in events) == 2)
                runner.check(name + ":exact-max-batch", len(results) == 1 and len(results[0].get("values", [])) == 16)
            elif mode == "high-fd":
                runner.check(name + ":real-large-fd", any(e.get("event") == "socket_descriptor" and
                    e.get("detail", 0) >= 1024 for e in events))
                runner.check(name + ":real-value", len(results) == 1 and results[0].get("outcome") == 0)
            else:
                runner.check(name + ":typed-get-set-get", len(results) == 3 and
                    all(e.get("outcome") == 0 and e.get("values") == expected_values() for e in results))
                retained = [e for e in events if e.get("event") == "retained_result"]
                runner.check(name + ":retained-after-pdu-release", len(retained) == 1 and
                    len(results) == 3 and retained[0].get("values") == results[0].get("values") == results[2].get("values"))
        name = f"adapter-ipv{family}-high-fd-timeout"
        proxy, child = runner.fault(name + "-proxy", family, peer, "drop")
        config, _ = adapter_configuration(runner, name, family, proxy, paths)
        events = runner.invoke(name, [driver, "high-fd", str(config), helper, "Main"])
        runner.stop_entry(child)
        results = [e for e in events if e.get("event") == "result"]
        runner.check(name + ":actual-socket-and-timeout", len(results) == 1 and results[0].get("outcome") == 1 and
            any(e.get("event") == "socket_descriptor" and e.get("detail", 0) >= 1024 for e in events))
        name = f"adapter-ipv{family}-unsigned-alias"
        config, document = adapter_configuration(runner, name, family, peer, paths)
        for binding in document["bindings"]:
            if binding["id"] in ("Main_G3", "Main_S3"):
                binding["valueType"] = "unsigned32"
        write_json(config, document)
        events = runner.invoke(name, [driver, "unsigned-alias", str(config), helper, "Main"])
        results = [e for e in events if e.get("event") == "result"]
        runner.check(name + ":actual-gauge-wire-with-expected-tag", len(results) == 3 and all(
            e.get("outcome") == 0 and e.get("values") == [{"type": 1, "data": 0xFFFFFFFF}] for e in results))
        for ai, auth in enumerate(runner.catalogue["authentication"]):
            for pi, privacy in enumerate(runner.catalogue["privacy"]):
                name = f"adapter-ipv{family}-algorithm-{ai}-{pi}"
                config, document = adapter_configuration(runner, name, family, peer, paths, 3)
                document["profiles"][0].update(user=f"algorithmUser{ai}_{pi}",
                    authAlgorithm=auth["name"], privAlgorithm=privacy["name"])
                write_json(config, document)
                events = runner.invoke(name, [driver, "get", str(config), helper, "Main"])
                results = [e for e in events if e.get("event") == "result"]
                runner.check(name + ":actual-advertised-pair", len(results) == 1 and
                    results[0].get("outcome") == 0 and results[0].get("values") == expected_values()[:1])
                runner.check(name + ":native-derived-key-match", any(e.get("event") == "native_material_matches"
                    and e.get("detail") == 1 for e in events))
        for explicit in (False, True):
            for difference in ("auth-secret", "privacy-secret", "auth-algorithm", "privacy-algorithm", "security-level"):
                for reverse in ((False, True) if difference.endswith("secret") else (False,)):
                    name = f"adapter-ipv{family}-conflict-{difference}-explicit{int(explicit)}-reverse{int(reverse)}"
                    proxy, child = runner.fault(name + "-proxy", family, peer, "pass")
                    config, document = adapter_configuration(runner, name, family, proxy, paths, 3)
                    if explicit:
                        document["profiles"][0]["securityEngineId"] = ENGINE_ID
                    second = second_endpoint(document)
                    if difference.endswith("secret"):
                        key = "authSecretFile" if difference.startswith("auth") else "privSecretFile"
                        secret_path = runner.output / (name + ".secret")
                        runner.secret(secret_path)
                        (document["profiles"][0] if reverse else second)[key] = str(secret_path)
                    elif difference == "auth-algorithm":
                        second["authAlgorithm"] = "SHA-256"
                    elif difference == "privacy-algorithm":
                        second["privAlgorithm"] = "AES-256"
                    else:
                        second["securityLevel"] = "authNoPriv"
                        del second["privAlgorithm"]
                        del second["privSecretFile"]
                    write_json(config, document)
                    events = runner.invoke(name, [driver, "conflict", str(config), helper, "Main", "Second"])
                    runner.stop_entry(child)
                    packets = [json.loads(line) for line in
                        (runner.output / (name + "-proxy.stdout")).read_text().splitlines()]
                    runner.check(name + ":actual-application-packet-count", sum(
                        e.get("event") == "fault_request" and not e.get("discovery") for e in packets)
                        == 1 + sum(e.get("event") == "native_callback" and e.get("operation") == 6
                                   for e in events) and sum(e.get("event") == "native_send" for e in events) == 1)
                    errors = [e for e in events if e.get("event") in ("open_error", "reopen_error")]
                    runner.check(name + ":conflict-pinned-through-last-close", len(errors) == 2 and
                        all(e.get("outcome") == 6 for e in errors))
                    runner.check(name + ":one-profile-open", sum(e.get("event") == "native_open" for e in events) == 1)
                    runner.check(name + ":pinned-native-keys-preserved", sum(e.get("event") == "pinned_material_preserved"
                        and e.get("detail") == 1 for e in events) == 2)
                    results = [e for e in events if e.get("event") == "result"]
                    runner.check(name + ":first-profile-real-path", len(results) == 1 and
                        results[0].get("outcome") in ((1, 4) if reverse else (0,)))
        for contexts in (("", ""), ("alpha", "beta")):
            name = f"adapter-ipv{family}-compatible-{contexts[0] or 'empty'}"
            config, document = adapter_configuration(runner, name, family, peer, paths, 3, context=contexts[0])
            second_endpoint(document, contexts[1])
            write_json(config, document)
            events = runner.invoke(name, [driver, "compatible", str(config), helper, "Main", "Second"])
            results = [e for e in events if e.get("event") == "result"]
            runner.check(name + ":distinct-compatible-context-values", len(results) == 5 and
                all(e.get("outcome") == 0 for e in results) and
                sorted(e["values"][0]["data"] for e in results) == sorted(
                    [-123 - (1 if contexts[0] else 0)] * 3 + [-123 - (2 if contexts[1] else 0)] * 2))
        name = f"adapter-ipv{family}-different-user"
        config, document = adapter_configuration(runner, name, family, peer, paths, 3)
        second_endpoint(document)["user"] = "independentFixtureUser"
        write_json(config, document)
        events = runner.invoke(name, [driver, "compatible", str(config), helper, "Main", "Second"])
        runner.check(name + ":independent-user-success", sum(e.get("event") == "result" and
            e.get("outcome") == 0 for e in events) == 5)
        other_peer, other_paths = runner.agent(family, engine="8000000001020305")
        name = f"adapter-ipv{family}-different-engine"
        config, document = adapter_configuration(runner, name, family, peer, paths, 3)
        second = second_endpoint(document)
        second.update(authSecretFile=str(other_paths["auth"]), privSecretFile=str(other_paths["privacy"]))
        document["endpoints"][1]["port"] = int(other_peer.rsplit(":", 1)[1])
        write_json(config, document)
        events = runner.invoke(name, [driver, "compatible", str(config), helper, "Main", "Second"])
        runner.check(name + ":independent-engine-success", sum(e.get("event") == "result" and
            e.get("outcome") == 0 for e in events) == 5 and len({tuple(e["security_engine"])
            for e in events if e.get("event") == "identity"}) == 2)
        for mode in ("open-failure", "send-failure", "security-report", "explicit-context-engine", "value-boundaries"):
            name = f"adapter-ipv{family}-{mode}"
            config, document = adapter_configuration(runner, name, family, peer, paths,
                version=3 if mode in ("security-report", "explicit-context-engine") else 2)
            case = mode
            if mode == "send-failure":
                for binding in document["bindings"]:
                    if binding["id"] == "Main_S6":
                        binding["capacity"] = 70000
            elif mode == "security-report":
                document["profiles"][0]["user"] = "absentFixtureUser"
                case = "get"
            elif mode == "explicit-context-engine":
                document["profiles"][0].update(securityEngineId=ENGINE_ID,
                    contextEngineId="8000000001020306", contextName="alpha")
                case = "get"
            elif mode == "value-boundaries":
                document["profiles"][0]["checkRanges"] = False
            write_json(config, document)
            events = runner.invoke(name, [driver, case, str(config), helper, "Main"],
                expected=1 if mode == "open-failure" else 0)
            results = [e for e in events if e.get("event") == "result"]
            if mode == "open-failure":
                runner.check(name + ":actual-native-open-failure", any(e.get("event") == "native_open" and
                    e.get("detail") == 0 for e in events))
            elif mode == "send-failure":
                runner.check(name + ":native-caller-ownership", len(results) == 1 and results[0].get("outcome") == 3 and
                    sum(e.get("event") == "caller_pdu_freed" for e in events) == 1 and
                    any(e.get("event") == "native_send" and e.get("detail") == 0 for e in events))
            elif mode == "security-report":
                runner.check(name + ":native-report-one-terminal", len(results) == 1 and results[0].get("outcome") == 4 and
                    sum(e.get("event") == "native_callback" for e in events) >= 2 and
                    sum(e.get("event") == "terminal" for e in events) == 1)
            elif mode == "explicit-context-engine":
                runner.check(name + ":independent-explicit-identities", len(results) == 1 and
                    results[0].get("values") == [{"type": 0, "data": -124}] and any(e.get("event") == "identity" and
                    e.get("security_engine") == [128, 0, 0, 0, 1, 2, 3, 4] and
                    e.get("context_engine") == [128, 0, 0, 0, 1, 2, 3, 6] for e in events))
            else:
                zero = [{"type": entry["type"], "data": 0} for entry in expected_values()]
                zero[0]["data"] = -2147483648
                zero[5]["data"] = []
                zero[6]["data"] = [0, 0]
                zero[7]["data"] = [0, 0, 0, 0]
                zero[8]["data"] = 0x80000000
                zero[9]["data"] = 0x8000000000000000
                high = expected_values()
                high[0]["data"] = 2147483647
                high[6]["data"] = [2, 0xFFFFFFFF - 80, 0xFFFFFFFF]
                high[8]["data"] = 0x7FC12345
                high[9]["data"] = 0x7FF8123456789ABC
                infinity = copy.deepcopy(high)
                infinity[8]["data"] = 0x7F800000
                infinity[9]["data"] = 0x7FF0000000000000
                runner.check(name + ":actual-zero-max-binary-floating-bits", len(results) == 7 and
                    [e.get("values") for e in results] == [zero, zero, high, high, infinity, infinity, expected_values()])
                runner.check(name + ":owned-first-result-survives", any(e.get("event") == "retained_result" and
                    e.get("values") == zero for e in events))
                runner.check(name + ":actual-native-oid-limit", any(e.get("event") == "native_oid_limit" and
                    e.get("rejected_before_dispatch") is True for e in events))
        for difference in ("version", "type", "oid"):
            name = f"adapter-ipv{family}-capability-{difference}"
            config, _ = adapter_configuration(runner, name, family, peer, paths)
            wrapper = runner.output / (name + ".helper")
            statement = {"version": "catalogue['version'] += '-external-difference'",
                         "type": "catalogue['authentication'][0]['type'] += 100",
                         "oid": "catalogue['authentication'][0]['oid'] += '.99'"}[difference]
            wrapper.write_text("#!/usr/bin/python3\nimport json, subprocess, sys\n"
                f"child = subprocess.Popen([{helper!r}, '--capabilities'], stdout=subprocess.PIPE, text=True)\n"
                "output, _ = child.communicate(timeout=5)\n"
                "print(json.dumps({'event':'external_helper_child_reaped','pid':child.pid,'returncode':child.returncode}), file=sys.stderr)\n"
                "if child.returncode: sys.exit(child.returncode)\n"
                "catalogue = json.loads(output)\n" + statement + "\nprint(json.dumps(catalogue))\n")
            wrapper.chmod(0o700)
            events = runner.invoke(name, [driver, "get", str(config), str(wrapper), "Main"], expected=1)
            runner.check(name + ":rejected-before-session-discovery", any(e.get("event") == "driver_error" and
                e.get("outcome") == 7 for e in events) and not any(e.get("event") in
                ("native_open", "discovery_open", "native_send") for e in events))
        name = f"adapter-ipv{family}-mixed-deadlines"
        proxy, child = runner.fault(name + "-proxy", family, peer, "drop")
        config, document = adapter_configuration(runner, name, family, proxy, paths)
        document["profiles"][0]["timeoutMs"] = 60
        second_endpoint(document)["timeoutMs"] = 20
        document["endpoints"][1]["port"] = int(peer.rsplit(":", 1)[1])
        write_json(config, document)
        events = runner.invoke(name, [driver, "mixed-deadlines", str(config), helper, "Main", "Second"])
        runner.stop_entry(child)
        results = [e for e in events if e.get("event") == "result"]
        runner.check(name + ":deadlines-during-peer-traffic", sum(e.get("outcome") == 1 for e in results) == 1 and
            sum(e.get("outcome") == 0 for e in results) >= 2 and all(e.get("outcome") in (0, 1) for e in results))
        name = f"adapter-ipv{family}-set-loss"
        proxy, child = runner.fault(name + "-proxy", family, peer, "drop")
        config, _ = adapter_configuration(runner, name, family, proxy, paths)
        events = runner.invoke(name, [driver, "set-once", str(config), helper, "Main"])
        runner.stop_entry(child)
        packets = [json.loads(line) for line in (runner.output / (name + "-proxy.stdout")).read_text().splitlines()]
        results = [e for e in events if e.get("event") == "result"]
        runner.check(name + ":native-retransmission-without-application-replay", len(results) == 1 and
            results[0].get("outcome") == 1 and sum(e.get("event") == "native_send" for e in events) == 1 and
            sum(e.get("event") == "fault_request" and e.get("command") == 163 for e in packets) == 2)
        for condition in ("type", "capacity"):
            name = f"adapter-ipv{family}-response-{condition}"
            config, document = adapter_configuration(runner, name, family, peer, paths)
            target = next(b for b in document["bindings"] if b["id"] == "Main_G1")
            if condition == "type":
                target["valueType"] = "counter32"
            else:
                target.update(oid="1.3.6.1.4.1.53864.4.6.0", valueType="octets", capacity=1)
            write_json(config, document)
            events = runner.invoke(name, [driver, "get", str(config), helper, "Main"])
            results = [e for e in events if e.get("event") == "result"]
            runner.check(name + ":real-response-storage-rejected", len(results) == 1 and
                results[0].get("outcome") == 8 and results[0].get("values") == [])
        for mode in ("errors", "exceptions", "integer-boundaries"):
            name = f"adapter-ipv{family}-{mode}"
            config, _ = adapter_configuration(runner, name, family, peer, paths)
            events = runner.invoke(name, [driver, mode, str(config), helper, "Main"])
            results = [e for e in events if e.get("event") == "result"]
            if mode == "errors":
                runner.check(name + ":pdu-status-index", len(results) == 2 and
                    [e.get("status") for e in results] == [1, 5] and
                    all(e.get("outcome") == 5 and e.get("index") in (0, 1) for e in results))
            elif mode == "exceptions":
                runner.check(name + ":exception-tags", len(results) == 3 and
                    [e.get("values") for e in results] == [[{"type": tag, "data": None}] for tag in (11, 12, 13)])
            else:
                runner.check(name + ":wire-integer-boundaries", len(results) == 3 and
                    [e.get("values") for e in results[:2]] == [[{"type": 0, "data": n}] for n in (-2147483648, 2147483647)] and
                    results[2].get("outcome") == 8)
        for mode in ("drop", "delay", "duplicate", "reorder", "missing", "extra", "duplicate-varbind",
                     "malformed", "wrong-request-id", "foreign-report"):
            trials = (0, 1, 3) if mode == "drop" else (1,)
            for retries in trials:
                name = f"adapter-ipv{family}-fault-{mode}-{retries}"
                proxy, child = runner.fault(name + "-proxy", family, peer, mode)
                config, document = adapter_configuration(runner, name, family, proxy, paths,
                    version=3 if mode == "foreign-report" else 2)
                document["profiles"][0]["retries"] = retries
                write_json(config, document)
                case = "batch" if mode in ("reorder", "missing", "extra", "duplicate-varbind") else "settle"
                if mode == "delay":
                    case = "interrupted"
                events = runner.invoke(name, [driver, case, str(config), helper, "Main"])
                runner.stop_entry(child)
                packets = [json.loads(line) for line in (runner.output / (name + "-proxy.stdout")).read_text().splitlines()]
                results = [e for e in events if e.get("event") == "result"]
                expected = 1 if mode == "drop" else 8 if mode in ("reorder", "missing", "extra", "duplicate-varbind") else 0
                runner.check(name + ":one-terminal-real-disposition", len(results) == 1 and results[0].get("outcome") == expected)
                runner.check(name + ":native-only-application-submit", sum(e.get("event") == "native_send" for e in events) == 1)
                if mode == "drop":
                    runner.check(name + ":actual-native-retries", sum(e.get("event") == "native_callback" and
                        e.get("operation") == 6 for e in events) == retries and
                        sum(e.get("event") == "fault_request" for e in packets) == retries + 1)
                elif mode == "delay":
                    runner.check(name + ":interrupted-real-wait", any(e.get("event") == "wait_interrupted" for e in events))
                elif mode in ("malformed", "wrong-request-id"):
                    runner.check(name + ":native-retry-after-rejection", any(e.get("event") == "native_callback" and
                        e.get("operation") == 6 for e in events))
                elif mode == "foreign-report":
                    runner.check(name + ":actual-foreign-report-delivered", any(e.get("event") == "fault_foreign_report" for e in packets))
                else:
                    runner.check(name + ":actual-outer-mutation", any(e.get("event") in ("fault_duplicate", "fault_mutation") for e in packets))
        restart_peer, restart_paths = runner.agent(family)
        agent_entry = runner.children[-1]
        name = f"adapter-ipv{family}-same-engine-restart"
        config, _ = adapter_configuration(runner, name, family, restart_peer, restart_paths, 3)
        marker = runner.output / (name + ".release")
        events = runner.invoke(name, [driver, "restart", str(config), helper, "Main", str(marker)],
            ready_hook=runner.restart_hook(agent_entry, marker))
        results = [e for e in events if e.get("event") == "result"]
        runner.check(name + ":actual-native-timeliness-recovery", len(results) == 2 and
            all(e.get("outcome") == 0 for e in results) and any(e.get("event") == "native_callback" and
            e.get("operation") == 6 for e in events))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", required=True, choices=["preflight", "adapter"])
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sanitizers", action="store_true", help="Build separate ASan/UBSan native products")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    products = build_sanitizers(output) if args.sanitizers else None
    runner = Runner(output, products, args.sanitizers)
    aborted = False
    try:
        (preflight if args.phase == "preflight" else adapter)(runner)
    except Exception as error:
        aborted = True
        runner.check("preflight-completed", False)
        write_json(output / "abort.json", {"exception_class": type(error).__name__})
    finally:
        runner.stop()
        runner.scan()
    inputs = [ROOT / "tests/rewrite/NativeAgent.cpp", ROOT / "tests/rewrite/NativeApiProbe.cpp",
              Path(__file__).resolve(), ROOT / "snmp3App/native/tests/Makefile"]
    inputs += [runner.products / name for name in ("snmp3NativeAgent", "snmp3NativeApiProbe")]
    inputs += [Path("/usr/include/net-snmp") / name for name in
               ("session_api.h", "library/snmp_api.h", "library/keytools.h", "library/large_fd_set.h",
                "library/default_store.h", "library/snmp_transport.h", "agent/net-snmp-agent-includes.h")]
    if args.phase == "adapter":
        inputs += [runner.products / "snmp3NativeTest",
                   runner.products / "libsnmp3Native.so" if args.sanitizers else ROOT / "lib" / ARCH / "libsnmp3Native.so"]
        inputs += [ROOT / "snmp3App/native" / name for name in
                   ("Native.h", "Native.cpp", "Worker.h", "Worker.cpp", "NativeInternal.h", "Capabilities.h", "Capabilities.cpp")]
        inputs += [ROOT / "tests/rewrite/NativeTest.cpp", ROOT / "tests/rewrite/helpers/udp_fault.py",
                   ROOT / "snmp3App/native/Makefile", runner.products / "snmp3NativeProbe"]
        inputs += sorted(output.glob("*.config.json"))
    result = {"phase": args.phase, "full_r4_acceptance": False, "sanitizers": args.sanitizers,
              "aborted": aborted, "checks": runner.checks,
              "passed": not aborted and all(c["passed"] for c in runner.checks),
              "sha256": {str(path): digest(path) for path in inputs},
              "loaded_libraries": runner.identities}
    write_json(output / "results.json", result)
    print(("PASS: " if result["passed"] else "FAIL: ") + str(output / "results.json"))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
