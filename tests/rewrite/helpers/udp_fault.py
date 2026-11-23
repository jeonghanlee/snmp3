#!/usr/bin/env python3
"""Apply faults only to actual loopback UDP messages; retain redacted metadata.

A worker session reads snmpEngineMaxMessageSize.0 of the unit when it opens, with the first requests after the discovery
of the client. That exchange is not an application request. Every mode lets it through except drop-size, which drops only
the responses of that exchange: a mode that loses the responses of the application requests (drop, drop-all) keeps modelling
that, and the read does not hold the request back by waiting for a lost response. Its requests and responses are the events
fault_size_read_request and fault_size_read_response, a response it drops is the event fault_size_read_dropped, and none of
them is in the counts of the application requests and responses (fault_request, fault_response, fault_dropped) or in the
counters requests and responses of fault_stopped, which are as they are without a read.
The exchange is the first non-discovery request of a client and its repetitions, up to --read-attempts requests (the
request and its retries: a request that is sent again has a new message ID in SNMPv3), and it ends at the first response
delivered to the client that is not a REPORT which makes an SNMPv3 session send the request again (a REPORT that sets the
clock or names the engine ID; after a wrong digest it does not). A response belongs to it when it answers one of those
requests: the message ID of SNMPv3 and the request ID of the other versions are the identity, because an SNMPv3 request is
encrypted and shows no object.
"""

import argparse
import heapq
import itertools
import json
import os
from pathlib import Path
import selectors
import signal
import socket
import struct
import time

RUNNING = True
REPORT_PREFIX = bytes([0x2b, 6, 1, 6, 3, 15, 1, 1])
# The REPORT causes after which an SNMPv3 session sends its request again: the clock is set, or the engine ID is known.
RESEND_CAUSES = (2, 4)
MODES = ("pass", "drop", "drop-all", "drop-size", "delay", "duplicate", "reorder", "missing", "extra", "duplicate-varbind",
         "malformed", "wrong-request-id", "foreign-report", "drop-protected", "controlled")
CONTROL_MODES = ("pass", "drop-all", "drop-protected", "duplicate", "error-status", "too-big", "bad-error-index",
                 "no-such-object", "no-such-instance", "end-of-mib", "wrong-type", "numeric-seed")


def exchange_key(metadata):
    """The identity that a request and its response share: the message ID of SNMPv3, else the request ID."""
    return metadata.get("message_id") if metadata.get("version") == 3 else metadata.get("request_id")


def stop(_signal, _frame):
    global RUNNING
    RUNNING = False


def emit(event, **fields):
    print(json.dumps({"event": event, "monotonic_ns": time.monotonic_ns(), **fields}), flush=True)


def tlv(data, position=0):
    start = position
    if position + 2 > len(data):
        raise ValueError("truncated TLV")
    tag, length = data[position:position + 2]
    position += 2
    if length & 128:
        count = length & 127
        if not count or count > 4 or position + count > len(data):
            raise ValueError("invalid TLV length")
        length = int.from_bytes(data[position:position + count], "big")
        position += count
    end = position + length
    if end > len(data):
        raise ValueError("truncated TLV body")
    return {"tag": tag, "body": data[position:end], "raw": data[start:end]}, end


def items(data):
    result, position = [], 0
    while position < len(data):
        node, position = tlv(data, position)
        result.append(node)
    return result


