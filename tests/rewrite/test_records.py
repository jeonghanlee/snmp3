#!/usr/bin/env python3
"""Exercise actual record/DSET/callback/worker/native paths with a loopback SNMP agent."""
import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from test_native import ROOT, ARCH, Runner, digest, write_json
from helpers.retired_hold import hold as hold_retired

# Deadline-queue and near-deadline trials run one snmp3RecordTest process each.
DEADLINE_SAMPLE_BUDGET_MS = 1000
DEADLINE_TRIALS = 3
NATIVE_TIMEOUT_MS = 10000
NEAR_DELAY_MS = 300
NEAR_MARGINS_MS = (14, 16, 18, 20, 22)
NEAR_REPEATS = 2
NEAR_TRIALS = len(NEAR_MARGINS_MS) * NEAR_REPEATS
QUEUE_CASES = ("deadline-queue", "near-deadline", "stop-queued", "rebuild")
STOP_BUDGET_MS = 300
# Shipped stop limits: the supervisor stop bound and the record drain budget that follows it.
STOP_BOUND_MS = 2000
# Base alarm condition COMM (9) and severity INVALID (3) as the live-detach event encodes them: stat * 10 + sevr.
COMM_INVALID = 93
DRAIN_BUDGET_MS = 2000
REPEAT_FIELDS = ("Records_TimeoutAi.INP", "Records_TimeoutAo.OUT")
# Pinned Base 7.0.10: link.h INST_IO, devSup.h S_dev_badInpType, Runtime.h State::Stopped.
INST_IO = 12
BAD_INP_TYPE = 33685511
STOPPED = 4
# Ipc.h terminal outcome values; native codes are fixed by WorkerMain.cpp::nativeCode.
IPC_COMPLETE, IPC_NATIVE_FAILURE = 1, 6
BOUNDARY_ROWS = ((15, 16, 16, 6, 7, 3), (16, 16, 15, 7, 6, 4), (39, 41, 40, 8, 8, 5),
                 (40, 40, 39, 6, 8, 4), (255, 256, 256, 8, 6, 5), (256, 257, 255, 7, 7, 4),
                 (32767, 32767, 32767, 8, 8, 4))
BOUNDARY_LENGTHS = (0, 1, 14, 15, 16, 17, 38, 39, 40, 41, 254, 255, 256, 257, 32765, 32766, 32767)


def boundary_checks(runner, events, output):
    inputs = [e for e in events if e.get("event") == "boundary_input"]
    outputs = [e for e in events if e.get("event") == "boundary_output"]
    summaries = [e for e in events if e.get("event") == "boundary_summary"]
    expected_inputs, expected_outputs = [], []
    for row, (capacity, sizv, nelm, oidcap, oidnelm, ipnelm) in enumerate(BOUNDARY_ROWS):
        for tag, samples in (("Octets", tuple(map(str, BOUNDARY_LENGTHS)) + ("nul", "high")),
                             ("Oid", ("6", "7", "8", "20", "21")), ("Ip", ("zero", "high"))):
            for sample in samples:
                for name in (("seed", sample) if tag != "Ip" else (sample,)):
                    for slot in range(3):
                        count = 4 if name == "seed" and tag == "Octets" else 2 if name == "seed" else (
                            4 if tag == "Ip" or name == "nul" else 2 if name == "high" else
                            20 if tag == "Oid" and name == "21" else int(name))
                        text_bytes = count if tag == "Octets" else (15 if sample == "high" else 7) if tag == "Ip" else (
                            40 if name == "21" else count * 2 - 1)
                        limit = capacity if tag == "Octets" else oidcap if tag == "Oid" and slot == 2 else 128
                        storage = 40 if slot == 0 else sizv if slot == 1 else (
                            nelm if tag == "Octets" else oidnelm if tag == "Oid" else ipnelm)
                        accepted = count <= limit and (count <= storage if slot == 2 else text_bytes < storage and name != "nul")
                        expected_inputs.append((row, tag, slot, name, count, text_bytes, accepted))
        for long_string in (False, True):
            storage = sizv if long_string else 40
            for length in BOUNDARY_LENGTHS:
                if length < storage:
                    expected_outputs.append((row, long_string, "valid", length, length, length <= capacity))
            expected_outputs.append((row, long_string, "unterminated", storage, storage, False))
            if long_string:
                expected_outputs += [(row, True, "zero-len", 4, 4, False), (row, True, "mismatch-len", 4, 4, False),
                                     (row, True, "client", storage + 2, storage - 1, storage - 1 <= capacity)]
    runner.check("boundary-input-exact-inventory-and-policy", [tuple(e.get(k) for k in
                 ("row", "tag", "slot", "sample", "count", "text_bytes", "accepted")) for e in inputs] == expected_inputs)
    runner.check("boundary-output-exact-inventory-and-policy", [tuple(e.get(k) for k in
                 ("row", "long", "mode", "length", "captured", "accepted")) for e in outputs] == expected_outputs)
    runner.check("boundary-failures-after-valid-input", bool(inputs) and all(e.get("prior_native_success") for e in inputs
                 if not e.get("accepted") and e.get("tag") != "Ip"))
    runner.check("boundary-separate-full-native-readback", len(outputs) == len(expected_outputs) and all(e.get("readback") for e in outputs))
    sets = sum(e[-1] for e in expected_outputs)
    stimuli = 7 * (19 * 2 + 5 * 2 + 2) + len(expected_outputs)
    gets = len(expected_inputs) + len(expected_outputs)
    runner.check("boundary-exact-summary", summaries == [{"event": "boundary_summary", "rows": 7, "stimuli": stimuli,
                 "gets": gets, "sets": sets, "rejected": len(expected_outputs) - sets}])
    wire = [json.loads(line) for line in (output / "boundary-response.stdout").read_text().splitlines() if line.startswith("{")]
    requests = [e for e in wire if e.get("event") == "fault_request"]
    responses = [e for e in wire if e.get("event") == "fault_response"]
    runner.check("boundary-exact-native-packets", len(requests) == len(responses) == gets + stimuli + sets and
                 sum(e.get("command") == 163 for e in requests) == stimuli + sets and
                 sum(e.get("command") == 160 for e in requests) == gets and
                 sorted(e.get("request_id") for e in requests) == sorted(e.get("request_id") for e in responses))
    stored = [json.loads(line) for line in (output / "agent-ipv4-1.stdout").read_text().splitlines() if line.startswith("{")]
    runner.check("boundary-exact-native-SET-commits", sum(e.get("event") == "agent_set_value" for e in stored) == stimuli + sets)


def repeat_detach_checks(runner, events):
    # Every property is judged even if a different event is missing or malformed.
    def one(name):
        matches = [event for event in events if event.get("event") == name]
        return matches[0] if len(matches) == 1 else {}

    def records(event):
        rows = event.get("records")
        if not isinstance(rows, list) or len(rows) != len(REPEAT_FIELDS):
            return {}
        if any(not isinstance(row, dict) or row.get("field") not in REPEAT_FIELDS for row in rows):
            return {}
        found = {row["field"]: row for row in rows}
        return found if len(found) == len(REPEAT_FIELDS) else {}

    def integer(value):
        return type(value) is int

    def snapshot(row):
        return (isinstance(row, dict) and row.get("field") in REPEAT_FIELDS and
                all(integer(row.get(key)) for key in ("record", "link_type", "dset", "dpvt", "contexts")) and
                (row.get("link") is None or isinstance(row.get("link"), str)) and
                "link" in row and "context_record" in row and
                (row["context_record"] is None or integer(row["context_record"])))

    baseline = one("repeat_detach_baseline")
    originals = records(baseline)
    initial = records(one("repeat_detach"))
    retained = baseline.get("contexts")
    ready = (baseline.get("ready") is True and integer(retained) and retained > 0 and
             len(originals) == 2 and all(snapshot(row) and row["record"] > 0 and row["dset"] > 0 and
                 row["dpvt"] > 0 and row["context_record"] == row["record"] and row["link_type"] == INST_IO and
                 isinstance(row["link"], str) and bool(row["link"]) and row["contexts"] == retained
                 for row in originals.values()))
    attempts = [event for event in events if event.get("event") == "repeat_detach_attempt"]
    expected = [(field, number) for number in (1, 2) for field in REPEAT_FIELDS]
    identities = [(event.get("field"), event.get("attempt")) for event in attempts]
    sequence = [event.get("event") for event in events if str(event.get("event", "")).startswith("repeat_detach")]
    runner.check("repeat-detach-events", sequence == ["repeat_detach_baseline", "repeat_detach"] +
                 ["repeat_detach_attempt"] * 4 + ["repeat_detach_before_free", "repeat_detach_cleanup"])
    runner.check("repeat-detach-fixture-ready", ready)
    runner.check("repeat-detach-four-attempts", identities == expected and all(
                 event.get("attempted") is True and type(event.get("attempt")) is int and
                 isinstance(event.get("replacement"), str) and event["replacement"].startswith("@binding=") and
                 event["replacement"][1:] != originals.get(event.get("field"), {}).get("link")
                 for event in attempts) and len(attempts) == 4)

    def check_snapshot(prefix, row, original):
        valid = ready and snapshot(row) and snapshot(original) and row["record"] == original["record"]
        runner.check(prefix + "-link-type", valid and row["link_type"] == original["link_type"])
        runner.check(prefix + "-link", valid and row["link"] == original["link"])
        runner.check(prefix + "-dset", valid and row["dset"] == original["dset"])
        runner.check(prefix + "-dpvt-null", valid and row["dpvt"] == 0)
        runner.check(prefix + "-context-record-null", valid and row["context_record"] == 0)
        runner.check(prefix + "-contexts-retained", valid and row["contexts"] == retained)

    for field, label in zip(REPEAT_FIELDS, ("ai", "ao")):
        original = originals.get(field, {})
        check_snapshot("repeat-detach-first-" + label, initial.get(field, {}), original)
        for number in (1, 2):
            matches = [event for event in attempts if event.get("field") == field and
                       type(event.get("attempt")) is int and event["attempt"] == number]
            attempt = matches[0] if len(matches) == 1 else {}
            prefix = f"repeat-detach-{label}-{number}"
            runner.check(prefix + "-refused", attempt.get("attempted") is True and
                         integer(attempt.get("status")) and attempt["status"] == BAD_INP_TYPE)
            row = attempt.get("snapshot", {})
            check_snapshot(prefix, row if isinstance(row, dict) else {}, original)
    before_free = one("repeat_detach_before_free")
    runner.check("repeat-detach-contexts-retained-before-free", ready and
                 integer(before_free.get("contexts")) and before_free["contexts"] == retained)
    cleanup = one("repeat_detach_cleanup")
    runner.check("repeat-detach-cleanup-contexts-zero", integer(cleanup.get("contexts")) and cleanup["contexts"] == 0)
    runner.check("repeat-detach-cleanup-stopped", integer(cleanup.get("state")) and cleanup["state"] == STOPPED)



DTYPE_MODES = ("idle", "active", "restored", "shutdown")
DTYPE_NAMES = ("Records_DtypeAi", "Records_DtypeAo")
DTYPE_DELAY_MS = 750
# Base 7.0.10 alarm.h: LINK condition and INVALID severity.
LINK_INVALID = (14, 3)


def dtype_phases(mode):
    phases = ["initial"]
    if mode == "idle":
        phases += ["changed", "refused", "restored", "completed"]
    elif mode in ("active", "restored"):
        for label in ("ai", "ao"):
            phases += ["inflight-" + label, "changed-" + label]
            if mode == "restored":
                phases += ["restored-" + label]
            phases += ["completed-" + label]
        if mode == "active":
            phases += ["refused"]
    else:
        phases += ["queued", "stop_window", "changed"]
    return phases + ["stop_done", "detached", "detached_put"]


def dtype_inventory(events):
    # Validate the entire observation schema before any property can use its values.
    def integers(row, keys):
        return all(type(row.get(key)) is int for key in keys.split())

    def flags(row, keys):
        return all(type(row.get(key)) is bool for key in keys.split())

    try:
        if any(not isinstance(e, dict) for e in events):
            return False
        observed = [e for e in events if str(e.get("event", "")).startswith("dtype") and e.get("event") != "dtype_wire"]
        if any(e.get("mode") not in DTYPE_MODES or e.get("event") not in
               ("dtype", "dtype_put", "dtype_window", "dtype_detach_checks", "dtype_free", "dtype_cleanup") for e in observed):
            return False
        for mode in DTYPE_MODES:
            selected = [e for e in observed if e["mode"] == mode]
            rows = [e for e in selected if e["event"] == "dtype"]
            if [e.get("phase") for e in rows] != dtype_phases(mode):
                return False
            for e in rows:
                if (not integers(e, "at_us packets contexts active queued pending entered completions state exited count bytes") or
                        not flags(e, "entry_open drain_failed admission settled") or
                        [r.get("name") for r in e["records"]] != list(DTYPE_NAMES)):
                    return False
                for r in e["records"]:
                    if (not integers(r, "record dpvt dset dtype original alternate pact stat sevr udf link_type") or
                            not isinstance(r.get("link"), str) or type(r.get("value")) not in (int, float) or
                            type(r.get("flnk")) not in (int, float)):
                        return False
                    c = r["context"]
                    if (not isinstance(c, dict) or not integers(c, "record dtype dset binding handle generation admission identity_binding activation revision deadline_ms") or
                            not flags(c, "published native_success")):
                        return False
                    if mode == "shutdown" and e["phase"] == "stop_window" and (
                            not integers(c, "terminal") or not flags(c, "terminal_same")):
                        return False
            puts = [e for e in selected if e["event"] == "dtype_put"]
            expected = []
            for phase in dtype_phases(mode):
                if phase.startswith(("changed", "restored", "detached_put")):
                    names = [DTYPE_NAMES[0 if phase.endswith("-ai") else 1]] if "-" in phase else DTYPE_NAMES
                    expected += [(phase, name) for name in names]
            if [(e.get("phase"), e.get("name")) for e in puts] != expected or any(
                    not integers(e, "value status") for e in puts):
                return False
            for kind in ("dtype_free", "dtype_cleanup"):
                matching = [e for e in selected if e["event"] == kind]
                if len(matching) != 1 or not integers(matching[0], "contexts"):
                    return False
            if [e["event"] for e in selected][-2:] != ["dtype_free", "dtype_cleanup"]:
                return False
            detach_checks = [e for e in selected if e["event"] == "dtype_detach_checks"]
            if len(detach_checks) != 1 or not flags(detach_checks[0], "dtype-shutdown-dpvt-cleared dtype-shutdown-record-cleared"):
                return False
            detached_index = next(i for i, e in enumerate(selected) if e.get("phase") == "detached")
            if selected[detached_index + 1]["event"] != "dtype_detach_checks":
                return False
            windows = [e for e in selected if e["event"] == "dtype_window"]
            if len(windows) != (1 if mode == "shutdown" else 0):
                return False
            if windows and not flags(windows[0], "reached still_stopping"):
                return False
        return True
    except (KeyError, TypeError, AttributeError, IndexError, StopIteration):
        return False


