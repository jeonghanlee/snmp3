#!/usr/bin/env python3
"""Sample the resources of the IOC and of every address worker while a sustained run goes on.

The sampler runs in a thread of run_module_database.py. Every interval it
  - reads /proc for the IOC and for each worker PID named by the `snmp3 worker:` lines of snmp3RuntimeReport:
    resident set size, threads, open descriptors, socket descriptors and CPU time;
  - asks the IOC for snmp3RuntimeReport and records the queue, record and loop counters.

It writes two CSV files into the case directory:
  resources.csv  one row per process and sample
  counters.csv   one row per address and sample (queue counters), with the record and loop counters repeated

The `sockets` column counts every socket descriptor of the process, the IPC channel included. It stands in for the
native session count, which the runtime report does not print.
"""
import csv
import os
import threading
import time
from pathlib import Path

CLOCK_TICKS = os.sysconf("SC_CLK_TCK")
RESOURCE_COLUMNS = ["t_s", "phase", "process", "pid", "rss_kb", "threads", "fds", "sockets", "cpu_s"]
COUNTER_COLUMNS = ["t_s", "phase", "address", "count", "bytes", "queued", "active", "undelivered",
                   "retirement_pending", "behind_retirement", "records_active", "records_pending",
                   "records_completions", "records_enqueue_failures", "loop_passes", "state"]


def fields(line):
    """The key=value pairs of one runtime report line."""
    out = {}
    for part in line.split():
        if "=" in part:
            key, _, value = part.partition("=")
            out[key] = value
    return out


def parse_report(lines):
    """The newest snmp3RuntimeReport block of the lines: runtime, records, queue/worker per address, loop."""
    ends = [i for i, line in enumerate(lines) if line.startswith("snmp3 loop:")]
    if not ends:
        return None
    block = lines[(ends[-2] + 1 if len(ends) > 1 else 0):ends[-1] + 1]
    report = {"runtime": {}, "records": {}, "queue": [], "worker": [], "loop": {}}
    for line in block:
        for name in ("runtime", "records", "loop"):
            if line.startswith(f"snmp3 {name}:"):
                report[name] = fields(line)
        for name in ("queue", "worker"):
            if line.startswith(f"snmp3 {name}:"):
                report[name].append(fields(line))
    return report


def read_process(pid):
    """RSS, threads, descriptors, sockets and CPU seconds of one process, or None when it is gone."""
    root = Path("/proc") / str(pid)
    try:
        status = {}
        for line in (root / "status").read_text().splitlines():
            key, _, value = line.partition(":")
            status[key] = value.strip()
        stat = (root / "stat").read_text()
        after = stat[stat.rindex(")") + 2:].split()
        cpu = (int(after[11]) + int(after[12])) / CLOCK_TICKS
        fds = os.listdir(root / "fd")
        sockets = 0
        for fd in fds:
            try:
                if os.readlink(root / "fd" / fd).startswith("socket:"):
                    sockets += 1
            except OSError:
                pass
        return {"rss_kb": int(status["VmRSS"].split()[0]), "threads": int(status["Threads"]), "fds": len(fds),
                "sockets": sockets, "cpu_s": round(cpu, 2)}
    except (OSError, KeyError, ValueError, IndexError):
        return None


class Sampler:
    """Samples a started IOC every `interval` seconds until stop()."""

    def __init__(self, ioc, directory, interval):
        self.ioc = ioc
        self.interval = interval
        self.phase = "start"
        self.samples = 0
        self.missed = 0
        self.stopped = threading.Event()
        self.origin = time.monotonic()
        self.resources_path = Path(directory) / "resources.csv"
        self.counters_path = Path(directory) / "counters.csv"
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self):
        self.thread.start()

    def stop(self):
        self.stopped.set()
        self.thread.join(timeout=self.interval + 10)

    def run(self):
        with self.resources_path.open("w", newline="") as r, self.counters_path.open("w", newline="") as c:
            resources = csv.writer(r)
            counters = csv.writer(c)
            resources.writerow(RESOURCE_COLUMNS)
            counters.writerow(COUNTER_COLUMNS)
            next_time = time.monotonic()
            while not self.stopped.is_set():
                self.sample(resources, counters)
                r.flush()
                c.flush()
                next_time += self.interval
                self.stopped.wait(max(0.0, next_time - time.monotonic()))

    def sample(self, resources, counters):
        if not self.ioc.alive():
            return
        t = round(time.monotonic() - self.origin, 1)
        report = None
        try:
            self.ioc.report()
            report = parse_report(self.ioc_lines())
        except (OSError, ValueError, BrokenPipeError):
            report = None
        if report is None:
            self.missed += 1
        targets = [("ioc", self.ioc.process.pid)]
        if report:
            for worker in report["worker"]:
                pid = int(worker.get("pid", "0"))
                if pid > 0:
                    targets.append((f"worker-{worker.get('address', '?')}", pid))
        for name, pid in targets:
            row = read_process(pid)
            if row:
                resources.writerow([t, self.phase, name, pid, row["rss_kb"], row["threads"], row["fds"],
                                    row["sockets"], row["cpu_s"]])
        if report:
            records = report["records"]
            loop = report["loop"].get("passes", "")
            for queue in report["queue"]:
                counters.writerow([t, self.phase, queue.get("address", ""), queue.get("count", ""),
                                   queue.get("bytes", ""), queue.get("queued", ""), queue.get("active", ""),
                                   queue.get("undelivered", ""), queue.get("retirementPending", ""),
                                   queue.get("behindRetirement", ""), records.get("active", ""),
                                   records.get("pending", ""), records.get("completions", ""),
                                   records.get("enqueueFailures", ""), loop, report["runtime"].get("state", "")])
        self.samples += 1

    def ioc_lines(self):
        return (self.ioc.case / "ioc.stdout").read_text(errors="replace").splitlines()

    def summary(self):
        return {"interval_s": self.interval, "samples": self.samples, "missed_reports": self.missed,
                "resources": self.resources_path.name, "counters": self.counters_path.name}
