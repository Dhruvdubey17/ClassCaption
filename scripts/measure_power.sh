#!/usr/bin/env bash
# Speed and energy per backend. powermetrics needs sudo, so run this yourself in a terminal.
# Keep the Mac in one power state for the whole run (plugged in, or not), with other apps closed.
set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p bench/data
log="bench/data/powermetrics-$(date +%Y%m%dT%H%M%S).plist"

sudo -v
sudo powermetrics --samplers cpu_power,gpu_power,ane_power -i 500 -f plist -o "$log" &
powermetrics_pid=$!
trap 'sudo kill -INT "$powermetrics_pid" 2>/dev/null || true' EXIT
sleep 5

uv run scripts/bench.py speed

sudo kill -INT "$powermetrics_pid"
wait "$powermetrics_pid" || true
trap - EXIT

uv run scripts/bench.py power "$log"
uv run scripts/bench.py table
