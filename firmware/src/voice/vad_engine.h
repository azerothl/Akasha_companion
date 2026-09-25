#pragma once

#include <Arduino.h>
#include "audio/wav_util.h"

enum class VadState : uint8_t {
  Silence,
  PreSpeech,
  Speech,
  Hangover,
  Cooldown,
};

enum class VadEvent : uint8_t {
  None,
  Started,       // entered speech (after pre-speech hysteresis)
  SegmentReady,  // valid PCM→WAV ready in out buffer
  Dropped,       // too short / aborted
};

struct VadPolicy {
  bool enabled = false;
  float threshold_rms = 0.035f;
  uint32_t min_speech_ms = 400;
  uint32_t max_speech_ms = 10000;
  uint32_t silence_hang_ms = 700;
  uint32_t cooldown_ms = 2500;
  String quiet_hours;  // informational; daemon already applies quiet_now
};

struct VadEngine {
  VadPolicy policy;
  VadState state = VadState::Silence;
  uint32_t state_since_ms = 0;
  uint32_t speech_ms = 0;
  uint32_t silence_ms = 0;
  uint32_t cooldown_until_ms = 0;
  uint32_t error_cooldown_until_ms = 0;
  uint32_t frames_above = 0;
  uint32_t frames_below = 0;
  // Capture buffer (mono PCM in PSRAM)
  int16_t *pcm = nullptr;
  size_t pcm_cap = 0;
  size_t pcm_len = 0;
  bool suspended = false;
  uint32_t drops = 0;
  uint32_t sends = 0;
};

void vad_init(VadEngine &e);
void vad_deinit(VadEngine &e);
void vad_reset(VadEngine &e);
void vad_set_policy(VadEngine &e, const VadPolicy &p);
void vad_set_suspended(VadEngine &e, bool suspended);
void vad_force_cooldown(VadEngine &e, uint32_t ms);

// Poll one short I2S frame (~20 ms). When SegmentReady, fills *out (caller wav_free).
VadEvent vad_poll(VadEngine &e, WavBuffer *out);
