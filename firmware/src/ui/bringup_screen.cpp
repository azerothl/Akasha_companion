#include "bringup_screen.h"

#include <TFT_eSPI.h>
#include <Wire.h>

#include "board.h"

static TFT_eSPI tft;
static String lines[10];
static int16_t last_tx = -1;
static int16_t last_ty = -1;

static void draw_all() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.printf("Akasha Companion\n%s %dx%d\n", BOARD_NAME, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (uint8_t i = 0; i < 10; ++i) {
    if (lines[i].length() == 0) {
      continue;
    }
    tft.setCursor(4, 36 + i * 14);
    tft.print(lines[i]);
  }
  if (last_tx >= 0) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(4, BOARD_LCD_HEIGHT - 16);
    tft.printf("Touch: %d,%d", last_tx, last_ty);
  }
}

bool bringup_display_init() {
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  for (auto &l : lines) {
    l = "";
  }
  bringup_set_line(0, "boot...");
  draw_all();
  return true;
}

void bringup_set_line(uint8_t row, const String &text) {
  if (row >= 10) {
    return;
  }
  lines[row] = text;
  draw_all();
}

void bringup_show_touch(int16_t x, int16_t y) {
  last_tx = x;
  last_ty = y;
  draw_all();
}

void bringup_render_status(bool wifi_ok, const DaemonStatus &daemon, const VoiceStatus &voice) {
  bringup_set_line(3, wifi_ok ? "WiFi: OK" : "WiFi: FAIL");
  if (!daemon.reachable) {
    bringup_set_line(4, "Daemon: FAIL");
  } else {
    bringup_set_line(4, daemon.ok ? "Daemon: OK" : "Daemon: degraded");
  }
  if (!voice.reachable) {
    bringup_set_line(5, "Voice: unreachable");
  } else {
    bringup_set_line(5, String("STT: ") + (voice.stt ? "yes" : "no") + " TTS: " +
                             (voice.tts ? "yes" : "no"));
  }
}

#if BOARD_HAS_TOUCH
static bool touch_ready = false;

static bool touch_hw_init() {
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(10);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(50);
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, I2C_FREQ_HZ);
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  const uint8_t err = Wire.endTransmission();
  touch_ready = (err == 0);
  Serial.printf("[touch] FT6336U probe -> %s\n", touch_ready ? "OK" : "missing");
  return touch_ready;
}

void bringup_poll_touch() {
  static bool inited = false;
  if (!inited) {
    inited = true;
    touch_hw_init();
  }
  if (!touch_ready) {
    return;
  }

  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0x02);  // TD_STATUS
  if (Wire.endTransmission(false) != 0) {
    return;
  }
  if (Wire.requestFrom(static_cast<uint8_t>(TOUCH_I2C_ADDR), static_cast<uint8_t>(5)) < 5) {
    return;
  }
  const uint8_t n = Wire.read() & 0x0F;
  if (n == 0) {
    return;
  }
  const uint8_t xh = Wire.read();
  const uint8_t xl = Wire.read();
  const uint8_t yh = Wire.read();
  const uint8_t yl = Wire.read();
  const int16_t x = ((xh & 0x0F) << 8) | xl;
  const int16_t y = ((yh & 0x0F) << 8) | yl;
  if (x != last_tx || y != last_ty) {
    bringup_show_touch(x, y);
    Serial.printf("[touch] %d,%d\n", x, y);
  }
}
#else
void bringup_poll_touch() {}
#endif
