#include "settings_surface.h"

#include <TFT_eSPI.h>
#include "board.h"

static TFT_eSprite *g_set_spr = nullptr;
static bool g_set_spr_ok = false;

static void ensure_spr() {
  if (g_set_spr) {
    return;
  }
  static TFT_eSPI tft;
  static TFT_eSprite spr(&tft);
  static bool inited = false;
  if (!inited) {
    tft.init();
    spr.setColorDepth(16);
    g_set_spr_ok = spr.createSprite(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
    inited = true;
  }
  g_set_spr = &spr;
}

static void draw_btn(TFT_eSPI &d, int16_t x, int16_t y, int16_t w, int16_t h, const char *label,
                     uint16_t fill) {
  d.fillRoundRect(x, y, w, h, 8, fill);
  d.setTextColor(TFT_WHITE, fill);
  d.setTextDatum(MC_DATUM);
  d.setTextFont(2);
  d.drawString(label, x + w / 2, y + h / 2);
  d.setTextDatum(TL_DATUM);
  d.setTextFont(1);
}

static bool hit(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

void settings_surface_init(SettingsSurface &s) {
  s.dirty = true;
  s.page = SettingsPage::Main;
  s.found_n = 0;
  soft_kb_init(s.host_kb);
  s.status_line = "";
  ensure_spr();
}

static void draw_main(TFT_eSprite &d, SettingsSurface &s, const CompanionConfig &cfg) {
  const int16_t W = BOARD_LCD_WIDTH;
  d.fillSprite(TFT_BLACK);

  d.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  d.setTextFont(2);
  d.setCursor(12, 8);
  d.print("Reglages");

  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(16, 36);
  d.print("Volume");
  draw_btn(d, 16, 58, 48, 32, "-", d.color565(50, 70, 90));
  draw_btn(d, 168, 58, 48, 32, "+", d.color565(50, 70, 90));
  char volbuf[8];
  snprintf(volbuf, sizeof(volbuf), "%u", (unsigned)cfg.volume);
  d.setTextDatum(MC_DATUM);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.drawString(volbuf, W / 2, 74);
  d.setTextDatum(TL_DATUM);
  d.fillRoundRect(16, 98, W - 32, 8, 3, d.color565(28, 28, 32));
  const int16_t fill_w = (int16_t)(((W - 32) * (int)cfg.volume) / 100);
  if (fill_w > 0) {
    d.fillRoundRect(16, 98, fill_w, 8, 3, d.color565(40, 160, 170));
  }

  AvatarStyle st = AvatarStyle::Cute;
  if (cfg.avatar_style < (uint8_t)AvatarStyle::Count) {
    st = (AvatarStyle)cfg.avatar_style;
  }
  char style_label[28];
  snprintf(style_label, sizeof(style_label), "Avatar: %s", avatar_style_name(st));
  draw_btn(d, 16, 116, W - 32, 32, style_label, d.color565(40, 70, 90));

  draw_btn(d, 16, 156, W - 32, 32, cfg.ptt_beep ? "Bip PTT: ON" : "Bip PTT: OFF",
           cfg.ptt_beep ? d.color565(60, 100, 70) : d.color565(80, 60, 60));

  draw_btn(d, 16, 196, W - 32, 32, "Connexion Akasha", d.color565(50, 80, 110));
  draw_btn(d, 16, 236, W - 32, 28, "Tester le son", d.color565(60, 90, 70));
  draw_btn(d, 16, 272, W - 32, 28, "Retour", d.color565(70, 60, 50));
  (void)s;
}

static void draw_connexion(TFT_eSprite &d, SettingsSurface &s, const CompanionConfig &cfg) {
  const int16_t W = BOARD_LCD_WIDTH;
  d.fillSprite(TFT_BLACK);
  d.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  d.setTextFont(2);
  d.setCursor(12, 6);
  d.print("Connexion");

  d.setTextFont(1);
  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.setCursor(12, 30);
  d.printf("%s:%u%s", cfg.host.c_str(), (unsigned)cfg.port, cfg.custom_endpoint ? " *" : "");

  if (s.status_line.length()) {
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(12, 44);
    d.print(s.status_line.substring(0, 36));
  }

  draw_btn(d, 16, 60, 100, 28, "Decouvrir", d.color565(50, 90, 110));
  draw_btn(d, 124, 60, 100, 28, "Custom", d.color565(70, 70, 100));

  int16_t y = 96;
  for (uint8_t i = 0; i < s.found_n && i < 4; ++i) {
    draw_btn(d, 16, y, W - 32, 26, s.found[i].label.substring(0, 28).c_str(),
             d.color565(40, 70, 90));
    y += 30;
  }
  if (s.found_n == 0) {
    d.setTextColor(d.color565(100, 100, 110), TFT_BLACK);
    d.setCursor(16, 110);
    d.print("Aucun daemon (UDP/LAN)");
  }

  draw_btn(d, 16, 272, W - 32, 28, "Retour", d.color565(70, 60, 50));
}

static void draw_custom(TFT_eSprite &d, SettingsSurface &s, const CompanionConfig &cfg) {
  const int16_t W = BOARD_LCD_WIDTH;
  d.fillSprite(TFT_BLACK);
  d.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  d.setTextFont(2);
  d.setCursor(12, 6);
  d.print("Host custom");
  d.setTextFont(1);
  d.setTextColor(d.color565(120, 120, 130), TFT_BLACK);
  d.setCursor(12, 28);
  d.print("IP, domaine ou host:port");

  d.fillRoundRect(8, 44, W - 16, 20, 4, d.color565(20, 22, 28));
  d.setTextColor(TFT_WHITE, d.color565(20, 22, 28));
  d.setCursor(12, 49);
  d.print(s.host_kb.buffer.substring(0, 34));

  // Digit / punctuation strip for IPs and ports
  static const char *kDigits = "1234567890.:";
  const int nd = 12;
  const int16_t dw = (W - 8 - (nd - 1) * 2) / nd;
  for (int i = 0; i < nd; ++i) {
    char lab[2] = {kDigits[i], 0};
    draw_btn(d, 4 + i * (dw + 2), 70, dw, 24, lab, d.color565(36, 40, 48));
  }

  soft_kb_draw(d, s.host_kb, 168);
  draw_btn(d, 16, 100, W - 32, 26, "Enregistrer", d.color565(50, 110, 80));
  draw_btn(d, 16, 130, 100, 26, "Annuler", d.color565(70, 60, 50));
  (void)cfg;
}

void settings_surface_draw(SettingsSurface &s, const CompanionConfig &cfg) {
  ensure_spr();
  if (!g_set_spr_ok || !g_set_spr) {
    return;
  }
  TFT_eSprite &d = *g_set_spr;
  if (s.page == SettingsPage::Connexion) {
    draw_connexion(d, s, cfg);
  } else if (s.page == SettingsPage::CustomHost) {
    draw_custom(d, s, cfg);
  } else {
    draw_main(d, s, cfg);
  }
  d.pushSprite(0, 0);
  s.dirty = false;
}

static bool apply_endpoint(CompanionConfig &cfg, const String &host, uint16_t port) {
  if (host.isEmpty()) {
    return false;
  }
  cfg.host = host;
  cfg.port = port ? port : 3876;
  cfg.custom_endpoint = true;
  cfg.token = "";  // re-pair against new daemon
  return true;
}

static bool parse_host_port(const String &raw, String &host_out, uint16_t &port_out) {
  String t = raw;
  t.trim();
  if (t.isEmpty()) {
    return false;
  }
  // strip scheme
  if (t.startsWith("http://")) {
    t = t.substring(7);
  } else if (t.startsWith("https://")) {
    t = t.substring(8);
  }
  const int slash = t.indexOf('/');
  if (slash >= 0) {
    t = t.substring(0, slash);
  }
  const int colon = t.lastIndexOf(':');
  if (colon > 0) {
    host_out = t.substring(0, colon);
    port_out = (uint16_t)t.substring(colon + 1).toInt();
    if (port_out == 0) {
      port_out = 3876;
    }
  } else {
    host_out = t;
    port_out = 3876;
  }
  return host_out.length() > 0;
}

bool settings_surface_handle_tap(SettingsSurface &s, int16_t x, int16_t y, CompanionConfig &cfg,
                                 bool &out_back, bool &out_beep, bool &out_reconnect) {
  out_back = false;
  out_beep = false;
  out_reconnect = false;
  const int16_t W = BOARD_LCD_WIDTH;

  if (s.page == SettingsPage::Main) {
    if (hit(x, y, 16, 58, 48, 32)) {
      cfg.volume = cfg.volume >= 5 ? (uint8_t)(cfg.volume - 5) : 0;
      s.dirty = true;
      out_beep = true;
      return true;
    }
    if (hit(x, y, 168, 58, 48, 32)) {
      cfg.volume = cfg.volume <= 95 ? (uint8_t)(cfg.volume + 5) : 100;
      s.dirty = true;
      out_beep = true;
      return true;
    }
    if (hit(x, y, 16, 92, W - 32, 18)) {
      int v = ((int)(x - 16) * 100) / (W - 32);
      if (v < 0) {
        v = 0;
      }
      if (v > 100) {
        v = 100;
      }
      cfg.volume = (uint8_t)v;
      s.dirty = true;
      out_beep = true;
      return true;
    }
    if (hit(x, y, 16, 116, W - 32, 32)) {
      cfg.avatar_style = (uint8_t)(((int)cfg.avatar_style + 1) % (int)AvatarStyle::Count);
      s.dirty = true;
      return true;
    }
    if (hit(x, y, 16, 156, W - 32, 32)) {
      cfg.ptt_beep = !cfg.ptt_beep;
      s.dirty = true;
      return true;
    }
    if (hit(x, y, 16, 196, W - 32, 32)) {
      s.page = SettingsPage::Connexion;
      s.status_line = "";
      s.dirty = true;
      return true;
    }
    if (hit(x, y, 16, 236, W - 32, 28)) {
      out_beep = true;
      return true;
    }
    if (hit(x, y, 16, 272, W - 32, 28)) {
      out_back = true;
      return true;
    }
    return false;
  }

  if (s.page == SettingsPage::Connexion) {
    if (hit(x, y, 16, 60, 100, 28)) {
      s.status_line = "Scan...";
      s.dirty = true;
      settings_surface_draw(s, cfg);
      s.found_n = akasha_discover(s.found, kMaxDiscovered, true);
      s.status_line = String(s.found_n) + " trouve(s)";
      s.dirty = true;
      return true;
    }
    if (hit(x, y, 124, 60, 100, 28)) {
      s.page = SettingsPage::CustomHost;
      soft_kb_clear(s.host_kb);
      if (cfg.host.length()) {
        s.host_kb.buffer = cfg.host;
        if (cfg.port && cfg.port != 3876) {
          s.host_kb.buffer += ":" + String(cfg.port);
        }
      }
      s.dirty = true;
      return true;
    }
    int16_t y0 = 96;
    for (uint8_t i = 0; i < s.found_n && i < 4; ++i) {
      if (hit(x, y, 16, y0, W - 32, 26)) {
        if (apply_endpoint(cfg, s.found[i].host, s.found[i].port)) {
          s.status_line = "OK " + s.found[i].label;
          out_reconnect = true;
          s.dirty = true;
        }
        return true;
      }
      y0 += 30;
    }
    if (hit(x, y, 16, 272, W - 32, 28)) {
      s.page = SettingsPage::Main;
      s.dirty = true;
      return true;
    }
    return false;
  }

  // CustomHost
  if (hit(x, y, 16, 130, 100, 26)) {
    s.page = SettingsPage::Connexion;
    s.dirty = true;
    return true;
  }
  if (hit(x, y, 16, 100, W - 32, 26)) {
    String host;
    uint16_t port = 3876;
    if (parse_host_port(s.host_kb.buffer, host, port) && apply_endpoint(cfg, host, port)) {
      out_reconnect = true;
      s.page = SettingsPage::Connexion;
      s.status_line = "Custom OK";
      s.dirty = true;
    } else {
      s.status_line = "host invalide";
      s.dirty = true;
    }
    return true;
  }
  // Digit strip
  {
    static const char *kDigits = "1234567890.:";
    const int nd = 12;
    const int16_t dw = (W - 8 - (nd - 1) * 2) / nd;
    if (y >= 70 && y < 94) {
      for (int i = 0; i < nd; ++i) {
        const int16_t bx = 4 + i * (dw + 2);
        if (x >= bx && x < bx + dw) {
          if (s.host_kb.buffer.length() < kSoftKbMaxLen) {
            s.host_kb.buffer += kDigits[i];
            s.dirty = true;
          }
          return true;
        }
      }
    }
  }
  bool send = false;
  if (y >= 168 && soft_kb_handle_tap(s.host_kb, x, y, 168, send)) {
    s.dirty = true;
    if (send) {
      String host;
      uint16_t port = 3876;
      if (parse_host_port(s.host_kb.buffer, host, port) && apply_endpoint(cfg, host, port)) {
        out_reconnect = true;
        s.page = SettingsPage::Connexion;
        s.status_line = "Custom OK";
        s.dirty = true;
      } else {
        s.status_line = "host invalide";
        s.dirty = true;
      }
    }
    return true;
  }
  return false;
}
