#pragma once

// Freenove FNK0104N — 3.5" 320x480 ST77922 + touch.
// UNVERIFIED — needs hardware bring-up. TFT/touch pins not frozen in Companion yet.
// Known I2S/I2C from Freenove public table (see docs/HARDWARE_FNK0104.md):
//   I2S_MCK=17 BCK=18 WS=21 DOUT=15 DIN=16 PA=1 I2C=38/39 BAT_ADC≈8
//
// Product support = FNK0104B only until this file is filled and tested.

#error "FNK0104N unsupported until bring-up — use BOARD_FNK0104B (product). Draft I2S pins in HARDWARE_FNK0104.md"
