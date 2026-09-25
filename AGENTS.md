# AGENTS.md — Akasha Companion

Firmware / UI pour le compagnon **vocal** Akasha sur **Freenove FNK0104**.

## Contexte

- Daemon Akasha = LLM + **STT/TTS** (`/api/voice/*`) + tools + `/api/companion/*`.
- Ce dépôt = thin client : **avatar TFT_eSPI**, capture mic, playback speaker, Wi‑Fi.
- **Mode principal = voix** ; texte / image = surfaces secondaires.
- **Pas** de STT/TTS conversationnel ni LLM sur l’ESP32 (**CMP-029**).
- Local : enveloppe RMS bouche, bips UX, VAD RMS mains-libres (Phase 5, pas wake-word).
- Détail : `docs/COMPANION_SPEC.md` § « Décision d’archi — STT/TTS ».

## Hardware

Docs : https://docs.freenove.com/projects/fnk0104/en/latest/

| SKU | Rôle |
|-----|------|
| FNK0104B | **Produit** — touch + MEMS mic + speaker kit |
| FNK0104A | Expérimental — sans touch (pins proches de B) |
| FNK0104N / S | Stubs — non validés |

Audio : codec **ES8311** (I2S).

## Specs

1. `docs/COMPANION_SPEC.md` — `CMP-*`, phases
2. `docs/UX_AVATAR_VOICE.md` — états avatar, gestures
3. `docs/HARDWARE_FNK0104.md` — pins display + I2S + multi-SKU
4. `docs/API_CONTRACT.md` — allowlist
5. `docs/user/*` — guide utilisateur (Phase 6)

## Règles

- PTT toujours disponible ; HF/VAD **opt-in** (privacy).
- Buffers audio courts (16 kHz mono, ≤ ~15 s).
- Config NVS + `secrets.h` pour Wi‑Fi / host / token / HF pref.
- Sur surfaces Texte/Image : `avatar_set_drawing_enabled(false)` — ne pas peindre l’avatar par-dessus.
- PR API daemon dans **Akasha**, puis maj `API_CONTRACT.md`.
- Ne pas republier le zip Freenove (CC BY-NC-SA) tel quel.
- Flash / provision : `scripts/flash.*`, `scripts/provision.*`.

## Structure

```
docs/        specs + docs/user (guide)
firmware/    PlatformIO (env fnk0104b)
hardware/    BOM
scripts/     flash / provisionnement
```