def dtype_checks(runner, events):
    valid = dtype_inventory(events)
    runner.check("dtype-events", valid)
    if not valid:
        return
    rows = {mode: {e["phase"]: e for e in events if e.get("event") == "dtype" and e["mode"] == mode}
            for mode in DTYPE_MODES}
    for mode, phases in rows.items():
        initial = phases["initial"]
        prefix = "dtype-" + mode
        # Field changes are explicit; completion, refusal and detach must preserve each record's expected value.
        expected_dtype = [r["original"] for r in initial["records"]]
        current_dtype = True
        for phase in dtype_phases(mode):
            if phase.startswith(("changed", "restored", "detached_put")):
                indices = range(len(DTYPE_NAMES)) if "-" not in phase else (0 if phase.endswith("-ai") else 1,)
                field = "alternate" if phase.startswith("changed") else "original"
                for i in indices:
                    expected_dtype[i] = initial["records"][i][field]
            current_dtype = current_dtype and all(r["dtype"] == expected for r, expected in
                                                  zip(phases[phase]["records"], expected_dtype))
        runner.check(prefix + "-current-dtype", current_dtype)
        runner.check(prefix + "-fixture", initial["contexts"] == 14 and initial["packets"] >= 0 and
                     all(r["pact"] == 0 and r["original"] != r["alternate"] and r["dpvt"] > 0 and
                         r["context"]["record"] == r["record"] and r["dtype"] == r["original"] and
                         r["context"]["handle"] > 0 and r["context"]["deadline_ms"] == 10000
                         for r in initial["records"]))
        attached = [e for phase, e in phases.items() if phase not in ("detached", "detached_put")]
        runner.check(prefix + "-binding-preserved", all(
            all(r[key] == base[key] for key in ("record", "dpvt", "dset", "link", "link_type")) and
            all(r["context"][key] == base["context"][key] for key in
                ("record", "dtype", "dset", "binding", "handle", "activation", "revision"))
            for e in attached for r, base in zip(e["records"], initial["records"])))
        puts = [e for e in events if e.get("event") == "dtype_put" and e["mode"] == mode]
        runner.check(prefix + "-puts", all(e["status"] == 0 and
            phases[e["phase"]]["records"][DTYPE_NAMES.index(e["name"])]["dtype"] == e["value"] and
            e["value"] == initial["records"][DTYPE_NAMES.index(e["name"])][
                "alternate" if e["phase"].startswith("changed") else "original"] for e in puts))
        if mode in ("active", "restored"):
            for i, label in enumerate(("ai", "ao")):
                before, changed, done = (phases[name + "-" + label] for name in ("inflight", "changed", "completed"))
                middle = phases["restored-" + label] if mode == "restored" else changed
                a, b, c, d = (e["records"][i] for e in (before, changed, middle, done))
                runner.check(prefix + "-" + label + "-active-window", a["pact"] == b["pact"] == c["pact"] == 1 and
                             before["packets"] == changed["packets"] == middle["packets"] and a["context"]["generation"] == 1)
                runner.check(prefix + "-" + label + "-identity", all(a["context"][k] == r["context"][k]
                             for k in ("generation", "admission", "identity_binding") for r in (b, c, d)))
                runner.check(prefix + "-" + label + "-completion", d["pact"] == 0 and d["flnk"] == 1 and
                             done["completions"] == i + 1 and done["active"] == 0 and
                             (d["stat"], d["sevr"]) == (LINK_INVALID if mode == "active" else (0, 0)))
            input_row = phases["completed-ai"]["records"][0]
            runner.check(prefix + "-input-publication", input_row["value"] == (42 if mode == "active" else -123) and
                         input_row["context"]["published"] is (mode == "restored") and
                         input_row["context"]["native_success"] is (mode == "restored"))
        if mode in ("idle", "active"):
            refused = phases["refused"]
            prior = initial if mode == "idle" else phases["completed-ao"]
            runner.check(prefix + "-refusal", refused["packets"] == prior["packets"] and refused["active"] == 0 and
                         refused["completions"] == prior["completions"] and all(
                         r["pact"] == 0 and (r["stat"], r["sevr"]) == LINK_INVALID and
                         all(r["context"][k] == b["context"][k] for k in ("generation", "admission", "identity_binding"))
                         for r, b in zip(refused["records"], prior["records"])))
        if mode == "idle":
            restored, done = phases["restored"], phases["completed"]
            runner.check(prefix + "-restore-no-work", restored["packets"] == initial["packets"] and restored["active"] == 0 and
                         all(r["context"]["generation"] == 0 for r in restored["records"]))
            runner.check(prefix + "-restored-success", done["completions"] == 2 and all(
                         r["pact"] == 0 and r["sevr"] == 0 and r["context"]["generation"] == 1
                         for r in done["records"]) and done["records"][0]["value"] == -123 and
                         done["records"][0]["context"]["published"] is True and
                         all(r["flnk"] == b["flnk"] + 1 for r, b in zip(done["records"], restored["records"])))
        if mode == "shutdown":
            queued, window, changed, done = (phases[k] for k in ("queued", "stop_window", "changed", "stop_done"))
            observation = next(e for e in events if e.get("event") == "dtype_window")
            runner.check("dtype-shutdown-window", observation["reached"] and observation["still_stopping"] and
                         queued["queued"] == window["queued"] == 2 and window["active"] == 2 and window["entered"] == 0 and
                         window["exited"] == 1 and window["admission"] is False and window["entry_open"] is True and
                         all(r["context"].get("terminal") == 1 and r["context"].get("terminal_same") is True for r in window["records"]) and
                         all(r["pact"] == 1 for r in changed["records"]))
            runner.check("dtype-shutdown-completion", done["completions"] == 2 and done["active"] == 0 and
                         all(r["pact"] == 0 and r["flnk"] == 1 and (r["stat"], r["sevr"]) == LINK_INVALID and
                             all(r["context"][k] == b["context"][k] for k in ("generation", "admission", "identity_binding"))
                             for r, b in zip(done["records"], queued["records"])) and
                         done["records"][0]["value"] == 42 and done["records"][0]["context"]["native_success"] is False and
                         done["records"][0]["context"]["published"] is False)
        detached = phases["detached"]
        after = phases["detached_put"]
        runner.check(prefix + "-detach-retained", detached["contexts"] == initial["contexts"] and
                     detached["entry_open"] is False and detached["entered"] == 0 and all(
                         r["dset"] == b["dset"] and r["dtype"] == b["dtype"] for r, b in
                         zip(detached["records"], phases["stop_done"]["records"])))
        runner.check(prefix + "-no-reattach", after["packets"] == detached["packets"] and after["contexts"] == initial["contexts"] and
                     after["active"] == 0 and all(r["dpvt"] == b["dpvt"] and r["context"]["record"] == b["context"]["record"]
                         for r, b in zip(after["records"], detached["records"])))
    runner.check("dtype-retirement-settled", all(p["stop_done"]["settled"] and p["stop_done"]["count"] == 0 and
                 p["stop_done"]["bytes"] == 0 and not p["stop_done"]["drain_failed"] for p in rows.values()))
    runner.check("dtype-shutdown-dpvt-cleared", all(r["dpvt"] == 0 for p in rows.values() for r in p["detached"]["records"]) and
                 all(e["dtype-shutdown-dpvt-cleared"] for e in events if e.get("event") == "dtype_detach_checks"))
    runner.check("dtype-shutdown-record-cleared", all(r["context"]["record"] == 0 for p in rows.values() for r in p["detached"]["records"]) and
                 all(e["dtype-shutdown-record-cleared"] for e in events if e.get("event") == "dtype_detach_checks"))
    runner.check("dtype-storage-lifetime", all(e["contexts"] == (14 if e["event"] == "dtype_free" else 0)
                 for e in events if e.get("event") in ("dtype_free", "dtype_cleanup")))
    runner.check("dtype-cleanup-stopped", all(e.get("state") == STOPPED for e in events if e.get("event") == "dtype_cleanup"))



SUPPORT_MODES = ("idle", "active", "drain", "before-close", "after-close")
SUPPORT_NAMES = ("Records_SupportAi", "Records_SupportAo")
CONSTANT = 0
# Base errMdef.h M_dbLib and dbStaticLib.h S_dbLib_badField.
BAD_FIELD = (512 << 16) | 11


def support_layout(mode, resume=True):
    # Each entry describes a complete phase and the real puts immediately preceding it.
    pair = (0, 1)
    phases = [("initial", (), ""), ("probe-selected", pair, "dtype"),
              ("probe-rejected", pair, "link"), ("probe-restored", pair, "dtype")]
    if mode == "idle":
        phases += [("soft-selected", pair, "dtype"), ("attempted", pair, "link")]
        if resume:
            phases += [("restored", pair, "dtype"), ("completed", (), "")]
    elif mode == "active":
        for i, label in enumerate(("ai", "ao")):
            phases += [("inflight-" + label, (), ""), ("selected-" + label, (i,), "dtype"),
                       ("attempted-" + label, (i,), "link"), ("completed-" + label, (), "")]
    elif mode == "drain":
        phases += [("queued", (), ""), ("stop_window", (), ""),
                   ("soft-selected", pair, "dtype"), ("attempted", pair, "link")]
    phases += [("stop_done", (), "")]
    if mode == "before-close":
        phases += [("soft-selected", pair, "dtype"), ("transferred", pair, "link"),
                   ("snmp-selected", pair, "dtype"), ("return-attempted", pair, "link")]
    phases += [("detached", (), "")]
    if mode == "after-close":
        phases += [("soft-selected", pair, "dtype"), ("repeat-1", pair, "link"), ("repeat-2", pair, "link")]
    return phases


def support_inventory(events, modes=SUPPORT_MODES):
    def ints(row, keys):
        return all(type(row.get(k)) is int for k in keys.split())

    def flags(row, keys):
        return all(type(row.get(k)) is bool for k in keys.split())

    try:
        if not modes or len(set(modes)) != len(modes) or any(m not in SUPPORT_MODES for m in modes):
            return False
        if any(not isinstance(e, dict) for e in events):
            return False
        observed = [e for e in events if str(e.get("event", "")).startswith("support") and e.get("event") != "support_wire"]
        if any(e.get("mode") not in modes for e in observed):
            return False
        if [e["mode"] for e in observed] != [m for m in modes for e in observed if e["mode"] == m]:
            return False
        for mode in modes:
            selected = [e for e in observed if e["mode"] == mode]
            resume = [e for e in selected if e.get("event") == "support_resume"]
            if len(resume) != (1 if mode == "idle" else 0) or any(not flags(e, "processing") for e in resume):
                return False
            processing = resume[0]["processing"] if resume else True
            expected = []
            for phase, indices, field in support_layout(mode, processing):
                expected += [("support_put", phase, SUPPORT_NAMES[i]) for i in indices]
                expected += [("support", phase, None)]
                if phase == "attempted" and mode == "idle":
                    expected += [("support_resume", None, None)]
            expected += [("support_free", None, None), ("support_cleanup", None, None)]
            if [(e.get("event"), e.get("phase"), e.get("name")) for e in selected] != expected:
                return False
            for e in selected:
                if e["event"] == "support_put":
                    field = "DTYP" if e["field"] == "DTYP" else ("INP" if e["name"] == SUPPORT_NAMES[0] else "OUT")
                    if (e["field"] != field or not ints(e, "status") or
                            type(e["value"]) is not (int if field == "DTYP" else str)):
                        return False
                elif e["event"] in ("support_free", "support_cleanup"):
                    if not ints(e, "contexts") or (e["event"] == "support_cleanup" and not ints(e, "state")):
                        return False
                elif e["event"] == "support":
                    if (not ints(e, "at_us packets contexts active queued pending entered completions state created exited count bytes") or
                            not flags(e, "entry_open detach_allowed drain_failed admission settled") or
                            not isinstance(e["records"], list) or [r.get("name") for r in e["records"]] != list(SUPPORT_NAMES)):
                        return False
                    for r in e["records"]:
                        if (not ints(r, "record dpvt dset dsxt original_dset original_dsxt soft_dset soft_dsxt dtype original soft pact stat sevr link_type") or
                                type(r.get("link")) is not str or type(r.get("value")) not in (int, float) or
                                type(r.get("flnk")) not in (int, float)):
                            return False
                        c = r["context"]
                        if (not isinstance(c, dict) or not ints(c, "pointer record dtype dset binding handle activation revision generation admission identity_binding") or
                                not flags(c, "published native_success")):
                            return False
                        if e["phase"] == "stop_window" and (not ints(c, "terminal") or not flags(c, "terminal_same")):
                            return False
            if not processing:
                attempted = next(e for e in selected if e.get("event") == "support" and e["phase"] == "attempted")
                if all(r["dpvt"] == r["context"]["pointer"] and r["context"]["record"] == r["record"] and
                       r["dset"] == r["original_dset"] for r in attempted["records"]):
                    return False
        return True
    except (KeyError, TypeError, AttributeError, IndexError, StopIteration):
        return False


