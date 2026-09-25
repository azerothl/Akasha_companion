#include "akasha_http.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <string.h>
#include <stdlib.h>

#include "esp_heap_caps.h"

static String make_url(const CompanionConfig &cfg, const String &path) {
  return String("http://") + cfg.host + ":" + String(cfg.port) + path;
}

static void apply_auth(HTTPClient &http, const CompanionConfig &cfg) {
  if (!cfg.token.isEmpty()) {
    http.addHeader("Authorization", String("Bearer ") + cfg.token);
  }
}

static void *ps_alloc(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) {
    p = malloc(n);
  }
  return p;
}

static bool is_progress_stub(const char *msg) {
  if (!msg || !*msg) {
    return true;
  }
  while (*msg == ' ' || *msg == '\t' || *msg == '\n' || *msg == '\r') {
    ++msg;
  }
  if (!*msg) {
    return true;
  }
  if (strcmp(msg, "Terminé.") == 0 || strcmp(msg, "Done.") == 0 || strcmp(msg, "Échec.") == 0 ||
      strcmp(msg, "Annulé.") == 0 || strcmp(msg, "Failed.") == 0 || strcmp(msg, "Cancelled.") == 0 ||
      strcmp(msg, "Sous-tâches en cours.") == 0) {
    return true;
  }
  return strncmp(msg, "Task delegated to agent", 23) == 0;
}

bool akasha_get_status(const CompanionConfig &cfg, DaemonStatus &out) {
  out = DaemonStatus{};
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  http.setTimeout(5000);
  http.setConnectTimeout(4000);
  if (!http.begin(make_url(cfg, "/api/status"))) {
    return false;
  }
  apply_auth(http, cfg);
  const int code = http.GET();
  out.raw = http.getString();
  http.end();

  out.reachable = (code > 0 && code < 500);
  if (code != 200) {
    Serial.printf("[http] GET /api/status -> %d (ip=%s -> %s:%u)\n", code,
                  WiFi.localIP().toString().c_str(), cfg.host.c_str(), (unsigned)cfg.port);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, out.raw)) {
    out.ok = true;
    return true;
  }
  if (doc["ok"].is<bool>()) {
    out.ok = doc["ok"].as<bool>();
  } else if (doc["status"].is<const char *>()) {
    out.ok = String(doc["status"].as<const char *>()) == "ok";
  } else {
    out.ok = true;
  }
  return true;
}

bool akasha_get_voice_status(const CompanionConfig &cfg, VoiceStatus &out) {
  out = VoiceStatus{};
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(make_url(cfg, "/api/voice/status"))) {
    return false;
  }
  apply_auth(http, cfg);
  const int code = http.GET();
  out.raw = http.getString();
  http.end();

  out.reachable = (code > 0 && code < 500);
  if (code != 200) {
    Serial.printf("[http] GET /api/voice/status -> %d\n", code);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, out.raw)) {
    return false;
  }
  out.stt = doc["stt_configured"] | false;
  out.tts = doc["tts_configured"] | false;
  return true;
}

