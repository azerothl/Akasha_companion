#include "image_surface.h"

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <stdlib.h>

#include "board.h"
#include "audio/wav_util.h"
#include "net/akasha_http.h"

static TFT_eSPI g_img_tft;
static TFT_eSprite g_img_spr(&g_img_tft);
static bool g_img_spr_ok = false;
static bool g_img_tft_ok = false;

static bool jpg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!g_img_spr_ok) {
    return false;
  }
  g_img_spr.pushImage(x, y, w, h, bitmap);
  return true;
}

void image_surface_init() {
  if (g_img_tft_ok) {
    return;
  }
  g_img_tft.init();
  g_img_spr.setColorDepth(16);
  g_img_spr_ok = g_img_spr.createSprite(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
  g_img_tft_ok = true;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(jpg_output);
  TJpgDec.setSwapBytes(true);
}

void image_surface_clear(ImageSurface &s) {
  s.ready = false;
  s.is_placeholder = false;
  s.caption = "";
  s.dirty = true;
}

static void draw_placeholder(ImageSurface &s, const String &msg) {
  if (!g_img_spr_ok) {
    return;
  }
  g_img_spr.fillSprite(TFT_BLACK);
  g_img_spr.setTextColor(TFT_ORANGE, TFT_BLACK);
  g_img_spr.setCursor(10, 40);
  g_img_spr.print("Image");
  g_img_spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String line = msg;
  if (line.length() > 90) {
    line = line.substring(0, 87) + "...";
  }
  for (uint16_t i = 0; i < line.length(); i += 36) {
    g_img_spr.setCursor(10, 70 + (i / 36) * 14);
    g_img_spr.print(line.substring(i, i + 36));
  }
  g_img_spr.setTextColor(TFT_CYAN, TFT_BLACK);
  g_img_spr.setCursor(10, 280);
  g_img_spr.print("tap pour fermer");
  g_img_spr.pushSprite(0, 0);
  s.is_placeholder = true;
  s.ready = true;
  s.dirty = false;
}

bool image_surface_load(ImageSurface &s, const String &ref) {
  image_surface_init();
  s.caption = ref;
  s.dirty = true;

  if (!g_img_spr_ok) {
    return false;
  }

  uint8_t *jpeg = nullptr;
  size_t jpeg_len = 0;

  if (ref.startsWith("data:image/jpeg")) {
    WavBuffer decoded = {};
    if (!wav_from_data_url(ref.c_str(), decoded) || !decoded.data) {
      draw_placeholder(s, "bad data url");
      return false;
    }
    jpeg = decoded.data;
    jpeg_len = decoded.size;
  } else if (ref.startsWith("http://") || ref.startsWith("https://")) {
    String low = ref;
    low.toLowerCase();
    const bool is_png = low.indexOf(".png") >= 0 && low.indexOf(".jpg") < 0 && low.indexOf(".jpeg") < 0;
    if (is_png) {
      draw_placeholder(s, ref);
      return false;
    }
    String err;
    if (!akasha_http_get_bytes(ref, &jpeg, &jpeg_len, 120 * 1024, err) || !jpeg) {
      draw_placeholder(s, err.length() ? err : ref);
      return false;
    }
  } else {
    draw_placeholder(s, ref);
    return false;
  }

  g_img_spr.fillSprite(TFT_BLACK);
  uint16_t jw = 0, jh = 0;
  if (TJpgDec.getJpgSize(&jw, &jh, jpeg, jpeg_len) != JDR_OK) {
    free(jpeg);
    draw_placeholder(s, "jpeg header");
    return false;
  }
  uint8_t scale = 1;
  while ((jw / scale > BOARD_LCD_WIDTH || jh / scale > BOARD_LCD_HEIGHT) && scale < 8) {
    scale = (uint8_t)(scale * 2);
  }
  TJpgDec.setJpgScale(scale);
  const int16_t dw = (int16_t)(jw / scale);
  const int16_t dh = (int16_t)(jh / scale);
  const int16_t ox = (BOARD_LCD_WIDTH - dw) / 2;
  const int16_t oy = (BOARD_LCD_HEIGHT - dh) / 2;
  const JRESULT r = TJpgDec.drawJpg(ox < 0 ? 0 : ox, oy < 0 ? 0 : oy, jpeg, jpeg_len);
  free(jpeg);
  if (r != JDR_OK) {
    draw_placeholder(s, "jpeg decode");
    return false;
  }
  g_img_spr.setTextColor(TFT_CYAN, TFT_BLACK);
  g_img_spr.setCursor(8, 300);
  g_img_spr.print("tap pour fermer");
  g_img_spr.pushSprite(0, 0);
  s.is_placeholder = false;
  s.ready = true;
  s.dirty = false;
  return true;
}

void image_surface_draw(ImageSurface &s) {
  if (!s.ready || !g_img_spr_ok) {
    return;
  }
  if (s.dirty) {
    g_img_spr.pushSprite(0, 0);
    s.dirty = false;
  }
}
