#include "ui/app_shell.h"

#include <WiFi.h>

#include "board.h"
#include "config/nvs_config.h"
#include "net/akasha_http.h"
#include "ui/avatar.h"
#include "ui/gestures.h"
#include "ui/text_surface.h"
#include "ui/image_surface.h"
#include "ui/settings_surface.h"
#include "ui/touch_input.h"
#include "ui/rgb_led.h"
#include "ui/power_ui.h"
#include "audio/es8311_bringup.h"
#include "chat/history.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

static AppSurface g_surface = AppSurface::Avatar;
static TextSurface g_text;
static ImageSurface g_image;
static SettingsSurface g_settings;
static ChatHistory g_hist;
static ConversationContext *g_ctx = nullptr;

static String device_id_mac() {
  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  char buf[24];
  snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
  return String(buf);
}

static void reconnect_after_endpoint_change(ConversationContext &ctx) {
  config_save(ctx.cfg);
  avatar_set_overlay(String("-> ") + ctx.cfg.host + ":" + String(ctx.cfg.port), 2500);
  DaemonStatus d;
  VoiceStatus v;
  if (akasha_get_status(ctx.cfg, d) && d.reachable) {
    akasha_get_voice_status(ctx.cfg, v);
    conversation_set_connectivity(ctx, ctx.wifi_ok, d, v);
    if (ctx.cfg.token.isEmpty()) {
      String token;
      String err;
      if (akasha_companion_pair(ctx.cfg, device_id_mac(), BOARD_NAME, String(AKASHA_PAIR_SECRET),
                               token, err)) {
        ctx.cfg.token = token;
        config_save(ctx.cfg);
        avatar_set_overlay("pair OK", 2000);
      } else {
        avatar_set_overlay(String("pair: ") + err, 2500);
      }
    }
  } else {
    d.reachable = false;
    conversation_set_connectivity(ctx, ctx.wifi_ok, d, v);
    avatar_set_overlay("daemon injoignable", 2500);
  }
}

static void apply_settings(ConversationContext &ctx, bool beep, bool reconnect) {
  audio_set_volume((int)ctx.cfg.volume);
  AvatarStyle st = AvatarStyle::Cute;
  if (ctx.cfg.avatar_style < (uint8_t)AvatarStyle::Count) {
    st = (AvatarStyle)ctx.cfg.avatar_style;
  }
  avatar_set_style(st);
  config_save(ctx.cfg);
  if (beep) {
    audio_ux_beep_volume();
  }
  if (reconnect) {
    reconnect_after_endpoint_change(ctx);
  }
}

static void open_settings(ConversationContext &ctx) {
  g_surface = AppSurface::Settings;
  avatar_set_drawing_enabled(false);
  g_settings.dirty = true;
  settings_surface_draw(g_settings, ctx.cfg);
  gesture_reset();
}

static void close_settings(ConversationContext &ctx) {
  g_surface = AppSurface::Avatar;
  avatar_set_drawing_enabled(true);
  avatar_force_redraw();
  gesture_reset();
  (void)ctx;
}

static void on_reply(const String &user_text, const String &assistant_text, void *user) {
  (void)user;
  chat_history_push(g_hist, ChatRole::User, user_text);
  chat_history_push(g_hist, ChatRole::Assistant, assistant_text);
  g_text.dirty = true;

  const String img = chat_extract_image_ref(assistant_text);
  if (img.length()) {
    if (image_surface_load(g_image, img)) {
      g_surface = AppSurface::Image;
      avatar_set_drawing_enabled(false);
      gesture_reset();
    }
  }
}

void app_shell_init(ConversationContext &ctx) {
  g_ctx = &ctx;
  chat_history_clear(g_hist);
  text_surface_init(g_text);
  image_surface_init();
  settings_surface_init(g_settings);
  ctx.on_reply = on_reply;
  ctx.on_reply_user = nullptr;
  g_surface = AppSurface::Avatar;

  // Apply persisted volume / style
  if (ctx.cfg.avatar_style >= (uint8_t)AvatarStyle::Count) {
    ctx.cfg.avatar_style = 0;
  }
  avatar_set_style((AvatarStyle)ctx.cfg.avatar_style);
  audio_set_volume((int)ctx.cfg.volume);
}

AppSurface app_shell_surface() {
  return g_surface;
}

ChatHistory &app_shell_history() {
  return g_hist;
}

