# Akasha Companion

Client tactile **ESP32-S3** pour le daemon [Akasha](https://github.com/azerothl/Akasha) — statut, actions rapides et chat court sur écran, sans LLM embarqué.

**Hardware cible :** [Freenove ESP32-S3 Display (FNK0104)](https://docs.freenove.com/projects/fnk0104/en/latest/)  
**Variante recommandée MVP :** **FNK0104B** (2.8″, 240×320, ILI9341, tactile)

```
┌─────────────────────┐        LAN / Wi‑Fi         ┌──────────────────┐
│  FNK0104 + LVGL     │ ──── HTTP (+ SSE later) ──►│  akasha-daemon   │
│  thin client        │                            │  :3876           │
└─────────────────────┘                            └──────────────────┘
```

## Docs

| Document | Contenu |
|----------|---------|
| [docs/COMPANION_SPEC.md](docs/COMPANION_SPEC.md) | Spéc produit, phases, exigences `CMP-*` |
| [docs/HARDWARE_FNK0104.md](docs/HARDWARE_FNK0104.md) | Variantes Freenove, I/O, contraintes |
| [docs/API_CONTRACT.md](docs/API_CONTRACT.md) | Contrat HTTP avec le daemon |
| [hardware/BOM.md](hardware/BOM.md) | Nomenclature d’achat |
| [AGENTS.md](AGENTS.md) | Conventions pour agents / contributeurs |

## Statut

Scaffold + spécifications. Firmware à venir (`firmware/`).

## Développement (prévu)

```bash
# Après Phase 1 — PlatformIO / ESP-IDF
cd firmware
pio run -t upload
pio device monitor
```

Prérequis runtime : daemon Akasha joignable sur le LAN (`GET http://<host>:3876/api/status`).

## Lien avec le monorepo

Les changements daemon éventuels (`/api/companion/*`) vivent dans **Akasha** (`crates/akasha-daemon`). Ce dépôt reste le firmware + UI embarquée + specs matériel.
