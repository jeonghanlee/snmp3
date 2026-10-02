#!/usr/bin/env python3
"""Record real IPC/scheduler/ABI/digest/conversion executions; no IOC integration claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    receipts = []
    products = args.products.resolve() if args.products else ROOT / "bin/linux-x86_64"
    source_paths = sorted((ROOT / "snmp3App").rglob("*.cpp"))
    source_paths += sorted((ROOT / "snmp3App").rglob("*.h"))
    source_paths += [Path(__file__).resolve(), ROOT / "tests/rewrite/IpcTest.cpp",
                     ROOT / "tests/rewrite/SchedulerTest.cpp", ROOT / "tests/rewrite/InventoryTest.cpp",
                     ROOT / "tests/rewrite/ConversionTest.cpp"]
    sources = {str(p): sha(p) for p in source_paths}
    environment = {"PATH": "/usr/bin:/bin", "LANG": "C", "LC_ALL": "C"}
    if args.sanitizers:
        environment.update(ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
                           UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def invoke(name, argv, expected=None):
        begin = time.monotonic_ns()
        with (output / (name + ".stdout")).open("xb") as stdout:
            with (output / (name + ".stderr")).open("xb") as stderr:
                child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=environment)
                forced = False
                try:
                    result = child.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    forced = True
                    child.kill()
                    result = child.wait()
        actual = (output / (name + ".stdout")).read_text().strip()
        diagnostics = (output / (name + ".stderr")).read_bytes()
        receipt = {"name": name, "argv": list(map(str, argv)), "environment": environment,
                   "pid": child.pid, "child_reaped": child.returncode is not None,
                   "returncode": result, "forced_cleanup": forced,
                   "elapsed_ns": time.monotonic_ns() - begin, "product_sha256": sha(Path(argv[0])),
                   "passed": result == 0 and not forced and not diagnostics and
                             (expected is None or actual == expected)}
        receipts.append(receipt)
        return actual

    for name in ("Ipc", "Scheduler", "Inventory", "Conversion"):
        invoke(name.lower(), [str(products / ("snmp3" + name + "Test"))])
    for length in (0, 1, 55, 56, 63, 64, 65, 4096, 65537):
        fixture = output / ("digest-" + str(length) + ".bin")
        with open(fixture, "xb", opener=lambda p, flags: os.open(p, flags, 0o600)) as stream:
            stream.write(bytes((i * 13 + 7) % 256 for i in range(length)))
        invoke("digest-" + str(length), [str(products / "snmp3IpcTest"), "--digest", str(fixture)], sha(fixture))
    for name, fixture in (("executable", products / "snmp3Worker"),
                          ("native-library", Path("/usr/lib/x86_64-linux-gnu/libnetsnmp.so.40.2.1"))):
        invoke("digest-" + name, [str(products / "snmp3IpcTest"), "--digest", str(fixture)], sha(fixture))
    summary = {"scope": "Component executions only; IOC/worker/native traffic and R5 acceptance remain pending",
               "sources": sources, "receipts": receipts, "passed": all(r["passed"] for r in receipts)}
    (output / "results.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(("PASS: " if summary["passed"] else "FAIL: ") + str(output / "results.json"))
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
