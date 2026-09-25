#pragma once

#include <Arduino.h>
#include "chat/history.h"
#include "soft_keyboard.h"

enum class TextChip : uint8_t {
  None,
  Brief,
  Stop,
  Repeat,
  Read,
  Mic,
  HandsFree,
};

struct TextSurface {
  SoftKeyboard kb;
  bool dirty = true;
  bool show_action_menu = false;  // Brief/Stop/Repete overlay
};

void text_surface_init(TextSurface &s);
void text_surface_draw(TextSurface &s, const ChatHistory &hist);
// Returns chip action or None; may set send_text if keyboard OK.
bool text_surface_handle_tap(TextSurface &s, int16_t x, int16_t y, TextChip &chip, String &send_text);
void text_surface_mark_dirty(TextSurface &s);
