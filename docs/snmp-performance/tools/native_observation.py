#!/usr/bin/env python3
"""Read actual numeric worker events and synchronously collect IOC reports."""

import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import statistics
import time

CALLBACK_HEADER = Path("/usr/include/net-snmp/library/snmp_api.h")
REPORT_TIMEOUT_SECONDS = 5


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


SAMPLER = load("actual_resource_sampler", Path(__file__).with_name("resource_sampler.py"))


def callback_operations(header=CALLBACK_HEADER):
    return {m[0]: int(m[1]) for m in re.findall(
        r"^#define NETSNMP_CALLBACK_OP_(\w+)\s+(\d+)", Path(header).read_text(), re.M)}


def native_events(path, start_us, end_us, callback_header=CALLBACK_HEADER):
    """Judge native-event coverage without calling received data a valid response."""
    operations = callback_operations(callback_header)
    issues, sends, callbacks, terminals, closes = [], {}, {}, {}, []
    dropped, events = 0, []
    if not Path(path).is_file():
        return {"coverage": "INCOMPLETE", "issues": ["native log absent"], "attempted": None,
                "timeout_percent": None, "wire_loss_percent": None}
    text = Path(path).read_text()
    if text and not text.endswith("\n"):
        issues.append("unterminated native event line")
    for number, line in enumerate(text.splitlines(), 1):
        if line.startswith("native_dropped "):
            dropped = max(dropped, int(line.split()[1]))
            continue
        if not line.startswith("native_event "):
            continue
        parts = line.split()
        if len(parts) != 10:
            issues.append(f"malformed native event at line {number}")
            continue
        try:
            session, token, operation, detail, stamp, address, epoch, pid = map(int, parts[2:])
        except ValueError:
            issues.append(f"nonnumeric native event at line {number}")
            continue
        row = {"event": parts[1], "session": session, "token": token, "operation": operation,
               "detail": detail, "at_us": stamp, "address": address, "epoch": epoch, "pid": pid,
               "line": number}
        events.append(row)
        key = (pid, address, epoch, session, token)
        if row["event"] == "native_send":
            if key in sends:
                issues.append("duplicate native send identity")
            sends[key] = row
        elif row["event"] == "native_callback":
            callbacks.setdefault(key, []).append(row)
        elif row["event"] == "terminal":
            if key in terminals:
                issues.append("duplicate native terminal identity")
            terminals[key] = row
        elif row["event"] == "native_close":
            closes.append(row)
    if dropped:
        issues.append("native event buffer dropped observations")
    selected = {key: row for key, row in sends.items() if start_us <= row["at_us"] <= end_us}
    if not selected:
        issues.append("no native send in the observation window")
    rows, latency = [], []
    counts = {"attempted": len(selected), "accepted": 0, "send_failed": 0,
              "timeout": 0, "received_callback": 0, "security_callback": 0,
              "send_failed_callback": 0, "other_terminal": 0, "incomplete": 0}
    names = {operations["RECEIVED_MESSAGE"]: "received_callback", operations["TIMED_OUT"]: "timeout",
             operations["SEC_ERROR"]: "security_callback", operations["SEND_FAILED"]: "send_failed_callback"}
    for key, sent in selected.items():
        terminal = terminals.get(key)
        eligible = [r for r in callbacks.get(key, []) if terminal and r["at_us"] <= terminal["at_us"]]
        last = eligible[-1] if eligible else None
        category = "send_failed" if sent["detail"] == 0 else None
        if sent["detail"] != 0:
            counts["accepted"] += 1
        if terminal is None or terminal["at_us"] > end_us:
            category = "incomplete"
        elif category is None:
            category = names.get(last["operation"], "other_terminal") if last else "other_terminal"
        counts[category] += 1
        if category in ("incomplete", "other_terminal"):
            issues.append("unclassified or right-censored native request")
        if last and terminal and category != "incomplete" and sent["detail"] != 0:
            elapsed = last["at_us"] - sent["at_us"]
            if elapsed < 0:
                issues.append("callback precedes post-submit observation")
            else:
                latency.append(elapsed)
            if last["detail"] != sent["detail"]:
                issues.append("callback and submit request IDs differ")
        rows.append({"send": sent, "terminal": terminal, "callback": last, "category": category})
    for closed in closes:
        if start_us <= closed["at_us"] <= end_us:
            issues.append("native session closed during the observation window")
    selected_keys = set(selected)
    for key, terminal in terminals.items():
        if start_us <= terminal["at_us"] <= end_us and key not in sends:
            issues.append("terminal without captured native send")
    ordered = sorted(latency)
    return {"coverage": "INCOMPLETE" if issues else "COMPLETE", "issues": sorted(set(issues)),
            **counts, "start_us": start_us, "end_us": end_us, "events": len(events), "requests": rows,
            "dropped_events": dropped, "callback_header_sha256": digest(callback_header),
            "callback_operations": operations,
            "timeout_percent": 100 * counts["timeout"] / counts["accepted"] if counts["accepted"] and not issues else None,
            "post_submit_to_callback_us": {"samples": len(ordered),
                "p50": statistics.median(ordered) if ordered else None,
                "p95": ordered[max(0, (95 * len(ordered) + 99) // 100 - 1)] if ordered else None,
                "max": max(ordered) if ordered else None},
            "wire_loss_percent": None, "wire_rtt_us": None, "protocol_successes": None,
            "member_oid_coverage": None,
            "meaning": "Accepted native submits and terminal callback observations; no wire or validated-PDU success claim"}


class Reports:
    """Read a fresh complete report from the actual IOC before sampling /proc."""

    def __init__(self, child, log, folder):
        self.child, self.log, self.folder = child, Path(log), Path(folder)
        self.samples = []
        self.origin_ns = time.monotonic_ns()
        self.resources = (self.folder / "resources.csv").open("x", newline="")
        self.counters = (self.folder / "counters.csv").open("x", newline="")
        self.admissions = (self.folder / "admissions.csv").open("x", newline="")
        self.resource_writer = csv.writer(self.resources)
        self.counter_writer = csv.writer(self.counters)
        self.admission_writer = csv.writer(self.admissions)
        self.resource_writer.writerow(SAMPLER.RESOURCE_COLUMNS)
        self.counter_writer.writerow(SAMPLER.COUNTER_COLUMNS)
        self.admission_writer.writerow(["t_s", "phase", "activation", "address", "admissions", "bursts",
                                        "burstSpanMaxUs", "burstGapMaxUs"])

    def sample(self, phase):
        number = len(self.samples)
        begin, end = f"APC_REPORT_{number}_BEGIN", f"APC_REPORT_{number}_END"
        started = time.monotonic_ns()
        utc = time.time_ns()
        self.child.stdin.write(f"echo {begin}\nsnmp3RuntimeReport\necho {end}\n")
        self.child.stdin.flush()
        deadline = time.monotonic() + REPORT_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            lines = self.log.read_text().splitlines()
            if end in lines:
                block = lines[lines.index(begin) + 1:lines.index(end)]
                break
            if self.child.poll() is not None:
                raise RuntimeError("IOC exited before fresh runtime report")
            time.sleep(0.01)
        else:
            raise RuntimeError("Fresh runtime report was not completed")
        report = SAMPLER.parse_report(block)
        if report is None:
            raise RuntimeError("Fresh runtime report cannot be parsed")
        raw = self.folder / f"report-{number:04}.txt"
        raw.write_text("\n".join(block) + "\n")
        t = (started - self.origin_ns) / 1e9
        targets = [("ioc", self.child.pid)] + [(f"worker-{p['address']}", int(p["pid"]))
                   for p in report["worker"] if int(p["pid"]) > 0]
        process_rows = []
        for name, pid in targets:
            row = SAMPLER.read_process(pid)
            if row is None:
                raise RuntimeError("Observed process disappeared before /proc sample")
            self.resource_writer.writerow([t, phase, name, pid, row["rss_kb"], row["threads"], row["fds"],
                                           row["sockets"], row["cpu_s"]])
            process_rows.append({"process": name, "pid": pid, **row})
        records, runtime = report["records"], report["runtime"]
        for queue in report["queue"]:
            self.counter_writer.writerow([t, phase, queue.get("address", ""), queue.get("count", ""),
                queue.get("bytes", ""), queue.get("queued", ""), queue.get("active", ""),
                queue.get("undelivered", ""), queue.get("retirementPending", ""), queue.get("behindRetirement", ""),
                records.get("active", ""), records.get("pending", ""), records.get("completions", ""),
                records.get("enqueueFailures", ""), report["loop"].get("passes", ""), runtime.get("state", "")])
        for line in block:
            if line.startswith("snmp3 admit:"):
                row = SAMPLER.fields(line)
                self.admission_writer.writerow([t, phase, runtime.get("activation", ""), row.get("address", ""),
                    row.get("admissions", ""), row.get("bursts", ""), row.get("burstSpanMaxUs", ""), row.get("burstGapMaxUs", "")])
        for stream in (self.resources, self.counters, self.admissions):
            stream.flush()
        sample = {"phase": phase, "start_ns": started, "end_ns": time.monotonic_ns(), "start_utc_ns": utc,
                  "report": report, "processes": process_rows, "raw": raw.name}
        self.samples.append(sample)
        (self.folder / "samples.json").write_text(json.dumps(self.samples, indent=2) + "\n")
        return sample

    def close(self):
        for stream in (self.resources, self.counters, self.admissions):
            stream.close()
