#!/usr/bin/env python3
"""Forward real worker frames and mutate only the external UNIX socket boundary."""
import argparse
from collections import deque
import json
import select
import signal
import socket
import struct
import time

RUNNING = True
HEADER = struct.Struct("!IHHII6Q")
MAX_FRAME = 16777216 + HEADER.size
MODES = ("partial", "coalesced", "stale", "malformed", "truncated", "partial-timeout",
         "full-channel", "bad-set", "oversize", "bootstrap-mismatch", "bootstrap-secret", "ready-executable", "ready-library")


def stop(_signal, _frame):
    global RUNNING
    RUNNING = False


def emit(event, **fields):
    print(json.dumps({"event": event, "monotonic_ns": time.monotonic_ns(), **fields}), flush=True)


class Forwarder:
    def __init__(self, mode, parent, worker):
        self.mode = mode
        self.sockets = [socket.socket(fileno=parent), socket.socket(fileno=worker)]
        for transport in self.sockets:
            transport.setblocking(False)
        self.inputs = [bytearray(), bytearray()]
        self.outputs = [deque(), deque()]
        self.closed = [False, False]
        self.shutdown = [False, False]
        self.offsets = [0, 0]
        self.held = None
        self.changed = False
        self.frames = [0, 0]
        self.paused = False

    def enqueue(self, target, packet):
        size = sum(len(entry[0]) for entry in self.outputs[target])
        if size + len(packet) > 2 * MAX_FRAME:
            raise RuntimeError("external fault fixture buffer exceeded")
        now = time.monotonic()
        if self.mode == "partial":
            self.outputs[target].append([packet[:1], now])
            self.outputs[target].append([packet[1:], now + 0.025])
            emit("partial_frame", direction=target, length=len(packet))
        else:
            self.outputs[target].append([packet, now])

    def frame(self, direction, packet):
        fields = list(HEADER.unpack_from(packet))
        kind = fields[2]
        if fields[0] != 0x46534E35 or fields[1] != 1 or fields[4] or not 1 <= kind <= 8:
            raise RuntimeError("actual worker frame header invalid before injection")
        fields[10] += self.offsets[direction]
        packet = HEADER.pack(*fields) + packet[64:]
        self.frames[direction] += 1
        emit("actual_frame", direction=direction, kind=kind, length=len(packet),
             activation=fields[5], epoch=fields[6], batch=fields[9], sequence=fields[10])
        target = 1 - direction
        if direction == 0 and kind == 1 and self.mode == "bootstrap-secret":
            altered = bytearray(packet)
            position = 68
            def number():
                nonlocal position
                value = struct.unpack_from("!I", altered, position)[0]
                position += 4
                return value
            def text():
                nonlocal position
                length = number()
                position += length
            text()
            for _ in range(2):
                for _ in range(number()):
                    text()
                    position += 4
                    arcs = number()
                    position += arcs * 4
            if not number():
                raise RuntimeError("actual Bootstrap has no profile")
            position += 8
            text()
            position += 20
            for _ in range(4):
                text()
            if not number():
                raise RuntimeError("actual profile has no community")
            altered[position] = 0
            self.enqueue(target, altered)
            emit("bootstrap_secret_actual_frame")
            return
        if direction == 0 and kind == 1 and self.mode == "bootstrap-mismatch":
            altered = bytearray(packet)
            altered[72] ^= 1
            self.enqueue(target, altered)
            emit("bootstrap_mismatch_actual_frame")
            return
        if direction == 1 and kind == 2 and self.mode in ("ready-executable", "ready-library"):
            altered = bytearray(packet)
            position = 64
            def number():
                nonlocal position
                value = struct.unpack_from("!I", altered, position)[0]
                position += 4
                return value
            position += 4 + struct.unpack_from("!I", altered, position)[0]
            for _ in range(2):
                for _ in range(number()):
                    length = number()
                    position += length + 4
                    arcs = number()
                    position += arcs * 4
            if self.mode == "ready-library":
                position += 32
                if not number():
                    raise RuntimeError("actual Ready has no library identity")
                length = number()
                position += length
            altered[position] ^= 1
            self.enqueue(target, altered)
            emit("ready_identity_actual_frame", field=self.mode)
            return
        if direction == 0 and kind == 3 and self.mode == "bad-set":
            altered = bytearray(packet)
            struct.pack_into("!H", altered, 120, 65535)
            self.enqueue(target, altered)
            emit("unsupported_actual_set_tag")
            return
        if direction == 1 and kind == 4:
            if self.mode == "coalesced":
                self.held = packet
                return
            if self.mode == "malformed" and not self.changed:
                self.changed = True
                altered = bytearray(packet)
                altered[0] ^= 1
                self.enqueue(target, altered)
                emit("malformed_actual_header")
                return
            if self.mode == "oversize" and not self.changed:
                self.changed = True
                fields[3] = 2097153
                self.enqueue(target, HEADER.pack(*fields) + packet[64:])
                emit("oversized_actual_header")
                return
            if self.mode in ("truncated", "partial-timeout") and not self.changed:
                self.changed = True
                self.enqueue(target, packet[:71] if self.mode == "truncated" else packet[:1])
                if self.mode == "truncated":
                    self.shutdown[target] = True
                emit("truncated_actual_frame", delivered=71 if self.mode == "truncated" else 1)
                return
        if self.changed and self.mode in ("truncated", "partial-timeout") and direction == 1:
            return
        if direction == 1 and kind == 5 and self.mode == "coalesced":
            if self.held is None:
                raise RuntimeError("Retired arrived without actual Result")
            self.enqueue(target, self.held + packet)
            self.held = None
            emit("coalesced_actual_frames", count=2)
            return
        if self.mode == "stale":
            variants = []
            if direction == 1 and kind == 4:
                for index in (5, 6, 9):
                    copy = fields.copy()
                    copy[index] += 1
                    variants.append((copy, packet[64:], "header"))
                payload = bytearray(packet[64:])
                generation = struct.unpack_from("!Q", payload, 16)[0]
                struct.pack_into("!Q", payload, 16, generation + 1)
                variants.append((fields.copy(), payload, "generation"))
            elif direction == 1 and kind == 5:
                payload = bytearray(packet[64:])
                generation = struct.unpack_from("!Q", payload, 16)[0]
                struct.pack_into("!Q", payload, 16, generation + 1)
                variants.append((fields.copy(), payload, "retirement"))
            elif direction == 0 and kind == 3 and fields[9] > 1:
                payload = bytearray(packet[64:])
                struct.pack_into("!Q", payload, 32, 1)
                variants.append((fields.copy(), payload, "admission"))
            for copy, body, name in variants:
                # Each injected frame occupies one real sequence position; originals follow it.
                copy[10] = fields[10]
                self.enqueue(target, HEADER.pack(*copy) + body)
                fields[10] += 1
                self.offsets[direction] += 1
                emit("stale_actual_frame", direction=direction, kind=kind, field=name)
            packet = HEADER.pack(*fields) + packet[64:]
            self.enqueue(target, packet)
            if direction == 1 and kind in (4, 5):
                self.enqueue(target, packet)
                emit("duplicate_actual_frame", kind=kind)
            return
        self.enqueue(target, packet)

    def run(self):
        emit("ipc_fault_ready", mode=self.mode)
        while RUNNING:
            now = time.monotonic()
            readers = [self.sockets[i] for i in range(2) if not self.closed[i] and not (i == 0 and self.paused)]
            writers = [self.sockets[i] for i in range(2) if self.outputs[i] and self.outputs[i][0][1] <= now]
            read, write, _ = select.select(readers, writers, [], 0.005)
            for transport in read:
                direction = self.sockets.index(transport)
                try:
                    data = transport.recv(64 if self.mode == "full-channel" and direction == 0 else 65536)
                except ConnectionResetError:
                    data = b""
                if not data:
                    self.closed[direction] = True
                    self.shutdown[1 - direction] = True
                    emit("actual_eof", direction=direction, partial=len(self.inputs[direction]))
                    continue
                self.inputs[direction].extend(data)
                if len(self.inputs[direction]) > MAX_FRAME:
                    raise RuntimeError("external receive frame bound exceeded")
                while len(self.inputs[direction]) >= 64:
                    if direction == 0 and self.mode == "full-channel" and HEADER.unpack_from(self.inputs[direction])[2] == 3:
                        self.paused = True
                        emit("full_channel_reader_stopped", received=len(self.inputs[direction]))
                        break
                    length = HEADER.unpack_from(self.inputs[direction])[3]
                    if length > MAX_FRAME - 64:
                        raise RuntimeError("actual frame length invalid before injection")
                    size = 64 + length
                    if len(self.inputs[direction]) < size:
                        break
                    packet = bytes(self.inputs[direction][:size])
                    del self.inputs[direction][:size]
                    self.frame(direction, packet)
            for transport in write:
                target = self.sockets.index(transport)
                if not self.outputs[target]:
                    continue
                entry = self.outputs[target][0]
                try:
                    count = transport.send(entry[0], socket.MSG_NOSIGNAL)
                except (BrokenPipeError, ConnectionResetError):
                    self.outputs[target].clear()
                    self.shutdown[target] = True
                    continue
                entry[0] = entry[0][count:]
                if not entry[0]:
                    self.outputs[target].popleft()
            for target in range(2):
                if self.shutdown[target] and not self.outputs[target]:
                    try:
                        self.sockets[target].shutdown(socket.SHUT_WR)
                    except OSError:
                        pass
            if all(self.closed) and not any(self.outputs):
                break
        for transport in self.sockets:
            transport.close()
        emit("ipc_fault_stopped", frames=self.frames, pending=[len(queue) for queue in self.outputs])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=MODES, required=True)
    parser.add_argument("--parent-fd", type=int, required=True)
    parser.add_argument("--worker-fd", type=int, required=True)
    args = parser.parse_args()
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    Forwarder(args.mode, args.parent_fd, args.worker_fd).run()


if __name__ == "__main__":
    main()
