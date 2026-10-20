#!/usr/bin/env python3
"""Verify the generated AP8932 IOC against delayed replies of the outer SNMP fixture.

Contract. The generated configuration sets the profile timeout (timeoutMs) and no retries. The outer fixture
delays every reply by a declared time and records, per request, its arrival, its reply time and its OIDs.
  within-timeout (0.4 x timeout): the public values reach the independently expected scaled values (the fixture
    values and the declared scales), each published timestamp follows the reply of the request that carried
    it, the fixture never holds more than one request without a reply, and no SET is sent.
  beyond-timeout (1.5 x timeout): the reply arrives after the library gave up, so the public values stay INVALID
    during the observation window. The supervisor may contain and relaunch the worker in this phase; the number
    of session changes seen by the fixture is recorded, not judged.
  recovery: with the delay removed the expected values return.
  shutdown: every launched worker is reaped (launches equal reaps), none is forced, the queues are empty.
Outcomes. Failed is an observed violation of a claim. Incomplete is missing evidence: too few requests, a measured
delay outside the declared one, or a causality check that cannot tell an early from a late publication. Exit code
0 passed, 1 failed, 2 incomplete. The measured delay of every request, not the configured one, is the exposure.
Controls (--control) change only the outer fixture to show that a claim can fail: late-accepted runs the beyond-timeout
phase with a delay below the timeout, so the late replies are accepted; extra-request adds foreign requests that
overlap the library's request. The expected outcome of each control is a failure of the named claim.
"""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import re
import socket
import statistics
import threading
import time
import traceback

from build_apcpdu import ARCH, ROOT, digest, load_peer, write_json
from test_apcpdu import Runner, WAIT_SECONDS, port

OBSERVE_SECONDS = 10
MINIMUM_REQUESTS = 50
CAUSAL_LAG_SECONDS = 0.2
EXPECTED = {"phase.current.value": 12, "phase.voltage.value": 208, "phase.power.value": 2.5,
            "phase.apparent_power.value": 2.6, "phase.power_factor.value": 0.96, "bank1.current.value": 6,
            "bank2.current.value": 6, "limits.device_power.near.value": 3, "limits.device_power.over.value": 3.5}
CURRENT_OID = "rPDU2PhaseStatusCurrent.1"


class Incomplete(Exception):
    pass


