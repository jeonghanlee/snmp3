#!/usr/bin/env python3
"""Run shipped Runtime, IOC and isolated Base lifecycle paths without internal substitutes."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

from run_independence import ARCH, BUILD_TIMEOUT, ROOT, TIMEOUT, digest, invoke, owns_udp, udp_port, validate_base

REPORT = re.compile(r"snmp3 runtime: state=(\w+) admission=(\d+) activation=(\d+) created=(\d+) exited=(\d+) joined=(\d+)")


def reports(text):
    return [(v[0], *map(int, v[1:])) for v in REPORT.findall(text)]


def ordered(text, tokens):
    position = 0
    for token in tokens:
        index = text.find(token, position)
        if index < 0:
            return False
        position = index + len(token)
    return True


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
        parser.error("invalid Base path characters")
    local = ROOT / "configure/RELEASE.local"
    if local.exists() and local.read_text() != f"EPICS_BASE = {base}\n":
        parser.error("existing configure/RELEASE.local differs")
    output.mkdir(parents=True)
    results = {"status": "FAIL", "checks": {}, "base": str(base), "cleanup": {}}
    repeater = None
    streams = []
    env = os.environ.copy()
    for key in ("LD_PRELOAD", "LD_AUDIT", "LD_DEBUG", "LD_DEBUG_OUTPUT", "LD_LIBRARY_PATH"):
        env.pop(key, None)
    env.update(EPICS_HOST_ARCH=ARCH, LD_LIBRARY_PATH=f"{ROOT}/lib/{ARCH}:{base}/lib/{ARCH}")

    def check(name, condition):
        results["checks"][name] = bool(condition)
        if not condition:
            raise RuntimeError(name)

    try:
        regression = invoke([sys.executable, str(ROOT / "tests/rewrite/run_independence.py"),
                             "--base", str(base), "--output", str(output / "independence")],
                            output, "independence", env, BUILD_TIMEOUT + 120)
        receipt = output / "independence/results.json"
        check("V2.7_independence", regression.returncode == 0 and
              json.loads(receipt.read_text())["status"] == "PASS")
        build = invoke(["make", "-C", "tests/rewrite", "-B", "-j2"], output, "test-build", env, BUILD_TIMEOUT)
        check("test_build", build.returncode == 0)
        product = ROOT / "bin" / ARCH / "snmp3LifecycleTest"
        binary = ROOT / "bin" / ARCH / "snmp3Ioc"
        dbd = ROOT / "dbd/snmp3Ioc.dbd"
        test_dbd = ROOT / "dbd/snmp3LifecycleTest.dbd"
        db = ROOT / "tests/rewrite/db/lifecycle.db"
        sources = [*sorted((ROOT / "snmp3App/src").glob("*.cpp")),
                   *sorted((ROOT / "snmp3App/src").glob("*.h")),
                   ROOT / "snmp3App/src/Makefile", ROOT / "tests/rewrite/Makefile",
                   ROOT / "tests/rewrite/LifecycleTest.cpp", Path(__file__).resolve(), db,
                   binary, product, ROOT / "lib" / ARCH / "libsnmp3.so", dbd, test_dbd,
                   ROOT / "snmp3App/src" / ("O." + ARCH) / "snmp3Ioc_registerRecordDeviceDriver.cpp"]
        results["sha256"] = {str(p): digest(p) for p in sources}
        port, repeat_port = udp_port(), udp_port()
        while port == repeat_port:
            repeat_port = udp_port()
        env.update(EPICS_CA_SERVER_PORT=str(port), EPICS_CA_REPEATER_PORT=str(repeat_port),
                   EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST="127.0.0.1",
                   EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
                   EPICS_CAS_BEACON_ADDR_LIST="127.0.0.1")
        results["ports"] = {"server": port, "repeater": repeat_port}
        streams = [(output / "repeater.stdout").open("w"), (output / "repeater.stderr").open("w")]
        repeater = subprocess.Popen([str(base / "bin" / ARCH / "caRepeater")],
                                    env=env, stdout=streams[0], stderr=streams[1])
        deadline = time.monotonic() + TIMEOUT
        while repeater.poll() is None and time.monotonic() < deadline:
            if owns_udp(repeater.pid, repeat_port):
                break
            time.sleep(0.02)
        check("owned_repeater_ready", repeater.poll() is None and owns_udp(repeater.pid, repeat_port))
        loader_env = dict(env, LD_DEBUG="libs")

        def run_case(name, command, expected):
            run = invoke(command, output, name, loader_env, stdin="exit\n")
            check(name + "_exit", run.returncode == expected)
            loaded = [str(Path(p).resolve()) for p in re.findall(r"calling init:\s*(\S+)", run.stderr)]
            results.setdefault("loaded_libraries", {})[name] = loaded
            check(name + "_production_library", str((ROOT / "lib" / ARCH / "libsnmp3.so").resolve()) in loaded)
            actual_base = [Path(p) for p in loaded if re.match(r"lib(?:Com|ca|dbCore|dbRecStd)\.so", Path(p).name)]
            check(name + "_base_identity", len(actual_base) == 4 and
                  all(p.parent == (base / "lib" / ARCH).resolve() for p in actual_base))
            return run

        prefix = (f'dbLoadDatabase("{dbd}")\n'
                  'snmp3Ioc_registerRecordDeviceDriver(pdbbase)\n')
        load = f'dbLoadRecords("{db}", "P=Lifecycle_")\n'

        def ioc(name, script, expected=0):
            path = output / (name + ".cmd")
            path.write_text("on error break\n" + prefix + script)
            return run_case(name, [str(binary), str(path)], expected)

        normal = ioc("normal", load + "iocInit\nsnmp3RuntimeReport\ndbpr(\"Lifecycle_PiniProbe\", 2)\n")
        sequence = ["snmp3 hook: AfterFinishDevSup", "snmp3 lifecycle: ready activation=1",
                    "dbProcess of 'Lifecycle_PiniProbe'", "snmp3 hook: AfterInitialProcess",
                    "snmp3 hook: AtShutdown", "snmp3 lifecycle: stop-requested activation=1",
                    "snmp3 lifecycle: worker-exit activation=1", "snmp3 lifecycle: joined activation=1",
                    "snmp3 hook: AfterStopScan", "snmp3 hook: AfterStopCallback",
                    "snmp3 hook: AfterShutdown", "snmp3 lifecycle: fallback"]
        check("V2.2_hook_PINI_shutdown_order", ordered(normal.stdout, sequence))
        check("V2.2_counts", reports(normal.stdout) == [("Running", 1, 1, 1, 0, 0), ("Stopped", 0, 1, 1, 1, 1)])
        check("V2.2_actual_Pini_value", bool(re.search(r"\bVAL\s*:\s*42\b", normal.stdout)))
        cold = ioc("cold", "snmp3RuntimeReport\n")
        check("V2.3_cold_fallback", reports(cold.stdout) == [("Cold", 0, 0, 0, 0, 0)] * 2)
        bad = ioc("before-init-error", "noSuchLifecycleCommand\necho MUST_NOT_RUN\n", 1)
        check("V2.3_preinit_error", reports(bad.stdout) == [("Cold", 0, 0, 0, 0, 0)] and
              "echo MUST_NOT_RUN" not in bad.stdout)
        partial = ioc("partial-build-error", load + "iocBuild\nsnmp3RuntimeReport\nnoSuchLifecycleCommand\n", 1)
        check("V2.4_partial_cleanup", ordered(partial.stdout, sequence) and
              reports(partial.stdout)[-1] == ("Stopped", 0, 1, 1, 1, 1) and
              partial.stdout.count("snmp3 lifecycle: joined activation=1") == 1)
        explicit = ioc("explicit-stop", load + "iocInit\nsnmp3Stop\nsnmp3Stop\nsnmp3RuntimeReport\n")
        check("V2.4_repeat_stop", reports(explicit.stdout) == [("Stopped", 0, 1, 1, 1, 1)] * 2 and
              explicit.stdout.count("snmp3 lifecycle: joined activation=1") == 1)
        module = run_case("module", [str(product), "module"], 0)
        check("V2.1_V2.6_module", "lifecycle test: PASS" in module.stdout and
              reports(module.stdout) == [("Stopped", 0, 20, 20, 20, 20)] * 2)
        limit_script = output / "continue.cmd"
        limit_script.write_text("on error continue\necho SUCCESSFUL_SCRIPT\n")
        failure = run_case("thread-failure", [str(product), "failure", str(limit_script)], 1)
        check("V2.5_real_limit_failure", "create-failed activation=1" in failure.stdout and
              reports(failure.stdout) == [("Failed", 0, 1, 0, 0, 0)] * 2 and
              "startup script failed" in failure.stderr and "SUCCESSFUL_SCRIPT" in failure.stdout)
        isolated = run_case("isolated", [str(product), "isolated", str(test_dbd), str(db)], 0)
        check("V2.6_isolated", "lifecycle test: PASS" in isolated.stdout and
              isolated.stdout.count("snmp3 hook: AfterFinishDevSup") == 2 and
              isolated.stdout.count("dbProcess of 'Isolated_PiniProbe'") == 2 and
              reports(isolated.stdout) == [("Stopped", 0, 2, 2, 2, 2)] * 2)
        check("frozen_inputs", all(digest(Path(p)) == value for p, value in results["sha256"].items()))
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
                results["cleanup"].update(repeater_returncode=rc, repeater_reaped=True)
                if rc != -15:
                    results["status"] = "FAIL"
            except subprocess.TimeoutExpired:
                repeater.kill()
                repeater.wait()
                results["cleanup"].update(forced_termination=True, repeater_reaped=True)
                results["status"] = "FAIL"
        for stream in streams:
            stream.close()
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(results["status"] + ": " + str(output / "results.json"))
    return 0 if results["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
