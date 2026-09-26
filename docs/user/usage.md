# Companion — Usage & dépannage

## Gestures (avatar)

| Action | Effet |
|--------|--------|
| Hold zone centrale / BOOT ≥ 300 ms | PTT (push-to-talk) |
| Tap court | Overlay statut (`d=`, `stt=`, `tts=`, `hf=`) |
| Swipe gauche | Surface **Texte** |
| Swipe droite (depuis Texte) | Retour **Avatar** |
| Long-press | **Réglages** (volume + style avatar) |
| Tap image | Fermer → Avatar |

## Réglages

Long-press depuis **Avatar** (ou Texte) :

- **Volume** : − / + (pas de 5) ou tap sur la barre — persisté NVS, appliqué au codec ES8311
- **Avatar** : cycle `Mignon` → `Orbe` → `Points`
- **Bip PTT** : ON/OFF (silence au début de l’écoute)
- **Connexion Akasha** : découvrir les daemons LAN, ou saisir un host custom (VPS)
- **Tester le son** : bip court au niveau choisi
- **Retour** (ou swipe droite)

### Connexion

1. **Découvrir** — UDP 3877 + scan HTTP local
2. Tap une entrée → bascule host/port + re-pair
3. **Custom** — clavier : `mon.vps.com` ou `10.0.0.5:3876`

Le daemon LAN doit tourner avec `AKASHA_BIND=0.0.0.0` pour la découverte UDP / HTTP.

## Surface Texte

- Historique court + chips : Brief, Stop, Répète, Lire, Mic, **HF**
- Clavier AZERTY + OK pour envoyer un message texte
- **Mic** = PTT sans quitter le contexte Texte
- **HF** = toggle mains-libres VAD (NVS + policy daemon)
- Long-press = Réglages

Le visage avatar ne doit **pas** réapparaître tout seul sur le clavier. Si c’est le cas, reflasher le firmware Phase 6+ (`avatar_set_drawing_enabled`).

## Hands-free VAD (Phase 5)

- Off par défaut (privacy)
- Activer : chip **HF**, ou `POST /api/companion/presence/config` avec `{"enabled":true}`
- Conditions : Wi‑Fi OK, daemon joignable, STT configuré, écran allumé
- Suspendu pendant PTT, TTS, sleep backlight, conversation busy
- Segments trop courts → drop discret (`…`), pas de bip agressif

Policy : [daemon.md](daemon.md)

## Sleep backlight

Après ~60 s d’inactivité → rétroéclairage off. Wake : touch ou BOOT. Le VAD est suspendu écran éteint.

## États avatar

`offline` · `idle` · `listening` · `thinking` · `speaking` · `error` · `notify`  
Détail UX : [../UX_AVATAR_VOICE.md](../UX_AVATAR_VOICE.md)

## Dépannage

| Symptoôme | Piste |
|-----------|--------|
| Upload *Could not open COMx* | Fermer moniteur série ; `flash.ps1 -Port COMx -NoMonitor` |
| `w=1 d=0` / HTTP `-1` | Daemon pas en `AKASHA_BIND=0.0.0.0`, mauvaise IP, **ou pare-feu Windows** — lancer `scripts/open-lan-firewall.ps1` en admin |
| Avatar ambre / « STT/TTS? » | `GET /api/voice/status` → configurer `voice_router.yaml` |
| Swipe Texte → clavier puis avatar | Firmware &lt; fix draw-enable ; reflasher Phase 6 |
| HF ne s’active pas | Pref NVS + `vad_enabled` snapshot ; quiet hours ; STT off |
| Faux départs VAD | Monter `threshold_rms`, `cooldown_ms` via presence config |
| Pas de son TTS | Speaker PH1.25 branché ; amp GPIO1 (déjà géré firmware B) |
| Pairing 403 | Aligner `AKASHA_PAIR_SECRET` et `AKASHA_COMPANION_PAIR_SECRET` |

Logs série utiles : `hb … w= d= hf= bl=` (Wi‑Fi, daemon, hands-free, backlight).
