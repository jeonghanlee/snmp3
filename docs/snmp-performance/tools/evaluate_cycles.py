#!/usr/bin/env python3
"""Evaluate retained polling generations without importing an equipment runner."""

import argparse
import json
from pathlib import Path

from native_observation import digest
from poll_cycles import cycle_summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--window", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Preserve existing evaluation results")
    try:
        if digest(args.input) != args.sha256:
            raise ValueError("Monitor input digest differs from the supplied proof")
        window_sha = digest(args.window)
        window = json.loads(args.window.read_text())
        start, end = window["start_utc_ns"], window["end_utc_ns"]
        if not isinstance(start, int) or not isinstance(end, int) or start < 0 or end <= start:
            raise ValueError("An increasing nonnegative UTC window is required")
        result = cycle_summary(args.input, start, end)
        if digest(args.input) != args.sha256 or digest(args.window) != window_sha:
            raise ValueError("Evaluation input changed during evaluation")
        if any(not tier["observed_cycles"] for tier in result["tiers"].values()):
            result["coverage"] = "Incomplete"
            result["issues"].append("Required polling tier has no observed cycle")
        result["window_sha256"] = window_sha
    except (OSError, ValueError, KeyError, TypeError) as error:
        result = {"coverage": "Incomplete", "issues": [str(error)]}
    result.update(input_sha256=args.sha256,
                  tool_sha256=digest(Path(__file__).with_name("poll_cycles.py")),
                  evaluator_sha256=digest(Path(__file__)))
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0 if result["coverage"] == "Observed" else 2


if __name__ == "__main__":
    raise SystemExit(main())
