#!/usr/bin/env bash
# Flash + serial monitor — Akasha Companion (Phase 6)
#
# Examples:
#   ./scripts/flash.sh
#   ./scripts/flash.sh --port /dev/ttyACM0 --no-monitor
#   ./scripts/flash.sh --build-only
#   ./scripts/flash.sh --monitor-only --port COM5
#   ./scripts/flash.sh --env fnk0104b

set -euo pipefail

MONITOR_ONLY=0
BUILD_ONLY=0
NO_MONITOR=0
PORT=""
ENV_NAME="fnk0104b"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --monitor-only|-m) MONITOR_ONLY=1; shift ;;
    --build-only|-b) BUILD_ONLY=1; shift ;;
    --no-monitor) NO_MONITOR=1; shift ;;
    --port|-p) PORT="${2:-}"; shift 2 ;;
    --env|-e) ENV_NAME="${2:-}"; shift 2 ;;
    -h|--help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

find_pio() {
  if command -v pio >/dev/null 2>&1; then
    command -v pio
    return
  fi
  local win="$HOME/.platformio/penv/Scripts/pio.exe"
  if [[ -x "$win" ]]; then
    echo "$win"
    return
  fi
  local unix="$HOME/.platformio/penv/bin/pio"
  if [[ -x "$unix" ]]; then
    echo "$unix"
    return
  fi
  echo "PlatformIO (pio) not found" >&2
  exit 1
}

PIO="$(find_pio)"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW="$ROOT/firmware"
cd "$FW"

if [[ ! -f include/secrets.h ]]; then
  cp include/secrets.example.h include/secrets.h
  echo "Created include/secrets.h from example — edit WiFi / daemon host before use."
  echo "Tip: ./scripts/provision.sh --wifi SSID --wifi-pass pass --host 192.168.1.10"
fi

case "$ENV_NAME" in
  fnk0104b|fnk0104a|fnk0104n|fnk0104s) ;;
  *) echo "Unknown --env '$ENV_NAME'" >&2; exit 1 ;;
esac
if [[ "$ENV_NAME" != "fnk0104b" ]]; then
  echo "WARNING: env '$ENV_NAME' is experimental (product support = fnk0104b)." >&2
fi

if [[ -n "$PORT" ]]; then
  export PLATFORMIO_UPLOAD_PORT="$PORT"
  export PLATFORMIO_MONITOR_PORT="$PORT"
  echo "Serial port: $PORT"
fi

run_monitor() {
  if [[ -n "$PORT" ]]; then
    "$PIO" device monitor -b 115200 --port "$PORT"
  else
    "$PIO" device monitor -b 115200
  fi
}

if [[ "$MONITOR_ONLY" -eq 1 ]]; then
  run_monitor
  exit $?
fi

if [[ "$BUILD_ONLY" -eq 1 ]]; then
  "$PIO" run -e "$ENV_NAME"
  exit $?
fi

set +e
"$PIO" run -e "$ENV_NAME" -t upload
rc=$?
set -e
if [[ $rc -ne 0 ]]; then
  echo ""
  echo "Upload failed. If the port is busy:"
  echo "  - Close any serial monitor using that port"
  echo "  - Retry with --port <device>"
  exit $rc
fi

if [[ "$NO_MONITOR" -eq 1 ]]; then
  echo "Flash OK (--no-monitor)."
  exit 0
fi

run_monitor
