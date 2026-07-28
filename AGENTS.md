# AGENTS.md — Akasha Companion

Firmware / UI embarquée pour le compagnon tactile Akasha sur **Freenove FNK0104** (ESP32-S3 Display).

## Contexte

- Le **daemon Akasha** reste la source de vérité (LLM, mémoire, tools, schedules).
- Ce dépôt = thin client : affichage, tactile, Wi‑Fi, appels HTTP.
- Ne pas embarquer de modèle LLM sur l’ESP32.

## Hardware de référence

Docs officielles : https://docs.freenove.com/projects/fnk0104/en/latest/

| SKU | Taille | Résolution | Driver | Tactile | Rôle projet |
|-----|--------|------------|--------|---------|-------------|
| FNK0104B | 2.8″ | 240×320 | ILI9341 | Oui | **Cible MVP** |
| FNK0104N | 3.5″ | 320×480 | ST77922 | Oui | Variante large |
| FNK0104S | 4.0″ | 320×480 | ST7796 | Oui | Variante large |
| FNK0104A | 2.8″ | 240×320 | ILI9341 | Non | Status-only (optionnel) |

Ressources Freenove (exemples LVGL, Wi‑Fi, SD, audio) :  
https://github.com/Freenove/Freenove_ESP32_S3_Display

## Specs à suivre

1. `docs/COMPANION_SPEC.md` — exigences `CMP-*`, phases
2. `docs/HARDWARE_FNK0104.md` — pins / capacités board
3. `docs/API_CONTRACT.md` — endpoints daemon autorisés côté companion

## Règles de contribution

- Préférer **ESP-IDF** ou PlatformIO + Arduino-ESP32 pour le firmware ; LVGL pour l’UI.
- Config runtime (host, port, token) en **NVS** — jamais de secrets en dur dans le repo.
- HTTP clair sur LAN en MVP ; TLS plus tard.
- Changements d’API daemon → PR dans le monorepo **Akasha**, puis mise à jour de `docs/API_CONTRACT.md` ici.
- Ne pas copier massivement le code Freenove sous licence CC BY-NC-SA dans ce dépôt sans revue licence ; s’inspirer des exemples / réécrire.

## Structure

```
docs/        specs
firmware/    code embarqué (à venir)
hardware/    BOM, notes assemblage
scripts/     flash, provisionnement
```
