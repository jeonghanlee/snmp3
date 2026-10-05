#!/usr/bin/env python3
"""Build defective support copies and require shipped record tests to detect them."""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from test_native import ROOT, digest, write_json


CONTROLS = {
    "communication-alarm": ("DeviceSupport.cpp", "alarms",
                            [("context.alarm=result.outcome==ipc::Outcome::NativeFailure && !transport ?",
                              "context.alarm=result.outcome==ipc::Outcome::NativeFailure ?")],
                            "native timeout communication alarm mismatch: Records_TimeoutAi"),
    "precision": ("Conversion.cpp", "edges",
                  [("require(exactInteger(number, digits));", "(void)digits;")],
                  "record alarm outcome mismatch: Records_WideDouble"),
    "capacity": ("Conversion.cpp", "edges",
                 [("require(bytes.size() <= dataCapacity);", "(void)dataCapacity;"),
                  ("bytes.data()), bytes.size());",
                   "bytes.data()), bytes.size() > dataCapacity ? dataCapacity : bytes.size());")],
                 "record alarm outcome mismatch: Records_Text"),
    "ties": ("Conversion.cpp", "baseline",
             [("(quotient & 1)", "((quotient + 1) & 1)")],
             "ao binary32 wire tie did not select even neighbor"),
    "ambient-rounding": ("Conversion.cpp", "edges",
                         [("const float rounded = roundBinary32(number);", "const float rounded = static_cast<float>(number);")],
                         "actual binary32 SET/GET disagreed with specified IEEE bits"),
    "callback-retry": ("Request.cpp", "baseline",
                       [("if(callbackRequest(&context.callback)) {\n            context.callbackState=CallbackState::Pending;",
                         "if(callbackRequest(&context.callback)) {\n            context.callbackState=CallbackState::Inert;")],
                       "full callback queue lost or prematurely consumed terminal"),
    "terminal-release": ("Request.cpp", "baseline",
                         [("if(context.terminal.result)context.owner->release(context.terminal.id);",
                           "(void)context.terminal.result;")],
                         "native retirement did not settle: Records_Ai"),
}


# Admission-behind-retirement controls: each alters Scheduler.cpp and must fail a named real-path cell
# that passes on the unmodified products. Kinds: "component" runs one SchedulerTest cell, "qualification"
# one qualification case, "record" one record runner case; "check" names the runner check that must fail.
EXPIRE_QUEUED = "auto id=*it; auto& g=*a.generations.at(id);"
STOP_QUEUED = "for(auto id:a.queue) { auto& g=*a.generations.at(id); select(g,ipc::Outcome::Stopping);"
BINDING_LOOKUP = [(EXPIRE_QUEUED, "auto id=*it; auto& g=*a.generations.lower_bound(Key(id.first,0))->second;"),
                  (STOP_QUEUED, "for(auto id:a.queue) { auto& g=*a.generations.lower_bound(Key(id.first,0))->second; select(g,ipc::Outcome::Stopping);")]
