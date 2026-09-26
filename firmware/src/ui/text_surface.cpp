#include "text_surface.h"

#include <TFT_eSPI.h>
#include "board.h"

static TFT_eSPI g_text_tft;  // only used if we need colors — prefer draw via shared sprite from shell
// Actually text_surface will use its own sprite OR receive TFT_eSPI&. Simpler: own sprite.

static TFT_eSprite *g_spr = nullptr;
static bool g_spr_ok = false;

static void ensure_spr() {
  if (g_spr) {
    return;
  }
  static TFT_eSPI tft;
  static TFT_eSprite spr(&tft);
  static bool inited = false;
  if (!inited) {
    // tft already inited by avatar — re-init is ok for color helpers
    tft.init();
    spr.setColorDepth(16);
    g_spr_ok = spr.createSprite(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
    inited = true;
  }
  g_spr = &spr;
}

void text_surface_init(TextSurface &s) {
  soft_kb_init(s.kb);
  s.dirty = true;
  s.show_action_menu = false;
  ensure_spr();
}

void text_surface_mark_dirty(TextSurface &s) {
  s.dirty = true;
}

static void draw_chip(TFT_eSPI &d, int16_t x, int16_t y, int16_t w, const char *label) {
  d.fillRoundRect(x, y, w, 18, 4, d.color565(40, 70, 90));
  d.setTextColor(TFT_WHITE, d.color565(40, 70, 90));
  d.setTextDatum(MC_DATUM);
  d.drawString(label, x + w / 2, y + 9);
  d.setTextDatum(TL_DATUM);
}

void text_surface_draw(TextSurface &s, const ChatHistory &hist) {
  ensure_spr();
  if (!g_spr_ok || !g_spr) {
    return;
  }
  TFT_eSprite &d = *g_spr;
  const int16_t W = BOARD_LCD_WIDTH;
  const int16_t H = BOARD_LCD_HEIGHT;
  d.fillSprite(TFT_BLACK);

  d.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  d.setTextFont(1);
  d.setTextSize(1);
  d.setCursor(6, 4);
  d.print("Texte  <swipe");

  // Chips row (compact — Phase 5 adds HF)
  draw_chip(d, 2, 18, 34, "Brief");
  draw_chip(d, 38, 18, 34, "Stop");
  draw_chip(d, 74, 18, 40, "Repete");
  draw_chip(d, 116, 18, 34, "Lire");
  draw_chip(d, 152, 18, 34, "Mic");
  draw_chip(d, 188, 18, 48, "HF");

  // History area
  const int16_t hist_top = 42;
  const int16_t kb_top = 168;
  d.fillRect(0, hist_top, W, kb_top - hist_top, d.color565(12, 14, 18));
  int16_t y = hist_top + 4;
  const uint8_t show_n = hist.count > 3 ? 3 : hist.count;
  const uint8_t start = hist.count - show_n;
  for (uint8_t i = 0; i < show_n; ++i) {
    const ChatMessage &m = hist.items[start + i];
    const bool user = m.role == ChatRole::User;
    d.setTextColor(user ? TFT_CYAN : TFT_WHITE, d.color565(12, 14, 18));
    d.setCursor(6, y);
    String line = (user ? "U: " : "A: ") + m.text;
    if (line.length() > 38) {
      line = line.substring(0, 35) + "...";
    }
    d.print(line);
    y += 14;
    if (y > kb_top - 16) {
      break;
    }
  }

  soft_kb_draw(d, s.kb, kb_top);

  if (s.show_action_menu) {
    d.fillRoundRect(30, 80, 180, 100, 8, d.color565(30, 30, 36));
    d.drawRoundRect(30, 80, 180, 100, 8, TFT_YELLOW);
    d.setTextColor(TFT_YELLOW, d.color565(30, 30, 36));
    d.setCursor(50, 90);
    d.print("Actions");
    draw_chip(d, 50, 112, 60, "Brief");
    draw_chip(d, 120, 112, 60, "Stop");
    draw_chip(d, 50, 140, 60, "Repete");
    draw_chip(d, 120, 140, 60, "HF");
  }

  g_spr->pushSprite(0, 0);
  s.dirty = false;
  (void)H;
}

bool text_surface_handle_tap(TextSurface &s, int16_t x, int16_t y, TextChip &chip, String &send_text) {
  chip = TextChip::None;
  send_text = "";

  if (s.show_action_menu) {
    auto hit = [&](int16_t x0, int16_t y0, int16_t w, TextChip c) {
      if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + 18) {
        chip = c;
        s.show_action_menu = false;
        s.dirty = true;
        return true;
      }
      return false;
    };
    if (hit(50, 112, 60, TextChip::Brief) || hit(120, 112, 60, TextChip::Stop) ||
        hit(50, 140, 60, TextChip::Repeat) || hit(120, 140, 60, TextChip::HandsFree)) {
      return true;
    }
    // tap outside closes
    s.show_action_menu = false;
    s.dirty = true;
    return true;
  }

  // Top chips
  if (y >= 18 && y < 36) {
    if (x < 38) {
      chip = TextChip::Brief;
      return true;
    }
    if (x < 74) {
      chip = TextChip::Stop;
      return true;
    }
    if (x < 116) {
      chip = TextChip::Repeat;
      return true;
    }
    if (x < 152) {
      chip = TextChip::Read;
      return true;
    }
    if (x < 188) {
      chip = TextChip::Mic;
      return true;
    }
    if (x < 240) {
      chip = TextChip::HandsFree;
      return true;
    }
  }

  const int16_t kb_top = 168;
  if (y >= kb_top) {
    bool send = false;
    if (soft_kb_handle_tap(s.kb, x, y, kb_top, send)) {
      s.dirty = true;
      if (send) {
        send_text = s.kb.buffer;
        soft_kb_clear(s.kb);
      }
      return true;
    }
  }
  return false;
}
