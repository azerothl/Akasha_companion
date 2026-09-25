#pragma once

#include <Arduino.h>

// Backlight sleep / wake (CMP-011). Keeps Wi-Fi and polling alive.

void power_ui_init();
void power_ui_note_activity();  // reset idle timer; wake if sleeping
void power_ui_tick(bool conversation_busy, bool keep_awake = false);
bool power_ui_display_asleep();
