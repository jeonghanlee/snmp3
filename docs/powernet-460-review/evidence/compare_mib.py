#!/usr/bin/env python3
"""Compare the objects of a monitoring set between two PowerNet-MIB files.

Self-contained: it parses both MIB files itself and takes the monitoring set as a TSV with a
header (oid, mib_object, tier, value_type), for example
docs/snmp-performance/inputs/ap8932-oids.tsv.

For every distinct mib_object it compares the numeric OID, SYNTAX, ACCESS, STATUS, UNITS,
DESCRIPTION, the INDEX clause of the parent entry and a hash of the whitespace-normalized body.
It checks that every instance OID of the set lies under its object's OID in both files, and lists
the rPDU2 objects added, removed or changed between the files.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path

ENTERPRISES = (1, 3, 6, 1, 4, 1)


def norm(text):
    return " ".join(text.replace("\r", "").split())


def parse(path):
    text = path.read_text(errors="replace").replace("\r", "")
    items, names = {}, []
    pattern = (r'^[ \t]*([A-Za-z][\w-]*)[ \t]+OBJECT-TYPE\b(.*?)::=[ \t]*'
               r'\{[ \t]*([\w-]+)[ \t]+(\d+)[ \t]*\}')
    for m in re.finditer(pattern, text, re.M | re.S):
        names.append(m.group(1))
        items[m.group(1)] = (norm(m.group(2)), m.group(3), int(m.group(4)))
    for m in re.finditer(r'^[ \t]*([A-Za-z][\w-]*)[ \t]+OBJECT IDENTIFIER[ \t]*::=[ \t]*'
                         r'\{[ \t]*([\w-]+)[ \t]+(\d+)[ \t]*\}', text, re.M):
        items.setdefault(m.group(1), ("", m.group(2), int(m.group(3))))
    duplicates = sorted({n for n in names if names.count(n) > 1})
    return items, duplicates


class Mib:
    def __init__(self, path):
        self.path = path
        self.items, self.duplicates = parse(path)
        self.cache = {"enterprises": ENTERPRISES}

    def oid(self, name):
        if name not in self.cache:
            _, parent, arc = self.items[name]
            self.cache[name] = self.oid(parent) + (arc,)
        return self.cache[name]

    def facts(self, name):
        body, parent, _ = self.items[name]

        def clause(key, stop):
            m = re.search(key + r'\s+(.*?)\s+(?:' + stop + r')\b', body)
            return m.group(1) if m else None
        entry = self.items.get(parent)
        index = re.search(r'INDEX\s*\{(.*?)\}', entry[0]) if entry else None
        description = re.search(r'DESCRIPTION\s+"(.*?)"', body)
        return {
            "oid": ".".join(map(str, self.oid(name))),
            "syntax": clause("SYNTAX", "ACCESS"),
            "access": clause("ACCESS", "STATUS"),
            "status": clause("STATUS", "DESCRIPTION"),
            "units": clause("UNITS", "ACCESS|STATUS|DESCRIPTION|SYNTAX") if "UNITS" in body else None,
            "description": norm(description.group(1)) if description else None,
            "entry": parent,
            "entry_index": norm(index.group(1)) if index else None,
            "body_sha": hashlib.sha256(body.encode()).hexdigest()[:16],
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old", type=Path, required=True, help="reference MIB file")
    parser.add_argument("--new", type=Path, required=True, help="MIB file under review")
    parser.add_argument("--objects", type=Path, required=True, help="monitoring set TSV")
    parser.add_argument("--output", type=Path, required=True, help="result JSON to write")
    args = parser.parse_args()
    old, new = Mib(args.old), Mib(args.new)
    rows = [line.split("\t") for line in args.objects.read_text().splitlines()[1:] if line.strip()]
    selected = sorted({r[1] for r in rows})
    fields = ("oid", "syntax", "access", "status", "units", "description", "entry", "entry_index", "body_sha")
    report = {
        "old": {"file": args.old.name, "sha256": hashlib.sha256(args.old.read_bytes()).hexdigest()},
        "new": {"file": args.new.name, "sha256": hashlib.sha256(args.new.read_bytes()).hexdigest()},
        "set": {"file": args.objects.name, "instances": len(rows), "distinct_objects": len(selected)},
        "duplicate_names_in_selection": {
            "old": sorted(set(old.duplicates) & set(selected)), "new": sorted(set(new.duplicates) & set(selected))},
        "missing_in_old": [n for n in selected if n not in old.items],
        "missing_in_new": [n for n in selected if n not in new.items],
        "changed": [], "identical": 0, "instances_outside_object_oid": [],
    }
    for name in selected:
        if name not in old.items or name not in new.items:
            continue
        a, b = old.facts(name), new.facts(name)
        diff = {f: {"old": a[f], "new": b[f]} for f in fields if a[f] != b[f]}
        if diff:
            report["changed"].append({"object": name, "diff": diff})
        else:
            report["identical"] += 1
    for oid, name, _, _ in rows:
        for label, mib in (("old", old), ("new", new)):
            if name in mib.items and not oid.startswith(".".join(map(str, mib.oid(name))) + "."):
                report["instances_outside_object_oid"].append({"mib": label, "object": name, "oid": oid})
    r_old = {n for n in old.items if n.startswith("rPDU2")}
    r_new = {n for n in new.items if n.startswith("rPDU2")}
    report["rPDU2_family"] = {
        "old": len(r_old), "new": len(r_new), "added": sorted(r_new - r_old), "removed": sorted(r_old - r_new),
        "body_changed": sorted(n for n in r_old & r_new
                               if old.items[n][0] != new.items[n][0] or old.oid(n) != new.oid(n)),
    }
    args.output.write_text(json.dumps(report, indent=1) + "\n")
    family = report["rPDU2_family"]
    print(f"objects {report['set']['distinct_objects']} instances {report['set']['instances']} "
          f"identical {report['identical']} changed {len(report['changed'])} "
          f"missing_new {len(report['missing_in_new'])}")
    print(f"rPDU2 added {len(family['added'])} removed {len(family['removed'])} "
          f"body_changed {len(family['body_changed'])}")


main()
