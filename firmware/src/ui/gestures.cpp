#include "gestures.h"

#include "board.h"

static bool g_tracking = false;
static bool g_long_fired = false;
static bool g_ptt_fired = false;
static bool g_was_down = false;
static int16_t g_start_x = 0;
static int16_t g_start_y = 0;
static int16_t g_last_x = 0;
static int16_t g_last_y = 0;
static uint32_t g_start_ms = 0;

void gesture_reset() {
  g_tracking = false;
  g_long_fired = false;
  g_ptt_fired = false;
  g_was_down = false;
}

bool gesture_feed(const TouchSample &sample, bool boot_btn_down, GestureEvent &out) {
  out = {};
  const bool touch_down = sample.down;
  const bool down = touch_down || boot_btn_down;

  if (touch_down) {
    g_last_x = sample.x;
    g_last_y = sample.y;
  }

  if (down && !g_was_down) {
    g_tracking = true;
    g_long_fired = false;
    g_ptt_fired = false;
    g_start_ms = millis();
    g_start_x = touch_down ? sample.x : (BOARD_LCD_WIDTH / 2);
    g_start_y = touch_down ? sample.y : (BOARD_LCD_HEIGHT / 2);
    g_last_x = g_start_x;
    g_last_y = g_start_y;
  }

  if (g_tracking && down && !g_long_fired) {
    const uint32_t held = millis() - g_start_ms;
    if (held >= 600) {
      g_long_fired = true;
      out.kind = GestureKind::LongPress;
      out.x = g_start_x;
      out.y = g_start_y;
      g_was_down = down;
      return true;
    }
    if (!g_ptt_fired && held >= 300) {
      const bool in_zone =
          boot_btn_down || (touch_down && touch_in_ptt_zone(sample.x, sample.y));
      if (in_zone) {
        g_ptt_fired = true;
        out.kind = GestureKind::HoldPtt;
        out.x = g_last_x;
        out.y = g_last_y;
        g_was_down = down;
        return true;
      }
    }
  }

  if (!down && g_was_down && g_tracking) {
    const int16_t rdx = g_last_x - g_start_x;
    const uint32_t held = millis() - g_start_ms;
    const bool suppress = g_long_fired || g_ptt_fired;
    g_tracking = false;
    g_was_down = false;
    if (suppress) {
      return false;
    }
    if (rdx <= -40) {
      out.kind = GestureKind::SwipeLeft;
      out.dx = rdx;
      out.x = g_last_x;
      out.y = g_last_y;
      return true;
    }
    if (rdx >= 40) {
      out.kind = GestureKind::SwipeRight;
      out.dx = rdx;
      out.x = g_last_x;
      out.y = g_last_y;
      return true;
    }
    if (held < 300) {
      out.kind = GestureKind::Tap;
      out.x = g_start_x;
      out.y = g_start_y;
      return true;
    }
    return false;
  }

  g_was_down = down;
  return false;
}
