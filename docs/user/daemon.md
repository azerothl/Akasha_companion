# Companion — Daemon Akasha

Le Companion est un **thin client** : STT / TTS / LLM restent sur le daemon (`CMP-029`).

## Bind LAN (obligatoire)

Par défaut le daemon écoute `127.0.0.1` → l’ESP sur le Wi‑Fi ne peut pas joindre.

```env
AKASHA_BIND=0.0.0.0
AKASHA_PORT=3876
```

Dans `~/akasha/akasha.env` (Windows : `%USERPROFILE%\akasha\akasha.env`) ou variables d’environnement du process LAN.

## Voix

Fichier `voice_router.yaml` dans le data_dir, par ex. :

```yaml
tts:
  base_url: http://localhost:8765
stt:
  base_url: http://localhost:8766
```

Contrôle : `GET /api/voice/status` → `stt_configured` / `tts_configured`.

**Important :** `configured` = URLs présentes, **pas** services vivants. Sans process sur 8765/8766, le Companion affiche une erreur (ex. « STT coupe » / « request error »).

Démarrer les services (Docker Desktop requis) :

```bash
cd akasha-models
docker compose --profile voice up -d
```

Ou sans Docker, depuis `akasha-models` avec un venv Python :

```powershell
# TTS (8765) + STT (8766) — voir akasha-models/README.md
$env:WHISPER_MODEL="tiny"
python -m uvicorn server:app --host 0.0.0.0 --port 8765   # dans tts/
python -m uvicorn server:app --host 0.0.0.0 --port 8766   # dans stt/
```

Santé : `http://127.0.0.1:8765/health` et `http://127.0.0.1:8766/health`.

## Companion (Phases 4–5)

| Variable / fichier | Rôle |
|--------------------|------|
| `AKASHA_COMPANION_PAIR_SECRET` | Secret optionnel pour `POST /api/companion/pair` |
| `AKASHA_COMPANION_VAD_ENABLED` | Override policy VAD (`1`/`0`) |
| `companion_presence.json` | Policy VAD persistée |
| `companion_devices.json` | Devices pairés + dernier événement présence |

Routes :

- `GET /api/companion/snapshot`
- `POST /api/companion/pair`
- `GET` / `POST /api/companion/presence/config`
- `POST /api/companion/presence/event`

Exemple activation HF :

```powershell
Invoke-RestMethod -Method POST http://192.168.1.168:3876/api/companion/presence/config `
  -ContentType application/json -Body '{"enabled":true,"threshold_rms":0.035}'
```

Référence complète daemon : [configuration Akasha](https://github.com/azerothl/Akasha/blob/main/docs/user/configuration.md)  
Contrat Companion : [../API_CONTRACT.md](../API_CONTRACT.md)

## Deux daemons sur le même PC

Souvent : UI locale sur `127.0.0.1:3876` **et** binaire LAN sur `0.0.0.0:3876`.  
Le Companion doit pointer vers l’IP LAN (`192.168.x.x`) du process qui expose `/api/companion/*` et la voix.
