#!/usr/bin/env python3
"""Build separately identified R5 products with ASan and UBSan instrumentation."""
import argparse
from pathlib import Path
import shlex
import subprocess
from test_native import ROOT, ARCH, digest, write_json


def build(output):
    products = output / "products"
    products.mkdir(mode=0o700)
    base = Path(next(line.split("=", 1)[1].strip() for line in
                    (ROOT / "configure/RELEASE.local").read_text().splitlines()
                    if line.strip().startswith("EPICS_BASE") and "=" in line))
    common = ["g++", "-std=c++11", "-O1", "-g", "-fPIC", "-pthread",
              "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-sanitize-recover=all"]
    common += ["-I" + str(p) for p in (ROOT / "snmp3App/src", ROOT / "snmp3App/native",
               base / "include", base / "include/os/Linux", base / "include/compiler/gcc")]
    native_flags = shlex.split(subprocess.check_output(["net-snmp-config", "--cflags"], text=True))
    native_libs = shlex.split(subprocess.check_output(["net-snmp-config", "--libs"], text=True))
    link = ["-L" + str(products), "-L" + str(base / "lib" / ARCH),
            "-Wl,-rpath,$ORIGIN:" + str(base / "lib" / ARCH)]
    src, native, tests = ROOT / "snmp3App/src", ROOT / "snmp3App/native", ROOT / "tests/rewrite"
    jobs = [("libsnmp3Wire.so", [src / (n + ".cpp") for n in ("Ipc", "Identity")],
             ["-shared"], ["-lCom"], False),
            ("libsnmp3.so", [src / (n + ".cpp") for n in
             ("Register", "Runtime", "Config", "Json", "Scheduler", "Supervisor")],
             ["-shared"], ["-lsnmp3Wire", "-lCom"], False),
            ("libsnmp3Native.so", [native / (n + ".cpp") for n in ("Capabilities", "Native", "Worker")],
             ["-shared"], ["-lCom"] + native_libs, True),
            ("snmp3NativeProbe", [native / "NativeProbe.cpp"], [],
             ["-lsnmp3Native", "-lCom"] + native_libs, True),
            ("snmp3NativeAgent", [tests / "NativeAgent.cpp"], [], ["-lnetsnmpagent"] + native_libs, True),
            ("snmp3Worker", [native / "WorkerMain.cpp"], [],
             ["-lsnmp3Native", "-lsnmp3Wire", "-lCom"] + native_libs, True)]
    for name in ("Qualification", "Supervisor", "Ipc", "Scheduler", "Inventory"):
        jobs.append(("snmp3" + name + "Test", [tests / (name + "Test.cpp")], [],
                     ["-lsnmp3", "-lsnmp3Wire", "-lCom"], name == "Inventory"))
    jobs.append(("snmp3RuntimeTest", [tests / "RuntimeTest.cpp", tests / ("O." + ARCH) /
                 "snmp3RuntimeTest_registerRecordDeviceDriver.cpp"], [],
                 ["-lsnmp3", "-lsnmp3Wire", "-ldbRecStd", "-ldbCore", "-lca", "-lCom"], False))
    receipts = []
    inputs = sorted(src.glob("*.h")) + sorted(native.glob("*.h"))
    for name, sources, flags, libraries, use_native in jobs:
        argv = common + (native_flags if use_native else []) + flags + list(map(str, sources)) + link + libraries
        argv += ["-o", str(products / name)]
        with (output / (name + ".build.stdout")).open("xb") as stdout:
            with (output / (name + ".build.stderr")).open("xb") as stderr:
                child = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
                code = child.wait()
        receipt = {"argv": argv, "returncode": code, "pid": child.pid, "child_reaped": True,
                   "sources": {str(p): digest(p) for p in sources + inputs}}
        if code == 0:
            receipt["product_sha256"] = digest(products / name)
            symbols = subprocess.check_output(["nm", "-D", str(products / name)], text=True)
            receipt["asan_imports"] = "__asan_" in symbols
            receipt["ubsan_imports"] = "__ubsan_" in symbols
        receipts.append(receipt)
        write_json(output / "sanitizer-build.json", {"products": receipts,
                   "coverage": "R5 module, wire, native owner, actual worker, component drivers and actual IOC; dependencies uninstrumented; leaks disabled"})
        if code or not receipt.get("asan_imports") or (name != "snmp3InventoryTest" and not receipt.get("ubsan_imports")):
            raise RuntimeError("instrumented R5 build failed: " + name)
    return products


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(mode=0o700)
    print(build(args.output.resolve()))