def encode(tag, body):
    length = len(body)
    if length < 128:
        size = bytes([length])
    else:
        number = length.to_bytes((length.bit_length() + 7) // 8, "big")
        size = bytes([128 | len(number)]) + number
    return bytes([tag]) + size + body


def integer(node):
    if node["tag"] != 2 or not node["body"]:
        raise ValueError("invalid integer TLV")
    return int.from_bytes(node["body"], "big", signed=True)


def parse(data):
    root, end = tlv(data)
    if root["tag"] != 48 or end != len(data):
        raise ValueError("invalid message sequence")
    outer = items(root["body"])
    version = integer(outer[0])
    metadata = {"version": version, "length": len(data), "discovery": False}
    if version == 3:
        header = items(outer[1]["body"])
        if header[2]["tag"] != 4 or len(header[2]["body"]) != 1:
            raise ValueError("invalid SNMPv3 flags")
        security_root, _ = tlv(outer[2]["body"])
        security = items(security_root["body"])
        metadata.update(message_id=integer(header[0]), max_size=integer(header[1]), engine_length=len(security[0]["body"]),
                        boots=integer(security[1]), engine_time=integer(security[2]),
                        user_length=len(security[3]["body"]), discovery=not security[3]["body"],
                        flags=header[2]["body"][0])
        if outer[3]["tag"] != 48:
            metadata["encrypted"] = True
            return outer, None, None, metadata
        scoped = items(outer[3]["body"])
        pdu = scoped[2]
    else:
        pdu = outer[2]
    fields = items(pdu["body"])
    variables = items(fields[3]["body"])
    metadata.update(command=pdu["tag"], request_id=integer(fields[0]), status=integer(fields[1]),
                    index=integer(fields[2]), varbinds=len(variables))
    metadata["variable_metadata"] = [{"type": pair[1]["tag"], "length": len(pair[1]["body"])}
                                     for pair in (items(v["body"]) for v in variables)]
    if pdu["tag"] == 168 and variables:
        # A REPORT names its cause by an object of usmStats (1.3.6.1.6.3.15.1.1.<cause>.0): 2 notInTimeWindows, 4
        # unknownEngineIDs, 5 wrongDigests.
        name = items(variables[0]["body"])[0]["body"]
        if len(name) == 10 and name[:8] == REPORT_PREFIX and name[9] == 0:
            metadata["report_cause"] = name[8]
    return outer, pdu, fields, metadata


def mutate(data, mode):
    outer, pdu, fields, _ = parse(data)
    if pdu is None or integer(outer[0]) == 3:
        raise ValueError("varbind faults require actual community responses")
    variables = items(fields[3]["body"])
    if mode in ("error-status", "too-big", "bad-error-index"):
        fields[1]["raw"] = encode(2, bytes([1 if mode == "too-big" else 5]))
        fields[2]["raw"] = encode(2, bytes([0 if mode == "too-big" else 2 if mode == "bad-error-index" else 1]))
    elif mode in ("no-such-object", "no-such-instance", "end-of-mib", "wrong-type"):
        tag = {"no-such-object": 128, "no-such-instance": 129, "end-of-mib": 130, "wrong-type": 5}[mode]
        changed = []
        for variable in variables:
            pair = items(variable["body"])
            changed.append(encode(48, pair[0]["raw"] + encode(tag, b"")))
        fields[3]["raw"] = encode(48, b"".join(changed))
    elif mode == "numeric-seed":
        if pdu["tag"] != 162 or integer(fields[1]) or len(variables) != 1:
            raise ValueError("numeric seed requires one successful response varbind")
        pair = items(variables[0]["body"])
        value = pair[1]
        if value["tag"] == 70:
            body = b"\x07"
        elif value["tag"] == 68 and value["body"][:3] == b"\x9f\x78\x04" and len(value["body"]) == 7:
            body = value["body"][:3] + struct.pack("!f", 7.0)
        elif value["tag"] == 68 and value["body"][:3] == b"\x9f\x79\x08" and len(value["body"]) == 11:
            body = value["body"][:3] + struct.pack("!d", 7.0)
        else:
            raise ValueError("numeric seed requires Counter64 or opaque float/double")
        emit("numeric_seed", request_id=integer(fields[0]), tag=value["tag"],
             original_hex=value["body"].hex(), seed_hex=body.hex())
        fields[3]["raw"] = encode(48, encode(48, pair[0]["raw"] + encode(value["tag"], body)))
    elif mode == "wrong-request-id":
        request = (integer(fields[0]) + 1) & 0x7FFFFFFF
        raw = request.to_bytes(max(1, (request.bit_length() + 8) // 8), "big")
        fields[0]["raw"] = encode(2, raw)
    else:
        if mode == "reorder":
            variables.reverse()
        elif mode == "missing":
            variables = variables[:-1]
        elif mode == "extra":
            variables.append(variables[0])
        elif mode == "duplicate-varbind":
            variables[-1] = variables[0]
        fields[3]["raw"] = encode(48, b"".join(v["raw"] for v in variables))
    outer[2]["raw"] = encode(pdu["tag"], b"".join(f["raw"] for f in fields))
    return encode(48, b"".join(f["raw"] for f in outer))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--family", type=int, choices=(4, 6), required=True)
    parser.add_argument("--agent-port", type=int, required=True)
    parser.add_argument("--mode", choices=MODES, required=True)
    parser.add_argument("--delay-ms", type=int, default=75)
    parser.add_argument("--read-attempts", type=int, default=2,
                        help="most requests of a client that belong to the size read: the request and its retries")
    args = parser.parse_args()
    family = socket.AF_INET if args.family == 4 else socket.AF_INET6
    host = "127.0.0.1" if args.family == 4 else "::1"
    if not 1024 <= args.agent_port <= 65535 or not 0 <= args.delay_ms <= 1000:
        parser.error("unprivileged port and bounded delay required")
    selector = selectors.DefaultSelector()
    listener = socket.socket(family, socket.SOCK_DGRAM)
    listener.bind((host, 0))
    listener.setblocking(False)
    selector.register(listener, selectors.EVENT_READ, None)
    upstream = {}
    pending = []
    serial = itertools.count()
    reports = {}
    mutated = set()
    size_reads = {}
    read_open = {}
    read_used = {}
    counts = {"requests": 0, "responses": 0, "application_requests": 0, "discovery_requests": 0, "size_read_requests": 0}
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    emit("fault_ready", port=listener.getsockname()[1], mode=args.mode)
    try:
        while RUNNING:
            now = time.monotonic_ns()
            while pending and pending[0][0] <= now:
                _, _, packet, client = heapq.heappop(pending)
                listener.sendto(packet, client)
                emit("fault_delayed_delivery", length=len(packet))
            timeout = min(0.05, max(0, (pending[0][0] - now) / 1e9)) if pending else 0.05
            for key, _ in selector.select(timeout):
                packet, source = key.fileobj.recvfrom(65535)
                try:
                    _, _, _, metadata = parse(packet)
                except (ValueError, IndexError):
                    metadata = {"parsed": False, "length": len(packet)}
                if key.fileobj is listener:
                    client = source
                    if source[0] != host:
                        continue
                    key = None if metadata.get("discovery") else exchange_key(metadata)
                    is_read = False
                    if key is not None and read_open.get(client, True):
                        read_used[client] = read_used.get(client, 0) + 1
                        size_reads.setdefault(client, set()).add(key)
                        if read_used[client] >= max(1, args.read_attempts):
                            read_open[client] = False
                    is_read = key is not None and key in size_reads.get(client, ())
                    if is_read:
                        # The read is not an application request: its own counter and events keep the counts and the
                        # events of the application requests as they are without a read.
                        counts["size_read_requests"] += 1
                        emit("fault_size_read_request", **metadata)
                    else:
                        counts["requests"] += 1
                        counts["discovery_requests" if metadata.get("discovery") else "application_requests"] += 1
                        emit("fault_request", **metadata)
                    if client not in upstream:
                        transport = socket.socket(family, socket.SOCK_DGRAM)
                        transport.bind((host, 0))
                        transport.connect((host, args.agent_port))
                        transport.setblocking(False)
                        upstream[client] = transport
                        selector.register(transport, selectors.EVENT_READ, client)
                    upstream[client].send(packet)
                else:
                    client = key.data
                    key = None if metadata.get("discovery") else exchange_key(metadata)
                    is_read = key is not None and key in size_reads.get(client, ())
                    if is_read:
                        emit("fault_size_read_response", **metadata)
                        if args.mode == "drop-size":
                            emit("fault_size_read_dropped", length=len(packet))
                        else:
                            listener.sendto(packet, client)
                            if metadata.get("report_cause") not in RESEND_CAUSES:
                                read_open[client] = False
                        continue
                    counts["responses"] += 1
                    emit("fault_response", **metadata)
                    if metadata.get("command") == 168 and metadata.get("discovery") and args.mode != "drop-all":
                        reports[client] = packet
                        reports["latest"] = packet
                        listener.sendto(packet, client)
                        continue
                    if args.mode == "drop-size":
                        listener.sendto(packet, client)
                        continue
                    mode = args.mode
                    if mode == "controlled":
                        mode = Path(os.environ["SNMP3_RECORD_FAULT_CONTROL"]).read_text().strip()
                        if mode not in CONTROL_MODES:
                            raise ValueError("invalid controlled response mode")
                        if mode not in ("pass", "drop-all", "drop-protected", "duplicate"):
                            packet = mutate(packet, mode)
                        emit("fault_controlled", mode=mode, **parse(packet)[3])
                        if mode not in ("drop-all", "drop-protected"):
                            if mode == "duplicate":
                                listener.sendto(packet, client)
                                emit("fault_duplicate", **parse(packet)[3])
                            listener.sendto(packet, client)
                            continue
                    if mode == "drop-protected":
                        if metadata.get("version") == 3 and metadata.get("flags", 0) & 3 == 3:
                            emit("fault_dropped", mode=mode, **metadata)
                        else:
                            listener.sendto(packet, client)
                        continue
                    if mode in ("drop", "drop-all"):
                        emit("fault_dropped", length=len(packet))
                        continue
                    if mode == "delay":
                        heapq.heappush(pending, (time.monotonic_ns() + args.delay_ms * 1000000,
                                                next(serial), packet, client))
                        continue
                    if client not in mutated and mode != "pass":
                        mutated.add(client)
                        if mode == "foreign-report":
                            report = reports.get(client, reports.get("latest"))
                            if report is None:
                                raise RuntimeError("actual discovery REPORT unavailable")
                            listener.sendto(report, client)
                            emit("fault_foreign_report", length=len(report))
                        elif mode == "duplicate":
                            listener.sendto(packet, client)
                            emit("fault_duplicate", length=len(packet))
                        elif mode == "malformed":
                            packet = packet[:-1]
                            emit("fault_mutation", mode=mode, length=len(packet))
                        else:
                            packet = mutate(packet, mode)
                            emit("fault_mutation", mode=mode, length=len(packet))
                    listener.sendto(packet, client)
    finally:
        selector.close()
        listener.close()
        for transport in upstream.values():
            transport.close()
        emit("fault_stopped", **counts, pending_deliveries=len(pending))


if __name__ == "__main__":
    main()
