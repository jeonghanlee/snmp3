#!/usr/bin/env python3
"""Evaluate a completed native-event capture with an independent input digest."""

import argparse
import json
from pathlib import Path

from native_observation import CALLBACK_HEADER, digest, native_events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-log", type=Path, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--start-us", type=int, required=True)
    parser.add_argument("--end-us", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--callback-header", type=Path, default=CALLBACK_HEADER)
    parser.add_argument("--callback-header-sha256")
    args = parser.parse_args()
    if args.output.exists() or args.start_us < 0 or args.end_us <= args.start_us:
        parser.error("A new output and an increasing nonnegative monotonic window are required")
    try:
        if digest(args.native_log) != args.sha256:
            raise ValueError("Native-event input digest differs from the supplied proof")
        if args.callback_header_sha256 and digest(args.callback_header) != args.callback_header_sha256:
            raise ValueError("Callback header digest differs from the supplied proof")
        header_sha = digest(args.callback_header)
        result = native_events(args.native_log, args.start_us, args.end_us, args.callback_header)
        if digest(args.callback_header) != header_sha:
            raise ValueError("Callback header changed during evaluation")
        if digest(args.native_log) != args.sha256:
            raise ValueError("Native-event capture changed during evaluation")
    except (OSError, ValueError, KeyError, IndexError) as error:
        result = {"coverage": "INCOMPLETE", "issues": [str(error)], "timeout_percent": None}
    result["input_sha256"] = args.sha256
    result["evaluator_sha256"] = digest(Path(__file__))
    result["observation_module_sha256"] = digest(Path(__file__).with_name("native_observation.py"))
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0 if result["coverage"] == "COMPLETE" else 2


if __name__ == "__main__":
    raise SystemExit(main())
