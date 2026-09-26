#include "soft_keyboard.h"

#include <string.h>
#include "board.h"

// AZERTY rows
static const char *kRow1 = "azertyuiop";
static const char *kRow2 = "qsdfghjklm";
static const char *kRow3 = "wxcvbn";

static constexpr int16_t kKeyH = 28;
static constexpr int16_t kKeyGap = 2;

void soft_kb_init(SoftKeyboard &kb) {
  kb.buffer = "";
  kb.shift = false;
  kb.dirty = true;
}

void soft_kb_clear(SoftKeyboard &kb) {
  kb.buffer = "";
  kb.dirty = true;
}

static void draw_key(TFT_eSPI &d, int16_t x, int16_t y, int16_t w, int16_t h, const char *label,
                     bool accent) {
  const uint16_t fill = accent ? d.color565(50, 90, 80) : d.color565(36, 40, 48);
  d.fillRoundRect(x, y, w, h, 3, fill);
  d.drawRoundRect(x, y, w, h, 3, d.color565(70, 80, 90));
  d.setTextFont(1);
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, fill);
  d.setTextDatum(MC_DATUM);
  d.drawString(label, x + w / 2, y + h / 2);
  d.setTextDatum(TL_DATUM);
}

void soft_kb_draw(TFT_eSPI &d, SoftKeyboard &kb, int16_t y0) {
  const int16_t W = BOARD_LCD_WIDTH;
  d.fillRect(0, y0, W, BOARD_LCD_HEIGHT - y0, d.color565(16, 18, 22));

  // Compose line
  d.fillRect(4, y0 + 2, W - 8, 18, d.color565(24, 28, 32));
  d.setTextColor(TFT_CYAN, d.color565(24, 28, 32));
  d.setTextSize(1);
  d.setCursor(8, y0 + 6);
  String shown = kb.buffer;
  if (shown.length() > 34) {
    shown = shown.substring(shown.length() - 34);
  }
  d.print(shown.length() ? shown : "_");

  const int16_t base = y0 + 24;
  auto draw_row = [&](const char *row, int16_t row_i, int16_t x_off) {
    const int n = (int)strlen(row);
    const int16_t kw = (W - 8 - (n - 1) * kKeyGap) / n;
    for (int i = 0; i < n; ++i) {
      char lab[2] = {row[i], 0};
      if (kb.shift && lab[0] >= 'a' && lab[0] <= 'z') {
        lab[0] = (char)(lab[0] - 32);
      }
      draw_key(d, 4 + x_off + i * (kw + kKeyGap), base + row_i * (kKeyH + kKeyGap), kw, kKeyH, lab,
               false);
    }
  };

  draw_row(kRow1, 0, 0);
  draw_row(kRow2, 1, 4);
  // row3: shift + letters + backspace
  {
    const int n = (int)strlen(kRow3);
    const int16_t special_w = 36;
    const int16_t avail = W - 8 - special_w * 2 - kKeyGap * (n + 1);
    const int16_t kw = avail / n;
    int16_t x = 4;
    draw_key(d, x, base + 2 * (kKeyH + kKeyGap), special_w, kKeyH, kb.shift ? "ABC" : "abc", true);
    x += special_w + kKeyGap;
    for (int i = 0; i < n; ++i) {
      char lab[2] = {kRow3[i], 0};
      if (kb.shift && lab[0] >= 'a' && lab[0] <= 'z') {
        lab[0] = (char)(lab[0] - 32);
      }
      draw_key(d, x, base + 2 * (kKeyH + kKeyGap), kw, kKeyH, lab, false);
      x += kw + kKeyGap;
    }
    draw_key(d, x, base + 2 * (kKeyH + kKeyGap), special_w, kKeyH, "<", true);
  }
  // space + send
  {
    const int16_t y = base + 3 * (kKeyH + kKeyGap);
    draw_key(d, 4, y, W - 80, kKeyH, "espace", false);
    draw_key(d, W - 72, y, 68, kKeyH, "OK", true);
  }
}

static bool hit_row_char(const char *row, int16_t x, int16_t y, int16_t row_y, int16_t x_off,
                        char &out) {
  const int n = (int)strlen(row);
  const int16_t W = BOARD_LCD_WIDTH;
  const int16_t kw = (W - 8 - (n - 1) * kKeyGap) / n;
  if (y < row_y || y >= row_y + kKeyH) {
    return false;
  }
  for (int i = 0; i < n; ++i) {
    const int16_t kx = 4 + x_off + i * (kw + kKeyGap);
    if (x >= kx && x < kx + kw) {
      out = row[i];
      return true;
    }
  }
  return false;
}

bool soft_kb_handle_tap(SoftKeyboard &kb, int16_t x, int16_t y, int16_t y0, bool &send_out) {
  send_out = false;
  const int16_t base = y0 + 24;
  char ch = 0;
  if (hit_row_char(kRow1, x, y, base, 0, ch) || hit_row_char(kRow2, x, y, base + (kKeyH + kKeyGap), 4, ch)) {
    if (kb.shift && ch >= 'a' && ch <= 'z') {
      ch = (char)(ch - 32);
    }
    if (kb.buffer.length() < kSoftKbMaxLen) {
      kb.buffer += ch;
      kb.shift = false;
      kb.dirty = true;
    }
    return true;
  }

  const int16_t row3_y = base + 2 * (kKeyH + kKeyGap);
  if (y >= row3_y && y < row3_y + kKeyH) {
    const int16_t special_w = 36;
    if (x >= 4 && x < 4 + special_w) {
      kb.shift = !kb.shift;
      kb.dirty = true;
      return true;
    }
    const int n = (int)strlen(kRow3);
    const int16_t avail = BOARD_LCD_WIDTH - 8 - special_w * 2 - kKeyGap * (n + 1);
    const int16_t kw = avail / n;
    int16_t cx = 4 + special_w + kKeyGap;
    for (int i = 0; i < n; ++i) {
      if (x >= cx && x < cx + kw) {
        char c = kRow3[i];
        if (kb.shift && c >= 'a' && c <= 'z') {
          c = (char)(c - 32);
        }
        if (kb.buffer.length() < kSoftKbMaxLen) {
          kb.buffer += c;
          kb.shift = false;
          kb.dirty = true;
        }
        return true;
      }
      cx += kw + kKeyGap;
    }
    if (x >= cx && x < cx + special_w) {
      if (kb.buffer.length()) {
        kb.buffer.remove(kb.buffer.length() - 1);
        kb.dirty = true;
      }
      return true;
    }
  }

  const int16_t row4_y = base + 3 * (kKeyH + kKeyGap);
  if (y >= row4_y && y < row4_y + kKeyH) {
    if (x >= 4 && x < BOARD_LCD_WIDTH - 76) {
      if (kb.buffer.length() < kSoftKbMaxLen) {
        kb.buffer += ' ';
        kb.dirty = true;
      }
      return true;
    }
    if (x >= BOARD_LCD_WIDTH - 72) {
      send_out = kb.buffer.length() > 0;
      return true;
    }
  }
  return false;
}