static void handle_chip(ConversationContext &ctx, TextChip chip) {
  switch (chip) {
    case TextChip::Brief:
      conversation_run_text_message(ctx, "Fais un brief court de la situation.", true);
      break;
    case TextChip::Stop:
      conversation_stop_speak(ctx);
      avatar_set_overlay("stop", 1500);
      break;
    case TextChip::Repeat:
    case TextChip::Read: {
      String last = ctx.last_assistant;
      if (last.isEmpty()) {
        last = chat_history_last_assistant(g_hist);
      }
      if (last.length()) {
        g_surface = AppSurface::Avatar;
        avatar_set_drawing_enabled(true);
        conversation_speak_text(ctx, last);
      } else {
        avatar_set_overlay("rien a lire", 1500);
      }
      break;
    }
    case TextChip::Mic:
      conversation_run_utterance(ctx);
      g_text.dirty = true;
      break;
    case TextChip::HandsFree:
      conversation_set_hands_free_pref(ctx, !ctx.hands_free_pref, true);
      g_text.dirty = true;
      break;
    default:
      break;
  }
}

void app_shell_tick(ConversationContext &ctx) {
  const bool btn = (digitalRead(PIN_BUTTON) == LOW);
  TouchSample touch;
  touch_read(touch);

  static bool prev_btn = false;
  if (avatar_state() == AvatarState::Speaking && btn && !prev_btn) {
    conversation_stop_speak(ctx);
  }
  prev_btn = btn;

  GestureEvent ge;
  if (gesture_feed(touch, btn, ge)) {
    power_ui_note_activity();
    if (g_surface == AppSurface::Image) {
      if (ge.kind == GestureKind::Tap) {
        g_surface = AppSurface::Avatar;
        avatar_set_drawing_enabled(true);
        avatar_force_redraw();
        gesture_reset();
      }
    } else if (g_surface == AppSurface::Settings) {
      if (ge.kind == GestureKind::Tap) {
        bool back = false;
        bool beep = false;
        bool reconnect = false;
        if (settings_surface_handle_tap(g_settings, ge.x, ge.y, ctx.cfg, back, beep, reconnect)) {
          apply_settings(ctx, beep, reconnect);
          if (back) {
            close_settings(ctx);
          } else if (g_settings.dirty) {
            settings_surface_draw(g_settings, ctx.cfg);
          }
        }
      } else if (ge.kind == GestureKind::SwipeRight || ge.kind == GestureKind::LongPress) {
        close_settings(ctx);
      }
    } else if (g_surface == AppSurface::Text) {
      if (ge.kind == GestureKind::SwipeRight) {
        g_surface = AppSurface::Avatar;
        avatar_set_drawing_enabled(true);
        avatar_force_redraw();
        gesture_reset();
      } else if (ge.kind == GestureKind::Tap) {
        TextChip chip = TextChip::None;
        String send;
        if (text_surface_handle_tap(g_text, ge.x, ge.y, chip, send)) {
          if (chip != TextChip::None) {
            handle_chip(ctx, chip);
          } else if (send.length()) {
            conversation_run_text_message(ctx, send, true);
            g_text.dirty = true;
          }
          if (g_surface == AppSurface::Text) {
            g_text.dirty = true;
          }
        }
      } else if (ge.kind == GestureKind::LongPress) {
        open_settings(ctx);
      }
    } else {  // Avatar
      if (ge.kind == GestureKind::SwipeLeft) {
        g_surface = AppSurface::Text;
        avatar_set_drawing_enabled(false);
        g_text.dirty = true;
        text_surface_draw(g_text, g_hist);
        gesture_reset();
      } else if (ge.kind == GestureKind::LongPress) {
        open_settings(ctx);
      } else if (ge.kind == GestureKind::HoldPtt) {
        const AvatarState st = avatar_state();
        if (st == AvatarState::Idle || st == AvatarState::Notify) {
          conversation_run_utterance(ctx);
        }
        gesture_reset();
      } else if (ge.kind == GestureKind::Tap) {
        String line = String("d=") + (ctx.daemon.ok ? "ok" : "no") + " v=" + ctx.daemon.version +
                      " stt=" + (ctx.voice.stt ? "1" : "0") + " tts=" + (ctx.voice.tts ? "1" : "0") +
                      " hf=" + (ctx.hands_free_active ? "1" : "0");
        avatar_set_overlay(line, 2000);
      }
    }
  }

  if (g_surface == AppSurface::Avatar) {
    if (!avatar_drawing_enabled()) {
      avatar_set_drawing_enabled(true);
    }
    conversation_tick(ctx);
  } else if (g_surface == AppSurface::Text) {
    avatar_set_drawing_enabled(false);
    conversation_tick(ctx);
    if (g_text.dirty) {
      text_surface_draw(g_text, g_hist);
    }
  } else if (g_surface == AppSurface::Image) {
    avatar_set_drawing_enabled(false);
    image_surface_draw(g_image);
  } else if (g_surface == AppSurface::Settings) {
    avatar_set_drawing_enabled(false);
    if (g_settings.dirty) {
      settings_surface_draw(g_settings, ctx.cfg);
    }
  }
}
