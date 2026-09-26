# Firmware — Akasha Companion (Phase 6)

PlatformIO + Arduino-ESP32 (`espressif32 @ 6.9.0`).  
**Env produit :** `fnk0104b`.

## Build / flash

Depuis la racine du dépôt (recommandé) :

```powershell
..\scripts\flash.ps1 -Port COM5 -NoMonitor
..\scripts\flash.ps1 -BuildOnly
```

Ou dans ce dossier :

```powershell
pio run -e fnk0104b
pio run -e fnk0104b -t upload --upload-port COM5
```

Secrets : `include/secrets.h` (voir `secrets.example.h` ou `../scripts/provision.ps1`).

## Fonctions (Phases 2–5)

- Avatar TFT_eSPI + PTT → STT → message → TTS
- Surfaces **Texte** / **Image** ; chip **HF** (mains-libres VAD)
- Snapshot / pair / sleep backlight
- Sur Texte/Image : dessin avatar **désactivé** (évite d’écraser le clavier)

## Gestures

| Action | Effet |
|--------|--------|
| Swipe gauche (avatar) | Surface **Texte** |
| Swipe droite (texte) | Retour **Avatar** |
| Hold centre / BOOT ≥ 300 ms | PTT vocal |
| Long-press avatar ≥ 600 ms | Menu chips |
| Tap image | Fermer → Avatar |
| Tap court avatar | Overlay statut |

## Surface Texte

- Historique 3 messages, chips Brief / Stop / Répète / Lire / Mic / **HF**
- Clavier AZERTY + OK
- Mic = PTT ; HF = toggle VAD (NVS + daemon)

## Surface Image

- URL `.jpg`/`.jpeg` ou `data:image/jpeg;base64,…` → `TJpg_Decoder`
- PNG / échec → placeholder + tap pour fermer

## Prérequis daemon

`AKASHA_BIND=0.0.0.0` + STT/TTS.  
Guide : [docs/user/daemon.md](../docs/user/daemon.md)

## Multi-SKU

| Env | Support |
|-----|---------|
| `fnk0104b` | Produit |
| `fnk0104a` | Expérimental (même panel, sans touch) |
| `fnk0104n` / `fnk0104s` | Stubs — non supportés jusqu’à bring-up |

Voir [docs/HARDWARE_FNK0104.md](../docs/HARDWARE_FNK0104.md).
