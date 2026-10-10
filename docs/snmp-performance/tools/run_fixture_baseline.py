#!/usr/bin/env python3
"""Run the Net-SNMP direct baseline against the fixture agent through a UDP delay proxy."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fixture_harness import Bench, USER  # noqa: E402


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--n", type=int, default=100)
    parser.add_argument("--delays", default="0,20,100")
    parser.add_argument("--versions", default="2c,3")
    parser.add_argument("--repeat", type=int, default=3)
    args = parser.parse_args()
    bench = Bench(args.output)
    results = []
    try:
        agent_port = bench.agent()
        for delay in [int(d) for d in args.delays.split(",")]:
            port, proxy = bench.proxy(agent_port, delay)
            for version in args.versions.split(","):
                for mode in ("seq", "conc", "multi"):
                    for run in range(args.repeat):
                        argv = [str(args.binary), f"udp:127.0.0.1:{port}", version, str(args.n), mode,
                                str(bench.paths["community"]), str(bench.paths["auth"]),
                                str(bench.paths["priv"]), USER]
                        done = subprocess.run(argv, capture_output=True, text=True, timeout=300)
                        try:
                            row = json.loads(done.stdout.strip().splitlines()[-1])
                        except (ValueError, IndexError):
                            row = {"error": "no result", "stderr": done.stderr[-300:],
                                   "returncode": done.returncode}
                        row.update(delay_ms=delay, run=run, returncode=done.returncode)
                        results.append(row)
                        print(json.dumps(row), flush=True)
            bench.stop(proxy)
    finally:
        bench.close()
    (args.output / "baseline-results.json").write_text(json.dumps(results, indent=1) + "\n")


main()
