#pragma once

// Copy to secrets.h (gitignored) and fill in local values.
// Used to seed NVS on first boot (CMP-006).

#ifndef WIFI_SSID
#define WIFI_SSID "your-wifi-ssid"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "your-wifi-password"
#endif

#ifndef AKASHA_HOST
#define AKASHA_HOST "192.168.1.10"
#endif

#ifndef AKASHA_PORT
#define AKASHA_PORT 3876
#endif

#ifndef AKASHA_TOKEN
#define AKASHA_TOKEN ""
#endif

// Optional: must match daemon AKASHA_COMPANION_PAIR_SECRET when set.
#ifndef AKASHA_PAIR_SECRET
#define AKASHA_PAIR_SECRET ""
#endif