bool akasha_companion_snapshot(const CompanionConfig &cfg, CompanionSnapshot &out) {
  out = CompanionSnapshot{};
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  http.setTimeout(5000);
  http.setConnectTimeout(4000);
  if (!http.begin(make_url(cfg, "/api/companion/snapshot"))) {
    return false;
  }
  apply_auth(http, cfg);
  const int code = http.GET();
  const String resp = http.getString();
  http.end();

  if (code != 200) {
    Serial.printf("[http] GET /api/companion/snapshot -> %d (ip=%s -> %s:%u)\n", code,
                  WiFi.localIP().toString().c_str(), cfg.host.c_str(), (unsigned)cfg.port);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    return false;
  }
  out.ok = true;
  out.daemon_ok = doc["daemon"]["ok"] | true;
  if (doc["daemon"]["version"].is<const char *>()) {
    out.daemon_version = doc["daemon"]["version"].as<const char *>();
  }
  if (doc["llm"]["provider"].is<const char *>()) {
    out.llm_provider = doc["llm"]["provider"].as<const char *>();
  }
  if (doc["llm"]["model"].is<const char *>()) {
    out.llm_model = doc["llm"]["model"].as<const char *>();
  }
  out.stt = doc["voice"]["stt"] | false;
  out.tts = doc["voice"]["tts"] | false;
  out.tasks_active = doc["tasks"]["active"] | 0;
  out.notify_unread = doc["notify"]["unread"] | 0;
  if (doc["notify"]["headline"].is<const char *>()) {
    out.notify_headline = doc["notify"]["headline"].as<const char *>();
  }
  if (doc["avatar_hint"].is<const char *>()) {
    out.avatar_hint = doc["avatar_hint"].as<const char *>();
  } else {
    out.avatar_hint = "idle";
  }
  if (doc["presence_mode"].is<const char *>()) {
    out.presence_mode = doc["presence_mode"].as<const char *>();
  } else {
    out.presence_mode = "ptt";
  }
  out.vad_enabled = doc["vad_enabled"] | false;
  if (doc["last_event_ts"].is<const char *>()) {
    out.last_event_ts = doc["last_event_ts"].as<const char *>();
  }
  JsonObject presence = doc["presence"].as<JsonObject>();
  if (!presence.isNull()) {
    out.presence_enabled = presence["enabled"] | false;
    out.threshold_rms = presence["threshold_rms"] | 0.035f;
    out.min_speech_ms = presence["min_speech_ms"] | 400;
    out.max_speech_ms = presence["max_speech_ms"] | 10000;
    out.silence_hang_ms = presence["silence_hang_ms"] | 700;
    out.cooldown_ms = presence["cooldown_ms"] | 2500;
    if (presence["quiet_hours"].is<const char *>()) {
      out.quiet_hours = presence["quiet_hours"].as<const char *>();
    }
    out.quiet_now = presence["quiet_now"] | false;
  }
  return true;
}

bool akasha_presence_config_get(const CompanionConfig &cfg, PresenceConfig &out) {
  out = PresenceConfig{};
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(make_url(cfg, "/api/companion/presence/config"))) {
    return false;
  }
  apply_auth(http, cfg);
  const int code = http.GET();
  const String resp = http.getString();
  http.end();
  if (code != 200) {
    Serial.printf("[http] GET /api/companion/presence/config -> %d\n", code);
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    return false;
  }
  out.enabled = doc["enabled"] | false;
  out.threshold_rms = doc["threshold_rms"] | 0.035f;
  out.min_speech_ms = doc["min_speech_ms"] | 400;
  out.max_speech_ms = doc["max_speech_ms"] | 10000;
  out.silence_hang_ms = doc["silence_hang_ms"] | 700;
  out.cooldown_ms = doc["cooldown_ms"] | 2500;
  if (doc["quiet_hours"].is<const char *>()) {
    out.quiet_hours = doc["quiet_hours"].as<const char *>();
  }
  out.quiet_now = doc["quiet_now"] | false;
  out.effective_enabled = doc["effective_enabled"] | (out.enabled && !out.quiet_now);
  return true;
}

bool akasha_presence_config_post(const CompanionConfig &cfg, bool enabled, String &err) {
  err = "";
  if (WiFi.status() != WL_CONNECTED) {
    err = "wifi";
    return false;
  }
  JsonDocument body;
  body["enabled"] = enabled;
  String payload;
  serializeJson(body, payload);

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(make_url(cfg, "/api/companion/presence/config"))) {
    err = "begin";
    return false;
  }
  apply_auth(http, cfg);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(payload);
  http.end();
  Serial.printf("[http] POST /api/companion/presence/config -> %d\n", code);
  if (code != 200) {
    err = String("http_") + code;
    return false;
  }
  return true;
}

