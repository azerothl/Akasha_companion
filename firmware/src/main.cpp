#include <Arduino.h>
#include <WiFi.h>

#include "board.h"
#include "config/nvs_config.h"
#include "net/wifi_sta.h"
#include "net/akasha_http.h"
#include "ui/avatar.h"
#include "ui/glcd_text.h"
#include "ui/rgb_led.h"
#include "ui/touch_input.h"
#include "ui/app_shell.h"
#include "ui/power_ui.h"
#include "audio/es8311_bringup.h"
#include "voice/conversation.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

static ConversationContext g_ctx;
static bool g_ready = false;
static CompanionSnapshot g_snap;
static uint32_t g_last_notify_unread = 0;

static void slog(const String &msg) {
  Serial.println(glcd_ascii(msg));
}

static String device_id_from_mac() {
  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  char buf[24];
  snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
  return String(buf);
}

static void apply_snapshot_to_ctx(const CompanionSnapshot &snap) {
  g_ctx.daemon.reachable = snap.ok;
  g_ctx.daemon.ok = snap.daemon_ok;
  g_ctx.daemon.version = snap.daemon_version;
  g_ctx.voice.reachable = snap.ok;
  g_ctx.voice.stt = snap.stt;
  g_ctx.voice.tts = snap.tts;
  conversation_set_connectivity(g_ctx, g_ctx.wifi_ok, g_ctx.daemon, g_ctx.voice);
  conversation_apply_presence(g_ctx, snap);

  const bool want_notify = (snap.avatar_hint == "notify" || snap.notify_unread > 0);
  const AvatarState st = avatar_state();
  if (want_notify &&
      (st == AvatarState::Idle || st == AvatarState::Notify || st == AvatarState::Offline)) {
    avatar_set_state(AvatarState::Notify);
    if (snap.notify_unread != g_last_notify_unread) {
      g_last_notify_unread = snap.notify_unread;
      String headline = snap.notify_headline.length()
                            ? snap.notify_headline
                            : String("notif ") + String(snap.notify_unread);
      avatar_set_overlay(headline, 4000);
      if (g_ctx.audio_ok) {
        audio_ux_beep_ok();
      }
    }
  } else if (!want_notify) {
    g_last_notify_unread = 0;
  }
}

static bool try_pair_if_needed() {
  if (!g_ctx.cfg.token.isEmpty()) {
    slog("pair: token already set");
    return true;
  }
  if (!g_ctx.wifi_ok || !g_ctx.daemon.reachable) {
    slog("pair: skip (offline)");
    return false;
  }
  const String did = device_id_from_mac();
  String token;
  String err;
  const String secret = AKASHA_PAIR_SECRET;
  slog(String("pair: device_id=") + did);
  if (!akasha_companion_pair(g_ctx.cfg, did, BOARD_NAME, secret, token, err)) {
    slog(String("pair FAIL: ") + err);
    return false;
  }
  g_ctx.cfg.token = token;
  config_save(g_ctx.cfg);
  slog("pair OK token saved");
  return true;
}

