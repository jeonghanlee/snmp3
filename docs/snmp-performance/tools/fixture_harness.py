#!/usr/bin/env python3
"""Shared process helpers for the throughput measurement: agent, UDP delay proxy, secrets."""

import json
import os
from pathlib import Path
import secrets
import signal
import socket
import subprocess
import sys
import time

ROOT = Path(os.environ.get("SNMP3_ROOT", Path(__file__).resolve().parents[3]))
PRODUCTS = Path(os.environ.get("SNMP3_PRODUCTS", ROOT / "bin" / "linux-x86_64"))
USER = "benchUser"


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def private_file(path, text):
    with open(path, "x", opener=lambda p, flags: os.open(p, flags, 0o600)) as stream:
        stream.write(text)


class Bench:
    def __init__(self, output):
        self.output = Path(output).resolve()
        self.output.mkdir(mode=0o700)
        self.children = []
        self.material = {k: secrets.token_hex(12) for k in ("community", "auth", "priv")}
        self.paths = {}
        for key, value in self.material.items():
            self.paths[key] = self.output / ("secret-" + key)
            private_file(self.paths[key], value + "\n")
        self.env = dict(os.environ)

    def spawn(self, name, argv, env=None):
        stdout = (self.output / (name + ".stdout")).open("wb")
        stderr = (self.output / (name + ".stderr")).open("wb")
        child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=env or self.env,
                                 stdin=subprocess.DEVNULL)
        self.children.append((name, child, stdout, stderr))
        return child

    def agent(self):
        port = free_port()
        config = self.output / "agent.conf"
        m = self.material
        private_file(config,
                     f"rwcommunity {m['community']} 127.0.0.1 .1.3.6.1.4.1.53864\n"
                     f"createUser {USER} SHA {m['auth']} AES {m['priv']}\n"
                     f"group benchGroup usm {USER}\n"
                     "view benchView included .1.3.6.1.4.1.53864\n"
                     "access benchGroup \"\" usm noauth prefix benchView benchView none\n")
        self.spawn("agent", [str(PRODUCTS / "snmp3NativeAgent"), str(port), "4", str(config)])
        time.sleep(1.0)
        return port

    def proxy(self, agent_port, delay_ms):
        name = f"proxy-{delay_ms}ms"
        mode = "delay" if delay_ms else "pass"
        argv = [sys.executable, str(ROOT / "tests/rewrite/helpers/udp_fault.py"), "--family", "4",
                "--agent-port", str(agent_port), "--mode", mode, "--delay-ms", str(delay_ms)]
        child = self.spawn(name, argv)
        end = time.monotonic() + 10
        while time.monotonic() < end and child.poll() is None:
            for line in (self.output / (name + ".stdout")).read_text().splitlines():
                try:
                    event = json.loads(line)
                except ValueError:
                    continue
                if event.get("event") == "fault_ready":
                    return event["port"], child
            time.sleep(0.02)
        raise RuntimeError("proxy not ready")

    def stop(self, child):
        if child.poll() is None:
            child.send_signal(signal.SIGTERM)
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()

    def close(self):
        for _, child, out, err in reversed(self.children):
            self.stop(child)
            out.close()
            err.close()
