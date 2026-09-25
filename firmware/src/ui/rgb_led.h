#pragma once

#include <Arduino.h>

enum class RgbState { Boot, Ok, Warn, Error, Offline };

bool rgb_init();
void rgb_set(RgbState state);
