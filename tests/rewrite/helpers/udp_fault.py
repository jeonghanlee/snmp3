#!/usr/bin/env python3
"""Apply faults only to actual loopback UDP messages; retain redacted metadata."""

import argparse
import heapq
import itertools
import json
import selectors
import signal
import socket
import time

RUNNING = True
MODES = ("pass", "drop", "drop-all", "delay", "duplicate", "reorder", "missing", "extra", "duplicate-varbind",
         "malformed", "wrong-request-id", "foreign-report")


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
        security_root, _ = tlv(outer[2]["body"])
        security = items(security_root["body"])
        metadata.update(message_id=integer(header[0]), engine_length=len(security[0]["body"]),
                        boots=integer(security[1]), engine_time=integer(security[2]),
                        user_length=len(security[3]["body"]), discovery=not security[3]["body"])
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
    return outer, pdu, fields, metadata


def mutate(data, mode):
    outer, pdu, fields, _ = parse(data)
    if pdu is None or integer(outer[0]) == 3:
        raise ValueError("varbind faults require actual community responses")
    variables = items(fields[3]["body"])
    if mode == "wrong-request-id":
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
    counts = {"requests": 0, "responses": 0, "application_requests": 0, "discovery_requests": 0}
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
                    counts["requests"] += 1
                    category = "discovery_requests" if metadata.get("discovery") else "application_requests"
                    counts[category] += 1
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
                    counts["responses"] += 1
                    emit("fault_response", **metadata)
                    if metadata.get("command") == 168 and metadata.get("discovery") and args.mode != "drop-all":
                        reports[client] = packet
                        reports["latest"] = packet
                        listener.sendto(packet, client)
                        continue
                    mode = args.mode
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