D7_CONTROLS = {
    "per-handle-bound": ("component", "admission-behind-retirement", None,
                         [("require(existing<=1 && consumed); behind.push_back(existing==1);",
                           "(void)consumed; behind.push_back(existing==1);")]),
    "early-release": ("component", "admission-behind-retirement", None,
                      [("if(g.consumed && g.retired) {", "if(g.consumed) {")]),
    "binding-lookup-component": ("component", "two-generation-lifecycle", None, BINDING_LOOKUP),
    "binding-lookup-qualification": ("qualification", "behind-retirement", None, BINDING_LOOKUP),
    "binding-lookup-record": ("record", "stop-queued", "queued-successor-completes-stopping-once", BINDING_LOOKUP),
    "queued-deadline-restart": ("record", "deadline-queue", "below-threshold-queued-generation-not-sent",
                                [("if(g.command.deadline>nowUs) { ++it; continue; }",
                                  "if(g.command.deadline>nowUs || g.behind) { ++it; continue; }"),
                                 ("if(initial.command.deadline<=nowUs)return d;",
                                  "if(initial.command.deadline<=nowUs && !initial.behind)return d;"),
                                 ("if(g.command.deadline<=nowUs || !compatible(first.first,id.first,initial,g)",
                                  "if((g.command.deadline<=nowUs && !g.behind) || !compatible(first.first,id.first,initial,g)"),
                                 ("for(auto id:a.active) { a.generations.at(id)->active=true;",
                                  "for(auto id:a.active) { auto& moved=*a.generations.at(id); if(moved.behind && moved.command.deadline<=nowUs)moved.command.deadline=ipc::add(nowUs,1000000); moved.active=true;")]),
    "stop-one-generation": ("record", "stop-queued", "queued-successor-completes-stopping-once", [BINDING_LOOKUP[1]]),
    "storage-validation": ("component", "forged-retirement", None,
                           [("for(size_t i=0;i<ids.size();++i)if(!(a.generations.at(a.active[i])->command.id==ids[i]))return false;",
                             "for(size_t i=0;i<ids.size();++i)if(!a.generations.count(key(ids[i])))return false;")]),
}


def run_cell(kind, cell, products, output, sanitizers=True):
    # Executes one real-path cell against a product directory and returns its observable outcome.
    output.mkdir(mode=0o700, exist_ok=True)
    if kind == "component":
        argv = [str(products / "snmp3SchedulerTest"), cell]
    elif kind == "qualification":
        argv = [sys.executable, str(ROOT / "tests/rewrite/test_qualification.py"), "--cases", cell,
                "--products", str(products), "--output", str(output / "run")] + (["--sanitizers"] if sanitizers else [])
    else:
        argv = [sys.executable, str(ROOT / "tests/rewrite/test_records.py"), "--case", cell,
                "--products", str(products), "--output", str(output / "run")] + (["--sanitizers"] if sanitizers else [])
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    with (output / "cell.stdout").open("xb") as stdout, (output / "cell.stderr").open("xb") as stderr:
        child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=environment)
        code = waited(child, 900)
    failed = []
    aborted = False
    results = output / "run" / "results.json"
    if results.exists():
        data = json.loads(results.read_text())
        failed = [check["name"] for check in data.get("checks", []) if not check["passed"]]
        aborted = bool(data.get("aborted"))
        for nested in sorted((output / "run").glob("*/results.json")):
            nested_data = json.loads(nested.read_text())
            failed += [check["name"] for check in nested_data.get("checks", []) if not check["passed"]]
            aborted = aborted or bool(nested_data.get("aborted"))
    # A record cell counts only when every trial process exited by itself and stop-queued printed its event.
    forced = any(json.loads(receipt.read_text()).get("forced_cleanup")
                 for receipt in sorted((output / "run").glob("records*.receipt.json")))
    observations = output / "run" / "record-observations.json"
    observed = kind != "record" or cell != "stop-queued" or (observations.exists() and any(
        event.get("event") == "stop_queued" for event in json.loads(observations.read_text())))
    return {"argv": argv, "returncode": code, "pid": child.pid, "child_reaped": True, "failed_checks": failed,
            "aborted": aborted, "forced_cleanup": forced, "observed": observed,
            "stderr_tail": (output / "cell.stderr").read_text()[-400:]}


def detected(outcome, check):
    # A control is detected when its cell fails without aborting, forced cleanup or a missing stop-queued event;
    # a named check must be among the failed checks.
    return (outcome["returncode"] != 0 and not outcome["aborted"] and not outcome["forced_cleanup"] and
            outcome["observed"] and (check is None or check in outcome["failed_checks"]))


