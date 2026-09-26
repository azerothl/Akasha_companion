#include "vad_engine.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "audio/es8311_bringup.h"
#include "esp_heap_caps.h"

static constexpr uint16_t kFrameMs = 20;

static void *ps_alloc(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) {
    p = malloc(n);
  }
  return p;
}

static void free_pcm(VadEngine &e) {
  if (e.pcm) {
    free(e.pcm);
    e.pcm = nullptr;
  }
  e.pcm_cap = 0;
  e.pcm_len = 0;
}

static bool ensure_pcm_cap(VadEngine &e, size_t need_samples) {
  if (e.pcm && e.pcm_cap >= need_samples) {
    return true;
  }
  const size_t rate = (size_t)audio_sample_rate();
  const size_t max_samples =
      rate * (size_t)(e.policy.max_speech_ms > 0 ? e.policy.max_speech_ms : 10000) / 1000 +
      rate / 10;
  const size_t cap = need_samples > max_samples ? need_samples : max_samples;
  int16_t *nbuf = (int16_t *)ps_alloc(cap * sizeof(int16_t));
  if (!nbuf) {
    return false;
  }
  if (e.pcm && e.pcm_len > 0) {
    const size_t copy = e.pcm_len < cap ? e.pcm_len : cap;
    memcpy(nbuf, e.pcm, copy * sizeof(int16_t));
    e.pcm_len = copy;
    free(e.pcm);
  } else {
    e.pcm_len = 0;
  }
  e.pcm = nbuf;
  e.pcm_cap = cap;
  return true;
}

static bool append_pcm(VadEngine &e, const int16_t *mono, size_t n) {
  if (n == 0) {
    return true;
  }
  if (!ensure_pcm_cap(e, e.pcm_len + n)) {
    return false;
  }
  if (e.pcm_len + n > e.pcm_cap) {
    n = e.pcm_cap - e.pcm_len;
  }
  memcpy(e.pcm + e.pcm_len, mono, n * sizeof(int16_t));
  e.pcm_len += n;
  return true;
}

void vad_init(VadEngine &e) {
  e = VadEngine{};
  e.policy = VadPolicy{};
  e.state = VadState::Silence;
  e.state_since_ms = millis();
}

void vad_deinit(VadEngine &e) {
  free_pcm(e);
  e = VadEngine{};
}

void vad_reset(VadEngine &e) {
  free_pcm(e);
  e.state = VadState::Silence;
  e.state_since_ms = millis();
  e.speech_ms = 0;
  e.silence_ms = 0;
  e.frames_above = 0;
  e.frames_below = 0;
}

void vad_set_policy(VadEngine &e, const VadPolicy &p) {
  e.policy = p;
  if (p.max_speech_ms < 1000) {
    e.policy.max_speech_ms = 1000;
  }
  if (p.max_speech_ms > 15000) {
    e.policy.max_speech_ms = 15000;
  }
  if (p.min_speech_ms < 100) {
    e.policy.min_speech_ms = 100;
  }
  if (p.threshold_rms < 0.001f) {
    e.policy.threshold_rms = 0.001f;
  }
  if (p.threshold_rms > 0.5f) {
    e.policy.threshold_rms = 0.5f;
  }
}

void vad_set_suspended(VadEngine &e, bool suspended) {
  if (suspended && !e.suspended) {
    // Drop in-progress capture quietly
    if (e.state == VadState::PreSpeech || e.state == VadState::Speech ||
        e.state == VadState::Hangover) {
      free_pcm(e);
      e.state = VadState::Silence;
      e.speech_ms = 0;
      e.silence_ms = 0;
    }
  }
  e.suspended = suspended;
}

void vad_force_cooldown(VadEngine &e, uint32_t ms) {
  free_pcm(e);
  e.state = VadState::Cooldown;
  e.state_since_ms = millis();
  e.cooldown_until_ms = millis() + ms;
  e.error_cooldown_until_ms = e.cooldown_until_ms;
  e.speech_ms = 0;
  e.silence_ms = 0;
}

