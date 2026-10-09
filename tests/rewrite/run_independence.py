#!/usr/bin/env python3
"""Build and exercise the independent Base-only IOC; retain all evidence."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
ARCH = "linux-x86_64"
TIMEOUT = 30
BUILD_TIMEOUT = 180
LEGACY = r"devSnmp|(?<![A-Za-z])snmpApp/|snmpEpics"
FORBIDDEN = re.compile(LEGACY + r"|snmpNative|netsnmp|net-snmp", re.I)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def invoke(args, output, name, env, timeout=TIMEOUT, stdin=None):
    (output / (name + ".command.json")).write_text(json.dumps(args))
    try:
        result = subprocess.run(args, cwd=ROOT, env=env, input=stdin, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        for suffix, data in (("stdout", exc.stdout), ("stderr", exc.stderr)):
            if isinstance(data, bytes):
                data = data.decode(errors="replace")
            (output / (name + "." + suffix)).write_text(data or "")
        (output / (name + ".outcome.json")).write_text(
            json.dumps({"timeout": True, "forced_termination": True}))
        raise
    (output / (name + ".stdout")).write_text(result.stdout)
    (output / (name + ".stderr")).write_text(result.stderr)
    (output / (name + ".outcome.json")).write_text(
        json.dumps({"returncode": result.returncode, "timeout": False, "reaped": True}))
    return result


def udp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def owns_udp(pid, port):
    sockets = set()
    for fd in Path(f"/proc/{pid}/fd").iterdir():
        try:
            target = fd.readlink().as_posix()
        except FileNotFoundError:
            continue
        match = re.fullmatch(r"socket:\[(\d+)\]", target)
        if match:
            sockets.add(match.group(1))
    for name in ("udp", "udp6"):
        for line in Path(f"/proc/{pid}/net/{name}").read_text().splitlines()[1:]:
            cols = line.split()
            if int(cols[1].split(":")[1], 16) == port and cols[9] in sockets:
                return True
    return False


def validate_base(parser, path):
    base = path.resolve()
    header = base / "include/epicsVersion.h"
    if not header.is_file():
        parser.error("--base must name an installed EPICS Base 7.0.10 tree")
    text = header.read_text()
    values = [re.search(r"^#define\s+" + macro + r"\s+(\d+)", text, re.M)
              for macro in ("EPICS_VERSION", "EPICS_REVISION", "EPICS_MODIFICATION")]
    if not all(values):
        parser.error("malformed Base version header")
    version = tuple(int(value.group(1)) for value in values)
    if version != (7, 0, 10):
        parser.error("--base must be version 7.0.10")
    for name in (f"lib/{ARCH}/libCom.so", f"bin/{ARCH}/caRepeater", "configure/CONFIG"):
        if not (base / name).is_file():
            parser.error("missing Base prerequisite: " + name)
    return base


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    base = validate_base(parser, args.base)
    output = args.output.resolve()
    if output.exists():
        parser.error("--output must be a new directory")
    if any(c in str(base) for c in " \t\n#$"):
        parser.error("--base path cannot contain whitespace or Make metacharacters")
    local = ROOT / "configure/RELEASE.local"
    expected = f"EPICS_BASE = {base}\n"
    if local.exists() and local.read_text() != expected:
        parser.error("configure/RELEASE.local differs; select its Base or resolve configuration explicitly")
    output.mkdir(parents=True)
    results = {"status": "FAIL", "base": str(base), "checks": {}, "cleanup": {}}
    repeater = None
    logs = []
    env = os.environ.copy()
    for key in ("LD_PRELOAD", "LD_AUDIT", "LD_DEBUG", "LD_DEBUG_OUTPUT", "LD_LIBRARY_PATH"):
        env.pop(key, None)
    env["EPICS_HOST_ARCH"] = ARCH
    env["LD_LIBRARY_PATH"] = str(ROOT / "lib" / ARCH) + ":" + str(base / "lib" / ARCH)

    def check(name, condition):
        results["checks"][name] = bool(condition)
        if not condition:
            raise RuntimeError(name)

    try:
        local.write_text(expected)
        cfg = invoke(["make", "--no-print-directory", "-s",
                      "--eval=freshPrintConfiguration:;@echo $(realpath $(EPICS_BASE)); echo $(realpath $(RULES)); echo $(realpath $(INSTALL_LOCATION))",
                      "freshPrintConfiguration"], output, "configuration", env)
        check("selected_configuration", cfg.returncode == 0 and
              cfg.stdout.splitlines() == [str(base), str(base), str(ROOT)])
        build = invoke(["make", "-B", "-j2", "--output-sync=recurse"],
                       output, "build", env, BUILD_TIMEOUT)
        check("build", build.returncode == 0)
        dry = invoke(["make", "-C", "snmp3App/src", "-B", "-n"],
                     output, "dry-run", env, BUILD_TIMEOUT)
        check("dry_run_independence", dry.returncode == 0 and not FORBIDDEN.search(dry.stdout + dry.stderr))
        # Recurse output synchronization preserves each product's observed command block.
        directory = str(ROOT / "snmp3App/src" / ("O." + ARCH))
        begin = build.stdout.find("Entering directory '" + directory + "'")
        end = build.stdout.find("Leaving directory '" + directory + "'", begin)
        commands = build.stdout[begin:end] if begin >= 0 and end > begin else ""
        (output / "ioc-build-commands.txt").write_text(commands)
        check("build_commands_independence", bool(commands) and not FORBIDDEN.search(commands) and
              not re.search(LEGACY, build.stdout + build.stderr, re.I))
        src = ROOT / "snmp3App/src" / ("O." + ARCH)
        objects = sorted(p.name for p in src.glob("*.o"))
        results["objects"] = objects
        check("object_set", objects == ["Config.o", "Conversion.o", "DeviceSupport.o", "Identity.o", "Ipc.o", "Json.o", "Main.o", "Register.o", "Request.o", "Runtime.o", "Scheduler.o", "Supervisor.o",
                                        "snmp3Ioc_registerRecordDeviceDriver.o"])
        dependencies = "\n".join(p.read_text() for p in src.glob("*.d"))
        (output / "dependencies.txt").write_text(dependencies)
        check("dependency_independence", bool(dependencies) and not FORBIDDEN.search(dependencies))
        binary = ROOT / "bin" / ARCH / "snmp3Ioc"
        library = ROOT / "lib" / ARCH / "libsnmp3.so"
        dbd = ROOT / "dbd/snmp3Ioc.dbd"
        registrar = src / "snmp3Ioc_registerRecordDeviceDriver.cpp"
        check("registration", "snmp3Registrar" in registrar.read_text() and
              not FORBIDDEN.search(dbd.read_text() + registrar.read_text()))
        inputs = [ROOT / "Makefile", ROOT / "configure/CONFIG_SITE", local,
                  ROOT / "snmp3App/Makefile", ROOT / "snmp3App/src/Makefile",
                  ROOT / "snmp3App/src/snmp3.dbd",
                  *sorted((ROOT / "snmp3App").glob("**/*.cpp")),
                  *sorted((ROOT / "snmp3App/src").glob("*.h")), Path(__file__).resolve(),
                  ROOT / "tests/rewrite/db/independence.db", base / "include/epicsVersion.h",
                  binary, library, ROOT / "lib" / ARCH / "libsnmp3Wire.so", dbd, registrar]
        results["sha256"] = {str(p): digest(p) for p in inputs}
        linked = invoke(["ldd", str(binary)], output, "linked", env)
        check("linked_independence", linked.returncode == 0 and "not found" not in linked.stdout and
              "libsnmp3.so" in linked.stdout and not FORBIDDEN.search(linked.stdout))
        native = ROOT / "bin" / ARCH / "snmp3NativeProbe"
        native_linked = invoke(["ldd", str(native)], output, "native-linked", env)
        native_objects = sorted(p.name for p in (ROOT / "snmp3App/native" / ("O." + ARCH)).glob("*.o"))
        check("separate_native_product", native_objects == ["Capabilities.o", "Native.o", "NativeProbe.o", "Worker.o", "WorkerMain.o"] and
              native_linked.returncode == 0 and "libnetsnmp" in native_linked.stdout and
              "not found" not in native_linked.stdout and
              not re.search(LEGACY, native_linked.stdout, re.I))
        results["sha256"][str(native)] = digest(native)
        check("declared_native_sources", sorted(p.name for p in (ROOT / "snmp3App/native").glob("*.cpp")) ==
              ["Capabilities.cpp", "Native.cpp", "NativeProbe.cpp", "Worker.cpp", "WorkerMain.cpp"])
        native_test_objects = sorted(p.name for p in (ROOT / "snmp3App/native/tests" / ("O." + ARCH)).glob("*.o"))
        check("declared_native_test_objects", native_test_objects == ["NativeAgent.o", "NativeApiProbe.o", "NativeTest.o"])
        products = [ROOT / "lib" / ARCH / "libsnmp3Native.so"]
        products += [ROOT / "bin" / ARCH / name for name in
                     ("snmp3NativeApiProbe", "snmp3NativeAgent", "snmp3NativeTest", "snmp3Worker")]
        for product in products:
            observed = invoke(["ldd", str(product)], output, product.name + "-linked", env)
            check(product.name + "_native_link", observed.returncode == 0 and "libnetsnmp" in observed.stdout and
                  "not found" not in observed.stdout and not re.search(LEGACY, observed.stdout, re.I))
            results["sha256"][str(product)] = digest(product)
        native_dependencies = "\n".join(p.read_text() for p in
            (ROOT / "snmp3App/native" / ("O." + ARCH)).glob("*.d"))
        check("native_sources_exclude_legacy", bool(native_dependencies) and not re.search(LEGACY, native_dependencies, re.I))
        server_port = udp_port()
        repeater_port = udp_port()
        while repeater_port == server_port:
            repeater_port = udp_port()
        env.update(EPICS_CA_SERVER_PORT=str(server_port), EPICS_CA_REPEATER_PORT=str(repeater_port),
                   EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST="127.0.0.1",
                   EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
                   EPICS_CAS_BEACON_ADDR_LIST="127.0.0.1")
        results["ports"] = {"server": server_port, "repeater": repeater_port}
        logs = [(output / "repeater.stdout").open("w"), (output / "repeater.stderr").open("w")]
        repeater = subprocess.Popen([str(base / "bin" / ARCH / "caRepeater")],
                                    env=env, stdout=logs[0], stderr=logs[1])
        deadline = time.monotonic() + TIMEOUT
        while repeater.poll() is None and time.monotonic() < deadline:
            if owns_udp(repeater.pid, repeater_port):
                break
            time.sleep(0.02)
        check("owned_repeater_ready", repeater.poll() is None and owns_udp(repeater.pid, repeater_port))
        script = output / "startup.cmd"
        script.write_text('on error break\n'
                          f'dbLoadDatabase("{dbd}")\n'
                          'snmp3Ioc_registerRecordDeviceDriver(pdbbase)\n'
                          f'dbLoadRecords("{ROOT}/tests/rewrite/db/independence.db", "P=Rewrite_")\n'
                          'iocInit\nsnmp3Report\ndbpf("Rewrite_Probe.PROC", "1")\n'
                          'dbpr("Rewrite_Probe", 2)\n')
        loader_env = dict(env, LD_DEBUG="libs")
        run = invoke([str(binary), str(script)], output, "ioc", loader_env, stdin="exit\n")
        check("ioc_exit", run.returncode == 0)
        check("report", "snmp3: Base 7.0.10; capability=owned-worker-transport; recordSupport=available" in run.stdout)
        check("actual_processing", re.search(r"dbProcess of 'Rewrite_Probe'", run.stdout) is not None)
        check("probe_result", re.search(r"\bVAL\s*:\s*42\b", run.stdout) is not None and
              re.search(r"\bUDF\s*:\s*0\b", run.stdout) is not None and
              re.search(r"\bPACT\s*:\s*0\b", run.stdout) is not None)
        loaded = [Path(p).resolve() for p in re.findall(r"calling init:\s*(\S+)", run.stderr)]
        results["loaded_libraries"] = [str(p) for p in loaded]
        check("actual_loader_independence", library.resolve() in loaded and not FORBIDDEN.search(run.stderr))
        base_libs = [p for p in loaded if p.name in ("libCom.so.3.25.0",) or
                     re.match(r"lib(?:Com|ca|dbCore|dbRecStd)\.so", p.name)]
        check("actual_base_identity", len(base_libs) >= 4 and
              all(p.parent == (base / "lib" / ARCH).resolve() for p in base_libs))
        invalid = output / "invalid.cmd"
        invalid.write_text("on error break\nnoSuchSnmp3Command\necho SHOULD_NOT_EXECUTE\n")
        bad = invoke([str(binary), str(invalid)], output, "invalid-startup", env, stdin="")
        check("startup_command_error", bad.returncode == 1 and
              "startup script failed" in bad.stderr and "SHOULD_NOT_EXECUTE" not in bad.stdout)
        missing = invoke([str(binary), str(output / "absent.cmd")], output, "missing-startup", env, stdin="")
        check("missing_startup", missing.returncode == 1)
        before = digest(local)
        rejected_output = output / "must-not-exist"
        badbase = invoke([sys.executable, str(Path(__file__).resolve()), "--base", str(output / "absent-base"),
                          "--output", str(rejected_output)], output, "invalid-base", env)
        badcli = invoke([sys.executable, str(Path(__file__).resolve()), "--invalid-option"],
                        output, "invalid-cli", env)
        check("cli_rejection_before_mutation", badbase.returncode == 2 and badcli.returncode == 2 and
              not rejected_output.exists() and digest(local) == before)
        results["status"] = "PASS"
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as exc:
        results["error"] = str(exc)
    finally:
        if repeater is not None:
            results["cleanup"]["repeater_pid"] = repeater.pid
            if repeater.poll() is None:
                repeater.terminate()
            try:
                rc = repeater.wait(timeout=TIMEOUT)
                results["cleanup"]["repeater_returncode"] = rc
                results["cleanup"]["repeater_reaped"] = True
                if rc != -15:
                    results["status"] = "FAIL"
                    results["cleanup"]["unexpected_exit"] = True
            except subprocess.TimeoutExpired:
                repeater.kill()
                repeater.wait()
                results["cleanup"]["forced"] = True
                results["status"] = "FAIL"
        for log in logs:
            log.close()
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(results["status"] + ": " + str(output / "results.json"))
    return 0 if results["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
