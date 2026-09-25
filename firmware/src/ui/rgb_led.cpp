#include "rgb_led.h"

#include <Adafruit_NeoPixel.h>

#include "board.h"

static Adafruit_NeoPixel pixel(1, PIN_RGB, NEO_GRB + NEO_KHZ800);

bool rgb_init() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pixel.begin();
  pixel.setBrightness(24);
  rgb_set(RgbState::Boot);
  return true;
}

void rgb_set(RgbState state) {
  uint32_t c = 0;
  switch (state) {
    case RgbState::Boot:
      c = pixel.Color(0, 0, 40);
      break;
    case RgbState::Ok:
      c = pixel.Color(0, 40, 0);
      break;
    case RgbState::Warn:
      c = pixel.Color(40, 24, 0);
      break;
    case RgbState::Error:
      c = pixel.Color(40, 0, 0);
      break;
    case RgbState::Offline:
      c = pixel.Color(40, 0, 0);
      break;
  }
  pixel.setPixelColor(0, c);
  pixel.show();
}
