#include "wav_util.h"

#include <string.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "mbedtls/base64.h"
#include "es8311.h"

static void *ps_alloc(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) {
    p = malloc(n);
  }
  return p;
}

void wav_free(WavBuffer &w) {
  if (w.data) {
    free(w.data);
    w.data = nullptr;
  }
  w.size = 0;
}

uint32_t wav_sample_rate() {
  return EXAMPLE_SAMPLE_RATE;
}

static void write_u32_le(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void write_u16_le(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

bool wav_build_mono_16(const int16_t *pcm_mono, size_t sample_count, uint32_t sample_rate,
                       WavBuffer &out) {
  out = {};
  const uint32_t data_bytes = (uint32_t)(sample_count * sizeof(int16_t));
  const size_t total = 44 + data_bytes;
  uint8_t *buf = (uint8_t *)ps_alloc(total);
  if (!buf) {
    return false;
  }
  memcpy(buf, "RIFF", 4);
  write_u32_le(buf + 4, 36 + data_bytes);
  memcpy(buf + 8, "WAVEfmt ", 8);
  write_u32_le(buf + 16, 16);
  write_u16_le(buf + 20, 1);  // PCM
  write_u16_le(buf + 22, 1);  // mono
  write_u32_le(buf + 24, sample_rate);
  write_u32_le(buf + 28, sample_rate * 2);
  write_u16_le(buf + 32, 2);
  write_u16_le(buf + 34, 16);
  memcpy(buf + 36, "data", 4);
  write_u32_le(buf + 40, data_bytes);
  memcpy(buf + 44, pcm_mono, data_bytes);
  out.data = buf;
  out.size = total;
  return true;
}

static uint32_t read_u32_le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

bool wav_parse_pcm16(const uint8_t *wav, size_t wav_size, const int16_t *&pcm, size_t &samples,
                     uint16_t &channels, uint32_t &rate, WavBuffer *owned_copy) {
  pcm = nullptr;
  samples = 0;
  channels = 0;
  rate = 0;
  if (owned_copy) {
    *owned_copy = {};
  }
  if (!wav || wav_size < 44 || memcmp(wav, "RIFF", 4) != 0) {
    return false;
  }

  size_t off = 12;
  const uint8_t *data_ptr = nullptr;
  uint32_t data_size = 0;
  uint16_t bits = 0;

  while (off + 8 <= wav_size) {
    const char *id = (const char *)(wav + off);
    const uint32_t chunk_size = read_u32_le(wav + off + 4);
    const size_t payload = off + 8;
    if (payload + chunk_size > wav_size) {
      break;
    }
    if (memcmp(id, "fmt ", 4) == 0 && chunk_size >= 16) {
      channels = read_u16_le(wav + payload + 2);
      rate = read_u32_le(wav + payload + 4);
      bits = read_u16_le(wav + payload + 14);
    } else if (memcmp(id, "data", 4) == 0) {
      data_ptr = wav + payload;
      data_size = chunk_size;
      break;
    }
    off = payload + chunk_size + (chunk_size & 1);
  }

  if (!data_ptr || bits != 16 || channels < 1 || channels > 2 || rate == 0) {
    return false;
  }

  samples = data_size / (sizeof(int16_t) * channels);
  pcm = (const int16_t *)data_ptr;
  (void)owned_copy;
  return samples > 0;
}

String wav_to_data_url(const uint8_t *wav, size_t wav_size) {
  if (!wav || wav_size == 0) {
    return String();
  }
  size_t olen = 0;
  mbedtls_base64_encode(nullptr, 0, &olen, wav, wav_size);
  char *b64 = (char *)ps_alloc(olen + 1);
  if (!b64) {
    return String();
  }
  size_t written = 0;
  if (mbedtls_base64_encode((unsigned char *)b64, olen + 1, &written, wav, wav_size) != 0) {
    free(b64);
    return String();
  }
  b64[written] = 0;
  String out = String("data:audio/wav;base64,") + b64;
  free(b64);
  return out;
}

bool wav_from_data_url(const char *data_url_or_b64, WavBuffer &out) {
  out = {};
  if (!data_url_or_b64 || !*data_url_or_b64) {
    return false;
  }
  const char *b64 = data_url_or_b64;
  const char *comma = strstr(data_url_or_b64, "base64,");
  if (comma) {
    b64 = comma + 7;
  }
  size_t olen = 0;
  const size_t in_len = strlen(b64);
  mbedtls_base64_decode(nullptr, 0, &olen, (const unsigned char *)b64, in_len);
  uint8_t *buf = (uint8_t *)ps_alloc(olen + 4);
  if (!buf) {
    return false;
  }
  size_t written = 0;
  if (mbedtls_base64_decode(buf, olen + 4, &written, (const unsigned char *)b64, in_len) != 0) {
    free(buf);
    return false;
  }
  out.data = buf;
  out.size = written;
  return written > 0;
}