class DelayRunner(Runner):
    def __init__(self, ioc, output, control="none", marks=False, watch=False):
        super().__init__(ioc, output)
        self.control, self.marks, self.watch = control, marks, watch
        document = json.loads((ioc / "config.template.json").read_text())
        self.timeout = document["profiles"][0]["timeoutMs"] / 1000.0
        if document["profiles"][0].get("retries") != 0:
            raise ValueError("The delay contract assumes no retries")
        self.within, self.beyond = 0.4 * self.timeout, 1.5 * self.timeout

    def missing(self, name, detail):
        self.checks.append({"name": name, "passed": None, "detail": detail})
        print(f"[ INCOMPLETE ] {name}", flush=True)
        raise Incomplete(f"{name}: {detail}")

    @contextmanager
    def delayed_fixture(self, name):
        folder = self.output / name
        folder.mkdir()
        module = load_peer(self.ioc / "source/tests/snmp_peer.py")
        peer = module.Pdu(0, folder)
        peer.delay_s, peer.timeline, peer.outstanding, peer.max_outstanding = 0.0, [], 0, 0
        gate = threading.Lock()

        class Delayed:
            def __init__(self, sock, row):
                self.sock, self.row = sock, row

            def sendto(self, data, address):
                time.sleep(peer.delay_s)
                self.row["reply_ns"] = time.time_ns()
                self.sock.sendto(data, address)

        class Handler(module.Handler):
            def handle(self):
                data, sock = self.request
                row = {"arrival_ns": time.time_ns(), "reply_ns": None, "names": [], "source_port": self.client_address[1]}
                try:
                    request = list(module.items(next(module.items(data))[1]))[2]
                    fields = list(module.items(request[1]))
                    for _, binding, _ in module.items(fields[3][1]):
                        oid = list(module.items(binding))[0][1]
                        row["names"].append(peer.names.get(oid, oid.hex()))
                    row["pdu"] = request[0]
                except Exception as error:
                    row["parse_error"] = repr(error)
                with gate:
                    peer.outstanding += 1
                    peer.max_outstanding = max(peer.max_outstanding, peer.outstanding)
                self.request = (data, Delayed(sock, row))
                try:
                    super().handle()
                finally:
                    with gate:
                        peer.outstanding -= 1
                        peer.timeline.append(row)

        peer.RequestHandlerClass = Handler
        try:
            config = self.config(folder, peer.server_address[1])
            script = folder / "startup.cmd"
            script.write_text(self.header() +
                              f'iocshLoad("{self.ioc}/iocsh/device.cmd","CONFIG={config}")\n' +
                              'iocInit\npvxsi\nsnmp3ConfigReport\nsnmp3RuntimeReport\necho LOCAL_IOC_READY\n')
            with self.process(name, [self.binary, script]) as child:
                yield peer, child
        finally:
            peer.close()
            with gate:
                rows = list(peer.timeline)
            (folder / "timeline.jsonl").write_text("".join(json.dumps(row) + "\n" for row in rows))
            write_json(folder / "peer.json", {"errors": peer.errors, "requests": len(peer.requests),
                       "sets": sum(kind == 0xA3 for kind, _ in peer.requests),
                       "max_outstanding_run": peer.max_outstanding, "socket_closed": peer.fileno() == -1})

    def phase_rows(self, peer, start_ns):
        return [row for row in list(peer.timeline) if row["arrival_ns"] >= start_ns and row["reply_ns"]]

    def exposure(self, name, rows, low, high):
        if len(rows) < MINIMUM_REQUESTS:
            self.missing(name + ":requests-observed", f"{len(rows)} replies, fewer than {MINIMUM_REQUESTS}")
        delays = [(row["reply_ns"] - row["arrival_ns"]) / 1e9 for row in rows]
        summary = {"requests": len(rows), "delay_min_s": min(delays), "delay_median_s": statistics.median(delays),
                   "delay_max_s": max(delays)}
        if min(delays) < low or (high is not None and max(delays) >= high):
            self.missing(name + ":measured-delay", summary)
        return summary

    @contextmanager
    def foreign_requests(self, peer):
        stop = threading.Event()
        module_oid = peer.resolve("rPDU2PhaseStatusVoltage") + (1,)

        def send():
            from build_apcpdu import load_peer as _load
            module = _load(self.ioc / "source/tests/snmp_peer.py")
            oid = module.oid_bytes(module_oid)
            varbind = module.tlv(0x30, module.tlv(0x06, oid) + module.tlv(0x05, b""))
            number = 1
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                while not stop.is_set():
                    request = module.tlv(0xA0, module.integer(number) + module.integer(0) + module.integer(0) +
                                         module.tlv(0x30, varbind))
                    message = module.tlv(0x30, module.integer(1) + module.tlv(0x04, b"apc-local-test") + request)
                    sock.sendto(message, peer.server_address)
                    number += 1
                    time.sleep(0.02)
        thread = threading.Thread(target=send, daemon=True)
        if self.control == "extra-request":
            thread.start()
        try:
            yield
        finally:
            stop.set()
            if thread.is_alive():
                thread.join(timeout=2)

    @contextmanager
    def watch_children(self, ioc_pid):
        """Record the time at which the set of child processes of the IOC changes, without touching the IOC."""
        changes, stop = [], threading.Event()

        def children():
            found = set()
            for entry in Path("/proc").iterdir():
                if entry.name.isdigit():
                    try:
                        fields = (entry / "stat").read_text().rsplit(")", 1)[1].split()
                        if int(fields[1]) == ioc_pid:
                            found.add(int(entry.name))
                    except (OSError, IndexError, ValueError):
                        continue
            return found

        def watch():
            last = None
            while not stop.is_set():
                now = children()
                if now != last:
                    changes.append({"time_ns": time.time_ns(), "pids": sorted(now)})
                    last = now
                time.sleep(0.25)
        thread = threading.Thread(target=watch, daemon=True)
        thread.start()
        try:
            yield changes
        finally:
            stop.set()
            thread.join(timeout=2)

    @contextmanager
    def watcher(self, ioc_pid):
        if not self.watch:
            yield []
            return
        with self.watch_children(ioc_pid) as changes:
            yield changes

    def mark(self, child, label):
        if not self.marks:
            return
        child.stdin.write(f"echo MARK_{label}\nsnmp3RuntimeReport\n")
        child.stdin.flush()

    def launches(self, name):
        """Worker epoch and launch count reported after each mark of the IOC log."""
        result, label = {}, None
        for line in (self.output / (name + ".log")).read_text().splitlines():
            if line.startswith("MARK_"):
                label = line[5:].strip()
            elif label and line.startswith("snmp3 worker: address=1"):
                fields = dict(item.split("=", 1) for item in line.split()[3:] if "=" in item)
                result[label] = {"epoch": int(fields["epoch"]), "launches": int(fields["launches"])}
                label = None
        return result

    @staticmethod
    def settled(values):
        return (all(abs(values.get(key, -1e9) - value) < 1e-9 for key, value in EXPECTED.items()) and
                values.get("phase.current.alarm.severity") == 0)

    def delay_shutdown_evidence(self):
        log = (self.output / "delay.log").read_text()
        worker = re.search(r"pid=0 ready=0 closing=[01] batch=0 launches=(\d+) reaps=(\d+) forced=(\d+)", log)
        self.check("delay:worker-reaped", worker is not None and worker.group(1) == worker.group(2) and
                   worker.group(3) == "0", worker.groups() if worker else None)
        self.check("delay:queues-empty",
                   "state=Stopped admission=0 activation=1 created=1 exited=1 joined=1" in log and
                   "active=0 pending=0 queued=0 running=0 inert=0 entered=0" in log and "count=0 bytes=0" in log)

    def poll(self):
        end, snapshots = time.monotonic() + OBSERVE_SECONDS, []
        while time.monotonic() < end:
            snapshots.append(self.group()[0])
            time.sleep(0.5)
        return snapshots

    def delay_case(self):
        summary = {"timeout_s": self.timeout, "within_s": self.within, "beyond_s": self.beyond}
        starts = []
        with self.delayed_fixture("delay") as (peer, child), self.watcher(child.pid) as changes:
            starts.append(("cold", time.time_ns()))
            values, _ = self.wait_group(lambda v: v.get("phase.current.alarm.severity") == 3)
            self.check("cold-start:unavailable", values["phase.current.alarm.severity"] == 3)
            self.mark(child, "cold")

            peer.delay_s, start = self.within, time.time_ns()
            starts.append(("within", start))
            with peer.lock:
                peer.max_outstanding = peer.outstanding
            foreign = self.foreign_requests(peer)
            foreign.__enter__()
            peer.enabled = True
            values, _ = self.wait_group(self.settled)
            for key, value in EXPECTED.items():
                self.check("within:" + key, abs(values[key] - value) < 1e-9, values[key])
            snapshots = self.poll()
            self.check("within:stays-valid", all(s["phase.current.alarm.severity"] == 0 and
                       s["phase.current.value"] == 12 for s in snapshots), len(snapshots))
            rows = self.phase_rows(peer, start)
            summary["within"] = self.exposure("within", rows, self.within - 0.002, self.timeout)
            replies = sorted(row["reply_ns"] for row in rows if CURRENT_OID in row["names"])
            periods = [(b - a) / 1e9 for a, b in zip(replies, replies[1:])]
            if not periods or statistics.median(periods) < 2 * CAUSAL_LAG_SECONDS * 2:
                self.missing("within:causality-resolution", {"median_period_s": statistics.median(periods) if periods else None})
            lags = []
            for snapshot in snapshots:
                stamp = (snapshot["phase.current.timeStamp.secondsPastEpoch"] * 10**9 +
                         snapshot["phase.current.timeStamp.nanoseconds"])
                earlier = [r for r in replies if r <= stamp]
                lags.append((stamp - earlier[-1]) / 1e9 if earlier else None)
            summary["within"]["published_after_reply_lag_s"] = [lag for lag in lags]
            self.check("within:published-after-its-reply", all(lag is not None and 0 <= lag <= CAUSAL_LAG_SECONDS
                       for lag in lags), lags)
            foreign.__exit__(None, None, None)
            self.mark(child, "within")
            with peer.lock:
                self.check("within:one-request-outstanding", peer.max_outstanding <= 1, peer.max_outstanding)
            self.check("within:no-session-change", len({row["source_port"] for row in rows}) == 1,
                       sorted({row["source_port"] for row in rows}))

            peer.delay_s, start = (self.within if self.control == "late-accepted" else self.beyond), time.time_ns()
            starts.append(("beyond", start))
            try:
                self.wait_group(lambda v: v.get("phase.current.alarm.severity") == 3)
                invalid = True
            except AssertionError:
                invalid = False
            self.check("beyond:public-invalid", invalid, "the public value did not become INVALID")
            snapshots = self.poll()
            self.check("beyond:late-replies-not-published",
                       all(s["phase.current.alarm.severity"] == 3 for s in snapshots), len(snapshots))
            time.sleep(self.beyond + 0.1)
            self.mark(child, "beyond")
            summary["beyond"] = self.exposure("beyond", self.phase_rows(peer, start), self.beyond - 0.002, None)

            peer.delay_s = 0.0
            starts.append(("recovery", time.time_ns()))
            values, _ = self.wait_group(self.settled)
            for key, value in EXPECTED.items():
                self.check("recovery:" + key, abs(values[key] - value) < 1e-9, values[key])
            self.mark(child, "recovery")
            with peer.lock:
                self.check("wire:no-unintended-set", bool(peer.requests) and not peer.errors and
                           not any(kind == 0xA3 for kind, _ in peer.requests))
        with open(self.output / "delay" / "timeline.jsonl") as stream:
            ordered = sorted((json.loads(line) for line in stream), key=lambda row: row["arrival_ns"])
        changes_of_port, last = [], None
        for row in ordered:
            if row["source_port"] != last:
                changes_of_port.append({"arrival_ns": row["arrival_ns"], "source_port": row["source_port"]})
                last = row["source_port"]
        summary["source_port_changes"] = changes_of_port
        summary["phase_starts_ns"] = starts
        summary["child_process_changes"] = changes
        self.check("delay:normal-exit", self.processes[-1]["returncode"] == 0 and not self.processes[-1]["forced"])
        summary["worker_launches_after_phase"] = self.launches("delay")
        write_json(self.output / "summary.json", summary)
        self.delay_shutdown_evidence()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ioc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--marks", action="store_true",
                        help="ask the IOC for a worker report at the end of each phase (changes the timing)")
    parser.add_argument("--watch-children", action="store_true",
                        help="record the child processes of the IOC every 0.25 s (changes the timing)")
    parser.add_argument("--control", choices=("none", "late-accepted", "extra-request"), default="none",
                        help="change only the outer fixture to show that a claim can fail")
    args = parser.parse_args()
    ioc, output = args.ioc.resolve(), args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / "work"):
        parser.error("--output must be a new directory under this checkout's work/")
    output.mkdir(parents=True)
    runner = DelayRunner(ioc, output, args.control, args.marks, args.watch_children)
    error, outcome = None, "PASS"
    try:
        for path, expected in runner.build["products"].items():
            if digest(path) != expected:
                raise ValueError(f"Build product changed: {path}")
        for name, expected in runner.build["source_hashes"].items():
            if digest(ioc / "source" / name) != expected:
                raise ValueError(f"Exported source changed: {name}")
        runner.check("products:build-identities", True)
        with runner.process("repeater", [runner.base / "bin" / ARCH / "caRepeater"]):
            runner.delay_case()
    except Incomplete:
        error, outcome = traceback.format_exc(), "INCOMPLETE"
    except Exception:
        error, outcome = traceback.format_exc(), "FAIL"
    finally:
        if error:
            (output / "error.txt").write_text(error)
            print(error, flush=True)
        write_json(output / "result.json", {"case": "delay", "control": args.control, "outcome": outcome, "checks": runner.checks, "error": error,
                   "processes": runner.processes, "runner_sha256": digest(__file__),
                   "build_sha256": digest(ioc / "build.json"),
                   "limits": ["No hardware or legacy comparator", "Loopback fixture only", "One address only",
                              "Reordered replies are not possible with one request outstanding per address"]})
    print(f"Evidence: {output}; checks: {len(runner.checks)}; result: {outcome}")
    return {"PASS": 0, "FAIL": 1, "INCOMPLETE": 2}[outcome]


if __name__ == "__main__":
    raise SystemExit(main())
