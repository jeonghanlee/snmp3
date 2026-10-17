#!/usr/bin/env python3
"""Run the shipped IOC (snmp3 module) over a database built from an object list and time its passes.

Two database shapes:

  tiers  The object list is cut tier by tier (state, meas, slow) into chunks of K objects, in list order, the
         remainder last. The chunks of a pass are chained inside the database: the last record of a chunk
         completes into the trigger of the next chunk by FLNK, so one request is outstanding at a time like the
         direct sequential pass. --once times --passes passes, each started by one put with callback; without
         --once the tiers run on their own periods (5, 10, 60 s) and contend for the address queue.
  flat   N records of one binding behind a fanout tree, admitted together by one PROC write: the 100-record
         measurement of run_fixture_module.py, for the reproduction check of the new tool.

With --groups off every object is its own binding and record, so a chunk of K objects is K single-object GET
requests and a pass of the 358-object list is 358 requests; the address worker serves one request at a time.
The direct pass of --direct-binary sends 14.

Against the fixture agent through the delay proxy of fixture_harness.py, or against the devices of a targets file
(read-only GET; the run needs the owner's authorization and a person present, so the device path refuses to
start without --confirm-authorized, which only checks that a text is given and records it in the results).
With --direct-binary (device_sweep) a --once run also takes the direct sequential pass of the same session,
with the same K, the same tier cut and the same objects in tier order.

Pass time is the difference of the server time stamps of a synchronous marker record at the start of the pass
and of the marker after the last chunk, read by one persistent camonitor (asynchronous snmp3 records are
stamped at completion). The 45 ms start-up cost of a caput process is outside it.

Stop rules (test-procedure.md), evaluated from the monitor events of every record while the run goes on (every
0.5 s in --once, every 2 s in the sustained schedule), so a stop takes effect within about a second and the IOC is
closed at once (its worker answers the requests in flight with Stopping): a member with status READ stops the
run; members with status COMM and no message are lost requests, and two or more above 0.5 percent of the requests
processed so far stop it; a member expired with "deadline before send" was never sent: the run is invalid and
ends at once. A loss is counted at every processing of a record, so it is neither overwritten by the next cycle
nor merged with the loss of the same record in the next cycle. A direct pass that stopped, lost a request or
returned fewer passes than asked stops the run and gives no ratio. The numbers of a stopped run are those taken
at the moment of the stop, before the IOC shut down.

--groups on writes one group per chunk and ends a chunk by a counter (`C###_Cnt`, `C###_Chk`, reset by
`C###_Rst` when the chunk is triggered) instead of the last member, because the members complete from one
result on parallel callback threads.

Child processes (camonitor, caRepeater, the second client, the direct pass, the fixture agent and proxy) die with
this process; a killed tool leaves no results document. Exit codes: 0 finished, 1 error (also a refused start),
10 a stop rule fired, a pass did not finish or the direct pass failed, 12 the run is invalid (an unsent record)."""

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fixture_harness import Bench, PRODUCTS, ROOT, USER, die_with_parent, free_port  # noqa: E402
from resource_sampler import Sampler  # noqa: E402

BASE = Path(os.environ.get("EPICS_BASE", "/nonexistent-epics-base"))
CA = BASE / "bin" / "linux-x86_64"
STAMP = "%Y-%m-%d %H:%M:%S.%f"
TIERS = ("state", "meas", "slow")
PERIODS = {"state": 5, "meas": 10, "slow": 60}
LINKS = "0123456789ABCDEF"
FIXTURE_OID = {"integer": "1.3.6.1.4.1.53864.4.1.0", "octets": "1.3.6.1.4.1.53864.4.6.0"}
SYS_UPTIME = "1.3.6.1.2.1.1.3.0"
ALARM_SEVERITY = ("MINOR", "MAJOR", "INVALID")
PROBE_SAMPLE_LIMIT = 4096
DEFAULT_OBJECTS = ROOT / "docs" / "snmp-performance" / "inputs" / "ap8932-oids.tsv"
OCTETS_CAPACITY = [64]


def popen(argv, **kwargs):
    return subprocess.Popen(argv, preexec_fn=die_with_parent, **kwargs)


def read_text(path):
    """Device strings are not always UTF-8: a byte that does not decode must not end a case."""
    return Path(path).read_text(errors="replace")


def read_objects(path):
    with open(path, newline="") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    for row in rows:
        if row["tier"] not in TIERS or row["value_type"] not in FIXTURE_OID:
            raise SystemExit(f"unsupported object row: {row}")
    return rows


def fanout(name, members, scan=None, gate=False):
    """A two-level tree of fanout records (one fanout has 16 links); members are record or fanout names."""
    lines = []
    head = f'record(fanout, "{name}") {{\n    field(SELM, "All")'
    if scan:
        head += f'\n    field(SCAN, "{scan}")'
    if gate:
        head += '\n    field(SDIS, "Run_Gate")\n    field(DISV, "1")'
    if len(members) <= 16:
        lines.append(head)
        for index, member in enumerate(members):
            lines.append(f'    field(LNK{LINKS[index]}, "{member}")')
        lines.append("}")
        return lines
    parts = [members[i:i + 16] for i in range(0, len(members), 16)]
    if len(parts) > 16:
        raise SystemExit("a chunk of more than 256 records needs a deeper tree")
    lines.append(head)
    for index in range(len(parts)):
        lines.append(f'    field(LNK{LINKS[index]}, "{name}_{index}")')
    lines.append("}")
    for index, part in enumerate(parts):
        lines += fanout(f"{name}_{index}", part)
    return lines


def marker(name):
    """A synchronous record that posts a monitor on every processing and carries the server time stamp."""
    return f'record(calc, "{name}") {{\n    field(CALC, "1")\n    field(MDEL, "-1")\n}}'


def record(index, row, deadline_ms, flnk=None):
    name = f"O{index:03d}"
    kind = "longin" if row["value_type"] == "integer" else "lsi"
    lines = [f'record({kind}, "{name}") {{', '    field(DTYP, "snmp3")',
             f'    field(INP, "@binding={name} deadline_ms={deadline_ms}")']
    if kind == "longin":
        lines.append('    field(MDEL, "-1")')
    else:
        lines.append(f'    field(SIZV, "{OCTETS_CAPACITY[0] + 1}")\n    field(MPST, "Always")')
    if flnk:
        lines.append(f'    field(FLNK, "{flnk}")')
    lines.append("}")
    return "\n".join(lines)


def chunks_of(rows, k, tiers):
    """[(tier, [object index, ...]), ...] in tier order and list order, remainder last."""
    result = []
    for tier in tiers:
        members = [i for i, row in enumerate(rows) if row["tier"] == tier]
        for start in range(0, len(members), k):
            result.append((tier, members[start:start + k]))
    return result


