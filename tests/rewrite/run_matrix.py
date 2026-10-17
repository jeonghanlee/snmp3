#!/usr/bin/env python3
"""Run the integrated qualification matrix of one environment and write a cell table.

Stages, in order:
  environment   OS, kernel, commit, tree state, tool versions and product hashes -> environment.json
  ordinary      the regression set on the ordinary products
  instrumented  the ASan and UBSan build and the runners that accept --sanitizers
  controls      every control group of test_record_controls.py (needs the instrumented build)
  load          warm-up, then the mixed sustained load of the module database tool with resource sampling

Instrumented products and the controls run where the environment policy asks for them: by default on Debian 13 and
Rocky Linux 8.10 only. Elsewhere those cells are written as NOT APPLICABLE and never as PASS. The thread sanitizer
cell is NOT APPLICABLE on every environment because no build tool exists.

cells.tsv has one row per cell: name, status, return code, seconds, detail. A status is PASS, FAIL, NOT RUN or
NOT APPLICABLE. The output directory must not exist.
"""
import argparse
import hashlib
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
INSTRUMENTED_OS = {("debian", "13"), ("rocky", "8.10")}

RECORD_CASES = ("baseline scan-proc alarm-flow chain-flow queued-simm boundaries retirement-flow edges alarms active "
                "policy numeric shutdown queued-shutdown stop-queued active-unforced accounting deadline-queue "
                "near-deadline rebuild stop-inflight stop-enqueue-failed stop-downstream live-detach repeat-detach "
                "contract stale-record final-clauses wake-loop").split()


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def os_release():
    fields = {}
    try:
        for line in Path("/etc/os-release").read_text().splitlines():
            if "=" in line:
                key, _, value = line.partition("=")
                fields[key] = value.strip().strip('"')
    except OSError:
        pass
    return fields


def base_path():
    for line in (ROOT / "configure/RELEASE.local").read_text().splitlines():
        if line.strip().startswith("EPICS_BASE") and "=" in line:
            return line.split("=", 1)[1].strip()
    raise SystemExit("configure/RELEASE.local names no EPICS_BASE")


def output_of(command):
    try:
        return subprocess.run(command, capture_output=True, text=True, timeout=60).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return ""


def environment_record(base):
    release = os_release()
    products = {}
    for pattern in ("bin/linux-x86_64/snmp3Ioc", "bin/linux-x86_64/snmp3Worker", "bin/linux-x86_64/snmp3NativeProbe",
                    "lib/linux-x86_64/libsnmp3.so", "lib/linux-x86_64/libsnmp3Wire.so",
                    "lib/linux-x86_64/libsnmp3Native.so"):
        path = ROOT / pattern
        products[pattern] = sha256(path) if path.exists() else None
    return {"date_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "os_id": release.get("ID"), "os_version_id": release.get("VERSION_ID"),
            "os_pretty_name": release.get("PRETTY_NAME"), "kernel": platform.release(), "machine": platform.machine(),
            "commit": output_of(["git", "-C", str(ROOT), "rev-parse", "HEAD"]),
            "tree_modified": bool(output_of(["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=no"])),
            "net_snmp": output_of(["net-snmp-config", "--version"]), "epics_base": base,
            "python": platform.python_version(), "product_sha256": products}


class Matrix:
    def __init__(self, output, only, timeout, prune=False):
        self.output = output
        self.only = only
        self.timeout = timeout
        self.prune = prune
        self.cells = []

    def selected(self, name):
        return not self.only or any(token in name for token in self.only)

    def record(self, name, status, rc="", seconds="", detail=""):
        self.cells.append((name, status, rc, seconds, detail))
        with (self.output / "cells.tsv").open("a") as f:
            f.write("\t".join(str(x) for x in (name, status, rc, seconds, detail)) + "\n")
        print(f"{status:15s} {name}" + (f"  rc={rc} {seconds}s" if rc != "" else ""), flush=True)

    def run(self, name, argv, timeout=None, env=None):
        if not self.selected(name):
            return None
        started = time.monotonic()
        log = self.output / f"{name}.log"
        try:
            with log.open("wb") as f:
                result = subprocess.run(argv, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT, env=env,
                                        timeout=timeout or self.timeout)
            rc = result.returncode
            detail = ""
        except subprocess.TimeoutExpired:
            rc, detail = 124, "timeout"
        seconds = round(time.monotonic() - started)
        if self.prune and rc == 0 and "--output" in argv:
            self.prune_output(name, Path(argv[argv.index("--output") + 1]))
        self.record(name, "PASS" if rc == 0 else "FAIL", rc, seconds, detail)
        return rc

    def prune_output(self, name, directory):
        """Keeps the results file of a passed cell and removes the rest of its output directory."""
        results = directory / "results.json"
        if results.exists():
            (self.output / f"{name}.results.json").write_bytes(results.read_bytes())
        shutil.rmtree(directory, ignore_errors=True)