bool akasha_presence_event(const CompanionConfig &cfg, const String &event, const String &device_id,
                           const String &detail) {
  if (WiFi.status() != WL_CONNECTED || event.isEmpty()) {
    return false;
  }
  JsonDocument body;
  body["event"] = event;
  if (device_id.length()) {
    body["device_id"] = device_id;
  }
  if (detail.length()) {
    body["detail"] = detail;
  }
  String payload;
  serializeJson(body, payload);

  HTTPClient http;
  http.setTimeout(4000);
  if (!http.begin(make_url(cfg, "/api/companion/presence/event"))) {
    return false;
  }
  apply_auth(http, cfg);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(payload);
  http.end();
  if (code != 200) {
    Serial.printf("[http] POST /api/companion/presence/event -> %d\n", code);
    return false;
  }
  return true;
}

bool akasha_companion_pair(const CompanionConfig &cfg, const String &device_id, const String &name,
                           const String &secret, String &token_out, String &err) {
  token_out = "";
  err = "";
  if (WiFi.status() != WL_CONNECTED) {
    err = "wifi";
    return false;
  }

  JsonDocument body;
  body["device_id"] = device_id;
  body["name"] = name;
  if (secret.length()) {
    body["secret"] = secret;
  }
  String payload;
  serializeJson(body, payload);

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(make_url(cfg, "/api/companion/pair"))) {
    err = "begin";
    return false;
  }
  // Pairing itself may run without a prior token
  if (!cfg.token.isEmpty()) {
    apply_auth(http, cfg);
  }
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(payload);
  const String resp = http.getString();
  http.end();
  Serial.printf("[http] POST /api/companion/pair -> %d\n", code);

  if (code != 200) {
    err = String("http_") + code;
    JsonDocument ed;
    if (!deserializeJson(ed, resp) && ed["error"].is<const char *>()) {
      err = ed["error"].as<const char *>();
    }
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    err = "json";
    return false;
  }
  if (!doc["token"].is<const char *>()) {
    err = "no_token";
    return false;
  }
  token_out = doc["token"].as<const char *>();
  return token_out.length() > 0;
}

bool akasha_voice_stt(const CompanionConfig &cfg, const uint8_t *wav, size_t wav_size, String &text,
                      String &err) {
  text = "";
  err = "";
  if (WiFi.status() != WL_CONNECTED) {
    err = "wifi";
    return false;
  }
  const String data_url = wav_to_data_url(wav, wav_size);
  if (data_url.isEmpty()) {
    err = "encode";
    return false;
  }

  // Manual JSON — avoids ArduinoJson copying the huge data_url
  const size_t payload_len = 16 + data_url.length() + 2;
  char *payload = (char *)ps_alloc(payload_len + 1);
  if (!payload) {
    err = "oom";
    return false;
  }
  snprintf(payload, payload_len + 1, "{\"data_url\":\"%s\"}", data_url.c_str());

  HTTPClient http;
  http.setTimeout(60000);  // ms (uint16 max 65535)
  if (!http.begin(make_url(cfg, "/api/voice/stt"))) {
    free(payload);
    err = "begin";
    return false;
  }
  apply_auth(http, cfg);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST((uint8_t *)payload, strlen(payload));
  free(payload);
  const String resp = http.getString();
  http.end();
  Serial.printf("[http] POST /api/voice/stt -> %d\n", code);

  if (code != 200) {
    err = String("http_") + code;
    JsonDocument ed;
    if (!deserializeJson(ed, resp) && ed["error"].is<const char *>()) {
      err = ed["error"].as<const char *>();
    }
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    err = "json";
    return false;
  }
  text = doc["text"] | "";
  text.trim();
  if (text.isEmpty()) {
    err = "empty_transcript";
    return false;
  }
  return true;
}

