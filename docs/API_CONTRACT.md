# Contrat API — Companion ↔ Daemon Akasha

Base URL MVP : `http://<daemon-host>:3876`  
Auth MVP : header `Authorization: Bearer <token>` si le daemon l’exige (même politique que l’UI locale) ; sinon LAN trusted + token device en NVS dès Phase 4.

Le companion **ne** parle **qu’**aux routes listées ici (allowlist). Toute nouvelle route → mise à jour de ce fichier + exigence `CMP-*`.

---

## Phase 1–2 (existant daemon)

### `GET /api/status`

Santé / état runtime.

**Usage companion :** écran Home + LED RGB (vert = ok, rouge = unreachable).

### `GET /` (optionnel)

Health check minimal si `/api/status` indisponible.

### `POST /api/message`

```json
{
  "message": "string",
  "session_id": "companion-<device_id>" 
}
```

**Réponse :** `{ "task_id": "..." }` (champs selon daemon).

**Usage :** écran Chat ; `session_id` stable par device pour continuité courte.

### `GET /api/tasks/:id`

Poll statut / résultat jusqu’à `done` / `failed` / timeout UI (ex. 120 s).

**Usage :** afficher `result` / dernier message assistant tronqué (ex. 800 caractères).

### Actions rapides (Phase 2)

Mapper les boutons sur des messages ou endpoints déjà stables, par ex. :

| Bouton UI | Appel suggéré |
|-----------|----------------|
| Morning brief | Message NL ou route Life layer si exposée (`/api/channels/notify` / schedules — à figer en Phase 2) |
| Résumé | `POST /api/message` avec prompt court allowlisté |
| Stop | endpoint cancel tâche si disponible, sinon message « stop » documenté |

Les libellés exacts seront figés quand le firmware Actions sera implémenté (éviter les tools dangereux).

---

## Phase 4 (à ajouter dans monorepo Akasha)

### `GET /api/companion/snapshot` (proposé)

Une réponse JSON condensée pour peindre Home en **un** round-trip.

```json
{
  "schema_version": 1,
  "daemon": {
    "ok": true,
    "version": "x.y.z",
    "uptime_s": 0
  },
  "llm": {
    "provider": "string",
    "model": "string"
  },
  "tasks": {
    "active": 0,
    "last_task_id": null
  },
  "notify": {
    "unread": 0,
    "headline": null
  },
  "brief": {
    "text": null,
    "updated_at": null
  }
}
```

**Règles :**

- Payload &lt; ~4 KiB recommandé (heap ESP32).
- Pas de secrets, pas de transcripts complets.
- `schema_version` incrémenté de façon incompatible uniquement.

### Pairing (proposé)

- `POST /api/companion/pair` (code court affiché sur PC / QR) → token device
- Token stocké NVS ; révocation côté daemon

Détail d’implémentation = PR Akasha ; ce dépôt consomme seulement le contrat.

---

## Phase 3 — Events

Cible alignée sur le contrat events daemon (SSE / WebSocket).  
En attendant : poll `GET /api/status` + tâche active toutes les 1–2 s pendant busy.

---

## Erreurs & UX

| Situation | UI |
|-----------|-----|
| Timeout TCP | Bannière Offline + LED rouge |
| HTTP 4xx/5xx | Message court + retry |
| Tâche failed | Afficher extrait erreur |
| Réponse trop longue | Truncate + « … » |

---

## Hors contrat companion

- Upload gros fichiers / studio / terminal PTY
- Invocation tools arbitraires
- Accès vault / clés brutes
