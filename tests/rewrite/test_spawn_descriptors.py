#!/usr/bin/env python3
"""Check that the address worker inherits no descriptor beyond its standard streams and its IPC channel.

The shipped IOC starts with two extra descriptors that are inheritable (two pipes passed to the IOC). The runner
reads /proc of the IOC and of its worker through the real spawn path and requires that both pipes are open in the
IOC and absent in the worker, that descriptors 0 and 1 are /dev/null, and that descriptor 3 is the IPC socket.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
ARCH = "linux-x86_64"


def base_path():
    for line in (ROOT / "configure/RELEASE.local").read_text().splitlines():
        if line.strip().startswith("EPICS_BASE") and "=" in line:
            return line.split("=", 1)[1].strip()
    raise SystemExit("configure/RELEASE.local names no EPICS_BASE")


def links(pid):
    out = {}
    for name in os.listdir(f"/proc/{pid}/fd"):
        try:
            out[int(name)] = os.readlink(f"/proc/{pid}/fd/{name}")
        except OSError:
            pass
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    base = base_path()
    config = output / "config"
    config.mkdir(mode=0o700)
    shutil.copy(ROOT / "tests/rewrite/config/v2c.json", config / "v2c.json")
    secret = config / "community.txt"
    secret.write_text("public\n")
    secret.chmod(0o600)
    env = dict(os.environ)
    for key in ("LD_PRELOAD", "LD_AUDIT", "LD_DEBUG", "LD_LIBRARY_PATH"):
        env.pop(key, None)
    env.update(LD_LIBRARY_PATH=f"{ROOT}/lib/{ARCH}:{base}/lib/{ARCH}", CONFIG_JSON=str(config / "v2c.json"),
               NATIVE_PROBE=str(ROOT / f"bin/{ARCH}/snmp3NativeProbe"), WORKER_PATH=str(ROOT / f"bin/{ARCH}/snmp3Worker"))
    markers = []
    for _ in range(2):
        r, w = os.pipe()
        os.set_inheritable(r, True)
        os.set_inheritable(w, True)
        markers += [r, w]
    stdout = (output / "ioc.stdout").open("wb")
    stderr = (output / "ioc.stderr").open("wb")
    ioc = subprocess.Popen([str(ROOT / f"bin/{ARCH}/snmp3Ioc"), str(ROOT / "tests/rewrite/config/startup.cmd")], cwd=ROOT,
                           env=env, stdin=subprocess.PIPE, stdout=stdout, stderr=stderr, pass_fds=markers)
    checks = {}
    detail = {}
    try:
        marker_targets = {os.readlink(f"/proc/self/fd/{fd}") for fd in markers}
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline and ioc.poll() is None:
            if b"AfterInitialProcess" in (output / "ioc.stdout").read_bytes():
                break
            time.sleep(0.1)
        ioc.stdin.write(b"snmp3RuntimeReport\n")
        ioc.stdin.flush()
        pid = 0
        while time.monotonic() < deadline and ioc.poll() is None:
            found = re.findall(r"snmp3 worker: address=\d+ epoch=\d+ pid=(\d+) ready=1", (output / "ioc.stdout").read_text(errors="replace"))
            if found:
                pid = int(found[-1])
                break
            time.sleep(0.1)
        checks["worker-ready"] = pid > 0
        if pid:
            parent, child = links(ioc.pid), links(pid)
            detail = {"ioc_fds": sorted(parent), "worker_fds": {str(k): v for k, v in sorted(child.items())}}
            checks["markers-open-in-ioc"] = marker_targets <= set(parent.values())
            checks["markers-absent-in-worker"] = not (marker_targets & set(child.values()))
            checks["worker-stdin-stdout-null"] = child.get(0) == "/dev/null" and child.get(1) == "/dev/null"
            checks["worker-fd3-socket"] = child.get(3, "").startswith("socket:")
            checks["worker-no-other-pipe"] = not any(v.startswith("pipe:") for v in child.values())
    finally:
        try:
            ioc.stdin.write(b"exit\n")
            ioc.stdin.close()
        except (BrokenPipeError, ValueError, OSError):
            pass
        try:
            ioc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            ioc.kill()
            ioc.wait()
        stdout.close()
        stderr.close()
        for fd in markers:
            os.close(fd)
    passed = bool(checks) and all(checks.values())
    (output / "results.json").write_text(json.dumps({"passed": passed, "checks": checks, "detail": detail,
                                                      "ioc_returncode": ioc.returncode}, indent=1) + "\n")
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