def tier_database(rows, k, deadline_ms, once, tiers, groups="off"):
    """Returns (database text, chunk end markers, last marker per tier, chunks).

    With groups "on" the members of a chunk complete from one result on parallel callback threads, so the last
    member of the list is not the last to finish: every member records into a per-chunk counter by FLNK, the
    counter is reset when the chunk is triggered, and a check record fires the end of the chunk when the count
    equals the number of members."""
    chunks = chunks_of(rows, k, tiers)
    lines, markers, tier_end = [], [], {}
    lines.append(marker("Pass_Start"))
    for position, (tier, members) in enumerate(chunks):
        last_of_tier = position + 1 == len(chunks) or chunks[position + 1][0] != tier
        triggered = [f"O{i:03d}" for i in members]
        if groups == "on":
            triggered.insert(0, f"C{position:03d}_Rst")
        lines += fanout(f"C{position:03d}_Trig", triggered)
        done = f"C{position:03d}_Done"
        markers.append(done)
        lines.append(marker(done))
        if last_of_tier:
            tier_end[tier] = done
        # The last record of the chunk completes into its marker and, in a chain, the next chunk.
        successors = [done]
        if position + 1 < len(chunks) and (once or not last_of_tier):
            successors.append(f"C{position + 1:03d}_Trig")
        lines += fanout(f"C{position:03d}_End", successors)
        if groups == "on":
            count, check, reset = (f"C{position:03d}_Cnt", f"C{position:03d}_Chk", f"C{position:03d}_Rst")
            lines.append(f'record(calcout, "{reset}") {{\n    field(CALC, "0")\n    field(OUT, "{count}.VAL NPP")\n}}')
            lines.append(f'record(calcout, "{count}") {{\n    field(INPA, "{count}")\n    field(CALC, "A+1")\n'
                         f'    field(MDEL, "-1")\n    field(FLNK, "{check}")\n}}')
            lines.append(f'record(calcout, "{check}") {{\n    field(INPA, "{count}")\n'
                         f'    field(CALC, "A=={len(members)}")\n    field(OOPT, "When Non-zero")\n'
                         f'    field(OUT, "C{position:03d}_End.PROC PP")\n}}')
            for member in members:
                lines.append(record(member, rows[member], deadline_ms, flnk=count))
        else:
            for index, member in enumerate(members):
                lines.append(record(member, rows[member], deadline_ms,
                                    flnk=f"C{position:03d}_End" if index + 1 == len(members) else None))
    if once:
        lines += fanout("Pass_Trig", ["Pass_Start", "C000_Trig"])
    else:
        # A gate holds every scan until the run starts, so that an idle phase can precede the schedule.
        lines.append('record(bo, "Run_Gate") {\n    field(VAL, "1")\n    field(PINI, "YES")\n}')
        menu = {1: "1 second", 2: "2 second", 5: "5 second", 10: "10 second"}
        first_of_tier = {}
        for position, (tier, _) in enumerate(chunks):
            first_of_tier.setdefault(tier, position)
        for tier, position in first_of_tier.items():
            lines.append(marker(f"S_{tier}"))
            period = PERIODS[tier]
            members = [f"S_{tier}", f"C{position:03d}_Trig"]
            if period in menu:
                lines += fanout(f"Tier_{tier}_Trig", members, scan=menu[period], gate=True)
            else:
                # The scan menu has no such period: a 10 s clock counts and fires every period/10-th scan
                # (its first scan fires at the next 10 s tick after the gate opens).
                every = period // 10
                lines += fanout(f"Tier_{tier}_Trig", members)
                lines.append(f'record(calcout, "Clock_{tier}") {{\n    field(SCAN, "10 second")\n'
                             f'    field(SDIS, "Run_Gate")\n    field(DISV, "1")\n'
                             f'    field(VAL, "{every - 1}")\n    field(INPA, "Clock_{tier}")\n'
                             f'    field(CALC, "(A+1)%{every}")\n    field(OOPT, "When Zero")\n'
                             f'    field(OUT, "Tier_{tier}_Trig.PROC PP")\n}}')
    return "\n".join(lines) + "\n", markers, tier_end, chunks


def flat_database(count, deadline_ms):
    """N longin records of one binding behind a fanout tree, as run_fixture_module.py builds them."""
    names = [f"B_R{i:03d}" for i in range(count)]
    lines = fanout("B_Trig", names)
    for name in names:
        lines.append(f'record(longin, "{name}") {{\n    field(DTYP, "snmp3")\n'
                     f'    field(INP, "@binding=Read deadline_ms={deadline_ms}")\n    field(MDEL, "-1")\n}}')
    return "\n".join(lines) + "\n", names


def profile_for(version, secrets, timeout_ms, retries, max_varbinds, user=USER, auth="SHA", priv="AES"):
    profile = {"id": "P", "version": version, "timeoutMs": timeout_ms, "retries": retries}
    if max_varbinds:
        profile["maxVarbinds"] = max_varbinds
    if version == "2c":
        profile["communityFile"] = str(secrets["community"])
    else:
        profile.update(user=user, securityLevel="authPriv", authAlgorithm=auth,
                       authSecretFile=str(secrets["auth"]), privAlgorithm=priv,
                       privSecretFile=str(secrets["priv"]))
    return profile


def configuration(rows, shape, groups, chunks, profile, address, port, fixture):
    bindings = []
    if shape == "flat":
        bindings.append({"id": "Read", "endpoint": "E", "operation": "get", "valueType": "integer",
                         "oid": FIXTURE_OID["integer"]})
    else:
        group_of = {}
        if groups == "on":
            for position, (tier, members) in enumerate(chunks):
                for member in members:
                    group_of[member] = f"G{position:03d}"
        for index, row in enumerate(rows):
            binding = {"id": f"O{index:03d}", "endpoint": "E", "operation": "get", "valueType": row["value_type"],
                       "oid": FIXTURE_OID[row["value_type"]] if fixture else row["oid"]}
            if row["value_type"] == "octets":
                binding["capacity"] = OCTETS_CAPACITY[0]
            if index in group_of:
                binding["group"] = group_of[index]
            bindings.append(binding)
    return {"schema": 1, "profiles": [profile],
            "endpoints": [{"id": "E", "address": address, "port": port, "profile": "P"}], "bindings": bindings}


