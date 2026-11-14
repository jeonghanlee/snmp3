#!/usr/bin/env python3
"""Search the tracked files for a snmp3Load call that no snmp3WorkerPath call precedes in the same script or example.

A call is the text snmp3Load( followed by a quote. In a Markdown file the example is the fenced block that holds the
call; in any other file a snmp3WorkerPath call earlier in the file precedes the call. The search reads static text only:
several scripts in one code file are not told apart, and the scripts that tests generate at run time are exercised by
the test cells themselves.
usage: check_worker_path_order.py [--root DIR] [FILE ...]
Exit codes: 0 no offender, 1 at least one offender, 2 usage or read error or no snmp3Load call found at all (a search
that sees no call has looked at the wrong files).
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

CALL = re.compile(r"snmp3Load\(\s*[\"']")
WORKER = re.compile(r"snmp3WorkerPath\(")
SKIP = {"docs/milestone-1.0.0.md"}


def calls(path):
    return sum(1 for line in path.read_text(errors="replace").splitlines() if CALL.search(line))


def offenders(path):
    lines = path.read_text(errors="replace").splitlines()
    markdown = path.suffix == ".md"
    found = []
    start = 0
    fenced = False
    for number, line in enumerate(lines):
        if markdown and line.lstrip().startswith("```"):
            fenced = not fenced
            start = number + 1
            continue
        if CALL.search(line):
            begin = start if markdown else 0
            if not any(WORKER.search(previous) for previous in lines[begin:number]):
                found.append((number + 1, line.strip()))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent.parent)
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()
    try:
        names = args.files or subprocess.run(["git", "-C", str(args.root), "ls-files"], check=True, capture_output=True,
                                             text=True).stdout.split("\n")
    except (OSError, subprocess.CalledProcessError) as error:
        print("cannot list the files: %s" % error, file=sys.stderr)
        return 2
    count = 0
    seen = 0
    for name in sorted(n for n in names if n and n not in SKIP):
        path = args.root / name
        if not path.is_file():
            continue
        try:
            seen += calls(path)
            for number, text in offenders(path):
                print("%s:%d: snmp3Load without snmp3WorkerPath before it: %s" % (name, number, text[:100]))
                count += 1
        except OSError as error:
            print("cannot read %s: %s" % (name, error), file=sys.stderr)
            return 2
    print("calls checked: %d, offenders: %d" % (seen, count))
    if not seen:
        print("no snmp3Load call found: the file list is wrong", file=sys.stderr)
        return 2
    return 1 if count else 0


sys.exit(main())
