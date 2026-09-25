#pragma once

#include <Arduino.h>

enum class AvatarState {
  Offline,
  Idle,
  Listening,
  Thinking,
  Speaking,
  Error,
  Notify,
};

enum class AvatarMood {
  Neutral,
  Happy,
  Concerned,
};

// Visual presence styles (NVS `av_style`)
enum class AvatarStyle : uint8_t {
  Cute = 0,     // eyes + mouth vector (default)
  SoftOrb = 1,  // round face disc + features
  Dot = 2,      // minimal LED-like eyes
  Count = 3,
};

bool avatar_init();
void avatar_set_state(AvatarState s);
AvatarState avatar_state();
void avatar_set_mouth(float open01);
void avatar_set_overlay(const String &line, uint32_t ms);
void avatar_set_mood(AvatarMood m, uint32_t ms);
void avatar_set_style(AvatarStyle style);
AvatarStyle avatar_style();
const char *avatar_style_name(AvatarStyle style);
void avatar_tick();  // call from loop ~20–50 Hz
void avatar_force_redraw();
// When false, tick/force_redraw skip TFT pushes (Text/Image/Settings own the screen).
void avatar_set_drawing_enabled(bool enabled);
bool avatar_drawing_enabled();