def support_checks(runner, events, modes=SUPPORT_MODES):
    valid = support_inventory(events, modes)
    runner.check("support-events", valid)
    if not valid:
        return
    for mode in modes:
        selected = [e for e in events if e.get("mode") == mode]
        rows = {e["phase"]: e for e in selected if e.get("event") == "support"}
        initial = rows["initial"]
        prefix = "support-" + mode
        puts = [e for e in selected if e.get("event") == "support_put"]
        resume = next((e["processing"] for e in selected if e.get("event") == "support_resume"), True)
        runner.check(prefix + "-fixture", initial["contexts"] == 14 and initial["admission"] and initial["entry_open"] and
                     not initial["detach_allowed"] and all(r["pact"] == 0 and r["flnk"] == 0 and r["dpvt"] > 0 and
                     r["context"]["pointer"] == r["dpvt"] and r["context"]["record"] == r["record"] > 0 and
                     r["dtype"] == r["original"] != r["soft"] and r["dset"] == r["original_dset"] > 0 and
                     r["dsxt"] == r["original_dsxt"] > 0 and r["soft_dset"] > 0 and r["soft_dsxt"] > 0 and
                     r["soft_dset"] != r["original_dset"] and r["link_type"] == INST_IO and r["link"].startswith("binding=Support") and
                     all(r["context"][k] > 0 for k in ("binding", "handle", "activation", "revision")) for r in initial["records"]))
        expected_dtype = [r["original"] for r in initial["records"]]
        state_ok = identity_ok = puts_ok = no_request = True
        soft_installed = failed_add = closed = False
        previous = initial
        admitted, completed = [False, False], [False, False]
        requests = [None, None]
        request_ok = values_ok = True
        for phase, indices, field in support_layout(mode, resume):
            e = rows[phase]
            original_choice = phase in ("probe-restored", "restored", "snmp-selected")
            if field == "dtype":
                for i in indices:
                    expected_dtype[i] = initial["records"][i]["original" if original_choice else "soft"]
            if phase == "transferred":
                soft_installed = True
            if phase == "return-attempted":
                failed_add = True
            if phase == "detached":
                closed = True
            for i, (r, base) in enumerate(zip(e["records"], initial["records"])):
                c, bc = r["context"], base["context"]
                label = ("ai", "ao")[i]
                if (mode == "idle" and phase == "completed") or (mode == "active" and phase == "inflight-" + label) or (mode == "drain" and phase == "queued"):
                    admitted[i] = True
                    requests[i] = tuple(c[k] for k in ("generation", "admission", "identity_binding"))
                    request_ok = request_ok and requests[i][0] == 1 and requests[i][1] > 0 and requests[i][2] == bc["handle"]
                if (mode == "idle" and phase == "completed") or (mode == "active" and phase == "completed-" + label) or (mode == "drain" and phase == "stop_done"):
                    completed[i] = True
                expected_request = requests[i] if admitted[i] else tuple(bc[k] for k in ("generation", "admission", "identity_binding"))
                request_ok = request_ok and tuple(c[k] for k in ("generation", "admission", "identity_binding")) == expected_request
                successful_input = i == 0 and mode == "idle" and completed[i]
                expected_alarm = ((0, 0) if mode == "idle" else LINK_INVALID) if completed[i] else (base["stat"], base["sevr"])
                expected_pact = 1 if failed_add or closed else int(admitted[i] and not completed[i])
                values_ok = values_ok and r["value"] == (-123 if successful_input else base["value"]) and r["flnk"] == int(completed[i]) and (r["stat"], r["sevr"]) == expected_alarm and r["pact"] == expected_pact
                if i == 0:
                    values_ok = values_ok and c["published"] is successful_input and c["native_success"] is successful_input
                dset = 0 if failed_add else base["soft_dset"] if soft_installed else base["dset"]
                dsxt = 0 if failed_add else base["soft_dsxt"] if soft_installed else base["dsxt"]
                text = base["link"] if not soft_installed or failed_add else "123"
                link_type = INST_IO if not soft_installed or failed_add else CONSTANT
                detached = soft_installed or closed
                state_ok = state_ok and r["dtype"] == expected_dtype[i] and r["dset"] == dset and r["dsxt"] == dsxt and r["link"] == text and r["link_type"] == link_type
                state_ok = state_ok and r["dpvt"] == (0 if detached else base["dpvt"]) and c["record"] == (0 if detached else base["record"])
                if failed_add:
                    state_ok = state_ok and r["pact"] == 1
                identity_ok = identity_ok and r["record"] == base["record"] and all(r[k] == base[k] for k in
                    ("original", "soft", "original_dset", "original_dsxt", "soft_dset", "soft_dsxt")) and all(c[k] == bc[k] for k in
                    ("pointer", "dtype", "dset", "binding", "handle", "activation", "revision"))
            phase_puts = [p for p in puts if p["phase"] == phase]
            for put, i in zip(phase_puts, indices):
                expect_field = "DTYP" if field == "dtype" else ("INP" if i == 0 else "OUT")
                value = expected_dtype[i] if field == "dtype" else ("@" + initial["records"][i]["link"] if phase in ("probe-rejected", "return-attempted") else "123")
                status = 0 if field == "dtype" or phase == "transferred" else BAD_FIELD if phase == "probe-rejected" else BAD_INP_TYPE
                puts_ok = puts_ok and put["field"] == expect_field and put["value"] == value and put["status"] == status
            if indices:
                no_request = no_request and e["packets"] == previous["packets"] and e["completions"] == previous["completions"] and all(
                    all(r["context"][k] == b["context"][k] for k in ("generation", "admission", "identity_binding"))
                    for r, b in zip(e["records"], previous["records"]))
            previous = e
        runner.check(prefix + "-request-identity", request_ok)
        runner.check(prefix + "-record-state", values_ok)
        runner.check(prefix + "-phase-state", state_ok)
        runner.check(prefix + "-context-identity", identity_ok)
        runner.check(prefix + "-put-results", puts_ok)
        runner.check(prefix + "-field-writes-no-request", no_request)
        probe = rows["probe-rejected"]
        runner.check(prefix + "-base-validation", all(p["status"] == BAD_FIELD for p in puts if p["phase"] == "probe-rejected") and
                     all(r["dpvt"] == b["dpvt"] and r["dset"] == b["dset"] and r["link"] == b["link"] and r["link_type"] == INST_IO
                         for r, b in zip(probe["records"], initial["records"])))
        if mode in ("idle", "active", "drain", "after-close"):
            phases = ("attempted",) if mode in ("idle", "drain") else ("attempted-ai", "attempted-ao") if mode == "active" else ("repeat-1", "repeat-2")
            attempts = [p for p in puts if p["phase"] in phases]
            runner.check(prefix + "-refusal", len(attempts) == (4 if mode == "after-close" else 2) and all(p["status"] == BAD_INP_TYPE for p in attempts))
        if mode == "idle":
            runner.check("support-idle-restored-processing", resume)
            if resume:
                done = rows["completed"]
                runner.check(prefix + "-successful-processing", done["completions"] == 2 and done["packets"]-initial["packets"] == 2 and
                             all(r["pact"] == 0 and r["flnk"] == 1 and (r["stat"], r["sevr"]) == (0, 0) for r in done["records"]) and
                             done["records"][0]["value"] == -123 and done["records"][0]["context"]["published"] and
                             done["records"][0]["context"]["native_success"])
        if mode == "active":
            for i, label in enumerate(("ai", "ao")):
                a, b, done = (rows[p + "-" + label] for p in ("inflight", "attempted", "completed"))
                ar, br, dr = (e["records"][i] for e in (a, b, done))
                runner.check(prefix + "-" + label + "-active-window", ar["pact"] == br["pact"] == 1 and
                             not b["detach_allowed"] and b["admission"] and ar["context"]["generation"] == 1)
                runner.check(prefix + "-" + label + "-completion", dr["pact"] == 0 and dr["flnk"] == 1 and
                             (dr["stat"], dr["sevr"]) == LINK_INVALID and done["completions"] == i+1 and done["active"] == 0 and
                             all(ar["context"][k] == br["context"][k] == dr["context"][k] for k in ("generation", "admission", "identity_binding")))
        if mode == "drain":
            window, attempt = rows["stop_window"], rows["attempted"]
            runner.check(prefix + "-window", all(not e["admission"] and e["entry_open"] and not e["detach_allowed"] and
                         e["entered"] == 0 and e["queued"] == 2 and e["created"] == e["exited"] > 0 and all(
                         r["pact"] == 1 and r["context"].get("terminal_same", True) for r in e["records"]) for e in (window, attempt)) and
                         all(r["context"]["terminal"] == 1 and r["context"]["terminal_same"] for r in window["records"]))
        if mode in ("active", "drain"):
            done = rows["stop_done"]
            runner.check(prefix + "-final-completion", done["completions"] == 2 and done["active"] == 0 and all(
                         r["pact"] == 0 and r["flnk"] == 1 and (r["stat"], r["sevr"]) == LINK_INVALID for r in done["records"]))
            inp = done["records"][0]
            runner.check(prefix + "-no-input-publication", inp["value"] == initial["records"][0]["value"] == 42 and
                         not inp["context"]["published"] and not inp["context"]["native_success"])
        if mode == "before-close":
            transfer, returned = rows["transferred"], rows["return-attempted"]
            runner.check("support-first-transfer", all(p["status"] == 0 for p in puts if p["phase"] == "transferred") and
                         all(r["dtype"] == r["soft"] and r["dset"] == r["soft_dset"] and r["dsxt"] == r["soft_dsxt"] and r["dpvt"] == r["context"]["record"] == 0 and
                             r["link_type"] == CONSTANT and r["link"] == "123" for r in transfer["records"]))
            runner.check("support-return-to-snmp3-refused", all(p["status"] == BAD_INP_TYPE for p in puts if p["phase"] == "return-attempted") and
                         all(r["dtype"] == r["original"] and r["dpvt"] == r["dset"] == r["context"]["record"] == 0 and r["pact"] == 1 and
                             r["link_type"] == INST_IO and r["link"] == b["link"] for r, b in zip(returned["records"], initial["records"])))
        stop = rows["stop_done"]
        runner.check(prefix + "-stop", not stop["admission"] and not stop["entry_open"] and stop["detach_allowed"] and
                     not stop["drain_failed"] and stop["entered"] == stop["active"] == stop["pending"] == stop["queued"] == 0 and
                     stop["settled"] and stop["count"] == stop["bytes"] == 0 and stop["created"] == stop["exited"] > 0 and stop["state"] == STOPPED)
        runner.check(prefix + "-storage-lifetime", all(e["contexts"] == initial["contexts"] for e in rows.values()) and
                     next(e for e in selected if e.get("event") == "support_free")["contexts"] == initial["contexts"])
        cleanup = next(e for e in selected if e.get("event") == "support_cleanup")
        runner.check(prefix + "-cleanup", cleanup["contexts"] == 0 and cleanup["state"] == STOPPED)


def queue_counter_lines(path):
    # Every `snmp3 queue:` report line of a record test process: the three counters of admissions behind a retirement.
    found = []
    for line in path.read_text().splitlines():
        match = re.search(r"^snmp3 queue: .* behindRetirement=(\d+) behindAdmitted=(\d+) behindNeverSent=(\d+)$", line)
        if match:
            found.append(tuple(int(value) for value in match.groups()))
    return found


def queue_counters(path):
    # The first report line of a record test process.
    found = queue_counter_lines(path)
    return found[0] if found else None


