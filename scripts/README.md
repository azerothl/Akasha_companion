# Scripts — Akasha Companion (Phase 6)

## Prérequis

- [PlatformIO Core](https://platformio.org/install) (`pio` dans le PATH ou `~/.platformio/penv`)
- Câble USB-C **data** branché sur le FNK0104B
- Daemon Akasha sur le LAN avec `AKASHA_BIND=0.0.0.0` (voir [docs/user/daemon.md](../docs/user/daemon.md))

## `provision.ps1` / `provision.sh`

Crée ou met à jour `firmware/include/secrets.h` (Wi‑Fi, host daemon, pairing).

```powershell
.\scripts\provision.ps1
.\scripts\provision.ps1 -Wifi "Maison" -WifiPass "…" -DaemonHost "192.168.1.168" -DaemonPort 3876
.\scripts\provision.ps1 -PairSecret "dev-secret" -Force
```

```bash
./scripts/provision.sh
./scripts/provision.sh --wifi Maison --wifi-pass '…' --host 192.168.1.168 --port 3876
```

Rappel : si `AKASHA_COMPANION_PAIR_SECRET` est défini côté daemon, le même secret doit être dans `AKASHA_PAIR_SECRET`.

## `flash.ps1` / `flash.sh`

Build + upload firmware (env produit : `fnk0104b`), puis moniteur série 115200.

```powershell
.\scripts\flash.ps1
.\scripts\flash.ps1 -Port COM5 -NoMonitor
.\scripts\flash.ps1 -BuildOnly
.\scripts\flash.ps1 -MonitorOnly -Port COM5
.\scripts\flash.ps1 -Env fnk0104b
```

```bash
./scripts/flash.sh
./scripts/flash.sh --port /dev/ttyACM0 --no-monitor
./scripts/flash.sh --build-only
./scripts/flash.sh --monitor-only --port COM5
```

Si l’upload échoue avec *Could not open COMx* : fermer tout moniteur série (autre terminal `pio device monitor`, IDE Serial Monitor), puis réessayer.

Crée `secrets.h` depuis l’exemple s’il manque (sinon préférer `provision.*`).

## Pare-feu Windows (Companion hors ligne / HTTP `-1`)

Si le Companion a du Wi‑Fi (`w=1`) mais `d=0` et les logs montrent `GET /api/... -> -1`, Windows bloque souvent le port **3876** depuis le LAN.

Exécuter **en administrateur** :

```powershell
.\scripts\open-lan-firewall.ps1
```

Puis vérifier que le daemon tourne avec `AKASHA_BIND=0.0.0.0`.


| Env | Support |
|-----|---------|
| `fnk0104b` | **Produit** (validé) |
| `fnk0104a` / `n` / `s` | Expérimental — voir [HARDWARE_FNK0104.md](../docs/HARDWARE_FNK0104.md) |
