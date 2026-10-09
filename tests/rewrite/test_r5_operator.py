#!/usr/bin/env python3
"""Execute documented operator commands through the actual production IOC."""
import argparse
from pathlib import Path
import subprocess
import time
from test_native import ROOT, Runner, digest, write_json
from test_qualification import configure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    runner = Runner(output)
    aborted = False
    try:
        config = configure(runner, "fifo")
        script = output / "operator.cmd"
        commands = ["on error break", 'dbLoadDatabase("' + str(ROOT / "dbd/snmp3Ioc.dbd") + '")',
                    "snmp3Ioc_registerRecordDeviceDriver(pdbbase)",
                    'snmp3Load("' + str(config) + '", "' + str(runner.products / "snmp3NativeProbe") + '")',
                    'snmp3WorkerPath("' + str(runner.products / "snmp3Worker") + '")',
                    'snmp3QueueLimit("127.0.0.1", 1024, 1048576)', "iocInit", "snmp3Report",
                    "snmp3ConfigReport", "snmp3RuntimeReport", "snmp3Stop", "snmp3RuntimeReport"]
        script.write_text("\n".join(commands) + "\n")
        argv = [str(runner.products / "snmp3Ioc"), str(script)]
        start = time.monotonic_ns()
        with (output / "operator.stdout").open("xb") as stdout:
            with (output / "operator.stderr").open("xb") as stderr:
                child = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr, env=runner.env)
                forced = False
                try:
                    code = child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    forced = True
                    child.kill()
                    code = child.wait()
        write_json(output / "operator.receipt.json", {"argv": argv, "pid": child.pid, "returncode": code,
                   "child_reaped": child.returncode is not None, "forced_cleanup": forced,
                   "elapsed_ns": time.monotonic_ns() - start, "product_sha256": digest(Path(argv[0]))})
        runner.check("actual-operator-exit", code == 0 and not forced)
        text = (output / "operator.stdout").read_text()
        runner.check("documented-capability", "capability=owned-worker-transport; recordSupport=available" in text)
        runner.check("actual-running-then-stopped", "state=Running admission=1" in text and "state=Stopped admission=0" in text)
        runner.check("actual-default-limits", "countLimit=1024 byteLimit=1048576" in text)
        runner.check("exact-one-join", "activation=1 created=1 exited=1 joined=1" in text)
        runner.check("frozen-config", "revision=1 frozen=1 profiles=2 endpoints=2 bindings=2" in text)
        ioc_libs = runner.loader_identity(output / "operator.stderr")
        runner.check("actual-IOC-loader-native-free", bool(ioc_libs) and not any("netsnmp" in path for path in ioc_libs))
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"exception_class": type(error).__name__})
    finally:
        runner.stop()
        runner.scan()
    inputs = [Path(__file__).resolve(), ROOT / "tests/rewrite/test_qualification.py", ROOT / "tests/rewrite/test_native.py"]
    inputs += sorted((ROOT / "snmp3App/src").glob("*.cpp")) + sorted((ROOT / "snmp3App/src").glob("*.h"))
    inputs += [runner.products / name for name in ("snmp3Ioc", "snmp3Worker", "snmp3NativeProbe")]
    passed = not aborted and all(c["passed"] for c in runner.checks)
    write_json(output / "results.json", {"passed": passed, "checks": runner.checks, "aborted": aborted,
               "inputs": {str(p): digest(p) for p in inputs}, "loaded_libraries": runner.identities,
               "scope": "Actual documented production IOC commands and loader; application Native traffic qualified separately"})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
