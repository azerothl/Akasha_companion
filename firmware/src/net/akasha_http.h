#pragma once

#include "config/nvs_config.h"
#include "audio/wav_util.h"

struct DaemonStatus {
  bool reachable = false;
  bool ok = false;
  String raw;
  String version;
};

struct VoiceStatus {
  bool reachable = false;
  bool stt = false;
  bool tts = false;
  String raw;
};

struct CompanionSnapshot {
  bool ok = false;
  bool daemon_ok = false;
  String daemon_version;
  String llm_provider;
  String llm_model;
  bool stt = false;
  bool tts = false;
  uint32_t tasks_active = 0;
  uint32_t notify_unread = 0;
  String notify_headline;
  String avatar_hint;  // idle | notify
  // Phase 5 presence
  String presence_mode;  // ptt | hands_free
  bool vad_enabled = false;
  String last_event_ts;
  bool presence_enabled = false;
  float threshold_rms = 0.035f;
  uint32_t min_speech_ms = 400;
  uint32_t max_speech_ms = 10000;
  uint32_t silence_hang_ms = 700;
  uint32_t cooldown_ms = 2500;
  String quiet_hours;
  bool quiet_now = false;
};

struct PresenceConfig {
  bool enabled = false;
  float threshold_rms = 0.035f;
  uint32_t min_speech_ms = 400;
  uint32_t max_speech_ms = 10000;
  uint32_t silence_hang_ms = 700;
  uint32_t cooldown_ms = 2500;
  String quiet_hours;
  bool quiet_now = false;
  bool effective_enabled = false;
};

struct TaskPollResult {
  bool ok = false;
  bool done = false;
  bool failed = false;
  String status;
  String reply_text;
  String error;
};

bool akasha_get_status(const CompanionConfig &cfg, DaemonStatus &out);
bool akasha_get_voice_status(const CompanionConfig &cfg, VoiceStatus &out);
bool akasha_companion_snapshot(const CompanionConfig &cfg, CompanionSnapshot &out);
bool akasha_companion_pair(const CompanionConfig &cfg, const String &device_id, const String &name,
                           const String &secret, String &token_out, String &err);
bool akasha_presence_config_get(const CompanionConfig &cfg, PresenceConfig &out);
bool akasha_presence_config_post(const CompanionConfig &cfg, bool enabled, String &err);
bool akasha_presence_event(const CompanionConfig &cfg, const String &event, const String &device_id,
                           const String &detail);

// POST /api/voice/stt — wav bytes → transcript text
bool akasha_voice_stt(const CompanionConfig &cfg, const uint8_t *wav, size_t wav_size, String &text,
                      String &err);

// POST /api/voice/tts — text → wav buffer (PSRAM)
bool akasha_voice_tts(const CompanionConfig &cfg, const String &text, WavBuffer &wav_out, String &err);

// POST /api/message → task_id
bool akasha_post_message(const CompanionConfig &cfg, const String &message, const String &session_id,
                         String &task_id, String &err);

// Single GET /api/tasks/:id poll
bool akasha_poll_task(const CompanionConfig &cfg, const String &task_id, TaskPollResult &out);

// Poll until done/failed or timeout_ms
bool akasha_wait_task(const CompanionConfig &cfg, const String &task_id, uint32_t timeout_ms,
                      uint32_t poll_ms, TaskPollResult &out);

// Absolute URL GET into PSRAM (caller free()). max_len budget.
bool akasha_http_get_bytes(const String &url, uint8_t **out, size_t *out_len, size_t max_len,
                           String &err);

// Best-effort cancel
bool akasha_cancel_task(const CompanionConfig &cfg, const String &task_id);
