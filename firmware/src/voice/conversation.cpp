#include "conversation.h"



#include <WiFi.h>



#include "audio/es8311_bringup.h"

#include "ui/avatar.h"

#include "ui/glcd_text.h"

#include "ui/power_ui.h"

#include "ui/rgb_led.h"

#include "ui/touch_input.h"

#include "board.h"



static bool ptt_held_now() {

  const bool btn = (digitalRead(PIN_BUTTON) == LOW);

  TouchSample t;

  bool touch = false;

  if (touch_read(t) && t.down && touch_in_ptt_zone(t.x, t.y)) {

    touch = true;

  }

  return btn || touch;

}



static bool stop_when_ptt_released(void * /*user*/) {

  return !ptt_held_now();

}



static void on_rms(float open, void *user) {

  (void)user;

  avatar_set_mouth(open);

  static uint32_t last = 0;

  if (millis() - last > 50) {

    last = millis();

    avatar_tick();

  }

}



static void restore_idle(ConversationContext &ctx) {

  ctx.conversation_busy = false;

  if (!ctx.wifi_ok || !ctx.daemon.reachable) {

    avatar_set_state(AvatarState::Offline);

    rgb_set(RgbState::Offline);

    return;

  }

  avatar_set_state(AvatarState::Idle);

  if (!ctx.voice.stt || !ctx.voice.tts) {

    rgb_set(RgbState::Warn);

  } else if (ctx.hands_free_active) {

    rgb_set(RgbState::Ok);

  } else {

    rgb_set(RgbState::Ok);

  }

}



static String short_voice_error(const String &raw) {
  const String s = glcd_ascii(raw);
  if (s.indexOf("speech_transcribe") >= 0 || s.indexOf("/stt") >= 0) {
    if (s.indexOf("connect") >= 0 || s.indexOf("refused") >= 0 || s.indexOf("10061") >= 0) {
      return "STT coupe";
    }
    return "STT erreur";
  }
  if (s.indexOf("speech_synthesize") >= 0 || s.indexOf("/tts") >= 0) {
    if (s.indexOf("connect") >= 0 || s.indexOf("refused") >= 0 || s.indexOf("10061") >= 0) {
      return "TTS coupe";
    }
    return "TTS erreur";
  }
  if (s.startsWith("http_502") || s.indexOf("502") >= 0) {
    return "voix KO";
  }
  if (s.length() > 28) {
    return s.substring(0, 28);
  }
  return s;
}

static void fail_ux(ConversationContext &ctx, const char *msg) {

  const String short_msg = short_voice_error(String(msg));
  Serial.printf("[voice] fail: %s\n", glcd_ascii(String(msg)).c_str());

  avatar_set_state(AvatarState::Error);

  avatar_set_mood(AvatarMood::Concerned, 2000);

  avatar_set_overlay(short_msg, 2500);

  rgb_set(RgbState::Error);

  if (ctx.audio_ok) {

    audio_ux_beep_error();

  }

  delay(350);

  restore_idle(ctx);

}



static void emit_reply(ConversationContext &ctx, const String &user_text, const String &assistant) {

  ctx.last_assistant = assistant;

  if (ctx.on_reply) {

    ctx.on_reply(user_text, assistant, ctx.on_reply_user);

  }

}



static void post_presence(ConversationContext &ctx, const char *event, const String &detail) {

  if (!ctx.wifi_ok || !ctx.daemon.reachable) {

    return;

  }

  akasha_presence_event(ctx.cfg, event, ctx.device_id, detail);

}



static void recompute_hands_free(ConversationContext &ctx) {

  const bool was = ctx.hands_free_active;

  ctx.hands_free_active = ctx.hands_free_pref && ctx.daemon_vad_enabled && ctx.wifi_ok &&

                          ctx.daemon.reachable && ctx.voice.stt && ctx.audio_ok;

  VadPolicy p = ctx.vad.policy;

  p.enabled = ctx.hands_free_active;

  vad_set_policy(ctx.vad, p);

  if (was != ctx.hands_free_active) {

    Serial.printf("[vad] active=%d pref=%d daemon=%d stt=%d\n", (int)ctx.hands_free_active,

                  (int)ctx.hands_free_pref, (int)ctx.daemon_vad_enabled, (int)ctx.voice.stt);

    if (ctx.hands_free_active) {

      post_presence(ctx, "hands_free_on", "");

      avatar_set_overlay("mains libres", 2000);

    } else {

      post_presence(ctx, "hands_free_off", "");

      vad_reset(ctx.vad);

    }

  }

}



