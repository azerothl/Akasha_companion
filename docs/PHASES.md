# Phases (pointer)

Full phase table, dependencies, and traceable requirements (**CMP-001** …) live in [COMPANION_SPEC.md](COMPANION_SPEC.md#table-des-phases).

| Phase | Summary |
|-------|---------|
| 0 | Repo, specs, BOM, API contract |
| 1 | Display, touch, Wi-Fi, `GET /api/status`, NVS |
| 2 | MVP UI: home, chat, quick actions |
| 3 | Sleep/wake, SSE or polling for events |
| 4 | `GET /api/companion/snapshot`, device pairing |
| 5 | Audio (roadmap) |
| 6 | Flash tooling, user docs, multi-SKU packaging |

Update requirement statuses in `COMPANION_SPEC.md` when a phase closes.
