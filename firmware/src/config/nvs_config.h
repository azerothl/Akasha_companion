#pragma once

#include <Arduino.h>

struct CompanionConfig {
  String wifi_ssid;
  String wifi_pass;
  String host;
  uint16_t port;
  String token;
  bool hands_free = false;     // local NVS preference (Phase 5)
  uint8_t volume = 82;         // ES8311 0..100
  uint8_t avatar_style = 0;    // AvatarStyle ordinal
  bool ptt_beep = true;        // beep when starting PTT listen
  bool custom_endpoint = false; // if true, NVS host/port win over secrets.h
};

bool config_load(CompanionConfig &out);
bool config_save(const CompanionConfig &cfg);
