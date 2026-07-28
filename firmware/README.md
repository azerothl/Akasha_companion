# Firmware

État : **placeholder** — Phase 1 (bring-up) non démarrée.

## Objectif

Firmware ESP32-S3 multi-SKU (FNK0104B/N/S/A) :

- LVGL UI (Home / Chat / Actions)
- Client HTTP vers le daemon Akasha
- Config NVS (Wi‑Fi, host, token)

## Stack prévue

| Élément | Choix |
|---------|--------|
| Toolchain | PlatformIO ou ESP-IDF |
| UI | LVGL 8/9 |
| HTTP | `esp_http_client` ou équivalent Arduino |
| JSON | cJSON / ArduinoJson |

## Structure cible

```
firmware/
  platformio.ini          # ou CMakeLists idf
  boards/                 # pins & macros par SKU
  src/
    main.c
    ui/                   # écrans LVGL
    net/                  # Wi‑Fi + client API
    config/               # NVS
```

## Bring-up immédiat

1. Valider le hardware avec les sketches Freenove (LVGL / Board Test) — **hors ce dépôt** pour respecter la licence CC BY-NC-SA des exemples.
2. Réimplémenter un hello minimal ici (display + Wi‑Fi + `GET /api/status`).

Voir `docs/COMPANION_SPEC.md` (CMP-003 … CMP-006).