static void process_stt_wav(ConversationContext &ctx, WavBuffer &wav, bool from_vad) {

  ctx.conversation_busy = true;

  avatar_set_state(AvatarState::Thinking);

  avatar_set_overlay("Reflexion...", 120000);

  avatar_force_redraw();



  String transcript;

  String err;

  if (!akasha_voice_stt(ctx.cfg, wav.data, wav.size, transcript, err)) {

    wav_free(wav);

    if (from_vad) {

      post_presence(ctx, "error", err);

      vad_force_cooldown(ctx.vad, ctx.vad.policy.cooldown_ms + 1500);

    }

    fail_ux(ctx, err.c_str());

    return;

  }

  wav_free(wav);

  Serial.printf("[voice] stt: %s\n", glcd_ascii(transcript).c_str());

  avatar_set_overlay(transcript.substring(0, 36), 2000);

  if (from_vad) {

    post_presence(ctx, "speech_sent", String(transcript.length()) + "c");

  }

  conversation_run_text_message(ctx, transcript, true);

}



void conversation_init(ConversationContext &ctx) {

  ctx.cancel_speak = false;

  ctx.last_task_id = "";

  ctx.last_assistant = "";

  ctx.conversation_busy = false;

  vad_init(ctx.vad);

  uint8_t mac[6] = {};

  WiFi.macAddress(mac);

  char sid[48];

  snprintf(sid, sizeof(sid), "companion-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3],

           mac[4], mac[5]);

  ctx.session_id = sid;

  char did[16];

  snprintf(did, sizeof(did), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],

           mac[5]);

  ctx.device_id = did;

  ctx.hands_free_pref = ctx.cfg.hands_free;

  Serial.printf("[voice] session_id=%s hands_free_pref=%d\n", sid, (int)ctx.hands_free_pref);

}



void conversation_set_connectivity(ConversationContext &ctx, bool wifi_ok, const DaemonStatus &d,

                                   const VoiceStatus &v) {

  ctx.wifi_ok = wifi_ok;

  ctx.daemon = d;

  ctx.voice = v;



  // Auto-disable hands-free if STT gone or offline

  if (!wifi_ok || !d.reachable || !v.stt) {

    if (ctx.hands_free_active) {

      ctx.daemon_vad_enabled = false;

    }

  }

  recompute_hands_free(ctx);



  const AvatarState s = avatar_state();

  if (s == AvatarState::Listening || s == AvatarState::Thinking || s == AvatarState::Speaking ||

      s == AvatarState::Error) {

    return;

  }

  if (!wifi_ok || !d.reachable) {

    avatar_set_state(AvatarState::Offline);

    rgb_set(RgbState::Offline);

    return;

  }

  if (s != AvatarState::Notify) {

    avatar_set_state(AvatarState::Idle);

  }

  if (!v.stt || !v.tts) {

    rgb_set(RgbState::Warn);

  } else {

    rgb_set(RgbState::Ok);

  }

}



void conversation_apply_presence(ConversationContext &ctx, const CompanionSnapshot &snap) {

  VadPolicy p;

  p.enabled = false;  // set via recompute

  p.threshold_rms = snap.threshold_rms > 0 ? snap.threshold_rms : 0.035f;

  p.min_speech_ms = snap.min_speech_ms ? snap.min_speech_ms : 400;

  p.max_speech_ms = snap.max_speech_ms ? snap.max_speech_ms : 10000;

  p.silence_hang_ms = snap.silence_hang_ms ? snap.silence_hang_ms : 700;

  p.cooldown_ms = snap.cooldown_ms ? snap.cooldown_ms : 2500;

  p.quiet_hours = snap.quiet_hours;

  vad_set_policy(ctx.vad, p);



  ctx.daemon_vad_enabled = snap.vad_enabled || (snap.presence_enabled && !snap.quiet_now);

  // Prefer explicit vad_enabled from snapshot

  if (snap.ok) {

    ctx.daemon_vad_enabled = snap.vad_enabled;

  }

  recompute_hands_free(ctx);

}



void conversation_set_hands_free_pref(ConversationContext &ctx, bool on, bool sync_daemon) {

  ctx.hands_free_pref = on;

  ctx.cfg.hands_free = on;

  config_save(ctx.cfg);

  if (sync_daemon && ctx.wifi_ok && ctx.daemon.reachable) {

    String err;

    if (!akasha_presence_config_post(ctx.cfg, on, err)) {

      Serial.printf("[vad] config post fail: %s\n", err.c_str());

    } else {

      ctx.daemon_vad_enabled = on;

    }

  }

  recompute_hands_free(ctx);

  avatar_set_overlay(on ? "HF on" : "HF off", 1800);

}



bool conversation_is_busy(const ConversationContext &ctx) {

  const AvatarState s = avatar_state();

  return ctx.conversation_busy || s == AvatarState::Listening || s == AvatarState::Thinking ||

         s == AvatarState::Speaking;

}



