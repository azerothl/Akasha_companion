# Akasha Companion — spécification

Document de référence pour le client tactile ESP32 lié au daemon Akasha.  
Hardware de base : [Freenove ESP32-S3 Display FNK0104](https://docs.freenove.com/projects/fnk0104/en/latest/).

À mettre à jour à chaque phase clôturée.

---

## Vision

Un **compagnon de bureau** (smart display) qui :

1. Affiche l’état du daemon Akasha (online, modèle / backend, charge légère).
2. Permet des **actions rapides** (brief, résumé, stop tâche).
3. Offre un **chat court** (message → réponse tronquée) via `POST /api/message`.
4. Reste utilisable hors clavier PC ; pas de LLM local sur l’ESP32.

Inspiration UX board : tutoriels Freenove LVGL + parcours type assistant (ex. [XiaoZhi sur FNK0104](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/xiaozhi.html)) — **côté Akasha le backend est le daemon maison**, pas un cloud tiers.

---

## Hardware cible

| Priorité | SKU | Notes |
|----------|-----|--------|
| P0 MVP | **FNK0104B** | 2.8″ tactile 240×320 ILI9341 — meilleur compromis taille / perf LVGL |
| P1 | FNK0104N / FNK0104S | 3.5″ / 4.0″ 320×480 — mêmes écrans UI, assets plus grands |
| P2 | FNK0104A | Non-tactile — home status + bouton physique seulement |

Détail pins / I/O : [HARDWARE_FNK0104.md](HARDWARE_FNK0104.md).

Capacités board utilisées (selon phase) :

| Capacité Freenove | Usage Companion |
|-------------------|-----------------|
| TFT + touch | UI LVGL |
| Wi‑Fi | Client daemon |
| RGB LED | État connexion / tâche |
| Bouton | Wake / cancel / page suivante |
| Batterie (ADC) | Indicateur charge (optionnel) |
| Speaker PH1.25 | TTS / alertes (phase tardive) |
| MicroSD | Cache assets UI / logs (optionnel) |
| BLE | Provisioning (optionnel) |

---

## Table des phases

| Phase | Objectif | In / out | Dépendances | Risques |
|-------|----------|----------|-------------|---------|
| 0 Cadre | Repo, specs, BOM, contrat API | In: ce dépôt | — | Scope creep |
| 1 Bring-up | Display, touch, Wi‑Fi, `GET /api/status` | In: firmware minimal | Board FNK0104B | Mauvais flags PSRAM / flash |
| 2 MVP UI | Home + chat + 3 actions | In: LVGL screens | Daemon :3876 | Mémoire heap |
| 3 Temps réel | Polling rapide → SSE ; sleep/wake | In: events | Contrat SSE daemon | Latence / buffer |
| 4 Snapshot API | `GET /api/companion/snapshot` | In: monorepo Akasha | Auth device | Versioning API |
| 5 Audio | Speaker / push-to-talk | Out (roadmap) | Mic externe éventuel | Audio + Wi‑Fi concurrent |
| 6 Produit | Packaging, flash tool, doc user | Out (roadmap) | Akasha_app | Support multi-SKU |

---

## Exigences traçables

| ID | Phase | Exigence | Statut |
|----|-------|----------|--------|
| CMP-001 | 0 | Repo séparé du monorepo Rust ; specs + BOM | Fait |
| CMP-002 | 0 | Cible primaire documentée : FNK0104B ; variantes N/S/A listées | Fait |
| CMP-003 | 1 | Build firmware sélectionnable par macro / flag SKU (`FNK0104B`…) | Planifié |
| CMP-004 | 1 | Affichage + tactile opérationnels (demo LVGL ou hello) | Planifié |
| CMP-005 | 1 | Connexion Wi‑Fi + `GET /api/status` → écran Connected/Offline | Planifié |
| CMP-006 | 1 | Host/port/token stockés en NVS (pas de secrets commités) | Planifié |
| CMP-007 | 2 | Écran **Home** : heure, statut daemon, indicateur RGB | Planifié |
| CMP-008 | 2 | Écran **Chat** : envoi message, poll `GET /api/tasks/:id`, affichage réponse bornée | Planifié |
| CMP-009 | 2 | Écran **Actions** : ≥ 3 boutons mappés (ex. brief, résumé, stop) | Planifié |
| CMP-010 | 2 | Timeout / erreur réseau visibles à l’utilisateur | Planifié |
| CMP-011 | 3 | Mode sommeil backlight + wake tactile ou bouton | Planifié |
| CMP-012 | 3 | Abonnement events (SSE ou équivalent) quand disponible côté daemon | Planifié |
| CMP-013 | 4 | Consommation de `GET /api/companion/snapshot` (1 round-trip = home) | Planifié |
| CMP-014 | 4 | Pairing device (token court / QR côté daemon) | Planifié |
| CMP-015 | 5 | Sortie audio speaker (alerte / TTS court) | Roadmap |
| CMP-016 | 6 | Script flash + README utilisateur | Roadmap |

---

## Principes d’architecture

1. **Thin client** — pas d’orchestration d’outils sur l’ESP32.
2. **LAN first** — HTTP clair vers `http://<daemon>:3876` ; TLS hors MVP.
3. **UI bornée** — textes tronqués ; pas de Markdown riche en MVP.
4. **Multi-SKU** — abstraction display (résolution + driver) derrière une couche board.
5. **Licence** — code Akasha Companion en licence ouverte du projet ; ne pas republier tel quel le zip Freenove (CC BY-NC-SA) sans analyse.

---

## Hors scope (MVP)

- Inference LLM sur device
- Remplacement de l’UI Tauri / Code Studio
- Contrôle shell / tools dangereux depuis l’écran (sauf actions allowlistées)
- Multi-daemon / cluster depuis le companion

---

## Références

- Docs Freenove FNK0104 : https://docs.freenove.com/projects/fnk0104/en/latest/
- Exemples : https://github.com/Freenove/Freenove_ESP32_S3_Display
- Daemon Akasha : `GET /api/status`, `POST /api/message`, `GET /api/tasks/:id` (port **3876**)
- Contrat détaillé : [API_CONTRACT.md](API_CONTRACT.md)
