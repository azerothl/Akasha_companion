#include "akasha_discover.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

static bool already_have(const DiscoveredDaemon *list, uint8_t n, const String &host, uint16_t port) {
  for (uint8_t i = 0; i < n; ++i) {
    if (list[i].host == host && list[i].port == port) {
      return true;
    }
  }
  return false;
}

static bool add_one(DiscoveredDaemon *out, uint8_t &n, uint8_t max_out, const String &host,
                    uint16_t port, const String &version) {
  if (host.isEmpty() || n >= max_out) {
    return false;
  }
  if (already_have(out, n, host, port)) {
    return false;
  }
  out[n].host = host;
  out[n].port = port ? port : 3876;
  out[n].version = version;
  out[n].label = host + ":" + String(out[n].port);
  if (version.length()) {
    out[n].label += " v" + version;
  }
  n++;
  return true;
}

static void parse_reply(const String &json, const IPAddress &from, DiscoveredDaemon *out, uint8_t &n,
                        uint8_t max_out) {
  // Minimal parse — avoid ArduinoJson dependency here
  uint16_t port = 3876;
  String version;
  const int pi = json.indexOf("\"port\"");
  if (pi >= 0) {
    const int colon = json.indexOf(':', pi);
    if (colon > 0) {
      port = (uint16_t)json.substring(colon + 1).toInt();
      if (port == 0) {
        port = 3876;
      }
    }
  }
  const int vi = json.indexOf("\"version\"");
  if (vi >= 0) {
    const int q1 = json.indexOf('"', vi + 9);
    const int q2 = q1 > 0 ? json.indexOf('"', q1 + 1) : -1;
    if (q1 > 0 && q2 > q1) {
      version = json.substring(q1 + 1, q2);
    }
  }
  if (json.indexOf("akasha") < 0 && json.indexOf("\"port\"") < 0) {
    return;
  }
  add_one(out, n, max_out, from.toString(), port, version);
}

static bool probe_http_status(const String &host, uint16_t port, String &version_out) {
  version_out = "";
  HTTPClient http;
  http.setTimeout(120);
  http.setConnectTimeout(100);
  const String url = String("http://") + host + ":" + String(port) + "/api/status";
  if (!http.begin(url)) {
    return false;
  }
  const int code = http.GET();
  const String body = http.getString();
  http.end();
  if (code != 200) {
    return false;
  }
  const int vi = body.indexOf("\"version\"");
  if (vi >= 0) {
    const int q1 = body.indexOf('"', vi + 9);
    const int q2 = q1 > 0 ? body.indexOf('"', q1 + 1) : -1;
    if (q1 > 0 && q2 > q1) {
      version_out = body.substring(q1 + 1, q2);
    }
  }
  return true;
}

uint8_t akasha_discover(DiscoveredDaemon *out, uint8_t max_out, bool http_scan) {
  uint8_t n = 0;
  if (!out || max_out == 0 || WiFi.status() != WL_CONNECTED) {
    return 0;
  }

  WiFiUDP udp;
  if (udp.begin(0)) {
    udp.beginPacket("255.255.255.255", kDiscoverUdpPort);
    udp.print("AKASHA_DISCOVER");
    udp.endPacket();
    // Also try directed broadcast of /24
    IPAddress ip = WiFi.localIP();
    IPAddress bcast(ip[0], ip[1], ip[2], 255);
    udp.beginPacket(bcast, kDiscoverUdpPort);
    udp.print("AKASHA_DISCOVER");
    udp.endPacket();

    const uint32_t t0 = millis();
    while (millis() - t0 < 1200) {
      const int packet = udp.parsePacket();
      if (packet > 0) {
        char buf[192];
        const int len = udp.read(buf, sizeof(buf) - 1);
        if (len > 0) {
          buf[len] = 0;
          parse_reply(String(buf), udp.remoteIP(), out, n, max_out);
        }
      } else {
        delay(10);
        yield();
      }
    }
    udp.stop();
  }

  if (http_scan && n < max_out) {
    IPAddress ip = WiFi.localIP();
    IPAddress gw = WiFi.gatewayIP();
    // Probe gateway + a window around our host
    const int me = ip[3];
    int candidates[48];
    uint8_t nc = 0;
    auto push = [&](int octet) {
      if (octet < 1 || octet > 254 || nc >= 48) {
        return;
      }
      for (uint8_t i = 0; i < nc; ++i) {
        if (candidates[i] == octet) {
          return;
        }
      }
      candidates[nc++] = octet;
    };
    push(gw[3]);
    push(me);
    for (int d = 1; d <= 20; ++d) {
      push(me - d);
      push(me + d);
    }
    for (uint8_t i = 0; i < nc && n < max_out; ++i) {
      IPAddress target(ip[0], ip[1], ip[2], candidates[i]);
      const String host = target.toString();
      if (already_have(out, n, host, 3876)) {
        continue;
      }
      String ver;
      if (probe_http_status(host, 3876, ver)) {
        add_one(out, n, max_out, host, 3876, ver);
      }
      yield();
    }
  }

  return n;
}
