#pragma once

#include <Arduino.h>

struct TouchSample {
  bool down = false;
  int16_t x = 0;
  int16_t y = 0;
};

bool touch_init();
bool touch_read(TouchSample &out);

// Center PTT zone (~middle 60% of screen)
bool touch_in_ptt_zone(int16_t x, int16_t y);
