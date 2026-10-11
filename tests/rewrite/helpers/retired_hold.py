"""Hold one genuine worker Retired send at the Linux syscall boundary."""
import ctypes
import os
from pathlib import Path
import platform
import signal
import struct
import time

from .runtime_thread_limit import ptrace, SYSCALL, GETINFO, TRACESYSGOOD, X86_64

SEIZE, INTERRUPT, DETACH = 0x4206, 0x4207, 17
SENDTO, HEADER_BYTES = 44, 64
WALL = 0x40000000
TIMEOUT = 12


def hold(child, worker, marker, windows=1):
    """Trace only the identified owned worker; never alter its registers or bytes.

    Holds `windows` consecutive Retired sends. Window 1 uses the markers `.held` and `.release`;
    window n > 1 uses `.held.n` and `.release.n`. The top-level receipt fields describe window 1,
    `windows_held` lists every window and the trace detaches after the last release."""
    receipt = {"worker_pid": None, "attached": False, "held": False, "released": False,
               "detached": False, "error": None, "cleanup_error": None, "windows_held": []}
    window = 1
    pid, attached, stopped = None, False, False
    deadline = time.monotonic() + TIMEOUT

    def path(suffix):
        return Path(str(marker) + suffix)

    def numbered(base, number):
        return path(base if number == 1 else f"{base}.{number}")

    def wait_stop(end):
        # A blocking wait with an interval timer for the bound: sleeping between polls would add a
        # millisecond to every traced syscall and move the timing of the code under test.
        nonlocal stopped
        remaining = end - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("worker trace stop deadline exceeded")

        def expired(signum, frame):
            raise TimeoutError("worker trace stop deadline exceeded")

        previous = signal.signal(signal.SIGALRM, expired)
        signal.setitimer(signal.ITIMER_REAL, remaining)
        try:
            _, status = os.waitpid(pid, WALL)
        finally:
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, previous)
        if not os.WIFSTOPPED(status):
            raise RuntimeError("worker exited while traced")
        stopped = True
        return status

    try:
        if platform.system() != "Linux" or platform.machine() != "x86_64":
            raise RuntimeError("NOT RUN: Retired hold requires Linux x86_64")
        while not path(".pid").exists():
            if child.poll() is not None or time.monotonic() >= deadline:
                raise RuntimeError("owned worker marker absent")
            time.sleep(0.001)
        pid = int(path(".pid").read_text())
        status = Path(f"/proc/{pid}/status").read_text()
        parent = next(int(line.split()[1]) for line in status.splitlines() if line.startswith("PPid:"))
        if parent != child.pid or Path(f"/proc/{pid}/exe").resolve() != worker.resolve():
            raise RuntimeError("worker ownership or executable mismatch")
        receipt["worker_pid"] = pid
        ptrace(SEIZE, pid, 0, TRACESYSGOOD)
        attached = True
        receipt.update(attached=True, attached_ns=time.monotonic_ns())
        ptrace(INTERRUPT, pid)
        wait_stop(deadline)
        path(".attached").write_text("ready\n")
        deliver = 0
        while time.monotonic() < deadline:
            ptrace(SYSCALL, pid, 0, deliver)
            stopped = False
            status = wait_stop(deadline)
            sig, event = os.WSTOPSIG(status), status >> 16
            deliver = 0
            if event:
                raise RuntimeError("unexpected worker trace event")
            if sig != (signal.SIGTRAP | 0x80):
                if sig in (signal.SIGTRAP, signal.SIGSTOP):
                    raise RuntimeError("unexpected worker group stop")
                deliver = sig
                continue
            info = ctypes.create_string_buffer(88)
            size = ptrace(GETINFO, pid, len(info), ctypes.addressof(info))
            if size < 24 or struct.unpack_from("I", info.raw, 4)[0] != X86_64:
                raise RuntimeError("unexpected worker syscall ABI")
            if info.raw[0] != 1:
                continue
            if size < 80:
                raise RuntimeError("incomplete worker syscall entry")
            number, *args = struct.unpack_from("7Q", info.raw, 24)
            if number != SENDTO or args[2] != HEADER_BYTES:
                continue
            descriptor = os.open(f"/proc/{pid}/mem", os.O_RDONLY)
            try:
                header = os.pread(descriptor, HEADER_BYTES, args[1])
            finally:
                os.close(descriptor)
            if len(header) != HEADER_BYTES:
                raise RuntimeError("incomplete actual send header")
            fields = struct.unpack("!IHHII6Q", header)
            if fields[:3] != (0x46534e35, 1, 5):
                continue
            seen = dict(held_ns=time.monotonic_ns(), activation=fields[5], epoch=fields[6], address=fields[7],
                        revision=fields[8], batch=fields[9], sequence=fields[10], syscall=number, send_bytes=args[2])
            if window == 1:
                receipt.update(held=True, **seen)
            numbered(".held", window).write_text("held\n")
            while not numbered(".release", window).exists():
                if child.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Retired release marker absent")
                time.sleep(0.001)
            seen["released_ns"] = time.monotonic_ns()
            receipt["windows_held"].append(seen)
            if window < windows:
                window += 1
                deadline = time.monotonic() + TIMEOUT
                continue
            ptrace(DETACH, pid)
            attached, stopped = False, False
            receipt.update(released=True, detached=True, released_ns=seen["released_ns"])
            return receipt
        raise TimeoutError("actual Retired send not observed")
    except BaseException as error:
        receipt["error"] = type(error).__name__ + ": " + str(error)
    finally:
        if attached:
            try:
                if not stopped:
                    ptrace(INTERRUPT, pid)
                    wait_stop(time.monotonic() + 5)
                ptrace(DETACH, pid)
                receipt["detached"] = True
            except BaseException as error:
                receipt["cleanup_error"] = type(error).__name__ + ": " + str(error)
    return receipt
