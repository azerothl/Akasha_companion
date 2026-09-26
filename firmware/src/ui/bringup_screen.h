#pragma once

#include <Arduino.h>
#include "net/akasha_http.h"

bool bringup_display_init();
void bringup_set_line(uint8_t row, const String &text);
void bringup_show_touch(int16_t x, int16_t y);
void bringup_render_status(bool wifi_ok, const DaemonStatus &daemon, const VoiceStatus &voice);
void bringup_poll_touch();
