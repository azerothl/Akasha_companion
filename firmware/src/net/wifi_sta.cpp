#include "wifi_sta.h"

#include <WiFi.h>

bool wifi_connect(const CompanionConfig &cfg, uint32_t timeout_ms) {
  if (cfg.wifi_ssid.isEmpty()) {
    Serial.println("[wifi] SSID empty");
    return false;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);

  Serial.print("[wifi] SSID=");
  Serial.println(cfg.wifi_ssid);
  WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());

  const uint32_t start = millis();
  wl_status_t last = WL_IDLE_STATUS;
  while (WiFi.status() != WL_CONNECTED) {
    const wl_status_t st = WiFi.status();
    if (st != last) {
      last = st;
      Serial.print("[wifi] status=");
      Serial.println((int)st);
    }
    if (millis() - start > timeout_ms) {
      Serial.print("[wifi] timeout status=");
      Serial.println((int)WiFi.status());
      return false;
    }
    delay(250);
  }
  Serial.print("[wifi] OK ip=");
  Serial.println(WiFi.localIP());
  return true;
}
