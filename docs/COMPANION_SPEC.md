# Akasha Companion — spécification

Document de référence pour le client tactile ESP32 lié au daemon Akasha.  
Hardware de base : [Freenove ESP32-S3 Display FNK0104](https://docs.freenove.com/projects/fnk0104/en/latest/).

À mettre à jour à chaque phase clôturée.

---

## Vision

Un **compagnon de bureau vocal** : l’utilisateur parle à Akasha ; l’écran montre une **présence** (avatar), pas un dashboard.

1. **Mode principal = voix** — micro MEMS embarqué → STT daemon → réponse → TTS → haut-parleur du kit.
2. **Écran principal = avatar** interactif (yeux, clignements, idle naturel, réaction listening / thinking / speaking).
3. **Texte = second choix** — bascule manuelle vers une UI chat / clavier quand la voix est inadaptée.
4. **Image / média** — bascule contextuelle (aperçu image générée, carte, screenshot) puis retour avatar.
5. Thin client : pas de LLM sur l’ESP32 ; STT/TTS/LLM sur le daemon (`voice_router.yaml` + routeur LLM).

Parallèle board : parcours type [XiaoZhi sur FNK0104](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/xiaozhi.html) — **backend = daemon Akasha** (`/api/voice/*`, `/api/message`), pas un cloud tiers.

---

## Modes d’interface (surfaces)

| Surface | Rôle | Entrée | Sortie |
|---------|------|--------|--------|
| **Avatar** (défaut) | Présence + dialogue vocal | Voix (PTT / hold-to-talk ; wake optionnel plus tard) | Audio + animation visage |
| **Texte** | Fallback / précision | Clavier logiciel, phrases préfabriquées | Bulles tronquées + option « lire à voix haute » |
| **Image / média** | Contenu riche | Tap depuis avatar ou action | Image plein écran / galerie courte |
| **Settings** (discret) | Wi‑Fi, host daemon, volume | Tactile | — |

Navigation : swipe ou bouton / coin d’écran pour **Avatar ↔ Texte** ; ouverture **Image** uniquement si le daemon pousse un média ou l’utilisateur le demande.

Détail avatar : [UX_AVATAR_VOICE.md](UX_AVATAR_VOICE.md).

---

## Hardware cible

| Priorité | SKU | Notes |
|----------|-----|--------|
| P0 MVP | **FNK0104B** | 2.8″ tactile + **MEMS mic** + **speaker kit** (ES8311 / I2S) |
| P1 | FNK0104N / FNK0104S | Même pipeline audio, avatar plus grand |
| P2 | FNK0104A | Sans tactile — voix + bouton ; avatar lecture seule |

Détail pins / audio : [HARDWARE_FNK0104.md](HARDWARE_FNK0104.md).

| Capacité | Usage Companion |
|----------|-----------------|
| TFT + touch | Avatar + surfaces Texte / Image |
| **MEMS mic (ES8311)** | Capture voix (primaire) |
| **Speaker (kit, PH1.25)** | TTS / feedback sonore |
| Wi‑Fi | Client daemon |
| RGB LED | idle / listening / busy / error |
| Bouton | PTT, interrupt, cycle surface |
| Batterie (ADC) | Jauge (optionnel) |
| MicroSD | Cache wav / assets avatar |
| BLE | Provisioning (optionnel) |

---

## Pipeline vocal (référence)

```
[Mic I2S] → PCM/WAV court → POST /api/voice/stt
         → texte → POST /api/message → poll /api/tasks/:id
         → texte réponse → POST /api/voice/tts → PCM → [Speaker I2S]
```

Pendant ce temps l’avatar enchaîne les états : `listening` → `thinking` → `speaking` → `idle`.

Prérequis daemon : `voice_router.yaml` avec `stt.base_url` et `tts.base_url` ; `GET /api/voice/status` doit indiquer STT/TTS ok.  
Pour le Companion sur le Wi‑Fi : démarrer le daemon avec **`AKASHA_BIND=0.0.0.0`** (sinon il n’écoute que `127.0.0.1` et l’ESP32 reçoit HTTP `-1`).

---

## Décision d’archi — STT/TTS : daemon vs ESP32

**Décision :** ne **pas** embarquer de STT/TTS conversationnel sur l’ESP32. Qualité et latence perçue viennent du daemon ; l’ESP ne fait que capturer, jouer, et animer.

### Pourquoi pas de STT/TTS complets on-device

| Critère | Effet sur FNK0104 (S3 + LVGL + I2S) |
|---------|-------------------------------------|
| Latence | Un modèle correct est trop lourd ; un modèle minuscule est lent *et* mauvais → souvent **pire** qu’un round-trip LAN vers le PC/NAS |
| Qualité | Whisper-class / TTS neural inutilisables en parallèle de l’UI avatar |
| RAM / CPU | Concurrence framebuffer, Wi‑Fi, buffers audio — risque de jank avatar |
| Avatar | La sync bouche **n’exige pas** un TTS local (voir ci-dessous) |

### Ce qui reste **local** sur l’ESP32 (recommandé)

| Couche | Rôle | Phase |
|--------|------|-------|
| **Enveloppe RMS** pendant playback | Ouvre bouche / gestes `speaking` à partir du **PCM joué** (wav daemon) | 2 |
| **Samples UX locaux** | Bips, « OK », erreurs — feedback immédiat sans réseau | 2 |
| **VAD** (fin de parole) | Raccourcit les clips envoyés au STT → latence perçue ↓ | 4–5 |
| **Wake-word** (ex. ESP-SR) | Mains-libres sans vrai STT ; vocabulaire limité | 5 |
| Commandes fermées ESP-SR (option) | 10–20 intents offline — **pas** un chat libre | 5+ si besoin |

```
ESP32                                      Daemon (PC/NAS)
─────                                      ───────────────
PTT / (VAD, wake plus tard)                STT qualité (/api/voice/stt)
capture PCM court ─────────────────────►   LLM (/api/message)
enveloppe RMS sur PCM joué ◄────────────   TTS qualité (/api/voice/tts)
avatar LVGL + bips locaux
```

### Avatar : sync sans TTS embarqué

- Pendant `POST /api/voice/tts` → lecture I2S, calculer l’énergie (RMS / frame) du buffer **local**.
- États / « voie » émotionnelle = machine à états + hints daemon (`avatar_hint`), pas un moteur speech on-device.

### Quand reconsidérer du speech on-device

- Mode **offline** obligatoire (pas de daemon).
- Vocabulaire **fermé** uniquement (commandes ESP-SR).
- Autre plateforme nettement plus costaude (hors scope FNK0104).

Cette décision est figée pour le MVP et la Phase 2–4 ; toute dérogation exige une maj explicite de ce paragraphe + `CMP-029`.

---

## Table des phases

| Phase | Objectif | In / out | Dépendances | Risques |
|-------|----------|----------|-------------|---------|
| 0 Cadre | Repo, specs voix/avatar, BOM, contrat API | In: ce dépôt | — | — |
| 1 Bring-up | Display, touch, Wi‑Fi, status ; **init ES8311** (loopback mic→speaker test) | In: firmware | FNK0104B + speaker branché | Pins I2S / codec |
| 2 Voix + Avatar MVP | Avatar idle animé + PTT → STT → message → TTS | In: LVGL avatar + net voice | Daemon voice + LLM | Heap, latence, qualité mic |
| 3 Surfaces secondaires | Bascule Texte + Image ; lire réponse à voix haute | In: UI modes | Phase 2 | Densité 240×320 |
| 4 Temps réel & snapshot | SSE/poll, `GET /api/companion/snapshot`, pairing | In: monorepo | Auth device | Buffer events |
| 5 Presence / VAD | Hands-free VAD (RMS + hysteresis), policy daemon, PTT conservé | Done | Faux positifs / echo | Cooldown + quiet hours |
| 6 Produit | Flash tool, doc user, multi-SKU | Done | — | Support A/N/S |

---

## Exigences traçables

| ID | Phase | Exigence | Statut |
|----|-------|----------|--------|
| CMP-001 | 0 | Repo séparé ; specs + BOM | Fait |
| CMP-002 | 0 | Cible FNK0104B ; variantes N/S/A | Fait |
| CMP-017 | 0 | Mode principal **voix** ; texte = secondaire | Fait (spec) |
| CMP-018 | 0 | Home = **avatar** interactif ; surfaces Texte / Image commutables | Fait (spec) |
| CMP-029 | 0 | **Pas** de STT/TTS conversationnel on-device ; daemon via `/api/voice/*` | Fait (spec) |
| CMP-030 | 2 | Sync avatar `speaking` via **enveloppe RMS** du PCM en lecture (pas TTS local) | In progress (firmware) |
| CMP-031 | 2 | Samples UX locaux (bip / erreur) pour feedback immédiat | In progress (firmware) |
| CMP-003 | 1 | Build multi-SKU (`BOARD_FNK0104B`…) | Done (B produit ; A expérimental ; N/S stubs) |
| CMP-004 | 1 | Display + tactile ok | Fait (FNK0104B) |
| CMP-005 | 1 | Wi‑Fi + `GET /api/status` | Fait (LAN + `AKASHA_BIND=0.0.0.0`) |
| CMP-006 | 1 | Host/port/token en NVS | Fait (`secrets.h` Phase 1) |
| CMP-019 | 1 | ES8311 : lecture mic + lecture speaker (test tonalité / echo court) | Fait (driver Freenove + amp GPIO1 LOW) |
| CMP-020 | 1 | `GET /api/voice/status` → UI / LED si STT/TTS absents | Fait |
| CMP-021 | 2 | Avatar : idle (clignement, micro-mouvements) sans réseau | Done (firmware) |
| CMP-022 | 2 | États avatar : `idle`, `listening`, `thinking`, `speaking`, `error`, `offline` | Done (firmware) |
| CMP-023 | 2 | PTT (bouton ou hold tactile) → capture → `POST /api/voice/stt` | Done (firmware) |
| CMP-024 | 2 | Texte STT → `POST /api/message` + poll tâche | Done (firmware) |
| CMP-025 | 2 | Réponse → `POST /api/voice/tts` → lecture speaker | Done (firmware) |
| CMP-026 | 2 | Feedback erreur vocale (bip / avatar error) si STT/TTS/réseau fail | Done (firmware) |
| CMP-008 | 3 | Surface **Texte** : envoi clavier / chips, historique borné | Done (firmware) |
| CMP-009 | 3 | Actions rapides accessibles depuis Texte ou long-press avatar | Done (firmware) |
| CMP-027 | 3 | Surface **Image** : afficher média daemon / URL locale, retour avatar | Done (firmware) |
| CMP-028 | 3 | Gesture ou contrôle UI pour basculer Avatar ↔ Texte | Done (firmware ; draw-enable Phase 6) |
| CMP-010 | 2–3 | Timeout / erreur visibles (avatar + toast court) | Done (firmware) |
| CMP-011 | 4 | Sleep backlight + wake (touch / bouton / voix si VAD) | Done (firmware; wake touch/BOOT) |
| CMP-012 | 4 | Events SSE ou poll enrichi | Done (poll snapshot Companion) |
| CMP-013 | 4 | `GET /api/companion/snapshot` | Done (daemon + firmware) |
| CMP-014 | 4 | Pairing device | Done (daemon + NVS) |
| CMP-015 | 5 | Hands-free VAD (pas wake-word) ; policy daemon + NVS fallback ; PTT inchangé | Done (daemon + firmware) |
| CMP-016 | 6 | Script flash + doc user | Done (scripts + docs/user + Akasha_app) |

*(CMP-007 « Home statut » remplacé par CMP-021/022 — le statut reste en overlay discret sur l’avatar.)*

---

## Principes d’architecture

1. **Voice-first, screen-as-presence** — l’écran n’est pas un TUI miniature.
2. **Thin client** — STT/TTS/LLM côté daemon ; l’ESP encode/décode audio et anime (**CMP-029**).
3. **Latence perçue** — VAD / clips courts / bips locaux ; pas un moteur speech embarqué.
4. **Avatar suit le PCM** — enveloppe pendant playback ; pas besoin de TTS on-device (**CMP-030**).
5. **LAN first** — HTTP clair ; payloads audio bornés (ex. clips ≤ 10–15 s, mono 16 kHz).
6. **Allowlist API** — voir [API_CONTRACT.md](API_CONTRACT.md).
7. **Multi-SKU** — board files (display + I2S pins).
8. **Licence** — ne pas republier le zip Freenove (CC BY-NC-SA) tel quel.

---

## Hors scope (MVP)

- STT / TTS **conversationnels** on-device (voir décision d’archi ci-dessus)
- LLM on-device
- Wake-word on-device (Phase 5 = VAD RMS, pas wake-word)
- Remplacement UI Tauri / Code Studio
- Tools dangereux depuis le companion (sauf allowlist)

---

## Références

- Freenove FNK0104 : https://docs.freenove.com/projects/fnk0104/en/latest/
- Audio (Music / MEMS-MIC) : https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/7_Music.html
- Daemon : `/api/voice/status`, `/api/voice/stt`, `/api/voice/tts`, `/api/message`, `/api/tasks/:id`
- [API_CONTRACT.md](API_CONTRACT.md) · [UX_AVATAR_VOICE.md](UX_AVATAR_VOICE.md) · [HARDWARE_FNK0104.md](HARDWARE_FNK0104.md)
