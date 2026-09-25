#pragma once

#include <Arduino.h>
#include "touch_input.h"

enum class GestureKind : uint8_t {
  None,
  Tap,
  LongPress,
  SwipeLeft,
  SwipeRight,
  HoldPtt,  // held ≥ 300 ms in PTT zone (avatar)
};

struct GestureEvent {
  GestureKind kind = GestureKind::None;
  int16_t x = 0;
  int16_t y = 0;
  int16_t dx = 0;
};

// Call every loop with fresh sample. Emits at most one event per call (on release or long-press fire).
bool gesture_feed(const TouchSample &sample, bool boot_btn_down, GestureEvent &out);
void gesture_reset();
