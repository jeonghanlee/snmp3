#!/usr/bin/env python3
"""Observe real CA timestamps or concurrently process existing IOC poll heads.

The API and DBR_TIME_LONG layout follow the selected Base 7.0.10 headers.
Each pulse thread owns a native CA context and connection. No IOC, DB,
dispatcher, completion callback or device path is reproduced in this client.
"""

import argparse
import ctypes as C
import json
from pathlib import Path
import sys
import threading
import time

TIMEOUT = 3.0
DBR_SHORT = 1
DBR_TIME_LONG = 19


class TimeLong(C.Structure):
    _fields_ = [("status", C.c_int16), ("severity", C.c_int16),
                ("seconds", C.c_uint32), ("nanoseconds", C.c_uint32), ("value", C.c_int32)]


def library(path):
    C.CDLL(str(path.parent / "libCom.so"), mode=C.RTLD_GLOBAL)
    ca = C.CDLL(str(path), mode=C.RTLD_GLOBAL)
    signatures = {
        "ca_context_create": ([C.c_int], C.c_int),
        "ca_context_destroy": ([], None),
        "ca_create_channel": ([C.c_char_p, C.c_void_p, C.c_void_p, C.c_uint, C.POINTER(C.c_void_p)], C.c_int),
        "ca_pend_io": ([C.c_double], C.c_int),
        "ca_flush_io": ([], C.c_int),
        "ca_array_get": ([C.c_long, C.c_ulong, C.c_void_p, C.c_void_p], C.c_int),
        "ca_array_put": ([C.c_long, C.c_ulong, C.c_void_p, C.c_void_p], C.c_int),
        "ca_message": ([C.c_int], C.c_char_p),
    }
    for name, (args, result) in signatures.items():
        function = getattr(ca, name)
        function.argtypes, function.restype = args, result
    sizes = (C.c_ushort * (DBR_TIME_LONG + 1)).in_dll(ca, "dbr_size")
    offsets = (C.c_ushort * (DBR_TIME_LONG + 1)).in_dll(ca, "dbr_value_offset")
    if sizes[DBR_TIME_LONG] != C.sizeof(TimeLong) or offsets[DBR_TIME_LONG] != TimeLong.value.offset:
        raise RuntimeError("Installed DBR_TIME_LONG ABI does not match the declared observation structure")
    return ca


def checked(ca, status):
    if status != 1:
        raise RuntimeError(ca.ca_message(status).decode())


def connect(ca, names):
    channels = []
    for name in names:
        channel = C.c_void_p()
        checked(ca, ca.ca_create_channel(name.encode(), None, None, 0, C.byref(channel)))
        channels.append(channel)
    checked(ca, ca.ca_pend_io(TIMEOUT))
    return channels


def snapshot(ca, names):
    checked(ca, ca.ca_context_create(1))
    try:
        channels = connect(ca, names)
        buffers = [TimeLong() for _ in names]
        for channel, data in zip(channels, buffers):
            checked(ca, ca.ca_array_get(DBR_TIME_LONG, 1, channel, C.byref(data)))
        checked(ca, ca.ca_pend_io(TIMEOUT))
        return {name: {"value": data.value, "severity": data.severity, "status": data.status,
                       "secondsPastEpoch1990": data.seconds, "nanoseconds": data.nanoseconds}
                for name, data in zip(names, buffers)}
    finally:
        ca.ca_context_destroy()


def pulse(ca, names):
    barrier = threading.Barrier(len(names))
    observations, errors = {}, []

    def worker(name):
        active = False
        try:
            checked(ca, ca.ca_context_create(1))
            active = True
            channel = connect(ca, [name])[0]
            barrier.wait(timeout=TIMEOUT)
            value = C.c_int16(1)
            start = time.monotonic_ns()
            checked(ca, ca.ca_array_put(DBR_SHORT, 1, channel, C.byref(value)))
            checked(ca, ca.ca_flush_io())
            observations[name] = {"start_ns": start, "flushed_ns": time.monotonic_ns()}
            time.sleep(0.05)
        except Exception as e:
            errors.append(str(e))
            barrier.abort()
        finally:
            if active:
                ca.ca_context_destroy()

    threads = [threading.Thread(target=worker, args=(name,)) for name in names]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    if errors:
        raise RuntimeError(repr(errors))
    return observations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--libca", type=Path, required=True)
    parser.add_argument("mode", choices=("snapshot", "pulse"))
    parser.add_argument("pvs", nargs="+")
    args = parser.parse_args()
    if args.mode == "pulse" and len(args.pvs) != 2:
        parser.error("pulse requires two existing poll-head PROC channels")
    ca = library(args.libca.resolve())
    try:
        result = snapshot(ca, args.pvs) if args.mode == "snapshot" else pulse(ca, args.pvs)
    except Exception as e:
        print(str(e), file=sys.stderr)
        return 1
    print(json.dumps({"mode": args.mode, "observations": result}, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
