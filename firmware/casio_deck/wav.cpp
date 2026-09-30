#include "wav.h"

#include <string.h>

static void put16(uint8_t* p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = v >> 8;
}

static void put32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF;
}

void wavHeader(uint8_t* out, uint32_t dataBytes, uint32_t sampleRate) {
  memcpy(out, "RIFF", 4);
  put32(out + 4, 36 + dataBytes);
  memcpy(out + 8, "WAVEfmt ", 8);
  put32(out + 16, 16);              // Groesse des fmt-Blocks
  put16(out + 20, 1);               // PCM
  put16(out + 22, 1);               // mono
  put32(out + 24, sampleRate);
  put32(out + 28, sampleRate * 2);  // Bytes pro Sekunde
  put16(out + 32, 2);               // Bytes pro Sample
  put16(out + 34, 16);              // Bits pro Sample
  memcpy(out + 36, "data", 4);
  put32(out + 40, dataBytes);
}

void wavAmplify(int16_t* samples, size_t count, int gain) {
  if (count == 0) return;
  int64_t sum = 0;
  for (size_t i = 0; i < count; i++) sum += samples[i];
  int32_t dc = static_cast<int32_t>(sum / static_cast<int64_t>(count));
  for (size_t i = 0; i < count; i++) {
    int32_t v = (samples[i] - dc) * gain;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    samples[i] = static_cast<int16_t>(v);
  }
}
