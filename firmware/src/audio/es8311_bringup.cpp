#include "es8311_bringup.h"

#include <Wire.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "driver/i2s.h"
#include "esp_heap_caps.h"
#include "board.h"
#include "es8311.h"

static bool g_audio_ok = false;
static constexpr i2s_port_t kPort = I2S_NUM_0;
static constexpr int kSampleRate = EXAMPLE_SAMPLE_RATE;
static constexpr int kMclkMultiple = EXAMPLE_MCLK_MULTIPLE;
static es8311_handle_t g_es = nullptr;
static int g_volume = EXAMPLE_VOICE_VOLUME;

static void *ps_alloc(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) {
    p = malloc(n);
  }
  return p;
}

static bool i2s_start() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = kSampleRate * kMclkMultiple;

  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[audio] i2s_driver_install failed");
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.mck_io_num = PIN_I2S_MCK;
  pins.bck_io_num = PIN_I2S_BCK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = PIN_I2S_DIN;

  if (i2s_set_pin(kPort, &pins) != ESP_OK) {
    Serial.println("[audio] i2s_set_pin failed");
    i2s_driver_uninstall(kPort);
    return false;
  }

  int16_t silence[2] = {0, 0};
  size_t w = 0;
  for (int i = 0; i < 64; ++i) {
    i2s_write(kPort, silence, sizeof(silence), &w, pdMS_TO_TICKS(20));
  }
  delay(50);
  Serial.printf("[audio] I2S ready rate=%d mclk=%d\n", kSampleRate, kSampleRate * kMclkMultiple);
  return true;
}

bool audio_bringup_init() {
  pinMode(PIN_PA_ENABLE, OUTPUT);
  digitalWrite(PIN_PA_ENABLE, LOW);
  Serial.printf("[audio] PA_ENABLE GPIO%d = LOW (amp on)\n", PIN_PA_ENABLE);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  delay(20);

  Wire.beginTransmission(ES8311_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[audio] ES8311 not on I2C - skip audio");
    g_audio_ok = false;
    return false;
  }

  if (!i2s_start()) {
    g_audio_ok = false;
    return false;
  }

  const esp_err_t codec = es8311_codec_init();
  if (codec != ESP_OK) {
    Serial.printf("[audio] es8311_codec_init failed: %s\n", esp_err_to_name(codec));
    g_audio_ok = false;
    return false;
  }

  g_audio_ok = true;
  g_es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  g_volume = EXAMPLE_VOICE_VOLUME;
  Serial.println("[audio] Freenove/Espressif ES8311 + I2S ready");
  return true;
}

bool audio_ok() {
  return g_audio_ok;
}

bool audio_set_volume(int volume_0_100) {
  if (volume_0_100 < 0) {
    volume_0_100 = 0;
  } else if (volume_0_100 > 100) {
    volume_0_100 = 100;
  }
  g_volume = volume_0_100;
  if (!g_audio_ok) {
    return false;
  }
  if (!g_es) {
    g_es = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  }
  if (!g_es) {
    return false;
  }
  const esp_err_t err = es8311_voice_volume_set(g_es, g_volume, nullptr);
  Serial.printf("[audio] volume=%d err=%s\n", g_volume, esp_err_to_name(err));
  return err == ESP_OK;
}

int audio_get_volume() {
  return g_volume;
}

bool audio_ux_beep_volume() {
  // Scale tone amplitude with volume so the preview matches perception
  if (!g_audio_ok) {
    return false;
  }
  const uint16_t amp = (uint16_t)(4000 + (g_volume * 140));
  digitalWrite(PIN_PA_ENABLE, LOW);
  const size_t frames = (size_t)kSampleRate * 140 / 1000;
  int16_t *buf = (int16_t *)ps_alloc(frames * 2 * sizeof(int16_t));
  if (!buf) {
    return false;
  }
  for (size_t i = 0; i < frames; ++i) {
    const float t = (float)i / (float)kSampleRate;
    const float s = sinf(2.0f * (float)M_PI * 880.0f * t);
    const int16_t v = (int16_t)((s >= 0.0f ? 1.0f : -1.0f) * (float)amp);
    buf[i * 2] = v;
    buf[i * 2 + 1] = v;
  }
  size_t written = 0;
  const esp_err_t err =
      i2s_write(kPort, buf, frames * 2 * sizeof(int16_t), &written, pdMS_TO_TICKS(400));
  free(buf);
  return err == ESP_OK && written > 0;
}

int audio_sample_rate() {
  return kSampleRate;
}

