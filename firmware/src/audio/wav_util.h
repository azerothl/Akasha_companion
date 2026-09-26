#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

struct WavBuffer {
  uint8_t *data = nullptr;
  size_t size = 0;
};

void wav_free(WavBuffer &w);

// Build mono 16-bit PCM WAV in PSRAM. pcm_mono: interleaved samples already mono.
bool wav_build_mono_16(const int16_t *pcm_mono, size_t sample_count, uint32_t sample_rate,
                       WavBuffer &out);

// Parse WAV; returns pointer into buffer (or converted copy). pcm_out owned if owned=true.
bool wav_parse_pcm16(const uint8_t *wav, size_t wav_size, const int16_t *&pcm, size_t &samples,
                     uint16_t &channels, uint32_t &rate, WavBuffer *owned_copy);

// Base64 encode binary → String (heap; large — use for HTTP body).
String wav_to_data_url(const uint8_t *wav, size_t wav_size);

// Decode data:audio/...;base64,... or raw base64 into WavBuffer (PSRAM).
bool wav_from_data_url(const char *data_url_or_b64, WavBuffer &out);

uint32_t wav_sample_rate();
