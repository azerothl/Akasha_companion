#pragma once

#include <Arduino.h>
#include "config/nvs_config.h"
#include "ui/avatar.h"
#include "net/akasha_discover.h"
#include "ui/soft_keyboard.h"

enum class SettingsPage : uint8_t { Main, Connexion, CustomHost };

struct SettingsSurface {
  bool dirty = true;
  SettingsPage page = SettingsPage::Main;
  DiscoveredDaemon found[kMaxDiscovered];
  uint8_t found_n = 0;
  SoftKeyboard host_kb;
  String status_line;
};

void settings_surface_init(SettingsSurface &s);
void settings_surface_draw(SettingsSurface &s, const CompanionConfig &cfg);

// out_back: leave settings entirely
// out_beep: play volume test beep
// out_reconnect: host/port changed — caller should re-pair / refresh connectivity
bool settings_surface_handle_tap(SettingsSurface &s, int16_t x, int16_t y, CompanionConfig &cfg,
                                 bool &out_back, bool &out_beep, bool &out_reconnect);
