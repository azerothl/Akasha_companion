# Hardware — Freenove ESP32-S3 Display (FNK0104)

Source officielle : [docs.freenove.com — FNK0104](https://docs.freenove.com/projects/fnk0104/en/latest/)  
Repo exemples : [Freenove/Freenove_ESP32_S3_Display](https://github.com/Freenove/Freenove_ESP32_S3_Display)

---

## Variantes écran

D’après la [fiche modèles Freenove](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/Freenove_ESP32S3_Display.html) :

| Modèle | Taille | Résolution | Driver LCD | Tactile | Rôle Akasha Companion |
|--------|--------|------------|------------|---------|------------------------|
| **FNK0104B** | 2.8″ | 240×320 | ILI9341 | Oui (capacitif) | **Cible MVP** |
| FNK0104N | 3.5″ | 320×480 | ST77922 | Oui | Variante large |
| FNK0104S | 4.0″ | 320×480 | ST7796 | Oui | Variante large |
| FNK0104A | 2.8″ | 240×320 | ILI9341 | Non | Status + bouton uniquement |

> Note GitHub Freenove : FNK0104S parfois listé ST7789 vs ST7796 dans les docs — **valider le driver sur le PCB / sketch Freenove** avant de figer les pins firmware.

---

## MCU & mémoire

- SoC : **ESP32-S3** (Wi‑Fi + BLE)
- Flash / PSRAM : selon module soudé (souvent N8R8 ou équivalent) — **vérifier le marquage** et activer PSRAM correctement (souvent `qio_opi` sous Arduino-ESP32)
- Alim : USB-C ; batterie Li-ion optionnelle **3.7–4.2 V** (connecteur MX1.25) — non fournie
- LVGL + Wi‑Fi + JSON : **PSRAM fortement recommandé** ; sans PSRAM, rester sur UI minimale

---

## Périphériques utiles (tutoriels Freenove)

Les chapitres Touch Tutorial couvrent notamment :

| Chapitre / feature | Usage Companion |
|--------------------|-----------------|
| Serial | Debug |
| RGB LED | État (offline / idle / busy / error) |
| Button (+ interrupt) | Wake, cancel, cycle d’écrans |
| Battery voltage (ADC) | Jauge batterie (GPIO9 sur 2.8″ ; GPIO8 sur 3.5″ d’après exemples Freenove) |
| SD MMC | Cache assets / logs |
| Music / speaker (PH1.25) | Alertes / TTS court (phase 5) |
| BLE | Provisioning Wi‑Fi (optionnel) |
| Wi‑Fi web server | Référence ; Companion = **client HTTP**, pas serveur principal |
| TFT / Touch / Drawing | Base display |
| LVGL (+ picture, timer, RGB, music, multifunction) | Stack UI retenue |

Tutoriels : [Touch](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN.html) · [NonTouch](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN_NonTouch.html) · [XiaoZhi](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/xiaozhi.html) · [Board Test](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/Board_Test.html)

### Batterie (ADC)

- Diviseur ×0.5 vers l’ADC (pleine charge 4.2 V → ~2.1 V mesurés)
- Exemples Freenove : `BAT_ADC_PIN` **9** (FNK0104A/B) ou **8** (FNK0104N) — à confirmer sur schéma du SKU

### MicroSD

- Interface SDMMC (pins selon SKU ; exemples Freenove : CMD/CLK/D0–D3 distincts pour N vs AB)
- Carte **non fournie** avec le kit

### Speaker

- Connecteur **PH1.25** — haut-parleur externe à prévoir dans la BOM phase 5

---

## Sélection firmware (convention projet)

Un seul firmware multi-SKU via **une** macro active :

```c
#define BOARD_FNK0104B   // MVP
// #define BOARD_FNK0104N
// #define BOARD_FNK0104S
// #define BOARD_FNK0104A
```

Chaque board file définit : résolution, driver TFT, présence touch, pins RGB / bouton / BAT / SD.

---

## Contraintes de design UI

| SKU | Résolution | Orientation recommandée MVP | Densité UI |
|-----|------------|------------------------------|------------|
| B / A | 240×320 | Portrait | Gros boutons, clavier minimal ou phrases préfabriquées |
| N / S | 320×480 | Portrait | Plus de texte visible, même flux d’écrans |

Écrans MVP (tous SKU) :

1. **Home** — statut + heure  
2. **Chat** — historique court (2–4 bulles)  
3. **Actions** — grille 2×2 max  

---

## Checklist bring-up (Phase 1)

- [ ] Identifier le SKU (étiquette / taille écran)
- [ ] Flash sketch Freenove Board Test ou LVGL hello
- [ ] Confirmer PSRAM détectée au boot
- [ ] Wi‑Fi joint au LAN du daemon
- [ ] `curl` / client board → `GET http://<daemon>:3876/api/status`
- [ ] Documenter pins réellement utilisées dans `firmware/boards/`

---

## Hors board (accessoires)

Voir [../hardware/BOM.md](../hardware/BOM.md).
