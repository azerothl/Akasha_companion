#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

struct SoftKeyboard {
  String buffer;
  bool shift = false;
  bool dirty = true;
};

void soft_kb_init(SoftKeyboard &kb);
void soft_kb_clear(SoftKeyboard &kb);
// Draw keyboard in bottom area of sprite/display. y0 = top of keyboard.
void soft_kb_draw(TFT_eSPI &d, SoftKeyboard &kb, int16_t y0);
// Handle tap; returns true if buffer changed or send requested.
// send_out set true when Enter/Envoyer pressed.
bool soft_kb_handle_tap(SoftKeyboard &kb, int16_t x, int16_t y, int16_t y0, bool &send_out);

static constexpr uint16_t kSoftKbMaxLen = 160;
