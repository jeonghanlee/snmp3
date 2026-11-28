"""Hold one genuine worker Retired send at the Linux syscall boundary."""
import os
from pathlib import Path
import platform
import signal
import struct
import time

try:
    from .runtime_thread_limit import ptrace, SyscallTracker, SYSCALL, TRACESYSGOOD
except ImportError:  # run as a script: the helpers directory is the first entry of sys.path
    from runtime_thread_limit import ptrace, SyscallTracker, SYSCALL, TRACESYSGOOD

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
        tracker = SyscallTracker()
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
            info = tracker.read(pid)
            if info["op"] != 1:
                continue
            number, args = info["number"], info["args"]
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


def main():
    """Run a command and hold its worker's Retired sends.

    --marker MARKER --worker WORKER --windows N --receipt FILE [--timeout SECONDS] -- COMMAND...
    The command keeps the descriptors and the environment of this process. The receipt of `hold` is written to FILE.
    The exit status is the one of the command; 124 when the command had to be killed after SECONDS."""
    import argparse
    import json
    import subprocess
    parser = argparse.ArgumentParser(description=main.__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--marker", required=True, type=Path)
    parser.add_argument("--worker", required=True, type=Path)
    parser.add_argument("--windows", required=True, type=int)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=None)
    parser.add_argument("argv", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.argv[1:] if args.argv[:1] == ["--"] else args.argv
    if not command or args.windows < 1:
        parser.error("a command and a positive number of windows are required")
    child = subprocess.Popen(command)
    forced = False
    try:
        receipt = hold(child, args.worker, args.marker, args.windows)
        args.receipt.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
        try:
            code = child.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            forced = True
            child.kill()
            code = child.wait(timeout=10)
    except BaseException:
        if child.poll() is None:
            child.kill()
            child.wait()
        raise
    if forced:
        return 124
    return code if code >= 0 else 128 - code


if __name__ == "__main__":
    raise SystemExit(main())