def py(*args):
    return [sys.executable, *args]


def ordinary(matrix, base):
    out = matrix.output
    for case in RECORD_CASES:
        matrix.run(f"rec-{case}", py("tests/rewrite/test_records.py", "--case", case, "--output", str(out / f"o-rec-{case}")))
    matrix.run("record-shutdown", py("tests/rewrite/test_record_shutdown.py", "--output", str(out / "o-shutdown")))
    matrix.run("record-startup", py("tests/rewrite/test_record_startup.py", "--output", str(out / "o-startup")))
    matrix.run("record-ca", py("tests/rewrite/test_record_ca.py", "--output", str(out / "o-ca")))
    matrix.run("supervisor-ioc", py("tests/rewrite/test_supervisor.py", "--ioc", "--output", str(out / "o-supervisor")))
    matrix.run("qualification", py("tests/rewrite/test_qualification.py", "--output", str(out / "o-qualification")))
    matrix.run("r5-components", py("tests/rewrite/test_r5_components.py", "--output", str(out / "o-components")))
    matrix.run("spawn-descriptors", py("tests/rewrite/test_spawn_descriptors.py", "--output", str(out / "o-spawn")))
    matrix.run("r5-operator", py("tests/rewrite/test_r5_operator.py", "--output", str(out / "o-operator")))
    matrix.run("native-adapter", py("tests/rewrite/test_native.py", "--phase", "adapter", "--output", str(out / "o-native")))
    matrix.run("config", py("tests/rewrite/test_config.py", "--base", base, "--output", str(out / "o-config")))
    matrix.run("lifecycle", py("tests/rewrite/test_lifecycle.py", "--base", base, "--output", str(out / "o-lifecycle")))


def instrumented(matrix):
    out = matrix.output
    build = out / "san"
    rc = matrix.run("san-build", py("tests/rewrite/build_r5_sanitizers.py", "--output", str(build)))
    if rc != 0:
        return None
    products = str(build / "products")
    common = ["--products", products, "--sanitizers"]
    for case in RECORD_CASES:
        matrix.run(f"san-rec-{case}", py("tests/rewrite/test_records.py", "--case", case, *common,
                                          "--output", str(out / f"s-rec-{case}")))
    matrix.run("san-record-shutdown", py("tests/rewrite/test_record_shutdown.py", *common, "--output", str(out / "s-shutdown")))
    matrix.run("san-record-startup", py("tests/rewrite/test_record_startup.py", *common, "--output", str(out / "s-startup")))
    matrix.run("san-record-ca", py("tests/rewrite/test_record_ca.py", *common, "--output", str(out / "s-ca")))
    matrix.run("san-supervisor-ioc", py("tests/rewrite/test_supervisor.py", "--ioc", *common, "--output", str(out / "s-supervisor")))
    matrix.run("san-qualification", py("tests/rewrite/test_qualification.py", *common, "--output", str(out / "s-qualification")))
    matrix.run("san-r5-components", py("tests/rewrite/test_r5_components.py", *common, "--output", str(out / "s-components")))
    matrix.run("san-native-adapter", py("tests/rewrite/test_native.py", "--phase", "adapter", "--sanitizers",
                                         "--output", str(out / "s-native")))
    return build / "sanitizer-build.json"


def controls(matrix, receipt):
    groups = json.loads((HERE / "control-groups.json").read_text())
    out = matrix.output
    for option, names in groups.items():
        label = option.lstrip("-")
        if option == "--startup-controls":
            for phase in ("preflight", "thread"):
                matrix.run(f"ctl-startup-{phase}",
                           py("tests/rewrite/test_record_controls.py", "--build-receipt", str(receipt), "--output",
                              str(out / f"c-startup-{phase}"), option, *names, "--startup-phase", phase), timeout=7200)
        else:
            matrix.run(f"ctl-{label}", py("tests/rewrite/test_record_controls.py", "--build-receipt", str(receipt),
                                          "--output", str(out / f"c-{label}"), option, *names), timeout=7200)


