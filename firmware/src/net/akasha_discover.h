#pragma once

#include <Arduino.h>

struct DiscoveredDaemon {
  String host;
  uint16_t port = 3876;
  String version;
  String label;  // display
};

static constexpr uint8_t kMaxDiscovered = 6;
static constexpr uint16_t kDiscoverUdpPort = 3877;

// UDP broadcast probe + optional LAN HTTP scan. Blocking ≤ ~2.5 s.
uint8_t akasha_discover(DiscoveredDaemon *out, uint8_t max_out, bool http_scan);
