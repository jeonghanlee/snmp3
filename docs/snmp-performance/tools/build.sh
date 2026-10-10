#!/bin/bash
# Build the device measurement programs and the fixture baseline against the installed Net-SNMP.
# Usage: build.sh [output-directory]   (default: ./bin next to this script)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:-$here/bin}"
mkdir -p "$out"
flags="$(net-snmp-config --cflags) $(net-snmp-config --libs)"
for name in device_sweep device_monitor fixture_baseline; do
    gcc -O2 -Wall -o "$out/$name" "$here/$name.c" $flags
done
ls -l "$out"
