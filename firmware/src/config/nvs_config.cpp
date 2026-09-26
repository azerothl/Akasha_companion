#include "nvs_config.h"

#include <Preferences.h>

#if __has_include("secrets.h")
#include "secrets.h"
#define HAS_SECRETS_H 1
#else
#include "secrets.example.h"
#define HAS_SECRETS_H 0
#endif

static constexpr const char *kNs = "akasha";

bool config_load(CompanionConfig &out) {
  Preferences prefs;
  prefs.begin(kNs, true);
  out.wifi_ssid = prefs.getString("wifi_ssid", "");
  out.wifi_pass = prefs.getString("wifi_pass", "");
  out.host = prefs.getString("host", "");
  out.port = prefs.getUShort("port", 0);
  out.token = prefs.getString("token", "");
  out.hands_free = prefs.getBool("hands_free", false);
  out.volume = prefs.getUChar("volume", 82);
  if (out.volume > 100) {
    out.volume = 100;
  }
  out.avatar_style = prefs.getUChar("av_style", 0);
  out.ptt_beep = prefs.getBool("ptt_beep", true);
  out.custom_endpoint = prefs.getBool("custom_ep", false);
  prefs.end();

#if HAS_SECRETS_H
  // Wi-Fi always from secrets.h so reflash picks up edits.
  // Host/port: secrets only when user has not set a custom/discovered endpoint.
  out.wifi_ssid = WIFI_SSID;
  out.wifi_pass = WIFI_PASS;
  if (!out.custom_endpoint) {
    out.host = AKASHA_HOST;
    out.port = static_cast<uint16_t>(AKASHA_PORT);
  }
  if (strlen(AKASHA_TOKEN) > 0) {
    out.token = AKASHA_TOKEN;
  }
  config_save(out);
  Serial.printf("[config] secrets.h wifi; endpoint=%s:%u custom=%d\n", out.host.c_str(),
                (unsigned)out.port, (int)out.custom_endpoint);
#else
  bool seeded = false;
  if (out.wifi_ssid.isEmpty()) {
    out.wifi_ssid = WIFI_SSID;
    out.wifi_pass = WIFI_PASS;
    seeded = true;
  }
  if (out.host.isEmpty()) {
    out.host = AKASHA_HOST;
    seeded = true;
  }
  if (out.port == 0) {
    out.port = static_cast<uint16_t>(AKASHA_PORT);
    seeded = true;
  }
  if (out.token.isEmpty() && strlen(AKASHA_TOKEN) > 0) {
    out.token = AKASHA_TOKEN;
    seeded = true;
  }
  if (seeded) {
    Serial.println("[config] seeding NVS from defaults");
    config_save(out);
  }
#endif
  return true;
}

bool config_save(const CompanionConfig &cfg) {
  Preferences prefs;
  if (!prefs.begin(kNs, false)) {
    return false;
  }
  prefs.putString("wifi_ssid", cfg.wifi_ssid);
  prefs.putString("wifi_pass", cfg.wifi_pass);
  prefs.putString("host", cfg.host);
  prefs.putUShort("port", cfg.port);
  prefs.putString("token", cfg.token);
  prefs.putBool("hands_free", cfg.hands_free);
  prefs.putUChar("volume", cfg.volume > 100 ? 100 : cfg.volume);
  prefs.putUChar("av_style", cfg.avatar_style);
  prefs.putBool("ptt_beep", cfg.ptt_beep);
  prefs.putBool("custom_ep", cfg.custom_endpoint);
  prefs.end();
  return true;
}