void conversation_tick(ConversationContext &ctx) {

  avatar_tick();



  const bool asleep = power_ui_display_asleep();

  const bool busy = conversation_is_busy(ctx);

  const bool ptt = ptt_held_now();

  vad_set_suspended(ctx.vad, asleep || busy || ptt || !ctx.hands_free_active);



  if (!ctx.hands_free_active || asleep || busy || ptt) {

    return;

  }



  WavBuffer seg = {};

  const VadEvent ev = vad_poll(ctx.vad, &seg);

  if (ev == VadEvent::Started) {

    avatar_set_state(AvatarState::Listening);

    avatar_set_overlay("j'ecoute...", 8000);

    avatar_force_redraw();

    post_presence(ctx, "vad_start", "");

  } else if (ev == VadEvent::Dropped) {

    post_presence(ctx, "speech_dropped", "short");

    avatar_set_overlay("...", 600);

    restore_idle(ctx);

  } else if (ev == VadEvent::SegmentReady) {

    post_presence(ctx, "vad_end", String(seg.size));

    ctx.conversation_busy = true;

    process_stt_wav(ctx, seg, true);

  }

}



void conversation_stop_speak(ConversationContext &ctx) {

  ctx.cancel_speak = true;

  if (!ctx.last_task_id.isEmpty()) {

    akasha_cancel_task(ctx.cfg, ctx.last_task_id);

  }

}



bool conversation_speak_text(ConversationContext &ctx, const String &text) {

  if (!ctx.audio_ok || text.isEmpty()) {

    return false;

  }

  if (!ctx.wifi_ok || !ctx.voice.tts) {

    fail_ux(ctx, "TTS KO");

    return false;

  }

  ctx.conversation_busy = true;

  vad_set_suspended(ctx.vad, true);

  const String speak = speech_plain(text);
  if (speak.isEmpty()) {
    restore_idle(ctx);
    return false;
  }

  String err;

  WavBuffer tts = {};

  if (!akasha_voice_tts(ctx.cfg, speak, tts, err)) {

    fail_ux(ctx, err.c_str());

    return false;

  }

  avatar_set_state(AvatarState::Speaking);

  avatar_set_overlay("", 0);

  avatar_force_redraw();

  ctx.cancel_speak = false;

  audio_play_wav(tts.data, tts.size, on_rms, nullptr, &ctx.cancel_speak);

  wav_free(tts);

  avatar_set_mouth(0);

  // Cooldown after TTS to avoid capturing own speech / echo

  vad_force_cooldown(ctx.vad, ctx.vad.policy.cooldown_ms);

  avatar_set_mood(AvatarMood::Happy, 2200);

  restore_idle(ctx);

  return true;

}



bool conversation_run_text_message(ConversationContext &ctx, const String &text, bool speak) {

  if (text.isEmpty()) {

    return false;

  }

  if (!ctx.wifi_ok || !ctx.daemon.reachable) {

    fail_ux(ctx, "hors ligne");

    return false;

  }



  ctx.conversation_busy = true;

  vad_set_suspended(ctx.vad, true);

  avatar_set_state(AvatarState::Thinking);

  avatar_set_overlay("Reflexion...", 120000);

  avatar_force_redraw();



  String task_id;

  String err;

  if (!akasha_post_message(ctx.cfg, text, ctx.session_id, task_id, err)) {

    fail_ux(ctx, err.c_str());

    return false;

  }

  ctx.last_task_id = task_id;



  TaskPollResult task;

  if (!akasha_wait_task(ctx.cfg, task_id, 180000, 800, task)) {

    fail_ux(ctx, task.error.length() ? task.error.c_str() : "echec tache");

    return false;

  }

  {

    const String ascii = glcd_ascii(task.reply_text);

    Serial.printf("[voice] reply: %s\n", ascii.c_str());

  }

  emit_reply(ctx, text, task.reply_text);



  if (speak && ctx.voice.tts && ctx.audio_ok) {

    return conversation_speak_text(ctx, task.reply_text);

  }

  avatar_set_mood(AvatarMood::Happy, 2000);

  restore_idle(ctx);

  return true;

}



void conversation_run_utterance(ConversationContext &ctx) {

  if (!ctx.audio_ok) {

    fail_ux(ctx, "audio KO");

    return;

  }

  if (!ctx.wifi_ok || !ctx.daemon.reachable) {

    fail_ux(ctx, "hors ligne");

    return;

  }

  if (!ctx.voice.stt || !ctx.voice.tts) {

    fail_ux(ctx, "STT/TTS KO");

    return;

  }



  ctx.conversation_busy = true;

  vad_set_suspended(ctx.vad, true);

  vad_reset(ctx.vad);



  avatar_set_state(AvatarState::Listening);

  avatar_set_overlay("J'ecoute...", 9000);

  avatar_force_redraw();

  if (ctx.cfg.ptt_beep) {
    audio_ux_beep_ok();
  }



  WavBuffer wav = {};

  if (!audio_record_wav(8000, 600, stop_when_ptt_released, nullptr, wav)) {

    fail_ux(ctx, "echec micro");

    return;

  }



  process_stt_wav(ctx, wav, false);

}


