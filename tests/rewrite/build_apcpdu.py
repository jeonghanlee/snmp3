#!/usr/bin/env python3
"""Build a local AP8932 candidate from pinned, committed APC source files."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
ARCH = "linux-x86_64"
APC_COMMIT = "54345c73c9f2c7fb8210a8b39e593564f61271b5"
PREFIX = "APCTEST:PDU:"
APP = "snmp3ApcIoc"
DEADLINE_MS = 15000
TEMPLATES = ("pdu_scan", "pdu_device_info", "pdu_load_rpdu2g", "pdu_limits",
             "pdu_pva_status_24")
SUBSTITUTIONS = ("pdu_outlets_24_read", "pdu_banks_2", "pdu_outlets_24_write",
                 "pdu_outlets_24_group")
RECORD = re.compile(r'^record\((\w+),\s*"([^"]+)"\)\s*\{.*?^\}', re.M | re.S)
FIELD = re.compile(r'^\s*field\((\w+),\s*"([^"]*)"\)', re.M)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def load_peer(path):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location("apcpdu_peer", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def set_field(block, name, value):
    pattern = re.compile(r'^\s*field\(' + name + r',\s*"[^"]*"\)', re.M)
    line = f'    field({name}, "{value}")'
    if pattern.search(block):
        return pattern.sub(lambda _: "\n" + line, block)
    return block[:-1] + line + "\n}"


def migrate(source, resolve):
    """Transform expanded real records while preserving public identities."""
    records = list(RECORD.finditer(source))
    public = {m[2]: m[1] for m in records}
    scaled = {m[2]: m[2] + "Raw_" for m in records
              if dict(FIELD.findall(m[0])).get("LINR") == "SLOPE"}
    if any(raw in public or name + "Scale_" in public for name, raw in scaled.items()):
        raise ValueError("Private conversion name collision")
    bindings, conversions, private = [], [], []

    def convert(match):
        kind, name, block = match[1], match[2], match[0]
        fields = dict(FIELD.findall(block))
        # Move processing links only; value/limit references still use public ai.
        for field, value in fields.items():
            if field == "FLNK" or re.fullmatch(r"LNK[0-9A-F]", field):
                target = value.split(" ", 1)[0]
                if target in scaled:
                    block = set_field(block, field, value.replace(target, scaled[target], 1))
        outlet = re.fullmatch(re.escape(PREFIX) + r"Outlet(\d+)InfoUpdateFout_", name)
        if outlet:
            block = set_field(block, "LNK3", "")
        if fields.get("DTYP") != "Snmp":
            return block
        link = "OUT" if kind == "longout" else "INP"
        if kind not in ("ai", "longin", "longout", "stringin"):
            raise ValueError(f"Unsupported legacy record: {name}")
        oid = re.search(r"PowerNet-MIB::([\w-]+)\.([0-9.]+)", fields[link])
        if not oid:
            raise ValueError(f"Unresolved OID: {name}")
        numbers = resolve(oid[1]) + tuple(map(int, oid[2].split(".")))
        binding = {"id": f"B{len(bindings) + 1}", "endpoint": "Local",
                   "oid": ".".join(map(str, numbers)),
                   "operation": "set" if link == "OUT" else "get",
                   "valueType": "octets" if kind == "stringin" else "integer",
                   "capacity": 40 if kind == "stringin" else 1}
        bindings.append(binding)
        inst = f'@binding={binding["id"]} deadline_ms={DEADLINE_MS}'
        if name in scaled:
            raw, calc = scaled[name], name + "Scale_"
            slope, offset = fields["ESLO"], fields.get("EOFF", "0")
            float(slope), float(offset)
            conversions.append({"public": name, "raw": raw, "calc": calc,
                                "slope": float(slope), "offset": float(offset)})
            private.append(f'''record(ai, "{raw}") {{
    field(DESC, "Unscaled SNMP input")
    field(DTYP, "snmp3")
    field(INP, "{inst}")
    field(LINR, "NO CONVERSION")
    field(SCAN, "{fields.get('SCAN', 'Passive')}")
    field(PINI, "{fields.get('PINI', 'NO')}")
    field(FLNK, "{calc}")
}}
record(calc, "{calc}") {{
    field(DESC, "SNMP engineering units")
    field(INPA, "{raw}.VAL NPP MSS")
    field(CALC, "A*({slope})+({offset})")
    field(FLNK, "{name}")
}}''')
            for field, value in {"DTYP": "Soft Channel", "INP": calc + ".VAL NPP MSS",
                                 "LINR": "NO CONVERSION", "SCAN": "Passive", "PINI": "NO"}.items():
                block = set_field(block, field, value)
        else:
            block = set_field(set_field(block, "DTYP", "snmp3"), link, inst)
        outlet = re.fullmatch(re.escape(PREFIX) + r"OutletStatus(\d+)_", name)
        if outlet:
            block = set_field(block, "FLNK", PREFIX + "OutletStatus" + outlet[1])
        return block

    result = RECORD.sub(convert, source) + "\n\n" + "\n\n".join(private) + "\n"
    result = result.replace("This input shares the command OID cache with the SNMP output record.",
                            "This input reads the command OID independently of the output record.")
    result = result.replace("Summarize alarms after measurement and information readbacks finish.",
                            "Summarize currently available alarms on the information scan cadence.")
    if '"Snmp"' in result or "PowerNet-MIB::" in result or "$(" in result:
        raise ValueError("Migration left unresolved device support or macros")
    actual = {m[2]: m[1] for m in RECORD.finditer(result)}
    if any(actual.get(name) != kind for name, kind in public.items()):
        raise ValueError("Public record identity changed")
    return result, bindings, conversions, public


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("apcpdu", "base", "pvxs", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    apc, base, pvxs, output = (getattr(args, n).resolve() for n in ("apcpdu", "base", "pvxs", "output"))
    for path in (ROOT, apc, base, pvxs, output):
        if not re.fullmatch(r"[/\w.-]+", str(path), re.ASCII):
            parser.error("EPICS build paths must contain only ASCII letters, digits, _, ., -, /")
    if not output.is_relative_to(ROOT / "work") or output.exists():
        parser.error("--output must be a new directory under this checkout's work/")
    required = [base / "bin" / ARCH / n for n in ("msi", "caget", "caput", "caRepeater")]
    required += [pvxs / "bin" / ARCH / n for n in ("pvxget", "pvxput")]
    required += [ROOT / "bin" / ARCH / n for n in ("snmp3Worker", "snmp3NativeProbe")]
    required += [ROOT / "lib" / ARCH / "libsnmp3.so", pvxs / "lib" / ARCH / "libpvxsIoc.so"]
    if any(not path.is_file() for path in required):
        parser.error("Required installed EPICS, PVXS or snmp3 product is missing")
    if '"EPICS 7.0.10' not in (base / "include/epicsVersion.h").read_text():
        parser.error("EPICS Base 7.0.10 is required")
    output.mkdir(parents=True)
    commands = []

    def run(argv, cwd=output):
        result = subprocess.run(list(map(str, argv)), cwd=cwd, text=True, capture_output=True)
        commands.append({"argv": list(map(str, argv)), "cwd": str(cwd), "returncode": result.returncode})
        (output / f"command-{len(commands):02}.out").write_text(result.stdout)
        (output / f"command-{len(commands):02}.err").write_text(result.stderr)
        write_json(output / "commands.json", commands)
        result.check_returncode()
        return result.stdout

    source = output / "source"
    files = [f"apcpduApp/Db/{n}.template" for n in TEMPLATES]
    files += [f"apcpduApp/Db/{n}.substitutions" for n in SUBSTITUTIONS]
    files += [f"apcpduApp/Db/{n}.template" for n in
              ("pdu_outlet_read", "pdu_outlet_write", "pdu_outlet_group", "pdu_bank")]
    files += ["apcpduApp/Db/apcpdu_group.json", "apcpduApp/src/apcpduInfoScan.cpp",
              "apcpduApp/src/apcpduInfoScan.dbd", "tests/snmp_peer.py", "mibs/PowerNet-MIB"]
    hashes = {}
    for name in files:
        data = subprocess.check_output(["git", "-C", str(apc), "show", f"{APC_COMMIT}:{name}"])
        if (apc / name).read_bytes() != data:
            raise ValueError(f"Selected APC source differs from pinned baseline: {name}")
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        hashes[name] = digest(target)
    db = source / "apcpduApp/Db"
    macros = f"P={PREFIX},R=,HOST=127.0.0.1,COMM=local,TPRO=0,OBJ=group,OUTLET_SCAN={PREFIX}Outlet1TriggerProcessing_"
    msi = base / "bin" / ARCH / "msi"
    expanded = []
    for name in ("pdu_scan", "pdu_device_info", "pdu_outlets_24_read", "pdu_load_rpdu2g",
                 "pdu_banks_2", "pdu_limits", "pdu_outlets_24_write", "pdu_pva_status_24",
                 "pdu_outlets_24_group"):
        argv = [msi, "-V", "-I" + str(db), "-M" + macros]
        argv += ["-S" + str(db / (name + ".substitutions"))] if name in SUBSTITUTIONS else [db / (name + ".template")]
        expanded.append(run(argv))
    original = "\n".join(expanded)
    (output / "original.db").write_text(original)
    peer = load_peer(source / "tests/snmp_peer.py")
    migrated, bindings, conversions, public = migrate(original, peer.mib_oids())
    (output / "db").mkdir()
    (output / "db/ap8932.db").write_text(migrated)
    group_text = run([msi, "-V", "-M" + macros, db / "apcpdu_group.json"])
    (output / "db/apcpdu_group.json").write_text(group_text)
    mappings = {k: v for k, v in json.loads(group_text)[PREFIX + "group"].items() if not k.startswith("+")}
    for match in RECORD.finditer(expanded[-1]):
        field = re.search(r'"(ch\d+\.[^"]+)":\s*\{\+type: "scalar", \+channel: "VAL"', match[0])
        if not field or field[1] in mappings:
            raise ValueError("Unexpected outlet group mapping")
        mappings[field[1]] = {"+type": "scalar", "+channel": match[2] + ".VAL"}
    write_json(output / "inventory.json", {"public_records": public, "bindings": bindings,
               "conversions": conversions, "group": PREFIX + "group", "mappings": mappings})
    write_json(output / "config.template.json", {"schema": 1,
               "profiles": [{"id": "Local", "version": "2c", "communityFile": "community.txt",
                             "timeoutMs": 100, "retries": 0, "maxVarbinds": 24}],
               "endpoints": [{"id": "Local", "profile": "Local", "address": "127.0.0.1", "port": 161}],
               "bindings": bindings})
    configure = output / "configure"
    configure.mkdir()
    for name in ("CONFIG", "RULES", "RULES_TOP", "RULES_DIRS", "Makefile"):
        (configure / name).write_bytes((ROOT / "configure" / name).read_bytes())
    (configure / "RELEASE").write_text(f"SNMP3 = {ROOT}\nPVXS = {pvxs}\nEPICS_BASE = {base}\n")
    (configure / "CONFIG_SITE").write_text("CHECK_RELEASE = YES\nUSR_CXXFLAGS += -std=c++11\n")
    (output / "Makefile").write_text("TOP=.\ninclude $(TOP)/configure/CONFIG\nDIRS += configure app\napp_DEPEND_DIRS = configure\ninclude $(TOP)/configure/RULES_TOP\n")
    app = output / "app"
    app.mkdir()
    (app / "Main.cpp").write_bytes((ROOT / "snmp3App/src/Main.cpp").read_bytes())
    for name in ("apcpduInfoScan.cpp", "apcpduInfoScan.dbd"):
        (app / name).write_bytes((source / "apcpduApp/src" / name).read_bytes())
    (app / "Makefile").write_text(f'''TOP=..
include $(TOP)/configure/CONFIG
USR_INCLUDES += -I$(SNMP3)/snmp3App/src
PROD_IOC += {APP}
DBD += {APP}.dbd
{APP}_DBD += base.dbd snmp3.dbd pvxsIoc.dbd apcpduInfoScan.dbd
{APP}_SRCS += Main.cpp apcpduInfoScan.cpp {APP}_registerRecordDeviceDriver.cpp
{APP}_LIBS += snmp3 snmp3Wire pvxsIoc pvxs $(EPICS_BASE_IOC_LIBS)
include $(TOP)/configure/RULES
''')
    loaders = output / "iocsh"
    loaders.mkdir()
    (loaders / "configuration.cmd").write_text(f'''on error break
snmp3Load("$(CONFIG)","{ROOT}/bin/{ARCH}/snmp3NativeProbe")
snmp3WorkerPath("{ROOT}/bin/{ARCH}/snmp3Worker")
''')
    (loaders / "device.cmd").write_text(f'''on error break
iocshLoad("{loaders}/configuration.cmd","CONFIG=$(CONFIG)")
dbLoadRecords("{output}/db/ap8932.db")
dbLoadGroup("{output}/db/apcpdu_group.json")
''')
    run(["make", "-j2", "--output-sync=recurse"])
    binary = output / "bin" / ARCH / APP
    libraries = run(["ldd", binary])
    if "not found" in libraries or "devSnmp" in libraries or "freshSnmp" in libraries:
        raise ValueError("Unexpected application libraries")
    identities = {str(path): digest(path) for path in required + [binary, output / "dbd" / (APP + ".dbd"),
                  app / "Main.cpp", output / "db/ap8932.db", output / "db/apcpdu_group.json",
                  output / "inventory.json", output / "config.template.json",
                  loaders / "configuration.cmd", loaders / "device.cmd"]}
    for path in re.findall(r"=> (/\S+)", libraries):
        identities[path] = digest(path)
    write_json(output / "build.json", {"apcpdu_commit": APC_COMMIT, "source_hashes": hashes,
               "snmp3_commit": run(["git", "-C", ROOT, "rev-parse", "HEAD"]).strip(),
               "base": str(base), "pvxs": str(pvxs), "snmp3": str(ROOT), "app": APP,
               "prefix": PREFIX, "products": identities, "builder_sha256": digest(__file__),
               "deadline_ms": DEADLINE_MS,
               "public_records": len(public), "bindings": len(bindings), "conversions": conversions,
               "mappings": len(mappings)})
    print(f"Built {binary}; {len(public)} public records, {len(bindings)} bindings, {len(mappings)} group fields")


if __name__ == "__main__":
    main()
