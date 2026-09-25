# Contrat API — Companion ↔ Daemon Akasha

Base URL MVP : `http://<daemon-host>:3876`  
Le daemon doit écouter sur le LAN : **`AKASHA_BIND=0.0.0.0`** (défaut = `127.0.0.1` uniquement → HTTP `-1` depuis l’ESP32).  
Auth : Bearer token si exigé ; sinon LAN. Phase 4 : token device via `POST /api/companion/pair` (NVS Companion).

Allowlist stricte. Toute nouvelle route → ce fichier + `CMP-*`.

---

## Voice-first (existant daemon — Phase 1–2)

Le companion s’appuie sur les routes voice déjà présentes dans Akasha (`voice_router.yaml`).

### `GET /api/voice/status`

Indique si STT/TTS sont configurés.

**Usage :** gate du mode vocal ; avatar `error` / toast si absent ; LED ambre.

### `POST /api/voice/stt`

Body (selon daemon) :

```json
{ "data_url": "data:audio/wav;base64,..." }
```

ou `{ "audio_base64": "..." }`.

**Réponse :** texte transcrit.

**Usage :** après capture MEMS mic (clip court). Afficher brièvement le transcript en overlay avatar (optionnel).

### `POST /api/voice/tts`

```json
{ "text": "..." }
```

**Réponse :** `{ "data_url": "data:audio/wav;base64,..." }` (ou équivalent).

**Usage :** lire la réponse assistant sur le speaker. L’avatar `speaking` est piloté par l’**enveloppe RMS du PCM en lecture** sur l’ESP32 — le TTS n’est **pas** embarqué (voir décision d’archi dans `COMPANION_SPEC.md`).

### Boucle conversation

1. `POST /api/voice/stt` → `user_text`
2. `POST /api/message` avec `user_text` + `session_id: "companion-<device_id>"`
3. Poll `GET /api/tasks/:id` jusqu’à done
4. Extraire texte réponse (borné) → `POST /api/voice/tts` → playback

**Optimisation future (Phase 5+)** : `POST /api/companion/utterance` (audio in → audio out + state) pour réduire les round-trips — hors Phase 4.

---

## Core message (existant)

### `GET /api/status`

Santé daemon — overlay avatar + LED.

### `POST /api/message`

```json
{
  "message": "string",
  "session_id": "companion-<device_id>"
}
```

Utilisé après STT **et** depuis la surface Texte.

### `GET /api/tasks/:id`

Poll résultat / erreur.

---

## Surfaces Texte / Image

| Besoin | API |
|--------|-----|
| Chat texte | `POST /api/message` + poll |
| Actions rapides | chips allowlist Companion (pas de nouvelle route) : `Brief` → « Fais un brief court… », `Stop` → cancel TTS + `POST /api/tasks/:id/cancel`, `Répète`/`Lire` → re-TTS dernier assistant |
| Image | Pas d’upload ; le companion parse le texte de tâche pour une URL `http(s)://…jpg|jpeg` ou `data:image/jpeg;base64,…` puis GET + decode local |

### Chips (Phase 3)

Messages / actions figés côté ESP (même `session_id` `companion-<mac>`) — voir firmware `app_shell` / `conversation`.

---

## Phase 4 — Snapshot, pairing, events

### `GET /api/companion/snapshot` (CMP-013)

JSON condensé (&lt; ~4 KiB) pour overlay + notify. Poll Companion ~7 s (pas de SSE sur ESP).

```json
{
  "schema_version": 1,
  "daemon": { "ok": true, "version": "x.y.z" },
  "llm": { "provider": "string", "model": "string" },
  "voice": { "stt": true, "tts": true },
  "tasks": { "active": 0 },
  "notify": { "unread": 0, "headline": null },
  "avatar_hint": "idle",
  "presence_mode": "ptt",
  "vad_enabled": false,
  "last_event_ts": null,
  "presence": { "enabled": false, "threshold_rms": 0.035, "min_speech_ms": 400, "max_speech_ms": 10000, "silence_hang_ms": 700, "cooldown_ms": 2500, "quiet_hours": "", "quiet_now": false }
}
```

`avatar_hint` : suggestion (`notify`, `idle`) — l’ESP anime localement.

Implémentation daemon : `crates/akasha-daemon/src/api_routes_companion.rs`.

### `POST /api/companion/pair` (CMP-014)

```json
{ "device_id": "aabbccddeeff", "name": "FNK0104B", "secret": "..." }
```