static bool read_http_body_psram(HTTPClient &http, char **out_buf, size_t *out_len) {
  *out_buf = nullptr;
  *out_len = 0;
  const int len = http.getSize();
  WiFiClient *stream = http.getStreamPtr();
  if (!stream) {
    return false;
  }

  // Cap ~2.5 MiB for TTS JSON
  const size_t cap = (len > 0 && (size_t)len < 2500000) ? (size_t)len + 4 : 2500000;
  char *buf = (char *)ps_alloc(cap);
  if (!buf) {
    return false;
  }
  size_t n = 0;
  const uint32_t t0 = millis();
  while (http.connected() && (len < 0 || (int)n < len) && n + 1 < cap && millis() - t0 < 90000) {
    const size_t avail = stream->available();
    if (avail == 0) {
      delay(5);
      yield();
      continue;
    }
    const size_t chunk = avail < (cap - 1 - n) ? avail : (cap - 1 - n);
    const int got = stream->readBytes(buf + n, chunk);
    if (got <= 0) {
      break;
    }
    n += (size_t)got;
  }
  buf[n] = 0;
  *out_buf = buf;
  *out_len = n;
  return n > 0;
}

bool akasha_voice_tts(const CompanionConfig &cfg, const String &text, WavBuffer &wav_out, String &err) {
  wav_out = {};
  err = "";
  if (WiFi.status() != WL_CONNECTED) {
    err = "wifi";
    return false;
  }
  String clipped = text;
  if (clipped.length() > 400) {
    clipped = clipped.substring(0, 400);
  }

  JsonDocument body;
  body["text"] = clipped;
  String payload;
  serializeJson(body, payload);

  HTTPClient http;
  http.setTimeout(65000);
  if (!http.begin(make_url(cfg, "/api/voice/tts"))) {
    err = "begin";
    return false;
  }
  apply_auth(http, cfg);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(payload);
  Serial.printf("[http] POST /api/voice/tts -> %d\n", code);
  if (code != 200) {
    const String resp = http.getString();
    http.end();
    err = String("http_") + code;
    JsonDocument ed;
    if (!deserializeJson(ed, resp) && ed["error"].is<const char *>()) {
      err = ed["error"].as<const char *>();
    }
    return false;
  }

  char *resp = nullptr;
  size_t resp_len = 0;
  if (!read_http_body_psram(http, &resp, &resp_len)) {
    http.end();
    err = "body";
    return false;
  }
  http.end();

  const char *key = strstr(resp, "\"data_url\"");
  if (!key) {
    free(resp);
    err = "no_data_url";
    return false;
  }
  const char *q1 = strchr(key + 10, '"');
  if (!q1) {
    free(resp);
    err = "parse";
    return false;
  }
  ++q1;
  const char *q2 = strchr(q1, '"');
  if (!q2) {
    free(resp);
    err = "parse2";
    return false;
  }
  // Temporarily null-terminate for decoder
  char *mutable_q2 = (char *)q2;
  *mutable_q2 = 0;
  const bool ok = wav_from_data_url(q1, wav_out);
  *mutable_q2 = '"';
  free(resp);
  if (!ok) {
    err = "decode";
    return false;
  }
  return true;
}

bool akasha_post_message(const CompanionConfig &cfg, const String &message, const String &session_id,
                         String &task_id, String &err) {
  task_id = "";
  err = "";
  if (WiFi.status() != WL_CONNECTED) {
    err = "wifi";
    return false;
  }

  JsonDocument body;
  body["message"] = message;
  body["session_id"] = session_id;
  String payload;
  serializeJson(body, payload);

  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(make_url(cfg, "/api/message"))) {
    err = "begin";
    return false;
  }
  apply_auth(http, cfg);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(payload);
  const String resp = http.getString();
  http.end();
  Serial.printf("[http] POST /api/message -> %d\n", code);

  if (code != 200) {
    err = String("http_") + code;
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    err = "json";
    return false;
  }
  task_id = doc["task_id"] | "";
  if (task_id.isEmpty()) {
    err = "no_task_id";
    return false;
  }
  return true;
}

