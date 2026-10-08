"""Trace an owned IOC and optionally refuse its Runtime thread at the OS boundary."""
import ctypes
import errno
import os
import platform
import pty
import resource
import signal
import struct
import time

TRACEME, SYSCALL, SETOPTIONS, GETINFO = 0, 24, 0x4200, 0x420e
TRACESYSGOOD, TRACEEXEC, EXITKILL = 1, 0x10, 0x100000
EXEC_EVENT, X86_64 = 4, 0xc000003e
CLONE, CLONE3, CLONE_THREAD = 56, 435, 0x10000
MARKER = b"snmp3 lifecycle: starting activation=1\r\n"
TRACE_SECONDS, REAP_SECONDS = 60, 5
MAX_OUTPUT = 16 * 1024 * 1024
LIBC = ctypes.CDLL(None, use_errno=True)
LIBC.ptrace.restype = ctypes.c_long
LIBC.ptrace.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p, ctypes.c_void_p]


def ptrace(request, pid=0, address=0, data=0):
    ctypes.set_errno(0)
    result = LIBC.ptrace(request, pid, address, data)
    if result == -1 and ctypes.get_errno():
        raise OSError(ctypes.get_errno(), os.strerror(ctypes.get_errno()))
    return result


def trace(argv, env, output, inject):
    """Return evidence on success or failure; never change parent or hard limits."""
    receipt = {"argv": argv, "pid": None, "returncode": None, "child_reaped": False,
               "forced_cleanup": False, "error": None, "cleanup_errors": [], "syscalls": [],
               "inject": inject, "abi": platform.machine(), "started_ns": time.monotonic_ns(),
               "marker_count": 0, "parent_limit_before": list(resource.getrlimit(resource.RLIMIT_NPROC))}
    descriptors, raw = [], bytearray()
    pid = master = None
    saved = pending = None
    old_alarm = None
    reaped = False
    stdout_reserved = False
    stdout_path, stderr_path = output / "ioc.stdout", output / "ioc.stderr"

    def drain():
        if master is None:
            return
        while True:
            try:
                part = os.read(master, 65536)
            except BlockingIOError:
                return
            except OSError as error:
                if error.errno == errno.EIO:
                    return
                raise
            if not part:
                return
            raw.extend(part)
            if len(raw) > MAX_OUTPUT:
                raise RuntimeError("trace output exceeded budget")

    def restore():
        nonlocal saved
        if saved is not None:
            resource.prlimit(pid, resource.RLIMIT_NPROC, saved)
            actual = list(resource.prlimit(pid, resource.RLIMIT_NPROC))
            pending["restored_limit"] = actual
            pending["restored_ns"] = time.monotonic_ns()
            if actual != list(saved):
                raise RuntimeError("child limit restoration mismatch")
            saved = None

    def expired(signum, frame):
        raise TimeoutError("IOC trace exceeded deadline")

    try:
        # Exclusive logs keep a retry from silently replacing earlier evidence.
        stdout_path.touch(exist_ok=False)
        stdout_reserved = True
        error_fd = os.open(stderr_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        descriptors.append(error_fd)
        if platform.system() != "Linux" or platform.machine() != "x86_64" or os.geteuid() == 0:
            raise RuntimeError("NOT RUN: requires non-root Linux x86_64")
        if signal.getitimer(signal.ITIMER_REAL) != (0.0, 0.0):
            raise RuntimeError("NOT RUN: caller has an active real-time timer")
        master, slave = pty.openpty()
        descriptors.extend((master, slave))
        os.set_blocking(master, False)
        pid = os.fork()
        if pid == 0:
            try:
                os.setsid()
                null = os.open("/dev/null", os.O_RDONLY)
                os.dup2(null, 0)
                os.dup2(slave, 1)
                os.dup2(error_fd, 2)
                ptrace(TRACEME)
                os.kill(os.getpid(), signal.SIGSTOP)
                os.execve(argv[0], argv, env)
            except BaseException as error:
                os.write(2, ("trace child: " + repr(error) + "\n").encode())
                os._exit(125)
        receipt["pid"] = pid
        for fd in (slave, error_fd):
            os.close(fd)
            descriptors.remove(fd)
        old_alarm = signal.signal(signal.SIGALRM, expired)
        signal.setitimer(signal.ITIMER_REAL, TRACE_SECONDS)
        _, status = os.waitpid(pid, 0)
        if os.WIFEXITED(status) or os.WIFSIGNALED(status):
            reaped = True
            receipt["returncode"] = os.waitstatus_to_exitcode(status)
            raise RuntimeError("NOT RUN: child exited before initial trace stop")
        if not os.WIFSTOPPED(status) or os.WSTOPSIG(status) != signal.SIGSTOP:
            raise RuntimeError("unexpected initial trace stop")
        ptrace(SETOPTIONS, pid, 0, TRACESYSGOOD | TRACEEXEC | EXITKILL)
        deliver, injected, exec_seen = 0, False, False
        while not reaped:
            ptrace(SYSCALL, pid, 0, deliver)
            _, status = os.waitpid(pid, 0)
            drain()
            if os.WIFEXITED(status) or os.WIFSIGNALED(status):
                reaped = True
                receipt["returncode"] = os.waitstatus_to_exitcode(status)
                break
            if not os.WIFSTOPPED(status):
                raise RuntimeError("unexpected wait status")
            stopped, event = os.WSTOPSIG(status), status >> 16
            deliver = 0
            if stopped == signal.SIGTRAP and event == EXEC_EVENT and not exec_seen:
                exec_seen = True
                continue
            if event or stopped in (signal.SIGTRAP, signal.SIGSTOP):
                raise RuntimeError("unexpected trace event or group stop")
            if stopped != (signal.SIGTRAP | 0x80):
                deliver = stopped
                continue
            info = ctypes.create_string_buffer(88)
            size = ptrace(GETINFO, pid, len(info), ctypes.addressof(info))
            if size < 24 or struct.unpack_from("I", info.raw, 4)[0] != X86_64:
                raise RuntimeError("unexpected syscall ABI")
            op = info.raw[0]
            if op == 1:
                if size < 80 or pending is not None:
                    raise RuntimeError("invalid syscall entry sequence")
                number, *args = struct.unpack_from("7Q", info.raw, 24)
                if number not in (CLONE, CLONE3):
                    continue
                if number == CLONE:
                    flags = args[0]
                else:
                    fd = os.open(f"/proc/{pid}/mem", os.O_RDONLY)
                    try:
                        flags, = struct.unpack("Q", os.pread(fd, 8, args[0]))
                    finally:
                        os.close(fd)
                count = raw.count(MARKER)
                pending = {"syscall": number, "flags": flags, "thread": bool(flags & CLONE_THREAD),
                           "after_starting": count == 1, "limited": False, "at_ns": time.monotonic_ns()}
                receipt["syscalls"].append(pending)
                if count > 1:
                    raise RuntimeError("duplicate Runtime starting marker")
                if inject and count == 1 and flags & CLONE_THREAD and not injected:
                    saved = resource.prlimit(pid, resource.RLIMIT_NPROC)
                    pending.update(limited=True, original_limit=list(saved))
                    resource.prlimit(pid, resource.RLIMIT_NPROC, (0, saved[1]))
                    pending["restricted_limit"] = list(resource.prlimit(pid, resource.RLIMIT_NPROC))
                    if pending["restricted_limit"] != [0, saved[1]]:
                        raise RuntimeError("child limit restriction mismatch")
            elif op == 2:
                if size < 33:
                    raise RuntimeError("incomplete syscall exit information")
                if pending is None:
                    continue
                result, = struct.unpack_from("q", info.raw, 24)
                pending.update(return_value=result, exited_ns=time.monotonic_ns())
                limited = pending["limited"]
                restore()
                if limited:
                    if result == -errno.EAGAIN:
                        injected = True
                    elif pending["syscall"] != CLONE3 or result != -errno.ENOSYS:
                        raise RuntimeError("thread limit did not produce EAGAIN")
                pending = None
            else:
                raise RuntimeError("unexpected syscall information kind")
        if saved is not None:
            raise RuntimeError("child exited with an unrestored limit")
        if not exec_seen or raw.count(MARKER) != 1 or (inject and not injected):
            raise RuntimeError("missing Runtime marker or real kernel refusal")
    except BaseException as error:
        receipt["error"] = type(error).__name__ + ": " + str(error)
    finally:
        if old_alarm is not None:
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, old_alarm)
        if pid and not reaped:
            receipt["forced_cleanup"] = True
            try:
                restore()
            except Exception as error:
                receipt["cleanup_errors"].append("restore: " + repr(error))
            try:
                os.killpg(pid, signal.SIGKILL)
            except OSError as error:
                if error.errno != errno.ESRCH:
                    receipt["cleanup_errors"].append("kill group: " + repr(error))
                try:
                    os.kill(pid, signal.SIGKILL)
                except OSError as error:
                    if error.errno != errno.ESRCH:
                        receipt["cleanup_errors"].append("kill child: " + repr(error))
            deadline = time.monotonic() + REAP_SECONDS
            while time.monotonic() < deadline:
                try:
                    waited, status = os.waitpid(pid, os.WNOHANG)
                except OSError as error:
                    receipt["cleanup_errors"].append("wait: " + repr(error))
                    break
                if waited:
                    if os.WIFEXITED(status) or os.WIFSIGNALED(status):
                        reaped = True
                        receipt["returncode"] = os.waitstatus_to_exitcode(status)
                        break
                    try:
                        ptrace(SYSCALL, pid, 0, signal.SIGKILL)
                    except OSError as error:
                        receipt["cleanup_errors"].append("resume for reap: " + repr(error))
                        break
                time.sleep(0.01)
            if not reaped:
                receipt["cleanup_errors"].append("owned child reap deadline exceeded")
        try:
            drain()
        except (OSError, RuntimeError) as error:
            receipt["cleanup_errors"].append("output: " + repr(error))
        for fd in descriptors:
            try:
                os.close(fd)
            except OSError as error:
                receipt["cleanup_errors"].append("close: " + repr(error))
        if stdout_reserved:
            stdout_path.write_bytes(bytes(raw).replace(b"\r\n", b"\n"))
        receipt.update(child_reaped=reaped, marker_count=raw.count(MARKER), finished_ns=time.monotonic_ns(),
                       parent_limit_after=list(resource.getrlimit(resource.RLIMIT_NPROC)))
    return receipt
