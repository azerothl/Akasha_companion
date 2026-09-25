# Hardware — Freenove ESP32-S3 Display (FNK0104)

Source officielle : [docs.freenove.com — FNK0104](https://docs.freenove.com/projects/fnk0104/en/latest/)  
Repo exemples : [Freenove/Freenove_ESP32_S3_Display](https://github.com/Freenove/Freenove_ESP32_S3_Display)

---

## Variantes écran

D’après la [fiche modèles Freenove](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/Freenove_ESP32S3_Display.html) :

| Modèle | Taille | Résolution | Driver LCD | Tactile | Rôle Akasha Companion |
|--------|--------|------------|------------|---------|------------------------|
| **FNK0104B** | 2.8″ | 240×320 | ILI9341 | Oui (capacitif) | **Produit supporté** |
| FNK0104A | 2.8″ | 240×320 | ILI9341 | Non | Expérimental (`env:fnk0104a`, pins ≈ B) |
| FNK0104N | 3.5″ | 320×480 | ST77922 | Oui | Stub — unsupported until bring-up |
| FNK0104S | 4.0″ | 320×480 | ST7796 | Oui | Stub — unsupported until bring-up |

> Note : FNK0104S parfois listé ST7789 vs ST7796 — **valider sur PCB / sketch Freenove**.

Build :

```text
pio run -e fnk0104b          # produit
pio run -e fnk0104a          # expérimental
pio run -e fnk0104n|fnk0104s # attendu: #error board header
```

---

## MCU & mémoire

- SoC : **ESP32-S3** (Wi‑Fi + BLE)
- Flash / PSRAM : selon module — **PSRAM obligatoire** pour avatar LVGL + buffers audio I2S
- Alim : USB-C ; batterie Li-ion optionnelle 3.7–4.2 V (MX1.25)
- Codec audio : **ES8311** (I2S + I2C) — entrée MEMS mic, sortie speaker

---

## Audio (critique pour le produit)

Le kit Freenove FNK0104 inclut typiquement ([store](https://store.freenove.com/products/fnk0104)) :

| Élément | Emplacement | Rôle Companion |
|---------|-------------|----------------|
| **MEMS microphone** | Sur la carte (via ES8311) | Capture voix (mode principal) |
| **Speaker** | Fourni dans le kit, connecteur **PH1.25** | Lecture TTS / feedback |
| ES8311 | Sur la carte | Codec I2S full-duplex (selon config) |

Référence tutoriel : [Chapter 7 Music](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/7_Music.html) (playback + section **MEMS-MIC**).

### Pins I2S (exemples Freenove — à figer par SKU)

| Signal | **FNK0104B / A (2.8″)** | FNK0104N (3.5″) |
|--------|-------------------------|-----------------|
| I2S_MCK | **4** | 17 |
| I2S_BCK | **5** | 18 |
| I2S_WS | **7** | 21 |
| I2S_DOUT | **8** | 15 |
| I2S_DIN | **6** | 16 |
| PA / AP_ENABLE | **1** | 1 |
| I2C SDA / SCL | **16 / 15** | 38 / 39 |

TFT FNK0104B (ILI9341 HSPI) : MOSI 11, SCLK 12, MISO 13, CS 10, DC **46**, BL **45**, RST tied.  
Touch FT6336U : SDA 16, SCL 15, RST 18, INT 17. RGB WS2812 : GPIO **42**. Bouton : GPIO **0**.

Voir `firmware/boards/fnk0104b.h`.

### Contraintes audio firmware

- Capture MVP : mono, **16 kHz**, 16-bit, clips **≤ 10–15 s** (upload STT).
- Playback : wav/PCM renvoyé par `POST /api/voice/tts` (décoder data URL côté device).
- **Pas** de moteur STT/TTS conversationnel on-device (CPU/RAM + qualité) — voir `COMPANION_SPEC.md` CMP-029.
- Pendant playback : calculer RMS pour l’avatar (`CMP-030`) ; samples courts en flash pour UX (`CMP-031`).
- Éviter full-duplex simultané mic+speaker en MVP si écho ; PTT coupe le TTS.
- Brancher le speaker du kit avant les tests Phase 1.

---

## Périphériques (hors audio)

| Feature | Usage Companion |
|---------|-----------------|
| Serial | Debug |
| RGB LED | idle / listening / busy / error |
| Button (+ IRQ) | PTT alternatif, interrupt TTS, cycle surface |
| Battery ADC | Jauge (GPIO9 sur 2.8″ ; GPIO8 sur 3.5″ — exemples Freenove) |
| SD MMC | Cache wav / sprites avatar |
| BLE | Provisioning (optionnel) |
| TFT / Touch / LVGL | Avatar + Texte + Image |

Tutoriels : [Touch](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN.html) · [XiaoZhi](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/xiaozhi.html)

---

## Sélection firmware

```c
#define BOARD_FNK0104B   // produit
// #define BOARD_FNK0104A   // expérimental
// #define BOARD_FNK0104N   // stub #error
// #define BOARD_FNK0104S   // stub #error
```

Ou PlatformIO `-e fnk0104b` / `fnk0104a` / …  
Chaque board file : résolution, driver TFT, touch, **pins I2S/ES8311**, RGB, bouton, BAT, SD.

---

## Contraintes UI

| SKU | Résolution | Surface défaut | Secondaires |
|-----|------------|----------------|-------------|
| B / A | 240×320 | **Avatar** plein cadre | Texte / Image en overlay ou plein écran |
| N / S | 320×480 | Avatar + chrome un peu plus riche | Idem |

Surfaces : voir [UX_AVATAR_VOICE.md](UX_AVATAR_VOICE.md) et [COMPANION_SPEC.md](COMPANION_SPEC.md).

---

## Checklist bring-up (Phase 1)

- [ ] Identifier le SKU
- [x] Brancher le **speaker** PH1.25 du kit
- [x] Hello display + touch
- [ ] PSRAM OK
- [x] ES8311 init + tonalité speaker (+ loopback)
- [ ] Enregistrement mic court → playback local (loopback)
- [ ] Wi‑Fi + `GET /api/status` + `GET /api/voice/status`
- [ ] Documenter pins dans `firmware/boards/`

---

## Accessoires

Voir [../hardware/BOM.md](../hardware/BOM.md).
