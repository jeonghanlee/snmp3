#!/usr/bin/env python3
"""Read numeric and string CA values with native record status and timestamps."""

import argparse
import ctypes as C
import json
import math
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tests/rewrite"))
from apcpdu_grouped_ca import library, checked, connect, TIMEOUT


class TimeDouble(C.Structure):
    _fields_ = [("status", C.c_int16), ("severity", C.c_int16), ("seconds", C.c_uint32),
                ("nanoseconds", C.c_uint32), ("padding", C.c_int32), ("value", C.c_double)]


class TimeString(C.Structure):
    _fields_ = [("status", C.c_int16), ("severity", C.c_int16), ("seconds", C.c_uint32),
                ("nanoseconds", C.c_uint32), ("value", C.c_char * 40)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--libca", type=Path, required=True)
    parser.add_argument("--request", type=Path, required=True)
    args = parser.parse_args()
    request = json.loads(args.request.read_text())
    ca = library(args.libca)
    sizes = (C.c_ushort * 21).in_dll(ca, "dbr_size")
    offsets = (C.c_ushort * 21).in_dll(ca, "dbr_value_offset")
    for kind, layout in ((20, TimeDouble), (14, TimeString)):
        if sizes[kind] != C.sizeof(layout) or offsets[kind] != layout.value.offset:
            raise RuntimeError("Installed CA ABI differs from the selected Base headers")
    checked(ca, ca.ca_context_create(1))
    start = time.monotonic_ns()
    observations = {}
    try:
        for role, kind, layout in (("numeric", 20, TimeDouble), ("strings", 14, TimeString)):
            names = request[role]
            if not names:
                observations[role] = {}
                continue
            channels = connect(ca, names)
            buffers = [layout() for _ in names]
            for channel, data in zip(channels, buffers):
                checked(ca, ca.ca_array_get(kind, 1, channel, C.byref(data)))
            checked(ca, ca.ca_pend_io(TIMEOUT))
            rows = {}
            for name, data in zip(names, buffers):
                value = data.value.decode() if role == "strings" else data.value
                rows[name] = {"value": value if isinstance(value, str) or math.isfinite(value) else None,
                    "status": data.status, "severity": data.severity,
                    "secondsPastEpoch1990": data.seconds, "nanoseconds": data.nanoseconds}
            observations[role] = rows
    finally:
        ca.ca_context_destroy()
    print(json.dumps({"start_ns": start, "end_ns": time.monotonic_ns(), **observations}))


if __name__ == "__main__":
    main()