bool akasha_poll_task(const CompanionConfig &cfg, const String &task_id, TaskPollResult &out) {
  out = TaskPollResult{};
  if (WiFi.status() != WL_CONNECTED) {
    out.error = "wifi";
    return false;
  }

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(make_url(cfg, String("/api/tasks/") + task_id))) {
    out.error = "begin";
    return false;
  }
  apply_auth(http, cfg);
  const int code = http.GET();
  const String resp = http.getString();
  http.end();
  if (code != 200) {
    out.error = String("http_") + code;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    out.error = "json";
    return false;
  }

  out.status = doc["status"] | "";
  out.ok = true;
  if (out.status == "completed") {
    out.done = true;
  } else if (out.status == "failed" || out.status == "cancelled") {
    out.done = true;
    out.failed = true;
  }

  String best;
  JsonArray progress = doc["progress"].as<JsonArray>();
  if (!progress.isNull()) {
    for (JsonObject e : progress) {
      const char *msg = e["message"] | "";
      if (is_progress_stub(msg)) {
        continue;
      }
      best = msg;
    }
  }
  if (out.failed) {
    const char *fd = doc["failure_detail"] | "";
    if (fd && *fd) {
      best = fd;
    }
  }
  out.reply_text = best;
  return true;
}

bool akasha_wait_task(const CompanionConfig &cfg, const String &task_id, uint32_t timeout_ms,
                      uint32_t poll_ms, TaskPollResult &out) {
  const uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    if (!akasha_poll_task(cfg, task_id, out)) {
      delay(poll_ms);
      continue;
    }
    if (out.done) {
      if (out.failed) {
        return false;
      }
      if (out.reply_text.isEmpty()) {
        out.reply_text = "D'accord.";
      }
      return true;
    }
    delay(poll_ms);
    yield();
  }
  out.error = "timeout";
  out.ok = false;
  return false;
}

bool akasha_http_get_bytes(const String &url, uint8_t **out, size_t *out_len, size_t max_len,
                           String &err) {
  *out = nullptr;
  *out_len = 0;
  err = "";
  if (WiFi.status() != WL_CONNECTED) {
    err = "wifi";
    return false;
  }
  HTTPClient http;
  http.setTimeout(30000);
  if (!http.begin(url)) {
    err = "begin";
    return false;
  }
  const int code = http.GET();
  if (code != 200) {
    http.end();
    err = String("http_") + code;
    return false;
  }
  const int len = http.getSize();
  if (len > 0 && (size_t)len > max_len) {
    http.end();
    err = "too_large";
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  if (!stream) {
    http.end();
    err = "stream";
    return false;
  }
  const size_t cap = (len > 0) ? (size_t)len : max_len;
  uint8_t *buf = (uint8_t *)ps_alloc(cap + 4);
  if (!buf) {
    http.end();
    err = "oom";
    return false;
  }
  size_t n = 0;
  const uint32_t t0 = millis();
  while (http.connected() && n < cap && millis() - t0 < 30000) {
    const size_t avail = stream->available();
    if (!avail) {
      if (len > 0 && (int)n >= len) {
        break;
      }
      delay(5);
      yield();
      continue;
    }
    const size_t chunk = avail < (cap - n) ? avail : (cap - n);
    const int got = stream->readBytes(buf + n, chunk);
    if (got <= 0) {
      break;
    }
    n += (size_t)got;
    if (len > 0 && (int)n >= len) {
      break;
    }
  }
  http.end();
  if (n == 0) {
    free(buf);
    err = "empty";
    return false;
  }
  *out = buf;
  *out_len = n;
  return true;
}

bool akasha_cancel_task(const CompanionConfig &cfg, const String &task_id) {
  if (task_id.isEmpty() || WiFi.status() != WL_CONNECTED) {
    return false;
  }
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(make_url(cfg, String("/api/tasks/") + task_id + "/cancel"))) {
    return false;
  }
  apply_auth(http, cfg);
  const int code = http.POST("");
  http.end();
  Serial.printf("[http] POST cancel -> %d\n", code);
  return code == 200;
}