def compile_product(job, original_products, products, source, replacement, output):
    argv = ["-L" + str(products) if arg == "-L" + str(original_products) else arg for arg in job["argv"]]
    argv = [str(replacement) if arg == str(source) else arg for arg in argv]
    product = products / Path(job["argv"][-1]).name
    argv[-1] = str(product)
    with (output / (product.name + ".build.stdout")).open("xb") as stdout:
        with (output / (product.name + ".build.stderr")).open("xb") as stderr:
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
            code = waited(child, 120)
    inputs = [Path(arg) for arg in argv if arg.endswith(".cpp")]
    inputs += list((ROOT / "snmp3App/src").glob("*.h"))
    receipt = {"argv": argv, "returncode": code, "pid": child.pid, "child_reaped": True,
               "inputs": {str(path): digest(path) for path in sorted(inputs)}}
    if code == 0:
        receipt["product_sha256"] = digest(product)
    write_json(output / (product.name + ".build.json"), receipt)
    if code:
        raise RuntimeError("control build failed")
    return product


def waited(child, timeout):
    try:
        return child.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        child.kill()
        child.wait()
        raise


def resolved_support(executable):
    # Library path the dynamic loader resolves for libsnmp3.so from the given executable.
    listing = subprocess.run(["ldd", str(executable)], capture_output=True, text=True, check=True).stdout
    for line in listing.splitlines():
        if line.strip().startswith("libsnmp3.so"):
            return line.split("=>", 1)[1].split("(", 1)[0].strip()
    return ""