bool audio_play_tone(uint16_t freq_hz, uint16_t duration_ms) {
  if (!g_audio_ok) {
    return false;
  }
  digitalWrite(PIN_PA_ENABLE, LOW);

  const size_t frames = (size_t)kSampleRate * duration_ms / 1000;
  int16_t *buf = (int16_t *)ps_alloc(frames * 2 * sizeof(int16_t));
  if (!buf) {
    return false;
  }
  for (size_t i = 0; i < frames; ++i) {
    const float t = (float)i / (float)kSampleRate;
    const float s = sinf(2.0f * (float)M_PI * freq_hz * t);
    // Softer UX tones — avoid clipping the PA at volume 58+
    const int16_t v = (int16_t)((s >= 0.0f ? 1.0f : -1.0f) * 14000.0f);
    buf[i * 2] = v;
    buf[i * 2 + 1] = v;
  }

  size_t written = 0;
  const esp_err_t err = i2s_write(kPort, buf, frames * 2 * sizeof(int16_t), &written,
                                  pdMS_TO_TICKS(duration_ms + 800));
  free(buf);
  return err == ESP_OK && written > 0;
}

bool audio_ux_beep_error() {
  return audio_play_tone(440, 180) && audio_play_tone(330, 220);
}

bool audio_ux_beep_ok() {
  return audio_play_tone(880, 120);
}

bool audio_loopback_test(uint16_t record_ms) {
  WavBuffer wav = {};
  if (!audio_record_wav(record_ms, record_ms, nullptr, nullptr, wav)) {
    return false;
  }
  const bool ok = audio_play_wav(wav.data, wav.size, nullptr, nullptr, nullptr);
  wav_free(wav);
  return ok;
}

bool audio_record_wav(uint16_t max_ms, uint16_t min_ms, AudioStopFn stop_fn, void *user,
                      WavBuffer &out) {
  out = {};
  if (!g_audio_ok) {
    return false;
  }
  if (max_ms < 200) {
    max_ms = 200;
  }
  if (min_ms > max_ms) {
    min_ms = max_ms;
  }

  const size_t max_frames = (size_t)kSampleRate * max_ms / 1000;
  int16_t *stereo = (int16_t *)ps_alloc(max_frames * 2 * sizeof(int16_t));
  if (!stereo) {
    Serial.println("[audio] record: OOM stereo");
    return false;
  }
  memset(stereo, 0, max_frames * 2 * sizeof(int16_t));

  Serial.printf("[audio] recording max=%ums\n", max_ms);
  const uint32_t t0 = millis();
  size_t frames = 0;
  while (frames < max_frames) {
    const uint32_t elapsed = millis() - t0;
    if (elapsed >= min_ms && stop_fn && stop_fn(user)) {
      break;
    }
    if (elapsed >= max_ms) {
      break;
    }
    size_t got = 0;
    const size_t want = (max_frames - frames) * 2 * sizeof(int16_t);
    i2s_read(kPort, (uint8_t *)stereo + frames * 2 * sizeof(int16_t),
             want > 512 ? 512 : want, &got, pdMS_TO_TICKS(50));
    frames += got / (2 * sizeof(int16_t));
  }

  int16_t *mono = (int16_t *)ps_alloc(frames * sizeof(int16_t));
  if (!mono) {
    free(stereo);
    return false;
  }
  int16_t peak = 0;
  for (size_t i = 0; i < frames; ++i) {
    mono[i] = stereo[i * 2];  // left
    const int16_t a = mono[i] < 0 ? (int16_t)-mono[i] : mono[i];
    if (a > peak) {
      peak = a;
    }
  }
  free(stereo);

  Serial.printf("[audio] recorded frames=%u peak=%d\n", (unsigned)frames, (int)peak);
  const bool ok = wav_build_mono_16(mono, frames, (uint32_t)kSampleRate, out);
  free(mono);
  return ok;
}

bool audio_read_mono_frame(uint16_t frame_ms, int16_t *mono_out, size_t max_samples,
                           size_t *samples_out, float *rms01_out) {
  if (samples_out) {
    *samples_out = 0;
  }
  if (rms01_out) {
    *rms01_out = 0.0f;
  }
  if (!g_audio_ok || !mono_out || max_samples == 0 || frame_ms == 0) {
    return false;
  }
  if (frame_ms > 80) {
    frame_ms = 80;
  }
  size_t want_frames = (size_t)kSampleRate * frame_ms / 1000;
  if (want_frames > max_samples) {
    want_frames = max_samples;
  }
  if (want_frames == 0) {
    return false;
  }

  // Stereo I2S buffer
  int16_t stereo[640 * 2];
  const size_t stereo_cap = sizeof(stereo) / sizeof(stereo[0]) / 2;
  if (want_frames > stereo_cap) {
    want_frames = stereo_cap;
  }

  size_t got = 0;
  const esp_err_t err =
      i2s_read(kPort, stereo, want_frames * 2 * sizeof(int16_t), &got, pdMS_TO_TICKS(frame_ms + 40));
  if (err != ESP_OK || got < 4) {
    return false;
  }
  const size_t frames = got / (2 * sizeof(int16_t));
  double acc = 0.0;
  for (size_t i = 0; i < frames; ++i) {
    const int16_t s = stereo[i * 2];  // left / mic
    mono_out[i] = s;
    const double f = (double)s / 32768.0;
    acc += f * f;
  }
  if (samples_out) {
    *samples_out = frames;
  }
  if (rms01_out) {
    *rms01_out = (float)sqrt(acc / (double)(frames > 0 ? frames : 1));
  }
  return frames > 0;
}

