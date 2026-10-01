#!/usr/bin/env python3
"""External process-boundary faults; gate modes execute the production helper."""

import json
import os
from pathlib import Path
import sys
import time

EXECUTABLE = Path(sys.argv[0]).absolute()
MODE = EXECUTABLE.stem
PID_FILE = EXECUTABLE.with_suffix(".pid")
RELEASE_FILE = EXECUTABLE.with_suffix(".release")


def main():
    PID_FILE.write_text(json.dumps({"pid": os.getpid(),
                                   "start": Path("/proc/self/stat").read_text().split()[21]}))
    if MODE.startswith("gate-"):
        deadline = time.monotonic() + 10
        while not RELEASE_FILE.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        if not RELEASE_FILE.exists():
            return 8
        probe = (EXECUTABLE.parent / "production-probe.txt").read_text().strip()
        os.execv(probe, [probe, "--capabilities"])
    if MODE == "probe-fail":
        return 9
    if MODE == "probe-malformed":
        os.write(1, b'{"schema":')
        return 0
    if MODE == "probe-oversized":
        os.write(1, b"x" * (64 * 1024 + 1))
        return 0
    if MODE == "probe-closed-output":
        os.close(1)
        time.sleep(30)
        return 0
    if MODE == "probe-timeout":
        time.sleep(30)
        return 0
    if MODE == "probe-schema":
        os.write(1, b'{"schema":2,"version":"fault","authentication":[],"privacy":[]}')
        return 0
    if MODE == "probe-duplicate":
        os.write(1, b'{"schema":1,"schema":1}')
        return 0
    return 2


if __name__ == "__main__":
    sys.exit(main())