VadEvent vad_poll(VadEngine &e, WavBuffer *out) {
  if (out) {
    *out = {};
  }
  if (!e.policy.enabled || e.suspended || !audio_ok()) {
    return VadEvent::None;
  }
  const uint32_t now = millis();
  if (now < e.error_cooldown_until_ms) {
    e.state = VadState::Cooldown;
    return VadEvent::None;
  }
  if (e.state == VadState::Cooldown) {
    if (now < e.cooldown_until_ms) {
      return VadEvent::None;
    }
    e.state = VadState::Silence;
    e.state_since_ms = now;
  }

  int16_t frame[640];  // 40 ms @ 16 kHz max
  size_t n = 0;
  float rms = 0.0f;
  if (!audio_read_mono_frame(kFrameMs, frame, sizeof(frame) / sizeof(frame[0]), &n, &rms)) {
    return VadEvent::None;
  }
  if (n == 0) {
    return VadEvent::None;
  }

  const bool loud = rms >= e.policy.threshold_rms;
  if (loud) {
    e.frames_above++;
    e.frames_below = 0;
  } else {
    e.frames_below++;
    e.frames_above = 0;
  }

  VadEvent ev = VadEvent::None;

  switch (e.state) {
    case VadState::Silence:
      if (loud && e.frames_above >= 2) {
        e.state = VadState::PreSpeech;
        e.state_since_ms = now;
        e.speech_ms = kFrameMs;
        e.silence_ms = 0;
        free_pcm(e);
        append_pcm(e, frame, n);
      }
      break;

    case VadState::PreSpeech:
      if (!append_pcm(e, frame, n)) {
        vad_force_cooldown(e, e.policy.cooldown_ms);
        e.drops++;
        return VadEvent::Dropped;
      }
      if (loud) {
        e.speech_ms += kFrameMs;
        e.silence_ms = 0;
        if (e.speech_ms >= e.policy.min_speech_ms / 2) {
          e.state = VadState::Speech;
          e.state_since_ms = now;
          ev = VadEvent::Started;
        }
      } else {
        e.silence_ms += kFrameMs;
        if (e.silence_ms >= 120) {
          free_pcm(e);
          e.state = VadState::Silence;
          e.speech_ms = 0;
          e.silence_ms = 0;
        }
      }
      break;

    case VadState::Speech:
      if (!append_pcm(e, frame, n)) {
        // Buffer full — force end if enough speech, else drop
        if (e.speech_ms >= e.policy.min_speech_ms && out) {
          if (wav_build_mono_16(e.pcm, e.pcm_len, (uint32_t)audio_sample_rate(), *out)) {
            e.sends++;
            ev = VadEvent::SegmentReady;
          } else {
            e.drops++;
            ev = VadEvent::Dropped;
          }
        } else {
          e.drops++;
          ev = VadEvent::Dropped;
        }
        free_pcm(e);
        e.state = VadState::Cooldown;
        e.cooldown_until_ms = now + e.policy.cooldown_ms;
        e.speech_ms = 0;
        e.silence_ms = 0;
        break;
      }
      e.speech_ms += kFrameMs;
      if (loud) {
        e.silence_ms = 0;
      } else {
        e.silence_ms += kFrameMs;
        if (e.silence_ms >= e.policy.silence_hang_ms) {
          e.state = VadState::Hangover;
          e.state_since_ms = now;
        }
      }
      if (e.speech_ms >= e.policy.max_speech_ms) {
        if (out && wav_build_mono_16(e.pcm, e.pcm_len, (uint32_t)audio_sample_rate(), *out)) {
          e.sends++;
          ev = VadEvent::SegmentReady;
        } else {
          e.drops++;
          ev = VadEvent::Dropped;
        }
        free_pcm(e);
        e.state = VadState::Cooldown;
        e.cooldown_until_ms = now + e.policy.cooldown_ms;
        e.speech_ms = 0;
        e.silence_ms = 0;
      }
      break;

    case VadState::Hangover:
      if (loud) {
        // Resume speech
        if (!append_pcm(e, frame, n)) {
          vad_force_cooldown(e, e.policy.cooldown_ms);
          e.drops++;
          return VadEvent::Dropped;
        }
        e.state = VadState::Speech;
        e.silence_ms = 0;
        e.speech_ms += kFrameMs;
        break;
      }
      // finalize
      if (e.speech_ms >= e.policy.min_speech_ms && out && e.pcm_len > 0) {
        if (wav_build_mono_16(e.pcm, e.pcm_len, (uint32_t)audio_sample_rate(), *out)) {
          e.sends++;
          ev = VadEvent::SegmentReady;
        } else {
          e.drops++;
          ev = VadEvent::Dropped;
        }
      } else {
        e.drops++;
        ev = VadEvent::Dropped;
      }
      free_pcm(e);
      e.state = VadState::Cooldown;
      e.cooldown_until_ms = now + e.policy.cooldown_ms;
      e.speech_ms = 0;
      e.silence_ms = 0;
      break;

    case VadState::Cooldown:
      break;
  }

  return ev;
}
