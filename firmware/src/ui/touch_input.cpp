#include "touch_input.h"

#include <Wire.h>
#include "board.h"

static bool g_touch_ok = false;

bool touch_init() {
#if !BOARD_HAS_TOUCH
  g_touch_ok = false;
  return false;
#else
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW);
  delay(10);
  digitalWrite(PIN_TOUCH_RST, HIGH);
  delay(50);
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, I2C_FREQ_HZ);
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  g_touch_ok = (Wire.endTransmission() == 0);
  Serial.printf("[touch] FT6336U -> %s\n", g_touch_ok ? "OK" : "missing");
  return g_touch_ok;
#endif
}

bool touch_read(TouchSample &out) {
  out = {};
#if !BOARD_HAS_TOUCH
  return false;
#else
  if (!g_touch_ok) {
    return false;
  }
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<uint8_t>(TOUCH_I2C_ADDR), static_cast<uint8_t>(5)) < 5) {
    return false;
  }
  const uint8_t n = Wire.read() & 0x0F;
  const uint8_t xh = Wire.read();
  const uint8_t xl = Wire.read();
  const uint8_t yh = Wire.read();
  const uint8_t yl = Wire.read();
  if (n == 0) {
    return true;  // valid read, no touch
  }
  out.down = true;
  out.x = ((xh & 0x0F) << 8) | xl;
  out.y = ((yh & 0x0F) << 8) | yl;
  return true;
#endif
}

bool touch_in_ptt_zone(int16_t x, int16_t y) {
  const int16_t mx0 = BOARD_LCD_WIDTH / 5;
  const int16_t mx1 = BOARD_LCD_WIDTH - BOARD_LCD_WIDTH / 5;
  const int16_t my0 = BOARD_LCD_HEIGHT / 5;
  const int16_t my1 = BOARD_LCD_HEIGHT - BOARD_LCD_HEIGHT / 6;
  return x >= mx0 && x <= mx1 && y >= my0 && y <= my1;
}