class Ioc:
    def __init__(self, case, env, startup):
        self.case = case
        self.env = env
        self.closed = False
        self.admission = []
        self.repeater = popen([str(CA / "caRepeater")], env=env, stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
        self.out = (case / "ioc.stdout").open("wb")
        self.err = (case / "ioc.stderr").open("wb")
        self.process = popen([str(PRODUCTS / "snmp3Ioc"), str(startup)], env=env, stdin=subprocess.PIPE,
                             stdout=self.out, stderr=self.err)

    def alive(self):
        return not self.closed and self.process.poll() is None

    def wait_ready(self, seconds=60):
        end = time.monotonic() + seconds
        while time.monotonic() < end and self.process.poll() is None:
            if b"iocRun: All initialization complete" in (self.case / "ioc.stderr").read_bytes():
                return
            time.sleep(0.05)
        raise RuntimeError("IOC did not complete initialization")

    def command(self, text):
        self.process.stdin.write(text.encode() + b"\n")
        self.process.stdin.flush()

    def report(self):
        if not self.alive():
            return None
        mark = read_text(self.case / "ioc.stdout").count("snmp3 loop:")
        self.command("snmp3RuntimeReport")
        end = time.monotonic() + 5
        while time.monotonic() < end:
            if read_text(self.case / "ioc.stdout").count("snmp3 loop:") > mark:
                break
            time.sleep(0.05)
        lines = read_text(self.case / "ioc.stdout").splitlines()
        loop = [line for line in lines if line.startswith("snmp3 loop:")]
        # A report ends with its loop line, so the newest report is what follows the loop line before it.
        ends = [i for i, line in enumerate(lines) if line.startswith("snmp3 loop:")]
        block = lines[(ends[-2] + 1 if len(ends) > 1 else 0):(ends[-1] if ends else len(lines))]
        self.admission = [admit_fields(line) for line in block if line.startswith("snmp3 admit:")]
        return int(loop[-1].split("passes=")[1]) if loop else None

    def close(self):
        """Stops the IOC (its worker answers requests in flight with Stopping); safe to call twice."""
        if self.closed:
            return
        self.closed = True
        stdin = self.process.stdin
        if self.process.poll() is None:
            try:
                self.command("exit")
                stdin.close()
            except (BrokenPipeError, ValueError, OSError):
                pass
        else:
            try:
                stdin.close()
            except (BrokenPipeError, ValueError, OSError):
                pass
        self.process.stdin = None
        try:
            self.process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.out.close()
        self.err.close()
        self.repeater.terminate()
        self.repeater.wait(timeout=5)


def environment():
    server, repeater_port = free_port(), free_port()
    return dict(os.environ, EPICS_CA_SERVER_PORT=str(server), EPICS_CA_REPEATER_PORT=str(repeater_port),
                EPICS_CA_AUTO_ADDR_LIST="NO", EPICS_CA_ADDR_LIST=f"127.0.0.1:{server}",
                EPICS_CAS_INTF_ADDR_LIST="127.0.0.1", EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO",
                EPICS_CAS_BEACON_ADDR_LIST=f"127.0.0.1:{repeater_port}")


def parse_line(line):
    """(name, stamp, tokens) of one camonitor line, or None."""
    parts = line.split()
    if len(parts) < 3:
        return None
    try:
        stamp = datetime.strptime(parts[1] + " " + parts[2], STAMP)
    except ValueError:
        return None
    return parts[0], stamp, parts[3:]


def admit_fields(line):
    """{field: integer} of one `snmp3 admit:` report line."""
    return {key: int(value) for key, value in (token.split("=") for token in line.split()[2:])}


def hold_constants(admit, quiet_floor_us=500, hold_floor_us=2000, owner_limit_us=5000):
    """Quiet gap and hold bound of the plan rule from the admission statistics of one address."""
    quiet = max(quiet_floor_us, 2 * admit["burstGapMaxUs"])
    hold = max(hold_floor_us, 1.5 * admit["burstSpanMaxUs"], quiet)
    return {"quiet_gap_us": quiet, "hold_bound_us": hold, "owner_decision": quiet > owner_limit_us or hold > owner_limit_us}


def parse_stamps(path, start_line):
    """{record: [datetime, ...]} for the lines of a camonitor output after start_line."""
    result = {}
    for line in read_text(path).splitlines()[start_line:]:
        parsed = parse_line(line)
        if parsed:
            result.setdefault(parsed[0], []).append(parsed[1])
    return result


def alarms(env, names):
    """Per record (SEVR, STAT, AMSG) read after a pass."""
    if not names:
        return []

    def read(field):
        done = subprocess.run([str(CA / "caget"), "-t", "-w", "10", *[f"{n}.{field}" for n in names]],
                              env=env, capture_output=True, text=True, timeout=120, preexec_fn=die_with_parent)
        lines = done.stdout.split("\n")
        if lines and lines[-1] == "":
            lines.pop()
        return lines

    columns = [read("SEVR"), read("STAT"), read("AMSG")]
    if len({len(column) for column in columns}) != 1 or len(columns[0]) != len(names):
        raise RuntimeError("alarm readback incomplete: " + ", ".join(str(len(c)) for c in columns))
    return list(zip(*columns))


def stop_reason(read, lost, requests):
    if read:
        return "read-status"
    if lost >= 2 and lost * 1000 > requests * 5:
        return "loss-above-0.5-percent"
    return None


def classify(rows, requests, chunk_of=None):
    """The stop rules as the module shows them after one pass (state read back from the records). chunk_of maps
    a record position to its request (a group in --groups on), so that one lost group request counts once."""
    read = sum(1 for sev, stat, msg in rows if stat == "READ")
    lost_records = [i for i, (sev, stat, msg) in enumerate(rows) if stat == "COMM" and not msg]
    lost = len({chunk_of[i] for i in lost_records}) if chunk_of else len(lost_records)
    unsent = sum(1 for sev, stat, msg in rows if "deadline before send" in msg)
    other = sum(1 for sev, stat, msg in rows if sev != "NO_ALARM" and stat != "UDF") - read - len(lost_records) \
        - unsent
    return {"records": len(rows), "requests": requests, "read": read, "lost": lost, "unsent": unsent,
            "other_alarms": max(other, 0), "stop": stop_reason(read, lost, requests), "invalid": unsent > 0}


class Watch:
    """Counts, from the monitors of every record's value (posted at every processing) and message, the requests
    processed, the READ and COMM statuses and the unsent messages after a start line of the monitor file. With
    chunk_of (--groups on) a request is a group: its events within one 2 s bucket count once."""

    def __init__(self, path, start_line, chunk_of):
        self.path = path
        lines = read_text(path).splitlines(keepends=True)[:start_line]
        self.offset = sum(len(line.encode()) for line in lines)
        self.chunk_of = chunk_of
        self.processed_records = 0
        self.processed_groups = set()
        self.read = 0
        self.comm_records = 0
        self.comm_groups = set()
        self.unsent_records = 0
        self.unsent_groups = set()
        self.starts = {}

    def key(self, name, stamp):
        return (self.chunk_of[int(name[1:].split(".")[0])], int(stamp.timestamp()) // 2)

    def poll(self):
        with open(self.path, "rb") as stream:
            stream.seek(self.offset)
            data = stream.read()
        end = data.rfind(b"\n") + 1
        self.offset += end
        for line in data[:end].decode(errors="replace").splitlines():
            parsed = parse_line(line)
            if not parsed:
                continue
            name, stamp, tokens = parsed
            if name.startswith("O") and name.endswith(".AMSG"):
                if "deadline before send" in " ".join(tokens):
                    self.unsent_records += 1
                    if self.chunk_of:
                        self.unsent_groups.add(self.key(name, stamp))
            elif name.startswith("O") and "." not in name:
                # camonitor prints the value, then the alarm status and severity when there is an alarm.
                self.processed_records += 1
                if self.chunk_of:
                    self.processed_groups.add(self.key(name, stamp))
                status = tokens[-2] if len(tokens) >= 2 and tokens[-1] in ALARM_SEVERITY else None
                if status == "READ":
                    self.read += 1
                elif status == "COMM":
                    self.comm_records += 1
                    if self.chunk_of:
                        self.comm_groups.add(self.key(name, stamp))
            elif name.startswith("S_") and "." not in name:
                self.starts[name[2:]] = self.starts.get(name[2:], 0) + 1

    @property
    def processed(self):
        return len(self.processed_groups) if self.chunk_of else self.processed_records

    @property
    def unsent(self):
        return len(self.unsent_groups) if self.chunk_of else self.unsent_records

    def lost_requests(self):
        # A COMM processing that carries the unsent message is not a lost request.
        base = len(self.comm_groups) if self.chunk_of else self.comm_records
        return max(base - self.unsent, 0)

    def verdict(self):
        """(reason, invalid): the first stop rule that holds."""
        reason = stop_reason(self.read, self.lost_requests(), max(self.processed, 1))
        return reason, self.unsent > 0

    def numbers(self):
        return {"processed": self.processed, "read": self.read, "lost": self.lost_requests(),
                "unsent": self.unsent}


def probe_command(probe, seconds):
    return [str(probe["binary"]), "probefor", probe["peer"], probe["user"], probe["auth"], probe["priv"],
            str(probe["authfile"]), str(probe["privfile"]), str(probe["oidfile"]), str(probe["timeout_ms"]), "1",
            str(seconds)]


def probe_start(probe, seconds):
    return popen(probe_command(probe, seconds), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)


def probe_finish(process, seconds):
    try:
        out, _ = process.communicate(timeout=seconds + 120)
    except subprocess.TimeoutExpired:
        process.kill()
        out, _ = process.communicate()
    for line in reversed(out.splitlines()):
        try:
            return json.loads(line)
        except ValueError:
            continue
    return None


def probe_abort(process):
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def direct_pass(direct, passes, k, tiers_text):
    """device_sweep sweep: the direct sequential pass with the same K and the same tier cut; its total_ms rows.
    The pass is sound when device_sweep ended normally with all passes and no lost request or error status."""
    argv = [str(direct["binary"]), "sweep", direct["peer"], direct["user"], direct["auth"], direct["priv"],
            str(direct["authfile"]), str(direct["privfile"]), str(direct["oidfile"]), str(direct["timeout_ms"]),
            str(passes), str(k), "tiers=" + tiers_text]
    done = subprocess.run(argv, capture_output=True, text=True, timeout=3600, preexec_fn=die_with_parent)
    rows = []
    for line in done.stdout.splitlines():
        try:
            row = json.loads(line)
        except ValueError:
            continue
        if row.get("k") == k and "total_ms" in row and row.get("tiers") == tiers_text:
            rows.append(row)
    lost = sum(r.get("lost", 0) for r in rows)
    errors = sum(r.get("error_status", 0) for r in rows)
    sound = done.returncode == 0 and len(rows) == passes and lost == 0 and errors == 0
    return {"returncode": done.returncode, "rows": rows, "pass_ms": [r["total_ms"] for r in rows],
            "lost": lost, "error_status": errors, "sound": sound}


def pass_statistics(stamps, markers, tier_end):
    """(pass_ms, tier_ms, chunk_ms, chunks_done, finished) of one pass from the stamps of its markers."""
    begin = stamps.get("Pass_Start", [None])[0]
    tiers = {}
    for tier, marker_name in tier_end.items():
        seen = stamps.get(marker_name, [])
        if begin and seen:
            tiers[tier] = (seen[-1] - begin).total_seconds() * 1000
    last = stamps.get(markers[-1], [None])[-1]
    pass_ms = (last - begin).total_seconds() * 1000 if begin and last else None
    chunk_ms, previous = [], begin
    for marker_name in markers:
        seen = stamps.get(marker_name, [])
        if seen and previous:
            chunk_ms.append((seen[-1] - previous).total_seconds() * 1000)
            previous = seen[-1]
    done = sum(1 for m in markers if m in stamps)
    return pass_ms, tiers, chunk_ms, done, pass_ms is not None and done == len(markers)


def end_run(result, reason, invalid):
    result["stopped"] = reason
    result["invalid"] = result["invalid"] or bool(invalid)
    result["ended_early"] = "unsent" if invalid and not reason else reason


def counter_check(path, start_line, counters, stamps):
    """Problems found in the chunk counters of one pass (--groups on), [] when the counters are sound.

    counters is [(counter record, [member records], count, end marker)]: the counter must count 1..count once
    each (the reset to 0 may also be posted), every member must complete no later than the last count of its
    chunk, the end marker of the chunk must not be posted before that last count, and no member of the next
    chunk may complete before it (so the next chunk starts after the end)."""
    values = {name: [] for name, _, _, _ in counters}
    for line in read_text(path).splitlines()[start_line:]:
        parsed = parse_line(line)
        if parsed and parsed[0] in values and parsed[2]:
            try:
                values[parsed[0]].append((parsed[1], int(float(parsed[2][0]))))
            except ValueError:
                values[parsed[0]].append((parsed[1], None))
    problems, previous_end = [], None
    for name, members, count, done in counters:
        counted = [(stamp, value) for stamp, value in values[name] if value != 0]
        if [value for _, value in counted] != list(range(1, count + 1)):
            problems.append(f"{name}: counts {[v for _, v in counted]} instead of 1..{count}")
            previous_end = None
            continue
        end = counted[-1][0]
        ended = stamps.get(done, [])
        if not ended or ended[-1] < end:
            problems.append(f"{name}: the end marker {done} was posted before the last count")
        seen = [stamp for member in members for stamp in stamps.get(member, [])]
        if len(seen) != count:
            problems.append(f"{name}: {len(seen)} member completions instead of {count}")
        elif max(seen) > end:
            problems.append(f"{name}: a member completed after the last count")
        if previous_end is not None and seen and min(seen) < previous_end:
            problems.append(f"{name}: a member completed before the previous chunk ended")
        previous_end = end
    return problems


def run_once(case, env, record_names, markers, tier_end, args, requests, rest, chunk_of, result, ioc,
             counters=None):
    for index in range(args.passes):
        baseline = read_text(case / "monitor.out").count("\n")
        watch = Watch(case / "monitor.out", baseline, chunk_of)
        started = time.monotonic()
        putter = popen([str(CA / "caput"), "-c", "-w", str(args.pass_wait), "Pass_Trig.PROC", "1"], env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        reason = invalid = None
        while putter.poll() is None:
            time.sleep(0.5)
            if not ioc.alive():
                putter.kill()
                raise RuntimeError("the IOC exited during the pass")
            watch.poll()
            reason, invalid = watch.verdict()
            if reason or invalid:
                putter.kill()
                putter.wait()
                break
        time.sleep(0.3)
        watch.poll()
        if not (reason or invalid):
            reason, invalid = watch.verdict()
        stamps = parse_stamps(case / "monitor.out", baseline)
        pass_ms, tiers, chunk_ms, chunks_done, finished = pass_statistics(stamps, markers, tier_end)
        row = {"pass_ms": pass_ms, "tier_ms": tiers, "chunk_ms": chunk_ms, "chunks_done": chunks_done,
               "finished": finished, "caput_returncode": putter.returncode,
               "wall_ms": (time.monotonic() - started) * 1000, "watch": watch.numbers()}
        if counters:
            row["counter_problems"] = counter_check(case / "monitor.out", baseline, counters, stamps)
        result["passes"].append(row)
        if reason or invalid:
            # The numbers above were taken before the IOC is closed; its shutdown is not part of the run.
            end_run(result, reason, invalid)
            ioc.close()
            return
        if not finished:
            # caput -c returns 0 on a callback timeout: the pass is judged by its markers. The IOC is closed
            # first, so that its chain stops reading the device before anything else is read back.
            end_run(result, "pass-did-not-finish", False)
            ioc.close()
            return
        row["alarms"] = classify(alarms(env, record_names), requests, chunk_of)
        time.sleep(rest)


def pair_cycles(starts, finishes):
    """Each completion belongs to the earliest start that has none yet (cycles are served in order); a start
    without a completion is an incomplete cycle, and a cycle still running when the next one is due is an
    overrun. Returns (times in ms, overruns, incomplete, unmatched completions)."""
    times, overruns, incomplete, paired = [], 0, 0, 0
    for number, begin in enumerate(starts):
        nxt = starts[number + 1] if number + 1 < len(starts) else None
        candidates = [f for f in finishes[paired:] if f >= begin]
        if candidates:
            finish = candidates[0]
            paired = finishes.index(finish, paired) + 1
            times.append((finish - begin).total_seconds() * 1000)
            if nxt and finish > nxt:
                overruns += 1
        else:
            incomplete += 1
            if nxt:
                overruns += 1
    return times, overruns, incomplete, len(finishes) - paired


def schedule_block(case, baseline, watch, rows, chunks, tier_end, args, ran_seconds):
    """The numbers of the sustained run from the monitor file as it is now."""
    stamps = parse_stamps(case / "monitor.out", baseline)
    per_tier = {tier: sum(1 for r in rows if r["tier"] == tier) if args.groups == "off" else
                sum(1 for t, _ in chunks if t == tier) for tier in TIERS}
    cycles = {}
    for tier in TIERS:
        starts = stamps.get(f"S_{tier}", [])
        finishes = stamps.get(tier_end.get(tier, ""), [])
        if not starts:
            continue
        times, overruns, incomplete, unmatched = pair_cycles(starts, finishes)
        cycles[tier] = {"period_s": PERIODS[tier], "cycles": len(starts), "completed": len(times),
                        "incomplete": incomplete, "unmatched_completions": unmatched,
                        "objects": sum(1 for r in rows if r["tier"] == tier),
                        "p50_ms": statistics.median(times) if times else None,
                        "max_ms": max(times) if times else None, "overruns": overruns}
    lost = watch.lost_requests()
    return {"seconds": args.seconds, "ran_seconds": round(ran_seconds, 1), "tiers": cycles,
            "requests": watch.processed,
            "requests_started": sum(c["cycles"] * per_tier[tier] for tier, c in cycles.items()),
            "lost": lost, "loss_percent": (100.0 * lost / watch.processed) if watch.processed else None,
            "read": watch.read, "unsent": watch.unsent, "overruns": sum(c["overruns"] for c in cycles.values())}


def run_sustained(case, env, rows, chunks, tier_end, record_names, args, requests, probe, idle_seconds, chunk_of,
                  result, ioc):
    sweep = None
    sampler = None
    try:
        if args.sample_seconds > 0:
            sampler = Sampler(ioc, case, args.sample_seconds)
            sampler.phase = "idle"
            sampler.start()
        idle_row = None
        if probe:
            # Idle phase: the second client alone, before the schedule (the gate holds every scan).
            idle_row = probe_finish(probe_start(probe, int(idle_seconds)), idle_seconds)
            result["second_client"] = {"idle": idle_row, "object": probe["oid"]}
        else:
            time.sleep(idle_seconds)
        if not ioc.alive():
            raise RuntimeError("the IOC exited during the idle phase")
        baseline = read_text(case / "monitor.out").count("\n")
        watch = Watch(case / "monitor.out", baseline, chunk_of)
        subprocess.run([str(CA / "caput"), "-w", "10", "Run_Gate", "0"], env=env, stdout=subprocess.DEVNULL,
                       check=True, preexec_fn=die_with_parent)
        opened = time.monotonic()
        if sampler:
            sampler.phase = "schedule"
        if probe:
            sweep = probe_start(probe, args.seconds)
        end = opened + args.seconds
        try:
            while time.monotonic() < end:
                time.sleep(min(2.0, max(0.2, end - time.monotonic())))
                if not ioc.alive():
                    raise RuntimeError("the IOC exited during the schedule")
                watch.poll()
                reason, invalid = watch.verdict()
                if reason or invalid:
                    # The numbers are taken now; the shutdown of the IOC completes its pending records with
                    # READ and COMM and is not part of the run.
                    end_run(result, reason, invalid)
                    result["schedule"] = schedule_block(case, baseline, watch, rows, chunks, tier_end, args,
                                                        time.monotonic() - opened)
                    probe_abort(sweep)
                    sweep = None
                    if sampler:
                        sampler.stop()
                    ioc.close()
                    return
        except Exception:
            # The cycles finished before the failure stay in the row.
            watch.poll()
            result["schedule"] = schedule_block(case, baseline, watch, rows, chunks, tier_end, args,
                                                time.monotonic() - opened)
            raise
        time.sleep(1.0)
        watch.poll()
        result["schedule"] = schedule_block(case, baseline, watch, rows, chunks, tier_end, args,
                                            time.monotonic() - opened)
        if watch.unsent:
            result["invalid"] = True
        subprocess.run([str(CA / "caput"), "-w", "10", "Run_Gate", "1"], env=env, stdout=subprocess.DEVNULL,
                       preexec_fn=die_with_parent)
        result["schedule"]["final_alarms"] = classify(alarms(env, record_names), requests, chunk_of)
        if sweep:
            run = probe_finish(sweep, args.seconds)
            sweep = None
            result["second_client"]["run"] = run
            if run and idle_row and idle_row.get("p95_ms"):
                result["second_client"]["p95_ratio"] = run["p95_ms"] / idle_row["p95_ms"]
    finally:
        probe_abort(sweep)
        if sampler:
            sampler.stop()
            result["resources"] = sampler.summary()


def run_tiers(case, env, rows, chunks, markers, tier_end, args, requests, rest, probe, idle_seconds, chunk_of,
              direct, k, tiers_text, ioc, result):
    names = ["Pass_Start"] + markers
    record_names = [f"O{i:03d}" for i in range(len(rows))]
    counters = None
    if args.groups == "on" and args.once:
        counters = [(f"C{position:03d}_Cnt", [f"O{i:03d}" for i in members], len(members), markers[position])
                    for position, (_, members) in enumerate(chunks)]
        names = names + [name for name, _, _, _ in counters]
    flags = [] if args.once else [f"S_{tier}" for tier in TIERS if tier in tier_end]
    # Every record's value (posted at every processing) and message are monitored for the stop rules.
    watch_pvs = record_names + [f"{n}.AMSG" for n in record_names]
    mon_out = (case / "monitor.out").open("wb")
    monitor = popen([str(CA / "camonitor"), "-w", "10", *(names if args.once else markers + flags), *watch_pvs],
                    env=env, stdout=mon_out, stderr=subprocess.DEVNULL)
    time.sleep(2.0)
    result.update({"passes": [], "stopped": None, "invalid": False, "ended_early": None})
    try:
        if args.once:
            run_once(case, env, record_names, markers, tier_end, args, requests, rest, chunk_of, result, ioc,
                     counters)
            if direct and not result["ended_early"]:
                time.sleep(max(rest, 3.0))
                result["direct"] = direct_pass(direct, args.passes, k, tiers_text)
                if not result["direct"]["sound"]:
                    end_run(result, "direct-pass-failed", False)
        else:
            run_sustained(case, env, rows, chunks, tier_end, record_names, args, requests, probe, idle_seconds,
                          chunk_of, result, ioc)
    finally:
        monitor.terminate()
        monitor.wait(timeout=5)
        mon_out.close()


def run_flat(case, env, names, sweeps, count, delay, result):
    mon_out = (case / "monitor.out").open("wb")
    monitor = popen([str(CA / "camonitor"), "-w", "5", *names], env=env, stdout=mon_out, stderr=subprocess.DEVNULL)
    time.sleep(2.0)
    result["sweeps"] = []
    try:
        for sweep in range(sweeps):
            baseline = read_text(case / "monitor.out").count("\n")
            trigger = datetime.now()
            subprocess.run([str(CA / "caput"), "-w", "5", "B_Trig.PROC", "1"], env=env, stdout=subprocess.DEVNULL,
                           check=True, preexec_fn=die_with_parent)
            end = time.monotonic() + max(60.0, count * (delay + 30) / 1000.0 * 3)
            while time.monotonic() < end:
                if read_text(case / "monitor.out").count("\n") - baseline >= count:
                    break
                time.sleep(0.05)
            time.sleep(0.3)
            stamps = sorted(s for lst in parse_stamps(case / "monitor.out", baseline).values() for s in lst)
            alarmed = sum(1 for line in read_text(case / "monitor.out").splitlines()[baseline:]
                          if len(line.split()) > 4)
            if len(stamps) >= 2:
                span = (stamps[-1] - stamps[0]).total_seconds() * 1000
                result["sweeps"].append({"updates": len(stamps), "alarmed": alarmed,
                                         "first_ms": (stamps[0] - trigger).total_seconds() * 1000,
                                         "span_ms": span, "total_ms": (stamps[-1] - trigger).total_seconds() * 1000,
                                         "interval_ms": span / (len(stamps) - 1)})
            else:
                result["sweeps"].append({"updates": len(stamps), "alarmed": alarmed})
            time.sleep(1.0)
    finally:
        monitor.terminate()
        monitor.wait(timeout=5)
        mon_out.close()


def startup_error(case, error):
    lines = []
    for name in ("ioc.stderr", "ioc.stdout"):
        lines += read_text(case / name).splitlines()
    found = [line.strip() for line in lines if "ERROR" in line or "rror" in line or "snmp3:" in line][:4]
    return str(error) + ": " + " | ".join(found or lines[-3:])


def summarize_once(result, rows):
    """Valid passes and their statistics; also called when the case ended with an error."""
    good = [p["pass_ms"] for p in result.get("passes", [])
            if p.get("finished") and p.get("alarms") and not p.get("counter_problems")
            and not p["alarms"]["lost"]
            and not p["alarms"]["unsent"] and not p["alarms"]["read"]]
    result["valid_passes"] = len(good)
    if good:
        result["pass_p50_ms"] = statistics.median(good)
        result["pass_min_ms"] = min(good)
        result["pass_max_ms"] = max(good)
        result["per_record_ms"] = result["pass_p50_ms"] / len(rows)
        direct_row = result.get("direct")
        if direct_row and direct_row["sound"]:
            result["direct_p50_ms"] = statistics.median(direct_row["pass_ms"])
            result["module_to_direct_ratio"] = result["pass_p50_ms"] / result["direct_p50_ms"]


def run_case(bench, args, rows, label, agent_port, version, delay, target=None):
    case = bench.output / label
    case.mkdir()
    shape = args.shape
    tiers = TIERS if args.tier == "all" else (args.tier,)
    k = args.k
    # The flat shape reproduces run_fixture_module.py: timeoutMs 5000 and no maxVarbinds.
    timeout_ms = args.timeout_ms if args.timeout_ms is not None else (5000 if shape == "flat" else 3000)
    max_varbinds = args.max_varbinds if args.max_varbinds is not None else (0 if shape == "flat" else 32)
    idle_seconds = args.idle_seconds
    result = {"label": label, "shape": shape, "groups": args.groups, "version": version if not target else "3",
              "delay_ms": delay, "tier": args.tier,
              "mode": "flat" if shape == "flat" else ("once" if args.once else "sustained"),
              "fixture_objects_read_as_integer": not target, "timeout_ms": timeout_ms,
              "max_varbinds": max_varbinds, "authorization": args.confirm_authorized or None,
              "started": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    ioc = proxy = None
    try:
        if target:
            address, port = target["address"], int(target.get("port", 161))
            k = int(target.get("group_size", k))
            idle_seconds = float(target.get("idle_seconds", idle_seconds))
            result["idle_seconds_from_target"] = "idle_seconds" in target
            result["group_size_from_target"] = "group_size" in target
            for key in ("model", "card", "firmware"):
                result[key] = target.get(key)
            secrets = {"auth": target["authSecretFile"], "priv": target["privSecretFile"], "community": ""}
            auth, priv = target.get("authAlgorithm", "SHA"), target.get("privAlgorithm", "AES")
            profile = profile_for("3", secrets, timeout_ms, 0, max_varbinds, target["user"], auth, priv)
            user = target["user"]
            peer = f"udp:{address}:{port}"
            files = (target["authSecretFile"], target["privSecretFile"])
        else:
            address = "127.0.0.1"
            port, proxy = bench.proxy(agent_port, delay)
            profile = profile_for(version, bench.paths, timeout_ms, 0, max_varbinds)
            user, auth, priv = USER, "SHA", "AES"
            peer = f"udp:127.0.0.1:{port}"
            files = (bench.paths["auth"], bench.paths["priv"])
        if k < 1:
            raise ValueError("the group size must be at least 1")
        if args.probe_binary and not args.once and idle_seconds > PROBE_SAMPLE_LIMIT - 24:
            raise ValueError(f"the idle phase of the second client is at most {PROBE_SAMPLE_LIMIT - 24} s")
        result["k"] = k
        markers, tier_end, chunks, names = [], {}, [], []
        if shape == "flat":
            text, names = flat_database(args.n, args.deadline_ms)
        else:
            text, markers, tier_end, chunks = tier_database(rows, k, args.deadline_ms, args.once, tiers, args.groups)
        (case / "records.db").write_text(text)
        config = case / "config.json"
        config.write_text(json.dumps(configuration(rows, shape, args.groups, chunks, profile, address, port,
                                                   not target), indent=1))
        (case / "startup.cmd").write_text(
            'on error break\n'
            f'dbLoadDatabase("{ROOT}/dbd/snmp3Ioc.dbd")\n'
            'snmp3Ioc_registerRecordDeviceDriver(pdbbase)\n'
            f'snmp3Load("{config}","{PRODUCTS}/snmp3NativeProbe")\n'
            f'snmp3WorkerPath("{PRODUCTS}/snmp3Worker")\n'
            f'dbLoadRecords("{case}/records.db")\n'
            'iocInit\n')
        # The objects in tier order (state, meas, slow; list order inside a tier): the object file of the direct
        # pass, so that its tier cut holds the same objects as the chunks of the module.
        ordered = [index for tier in tiers for index, row in enumerate(rows) if row["tier"] == tier]
        ordered_oids = [FIXTURE_OID[rows[i]["value_type"]] if not target else rows[i]["oid"] for i in ordered]
        tiers_text = ",".join(str(sum(1 for r in rows if r["tier"] == t)) for t in tiers
                              if any(r["tier"] == t for r in rows))
        oidfile = case / "oids-by-tier.txt"
        if shape == "tiers":
            oidfile.write_text("\n".join(ordered_oids) + "\n")
        probe = direct = None
        if args.probe_binary and shape == "tiers" and not args.once:
            probe_file = case / "probe-oid.txt"
            probe_oid = args.probe_oid or (SYS_UPTIME if target else FIXTURE_OID["integer"])
            probe_file.write_text(probe_oid + "\n")
            probe = {"binary": args.probe_binary, "peer": peer, "user": user, "auth": auth, "priv": priv,
                     "authfile": files[0], "privfile": files[1], "oidfile": probe_file,
                     "timeout_ms": timeout_ms, "oid": probe_oid}
        if args.direct_binary and shape == "tiers" and args.once:
            direct = {"binary": args.direct_binary, "peer": peer, "user": user, "auth": auth, "priv": priv,
                      "authfile": files[0], "privfile": files[1], "oidfile": oidfile, "timeout_ms": timeout_ms}
        # record position -> the request it belongs to: a group in --groups on, the record itself otherwise
        chunk_of = None
        if args.groups == "on" and chunks:
            chunk_of = {}
            for position, (_, members) in enumerate(chunks):
                for member in members:
                    chunk_of[member] = position
        env = environment()
        ioc = Ioc(case, env, case / "startup.cmd")
        try:
            ioc.wait_ready()
        except RuntimeError as error:
            result["error"] = startup_error(case, error)
            return result
        before = ioc.report()
        if shape == "flat":
            run_flat(case, env, names, args.sweeps, args.n, delay, result)
            result["n"] = args.n
        else:
            requests = len(rows) if args.groups == "off" else len(chunks)
            rest = max(args.pass_rest, 3.0) if target else args.pass_rest
            run_tiers(case, env, rows, chunks, markers, tier_end, args, requests, rest, probe, idle_seconds,
                      chunk_of, direct, k, tiers_text, ioc, result)
            result["objects"] = len(rows)
            result["chunks"] = len(chunks)
            result["requests_per_pass"] = requests
        result["loop_passes"] = {"before": before, "after": ioc.report()}
        result["admission"] = [dict(row, **hold_constants(row)) for row in ioc.admission]
    except Exception as error:  # noqa: BLE001 - the case row records the failure instead of losing the document
        result["error"] = f"{type(error).__name__}: {error}"
    finally:
        if shape == "tiers" and args.once and "passes" in result:
            summarize_once(result, rows)
        if ioc:
            ioc.close()
            result["ioc_returncode"] = ioc.process.returncode
        if proxy:
            bench.stop(proxy)
    return result


def run_environment(args):
    """Facts of the run that the procedure asks to be recorded with every result. The tree flag looks at the
    product sources only (snmp3App, configure, Makefile); the hashes are those of the products the run used."""
    def command(argv):
        try:
            done = subprocess.run(argv, capture_output=True, text=True, timeout=10)
            return done.stdout.strip() or None
        except (OSError, subprocess.SubprocessError):
            return None

    def digest(path):
        try:
            return hashlib.sha256(Path(path).read_bytes()).hexdigest()
        except OSError:
            return None

    commit = command(["git", "-C", str(ROOT), "rev-parse", "HEAD"])
    dirty = command(["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=no", "--",
                     "snmp3App", "configure", "Makefile"])
    return {"date": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "tool_sha256": digest(Path(__file__)),
            "module_commit": commit, "module_product_sources_modified": bool(dirty),
            "product_sha256": {name: digest(PRODUCTS / name) for name in ("snmp3Ioc", "snmp3Worker")},
            "net_snmp": command(["net-snmp-config", "--version"]),
            "kernel": command(["uname", "-r"]),
            "epics_base": str(BASE),
            "objects_file": str(args.objects), "shape": args.shape,
            "mode": "flat" if args.shape == "flat" else ("once" if args.once else "sustained"),
            "groups": args.groups, "tier": args.tier, "k": args.k, "passes": args.passes,
            "seconds": args.seconds, "deadline_ms": args.deadline_ms, "pass_wait": args.pass_wait,
            "pass_rest": args.pass_rest, "device_rest": args.device_rest,
            "targets": str(args.targets) if args.targets else None,
            "fixture": not args.targets, "authorization": args.confirm_authorized or None}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, required=True,
                        help="a directory that does not exist yet; receives module-results.json")
    parser.add_argument("--objects", type=Path, default=DEFAULT_OBJECTS)
    parser.add_argument("--shape", choices=("tiers", "flat"), default="tiers")
    parser.add_argument("--once", action="store_true",
                        help="one chained pass per measurement (tiers shape); otherwise the periodic schedule")
    parser.add_argument("--groups", choices=("off", "on"), default="off",
                        help="off: no group field (the only mode before the grouped release)")
    parser.add_argument("--tier", choices=("all",) + TIERS, default="all")
    parser.add_argument("--k", type=int, default=28, help="objects per chunk (group_size of a target wins)")
    parser.add_argument("--passes", type=int, default=5)
    parser.add_argument("--seconds", type=int, default=900, help="duration of the periodic schedule")
    parser.add_argument("--delays", default="0,20,100", help="fixture: added round trip in ms")
    parser.add_argument("--versions", default="3")
    parser.add_argument("--n", type=int, default=100, help="records of the flat shape")
    parser.add_argument("--sweeps", type=int, default=3, help="repeats of the flat shape")
    parser.add_argument("--deadline-ms", type=int, default=60000)
    parser.add_argument("--timeout-ms", type=int, default=None,
                        help="profile timeoutMs (default 3000; 5000 for the flat shape)")
    parser.add_argument("--max-varbinds", type=int, default=None,
                        help="profile maxVarbinds (default 32; none for the flat shape)")
    parser.add_argument("--octets-capacity", type=int, default=64,
                        help="capacity of an octets binding: a longer value is a READ status and stops the run")
    parser.add_argument("--pass-rest", type=float, default=1.0,
                        help="seconds between passes (a device run uses at least 3)")
    parser.add_argument("--device-rest", type=float, default=10.0, help="seconds between devices")
    parser.add_argument("--pass-wait", type=int, default=120, help="seconds the caput callback may take")
    parser.add_argument("--probe-binary", type=Path,
                        help="device_sweep binary: the second client of the sustained schedule (probefor)")
    parser.add_argument("--probe-oid", default=None,
                        help="object of the second client (default sysUpTime on devices, as phase 4 of "
                             "run_device_phases.py; the served integer on the fixture)")
    parser.add_argument("--sample-seconds", type=float, default=0.0,
                        help="sustained run: sample the resources of the IOC and its workers every N seconds "
                             "into resources.csv and counters.csv of the case (0 = off)")
    parser.add_argument("--idle-seconds", type=float, default=60.0,
                        help="idle phase before the sustained schedule (a target may set idle_seconds)")
    parser.add_argument("--direct-binary", type=Path,
                        help="device_sweep binary: with --once, the direct sequential pass of the same session")
    parser.add_argument("--targets", type=Path, help="devices (targets file of test-procedure.md); read-only GET")
    parser.add_argument("--confirm-authorized", default="",
                        help="required with --targets: who authorized this equipment run and when (recorded)")
    args = parser.parse_args()
    if not os.environ.get("EPICS_BASE"):
        raise SystemExit("EPICS_BASE must name the EPICS Base 7.0.10 install (caget, caput, camonitor, caRepeater)")
    if args.k < 1 or args.passes < 1 or args.octets_capacity < 1:
        raise SystemExit("--k, --passes and --octets-capacity must be at least 1")
    if args.probe_binary and not args.once and args.seconds > PROBE_SAMPLE_LIMIT - 24:
        raise SystemExit(f"the second client records at most {PROBE_SAMPLE_LIMIT} samples: use --seconds "
                         f"{PROBE_SAMPLE_LIMIT - 24} or less")
    OCTETS_CAPACITY[0] = args.octets_capacity
    if args.shape == "flat" and args.targets:
        raise SystemExit("the flat shape runs on the fixture only")
    if args.targets and not args.confirm_authorized:
        raise SystemExit("a device run needs the owner's authorization and a person present: "
                         "state who authorized it and when with --confirm-authorized <text>")
    rows = read_objects(args.objects) if args.shape == "tiers" else []
    if not args.targets:
        # The fixture agent serves one integer object for the whole list (its octets object holds binary
        # bytes that a string record rejects), so every object is read as that integer; tiers, order and
        # count are those of the list.
        rows = [dict(row, value_type="integer") for row in rows]
    if args.shape == "tiers" and args.tier != "all":
        # A single tier runs alone: the database holds that tier's objects only.
        rows = [row for row in rows if row["tier"] == args.tier]
    if args.shape == "tiers" and not rows:
        raise SystemExit("the object list holds no object" + ("" if args.tier == "all" else f" of tier {args.tier}"))
    bench = Bench(args.output)
    results = []
    environment_record = run_environment(args)
    try:
        if args.targets:
            targets = json.loads(args.targets.read_text())["targets"]
            for number, target in enumerate(targets):
                if number:
                    time.sleep(args.device_rest)
                row = run_case(bench, args, rows, f"device-{target['label']}", None, "3", 0, target)
                results.append(row)
                print(json.dumps(row), flush=True)
                if row.get("ended_early") or row.get("error"):
                    break
        else:
            agent_port = bench.agent()
            done = False
            for version in args.versions.split(","):
                for delay in [int(d) for d in args.delays.split(",")]:
                    row = run_case(bench, args, rows, f"v{version}-d{delay}", agent_port, version, delay)
                    results.append(row)
                    print(json.dumps(row), flush=True)
                    if row.get("ended_early") or row.get("error"):
                        done = True
                        break
                if done:
                    break
    finally:
        bench.close()
        document = {"description": "snmp3 module database run (run_module_database.py)",
                    "environment": environment_record, "cases": results}
        (args.output / "module-results.json").write_text(json.dumps(document, indent=1) + "\n")
    if any(row.get("error") for row in results):
        sys.exit(1)
    if any(row.get("stopped") for row in results):
        sys.exit(10)
    if any(row.get("invalid") for row in results):
        sys.exit(12)


main()
