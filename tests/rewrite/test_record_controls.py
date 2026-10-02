#!/usr/bin/env python3
"""Build defective support copies and require shipped record tests to detect them."""
import argparse
import json
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-receipt", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--controls", nargs="+", choices=tuple(CONTROLS), default=list(CONTROLS))
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    jobs = json.loads(args.build_receipt.read_text())["products"]
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
