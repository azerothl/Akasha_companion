# UX — Avatar & voix

Surface par défaut du companion : une **présence graphique** pilotée par l’état du dialogue vocal.

---

## Objectif

L’utilisateur doit sentir qu’il parle **à** Akasha, pas qu’il commande un panneau.  
Le texte et les images sont des **overlays / surfaces** temporaires.

---

## États avatar (`AvatarState`)

| État | Trigger | Animation (MVP) | Audio |
|------|---------|-----------------|-------|
| `offline` | Pas de daemon / Wi‑Fi | Yeux fermés / gris | — |
| `idle` | Connecté, rien en cours | Clignements fluides, saccades regard, respiration légère (yeux + bouche) | Silence |
| `listening` | PTT / VAD | Yeux grands, halo soft, bouche pulse | Capture mic |
| `thinking` | STT ok → tâche | Regard latéral, squint léger, 3 points | — |
| `speaking` | Lecture TTS | Bouche sync RMS, bounce léger | Sortie speaker |
| `error` | STT/TTS/HTTP fail | Secousse + frown ; mood `concerned` | Bip court |
| `notify` | Snapshot / event | Regard vers badge, bounce | Ding optionnel |

**Moods courts** (`avatar_set_mood`) : `happy` ~2 s après une réponse OK (sourire + squint) ; `concerned` sur échec. Pas de NLP on-device.

Transitions interpolées (lerp) ; pas de hard cut sauf `error`.

### Style présence minimale

- Fond sombre ; **pas de gros cercle « tête »** — seulement **yeux ovales + bouche** vectoriels (TFT_eSPI).
- Yeux : blanc + iris teinté par état + pupille + highlight ; clignement = compression hauteur.
- Bouche : arc sourire / ellipse ouverte selon `mouth_open` (RMS en `speaking`).
- Chrome : pastille d’état compacte ; hint « tenir=parler » discret.
- Sprite PSRAM pour push atomique ; `avatar_set_drawing_enabled(false)` sur surfaces Texte/Image.

---

## Interactivité tactile (sur avatar)

| Gesture | Action |
|---------|--------|
| Hold zone centrale / bouton | PTT (push-to-talk) — **inchangé en Phase 5** |
| Tap court | Afficher overlay statut (daemon, modèle, `hf=`) 2 s |
| Swipe horizontal | Avatar ↔ Texte |
| Swipe vertical (option) | Volume / brightness |
| Long-press | **Réglages** : volume, style avatar, bip PTT, connexion |
| Double-tap | Interrupt TTS / cancel tâche si supporté |
| Chip **HF** (Texte) | Toggle mains-libres (NVS + policy daemon) |

### Styles avatar (`AvatarStyle`)

| Style | Description |
|-------|-------------|
| `Mignon` (défaut) | Yeux ovales + bouche, présence minimale |
| `Orbe` | Disque visage + yeux/bouche |
| `Points` | Deux points lumineux + bouche simple |

Volume : 0–100 via codec ES8311 (+/− 5, ou tap sur la barre). Bouton « Tester le son ».

**Bip PTT** : ON/OFF — désactive le bip au début de l’écoute (hold pour parler).

### Connexion Akasha

Réglages → **Connexion Akasha** :

- **Découvrir** : probe UDP `3877` (`AKASHA_DISCOVER`) + scan HTTP LAN ciblé
- Tap une instance listée → host/port NVS (`custom_endpoint`)
- **Custom** : saisir `host` ou `host:port` (VPS distant inclus)
- `*` à côté de l’endpoint = override utilisateur (secrets.h ne réécrit plus host/port)

---

## Style graphique (contraintes ESP)

- **Vector TFT_eSPI** (cercles / ellipses / arcs) — LVGL / atlas bitmap plus tard si besoin.
- Budget : sprite plein écran en PSRAM ; hors framebuffer viser léger.
- Palette teal / ambre / coral selon état (éviter le « purple AI cliché »).
- Sur 240×320 : **présence yeux+bouche** centrée ; chrome minimal.

### Naturalité

1. Timers blink + saccades idle.
2. Envelope RMS → ouverture bouche en `speaking`.
3. Moods `happy` / `concerned` après succès / échec de tâche (pas d’analyse de sentiment LLM).

### Sync bouche sans TTS embarqué

Le TTS reste sur le daemon (`POST /api/voice/tts`). Pendant la lecture I2S du wav reçu :

1. Calculer l’**énergie RMS** (ou pic) par frame du buffer PCM **local**.
2. Mapper l’énergie → ouverture bouche / mâchoire / légère poussée de tête.
3. Fin du buffer → transition `speaking` → `idle`.

Pas de phonèmes / lip-sync linguistique en MVP. Les bips d’erreur / confirmations peuvent être des **samples flash** (latence zéro réseau).  
Décision produit : [COMPANION_SPEC.md — STT/TTS daemon vs ESP32](COMPANION_SPEC.md#décision-darchi--stttts--daemon-vs-esp32).

---

## Surface Texte

- Historique 2–4 messages, police lisible, truncate.
- Chips d’actions (« brief », « stop », « répète »).
- Bouton mic pour revenir au flux vocal sans quitter le contexte session.
- Chip **HF** : active/désactive le mode mains-libres VAD (persistance NVS ; sync policy daemon).
- « Lire » → `POST /api/voice/tts` + bascule visuelle `speaking` (avatar miniature ou full).

## Surface Image

- Plein écran ou quasi ; pinch/zoom non requis en MVP (fit + tap pour fermer).
- Sources : URL renvoyée par tâche, data URL bornée, fichier SD cache.
- Retour : tap / bouton → Avatar (état `idle` ou `notify`).

---

## Hands-free VAD (Phase 5)

- **Pas de wake-word** : détection énergie RMS locale + hysteresis (`pre-speech` → `speech` → `hangover` → `cooldown`).
- Envoi STT seulement sur segments validés (`min_speech_ms` … `max_speech_ms`).
- Segments trop courts → `speech_dropped` + overlay discret `...` (pas de bip agressif).
- Suspendu si : backlight off, conversation busy, PTT, TTS en cours (anti-echo), STT off, daemon offline.
- Privacy : mode **off par défaut** (policy daemon `enabled: false` + NVS `hands_free: false`).

---

## Accessibilité / confort

- Volume TTS borné + mute soft.
- Indicateur clair « j’écoute » (légal / confiance) pendant capture PTT **et** VAD.
- Écoute continue uniquement si HF activé explicitement (chip / policy).