- Si `AKASHA_COMPANION_PAIR_SECRET` est défini côté daemon → `secret` obligatoire et égal.
- Réponse : `{ "token": "cmp_…", "device_id": "…" }` — stocké NVS Companion (`Authorization: Bearer`).
- Persistance daemon : `data_dir/companion_devices.json`.

### `POST /api/companion/utterance` (hors Phase 4)

Reporté — STT/TTS/message restent la boucle vocale.

### Events (CMP-012)

- **Companion** : poll enrichi via snapshot (ci-dessus).
- **Clients riches (UI desktop)** : `GET /api/events` SSE existant — hors chemin ESP.

### Sleep backlight (CMP-011)

Timeout idle 60 s → `PIN_TFT_BL` LOW ; wake touch / BOOT. Pas de deep-sleep ESP.  
Phase 5 : entrée VAD **suspendue** quand backlight off ; reprise au wake. Début de parole VAD peut rappeler l’activité (wake soft).

---

## Phase 5 — Presence / Hands-free VAD (CMP-015)

Policy runtime : `data_dir/companion_presence.json` (+ override env `AKASHA_COMPANION_VAD_ENABLED`).  
Dernier événement : champ `last_presence_event` dans `data_dir/companion_devices.json`.

### `GET /api/companion/presence/config`

```json
{
  "enabled": false,
  "threshold_rms": 0.035,
  "min_speech_ms": 400,
  "max_speech_ms": 10000,
  "silence_hang_ms": 700,
  "cooldown_ms": 2500,
  "quiet_hours": "",
  "quiet_now": false,
  "effective_enabled": false
}
```

`quiet_hours` : `"HH:MM-HH:MM"` (heure locale daemon) ; vide = toujours.  
`effective_enabled` = `enabled && !quiet_now`.

### `POST /api/companion/presence/config`

Body partiel accepté (`enabled`, seuils, `quiet_hours`). Persiste le fichier policy.

### `POST /api/companion/presence/event`

```json
{ "event": "vad_start|vad_end|speech_dropped|speech_sent|hands_free_on|hands_free_off|error",
  "device_id": "aabbccddeeff",
  "detail": "optional ascii" }
```

Pas de payload audio. Compteurs / métriques textuelles seulement.

### Snapshot étendu (Phase 5)

Champs additionnels sur `GET /api/companion/snapshot` :

```json
{
  "presence_mode": "ptt|hands_free",
  "vad_enabled": false,
  "last_event_ts": null,
  "presence": {
    "enabled": false,
    "threshold_rms": 0.035,
    "min_speech_ms": 400,
    "max_speech_ms": 10000,
    "silence_hang_ms": 700,
    "cooldown_ms": 2500,
    "quiet_hours": "",
    "quiet_now": false
  }
}
```

`vad_enabled` = policy enabled ∧ hors quiet hours ∧ STT configuré.

### Boucle firmware

1. Chip **HF** (surface Texte) ou long-press menu → toggle NVS + `POST …/presence/config`
2. Si actif : VAD local (RMS frames) → segment WAV borné → STT → message → TTS (flux existant)
3. PTT manuel inchangé ; VAD suspendu pendant busy / sleep BL / PTT / TTS (anti-echo)

---

## Phase 5 — Hands-free VAD

---

## Découverte LAN Companion (UDP)

Le daemon écoute **UDP 3877** (tous interfaces). Le Companion envoie `AKASHA_DISCOVER` (broadcast).

**Réponse JSON :**

```json
{ "service": "akasha", "port": 3876, "version": "0.10.0" }
```

Le Companion peut aussi sonder `GET /api/status` sur le LAN (fenêtre autour de son IP + gateway) si UDP ne répond pas.

Endpoint custom (VPS) : saisie `host` ou `host:port` en NVS (`custom_endpoint=true`) — `secrets.h` ne réécrit plus host/port dans ce cas.

---

## Erreurs & UX

| Situation | Avatar | Audio |
|-----------|--------|-------|
| Wi‑Fi / timeout | `offline` | — |
| Voice status KO | `error` + toast | Bip |
| STT vide / fail | `error` | Bip |
| Tâche failed | `error` | TTS court « désolé… » si possible |
| TTS fail mais texte ok | `idle` + bascule Texte | Bip ; afficher texte |

---

## Hors contrat

- Studio / PTY / vault / tools arbitraires
- Streams audio non bornés
- Upload fichiers gros (&gt; budget flash/RAM)