static void refresh_connectivity() {
  g_ctx.wifi_ok = (WiFi.status() == WL_CONNECTED);
  if (g_ctx.wifi_ok) {
    CompanionSnapshot snap;
    if (akasha_companion_snapshot(g_ctx.cfg, snap)) {
      g_snap = snap;
      apply_snapshot_to_ctx(snap);
      return;
    }
    // Fallback to legacy status endpoints if snapshot route missing (old daemon)
    akasha_get_status(g_ctx.cfg, g_ctx.daemon);
    akasha_get_voice_status(g_ctx.cfg, g_ctx.voice);
    PresenceConfig pc;
    CompanionSnapshot synth;
    synth.ok = g_ctx.daemon.reachable;
    synth.stt = g_ctx.voice.stt;
    synth.tts = g_ctx.voice.tts;
    if (akasha_presence_config_get(g_ctx.cfg, pc)) {
      synth.vad_enabled = pc.effective_enabled && g_ctx.voice.stt;
      synth.presence_enabled = pc.enabled;
      synth.threshold_rms = pc.threshold_rms;
      synth.min_speech_ms = pc.min_speech_ms;
      synth.max_speech_ms = pc.max_speech_ms;
      synth.silence_hang_ms = pc.silence_hang_ms;
      synth.cooldown_ms = pc.cooldown_ms;
      synth.quiet_hours = pc.quiet_hours;
      synth.quiet_now = pc.quiet_now;
    } else {
      // NVS fallback: local pref + default thresholds (no daemon policy file yet)
      synth.vad_enabled = g_ctx.hands_free_pref && g_ctx.voice.stt;
      synth.presence_enabled = g_ctx.hands_free_pref;
      synth.threshold_rms = 0.035f;
      synth.min_speech_ms = 400;
      synth.max_speech_ms = 10000;
      synth.silence_hang_ms = 700;
      synth.cooldown_ms = 2500;
    }
    conversation_set_connectivity(g_ctx, g_ctx.wifi_ok, g_ctx.daemon, g_ctx.voice);
    conversation_apply_presence(g_ctx, synth);
    return;
  }
  g_ctx.daemon = {};
  g_ctx.voice = {};
  g_snap = {};
  conversation_set_connectivity(g_ctx, g_ctx.wifi_ok, g_ctx.daemon, g_ctx.voice);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  slog("");
  slog("=== Akasha Companion Phase 5 ===");
  slog(String("board=") + BOARD_NAME);

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  rgb_init();
  rgb_set(RgbState::Boot);
  power_ui_init();

  if (!avatar_init()) {
    slog("display FAIL");
    rgb_set(RgbState::Error);
  } else {
    slog("display OK");
  }
  touch_init();

  config_load(g_ctx.cfg);
  conversation_init(g_ctx);
  app_shell_init(g_ctx);
  slog(String("host=") + g_ctx.cfg.host + ":" + String(g_ctx.cfg.port));
  slog(String("hands_free_nvs=") + (g_ctx.cfg.hands_free ? "1" : "0"));

  slog("audio init");
  g_ctx.audio_ok = audio_bringup_init();
  if (g_ctx.audio_ok) {
    audio_set_volume((int)g_ctx.cfg.volume);
    audio_play_tone(880, 120);
    slog(String("audio OK vol=") + String((int)g_ctx.cfg.volume));
  } else {
    slog("audio FAIL");
  }

  slog("wifi connect...");
  wifi_connect(g_ctx.cfg, 25000);
  slog(String("wifi status=") + String((int)WiFi.status()));
  g_ctx.wifi_ok = (WiFi.status() == WL_CONNECTED);
  if (g_ctx.wifi_ok) {
    akasha_get_status(g_ctx.cfg, g_ctx.daemon);
    akasha_get_voice_status(g_ctx.cfg, g_ctx.voice);
    conversation_set_connectivity(g_ctx, true, g_ctx.daemon, g_ctx.voice);
  }

  try_pair_if_needed();
  refresh_connectivity();

  if (g_ctx.hands_free_active) {
    avatar_set_overlay("HF: parle librement", 4000);
  } else if (g_ctx.wifi_ok && g_ctx.daemon.reachable && g_ctx.voice.stt && g_ctx.voice.tts) {
    avatar_set_overlay("LongPress=Reglages  Tenir=PTT", 4000);
  } else if (g_ctx.wifi_ok && g_ctx.daemon.reachable) {
    avatar_set_overlay("Daemon OK - STT/TTS?", 4000);
  } else {
    avatar_set_overlay("Attente daemon...", 4000);
  }

  g_ready = true;
  slog(String("session=") + g_ctx.session_id);
  slog("ready - Phase 5 hands-free VAD");
}

void loop() {
  if (g_ready) {
    TouchSample touch;
    const bool btn = (digitalRead(PIN_BUTTON) == LOW);
    if (touch_read(touch) && touch.down) {
      power_ui_note_activity();
    }
    if (btn) {
      power_ui_note_activity();
    }

    const bool busy = conversation_is_busy(g_ctx);
    const bool keep_awake = !g_ctx.daemon.reachable || !g_ctx.wifi_ok;
    power_ui_tick(busy, keep_awake);

    // Wake display on VAD speech start so listening is visible
    if (g_ctx.hands_free_active && avatar_state() == AvatarState::Listening) {
      power_ui_note_activity();
    }

    app_shell_tick(g_ctx);
  }

  static uint32_t last_hb = 0;
  if (millis() - last_hb >= 4000) {
    last_hb = millis();
    slog(String("hb ") + (millis() / 1000) + "s w=" + (g_ctx.wifi_ok ? "1" : "0") + " d=" +
         (g_ctx.daemon.reachable ? "1" : "0") + " hf=" + (g_ctx.hands_free_active ? "1" : "0") +
         " surf=" + String((int)app_shell_surface()) + " bl=" +
         (power_ui_display_asleep() ? "0" : "1"));
  }

  static uint32_t last_net = 0;
  if (g_ready && millis() - last_net > 7000) {
    last_net = millis();
    refresh_connectivity();
  }

  delay(5);
}
