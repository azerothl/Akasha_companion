#pragma once

#include <Arduino.h>

// TFT_eSPI built-in GLCD font is ASCII-only. Map UTF-8 (accents, ellipsis, dashes)
// to readable Latin ASCII for on-screen text.
String glcd_ascii(const String &in);

// Strip markdown / markup before TTS so formatting tokens are not spoken.
String speech_plain(const String &in);
