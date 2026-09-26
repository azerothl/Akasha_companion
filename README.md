# Akasha Companion

Compagnon **vocal** ESP32-S3 pour le daemon [Akasha](https://github.com/azerothl/Akasha) : on parle à Akasha ; l’écran montre un **avatar** vivant. Texte et images sont des surfaces secondaires.

**Hardware :** [Freenove ESP32-S3 Display (FNK0104)](https://docs.freenove.com/projects/fnk0104/en/latest/)  
**Produit supporté :** **FNK0104B** — tactile, **micro MEMS**, **haut-parleur** kit (codec ES8311).

```
┌──────────────────────────┐     LAN      ┌─────────────────────────────┐
│ Avatar TFT_eSPI + I2S    │◄────────────►│ akasha-daemon :3876         │
│ mic → STT → message → TTS│   HTTP       │ /api/voice/*  /api/companion│
└──────────────────────────┘              └─────────────────────────────┘
```

## Guide utilisateur

| Document | Contenu |
|----------|---------|
| [docs/user/getting-started.md](docs/user/getting-started.md) | Flash, secrets, premier PTT |
| [docs/user/usage.md](docs/user/usage.md) | Gestures, HF/VAD, dépannage |
| [docs/user/daemon.md](docs/user/daemon.md) | `AKASHA_BIND`, voix, presence |

Flash rapide :

```powershell
.\scripts\provision.ps1 -Wifi "…" -WifiPass "…" -DaemonHost "192.168.1.168"
.\scripts\flash.ps1 -Port COM5 -NoMonitor
```

Voir [scripts/README.md](scripts/README.md).

## Specs techniques

| Document | Contenu |
|----------|---------|
| [docs/COMPANION_SPEC.md](docs/COMPANION_SPEC.md) | Vision, phases, `CMP-*` |
| [docs/UX_AVATAR_VOICE.md](docs/UX_AVATAR_VOICE.md) | Avatar, gestures, surfaces |
| [docs/HARDWARE_FNK0104.md](docs/HARDWARE_FNK0104.md) | Board, ES8311, multi-SKU |
| [docs/API_CONTRACT.md](docs/API_CONTRACT.md) | Allowlist HTTP |
| [hardware/BOM.md](hardware/BOM.md) | Nomenclature |
| [AGENTS.md](AGENTS.md) | Conventions agents |

## Statut

| Phase | Contenu | État |
|-------|---------|------|
| 1 | Display, touch, Wi‑Fi, ES8311 | Validé FNK0104B |
| 2 | Avatar + boucle vocale PTT | Firmware |
| 3 | Surfaces Texte / Image | Firmware |
| 4 | Snapshot, pairing, sleep BL | Firmware + daemon |
| 5 | Hands-free VAD + policy | Firmware + daemon |
| 6 | Flash tooling, doc user, multi-SKU | **Produit** |

Daemon : `AKASHA_BIND=0.0.0.0` + STT/TTS. Variantes A/N/S = expérimentales ([HARDWARE](docs/HARDWARE_FNK0104.md)).

## Lien monorepo

Routes `/api/companion/*` → **Akasha** (`crates/akasha-daemon`).  
STT/TTS : `/api/voice/stt`, `/api/voice/tts`.
