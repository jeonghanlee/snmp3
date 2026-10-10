#!/usr/bin/env python3
"""Print the report tables of a device test run from its output directory.

Usage: summarize_results.py OUT_DIR

Reads phase0.json, phase1.json (and phase1b.json), kstar.json, phase2.json, phase3-*.json and
phase4.json when present, and prints Markdown tables per target label: pass time by group size with
the model fit, request in flight by group size, object order, and the sustained schedule.
"""

import json
import statistics as st
import sys
from pathlib import Path


def load(path):
    return json.loads(path.read_text()) if path.exists() else None


def points(rows):
    return [r for r in rows if "total_ms" in r and r.get("repeat") is not None]


def fmt(ms):
    if ms is None:
        return "-"
    return f"{ms / 1000:.2f} s" if ms >= 1000 else f"{ms:.0f} ms"


def group(rows, key):
    out = {}
    for k in sorted({key(r) for r in rows}):
        g = [r for r in rows if key(r) == k]
        t = [r["total_ms"] for r in g]
        out[k] = {"n": len(g), "pass": st.median(t), "lo": min(t), "hi": max(t),
                  "per_oid": st.median(r["per_oid_ms"] for r in g), "p50": st.median(r["p50_ms"] for r in g),
                  "p95": st.median(r["p95_ms"] for r in g), "max": max(r["max_ms"] for r in g),
                  "lost": sum(r["lost"] for r in g), "pdus": sum(r["pdus"] for r in g),
                  "oids": g[0]["oids"], "request_count": g[0]["pdus"]}
    return out


def fit(table):
    xs, ys = [], []
    for k, v in table.items():
        if k >= 2:
            xs.append(k)
            ys.append(v["pass"] / v["request_count"])
    if len(xs) < 3:
        return None
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    a = my - b * mx
    worst = max(abs(a + b * x - y) / y for x, y in zip(xs, ys))
    return a, b, worst * 100


def main():
    out = Path(sys.argv[1])
    phase0 = load(out / "phase0.json")
    if phase0:
        print("## Phase 0: idle probe (1 request per second)\n")
        for label, row in phase0.items():
            if isinstance(row, dict) and isinstance(row.get("probe"), dict):
                row = row["probe"]  # layout written by an earlier driver version
            if isinstance(row, dict) and "p50_ms" in row:
                print(f"- {label}: p50 {row['p50_ms']} ms, p95 {row['p95_ms']} ms, max {row['max_ms']} ms")
        print()
    kstar = load(out / "kstar.json")
    for name, title in (("phase1", "Phase 1: group size, one request in flight"),
                        ("phase1b", "Phase 1b: larger group sizes")):
        data = load(out / f"{name}.json")
        if not data:
            continue
        print(f"## {title}\n")
        if data["stopped"]:
            print("Stopped:", json.dumps(data["stopped"]), "\n")
        for label, rows in data["results"].items():
            table = group(points(rows), lambda r: r["k"])
            print(f"### {label}\n")
            print("| K | Requests | Pass (min to max) | Per object | Request p50 / p95 / max | Lost |")
            print("| --- | --- | --- | --- | --- | --- |")
            for k, v in table.items():
                print(f"| {k} | {v['request_count']} | {fmt(v['pass'])} ({fmt(v['lo'])} to {fmt(v['hi'])}) | "
                      f"{v['per_oid']:.2f} ms | {v['p50']:.0f} / {v['p95']:.0f} / {v['max']:.0f} ms | "
                      f"{v['lost']} of {v['pdus']} |")
            f = fit(table)
            if f:
                print(f"\nFit: request time = {f[0]:.1f} ms + {f[1]:.2f} ms * K (worst error {f[2]:.0f} percent)")
            if name == "phase1" and kstar and label in kstar:
                print(f"Rule result: K* = {kstar[label]['kstar']}, best K = {kstar[label]['best_k']}")
            print()
    ramp = load(out / "ramp.json")
    if ramp:
        print("## Outstanding requests ramp (single object per request)\n")
        if ramp["stopped"]:
            print("Stopped:", json.dumps(ramp["stopped"]), "\n")
        for label, rows in ramp["results"].items():
            table = group(points(rows), lambda r: r["window"])
            print(f"### {label}\n\n| In flight | Pass | Request p95 / max | Lost |\n| --- | --- | --- | --- |")
            for w, v in table.items():
                print(f"| {w} | {fmt(v['pass'])} | {v['p95']:.0f} / {v['max']:.0f} ms | {v['lost']} of {v['pdus']} |")
            print()
    phase2 = load(out / "phase2.json")
    if phase2:
        print("## Phase 2: requests in flight (pass time, request p95)\n")
        for label, rows in phase2["results"].items():
            table = group(points(rows), lambda r: (r["k"], r["window"]))
            print(f"### {label}\n\n| K | W=1 | W=2 | W=3 |\n| --- | --- | --- | --- |")
            for k in sorted({key[0] for key in table}):
                cells = [f"{fmt(table[(k, w)]['pass'])} (p95 {table[(k, w)]['p95']:.0f} ms)"
                         if (k, w) in table else "-" for w in (1, 2, 3)]
                print(f"| {k} | " + " | ".join(cells) + " |")
            print()
    orders = sorted(out.glob("phase3-*.json"))
    if orders:
        print("## Phase 3: object order (per object time at K*)\n")
        names = [p.stem.replace("phase3-", "") for p in orders]
        labels = list(json.loads(orders[0].read_text())["results"])
        print("| Order | " + " | ".join(labels) + " |\n| --- |" + " --- |" * len(labels))
        for p, n in zip(orders, names):
            d = json.loads(p.read_text())["results"]
            cells = []
            for label in labels:
                t = group(points(d[label]), lambda r: r["k"])
                cells.append(f"{list(t.values())[0]['per_oid']:.2f} ms" if t else "-")
            print(f"| {n} | " + " | ".join(cells) + " |")
        print()
    phase4 = load(out / "phase4.json")
    if phase4:
        print("## Phase 4: sustained schedule\n")
        for label, d in phase4.items():
            k = d.get("group_size", d.get("k"))
            idle, during = d["idle_probe"], d["probe_during"]
            tiers = [r for r in d["monitor"] if "tier" in r]
            busy = sum(t["cycles"] * t["cycle_p50_ms"] for t in tiers) / (d["monitor"][-1].get("duration_s", 900) * 1000)
            print(f"### {label} (K = {k})\n")
            print("| Tier | Objects | Period | Cycles | Overruns | Cycle p50 / p95 / max | Lost |")
            print("| --- | --- | --- | --- | --- | --- | --- |")
            for t in tiers:
                print(f"| {t['tier']} | {t['oids']} | {t['period_ms'] / 1000:.0f} s | {t['cycles']} | {t['overruns']} | "
                      f"{t['cycle_p50_ms']:.0f} / {t['cycle_p95_ms']:.0f} / {t['cycle_max_ms']:.0f} ms | "
                      f"{t['pdus_lost']} of {t['pdus_sent']} |")
            ratio = during["p95_ms"] / idle["p95_ms"] if idle["p95_ms"] else float("nan")
            print(f"\nSecond client p95: idle {idle['p95_ms']:.0f} ms, during {during['p95_ms']:.0f} ms, "
                  f"ratio {ratio:.2f}; unit busy about {busy * 100:.0f} percent (median cycle times)\n")


main()