def main_d7(args, jobs, output):
    selected = {Path(job["argv"][-1]).name: job for job in jobs}
    original_products = Path(selected["libsnmp3.so"]["argv"][-1]).parent
    tests = {"component": "snmp3SchedulerTest", "qualification": "snmp3QualificationTest", "record": "snmp3RecordTest"}
    references = {}
    for name in args.d7_controls:
        kind, cell, check, _ = D7_CONTROLS[name]
        if (kind, cell) not in references:
            outcome = run_cell(kind, cell, original_products, output / ("reference-" + kind + "-" + cell))
            references[(kind, cell)] = outcome
        # The reference must pass the very cell or named check the control has to fail; other checks of
        # the same case may fail for unrelated pending work.
        reference = references[(kind, cell)]
        reference.setdefault("passed_for", {})[name] = (not reference["aborted"] and not reference["forced_cleanup"] and
                                                        reference["observed"]) and (
            reference["returncode"] == 0 if check is None else check not in reference["failed_checks"])
    results = []
    for name in args.d7_controls:
        kind, cell, check, edits = D7_CONTROLS[name]
        item = output / name
        item.mkdir(mode=0o700)
        products = item / "products"
        products.mkdir(mode=0o700)
        source = ROOT / "snmp3App/src/Scheduler.cpp"
        text = source.read_text()
        for old, new in edits:
            if text.count(old) != 1:
                raise RuntimeError("control source anchor is not unique")
            text = text.replace(old, new)
        replacement = item / "Scheduler.cpp"
        replacement.write_text(text)
        write_json(item / "mutation.json", {"control": name, "source": str(source), "original_sha256": digest(source),
                   "mutated_sha256": digest(replacement), "edits": edits, "cell": [kind, cell], "check": check})
        rebuilt = ("libsnmp3.so", tests[kind])
        for existing in original_products.iterdir():
            if existing.name not in rebuilt:
                (products / existing.name).symlink_to(existing)
        defective = compile_product(selected["libsnmp3.so"], original_products, products, source, replacement, item)
        executable = compile_product(selected[tests[kind]], original_products, products, source, replacement, item)
        loaded = Path(resolved_support(executable)).resolve() == defective.resolve()
        outcome = run_cell(kind, cell, products, item / "cell")
        reference = references[(kind, cell)]
        passed = reference["passed_for"][name] and loaded and detected(outcome, check)
        results.append({"control": name, "passed": passed, "cell": [kind, cell], "check": check,
                        "reference_passed": reference["passed_for"][name], "defective_library_resolved": loaded,
                        "returncode": outcome["returncode"], "aborted": outcome["aborted"],
                        "forced_cleanup": outcome["forced_cleanup"], "observed": outcome["observed"],
                        "failed_checks": outcome["failed_checks"],
                        "defective_sha256": digest(defective)})
        write_json(output / "results.json", {"passed": all(row["passed"] for row in results),
                   "complete": len(results) == len(args.d7_controls), "controls": results,
                   "references": [dict(value, cell=list(key)) for key, value in references.items()],
                   "inputs": {str(Path(__file__).resolve()): digest(Path(__file__).resolve()),
                              str(args.build_receipt.resolve()): digest(args.build_receipt)}})
    passed = all(row["passed"] for row in results)
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-receipt", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--controls", nargs="+", choices=tuple(CONTROLS), default=list(CONTROLS))
    parser.add_argument("--d7-controls", nargs="+", choices=tuple(D7_CONTROLS))
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    jobs = json.loads(args.build_receipt.read_text())["products"]
    if args.d7_controls:
        return main_d7(args, jobs, output)
    selected = {Path(job["argv"][-1]).name: job for job in jobs}
    support_job = selected["libsnmp3.so"]
    test_job = selected["snmp3RecordTest"]
    original_products = Path(support_job["argv"][-1]).parent
    results = []
    for name in args.controls:
        item = output / name
        item.mkdir(mode=0o700)
        products = item / "products"
        products.mkdir(mode=0o700)
        source_name, case, edits, expected = CONTROLS[name]
        source = ROOT / "snmp3App/src" / source_name
        text = source.read_text()
        for old, new in edits:
            if text.count(old) != 1:
                raise RuntimeError("control source anchor is not unique")
            text = text.replace(old, new)
        replacement = item / source_name
        replacement.write_text(text)
        write_json(item / "mutation.json", {"control": name, "source": str(source),
                   "original_sha256": digest(source), "mutated_sha256": digest(replacement),
                   "edits": edits, "expected_assertion": expected})
        for product_name in ("libsnmp3Wire.so", "libsnmp3Native.so", "snmp3NativeProbe", "snmp3Worker", "snmp3NativeAgent"):
            (products / product_name).symlink_to(original_products / product_name)
        defective = compile_product(support_job, original_products, products, source, replacement, item)
        compile_product(test_job, original_products, products, source, replacement, item)
        argv = [sys.executable, str(ROOT / "tests/rewrite/test_records.py"), "--case", case,
                "--products", str(products), "--sanitizers", "--output", str(item / "run")]
        with (item / "driver.stdout").open("xb") as stdout, (item / "driver.stderr").open("xb") as stderr:
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
            code = waited(child, 60)
        run = item / "run"
        receipt = json.loads((run / "records.receipt.json").read_text())
        diagnostic = (run / "records.stderr").read_text()
        observed = expected in diagnostic
        loaded = receipt["loaded_libraries"].get(str(defective.resolve())) == digest(defective)
        passed = code == 1 and receipt["returncode"] == 1 and not receipt["forced_cleanup"] and observed and loaded
        results.append({"control": name, "passed": passed, "detected_assertion": observed,
                        "actual_defective_library_loaded": loaded, "driver_returncode": code,
                        "driver_pid": child.pid, "driver_reaped": True,
                        "scope": "Shipped Base record/DSET/Runtime/Scheduler/IPC/worker/native/agent path"})
        write_json(output / "results.json", {"passed": all(row["passed"] for row in results),
                   "complete": len(results) == len(args.controls), "controls": results,
                   "inputs": {str(Path(__file__).resolve()): digest(Path(__file__).resolve()),
                              str(args.build_receipt.resolve()): digest(args.build_receipt)},
                   "pending": "Stale-generation control; complete T14 remains pending"})
    passed = all(row["passed"] for row in results)
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