def load(matrix, warmup, seconds, sample):
    argv = py("docs/snmp-performance/tools/run_module_database.py", "--output", str(matrix.output / "load"),
              "--idle-seconds", str(warmup), "--seconds", str(seconds), "--sample-seconds", str(sample),
              "--delays", "0", "--versions", "3")
    matrix.run("load-sustained", argv, timeout=warmup + seconds + 1800)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", required=True, type=Path, help="new directory for logs, environment.json, cells.tsv")
    parser.add_argument("--stages", default="environment,ordinary,instrumented,controls,load",
                        help="comma list of stages (default: all)")
    parser.add_argument("--instrumented", choices=("auto", "yes", "no"), default="auto",
                        help="auto: Debian 13 and Rocky Linux 8.10 only")
    parser.add_argument("--only", action="append", default=[],
                        help="run only the cells whose name contains this text (repeatable)")
    parser.add_argument("--timeout", type=int, default=1200, help="seconds allowed per cell (default 1200)")
    parser.add_argument("--prune", action="store_true",
                        help="after a passed cell keep its log and results.json and remove its output directory "
                             "(for guests with little disk); a failed cell keeps everything")
    parser.add_argument("--warmup-seconds", type=int, default=300)
    parser.add_argument("--load-seconds", type=int, default=3600)
    parser.add_argument("--sample-seconds", type=float, default=10.0)
    parser.add_argument("--list", action="store_true", help="print the cells and exit")
    args = parser.parse_args()
    stages = set(args.stages.split(","))
    if args.list:
        print("ordinary:", ", ".join(["rec-" + c for c in RECORD_CASES]) + ", record-shutdown, record-startup, record-ca, "
              "supervisor-ioc, qualification, r5-components, spawn-descriptors, r5-operator, native-adapter, config, lifecycle")
        print("controls:", ", ".join(json.loads((HERE / "control-groups.json").read_text())))
        return 0
    if not os.environ.get("EPICS_BASE"):
        os.environ["EPICS_BASE"] = base_path()
    base = base_path()
    args.output.mkdir(mode=0o700)
    matrix = Matrix(args.output.resolve(), args.only, args.timeout, args.prune)
    (args.output / "cells.tsv").write_text("")
    env_record = environment_record(base)
    if "environment" in stages:
        (args.output / "environment.json").write_text(json.dumps(env_record, indent=1) + "\n")
        print("environment:", env_record["os_pretty_name"], env_record["kernel"], env_record["commit"][:12],
              "tree_modified" if env_record["tree_modified"] else "tree_clean", flush=True)
    do_instrumented = args.instrumented == "yes" or (
        args.instrumented == "auto" and (env_record["os_id"], env_record["os_version_id"]) in INSTRUMENTED_OS)
    if "ordinary" in stages:
        ordinary(matrix, base)
    receipt = None
    if "instrumented" in stages or "controls" in stages:
        if do_instrumented:
            receipt = instrumented(matrix) if "instrumented" in stages else None
            if "controls" in stages:
                if receipt is None and "instrumented" not in stages:
                    matrix.record("controls", "NOT RUN", detail="needs the instrumented stage in the same run")
                elif receipt is None:
                    matrix.record("controls", "NOT RUN", detail="the instrumented build failed")
                else:
                    controls(matrix, receipt)
        else:
            matrix.record("instrumented", "NOT APPLICABLE", detail="policy: instrumented products on Debian 13 and Rocky 8.10 only")
            matrix.record("controls", "NOT APPLICABLE", detail="the control harness needs the instrumented build")
    matrix.record("tsan", "NOT APPLICABLE", detail="no thread sanitizer build tool exists")
    if "load" in stages:
        load(matrix, args.warmup_seconds, args.load_seconds, args.sample_seconds)
    counts = {}
    for _, status, *_ in matrix.cells:
        counts[status] = counts.get(status, 0) + 1
    (args.output / "summary.json").write_text(json.dumps({"environment": env_record, "counts": counts}, indent=1) + "\n")
    print("summary:", ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
    return 0 if counts.get("FAIL", 0) == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