bool audio_play_wav(const uint8_t *wav, size_t wav_size, AudioRmsCallback rms_cb, void *user,
                    volatile bool *cancel_flag) {
  if (!g_audio_ok || !wav || wav_size == 0) {
    return false;
  }
  digitalWrite(PIN_PA_ENABLE, LOW);

  const int16_t *pcm = nullptr;
  size_t samples = 0;
  uint16_t channels = 0;
  uint32_t rate = 0;
  if (!wav_parse_pcm16(wav, wav_size, pcm, samples, channels, rate, nullptr)) {
    Serial.println("[audio] play: bad wav");
    return false;
  }

  // Chunk ~20 ms at our I2S rate (resample not implemented — play at device rate)
  const size_t chunk_frames = (size_t)kSampleRate / 50;
  int16_t *out = (int16_t *)ps_alloc(chunk_frames * 2 * sizeof(int16_t));
  if (!out) {
    return false;
  }

  // Extra digital headroom — ES8311+PA still clips near full-scale PCM.
  static constexpr float kPlayGain = 1.0f;
  // If WAV rate differs from I2S, linear resample (edge-tts often 24 kHz).
  const uint32_t src_rate = rate ? rate : (uint32_t)kSampleRate;
  const bool need_resample = (src_rate != (uint32_t)kSampleRate);

  size_t idx = 0;
  // Source sample cursor in Q16 fixed for resample
  uint64_t src_pos_q16 = 0;
  const uint64_t step_q16 =
      need_resample ? (((uint64_t)src_rate << 16) / (uint32_t)kSampleRate) : (1ull << 16);
  const size_t src_frames = channels == 1 ? samples : samples;

  while (true) {
    if (cancel_flag && *cancel_flag) {
      break;
    }
    if (!need_resample && idx >= samples) {
      break;
    }
    if (need_resample) {
      const size_t src_i = (size_t)(src_pos_q16 >> 16);
      if (src_i >= src_frames) {
        break;
      }
    }

    const size_t n = chunk_frames;
    double acc = 0.0;
    size_t produced = 0;
    for (size_t i = 0; i < n; ++i) {
      int16_t s = 0;
      if (!need_resample) {
        if (idx >= samples) {
          break;
        }
        if (channels == 1) {
          s = pcm[idx];
        } else {
          s = pcm[idx * 2];
        }
        idx++;
      } else {
        const size_t src_i = (size_t)(src_pos_q16 >> 16);
        if (src_i >= src_frames) {
          break;
        }
        const size_t src_j = (src_i + 1 < src_frames) ? src_i + 1 : src_i;
        const uint32_t frac = (uint32_t)(src_pos_q16 & 0xffffu);
        int16_t s0, s1;
        if (channels == 1) {
          s0 = pcm[src_i];
          s1 = pcm[src_j];
        } else {
          s0 = pcm[src_i * 2];
          s1 = pcm[src_j * 2];
        }
        const int32_t lerped =
            ((int32_t)s0 * (65536 - (int32_t)frac) + (int32_t)s1 * (int32_t)frac) >> 16;
        s = (int16_t)lerped;
        src_pos_q16 += step_q16;
      }
      int32_t scaled = (int32_t)((float)s * kPlayGain);
      if (scaled > 32767) {
        scaled = 32767;
      } else if (scaled < -32768) {
        scaled = -32768;
      }
      s = (int16_t)scaled;
      out[produced * 2] = s;
      out[produced * 2 + 1] = s;
      const double f = (double)s / 32768.0;
      acc += f * f;
      produced++;
    }
    if (produced == 0) {
      break;
    }
    for (size_t i = produced; i < chunk_frames; ++i) {
      out[i * 2] = 0;
      out[i * 2 + 1] = 0;
    }
    if (rms_cb) {
      const float rms = (float)sqrt(acc / (double)(produced > 0 ? produced : 1));
      float open = rms * 8.0f;
      if (open > 1.0f) {
        open = 1.0f;
      }
      rms_cb(open, user);
    }
    size_t bytes_total = produced * 2 * sizeof(int16_t);
    size_t bytes_done = 0;
    while (bytes_done < bytes_total) {
      size_t written = 0;
      const esp_err_t err =
          i2s_write(kPort, (uint8_t *)out + bytes_done, bytes_total - bytes_done, &written,
                    pdMS_TO_TICKS(500));
      if (err != ESP_OK || written == 0) {
        break;
      }
      bytes_done += written;
    }
    yield();
  }

  if (rms_cb) {
    rms_cb(0.0f, user);
  }
  free(out);
  return true;
}
