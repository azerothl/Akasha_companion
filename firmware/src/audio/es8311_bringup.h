#pragma once

#include <Arduino.h>
#include "audio/wav_util.h"

using AudioRmsCallback = void (*)(float rms01, void *user);

bool audio_bringup_init();
bool audio_ok();
int audio_sample_rate();

bool audio_play_tone(uint16_t freq_hz, uint16_t duration_ms);
bool audio_ux_beep_error();
bool audio_ux_beep_ok();
bool audio_loopback_test(uint16_t record_ms = 1000);

// Record until stop_fn returns true, or max_ms. Builds mono WAV in PSRAM.
// stop_fn may be null (record exactly duration_ms).
using AudioStopFn = bool (*)(void *user);
bool audio_record_wav(uint16_t max_ms, uint16_t min_ms, AudioStopFn stop_fn, void *user,
                      WavBuffer &out);

// Read one short mono frame from I2S (~frame_ms). rms01 is normalized float RMS (0..~1).
bool audio_read_mono_frame(uint16_t frame_ms, int16_t *mono_out, size_t max_samples,
                           size_t *samples_out, float *rms01_out);

// Play WAV (mono or stereo 16-bit). Expands mono to stereo I2S. Optional RMS callback ~every chunk.
// cancel_flag: if non-null and set true, stops early.
bool audio_play_wav(const uint8_t *wav, size_t wav_size, AudioRmsCallback rms_cb, void *user,
                    volatile bool *cancel_flag = nullptr);

// Speaker volume 0..100 (ES8311 DAC). Persisted by caller (NVS).
bool audio_set_volume(int volume_0_100);
int audio_get_volume();
// Short confirmation beep at current volume.
bool audio_ux_beep_volume();