class InconclusiveWait(Exception):
    """A stop-queued trial that stopped without an observed Base reprocess cannot judge the product."""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--products", type=Path)
    parser.add_argument("--sanitizers", action="store_true")
    parser.add_argument("--case", choices=("baseline", "shutdown", "queued-shutdown", "edges", "alarms", "active", "policy", "numeric",
                                                   "active-unforced", "deadline-queue", "near-deadline",
                                                   "accounting", "stop-queued", "rebuild", "stop-inflight", "stop-enqueue-failed", "stop-downstream", "live-detach", "repeat-detach", "live-dtype", "support-transition", "scan-proc", "alarm-flow", "chain-flow", "queued-simm", "boundaries", "retirement-flow", "contract", "stale-record", "final-clauses"), default="baseline")
    parser.add_argument("--support-mode", choices=SUPPORT_MODES)
    args = parser.parse_args()
    if args.support_mode is not None and args.case != "support-transition":
        parser.error("--support-mode requires --case support-transition")
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    runner = Runner(output, args.products.resolve() if args.products else None, args.sanitizers)
    aborted = False
    try:
        peer, material = runner.agent(4)
        specs = [("IntegerRead", 1, "get", "integer", 1),
                 ("Counter32Read", 2, "get", "counter32", 1),
                 ("Counter64Read", 5, "get", "counter64", 1),
                 ("OctetsRead", 6, "get", "octets", 1024),
                 ("OidRead", 7, "get", "oid", 128),
                 ("FloatRead", 9, "get", "opaqueFloat", 1),
                 ("DoubleRead", 10, "get", "opaqueDouble", 1),
                 ("IpRead", 8, "get", "ipAddress", 1),
                 ("IntegerWrite", 1, "set", "integer", 1),
                 ("Counter64Write", 5, "set", "counter64", 1),
                 ("OctetsWrite", 6, "set", "octets", 1024),
                 ("MaxOctetsRead", 6, "get", "octets", 32766),
                 ("MaxOctetsWrite", 6, "set", "octets", 32766),
                 ("FloatWrite", 9, "set", "opaqueFloat", 1)]
        configuration = {"schema": 1, "profiles": [{"id": "Local", "version": "2c",
                         "communityFile": str(material["community"]), "timeoutMs": 100,
                         "retries": 0, "maxVarbinds": 16}],
                         "endpoints": [{"id": "Local", "address": "127.0.0.1",
                         "port": int(peer.rsplit(":", 1)[1]), "profile": "Local"}],
                         "bindings": [{"id": name, "endpoint": "Local",
                         "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0",
                         "operation": operation, "valueType": tag, "capacity": capacity}
                         for name, index, operation, tag, capacity in specs]}
        if args.case in ("alarms", "policy", "stop-inflight", "stop-enqueue-failed", "stop-downstream", "live-detach", "repeat-detach"):
            dropped, _ = runner.fault("record-response-drop", 4, peer, "drop-all")
            configuration["endpoints"].append({"id": "Dropped", "address": "127.0.0.1",
                                               "port": int(dropped.rsplit(":", 1)[1]), "profile": "Local"})
            prefix = "Policy" if args.case == "policy" else "Timeout"
            configuration["bindings"] += [dict(binding, id=prefix + binding["id"], endpoint="Dropped")
                                           for binding in list(configuration["bindings"])]
        if args.case in ("active", "active-unforced", "accounting", "queued-simm"):
            delayed, _ = runner.fault("active-response-delay", 4, peer, "delay", delay_ms=750)
            dropped, _ = runner.fault("active-response-drop", 4, peer, "drop-all")
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "active-response-delay.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Delayed", timeoutMs=1500))
            configuration["profiles"].append(dict(configuration["profiles"][0], id="NativeRetry", retries=1))
            configuration["endpoints"] += [{"id": "Delayed", "address": "127.0.0.1",
                                            "port": int(delayed.rsplit(":", 1)[1]), "profile": "Delayed"},
                                           {"id": "Dropped", "address": "127.0.0.1",
                                            "port": int(dropped.rsplit(":", 1)[1]), "profile": "Local"},
                                           {"id": "NativeRetry", "address": "127.0.0.1",
                                            "port": int(dropped.rsplit(":", 1)[1]), "profile": "NativeRetry"}]
            outputs = [binding for binding in configuration["bindings"] if binding["operation"] == "set"]
            configuration["bindings"] += [dict(binding, id="Active" + binding["id"], endpoint="Delayed")
                                           for binding in outputs]
            configuration["bindings"].append(dict(next(binding for binding in outputs if binding["id"] == "FloatWrite"),
                                                   id="RetryFloatWrite", endpoint="Dropped"))
            configuration["bindings"].append(dict(next(binding for binding in outputs if binding["id"] == "FloatWrite"),
                                                   id="NativeRetryFloatWrite", endpoint="NativeRetry"))
        if args.case == "queued-simm":
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Deadline", timeoutMs=NATIVE_TIMEOUT_MS))
            configuration["endpoints"].append({"id": "Deadline", "address": "127.0.0.1",
                "port": int(dropped.rsplit(":", 1)[1]), "profile": "Deadline"})
            configuration["bindings"] += [dict(b, id="Deadline" + b["id"], endpoint="Deadline") for b in outputs]
        if args.case in ("alarm-flow", "chain-flow", "retirement-flow", "numeric"):
            control = output / "response-mode.txt"
            control.write_text("pass\n")
            runner.env["SNMP3_RECORD_FAULT_CONTROL"] = str(control)
            fault_name = {"chain-flow": "chain-response", "alarm-flow": "alarm-response", "retirement-flow": "retirement-response", "numeric": "numeric-response"}[args.case]
            faulted, _ = runner.fault(fault_name, 4, peer, "controlled")
            configuration["endpoints"][0]["port"] = int(faulted.rsplit(":", 1)[1])
        if args.case == "retirement-flow":
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "retirement-response.stdout")
            configuration["profiles"][0]["timeoutMs"] = NATIVE_TIMEOUT_MS
            peer6, material6 = runner.agent(6)
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Local6",
                                                   communityFile=str(material6["community"])))
            direct6, _ = runner.fault("retirement-progress", 6, peer6, "pass")
            configuration["endpoints"].append({"id": "Direct6", "address": "::1",
                "port": int(direct6.rsplit(":", 1)[1]), "profile": "Local6"})
            read = next(b for b in configuration["bindings"] if b["id"] == "IntegerRead")
            configuration["bindings"].append(dict(read, id="ProgressRead", endpoint="Direct6"))
        if args.case == "chain-flow":
            configuration["profiles"][0]["timeoutMs"] = 1500
            peer6, material6 = runner.agent(6)
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Local6",
                                                   communityFile=str(material6["community"])))
            configuration["endpoints"].append({"id": "Direct6", "address": "::1",
                "port": int(peer6.rsplit(":", 1)[1]), "profile": "Local6"})
            read = next(b for b in configuration["bindings"] if b["id"] == "IntegerRead")
            configuration["bindings"].append(dict(read, id="ChainRead", endpoint="Direct6"))
            for name, family, delay in (("Slow4", 4, 750), ("Fast6", 6, 100),
                                        ("Fast4", 4, 100), ("Slow6", 6, 750)):
                delayed, _ = runner.fault("chain-" + name, family, peer if family == 4 else peer6,
                                          "delay", delay_ms=delay)
                configuration["endpoints"].append({"id": name, "address": "127.0.0.1" if family == 4 else "::1",
                    "port": int(delayed.rsplit(":", 1)[1]), "profile": "Local" if family == 4 else "Local6"})
                configuration["bindings"].append(dict(read, id=name, endpoint=name))
        if args.case == "final-clauses":
            delayed, _ = runner.fault("final-response", 4, peer, "delay", delay_ms=100)
            configuration["profiles"][0]["timeoutMs"] = 1500
            configuration["endpoints"][0]["port"] = int(delayed.rsplit(":", 1)[1])
        if args.case == "scan-proc":
            delayed, _ = runner.fault("scan-proc-delay", 4, peer, "delay", delay_ms=750)
            configuration["profiles"][0]["timeoutMs"] = 1500
            configuration["endpoints"][0]["port"] = int(delayed.rsplit(":", 1)[1])
        if args.case == "live-dtype":
            delayed, _ = runner.fault("dtype-response-delay", 4, peer, "delay", delay_ms=DTYPE_DELAY_MS)
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "dtype-response-delay.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Dtype", timeoutMs=3000))
            configuration["endpoints"].append({"id": "Dtype", "address": "127.0.0.1",
                                               "port": int(delayed.rsplit(":", 1)[1]), "profile": "Dtype"})
            configuration["bindings"] += [dict(b, id="Dtype" + b["id"], endpoint="Dtype")
                                           for b in configuration["bindings"] if b["id"] in ("IntegerRead", "FloatWrite")]
        if args.case == "support-transition":
            delayed, _ = runner.fault("support-response-delay", 4, peer, "delay", delay_ms=DTYPE_DELAY_MS)
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "support-response-delay.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Support", timeoutMs=3000))
            configuration["endpoints"].append({"id": "Support", "address": "127.0.0.1",
                                               "port": int(delayed.rsplit(":", 1)[1]), "profile": "Support"})
            configuration["bindings"] += [dict(b, id="Support" + b["id"], endpoint="Support")
                                           for b in configuration["bindings"] if b["id"] in ("IntegerRead", "FloatWrite")]
        if args.case == "boundaries":
            direct, _ = runner.fault("boundary-response", 4, peer, "pass")
            configuration["endpoints"][0]["port"] = int(direct.rsplit(":", 1)[1])
            for name, index, tag, capacity in (("Octets", 6, "octets", 32767), ("Oid", 7, "oid", 128), ("Ip", 8, "ipAddress", 1)):
                for operation, suffix in (("get", "Read"), ("set", "Write")):
                    configuration["bindings"].append({"id": "Boundary" + name + suffix, "endpoint": "Local",
                        "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0", "operation": operation,
                        "valueType": tag, "capacity": capacity})
            for row, (capacity, _, _, oidcap, _, _) in enumerate(BOUNDARY_ROWS):
                for name, index, tag, size, operation in (("OctetsRead", 6, "octets", capacity, "get"),
                        ("OctetsWrite", 6, "octets", capacity, "set"), ("OidRead", 7, "oid", oidcap, "get")):
                    configuration["bindings"].append({"id": "B" + str(row) + name, "endpoint": "Local",
                        "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0", "operation": operation,
                        "valueType": tag, "capacity": size})
        if args.case == "stale-record":
            direct, _ = runner.fault("stale-response", 4, peer, "pass")
            configuration["endpoints"][0]["port"] = int(direct.rsplit(":", 1)[1])
            runner.env["SNMP3_RECORD_IPC_CONTROL"] = str(output / "stale-ipc")
            runner.env["SNMP3_RECORD_IPC_HELPER"] = str(ROOT / "tests/rewrite/helpers/ipc_fault.py")
            runner.env["SNMP3_RECORD_IPC_PYTHON"] = sys.executable
        if args.case == "contract":
            direct, _ = runner.fault("contract-response", 4, peer, "pass")
            configuration["endpoints"][0]["port"] = int(direct.rsplit(":", 1)[1])
        if args.case == "numeric":
            tags = (("Integer", 1, "integer"), ("Unsigned32", 3, "unsigned32"),
                    ("Counter32", 2, "counter32"), ("Gauge32", 3, "gauge32"),
                    ("TimeTicks", 4, "timeticks"), ("Counter64", 5, "counter64"),
                    ("OpaqueFloat", 9, "opaqueFloat"), ("OpaqueDouble", 10, "opaqueDouble"))
            for name, index, tag in tags:
                for operation, suffix in (("get", "Read"), ("set", "Write")):
                    configuration["bindings"].append({"id": "Matrix" + name + suffix, "endpoint": "Local",
                        "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0", "operation": operation,
                        "valueType": tag, "capacity": 1})
            fixed = (("WideHigh", 19, "counter64"), ("WideMax", 20, "counter64"),
                     ("FloatNaN", 21, "opaqueFloat"), ("FloatInfinity", 22, "opaqueFloat"),
                     ("FloatNegativeInfinity", 23, "opaqueFloat"), ("DoubleNaN", 24, "opaqueDouble"),
                     ("DoubleInfinity", 25, "opaqueDouble"), ("DoubleNegativeInfinity", 26, "opaqueDouble"))
            configuration["bindings"] += [{"id": "Matrix" + name + "Read", "endpoint": "Local",
                "oid": "1.3.6.1.4.1.53864.4." + str(index) + ".0", "operation": "get",
                "valueType": tag, "capacity": 1} for name, index, tag in fixed]
        if args.case in ("deadline-queue", "stop-queued", "rebuild"):
            dropped, _ = runner.fault("queue-response-drop", 4, peer, "drop-all")
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "queue-response-drop.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Slow", timeoutMs=NATIVE_TIMEOUT_MS))
            configuration["endpoints"].append({"id": "QueueDropped", "address": "127.0.0.1",
                                               "port": int(dropped.rsplit(":", 1)[1]), "profile": "Slow"})
            configuration["bindings"].append({"id": "QueueFloatWrite", "endpoint": "QueueDropped",
                                              "oid": "1.3.6.1.4.1.53864.4.9.0", "operation": "set",
                                              "valueType": "opaqueFloat", "capacity": 1})
            runner.env["SNMP3_RECORD_QUEUE_BINDING"] = "QueueFloatWrite"
        if args.case == "near-deadline":
            delayed, _ = runner.fault("near-response-delay", 4, peer, "delay", delay_ms=NEAR_DELAY_MS)
            runner.env["SNMP3_RECORD_DELAY_TRACE"] = str(output / "near-response-delay.stdout")
            configuration["profiles"].append(dict(configuration["profiles"][0], id="Near", timeoutMs=5 * NEAR_DELAY_MS))
            configuration["endpoints"].append({"id": "NearDelayed", "address": "127.0.0.1",
                                               "port": int(delayed.rsplit(":", 1)[1]), "profile": "Near"})
            configuration["bindings"].append({"id": "NearFloatWrite", "endpoint": "NearDelayed",
                                              "oid": "1.3.6.1.4.1.53864.4.9.0", "operation": "set",
                                              "valueType": "opaqueFloat", "capacity": 1})
            runner.env["SNMP3_RECORD_QUEUE_BINDING"] = "NearFloatWrite"
        config = output / "records.json"
        write_json(config, configuration)
        fixtures = [ROOT / "tests/rewrite/db/records.db", ROOT / "tests/rewrite/db/record-output.db"]
        argv = [str(runner.products / "snmp3RecordTest"), str(config),
                str(runner.products / "snmp3NativeProbe"), str(runner.products / "snmp3Worker"),
                str(ROOT / "dbd/snmp3RecordTest.dbd"), *map(str, fixtures), str(output / "workers.stderr"), args.case]
        def execute(tag, extra=None):
            # One actual snmp3RecordTest process; its stdout events and receipt are kept under the tag.
            environment = dict(runner.env, **(extra or {}))
            started = time.monotonic_ns()
            with (output / (tag + ".stdout")).open("xb") as stdout, (output / (tag + ".stderr")).open("xb") as stderr:
                child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=environment)
                forced = False
                try:
                    if "SNMP3_RETIRED_HOLD" in environment:
                        trace = hold_retired(child, runner.products / "snmp3Worker", Path(environment["SNMP3_RETIRED_HOLD"]))
                        write_json(output / (tag + ".retired.json"), trace)
                        runner.check(tag + ":retired-hold", all(trace.get(key) for key in
                                     ("attached", "held", "released", "detached")) and
                                     not trace.get("error") and not trace.get("cleanup_error") and
                                     trace.get("activation") == 1 and
                                     (trace.get("batch", 0) > 1 if args.case == "retirement-flow" else trace.get("batch") == 1))
                    code = child.wait(timeout=180 if args.case in ("numeric", "scan-proc", "chain-flow", "boundaries") + QUEUE_CASES else 30)
                except subprocess.TimeoutExpired:
                    forced = True
                    child.kill()
                    code = child.wait(timeout=10)
            libraries = runner.loader_identity(output / (tag + ".stderr"))
            write_json(output / (tag + ".receipt.json"), {"argv": argv, "returncode": code,
                       "environment": {key: extra[key] for key in sorted(extra or {})},
                       "pid": child.pid, "child_reaped": child.returncode is not None,
                       "forced_cleanup": forced, "elapsed_ns": time.monotonic_ns() - started,
                       "loaded_libraries": libraries, "product_sha256": digest(Path(argv[0]))})
            runner.check(tag + ":return", code == 0 and not forced)
            events = []
            for line in (output / (tag + ".stdout")).read_text().splitlines():
                if not line.startswith("{"):
                    continue
                try:
                    event = json.loads(line)
                    if args.case in ("repeat-detach", "live-dtype", "support-transition") and not isinstance(event, dict):
                        raise ValueError("record event must be an object")
                    events.append(event)
                except ValueError:
                    if args.case not in ("repeat-detach", "live-dtype", "support-transition"):
                        raise
                    events.append({"event": "support_error" if args.case == "support-transition" else "dtype_parse_error" if args.case == "live-dtype" else "repeat_detach_parse_error"})
            return libraries, events

        def proxy_sets(name):
            path = output / (name + ".stdout")
            return len([line for line in path.read_text().splitlines() if '"event": "fault_request"' in line and
                        '"command": 163' in line]) if path.exists() else 0

        if args.case == "deadline-queue":
            events, samples = [], []
            for trial in range(1, DEADLINE_TRIALS + 1):
                before = proxy_sets("queue-response-drop")
                libraries, trial_events = execute(f"records-sample-{trial}", {"SNMP3_RECORD_BUDGET_MS": str(DEADLINE_SAMPLE_BUDGET_MS),
                                                                              "SNMP3_RECORD_TRIAL": f"sample-{trial}"})
                for event in trial_events:
                    if event.get("event") == "deadline_queue":
                        event["proxy_sets"] = proxy_sets("queue-response-drop") - before
                        interval = event["admission_to_ready_us"] or event["deadline_to_ready_us"]
                        if interval:
                            samples.append(interval)
                events += trial_events
            runner.check("deadline-threshold-sampled", len(samples) == DEADLINE_TRIALS)
            low, high = (min(samples), max(samples)) if samples else (0, 0)
            threshold = {"event": "deadline_threshold", "trials": DEADLINE_TRIALS, "samples_us": samples,
                         "minimum_us": low, "maximum_us": high}
            events.append(threshold)
            budgets = (("below", max(1, low // 2000)), ("above", max(1, high * 2 // 1000)))
            for label, budget in budgets:
                before = proxy_sets("queue-response-drop")
                libraries, trial_events = execute(f"records-{label}", {"SNMP3_RECORD_BUDGET_MS": str(budget),
                                                                       "SNMP3_RECORD_TRIAL": label})
                for event in trial_events:
                    if event.get("event") == "deadline_queue":
                        event["proxy_sets"] = proxy_sets("queue-response-drop") - before
                        event["budget_ms"] = budget
                events += trial_events
            # Every relaunch the case measured, sampling and budget trials alike, bounds the documented threshold.
            relaunches = list(samples)
            for event in events:
                if event.get("event") == "deadline_queue" and event.get("trial") in ("below", "above"):
                    interval = event["admission_to_ready_us"] or event["deadline_to_ready_us"]
                    if interval:
                        relaunches.append(interval)
            events.append({"event": "deadline_threshold_all", "trials": len(relaunches), "samples_us": relaunches,
                           "minimum_us": min(relaunches) if relaunches else 0, "maximum_us": max(relaunches) if relaunches else 0})
        elif args.case == "near-deadline":
            events = []
            for repeat in range(1, NEAR_REPEATS + 1):
                for margin in NEAR_MARGINS_MS:
                    libraries, trial_events = execute(f"records-near-{margin}ms-{repeat}",
                                                      {"SNMP3_RECORD_BUDGET_MS": str(NEAR_DELAY_MS + margin)})
                    for event in trial_events:
                        if event.get("event") == "near_deadline":
                            event["margin_ms"] = margin
                    events += trial_events
        elif args.case == "stop-enqueue-failed":
            # One process per release mode: inside the drain budget, then after it has expired.
            events = []
            for mode in ("within", "late", "after"):
                libraries, trial_events = execute("records-" + mode, {"SNMP3_RECORD_RELEASE": mode})
                events += trial_events
        elif args.case == "support-transition":
            events = []
            modes = (args.support_mode,) if args.support_mode else SUPPORT_MODES
            trace_path, agent_path = output / "support-response-delay.stdout", output / "agent-ipv4-1.stdout"
            for mode in modes:
                wire_start, agent_start = len(trace_path.read_text().splitlines()), len(agent_path.read_text().splitlines())
                libraries, trial_events = execute("records-" + mode, {"SNMP3_RECORD_SUPPORT_MODE": mode})
                wire = [json.loads(line) for line in trace_path.read_text().splitlines()[wire_start:] if line.startswith("{")]
                stored = [json.loads(line) for line in agent_path.read_text().splitlines()[agent_start:] if line.startswith("{")]
                events += trial_events
                events.append({"event": "support_wire", "mode": mode, "events": wire,
                               "stored": [e for e in stored if e.get("event") == "agent_set_value"]})
                requests = [e for e in wire if e.get("event") == "fault_request"]
                responses = [e for e in wire if e.get("event") == "fault_response"]
                processing = next((e.get("processing") for e in trial_events if e.get("event") == "support_resume"), True)
                expected = 2 if mode in ("active", "drain") or (mode == "idle" and processing) else 0
                runner.check("support-" + mode + "-wire", len(requests) == len(responses) == expected and
                             sorted(e.get("command") for e in requests) == ([160, 163] if expected else []) and
                             sorted(e.get("request_id") for e in requests) == sorted(e.get("request_id") for e in responses) and
                             all(e.get("status") == 0 and e.get("varbinds") == 1 for e in responses))
                values = [e for e in stored if e.get("event") == "agent_set_value"]
                runner.check("support-" + mode + "-captured-SET", (len(values) == 1 and values[0].get("index") == 9 and
                             values[0].get("value") == "7.25") if expected else not values)
                if mode == "active":
                    delivered = [e for e in wire if e.get("event") == "fault_delayed_delivery"]
                    phases = {e["phase"]: e for e in trial_events if e.get("event") == "support"}
                    ordering = len(requests) == len(delivered) == 2
                    for i, label in enumerate(("ai", "ao")):
                        if ordering:
                            ordering = requests[i]["monotonic_ns"] < phases.get("selected-" + label, {}).get("at_us", 0)*1000 <= phases.get("attempted-" + label, {}).get("at_us", 0)*1000 < delivered[i]["monotonic_ns"]
                    runner.check("support-active-external-delay-window", ordering)
            support_checks(runner, events, modes)
        elif args.case == "live-dtype":
            events = []
            trace_path = output / "dtype-response-delay.stdout"
            agent_path = output / "agent-ipv4-1.stdout"
            for mode in DTYPE_MODES:
                wire_start = len(trace_path.read_text().splitlines())
                agent_start = len(agent_path.read_text().splitlines())
                libraries, trial_events = execute("records-" + mode, {"SNMP3_RECORD_DTYPE_MODE": mode})
                wire = [json.loads(line) for line in trace_path.read_text().splitlines()[wire_start:] if line.startswith("{")]
                stored = [json.loads(line) for line in agent_path.read_text().splitlines()[agent_start:] if line.startswith("{")]
                events += trial_events
                events.append({"event": "dtype_wire", "mode": mode, "events": wire,
                               "stored": [e for e in stored if e.get("event") == "agent_set_value"]})
                requests = [e for e in wire if e.get("event") == "fault_request"]
                responses = [e for e in wire if e.get("event") == "fault_response"]
                runner.check("dtype-" + mode + "-wire", len(requests) == len(responses) == 2 and
                             sorted(e.get("command") for e in requests) == [160, 163] and
                             sorted(e.get("request_id") for e in requests) == sorted(e.get("request_id") for e in responses) and
                             all(e.get("status") == 0 and e.get("varbinds") == 1 for e in responses))
                if mode in ("active", "restored"):
                    deliveries = [e for e in wire if e.get("event") == "fault_delayed_delivery"]
                    snapshots = {e["phase"]: e for e in trial_events if e.get("event") == "dtype"}
                    # Judge the original two request windows independently of forbidden later requests.
                    # The exact wire-count check still rejects every additional admission.
                    ordering = len(deliveries) >= 2 and len(requests) >= 2 and [
                        e.get("command") for e in requests[:2]] == [160, 163]
                    for i, label in enumerate(("ai", "ao")):
                        changed = snapshots.get("changed-" + label, {})
                        restored = snapshots.get("restored-" + label, changed)
                        if ordering:
                            ordering = (requests[i]["monotonic_ns"] < changed.get("at_us", 0) * 1000 <=
                                        restored.get("at_us", 0) * 1000 < deliveries[i]["monotonic_ns"])
                    runner.check("dtype-" + mode + "-external-delay-window", ordering)
                values = [e for e in stored if e.get("event") == "agent_set_value"]
                runner.check("dtype-" + mode + "-captured-SET", len(values) == 1 and values[0].get("index") == 9 and
                             values[0].get("value") == "7.25")
            dtype_checks(runner, events)
        elif args.case == "queued-simm":
            events = []
            for mode in ("normal", "deadline"):
                for kind in ("Ao", "Longout", "Int64out", "Stringout", "Lso"):
                    trace = "active-response-delay.stdout" if mode == "normal" else "active-response-drop.stdout"
                    extra = {"SNMP3_SIMM_MODE": mode, "SNMP3_SIMM_KIND": kind,
                             "SNMP3_RECORD_DELAY_TRACE": str(output / trace)}
                    if mode == "normal":
                        extra["SNMP3_RETIRED_HOLD"] = str(output / ("hold-" + kind))
                    libraries, trial_events = execute("records-" + mode + "-" + kind, extra)
                    events += trial_events
        elif args.case == "retirement-flow":
            events = []
            for mode in ("normal", "deadline"):
                for kind in ("Ai", "Longin", "Int64in", "Stringin", "Lsi", "Waveform"):
                    extra = {"SNMP3_RETIREMENT_MODE": mode, "SNMP3_RETIREMENT_KIND": kind}
                    if mode == "normal":
                        extra["SNMP3_RETIRED_HOLD"] = str(output / ("hold-" + kind))
                    libraries, trial_events = execute("records-" + mode + "-" + kind, extra)
                    events += trial_events
        elif args.case == "chain-flow":
            events = []
            for kind in ("Ai", "Longin", "Int64in", "Stringin", "Lsi", "Waveform",
                         "Ao", "Longout", "Int64out", "Stringout", "Lso"):
                libraries, trial_events = execute("records-" + kind, {"SNMP3_CHAIN_KIND": kind})
                events += trial_events
        elif args.case == "live-detach":
            # One process replacing links while requests run, one during the record drain of a stop with a full queue.
            events = []
            for phase in ("live", "drain"):
                libraries, trial_events = execute("records-" + phase, {"SNMP3_RECORD_PHASE": phase})
                events += trial_events
        elif args.case == "stop-downstream":
            # One process holding the downstream lock across the runtime stop, one holding it through the IOC shutdown.
            events = []
            for mode in ("stop", "shutdown"):
                libraries, trial_events = execute("records-" + mode, {"SNMP3_RECORD_RELEASE": mode})
                events += trial_events
        elif args.case in ("stop-queued", "rebuild"):
            before = proxy_sets("queue-response-drop")
            libraries, events = execute("records", {"SNMP3_RECORD_BUDGET_MS": str(STOP_BUDGET_MS)})
            for event in events:
                if event.get("event") == "stop_queued":
                    event["proxy_sets"] = proxy_sets("queue-response-drop") - before
        else:
            libraries, events = execute("records")
        summary = next((event for event in events if event.get("event") == "record_summary"), {})
        # Queue cases skip the baseline and pressure phases; their records are checked per trial below.
        if args.case not in QUEUE_CASES + ("stop-inflight", "stop-enqueue-failed", "stop-downstream", "live-detach", "repeat-detach", "live-dtype", "support-transition", "scan-proc", "alarm-flow", "chain-flow", "queued-simm", "boundaries", "retirement-flow", "contract", "stale-record", "final-clauses"):
            runner.check("actual-eleven-record-path", summary.get("records") == 11 and summary.get("checks", 0) > 100)
            completions = [event for event in events if event.get("event") == "record_completed"]
            runner.check("actual-record-identities", bool(completions) and all(event["activation"] == 1 and
                         event["revision"] == 1 and event["handle"] > 0 for event in completions))
            runner.check("actual-callback-pressure", any(event.get("event") == "callback_pressure" and
                         event.get("enqueue_failures", 0) > 0 and event.get("completed_once") for event in events))
            runner.check("actual-GET-and-SET-pressure", {event.get("record") for event in events if
                         event.get("event") == "callback_pressure"} == {"Records_Longin", "Records_Ao"})
        if args.case == "boundaries":
            boundary_checks(runner, events, output)
        if args.case == "final-clauses":
            phases = [e for e in events if e.get("event") == "callback_phases"]
            runner.check("final-callback-phase-inventory", [e.get("record") for e in phases] ==
                         ["Records_PressureGet", "Records_PressureSet"])
            runner.check("final-callback-owned-after-deadline", len(phases) == 2 and all(
                e.get("pending") == e.get("queued") == e.get("running") == e.get("completions_delta") == 1 and
                e.get("enqueue_failures", 0) > 0 and e.get("bytes", 0) > 0 and
                e.get("released_us", 0) > e.get("deadline_us", 0) > 0 and
                e.get("generation") == 1 and e.get("admission", 0) > 0 for e in phases))
            drive = [e for e in events if e.get("event") == "integer_drive"]
            runner.check("final-integer-drive", [(e.get("requested"), e.get("value"), e.get("readback")) for e in drive] ==
                [(str(value), str(expected), str(expected)) for value, expected in
                 [(-(2**63), 2**53+1), (2**53+2, 2**53+2), (2**63-1, 2**53+3)]])
            rounded = [e for e in events if e.get("event") == "adjacent_rounding"]
            expected_bits = [("3ff000000fffffff", "3f800000"), ("3ff0000010000001", "3f800001"),
                             ("bff000000fffffff", "bf800000"), ("bff0000010000001", "bf800001")]
            runner.check("final-adjacent-rounding", len(rounded) == 16 and
                         len({e.get("mode") for e in rounded}) == 4 and
                         [(e.get("source_hex"), e.get("expected_hex")) for e in rounded] == expected_bits * 4)
            wire = [json.loads(line) for line in (output / "final-response.stdout").read_text().splitlines()]
            requests = [e for e in wire if e.get("event") == "fault_request"]
            runner.check("final-exact-native-no-replay", len(requests) == 40 and
                         sum(e.get("command") == 160 for e in requests) == 20 and
                         sum(e.get("command") == 163 for e in requests) == 20 and
                         sum(e.get("event") == "fault_response" for e in wire) == 40)
        if args.case == "stale-record":
            rows = [e for e in events if e.get("event") == "stale_record"]
            runner.check("stale-record-inventory", len(rows) == 1)
            for row in rows:
                runner.check("stale-record-preserved", row.get("source_generation") == 2 and
                    row.get("successor_generation") == 3 and row.get("held_count") == 2 and
                    row.get("held_bytes", 0) > 0 and row.get("barrier") is True and
                    row.get("completions") == row.get("flnk") == 2)
                timeline = row.get("timeline", [])
                runner.check("stale-record-supervision", sum(e.get("code") == 4 for e in timeline) == 3 and
                    sum(e.get("code") == 5 for e in timeline) == 3 and sum(e.get("code") == 16 for e in timeline) == 1 and
                    all(e["at"] >= row["release_at"] for e in timeline if e.get("code") == 5 and e.get("batch") == 2) and
                    all(e.get("code") not in (6, 7, 8, 9, 12) for e in timeline))
            cleanup = [e for e in events if e.get("event") == "record_ipc_cleanup"]
            runner.check("stale-record-helper-cleanup", len(cleanup) == 1 and cleanup[0].get("reaped") is True and
                         cleanup[0].get("forced") is False and cleanup[0].get("passed") is True)
            ipc = [json.loads(line) for line in (output / "stale-ipc.log").read_text().splitlines() if line.startswith("{")]
            faults = [e for e in ipc if e.get("event") == "record_stale_retired"]
            runner.check("stale-record-real-frame-fault", len(faults) == 1 and faults[0].get("generation") == 1 and
                         faults[0].get("source_generation") == 2 and faults[0].get("changed_offsets") == [87] and
                         len([e for e in ipc if e.get("event") == "record_stale_barrier"]) == 1 and
                         len([e for e in ipc if e.get("event") == "record_original_retired"]) == 1 and
                         len([e for e in ipc if e.get("event") == "ipc_fault_stopped" and e.get("pending") == [0, 0]]) == 1)
            wire = [json.loads(line) for line in (output / "stale-response.stdout").read_text().splitlines()]
            requests = [e for e in wire if e.get("event") == "fault_request"]
            responses = [e for e in wire if e.get("event") == "fault_response"]
            runner.check("stale-record-exact-wire", len(requests) == len(responses) == 3 and
                         all(e.get("command") == 160 for e in requests) and
                         [e.get("request_id") for e in requests] == [e.get("request_id") for e in responses])
        if args.case == "contract":
            kinds = ("Ai", "Longin", "Int64in", "Stringin", "Lsi", "Waveform", "Ao", "Longout", "Int64out", "Stringout", "Lso")
            expected = ["RejectGrammar" + str(i) for i in range(13)]
            expected += [prefix + kind for kind in kinds for prefix in ("RejectOperation", "RejectType")]
            expected += ["RejectFtvl" + str(i) for i in range(9)] + ["RejectLinrai", "RejectLinrao", "RejectScan"]
            runner.check("contract-rejection-inventory", [e.get("record") for e in events if e.get("event") == "contract_rejected"] ==
                         ["Records_" + name for name in expected])
            initial = [e for e in events if e.get("event") == "contract_init"]
            runner.check("contract-initialized-storage", len(initial) == 1 and initial[0].get("rejected") == 47 and
                         initial[0].get("contexts") == 17 and initial[0].get("distinct_handles") == 11 and initial[0].get("default_sizv") == 41)
            rows = [e for e in events if e.get("event") == "contract_simm"]
            runner.check("contract-successful-SIMM-inventory", [e.get("record") for e in rows] == ["Records_" + k for k in kinds[:6]])
            for row in rows:
                runner.check(row["record"] + "-successful-SIMM", row.get("terminal") == row.get("native") == 1 and
                             row.get("generations") == row.get("completions") == 2 and row.get("flnk") == 3 and
                             row.get("simulation_published") is False and row.get("native_recovered") is True)
            rows = [e for e in events if e.get("event") == "contract_linr"]
            runner.check("contract-live-LINR", [e.get("record") for e in rows] == ["Records_Ai", "Records_Ao"] and
                         all(e.get("idle_admitted") is False and e.get("active_completed") is True and
                             e.get("recovered") is True and e.get("generations") == 3 for e in rows))
            wire = [json.loads(line) for line in (output / "contract-response.stdout").read_text().splitlines()]
            requests = [e for e in wire if e.get("event") == "fault_request"]
            responses = [e for e in wire if e.get("event") == "fault_response"]
            runner.check("contract-exact-wire", len(requests) == len(responses) == 26 and
                         sum(e.get("command") == 160 for e in requests) == 17 and
                         sum(e.get("command") == 163 for e in requests) == 9 and
                         [e.get("request_id") for e in requests] == [e.get("request_id") for e in responses])
            runner.check("contract-no-initialization-IO", len(initial) == 1 and len(requests) == 26 and
                         all(e["monotonic_ns"] > initial[0]["at"] * 1000 for e in requests))
        if args.case == "retirement-flow":
            kinds = ("Ai", "Longin", "Int64in", "Stringin", "Lsi", "Waveform")
            rows = [e for e in events if e.get("event") == "input_retirement"]
            runner.check("retirement-inventory", [(e.get("mode"), e.get("record")) for e in rows] ==
                         [(mode, "Records_" + kind) for mode in ("normal", "deadline") for kind in kinds])
            wire = [json.loads(line) for line in (output / "retirement-response.stdout").read_text().splitlines()]
            progress = [json.loads(line) for line in (output / "retirement-progress.stdout").read_text().splitlines()]
            for name, trace, count in (("source", wire, 102), ("independent", progress, 12)):
                requests = [e for e in trace if e.get("event") == "fault_request"]
                responses = [e for e in trace if e.get("event") == "fault_response"]
                runner.check("retirement-" + name + "-wire", len(requests) == len(responses) == count and
                             [e.get("request_id") for e in requests] == [e.get("request_id") for e in responses])
            runner.check("retirement-wire-commands", sum(e.get("event") == "fault_request" and e.get("command") == 163 for e in wire) == 60 and
                         sum(e.get("event") == "fault_request" and e.get("command") == 160 for e in wire) == 42 and
                         sum(e.get("event") == "fault_controlled" and e.get("mode") == "drop-all" for e in wire) == 6)
            for row in rows:
                deadline = row["mode"] == "deadline"
                label = row["mode"] + "-" + row["record"][8:]
                runner.check(label + "-retirement-accounting", row.get("held_count") == 2 and
                             row.get("held_bytes") == 2 * row.get("single_bytes", 0) > 0 and
                             (not deadline or row.get("consumed_count") == 1 and row.get("consumed_bytes") == row.get("single_bytes")))
                runner.check(label + "-retirement-outcome", row.get("terminal") == (2 if deadline else 1) and
                             row.get("sent") is (not deadline) and row.get("source_packets") == (1 if deadline else 2) and
                             row.get("source_generations") == row.get("source_completions") == row.get("flnk") == 3 and row.get("retry_recovered") is True)
                runner.check(label + "-retirement-progress", 0 < row.get("started_at", 0) < row.get("queued_at", 0) <
                             row.get("progress_at", 0) < row.get("released_at", 0) < row.get("reap_at" if deadline else "retired_at", 0) and
                             len([e for e in progress if e.get("event") == "fault_request" and e.get("command") == 160 and
                                  row["queued_at"] * 1000 < e["monotonic_ns"] < row["progress_at"] * 1000]) == 1)
                runner.check(label + "-retirement-message", not deadline or
                             row.get("empty_comm_events", 0) >= 1 and row.get("never_sent_events", 0) >= 1)
                packets = [e for e in wire if e.get("event") == "fault_request" and
                           row["started_at"] * 1000 < e["monotonic_ns"] < row["released_at"] * 1000]
                runner.check(label + "-retirement-blocked-wire", len(packets) == 1 and packets[0].get("command") == 160)
                if not deadline:
                    trace = json.loads((output / ("records-" + label + ".retired.json")).read_text())
                    result = next((e for e in row.get("timeline", []) if e.get("code") == 4 and e.get("at", 0) >= row["started_at"]), {})
                    runner.check(label + "-retirement-genuine-boundary", trace.get("batch") == result.get("batch") and
                                 trace.get("epoch") == result.get("epoch") and result.get("at", 0) < row["queued_at"] and
                                 trace.get("held_ns", 0) < row["queued_at"] * 1000 < row["released_at"] * 1000 <=
                                 trace.get("released_ns", 0) < row["retired_at"] * 1000)
        if args.case == "scan-proc":
            kinds = ("Ao", "Longout", "Int64out", "Stringout", "Lso",
                     "Ai", "Longin", "Int64in", "Stringin", "Lsi", "Waveform")
            rows = [event for event in events if event.get("event") == "scan_proc"]
            runner.check("scan-proc-complete-inventory", [(e.get("record"), e.get("route")) for e in rows] ==
                         [("Records_" + kind, route) for kind in kinds for route in ("scan", "proc")])
            wire = [json.loads(line) for line in (output / "scan-proc-delay.stdout").read_text().splitlines()]
            requests = [e for e in wire if e.get("event") == "fault_request"]
            responses = [e for e in wire if e.get("event") == "fault_response"]
            runner.check("scan-proc-exact-wire-count", len(requests) == len(responses) == 44 and
                         sorted(e.get("request_id") for e in requests) == sorted(e.get("request_id") for e in responses))
            for row in rows:
                label = row["record"] + ":" + row["route"]
                packets = [e for e in requests if row["started_us"] * 1000 <= e["monotonic_ns"] <= row["ended_us"] * 1000]
                command = 160 if row["record"][8:] in kinds[5:] else 163
                runner.check(label + "-wire", len(packets) == 2 and all(e.get("command") == command for e in packets))
                runner.check(label + "-completion", row.get("completions") == row.get("flnk") == 2 and
                             row.get("generation") == row.get("generation_before", -10) + 2 and
                             row.get("pact") == row.get("rpro") == row.get("severity") == 0 and
                             row.get("activation") == row.get("revision") == 1 and row.get("handle", 0) > 0 and
                             (row.get("active_scans", 0) >= 4 if row["route"] == "scan" else row.get("active_scans") == 0))
        if args.case == "queued-simm":
            kinds = ("Ao", "Longout", "Int64out", "Stringout", "Lso")
            rows = [e for e in events if e.get("event") == "queued_simm"]
            runner.check("queued-simm-inventory", [(e.get("mode"), e.get("record")) for e in rows] ==
                         [(mode, "Records_Active" + kind) for mode in ("normal", "deadline") for kind in kinds])
            for row in rows:
                deadline = row["mode"] == "deadline"
                kind = row["record"][len("Records_Active"):]
                expected = {"Ao": "31.5" if deadline else "32.5", "Longout": "31" if deadline else "32",
                            "Int64out": "9007199254740993" if deadline else "9007199254740995",
                            "Stringout": "first-short" if deadline else "latest-short",
                            "Lso": "F" * 200 if deadline else "latest-short"}[kind]
                label = row["mode"] + "-" + kind
                runner.check(label + "-actual-queued-switch", row.get("queued") == row.get("retirement_pending") == 1 and
                             row.get("count") == 2 and row.get("bytes", 0) > 0 and
                             0 < row.get("paused_at", 0) < row.get("switched_at", 0) and
                             (deadline or row.get("switched_at", 0) < row.get("retired_at", 0)))
                runner.check(label + "-completion-and-wire", row.get("terminal") == (2 if deadline else 1) and
                             row.get("sent") is (not deadline) and row.get("source_packets") == (1 if deadline else 2) and
                             row.get("source_generations") == row.get("source_completions") == row.get("flnk") == 2 and
                             row.get("readback") == expected)
                runner.check(label + "-message-monitor", not deadline or
                             row.get("empty_comm_events", 0) >= 1 and row.get("never_sent_events", 0) >= 1)
                if not deadline:
                    trace = json.loads((output / ("records-normal-" + kind + ".retired.json")).read_text())
                    result = next((e for e in row.get("timeline", []) if e.get("code") == 4), {})
                    runner.check(label + "-actual-Retired-boundary", trace.get("held") and trace.get("released") and
                                 trace.get("batch") == result.get("batch") == 1 and
                                 trace.get("epoch") == result.get("epoch") and
                                 trace.get("held_ns", 0) < row.get("paused_at", 0) * 1000 < row.get("switched_at", 0) * 1000 <
                                 trace.get("released_ns", 0) < row.get("retired_at", 0) * 1000 and
                                 result.get("at", 0) < row.get("paused_at", 0))
            for name, expected in (("active-response-delay", 10), ("active-response-drop", 5)):
                wire = [json.loads(line) for line in (output / (name + ".stdout")).read_text().splitlines()]
                requests = [e for e in wire if e.get("event") == "fault_request"]
                responses = [e for e in wire if e.get("event") == "fault_response"]
                runner.check(name + "-queued-simm-wire", len(requests) == len(responses) == expected and
                             all(e.get("command") == 163 for e in requests) and
                             [e["request_id"] for e in requests] == [e["request_id"] for e in responses])
        if args.case == "chain-flow":
            kinds = ("Ai", "Longin", "Int64in", "Stringin", "Lsi", "Waveform",
                     "Ao", "Longout", "Int64out", "Stringout", "Lso")
            modes = ("pass", "error-status", "duplicate", "drop-all")
            rows = [e for e in events if e.get("event") == "chain_flow"]
            runner.check("chain-flow-inventory", [(e.get("record"), e.get("mode")) for e in rows] ==
                         [("Records_" + kind, mode) for kind in kinds for mode in modes])
            for i, row in enumerate(rows):
                mode = row["mode"]
                good = mode in ("pass", "duplicate")
                kind = row["record"][len("Records_"):]
                alarm = 0 if good else 9 if mode == "drop-all" else 1 if kind in kinds[:6] else 2
                runner.check(row["record"] + ":" + mode, row.get("terminal") == (1 if good else 2 if mode == "drop-all" else 6) and
                             row.get("native") == (1 if good else 0 if mode == "drop-all" else 6) and
                             row.get("alarm") == alarm and row.get("severity") == (0 if good else 3) and
                             row.get("source_pact_at_flnk") == 1 and row.get("completions") == 2 and row.get("flnk") == 1 and
                             row.get("target_generation") == i % 4 + 1 and row.get("source_handle", 0) > 0 and
                             row.get("target_handle", 0) > 0 and row.get("finished_at", 0) > row.get("terminal_at", 0) > 0 and
                             row.get("value_length") == (201 if kind in ("Lsi", "Lso") else 0))
            wire = [json.loads(line) for line in (output / "chain-response.stdout").read_text().splitlines()]
            controlled = [e for e in wire if e.get("event") == "fault_controlled"]
            runner.check("chain-exact-wire", len(controlled) == 110 and
                         len([e for e in wire if e.get("event") == "fault_request"]) == 110 and
                         len([e for e in wire if e.get("event") == "fault_response"]) == 110)
            runner.check("chain-fault-counts", [sum(e.get("mode") == mode for e in controlled) for mode in modes] == [77, 11, 11, 11] and
                         len([e for e in wire if e.get("event") == "fault_duplicate"]) == 11)
            fanout = [e for e in events if e.get("event") == "fanout_flow"]
            runner.check("fanout-inventory", [e.get("trial") for e in fanout] == [0, 1] * 11)
            for i, row in enumerate(fanout):
                runner.check("fanout-order-" + str(i), row.get("no_barrier") is True and row.get("completions") == 2 and
                             row.get("first") == ("Records_Fast4" if i % 2 else "Records_Slow4") and
                             row.get("second") == ("Records_Slow6" if i % 2 else "Records_Fast6") and
                             row.get("started", 0) < row.get("fast_at", 0) < row.get("slow_at", 0))
            for name, delay in (("Slow4", 750), ("Fast6", 100), ("Fast4", 100), ("Slow6", 750)):
                wire = [json.loads(line) for line in (output / ("chain-" + name + ".stdout")).read_text().splitlines()]
                requests = [e for e in wire if e.get("event") == "fault_request"]
                responses = [e for e in wire if e.get("event") == "fault_response"]
                delivery = [e for e in wire if e.get("event") == "fault_delayed_delivery"]
                runner.check("fanout-wire-" + name, len(requests) == len(responses) == len(delivery) == 11 and
                             all(q["request_id"] == r["request_id"] and d["monotonic_ns"] - r["monotonic_ns"] >= delay * 1000000
                                 for q, r, d in zip(requests, responses, delivery)))
        if args.case == "alarm-flow":
            kinds = ("Ai", "Longin", "Int64in", "Stringin", "Lsi", "Waveform")
            modes = ("error-status", "too-big", "bad-error-index", "no-such-object",
                     "no-such-instance", "end-of-mib", "wrong-type", "drop-all")
            rows = [e for e in events if e.get("event") == "alarm_after_good"]
            runner.check("alarm-flow-inventory", [(e.get("record"), e.get("mode")) for e in rows] ==
                         [("Records_" + kind, mode) for kind in kinds for mode in modes])
            terminals = [e for e in events if e.get("event") == "native_terminal"]
            runner.check("alarm-flow-terminal-inventory", len(terminals) == 50)
            for index, row in enumerate(rows):
                label = row["record"] + ":" + row["mode"]
                alarm = 9 if row["mode"] == "drop-all" else 1
                runner.check(label + "-preserved-and-monitored", row.get("preserved") is True and
                             row.get("native_success") is True and row.get("published") is False and
                             row.get("alarm") == alarm and row.get("severity") == 3 and row.get("udf") == 0 and
                             row.get("monitor") == alarm * 10 + 3 and row.get("generation") == 2 * (index % 8 + 1))
                terminal = terminals[index] if index < len(terminals) else {}
                mode = row["mode"]
                native = 6 if mode in ("error-status", "too-big") else 9 if mode in ("bad-error-index", "wrong-type") else 2 if mode == "drop-all" else 1
                runner.check(label + "-native", terminal.get("record") == row["record"] and
                             terminal.get("native_code") == native and terminal.get("outcome") == (IPC_COMPLETE if native == 1 else IPC_NATIVE_FAILURE) and
                             terminal.get("error_status") == (1 if mode == "too-big" else 5 if mode in ("error-status", "bad-error-index") else 0) and
                             terminal.get("error_index") == (1 if mode == "error-status" else 2 if mode == "bad-error-index" else 0))
            competing = [e for e in events if e.get("event") == "competing_alarm"]
            runner.check("alarm-flow-competing-and-recovery", len(competing) == 1 and all(
                         competing[0].get(k) is True for k in ("limit_observed", "native_error_wins", "udf_preserved", "recovered")))
            wire = [json.loads(line) for line in (output / "alarm-response.stdout").read_text().splitlines()]
            controlled = [e for e in wire if e.get("event") == "fault_controlled"]
            runner.check("alarm-flow-real-fault-responses", all(sum(e.get("mode") == mode for e in controlled) ==
                         (8 if mode == "error-status" else 6) for mode in modes))
            runner.check("alarm-flow-exact-wire", len([e for e in wire if e.get("event") == "fault_request"]) ==
                         len([e for e in wire if e.get("event") == "fault_response"]) == 105)
        if args.case == "shutdown":
            runner.check("actual-blocked-shutdown", any(event.get("event") == "blocked_shutdown" and
                         event.get("waited_for_entered") and event.get("restart_rejected") for event in events))
        if args.case == "queued-shutdown":
            runner.check("actual-queued-shutdown", any(event.get("event") == "queued_shutdown" and
                         event.get("storage_retained_until_cleanup") and event.get("abandoned_finalized_once")
                         and event.get("restart_rejected") for event in events))
        if args.case == "edges":
            runner.check("actual-input-edges", any(event.get("event") == "input_edges" and
                         event.get("precision_rejected") and event.get("binary_exact") for event in events))
            runner.check("actual-Base-FLNK-order", any(event.get("event") == "base_order" and
                         event.get("flnk_source_pact") == 1 and event.get("target_completions") == 1 for event in events))
            runner.check("actual-six-input-simulation", len([event for event in events if
                         event.get("event") == "simulation_bypass" and event.get("terminal_released")]) == 6)
            runner.check("actual-normal-mode-native-publication", len([event for event in events if
                         event.get("event") == "simulation_bypass" and event.get("normal_native_published")]) == 4)
            runner.check("actual-output-edges", any(event.get("event") == "output_edges" and
                         event.get("rounding_modes") == 4 and event.get("wire_bit_fixtures") == 36 and
                         event.get("invalid_set_rejected") for event in events))
            runner.check("actual-max-payload-queue", any(event.get("event") == "max_payload_queue" and
                         event.get("data_bytes") == 32766 and event.get("rejected_without_admission") and
                         event.get("retained_before_release") for event in events))
        if args.case == "alarms":
            runner.check("actual-native-timeout-terminal", len([event for event in events if
                         event.get("event") == "native_terminal" and event.get("outcome") == 6 and
                         event.get("native_code") == 2 and event.get("error_status") == 0 and
                         event.get("error_index") == 0]) == 11)
            runner.check("actual-eleven-native-timeouts", len([event for event in events if
                         event.get("event") == "native_timeout" and event.get("communication_alarm")]) == 11)
            runner.check("actual-record-deadline", any(event.get("event") == "record_deadline" and
                         event.get("communication_alarm") for event in events))
            runner.check("actual-deadline-terminal", any(event.get("event") == "native_terminal" and
                         event.get("record") == "Records_Deadline" and event.get("outcome") == 2 for event in events))
            fault_events = [json.loads(line) for line in
                            (output / "record-response-drop.stdout").read_text().splitlines()]
            runner.check("actual-UDP-responses-dropped", len([event for event in fault_events if
                         event.get("event") == "fault_dropped"]) >= 11)
        if args.case == "active":
            active_events = [event for event in events if event.get("event") == "active_output"]
            runner.check("actual-five-output-RPRO", len(active_events) == 5 and all(
                         event.get("first_payload_owned") and event.get("latest_requested_preserved") and
                         event.get("generations") == 2 for event in active_events))
            delay_events = [json.loads(line) for line in
                            (output / "active-response-delay.stdout").read_text().splitlines()]
            requests = [event for event in delay_events if event.get("event") == "fault_request" and
                        event.get("command") == 163]
            runner.check("actual-ten-delayed-SET-packets", len(requests) == 10)
            runner.check("changes-after-real-SET-transmission", len(requests) == 10 and len(active_events) == 5 and all(
                         requests[index * 2]["monotonic_ns"] <= event["changed_at_us"] * 1000
                         for index, event in enumerate(active_events)))
            drop_events = [json.loads(line) for line in
                           (output / "active-response-drop.stdout").read_text().splitlines()]
            dropped_requests = [event for event in drop_events if event.get("event") == "fault_request" and
                                event.get("command") == 163]
            runner.check("explicit-only-same-value-retry", len(dropped_requests) == 4 and
                         dropped_requests[0]["request_id"] != dropped_requests[1]["request_id"] and any(
                         event.get("event") == "explicit_retry" and event.get("generations") == 2 for event in events))
            runner.check("actual-configured-native-retry", len(dropped_requests) == 4 and
                         dropped_requests[2]["request_id"] == dropped_requests[3]["request_id"] and any(
                         event.get("event") == "native_retry" and event.get("generations") == 1 for event in events))
            dropped_responses = [event for event in drop_events if event.get("event") == "fault_response"]
            runner.check("actual-successful-SET-responses-lost", len(dropped_responses) == 4 and all(
                         event.get("command") == 162 and event.get("status") == 0 and event.get("index") == 0
                         for event in dropped_responses))
        if args.case == "policy":
            policies = [event for event in events if event.get("event") == "output_policy"]
            runner.check("actual-five-output-ivoa-completion", len([event for event in policies if
                         not event["simulation"] and event["oopt"] == 0]) == 15 and all(
                         event["terminal_released_once"] and event["latest_requested_preserved"] for event in policies))
            runner.check("actual-five-output-simulation-bypass", len([event for event in policies if event["simulation"]]) == 5)
            runner.check("actual-all-live-oopt-choices", {event["oopt"] for event in policies if event["oopt"]} == set(range(1, 6)))
            runner.check("actual-five-output-sync-and-delayed-simulation", len([event for event in events if
                         event.get("event") == "output_simulation" and not event["native_admission"]]) == 10)
            runner.check("actual-five-first-pass-ivov", len([event for event in events if
                         event.get("event") == "output_ivov" and event["wire_readback_exact"]]) == 5)
            runner.check("actual-five-first-pass-continue-and-no-drive", len([event for event in events if
                         event.get("event") == "output_first_policy"]) == 10)
            runner.check("actual-ao-drive-oval-wire", any(event.get("event") == "output_drive" and
                         event["drive_clipped"] and event["oval_wire_exact"] for event in events))
            runner.check("actual-lso-dol-and-Base-capacity-boundary", any(event.get("event") == "output_dol" and
                         event["supervisory_ignores_dol"] and event["closed_loop_data_bytes"] == 200 and
                         event["source_data_bytes"] == 300 and event["base_prepared_data_bytes"] == 255 and
                         event["wire_readback_exact"] for event in events))
            switches = [event for event in events if event.get("event") == "input_source_switch"]
            runner.check("actual-six-input-failing-source-switch", len({event["record"] for event in switches}) == 6 and all(
                         event["native_failure_alarm"] and event["siol_selected"] and event["terminal_released_once"] and
                         not event["native_publication"] and event["normal_failure_preserved"] and
                         not event["ownerless_admission"] for event in switches))
            fault_events = [json.loads(line) for line in (output / "record-response-drop.stdout").read_text().splitlines()]
            runner.check("actual-policy-GET-count", len([event for event in fault_events if
                         event.get("event") == "fault_request" and event.get("command") == 160]) == 12)
            runner.check("actual-policy-SET-count", len([event for event in fault_events if
                         event.get("event") == "fault_request" and event.get("command") == 163]) == 38)
        if args.case == "numeric":
            inputs_observed = [event for event in events if event.get("event") == "numeric_input"]
            outputs_observed = [event for event in events if event.get("event") == "numeric_output"]
            summary_numeric = next((event for event in events if event.get("event") == "numeric_summary"), {})
            db_reads = [e for e in events if e.get("event") == "numeric_db_read"]
            db_writes = [e for e in events if e.get("event") == "numeric_db_write"]
            signed = {e["value"] for e in db_reads if e["type"] == "INT64"}
            unsigned = {e["value"] for e in db_reads if e["type"] == "UINT64"}
            written = {e["value"] for e in db_writes}
            runner.check("actual-DBR-64-bit-integer-access", all(values <= observed for values, observed in (
                ({"-9223372036854775808", "-2147483648", "9007199254740993", "9223372036854775807"}, signed),
                ({"4294967295", "9007199254740993", "9223372036854775808", "18446744073709551615"}, unsigned),
                ({"-9223372036854775808", "-2147483648", "2147483647", "4294967295",
                  "9007199254740993", "9223372036854775807"}, written))))
            runner.check("actual-68-advertised-numeric-GET-pairs", len({event["record"] for event in inputs_observed
                         if not event["fixed"]}) == 68)
            runner.check("actual-48-waveform-tag-FTVL-pairs", len({event["record"] for event in inputs_observed
                         if not event["fixed"] and "Wave" in event["record"]}) == 48)
            runner.check("all-advertised-numeric-inputs-have-success", len({event["record"] for event in inputs_observed
                         if not event["fixed"] and event["accepted"]}) == 68)
            runner.check("actual-20-advertised-numeric-SET-pairs", len({event["record"] for event in outputs_observed
                         if event["accepted"]}) == 20)
            runner.check("actual-60-fixed-high-or-nonfinite-GET-pairs", len({event["record"] for event in inputs_observed
                         if event["fixed"]}) == 60)
            fixed_inputs = [event for event in inputs_observed if event["fixed"]]
            fixed_names = {event["record"] for event in fixed_inputs}
            runner.check("fixed-inputs-seed-before-extreme", len(fixed_inputs) == 120 and len(fixed_names) == 60 and
                         all(len(pair := [e for e in fixed_inputs if e["record"] == name]) == 2 and
                             pair[0]["sample"] == "fixed_seed" and pair[0]["accepted"] and
                             not pair[0]["prior_native_success"] and pair[1]["sample"] != "fixed_seed" and
                             pair[1]["prior_native_success"] for name in fixed_names))
            rejected_fixed = [e for e in fixed_inputs if not e["accepted"]]
            runner.check("fixed-lossy-inputs-preserve-valid-data", len(rejected_fixed) == 55 and
                         all(e["prior_native_success"] and e["preserved_on_rejection"] for e in rejected_fixed))
            wire = [json.loads(line) for line in (output / "numeric-response.stdout").read_text().splitlines()
                    if line.startswith("{")]
            seeds = [e for e in wire if e.get("event") == "numeric_seed"]
            runner.check("fixed-seed-is-external-response-only", len(seeds) == 60 and
                         len({e["request_id"] for e in seeds}) == 60 and
                         sum(e["tag"] == 70 and e["seed_hex"] == "07" for e in seeds) == 18 and
                         sum(e["seed_hex"] == "9f780440e00000" for e in seeds) == 21 and
                         sum(e["seed_hex"] == "9f7908401c000000000000" for e in seeds) == 21)
            runner.check("failure-after-native-success-preserves-input", summary_numeric.get("failed_after_good", 0) > 50)
            runner.check("numeric-rejections-send-no-native-SET", summary_numeric.get("rejected_outputs", 0) >= 30)
            runner.check("numeric-output-wire-readback", all(event["separate_native_readback"] for event in outputs_observed))
            agent_events = [json.loads(line) for line in (output / "agent-ipv4-1.stdout").read_text().splitlines()
                            if line.startswith("{")]
            runner.check("actual-numeric-SET-actions", len([event for event in
                         agent_events if event.get("event") == "agent_handler" and
                         event.get("mode") == 2]) == summary_numeric.get("accepted_outputs", -1) + summary_numeric.get("stimuli", -1) + 6)
        if args.case == "active-unforced":
            trials = [event for event in events if event.get("event") == "active_unforced"]
            runner.check("unforced-five-outputs-ran", len(trials) == 5)
            for event in trials:
                same = (float(event["wire"]) == float(event["latest"])) if event["record"].endswith("Ao") else event["wire"] == event["latest"]
                runner.check(event["record"] + ":unforced-latest-admitted-and-sent",
                             event["generation"] == 2 and event["completions_delta"] == 2 and
                             event["sevr"] == "NO_ALARM" and event["pact"] == 0 and same)
            exercised = len([event for event in trials if event["branch_exercised"]])
            gaps = [event["retired_us"] - event["result_us"] for event in trials if event["result_us"] and event["retired_us"]]
            events.append({"event": "unforced_summary", "trials": len(trials), "branch_exercised": exercised,
                           "branch_status": "run" if exercised else "not run", "result_to_retired_us": gaps})
        if args.case == "deadline-queue":
            trials = {event["trial"]: event for event in events if event.get("event") == "deadline_queue"}
            summaries = [event for event in events if event.get("event") == "record_summary"]
            runner.check("deadline-trials-completed", len(summaries) == DEADLINE_TRIALS + 2)
            below, above = trials.get("below", {}), trials.get("above", {})
            # The proxy sees the first SET and the follow-up SET; the queued second generation is never sent.
            runner.check("below-threshold-queued-generation-not-sent",
                         below.get("generation") == 2 and not below.get("second_dispatched") and
                         below.get("proxy_sets") == 2 and below.get("stat") == "COMM" and below.get("sevr") == "INVALID")
            runner.check("below-threshold-never-sent-message", below.get("amsg") == "deadline before send")
            # (behindRetirement, behindAdmitted, behindNeverSent): the queued generation was admitted behind its
            # predecessor in both trials and expired unsent only below the threshold.
            runner.check("queue-report-counters",
                         queue_counters(output / "records-below.stdout") == (0, 1, 1) and
                         queue_counters(output / "records-above.stdout") == (0, 1, 0))
            all_relaunches = next((event for event in events if event.get("event") == "deadline_threshold_all"), {})
            listed = all_relaunches.get("samples_us", [])
            measured = [event["admission_to_ready_us"] or event["deadline_to_ready_us"]
                        for event in events if event.get("event") == "deadline_queue"]
            runner.check("threshold-over-every-relaunch",
                         all_relaunches.get("trials") == DEADLINE_TRIALS + 2 and
                         sorted(listed) == sorted(measured) and bool(listed) and
                         all_relaunches.get("minimum_us") == min(listed) and
                         all_relaunches.get("maximum_us") == max(listed) and
                         below.get("budget_ms", 0) * 1000 < all_relaunches.get("minimum_us", 0) and
                         above.get("budget_ms", 0) * 1000 > all_relaunches.get("maximum_us", 0))
            # A successor keeps the deadline of its own admission, so after the operator's put it can be dispatched at
            # most two budgets later, plus the callback delay between the predecessor's deadline and its completion.
            dispatched = [event for event in trials.values() if event.get("second_dispatched")]
            runner.check("put-to-dispatch-within-late-application-bound",
                         len(dispatched) >= 1 and all(
                             event["first_admitted_us"] < event["put_at_us"] and
                             event["put_to_dispatch_us"] == event["dispatched_us"] - event["put_at_us"] and
                             0 < event["put_to_dispatch_us"] <= 2 * event["budget_ms"] * 1000 +
                             max(0, event["admitted_us"] - (event["first_admitted_us"] + event["budget_ms"] * 1000))
                             for event in dispatched))
            runner.check("followup-after-never-sent-has-no-stale-message",
                         below.get("followup_generation") == 3 and below.get("followup_stat") == "COMM" and
                         below.get("followup_amsg") == "")
            runner.check("sent-deadline-without-never-sent-message",
                         len([event for event in trials.values() if event.get("second_dispatched")]) >= 1 and
                         all(event.get("amsg") != "deadline before send" for event in trials.values()
                             if event.get("second_dispatched")))
            runner.check("above-threshold-queued-generation-sent-after-ready",
                         above.get("generation") == 2 and above.get("second_dispatched") and
                         above.get("proxy_sets") == 2 and above.get("stat") == "COMM" and above.get("sevr") == "INVALID")
        if args.case == "near-deadline":
            trials = [event for event in events if event.get("event") == "near_deadline"]
            runner.check("near-deadline-trials-completed", len(trials) == NEAR_TRIALS)
            contained = len([event for event in trials if event["contained_after_result"]])
            exercised = len([event for event in trials if event["grace_exercised"]])
            events.append({"event": "near_deadline_summary", "trials": len(trials), "contained_after_result": contained,
                           "status": "observed" if contained else "not observed", "grace_exercised": exercised,
                           "grace_status": "run" if exercised else "not run"})
        if args.case == "accounting":
            windows = [event for event in events if event.get("event") == "accounting_window"]
            observed = [event for event in windows if event["observed"]]
            runner.check("two-generation-window-observed", bool(observed) and all(
                         event["peak_count"] == 2 and event["peak_bytes"] == 2 * event["single_bytes"] and
                         event["retirement_pending"] == 1 and event["queued"] == 1 for event in observed))
            runner.check("two-generation-trials-latest-sent", len(windows) == 3 and all(
                         event["generations"] == 2 and event["sevr"] == "NO_ALARM" for event in windows))
            limit = next((event for event in events if event.get("event") == "accounting_limit"), {})
            runner.check("count-limit-rejects-reprocess-synchronously", limit.get("limited_generations") == 1 and
                         limit.get("limited_stat") == "WRITE" and limit.get("limited_sevr") == "INVALID" and
                         limit.get("limited_pact") == 0)
            runner.check("raised-limit-admits-explicit-request", limit.get("explicit_sevr") == "NO_ALARM" and
                         limit.get("explicit_completions") == 1)
            events.append({"event": "accounting_summary", "trials": len(windows), "window_observed": len(observed)})
        if args.case == "stop-inflight":
            stop = next((event for event in events if event.get("event") == "stop_inflight"), {})
            rows = stop.get("records", [])
            runner.check("stop-inflight-every-record-completes-with-alarm",
                         len(rows) == 11 and stop.get("stopped") == 11 and stop.get("idle") == 11 and
                         all(row["pact"] == 0 and row["stat"] == "COMM" and row["sevr"] == "INVALID" and
                             row["published"] is False for row in rows))
            runner.check("stop-inflight-waveform-busy-clear", stop.get("waveform_busy") == 0 and
                         any(row["record"].endswith("Waveform") and row["busy"] == 0 for row in rows))
            runner.check("stop-inflight-completion-and-FLNK-once-each",
                         stop.get("completions_delta") == 11 and stop.get("flnk_delta") == 11)
            codes = [item["code"] for item in stop.get("timeline", [])]
            # One generation is on the worker channel and ten wait in the queue; no native result precedes the stop.
            runner.check("stop-inflight-one-sent-and-ten-queued-at-stop",
                         14 in codes and stop.get("pending_at_stop") == 11 and stop.get("held_queued") == 10 and
                         not any(item["code"] == 4 and item["at"] < stop.get("stop_at_us", 0)
                                 for item in stop.get("timeline", [])))
            runner.check("stop-inflight-drain-succeeds-and-worker-reaped",
                         stop.get("drain_failed") is False and stop.get("state") == 4 and stop.get("settled") is True and
                         stop.get("reap_at_us", 0) > 0 and stop.get("retirement_pending_after") == 0)
        if args.case == "stop-enqueue-failed":
            stops = {event["mode"]: event for event in events if event.get("event") == "stop_enqueue_failed"}
            cleanups = [event for event in events if event.get("event") == "stop_enqueue_failed_cleanup"]
            within, late, after = stops.get("within", {}), stops.get("late", {}), stops.get("after", {})
            runner.check("enqueue-failed-stop-retries-every-refused-completion",
                         all(stop.get("retrying") and stop.get("held_active") == 11 and stop.get("held_pending") == 11 and
                             stop.get("held_queued") == 0 and stop.get("enqueue_failures_delta", 0) >= 11
                             for stop in (within, late, after)) and len(stops) == 3)
            rows = within.get("records", [])
            # A stop shorter than the supervisor bound proves the runtime thread was still running at the release:
            # it leaves its bound early only once every borrowed terminal has been released by a completion.
            runner.check("release-within-budget-completes-every-record-once",
                         within.get("still_stopping_at_release") is True and within.get("drain_failed") is False and
                         0 < within.get("stop_duration_us", 0) < STOP_BOUND_MS * 1000 and
                         within.get("state") == 4 and within.get("completions_delta") == 11 and
                         within.get("flnk_delta") == 11 and within.get("stopped") == 11 and within.get("idle") == 11 and
                         within.get("waveform_busy") == 0 and len(rows) == 11 and
                         all(row["pact"] == 0 and row["sevr"] == "INVALID" and row["published"] is False for row in rows))
            rows = late.get("records", [])
            # The release lands after the supervisor stop bound and before the drain budget ends, so only the drain
            # can still enqueue the refused completions.
            held_us = late.get("release_at_us", 0) - late.get("stop_at_us", 0)
            runner.check("release-after-stop-bound-completes-every-record-once",
                         late.get("still_stopping_at_release") is True and late.get("drain_failed") is False and
                         STOP_BOUND_MS * 1000 < held_us < (STOP_BOUND_MS + DRAIN_BUDGET_MS) * 1000 and
                         late.get("state") == 4 and late.get("completions_delta") == 11 and late.get("flnk_delta") == 11 and
                         late.get("stopped") == 11 and late.get("idle") == 11 and len(rows) == 11 and
                         all(row["pact"] == 0 and row["sevr"] == "INVALID" for row in rows))
            rows = after.get("records", [])
            runner.check("release-after-expiry-fails-drain-and-retains-records",
                         after.get("expired_before_release") is True and after.get("returned_before_release") is True and
                         after.get("drain_failed") is True and after.get("state") == 5 and
                         after.get("restart_accepted") is False and after.get("completions_delta") == 0 and
                         after.get("flnk_delta") == 0 and after.get("active_after") == 11 and after.get("idle") == 0 and
                         len(rows) == 11 and all(row["pact"] == 1 for row in rows))
            runner.check("retained-records-finalized-once-at-cleanup",
                         len(cleanups) == 3 and all(event.get("contexts") == 0 and event.get("completions") == 11
                                                    for event in cleanups) and after.get("completions_delta") == 0)
        if args.case == "repeat-detach":
            repeat_detach_checks(runner, events)
        if args.case == "live-detach":
            cells = {event["phase"]: event for event in events if event.get("event") == "live_detach"}
            live, drain = cells.get("live", {}), cells.get("drain", {})
            # Scenario preconditions, which no product defect can make the only false term, so no control isolates
            # them: both phases ran (len(cells) == 2), and in the drain phase the stop was still running with both
            # completions pending when the probe landed (still_stopping_at_attempt, pending_during). Every other term
            # is a product property with a shipped control, except the five refused terms, which are redundant:
            # Base clears dpvt before add_record whenever it accepts a link put and snmp3 never restores it, so
            # whenever the paired intact term holds the refused term holds, and no defect makes a refused term the
            # only false term of its check. active_before (read after the in-flight probe) is decided by
            # refusal-releases-record in the live phase and by refusal-releases-record-with-alarm in the drain phase,
            # where the first control also changes the alarms; the drain phase's open_after is decided by
            # retried-completion-leaves-record-active.
            # The drain in_flight_intact term is decided by refused-detach-toggles-active-handle within the drain
            # check; the live in-flight check fails under the same control, because the product sees the same state
            # at both in-flight probes.
            runner.check("live-replacement-refused-while-request-in-flight",
                         len(cells) == 2 and live.get("active_before") == 2 and
                         live.get("in_flight_refused") is True and live.get("in_flight_intact") is True)
            runner.check("in-flight-request-completes-once-after-refusal",
                         live.get("open_after") == 0 and live.get("completions_delta") == 2 and
                         live.get("alarms") == [COMM_INVALID, COMM_INVALID])
            runner.check("live-replacement-refused-idle-and-after-operator-stop",
                         live.get("idle_refused") is True and live.get("idle_intact") is True and
                         live.get("stopped_refused") is True and live.get("stopped_intact") is True and
                         live.get("entry_open") is False and live.get("detach_allowed") is False)
            runner.check("live-replacement-refused-during-record-drain",
                         drain.get("active_before") == 2 and drain.get("in_flight_refused") is True and
                         drain.get("in_flight_intact") is True and drain.get("still_stopping_at_attempt") is True and drain.get("pending_during") == 2 and
                         drain.get("during_drain_refused") is True and drain.get("during_drain_intact") is True and
                         drain.get("open_after") == 0 and drain.get("completions_delta") == 2 and
                         drain.get("alarms") == [COMM_INVALID, COMM_INVALID] and drain.get("drain_failed") is False)
        if args.case == "stop-downstream":
            stops = {event["mode"]: event for event in events if event.get("event") == "stop_downstream"}
            stop, shutdown = stops.get("stop", {}), stops.get("shutdown", {})
            runner.check("downstream-link-is-external-and-connected",
                         len(stops) == 2 and all(event.get("link_connected") and event.get("separate_lockset")
                                                 for event in stops.values()))
            rows = stop.get("records", [])
            # The lock is held past the drain budget; the stop neither waits for it nor fails its drain.
            runner.check("held-downstream-does-not-hold-the-stop",
                         stop.get("still_running_at_release") is False and stop.get("state_while_held") == 4 and
                         stop.get("drain_failed_while_held") is False and stop.get("completions_while_held") == 11 and
                         0 < stop.get("stop_duration_us", 0) < STOP_BOUND_MS * 1000 and
                         stop.get("release_at_us", 0) - stop.get("stop_at_us", 0) > DRAIN_BUDGET_MS * 1000 and
                         stop.get("stopped") == 11 and stop.get("idle") == 11 and len(rows) == 11 and
                         all(row["pact"] == 0 and row["sevr"] == "INVALID" for row in rows))
            # Eleven processings, one per link: a repeated FLNK on one link would be absorbed by Base, which keeps one
            # pending put per CA link, so once-per-record is proven by the completion count and the stop-inflight case.
            runner.check("eleven-downstream-processings-after-release",
                         stop.get("external_while_held") == 0 and stop.get("external_after_release") == 11)
            # Through the IOC shutdown the snmp3 stop completes first; Base then waits for the downstream lock.
            runner.check("shutdown-snmp3-stop-completes-before-held-downstream",
                         shutdown.get("still_running_at_release") is True and shutdown.get("state_while_held") == 4 and
                         shutdown.get("drain_failed_while_held") is False and shutdown.get("completions_while_held") == 11 and
                         shutdown.get("reap_at_us", 0) > 0 and shutdown.get("external_while_held") == 0 and
                         shutdown.get("stop_duration_us", 0) > DRAIN_BUDGET_MS * 1000)
        if args.case in ("stop-queued", "rebuild"):
            stop = next((event for event in events if event.get("event") == "stop_queued"), {})
            # Product checks apply only when the harness observed the reprocess before stopping. A missing event
            # without a trial failure line means the stop under test itself failed, which the checks judge.
            before_wait = "record test failed after" in (output / "records.stderr").read_text(errors="replace")
            if stop.get("wait_exit") not in ("queued", "rejected") and (stop or before_wait):
                write_json(output / "record-observations.json", events)
                raise InconclusiveWait()
            runner.check("stop-with-retirement-pending-and-queued-successor",
                         stop.get("queued_successor") and stop.get("held_retirement_pending") == 1 and stop.get("held_queued") == 1)
            runner.check("queued-successor-completes-stopping-once",
                         stop.get("generation") == 2 and stop.get("pact") == 0 and stop.get("sevr") == "INVALID" and
                         stop.get("flnk_delta") == 2 and stop.get("completions_delta") == 2 and stop.get("proxy_sets") == 1)
            runner.check("stopping-successor-has-no-never-sent-message", stop.get("amsg") == "")
            runner.check("queue-report-counters", queue_counters(output / "records.stdout") == (0, 1, 0))
            runner.check("drain-succeeds-before-predecessor-reap",
                         stop.get("drain_failed") is False and stop.get("settled") is True)
        if args.case == "rebuild":
            rebuilt = next((event for event in events if event.get("event") == "rebuild"), {})
            counters = queue_counter_lines(output / "records.stdout")
            runner.check("rebuild-second-activation-new-worker",
                         rebuilt.get("second_activation") == rebuilt.get("first_activation", 0) + 1 and
                         rebuilt.get("first_worker_gone") is True and rebuilt.get("second_worker", 0) > 0 and
                         rebuilt.get("second_worker") != rebuilt.get("first_worker"))
            runner.check("rebuild-new-scheduler-counters",
                         len(counters) >= 2 and counters[0] == (0, 1, 0) and counters[-1] == (0, 0, 0))
        runner.check("IOC-native-free", bool(libraries) and not any("netsnmp" in path for path in libraries))
        write_json(output / "record-observations.json", events)
    except Exception as error:
        aborted = True
        write_json(output / "abort.json", {"exception_class": type(error).__name__})
    finally:
        runner.stop()
        runner.scan()
    inputs = sorted((ROOT / "snmp3App/src").glob("*.cpp")) + sorted((ROOT / "snmp3App/src").glob("*.h"))
    inputs += [Path(__file__).resolve(), ROOT / "tests/rewrite/RecordTest.cpp",
               ROOT / "tests/rewrite/NativeAgent.cpp", ROOT / "tests/rewrite/db/records.db",
               ROOT / "tests/rewrite/db/record-output.db", ROOT / "dbd/snmp3RecordTest.dbd"]
    if args.case == "edges":
        inputs += [ROOT / "tests/rewrite/db/record-edges.db", ROOT / "tests/rewrite/db/record-order.db"]
    if args.case in ("scan-proc", "alarm-flow", "chain-flow"):
        inputs += [ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "chain-flow":
        inputs += [ROOT / "tests/rewrite/db/record-chain.db"]
    if args.case == "alarms":
        inputs += [ROOT / "tests/rewrite/db/record-alarms.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case in ("stop-inflight", "stop-enqueue-failed", "stop-downstream", "live-detach", "repeat-detach"):
        inputs += [ROOT / "tests/rewrite/db/record-stop.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case in ("active", "active-unforced", "queued-simm"):
        inputs += [ROOT / "tests/rewrite/db/record-active.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "queued-simm":
        inputs += [ROOT / "tests/rewrite/db/record-queued-simm.db", ROOT / "tests/rewrite/helpers/retired_hold.py",
                   ROOT / "tests/rewrite/helpers/runtime_thread_limit.py"]
    if args.case in QUEUE_CASES:
        inputs += [ROOT / "tests/rewrite/db/record-queue.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "policy":
        inputs += [ROOT / "tests/rewrite/db/record-policy.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "numeric":
        inputs += [ROOT / "tests/rewrite/db/record-numeric.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "boundaries":
        inputs += [ROOT / "tests/rewrite/db/record-boundaries.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "retirement-flow":
        inputs += [ROOT / "tests/rewrite/db/record-retirement.db", ROOT / "tests/rewrite/helpers/udp_fault.py",
                   ROOT / "tests/rewrite/helpers/retired_hold.py", ROOT / "tests/rewrite/helpers/runtime_thread_limit.py"]
    if args.case == "stale-record":
        inputs += [ROOT / "tests/rewrite/db/record-stale.db", ROOT / "tests/rewrite/helpers/ipc_fault.py",
                   ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "contract":
        inputs += [ROOT / "tests/rewrite/db/record-contract.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case == "final-clauses":
        inputs += [ROOT / "tests/rewrite/db/record-final.db", ROOT / "tests/rewrite/helpers/udp_fault.py"]
    if args.case in ("live-dtype", "support-transition"):
        inputs += [ROOT / ("tests/rewrite/db/record-support.db" if args.case == "support-transition" else "tests/rewrite/db/record-dtype.db"), ROOT / "tests/rewrite/helpers/udp_fault.py",
                   ROOT / "configure/RELEASE.local"]
        base = Path(next(line.split("=", 1)[1].strip() for line in
                    (ROOT / "configure/RELEASE.local").read_text().splitlines() if line.startswith("EPICS_BASE")))
        inputs += [base / "include/epicsVersion.h", base / "include/dbAccessDefs.h", base / "include/alarm.h"]
    inputs += [ROOT / "tests/rewrite/test_native.py"]
    inputs += [runner.products / name for name in ("snmp3RecordTest", "snmp3NativeProbe", "snmp3Worker", "snmp3NativeAgent")]
    passed = not aborted and bool(runner.checks) and all(check["passed"] for check in runner.checks)
    write_json(output / "results.json", {"passed": passed, "aborted": aborted, "checks": runner.checks,
               "inputs": {str(path): digest(path) for path in inputs}, "loaded_libraries": runner.identities,
               "scope": f"Record case {args.case}; qualifies this invocation only and does not close the full T1-T14 matrix"})
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
