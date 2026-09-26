# BOM — Akasha Companion (FNK0104)

## Obligatoire (MVP)

| Item | Réf. / notes | Qté |
|------|----------------|-----|
| Freenove ESP32-S3 Display **FNK0104B** | 2.8″ tactile, ILI9341, **MEMS mic** onboard (ES8311) — [docs](https://docs.freenove.com/projects/fnk0104/en/latest/) · [store](https://store.freenove.com/products/fnk0104) | 1 |
| **Speaker** (inclus kit) | Connecteur **PH1.25** — à brancher pour TTS | 1 |
| Câble USB-C (data) | Flash + alim (souvent inclus) | 1 |

Contenu kit typique Freenove : module écran, speaker, câble USB-C, câbles de liaison — vérifier la boîte.

## Recommandé

| Item | Notes | Qté |
|------|--------|-----|
| Support / coque desk | Impression 3D ; laisse le mic et le speaker dégagés | 1 |
| MicroSD FAT32 | Cache clips / sprites avatar | 1 |

## Variantes

| Item | Statut Companion |
|------|------------------|
| **FNK0104B** | **Produit** — validé |
| FNK0104A | Expérimental (`pio run -e fnk0104a`) — sans tactile |
| FNK0104N / S | Stubs board — non flashables jusqu’à bring-up pins |

## Optionnel

| Item | Notes |
|------|--------|
| Batterie Li-ion 3.7 V + MX1.25 | USB préféré pour la sécurité |

## Non inclus

- Batterie et SD (selon kit)
- Daemon Akasha + services STT/TTS (`voice_router.yaml`) sur le LAN

## Liens

- Audio Freenove : https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/7_Music.html
- Support : support@freenove.com
