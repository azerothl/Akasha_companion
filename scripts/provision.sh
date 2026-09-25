#!/usr/bin/env bash
# Provision firmware/include/secrets.h for Akasha Companion (Phase 6)
#
# Examples:
#   ./scripts/provision.sh
#   ./scripts/provision.sh --wifi Maison --wifi-pass secret --host 192.168.1.168 --port 3876
#   ./scripts/provision.sh --pair-secret my-pair --force

set -euo pipefail

WIFI=""
WIFI_PASS=""
HOST=""
PORT=""
TOKEN=""
PAIR=""
FORCE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --wifi) WIFI="${2:-}"; shift 2 ;;
    --wifi-pass) WIFI_PASS="${2:-}"; shift 2 ;;
    --host) HOST="${2:-}"; shift 2 ;;
    --port) PORT="${2:-}"; shift 2 ;;
    --token) TOKEN="${2:-}"; shift 2 ;;
    --pair-secret) PAIR="${2:-}"; shift 2 ;;
    --force) FORCE=1; shift ;;
    -h|--help)
      sed -n '2,10p' "$0"
      exit 0
      ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXAMPLE="$ROOT/firmware/include/secrets.example.h"
TARGET="$ROOT/firmware/include/secrets.h"

if [[ ! -f "$EXAMPLE" ]]; then
  echo "Missing $EXAMPLE" >&2
  exit 1
fi

if [[ -f "$TARGET" && "$FORCE" -eq 0 && -z "$WIFI$WIFI_PASS$HOST$PORT$TOKEN$PAIR" ]]; then
  echo "secrets.h already exists. Use --force to overwrite, or pass --wifi/--host/… to patch."
  echo ""
  echo "Daemon reminder: set AKASHA_BIND=0.0.0.0 so the ESP can reach the daemon on the LAN."
  exit 0
fi

if [[ ! -f "$TARGET" || "$FORCE" -eq 1 ]]; then
  cp "$EXAMPLE" "$TARGET"
  echo "Wrote $TARGET from secrets.example.h"
fi

set_define() {
  local name="$1"
  local value="$2"
  local as_string="$3"
  local repl
  if [[ "$as_string" == "1" ]]; then
    local esc
    esc=$(printf '%s' "$value" | sed 's/\\/\\\\/g; s/"/\\"/g')
    repl="#define $name \"$esc\""
  else
    repl="#define $name $value"
  fi
  if grep -qE "^#define[[:space:]]+$name[[:space:]]" "$TARGET"; then
    # portable-ish in-place via temp
    local tmp
    tmp="$(mktemp)"
    awk -v n="$name" -v r="$repl" '
      $0 ~ ("^#define[[:space:]]+" n "[[:space:]]") { print r; next }
      { print }
    ' "$TARGET" > "$tmp"
    mv "$tmp" "$TARGET"
  else
    printf '\n%s\n' "$repl" >> "$TARGET"
  fi
}

[[ -n "$WIFI" ]] && set_define WIFI_SSID "$WIFI" 1
[[ -n "$WIFI_PASS" ]] && set_define WIFI_PASS "$WIFI_PASS" 1
[[ -n "$HOST" ]] && set_define AKASHA_HOST "$HOST" 1
[[ -n "$PORT" ]] && set_define AKASHA_PORT "$PORT" 0
[[ -n "$TOKEN" ]] && set_define AKASHA_TOKEN "$TOKEN" 1
[[ -n "$PAIR" ]] && set_define AKASHA_PAIR_SECRET "$PAIR" 1

echo "Provision OK: $TARGET"
echo ""
echo "Next:"
echo "  1. Start daemon with AKASHA_BIND=0.0.0.0 (LAN) + STT/TTS in voice_router.yaml"
echo "  2. Flash: ./scripts/flash.sh --port <device> --no-monitor"
echo "  3. See docs/user/getting-started.md"
