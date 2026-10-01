#!/usr/bin/env python3
"""Execute shipped configuration, native capability, IOC and regression paths."""

import argparse
import copy
import errno
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

from run_independence import ARCH, BUILD_TIMEOUT, ROOT, TIMEOUT, digest, invoke, owns_udp, udp_port, validate_base

REPORT = re.compile(r"snmp3 config: revision=(\d+) frozen=(\d+) profiles=(\d+) endpoints=(\d+) bindings=(\d+)")
PROFILE_FIELDS = {"timeoutMs": (1, 60000), "retries": (0, 10), "maxVarbinds": (1, 1024)}
VALUE_TYPES = ("integer", "unsigned32", "counter32", "gauge32", "timeticks", "counter64",
               "octets", "oid", "ipAddress", "opaqueFloat", "opaqueDouble")


def config_reports(text):
    return [tuple(map(int, fields)) for fields in REPORT.findall(text)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    base = validate_base(parser, args.base)
    output = args.output.resolve()
    if output.exists():
        parser.error("--output must be a new directory")
    if any(c in str(base) + str(output) for c in ' \t\n#$"\\'):
        parser.error("paths contain unsupported command or Make characters")
    local = ROOT / "configure/RELEASE.local"
    if local.exists() and local.read_text() != f"EPICS_BASE = {base}\n":
        parser.error("existing configure/RELEASE.local differs")
    output.mkdir(parents=True, mode=0o700)
    fixtures = output / "fixtures"
    fixtures.mkdir(mode=0o700)
    results = {"status": "FAIL", "base": str(base), "checks": {}, "not_run": {}, "cleanup": {},
               "fixture_sha256": {}, "fixture_metadata": {}}
    env = os.environ.copy()
    for key in ("LD_PRELOAD", "LD_AUDIT", "LD_DEBUG", "LD_DEBUG_OUTPUT", "LD_LIBRARY_PATH"):
        env.pop(key, None)
    env.update(EPICS_HOST_ARCH=ARCH, LD_LIBRARY_PATH=f"{ROOT}/lib/{ARCH}:{base}/lib/{ARCH}")
    repeater = None
    streams = []
    sentinels = []
    diagnostics = []
    probe = ROOT / "bin" / ARCH / "snmp3NativeProbe"
    product = ROOT / "bin" / ARCH / "snmp3ConfigTest"
    binary = ROOT / "bin" / ARCH / "snmp3Ioc"

    def check(name, condition):
        results["checks"][name] = bool(condition)
        if not condition:
            raise RuntimeError(name)

    def run(name, command, expected=0, timeout=TIMEOUT, loader=False, stdin=None):
        child_env = dict(env, LD_DEBUG="libs") if loader else env
        observed = invoke([str(arg) for arg in command], output, name, child_env,
                          timeout=timeout, stdin=stdin)
        diagnostics.extend([observed.stdout, observed.stderr])
        check(name + "_exit", observed.returncode == expected)
        if loader:
            loaded = [Path(p).resolve() for p in re.findall(r"calling init:\s*(\S+)", observed.stderr)]
            results.setdefault("loaded_libraries", {})[name] = [str(p) for p in loaded]
            results.setdefault("library_sha256", {}).update({str(p): digest(p) for p in loaded})
        return observed

    def write_case(name, document, raw=False):
        path = fixtures / (name + ".json")
        data = document if raw else json.dumps(document, ensure_ascii=False).encode()
        path.write_bytes(data if isinstance(data, bytes) else data.encode())
        return path

    def component(name, mode, *arguments):
        observed = run(name, [product, mode, *arguments], loader=True)
        loaded = results["loaded_libraries"][name]
        check(name + "_component_identity", str((ROOT / "lib" / ARCH / "libsnmp3.so").resolve()) in loaded and
              str((base / "lib" / ARCH / "libCom.so").resolve()) in loaded and
              not any("netsnmp" in path.lower() for path in loaded))
        receipt = json.loads(observed.stdout)
        check(name + "_real_component", receipt["status"] == "PASS" and receipt["checks"] > 0)
        results.setdefault("component_checks", {})[name] = receipt["checks"]

    def reaped(executable):
        note = executable.with_suffix(".pid")
        identity = json.loads(note.read_text())
        stat = Path(f'/proc/{identity["pid"]}/stat')
        try:
            gone = stat.read_text().split()[21] != identity["start"]
        except FileNotFoundError:
            gone = True
        results["cleanup"][executable.stem] = {**identity, "reaped": gone}
        check(executable.stem + "_reaped", gone)

    try:
        regression = run("lifecycle", [sys.executable, ROOT / "tests/rewrite/test_lifecycle.py",
                                       "--base", base, "--output", output / "lifecycle"],
                         timeout=BUILD_TIMEOUT + 240)
        check("V3.11_regressions", regression.returncode == 0 and
              json.loads((output / "lifecycle/results.json").read_text())["status"] == "PASS")
        inputs = [path for directory in (ROOT / "snmp3App", ROOT / "tests/rewrite", ROOT / "docs", ROOT / "configure")
                  for path in directory.rglob("*") if path.is_file() and
                  not any(part.startswith("O.") or part == "__pycache__" for part in path.parts)]
        inputs += [ROOT / "Makefile", base / "include/epicsVersion.h", base / "include/yajl_parse.h",
                   probe, product, binary, ROOT / "lib" / ARCH / "libsnmp3.so"]
        results["sha256"] = {str(path): digest(path) for path in sorted(set(inputs))}
        native = run("native-capabilities", [probe, "--capabilities"], loader=True)
        capabilities = json.loads(native.stdout)
        check("V3.6_actual_native_catalog", capabilities["schema"] == 1 and capabilities["version"] and
              capabilities["authentication"] and capabilities["privacy"] and
              any("libnetsnmp" in path for path in results["loaded_libraries"]["native-capabilities"]))
        bad_native = run("native-cli", [probe, "--invalid"], expected=2)
        check("native_cli_usage", "Usage:" in bad_native.stderr)
        seed = json.loads((ROOT / "tests/rewrite/config/seed.json").read_text())
        auth = capabilities["authentication"][0]["name"]
        priv = capabilities["privacy"][0]["name"]
        for profile in seed["profiles"]:
            if "authAlgorithm" in profile:
                profile["authAlgorithm"] = auth
            if "privAlgorithm" in profile:
                profile["privAlgorithm"] = priv
        for name in ("community", "auth", "priv"):
            sentinel = "FixtureSecret_" + uuid.uuid4().hex
            sentinels.append(sentinel)
            path = fixtures / (name + ".txt")
            path.write_text(sentinel + "\n")
            path.chmod(0o600)
        good = write_case("valid", seed)
        replacement = copy.deepcopy(seed)
        replacement["profiles"][1].update(timeoutMs=1500, checkRanges=False)
        replacement_path = write_case("replacement", replacement)
        operator_example = write_case("operator-example", json.loads(
            (ROOT / "tests/rewrite/config/v2c.json").read_text()))
        component("V3.13_operator_JSON", "accept", operator_example, probe)
        component("V3.2_4_5_8_9_positive", "positive", good, probe, replacement_path)
        component("V3.1_parser_limits", "json-limits")

        def negative(name, document, bad_probe=probe, raw=False):
            bad = write_case(name, document, raw=raw)
            component(name, "negative", good, probe, bad, bad_probe)

        def changed(name, section, index, key, value):
            document = copy.deepcopy(seed)
            if value is None:
                document[section][index].pop(key, None)
            else:
                document[section][index][key] = value
            negative(name, document)

        strict = json.dumps(seed)
        for name, text in {
            "json5-unquoted": strict.replace('"schema"', "schema", 1),
            "json5-single-quote": strict.replace('"schema"', "'schema'", 1),
            "json5-trailing-comma": strict[:-1] + ",}",
            "json-comment": "/* configuration */" + strict,
            "json-line-comment": "// configuration\n" + strict,
            "json-trailing-data": strict + " false",
            "json-truncated": strict[:-1],
            "json-duplicate-root": strict.replace('"schema": 1', '"schema": 1, "schema": 1', 1),
            "json-duplicate-nested": strict.replace('"id": "V1"', '"id": "V1", "id": "V1"', 1),
            "json-escaped-duplicate": strict.replace('"id": "V1"', '"id": "V1", "\\u0069d": "V1"', 1),
            "json-depth": "[" * 33 + "0" + "]" * 33,
            "json-empty": "",
        }.items():
            negative(name, text, raw=True)
        negative("json-invalid-utf8", strict.encode().replace(b"fixture-user", b"\xff", 1), raw=True)
        for name, data in {
            "overlong2": b"\xc0\xaf", "overlong3": b"\xe0\x80\xaf", "overlong4": b"\xf0\x80\x80\xaf",
            "surrogate": b"\xed\xa0\x80", "above-max": b"\xf4\x90\x80\x80",
            "leading-f5": b"\xf5\x80\x80\x80", "continuation": b"\x80",
            "incomplete2": b"\xc2", "incomplete3": b"\xe0\xa0", "incomplete4": b"\xf0\x90\x80",
            "bad-continuation": b"\xc2A",
        }.items():
            negative("utf8-" + name, strict.encode().replace(b"fixture-user", data, 1), raw=True)
        for name, value in (("low", r'"\udc00"'), ("high", r'"\ud800"'),
                            ("high-followed-by-scalar", r'"\ud800\u0041"')):
            negative("unicode-" + name, strict.replace('"fixture-user"', value, 1), raw=True)
        negative("json-size", b" " * (1024 * 1024 + 1), raw=True)
        for schema in (0, 2, "1", True, 1.0):
            document = copy.deepcopy(seed)
            document["schema"] = schema
            negative("schema-" + str(schema).replace(".", "-"), document)
        for section in ("profiles", "endpoints", "bindings"):
            document = copy.deepcopy(seed)
            del document[section]
            negative("missing-" + section, document)
            document = copy.deepcopy(seed)
            document[section] = {}
            negative("wrong-array-" + section, document)
            document = copy.deepcopy(seed)
            document[section].append(copy.deepcopy(document[section][0]))
            negative("duplicate-" + section, document)
            changed("unknown-" + section, section, 0, "unused", 1)
        document = copy.deepcopy(seed)
        document["unused"] = 1
        negative("unknown-root", document)
        for field, bounds in PROFILE_FIELDS.items():
            for value in (bounds[0] - 1, bounds[1] + 1, 1.5, "1", True):
                changed(field + "-" + str(value).replace(".", "-"), "profiles", 1, field, value)
        for value in (0, "true", 1):
            changed("checkRanges-" + str(value), "profiles", 1, "checkRanges", value)
        for value in ("", "x" * 65, "non ascii", "\u00e9", "bad/id", "bad\x00id"):
            changed("bad-id-" + str(len(results["checks"])), "profiles", 0, "id", value)
        changed("missing-profile", "endpoints", 0, "profile", "absent")
        changed("missing-endpoint", "bindings", 0, "endpoint", "absent")
        for value in ("localhost", "udp:127.0.0.1:161", "127.0.0.999", ""):
            changed("bad-address-" + str(len(results["checks"])), "endpoints", 0, "address", value)
        for value in (0, 65536, 1.5):
            changed("bad-port-" + str(value).replace(".", "-"), "endpoints", 0, "port", value)
        for value in ("", "1", "1.40", "3.0", "1.3.", "1..3", "1.-1", "1.4294967296", "name.oid",
                      ".1.3." + ".".join(["0"] * 127)):
            changed("bad-oid-" + str(len(results["checks"])), "bindings", 0, "oid", value)
        changed("bad-operation", "bindings", 0, "operation", "walk")
        changed("bad-value-type", "bindings", 0, "valueType", "unknown")
        for value in (0, 2, 1.5):
            changed("bad-scalar-capacity-" + str(value).replace(".", "-"), "bindings", 0, "capacity", value)
        changed("bad-octet-capacity", "bindings", 2, "capacity", 1024 * 1024 + 1)
        for field in ("user", "securityLevel", "authAlgorithm", "authSecretFile", "privAlgorithm", "privSecretFile",
                      "contextName", "securityEngineId", "contextEngineId"):
            changed("v2-forbids-" + field, "profiles", 1, field, "unused")
        changed("v3-forbids-community", "profiles", 2, "communityFile", "community.txt")
        changed("v1-missing-community", "profiles", 0, "communityFile", None)
        changed("v3-missing-user", "profiles", 2, "user", None)
        changed("v3-empty-user", "profiles", 2, "user", "")
        changed("v3-user-size", "profiles", 2, "user", "x" * 33)
        changed("v3-context-size", "profiles", 2, "contextName", "x" * 256)
        changed("bad-version", "profiles", 1, "version", "4")
        changed("bad-security-level", "profiles", 2, "securityLevel", "privOnly")
        for field in ("authAlgorithm", "authSecretFile", "privAlgorithm", "privSecretFile"):
            changed("noauth-forbids-" + field, "profiles", 2, field, "unused")
        changed("auth-missing-algorithm", "profiles", 3, "authAlgorithm", None)
        changed("auth-missing-secret", "profiles", 3, "authSecretFile", None)
        changed("auth-forbids-privacy", "profiles", 3, "privAlgorithm", priv)
        changed("priv-missing-algorithm", "profiles", 4, "privAlgorithm", None)
        changed("priv-missing-secret", "profiles", 4, "privSecretFile", None)
        changed("auth-unavailable", "profiles", 3, "authAlgorithm", "FixtureUnavailable")
        changed("priv-unavailable", "profiles", 4, "privAlgorithm", "FixtureUnavailable")
        for field in ("securityEngineId", "contextEngineId"):
            for value in ("", "80000001", "80" * 33, "800000001", "gg00000001", "00" * 5, "ff" * 5, "0x"):
                changed("engine-" + field + "-" + str(len(results["checks"])), "profiles", 4, field, value)

        def secret_case(name, data=None, mode=0o600, kind="regular", index=1, field="communityFile"):
            path = fixtures / ("secret-" + name)
            if kind == "symlink":
                path.symlink_to(fixtures / "community.txt")
            elif kind == "directory":
                path.mkdir()
            elif kind == "fifo":
                os.mkfifo(path, 0o600)
            elif kind == "missing":
                pass
            else:
                path.write_bytes(data)
                results["fixture_sha256"][str(path)] = digest(path)
                path.chmod(mode)
            changed("secret-" + name, "profiles", index, field, path.name)

        for mode in (0o644, 0o640, 0o604, 0o610, 0o601, 0):
            secret_case("mode-" + oct(mode)[2:], b"fixture-community", mode=mode)
        for kind in ("symlink", "directory", "fifo", "missing"):
            secret_case(kind, kind=kind)
        for name, data in {"empty": b"", "nul": b"secret\x00value", "control": b"secret\x01value",
                           "two-newlines": b"fixture-secret\n\n", "community256": b"x" * 256,
                           "large-file": b"x" * 1027}.items():
            secret_case(name, data)
        secret_case("short-auth", b"x" * 7, index=3, field="authSecretFile")
        secret_case("long-auth", b"x" * 1025, index=3, field="authSecretFile")
        wrong_owner = fixtures / "secret-owner"
        wrong_owner.write_bytes(b"fixture-community")
        results["fixture_sha256"][str(wrong_owner)] = digest(wrong_owner)
        wrong_owner.chmod(0o600)
        try:
            os.chown(wrong_owner, 0 if os.geteuid() != 0 else 1, -1)
        except OSError as error:
            if error.errno not in (errno.EPERM, errno.EACCES, errno.EINVAL):
                raise
            results["not_run"]["V3.3_wrong_owner"] = (
                "cannot create an independently owned regular fixture; chown errno=" + str(error.errno))
        else:
            changed("secret-wrong-owner", "profiles", 1, "communityFile", wrong_owner.name)

        positive_cases = {}
        for value in (0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xffff, 0x10000, 0x10ffff):
            document = copy.deepcopy(seed)
            document["profiles"][2]["user"] = chr(value)
            positive_cases["utf8-valid-" + hex(value)[2:]] = document
        component("unicode-valid-pair", "accept", write_case(
            "unicode-valid-pair", strict.replace('"fixture-user"', r'"\ud834\udd1e"', 1), raw=True), probe)
        document = copy.deepcopy(seed)
        for field, bounds in PROFILE_FIELDS.items():
            document["profiles"][0][field] = bounds[0]
            document["profiles"][1][field] = bounds[1]
        document["profiles"][0]["id"] = "i" * 64
        document["profiles"][2].update(user="u" * 32, contextName="c" * 255)
        document["profiles"][4].update(securityEngineId="0X800000000A", contextEngineId="800000000b")
        document["endpoints"][0]["port"] = 65535
        document["bindings"][0]["oid"] = ".2.4294967295." + ".".join(["0"] * 126)
        document["bindings"][2]["capacity"] = 1024 * 1024
        positive_cases["schema-boundaries"] = document
        document = copy.deepcopy(seed)
        document["bindings"] = [{**seed["bindings"][0], "id": "Type_" + tag, "valueType": tag,
                                 "capacity": 128 if tag == "oid" else 1} for tag in VALUE_TYPES]
        positive_cases["binding-types"] = document
        positive_cases["empty-config"] = {"schema": 1, "profiles": [], "endpoints": [], "bindings": []}
        document = copy.deepcopy(seed)
        for index in range(4096 - len(document["profiles"])):
            document["profiles"].append({"id": f"P{index}", "version": "1", "communityFile": "community.txt"})
        positive_cases["definitions4096"] = document
        excessive = copy.deepcopy(document)
        excessive["profiles"].append({"id": "overflow", "version": "1", "communityFile": "community.txt"})
        negative("definitions4097", excessive)
        for name, data, index, field in (("community255", b"x" * 255 + b"\r\n", 1, "communityFile"),
                                         ("auth8", b"x" * 8 + b"\n", 3, "authSecretFile"),
                                         ("auth1024", b"x" * 1024 + b"\r\n", 3, "authSecretFile")):
            path = fixtures / name
            path.write_bytes(data)
            path.chmod(0o600)
            document = copy.deepcopy(seed)
            document["profiles"][index][field] = str(path)
            positive_cases[name] = document
        for category, index, field in (("authentication", 3, "authAlgorithm"), ("privacy", 4, "privAlgorithm")):
            for algorithm in capabilities[category]:
                document = copy.deepcopy(seed)
                document["profiles"][index][field] = algorithm["name"]
                positive_cases["native-" + category + "-" + algorithm["name"]] = document
        for name, document in positive_cases.items():
            component(name, "accept", write_case(name, document), probe)

        fault_source = ROOT / "tests/rewrite/helpers/probe_fault.py"
        (fixtures / "production-probe.txt").write_text(str(probe))
        for mode in ("fail", "malformed", "oversized", "timeout", "closed-output", "schema", "duplicate"):
            helper = fixtures / ("probe-" + mode + ".py")
            helper.symlink_to(fault_source)
            start = time.monotonic()
            component("helper-" + mode, "negative", good, probe, good, helper)
            elapsed = time.monotonic() - start
            check("helper-" + mode + "_bounded", elapsed < 10)
            results.setdefault("elapsed_seconds", {})[mode] = elapsed
            reaped(helper)
        for name, helper in (("missing", fixtures / "absent-helper"),
                             ("nonexecutable", fixtures / "nonexecutable-helper"),
                             ("relative", Path("relative-helper"))):
            if name == "nonexecutable":
                helper.write_text("not executable")
                helper.chmod(0o600)
            component("helper-" + name, "negative", good, probe, good, helper)
        for mode in ("freeze", "bind"):
            gate = fixtures / ("gate-" + mode + ".py")
            gate.symlink_to(fault_source)
            component("V3.5_race-" + mode, "race-" + mode, good, probe, gate,
                      gate.with_suffix(".pid"), gate.with_suffix(".release"))
            reaped(gate)

        port, repeat_port = udp_port(), udp_port()
        while port == repeat_port:
            repeat_port = udp_port()
        env.update(EPICS_CA_SERVER_PORT=str(port), EPICS_CA_REPEATER_PORT=str(repeat_port),
                   EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST="127.0.0.1",
                   EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
                   EPICS_CAS_BEACON_ADDR_LIST="127.0.0.1")
        results["ports"] = {"server": port, "repeater": repeat_port}
        streams = [(output / "repeater.stdout").open("w"), (output / "repeater.stderr").open("w")]
        repeater = subprocess.Popen([str(base / "bin" / ARCH / "caRepeater")], env=env,
                                    stdout=streams[0], stderr=streams[1])
        deadline = time.monotonic() + TIMEOUT
        while repeater.poll() is None and time.monotonic() < deadline:
            if owns_udp(repeater.pid, repeat_port):
                break
            time.sleep(0.02)
        check("owned_repeater_ready", repeater.poll() is None and owns_udp(repeater.pid, repeat_port))
        worker = ROOT / "bin" / ARCH / "snmp3Worker"
        env.update(CONFIG_JSON=str(operator_example), NATIVE_PROBE=str(probe), WORKER_PATH=str(worker))
        documented = run("ioc-documented-startup", [binary, ROOT / "tests/rewrite/config/startup.cmd"],
                         loader=True, stdin="exit\n")
        check("V3.13_documented_startup", config_reports(documented.stdout) ==
              [(1, 0, 1, 1, 1), (1, 1, 1, 1, 1), (1, 1, 1, 1, 1)] and
              documented.stdout.index("snmp3 config: revision=1 frozen=1") <
              documented.stdout.index("dbProcess of 'Config_PiniProbe'"))
        prefix = (f'dbLoadDatabase("{ROOT}/dbd/snmp3Ioc.dbd")\n'
                  'snmp3Ioc_registerRecordDeviceDriver(pdbbase)\n'
                  f'snmp3WorkerPath("{worker}")\n')
        load = f'snmp3Load("{good}", "{probe}")\n'
        reload = f'snmp3Load("{replacement_path}", "{probe}")\n'
        db = f'dbLoadRecords("{ROOT}/tests/rewrite/db/lifecycle.db", "P=Config_")\n'

        def ioc(name, body, expected=0, policy="break"):
            script = output / (name + ".cmd")
            script.write_text("on error " + policy + "\n" + prefix + body)
            observed = run(name, [binary, script], expected=expected, loader=True, stdin="exit\n")
            loaded = results["loaded_libraries"][name]
            check(name + "_IOC_independence", not any("netsnmp" in path.lower() for path in loaded) and
                  str((ROOT / "lib" / ARCH / "libsnmp3.so").resolve()) in loaded)
            return observed

        normal = ioc("ioc-startup", load + "snmp3ConfigReport\n" + db + "iocInit\n"
                     'dbpr("Config_PiniProbe", 2)\nsnmp3ConfigReport\n')
        check("V3.10_before_PINI_freeze", config_reports(normal.stdout) ==
              [(1, 0, 6, 4, 3), (1, 1, 6, 4, 3), (1, 1, 6, 4, 3)] and
              normal.stdout.index("snmp3 config: revision=1 frozen=1") <
              normal.stdout.index("dbProcess of 'Config_PiniProbe'") and
              re.search(r"\bVAL\s*:\s*42\b", normal.stdout))
        changed_ioc = ioc("ioc-replacement", load + reload + "snmp3ConfigReport\n")
        check("V3.5_IOC_whole_replacement", config_reports(changed_ioc.stdout) == [(2, 0, 6, 4, 3)])
        for name, state in (("build", db + "iocBuild\n"), ("start", db + "iocInit\n"),
                            ("stop", db + "iocInit\nsnmp3Stop\n"), ("cold-stop", "snmp3Stop\n")):
            rejected = ioc("ioc-frozen-" + name, load + state + reload + "echo MUST_NOT_RUN\n", expected=1)
            check("V3.10_frozen_" + name, "configuration frozen" in rejected.stderr and
                  "echo MUST_NOT_RUN" not in rejected.stdout)
        rejected_json = write_case("ioc-json5", strict.replace('"schema"', "schema", 1), raw=True)
        failure = ioc("ioc-load-error", f'snmp3Load("{rejected_json}", "{probe}")\necho MUST_NOT_RUN\n', expected=1)
        check("V3.1_IOC_strict_JSON", "JSON rejected" in failure.stderr and "echo MUST_NOT_RUN" not in failure.stdout)
        preserved = ioc("ioc-load-preservation", load + "snmp3ConfigReport\n" +
                        f'snmp3Load("{rejected_json}", "{probe}")\n' +
                        "snmp3ConfigReport\necho CONTINUED\n", policy="continue")
        check("V3.5_IOC_continue_preservation", config_reports(preserved.stdout) ==
              [(1, 0, 6, 4, 3), (1, 0, 6, 4, 3)] and "CONTINUED" in preserved.stdout and
              "JSON rejected" in preserved.stderr)
        invalid_unicode = write_case("ioc-invalid-utf8", strict.encode().replace(
            b"fixture-user", b"\xc0\xaf", 1), raw=True)
        unicode_load = f'snmp3Load("{invalid_unicode}", "{probe}")\n'
        unicode_failure = ioc("ioc-utf8-error", load + "snmp3ConfigReport\n" +
                              unicode_load + "echo MUST_NOT_RUN\n", expected=1)
        check("V3.1_IOC_invalid_UTF8", config_reports(unicode_failure.stdout) == [(1, 0, 6, 4, 3)] and
              "JSON rejected" in unicode_failure.stderr and "echo MUST_NOT_RUN" not in unicode_failure.stdout)
        unicode_preserved = ioc("ioc-utf8-preservation", load + "snmp3ConfigReport\n" + unicode_load +
                                "snmp3ConfigReport\necho CONTINUED\n", policy="continue")
        check("V3.5_IOC_UTF8_preservation", config_reports(unicode_preserved.stdout) ==
              [(1, 0, 6, 4, 3), (1, 0, 6, 4, 3)] and "CONTINUED" in unicode_preserved.stdout and
              "JSON rejected" in unicode_preserved.stderr)
        arguments = ioc("ioc-missing-arguments", "snmp3Load\necho MUST_NOT_RUN\n", expected=1)
        check("IOC_argument_failure", "configuration arguments required" in arguments.stderr)
        help_run = ioc("ioc-help", "help snmp3Load\nhelp snmp3ConfigReport\nhelp snmp3Stop\n"
                       "help snmp3Report\nhelp snmp3RuntimeReport\n")
        check("V3.13_command_help", "nativeProbe is absolute" in help_run.stdout and
              "no secret values" in help_run.stdout and "Freeze configuration" in help_run.stdout)
        check("V3.12_secret_diagnostics", not any(secret in text for secret in sentinels for text in diagnostics))
        check("V3.11_inputs_unchanged", all(digest(Path(path)) == expected
              for path, expected in results["sha256"].items()))
        results["status"] = "PASS"
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        results["error"] = str(error)
    finally:
        if repeater is not None:
            if repeater.poll() is None:
                repeater.terminate()
            try:
                rc = repeater.wait(timeout=TIMEOUT)
                results["cleanup"]["repeater"] = {"pid": repeater.pid, "returncode": rc, "reaped": True}
                if rc != -15:
                    results["status"] = "FAIL"
            except subprocess.TimeoutExpired:
                repeater.kill()
                repeater.wait()
                results["cleanup"]["repeater"] = {"pid": repeater.pid, "forced": True, "reaped": True}
                results["status"] = "FAIL"
        for stream in streams:
            stream.close()
        for path in fixtures.iterdir():
            status = path.lstat()
            results["fixture_metadata"][str(path)] = {
                "mode": oct(status.st_mode), "uid": status.st_uid, "size": status.st_size}
            if path.is_file():
                try:
                    results["fixture_sha256"][str(path)] = digest(path)
                except PermissionError:
                    results["fixture_metadata"][str(path)]["hash_read_before_restriction"] = True
                    if str(path) not in results["fixture_sha256"]:
                        results["error"] = "fixture hash unavailable"
                        results["status"] = "FAIL"
        check_secret = not any(secret in json.dumps(results) for secret in sentinels)
        results["checks"]["V3.12_secret_receipt"] = check_secret
        if not check_secret:
            results["status"] = "FAIL"
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(results["status"] + ": " + str(output / "results.json"))
    return 0 if results["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
