#!/usr/bin/env python3
"""Run the SNMP device performance test phases (see docs/snmp-performance/test-procedure.md).

All traffic is SNMP GET. Targets, credentials paths and the OID list are inputs; nothing about a
particular device is stored in this file.

targets file (JSON):
  {"targets": [{"label": "unit-a", "address": "192.0.2.10", "port": 161, "user": "u",
                "authAlgorithm": "SHA", "privAlgorithm": "AES",
                "authSecretFile": "/abs/auth", "privSecretFile": "/abs/priv"}]}

OID list (TSV with header): oid, mib_object, tier (state|meas|slow), value_type (integer|octets)
"""

import argparse
import json
import random
import statistics as st
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_KS = "2,4,8,12,16,20,24,28,32,40,48"
PROBE_OID = "1.3.6.1.2.1.1.3.0"


def load_targets(path):
    targets = json.loads(Path(path).read_text())["targets"]
    for t in targets:
        t["peer"] = f"udp:{t['address']}:{t.get('port', 161)}"
    return targets


def load_oids(path):
    rows = []
    for line in Path(path).read_text().splitlines()[1:]:
        if line.strip():
            oid, mib, tier, vtype = line.split("\t")
            rows.append({"oid": oid, "mib": mib, "tier": tier, "type": vtype})
    return rows


def key(oid):
    return tuple(int(x) for x in oid.split("."))


def write_oid_files(out, rows):
    oids = [r["oid"] for r in rows]
    sets = {
        "inv": oids,
        "bycol": sorted(oids, key=key),
        "byrow": sorted(oids, key=lambda o: (key(o)[-1], key(o))),
        "nostr": [r["oid"] for r in rows if r["type"] == "integer"],
    }
    shuffled = list(oids)
    random.Random(7).shuffle(shuffled)
    sets["rand"] = shuffled
    paths = {}
    for name, lst in sets.items():
        paths[name] = out / f"oids-{name}.txt"
        paths[name].write_text("\n".join(lst) + "\n")
    paths["probe"] = out / "oids-probe.txt"
    paths["probe"].write_text(PROBE_OID + "\n")
    for tier in ("state", "meas", "slow"):
        paths[tier] = out / f"tier-{tier}.txt"
        paths[tier].write_text("\n".join(r["oid"] for r in rows if r["tier"] == tier) + "\n")
    return paths


def sweep_argv(tools, mode, target, oidfile, repeats, combos, timeout_ms):
    return [str(tools / "device_sweep"), mode, target["peer"], target["user"], target["authAlgorithm"],
            target["privAlgorithm"], target["authSecretFile"], target["privSecretFile"], str(oidfile),
            str(timeout_ms), str(repeats), combos]


def parse_lines(text):
    rows, repeat = [], None
    for line in text.splitlines():
        try:
            row = json.loads(line)
        except ValueError:
            continue
        if "repeat" in row and len(row) == 1:
            repeat = row["repeat"]
        else:
            row["repeat"] = repeat
            rows.append(row)
    return rows


def interleaved(args, targets, oidfile, combos, name):
    results = {t["label"]: [] for t in targets}
    stopped = {}
    for run in range(args.repeats):
        for index, t in enumerate(targets):
            if index:
                time.sleep(10)  # rest between devices (rule 4 of the procedure)
            label = t["label"]
            if label in stopped:
                continue
            c = combos[label] if isinstance(combos, dict) else combos
            done = subprocess.run(sweep_argv(args.tools, "sweep", t, oidfile, 1, c, args.timeout_ms),
                                  capture_output=True, text=True, timeout=7200)
            rows = parse_lines(done.stdout)
            for r in rows:
                r["run"] = run
            results[label] += rows
            if done.returncode:
                stopped[label] = {"run": run, "returncode": done.returncode}
            print(f"{name} run {run} {label} rc {done.returncode} rows {len(rows)}", flush=True)
    (args.out / f"{args.tag if args.tag and name == 'phase1' else name}.json").write_text(
        json.dumps({"results": results, "stopped": stopped}, indent=1) + "\n")
    return results, stopped


def table(results):
    out = {}
    for label, rows in results.items():
        by = {}
        for r in rows:
            if "total_ms" in r and r.get("repeat") is not None:
                by.setdefault((r["k"], r["window"]), []).append(r)
        out[label] = {k: {"per_oid_ms": st.median(x["per_oid_ms"] for x in v),
                          "pass_ms": st.median(x["total_ms"] for x in v),
                          "p95_ms": st.median(x["p95_ms"] for x in v)} for k, v in sorted(by.items())}
    return out


def choose_k(per_k, p95_limit_ms=1000):
    """Smallest K within 10 percent of the best per-OID time whose PDU p95 stays below the limit."""
    ks = {k: v for (k, w), v in per_k.items() if w == 1}
    if not ks:
        return None, None, None
    best = min(v["per_oid_ms"] for v in ks.values())
    best_k = min(ks, key=lambda k: ks[k]["per_oid_ms"])
    for k in sorted(ks):
        if ks[k]["per_oid_ms"] <= 1.10 * best and ks[k]["p95_ms"] <= p95_limit_ms:
            return k, best_k, best
    return None, best_k, best


