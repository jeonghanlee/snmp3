#!/usr/bin/env python3
"""Measure the actual snmp3 IOC, worker and native path: N longin records admitted together."""

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fixture_harness import Bench, PRODUCTS, ROOT, USER, free_port  # noqa: E402

BASE = Path(os.environ["EPICS_BASE"])
CA = BASE / "bin" / "linux-x86_64"
STAMP = "%Y-%m-%d %H:%M:%S.%f"


def database(count, deadline_ms):
    """N longin records behind a two-level fanout tree triggered by one PROC write."""
    lines = []
    groups = [list(range(g * 16, min(count, (g + 1) * 16))) for g in range((count + 15) // 16)]
    links = "0123456789ABCDEF"
    lines.append('record(fanout, "B_Trig") {\n    field(SELM, "All")')
    for g in range(len(groups)):
        lines.append(f'    field(LNK{links[g]}, "B_F{g}")')
    lines.append("}")
    for g, members in enumerate(groups):
        lines.append(f'record(fanout, "B_F{g}") {{\n    field(SELM, "All")')
        for k, i in enumerate(members):
            lines.append(f'    field(LNK{links[k]}, "B_R{i:03d}")')
        lines.append("}")
    for i in range(count):
        lines.append(f'record(longin, "B_R{i:03d}") {{\n    field(DTYP, "snmp3")\n'
                     f'    field(INP, "@binding=Read deadline_ms={deadline_ms}")\n'
                     '    field(MDEL, "-1")\n}')
    return "\n".join(lines) + "\n"


def run_case(bench, agent_port, version, delay, count, sweeps, deadline_ms, label):
    case = bench.output / label
    case.mkdir()
    port, proxy = bench.proxy(agent_port, delay)
    profile = {"id": "P", "version": version, "timeoutMs": 5000, "retries": 0}
    if version == "2c":
        profile["communityFile"] = str(bench.paths["community"])
    else:
        profile.update(user=USER, securityLevel="authPriv", authAlgorithm="SHA",
                       authSecretFile=str(bench.paths["auth"]), privAlgorithm="AES",
                       privSecretFile=str(bench.paths["priv"]))
    config = case / "config.json"
    config.write_text(json.dumps({"schema": 1, "profiles": [profile],
        "endpoints": [{"id": "E", "address": "127.0.0.1", "port": port, "profile": "P"}],
        "bindings": [{"id": "Read", "endpoint": "E", "operation": "get", "valueType": "integer",
                      "oid": "1.3.6.1.4.1.53864.4.1.0"}]}))
    (case / "records.db").write_text(database(count, deadline_ms))
    (case / "startup.cmd").write_text(
        'on error break\n'
        f'dbLoadDatabase("{ROOT}/dbd/snmp3Ioc.dbd")\n'
        'snmp3Ioc_registerRecordDeviceDriver(pdbbase)\n'
        f'snmp3Load("{config}","{PRODUCTS}/snmp3NativeProbe")\n'
        f'snmp3WorkerPath("{PRODUCTS}/snmp3Worker")\n'
        f'dbLoadRecords("{case}/records.db")\n'
        'iocInit\n')
    server, repeater_port = free_port(), free_port()
    env = dict(os.environ, EPICS_CA_SERVER_PORT=str(server), EPICS_CA_REPEATER_PORT=str(repeater_port),
               EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST=f"127.0.0.1:{server}",
               EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
               EPICS_CAS_BEACON_ADDR_LIST=f"127.0.0.1:{repeater_port}")
    repeater = subprocess.Popen([str(CA / "caRepeater")], env=env, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
    ioc_out = (case / "ioc.stdout").open("wb")
    ioc_err = (case / "ioc.stderr").open("wb")
    ioc = subprocess.Popen([str(PRODUCTS / "snmp3Ioc"), str(case / "startup.cmd")], env=env,
                           stdin=subprocess.PIPE, stdout=ioc_out, stderr=ioc_err)
    names = [f"B_R{i:03d}" for i in range(count)]
    result = {"label": label, "version": version, "delay_ms": delay, "n": count, "sweeps": []}
    try:
        end = time.monotonic() + 30
        while time.monotonic() < end and ioc.poll() is None:
            if b"iocRun: All initialization complete" in (case / "ioc.stderr").read_bytes():
                break
            time.sleep(0.05)
        if ioc.poll() is not None:
            raise RuntimeError("IOC exited during startup")
        mon_out = (case / "monitor.out").open("wb")
        monitor = subprocess.Popen([str(CA / "camonitor"), "-w", "5", *names], env=env, stdout=mon_out,
                                   stderr=subprocess.DEVNULL)
        time.sleep(2.0)
        seen = 0
        for sweep in range(sweeps):
            baseline = (case / "monitor.out").read_text().count("\n")
            trigger = datetime.now()
            subprocess.run([str(CA / "caput"), "-w", "5", "B_Trig.PROC", "1"], env=env,
                           stdout=subprocess.DEVNULL, check=True)
            end = time.monotonic() + max(60.0, count * (delay + 30) / 1000.0 * 3)
            while time.monotonic() < end:
                if (case / "monitor.out").read_text().count("\n") - baseline >= count:
                    break
                time.sleep(0.05)
            time.sleep(0.3)
            lines = (case / "monitor.out").read_text().splitlines()[baseline:]
            stamps, alarms = [], 0
            for line in lines:
                parts = line.split()
                if len(parts) < 4:
                    continue
                stamps.append(datetime.strptime(parts[1] + " " + parts[2], STAMP))
                if len(parts) > 4:
                    alarms += 1
            stamps.sort()
            if len(stamps) >= 2:
                span = (stamps[-1] - stamps[0]).total_seconds() * 1000
                total = (stamps[-1] - trigger).total_seconds() * 1000
                first = (stamps[0] - trigger).total_seconds() * 1000
            else:
                span = total = first = None
            result["sweeps"].append({"updates": len(stamps), "alarmed": alarms, "first_ms": first,
                                     "span_ms": span, "total_ms": total,
                                     "interval_ms": span / (len(stamps) - 1) if span and len(stamps) > 1 else None})
            time.sleep(1.0)
        monitor.terminate()
        monitor.wait(timeout=5)
        mon_out.close()
        ioc.stdin.write(b"snmp3RuntimeReport\n")
        ioc.stdin.flush()
        time.sleep(0.5)
    finally:
        try:
            ioc.stdin.write(b"exit\n")
            ioc.stdin.flush()
            ioc.stdin.close()
        except (BrokenPipeError, ValueError, OSError):
            pass
        try:
            ioc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            ioc.kill()
            ioc.wait()
        ioc_out.close()
        ioc_err.close()
        repeater.terminate()
        repeater.wait(timeout=5)
        bench.stop(proxy)
    result["ioc_returncode"] = ioc.returncode
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--n", type=int, default=100)
    parser.add_argument("--delays", default="0,20,100")
    parser.add_argument("--versions", default="2c,3")
    parser.add_argument("--sweeps", type=int, default=3)
    parser.add_argument("--deadline-ms", type=int, default=60000)
    args = parser.parse_args()
    bench = Bench(args.output)
    results = []
    try:
        agent_port = bench.agent()
        for version in args.versions.split(","):
            for delay in [int(d) for d in args.delays.split(",")]:
                label = f"v{version}-d{delay}"
                row = run_case(bench, agent_port, version, delay, args.n, args.sweeps, args.deadline_ms, label)
                results.append(row)
                print(json.dumps(row), flush=True)
    finally:
        bench.close()
    (args.output / "snmp3-results.json").write_text(json.dumps(results, indent=1) + "\n")


main()
