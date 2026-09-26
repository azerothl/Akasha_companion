#pragma once

#include <Arduino.h>

struct ImageSurface {
  bool ready = false;
  bool is_placeholder = false;
  String caption;
  bool dirty = true;
};

void image_surface_init();
// Load from http(s) jpeg URL or data:image/jpeg;base64,...
bool image_surface_load(ImageSurface &s, const String &ref);
void image_surface_draw(ImageSurface &s);
void image_surface_clear(ImageSurface &s);