def phase4(args, targets, paths):
    report = {}
    for t in targets:
        label = t["label"]
        k = t.get("group_size") or args.group_size
        common = [t["peer"], t["user"], t["authAlgorithm"], t["privAlgorithm"], t["authSecretFile"],
                  t["privSecretFile"]]
        idle = subprocess.run([str(args.tools / "device_sweep"), "probefor", *common, str(paths["probe"]),
                               str(args.timeout_ms), "1", "60"], capture_output=True, text=True, timeout=300)
        idle_summary = parse_lines(idle.stdout)[-1]
        time.sleep(5)
        monitor = subprocess.Popen([str(args.tools / "device_monitor"), *common, str(args.timeout_ms),
                                    str(args.duration), str(k), str(paths["state"]), str(args.state_ms),
                                    str(paths["meas"]), str(args.meas_ms), str(paths["slow"]), str(args.slow_ms)],
                                   stdout=subprocess.PIPE, text=True)
        time.sleep(3)
        probe = subprocess.run([str(args.tools / "device_sweep"), "probefor", *common, str(paths["probe"]),
                                str(args.timeout_ms), "1", str(args.duration - 10)], capture_output=True,
                               text=True, timeout=args.duration + 300)
        out, _ = monitor.communicate(timeout=300)
        report[label] = {"group_size": k, "idle_probe": idle_summary, "probe_during": parse_lines(probe.stdout)[-1],
                         "monitor": parse_lines(out), "monitor_returncode": monitor.returncode}
        print(label, "phase4 done", flush=True)
        time.sleep(10)
    (args.out / "phase4.json").write_text(json.dumps(report, indent=1) + "\n")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("phase", choices=("phase0", "phase1", "phase2", "phase3", "phase4"))
    p.add_argument("--targets", required=True)
    p.add_argument("--oids", required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--tools", type=Path, default=HERE / "bin")
    p.add_argument("--repeats", type=int, default=5)
    p.add_argument("--ks", default=DEFAULT_KS)
    p.add_argument("--timeout-ms", type=int, default=3000)
    p.add_argument("--tag", help="output name instead of the phase name (use phase1b for a larger-K sweep)")
    p.add_argument("--kstar", type=Path, help="JSON {label: {kstar, best_k}} from phase 1 (phases 2 and 3)")
    p.add_argument("--group-size", type=int, default=28, help="phase 4 default K per target")
    p.add_argument("--duration", type=int, default=900)
    p.add_argument("--state-ms", type=int, default=5000)
    p.add_argument("--meas-ms", type=int, default=10000)
    p.add_argument("--slow-ms", type=int, default=60000)
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    targets = load_targets(args.targets)
    paths = write_oid_files(args.out, load_oids(args.oids))
    if args.phase == "phase0":
        report = {}
        for t in targets:
            done = subprocess.run(sweep_argv(args.tools, "probe", t, paths["probe"], 1, "30", args.timeout_ms),
                                  capture_output=True, text=True, timeout=300)
            report[t["label"]] = parse_lines(done.stdout)[-1]
            print(t["label"], report[t["label"]], flush=True)
        (args.out / "phase0.json").write_text(json.dumps(report, indent=1) + "\n")
    elif args.phase == "phase1":
        results, stopped = interleaved(args, targets, paths["inv"], args.ks, "phase1")
        tab = table(results)
        kstar = {}
        for label, per_k in tab.items():
            if per_k:
                k, best_k, best = choose_k(per_k)
                if best_k is not None:
                    kstar[label] = {"kstar": k, "best_k": best_k, "best_per_oid_ms": best}
        kname = "kstar.json" if not args.tag else f"kstar-{args.tag}.json"
        (args.out / kname).write_text(json.dumps(kstar, indent=1) + "\n")
        print(json.dumps(kstar), json.dumps(stopped), flush=True)
    elif args.phase == "phase2":
        kstar = json.loads(args.kstar.read_text())
        combos = {}
        for t in targets:
            k = kstar[t["label"]]["kstar"] or kstar[t["label"]]["best_k"]
            ks = sorted({max(1, k // 2), k, kstar[t["label"]]["best_k"]})
            combos[t["label"]] = ",".join(f"{kk}x{w}" for kk in ks for w in (1, 2, 3))
        interleaved(args, targets, paths["inv"], combos, "phase2")
    elif args.phase == "phase3":
        kstar = json.loads(args.kstar.read_text())
        for name in ("inv", "bycol", "byrow", "rand", "nostr"):
            combos = {t["label"]: str(kstar[t["label"]]["kstar"] or kstar[t["label"]]["best_k"]) for t in targets}
            interleaved(args, targets, paths[name], combos, f"phase3-{name}")
    else:
        phase4(args, targets, paths)


main()
