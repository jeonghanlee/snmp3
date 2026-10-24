"""Pair grouped polling generations using retained CA monitor timestamps."""

from datetime import datetime, timezone
import re
import statistics


def cycle_summary(path, start_utc_ns, end_utc_ns):
    """Pair observed generation counters using original server timestamp text."""
    records, issues, slots = [], [], {}
    pattern = re.compile(r"^(\S+)\s+(\d{4}-\d\d-\d\d)\s+(\d\d:\d\d:\d\d)(\.\d+)?\s+(-?\d+(?:\.0*)?)(?:\s+.*)?$")
    epoch = datetime(1970, 1, 1, tzinfo=timezone.utc)
    for line in path.read_text().splitlines():
        unavailable = re.fullmatch(r"(\S+)\s+<undefined>\s+(-?\d+(?:\.0*)?)(?:\s+.*)?", line)
        if unavailable:
            pv, value = unavailable.groups()
            number = int(float(value))
            records.append({"pv": pv, "server_timestamp_text": "<undefined>",
                            "server_utc_ns": None, "value": number})
            if "_Poll" in pv and number > 0:
                issues.append("A noninitial poll generation has no server timestamp")
            continue
        match = pattern.fullmatch(line)
        if not match:
            if line.strip():
                issues.append("Unparsed monitor record retained")
            continue
        pv, date, clock, fraction, value = match.groups()
        digits = (fraction or ".")[1:]
        if len(digits) > 9:
            issues.append("Unsupported server timestamp precision")
            continue
        dt = datetime.strptime(date + " " + clock, "%Y-%m-%d %H:%M:%S").replace(tzinfo=timezone.utc) - epoch
        stamp = (dt.days * 86400 + dt.seconds) * 1000000000 + int(digits.ljust(9, "0") or "0")
        row = {"pv": pv, "server_timestamp_text": date + " " + clock + (fraction or ""),
               "server_utc_ns": stamp, "value": int(float(value))}
        records.append(row)
        if "_Poll" in pv:
            stem, suffix = pv.rsplit("_Poll", 1)
            tier = suffix.removesuffix("Done")
            kind = "done" if suffix.endswith("Done") else "start"
            slots.setdefault(tier, {}).setdefault(row["value"], {})[kind] = row
    tiers = {}
    for tier, period_ns in (("State", 5000000000), ("Meas", 10000000000), ("Slow", 60000000000)):
        for kind in ("start", "done"):
            generations = sorted(n for n, pair in slots.get(tier, {}).items() if n > 0 and kind in pair)
            if generations and generations != list(range(generations[0], generations[-1] + 1)):
                issues.append(tier + " " + kind + " generation sequence has gaps")
        cycles, unfinished = [], []
        for generation, pair in slots.get(tier, {}).items():
            begin = pair.get("start")
            if not begin or generation <= 0 or not start_utc_ns <= begin["server_utc_ns"] <= end_utc_ns:
                continue
            done = pair.get("done")
            if done is None:
                unfinished.append(generation)
                continue
            elapsed = done["server_utc_ns"] - begin["server_utc_ns"]
            if elapsed < 0:
                issues.append("Done timestamp precedes matching generation start")
                continue
            cycles.append({"generation": generation, "start": begin, "done": done, "elapsed_ns": elapsed})
        ordered = sorted(c["elapsed_ns"] for c in cycles)
        tiers[tier] = {"cycles": cycles, "observed_cycles": len(cycles), "unfinished_generations": unfinished,
            "period_exceedances": sum(n > period_ns for n in ordered),
            "p50_ms": statistics.median(ordered) / 1e6 if ordered else None,
            "p95_ms": ordered[max(0, (95 * len(ordered) + 99) // 100 - 1)] / 1e6 if ordered else None,
            "max_ms": max(ordered) / 1e6 if ordered else None, "busy_suppressed_due_ticks": None}
    return {"coverage": "Incomplete" if issues else "Observed", "issues": issues, "records": records,
            "start_utc_ns": start_utc_ns, "end_utc_ns": end_utc_ns,
            "tiers": tiers, "limits": ["Unchanged Busy-suppressed heads do not generate start observations",
              "Server timestamp precision is preserved as printed; no global no-overrun claim"]}
