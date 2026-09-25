# Companion — Démarrage rapide

Guide utilisateur pour le compagnon vocal **Freenove FNK0104B** + daemon [Akasha](https://github.com/azerothl/Akasha).

## Matériel

- Freenove **FNK0104B** (2,8″ tactile, mic MEMS, codec ES8311)
- Haut-parleur kit branché (connecteur PH1.25)
- Câble USB-C **data**
- PC sur le même Wi‑Fi / LAN que la carte

BOM : [hardware/BOM.md](../../hardware/BOM.md)

## Prérequis logiciels

1. [PlatformIO Core](https://platformio.org/install) (`pio`)
2. Daemon Akasha joignable sur le LAN (port **3876**)
3. STT + TTS configurés (`voice_router.yaml`) — voir [daemon.md](daemon.md)

## 1. Configurer Wi‑Fi et host daemon

```powershell
cd Akasha_companion
.\scripts\provision.ps1 -Wifi "MonWifi" -WifiPass "…" -DaemonHost "192.168.1.168" -DaemonPort 3876
```

Ou éditer `firmware/include/secrets.h` (copié depuis `secrets.example.h`).

Si le daemon a `AKASHA_COMPANION_PAIR_SECRET`, renseigner le même dans `AKASHA_PAIR_SECRET`.

## 2. Flash

```powershell
.\scripts\flash.ps1 -Port COM5 -NoMonitor
```

Sous Linux / macOS / Git Bash : `./scripts/flash.sh --port /dev/ttyACM0 --no-monitor`

Détails scripts : [scripts/README.md](../../scripts/README.md)

## 3. Daemon LAN

Le daemon doit écouter sur toutes les interfaces (sinon l’ESP reçoit HTTP `-1`) :

```text
AKASHA_BIND=0.0.0.0
AKASHA_PORT=3876
```

Vérifier depuis le PC :

```powershell
Invoke-RestMethod http://192.168.1.168:3876/api/status
Invoke-RestMethod http://192.168.1.168:3876/api/voice/status
Invoke-RestMethod http://192.168.1.168:3876/api/companion/snapshot
```

## 4. Premier usage

1. Brancher USB, attendre Wi‑Fi + overlay « pret »
2. **Tenir** la zone centrale (ou bouton BOOT) ≥ 300 ms → PTT → parler → relâcher
3. L’avatar passe `listening` → `thinking` → `speaking`
4. Swipe **gauche** → surface Texte (clavier + chips) ; swipe **droite** → Avatar

Pairing : au boot, si pas de token NVS, le firmware tente `POST /api/companion/pair` et sauve le token.

## Suite

- Gestures, HF/VAD, dépannage : [usage.md](usage.md)
- Config daemon / présence : [daemon.md](daemon.md)
- Contrat API : [../API_CONTRACT.md](../API_CONTRACT.md)
