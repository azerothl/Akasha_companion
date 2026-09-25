#pragma once

// Freenove FNK0104A — 2.8" 240x320 ILI9341, NO capacitive touch.
// UNVERIFIED on hardware — pins assumed same as FNK0104B except touch unused.
// Product support remains FNK0104B until A bring-up is signed off.

#define BOARD_NAME "FNK0104A"
#define BOARD_HAS_TOUCH 0
#define BOARD_LCD_WIDTH 240
#define BOARD_LCD_HEIGHT 320

// TFT (ILI9341 via HSPI) — same family as B
#define PIN_TFT_MISO 13
#define PIN_TFT_MOSI 11
#define PIN_TFT_SCLK 12
#define PIN_TFT_CS 10
#define PIN_TFT_DC 46
#define PIN_TFT_RST -1
#define PIN_TFT_BL 45

// No touch controller — stubs keep shared code compiling
#define PIN_TOUCH_SDA 16
#define PIN_TOUCH_SCL 15
#define PIN_TOUCH_RST 18
#define PIN_TOUCH_INT 17
#define TOUCH_I2C_ADDR 0x38

// ES8311 audio codec + I2S (same as B per Freenove 2.8" table)
#define PIN_I2S_MCK 4
#define PIN_I2S_BCK 5
#define PIN_I2S_WS 7
#define PIN_I2S_DOUT 8
#define PIN_I2S_DIN 6
#define PIN_PA_ENABLE 1
#define PIN_I2C_SDA 16
#define PIN_I2C_SCL 15
#define I2C_FREQ_HZ 400000
#define ES8311_I2C_ADDR 0x18

#define PIN_RGB 42
#define PIN_BUTTON 0
#define PIN_BAT_ADC 9
