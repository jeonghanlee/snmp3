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


def hold(child, worker, marker):
    """Trace only the identified owned worker; never alter its registers or bytes."""
    receipt = {"worker_pid": None, "attached": False, "held": False, "released": False,
               "detached": False, "error": None, "cleanup_error": None}
    pid, attached, stopped = None, False, False
    deadline = time.monotonic() + TIMEOUT

    def path(suffix):
        return Path(str(marker) + suffix)

    def wait_stop(end):
        nonlocal stopped
        while time.monotonic() < end:
            found, status = os.waitpid(pid, os.WNOHANG | WALL)
            if found:
                if not os.WIFSTOPPED(status):
                    raise RuntimeError("worker exited while traced")
                stopped = True
                return status
            time.sleep(0.001)
        raise TimeoutError("worker trace stop deadline exceeded")

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
            receipt.update(held=True, held_ns=time.monotonic_ns(), activation=fields[5],
                           epoch=fields[6], address=fields[7], revision=fields[8], batch=fields[9],
                           sequence=fields[10], syscall=number, send_bytes=args[2])
            path(".held").write_text("held\n")
            while not path(".release").exists():
                if child.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Retired release marker absent")
                time.sleep(0.001)
            ptrace(DETACH, pid)
            attached, stopped = False, False
            receipt.update(released=True, detached=True, released_ns=time.monotonic_ns())
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
