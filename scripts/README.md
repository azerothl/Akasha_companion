# Scripts (Phase 1 / 6)

Operational helpers for Freenove FNK0104 bring-up and later product packaging. Implementations land in later phases; this folder documents the intended commands and env vars.

## Prerequisites

- [PlatformIO](https://platformio.org/) (or ESP-IDF CLI) installed and on `PATH`
- USB data cable to the FNK0104 board
- Correct serial port (Windows: `COMx`; Linux/macOS: `/dev/ttyUSB*` or `/dev/ttyACM*`)
- Firmware project under `firmware/` (see `firmware/README.md`)

## Flash firmware

**Status:** placeholder (Phase 1 bring-up, CMP-003+)

```bash
# From repo root, once firmware/ is an ESP-IDF or PlatformIO project:
cd firmware
pio run -t upload
# Or: idf.py -p <PORT> flash
```

| Variable | Purpose |
|----------|---------|
| `COMPANION_PORT` | Serial port for upload/monitor |
| `COMPANION_ENV` | Optional `sdkconfig` / board profile (e.g. `FNK0104B`) |

Document the exact target and flags in `firmware/README.md` when the project exists.

## Serial monitor

**Status:** placeholder (Phase 1)

```bash
cd firmware
pio device monitor -b 115200
# Or: idf.py -p <PORT> monitor
```

Use the same `COMPANION_PORT` as flash. Exit monitor with the usual Ctrl+] (PlatformIO) or Ctrl+] / configured escape (IDF).

## NVS provisioning (Wi-Fi + daemon)

**Status:** placeholder (Phase 1, CMP-006)

Secrets must **not** be committed. Store locally as `.env` or `nvs_secrets.csv` (both gitignored).

Intended flow:

1. Copy `nvs_secrets.example.csv` (to be added with firmware) to `nvs_secrets.csv`.
2. Fill Wi-Fi SSID/password, daemon host (LAN IP or hostname), port (`3876`), and optional device token.
3. Run a provisioning script (TBD), e.g.:

```bash
# python scripts/provision_nvs.py --port "$COMPANION_PORT" --csv nvs_secrets.csv
```

NVS keys and partition layout will be defined in the firmware tree. Until then, configure via future `menuconfig` / captive portal / serial CLI as specified in `docs/COMPANION_SPEC.md` Phase 1.

## Related docs

- [COMPANION_SPEC.md](../docs/COMPANION_SPEC.md) — phases and requirements
- [API_CONTRACT.md](../docs/API_CONTRACT.md) — daemon HTTP contract
- [HARDWARE_FNK0104.md](../docs/HARDWARE_FNK0104.md) — board pins and SKUs
