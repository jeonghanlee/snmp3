#!/usr/bin/env python3
"""Build a diagnostic observer against an explicit SNMP3 module and EPICS Base."""

import argparse
import json
from pathlib import Path
import subprocess

from native_observation import digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module-root", type=Path, required=True)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--arch", default="linux-x86_64")
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("A new output directory is required")
    module, base = args.module_root.resolve(), args.base.resolve()
    source = Path(__file__).with_name("native_observer.cpp").resolve()
    directories = [module / "lib" / args.arch, base / "lib" / args.arch]
    dependencies = [source, module / "snmp3App/src/Runtime.h",
                    base / "include/iocsh.h", directories[0] / "libsnmp3.so",
                    directories[1] / "libdbCore.so", directories[1] / "libCom.so"]
    if any(not path.is_file() for path in dependencies):
        parser.error("Observer source, module headers and installed shared libraries are required")
    identities = {str(path): digest(path) for path in dependencies}
    args.output.mkdir(parents=True, mode=0o700)
    library = args.output.resolve() / "observer.so"
    argv = [args.compiler, "-std=c++11", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra",
            "-I" + str(module / "snmp3App/src"), "-I" + str(base / "include"),
            "-I" + str(base / "include/os/Linux"), "-I" + str(base / "include/compiler/gcc"),
            str(source), *["-L" + str(path) for path in directories],
            "-lsnmp3", "-ldbCore", "-lCom", "-Wl,-rpath," + ":".join(map(str, directories)),
            "-o", str(library)]
    status = 2
    receipt = {"argv": argv, "dependencies": identities}
    try:
        result = subprocess.run(argv, capture_output=True, timeout=60)
        (args.output / "build.stdout").write_bytes(result.stdout)
        (args.output / "build.stderr").write_bytes(result.stderr)
        status = result.returncode
        receipt["native_returncode"] = status
        if any(digest(Path(path)) != expected for path, expected in identities.items()):
            raise ValueError("A build dependency changed during compilation")
        if status == 0:
            receipt["library_sha256"] = digest(library)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        receipt["error"] = str(error)
        if isinstance(error, subprocess.TimeoutExpired):
            (args.output / "build.stdout").write_bytes(error.stdout or b"")
            (args.output / "build.stderr").write_bytes(error.stderr or b"")
        status = 2
    receipt["returncode"] = status
    (args.output / "build.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
