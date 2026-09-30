#include "mic.h"

#include <Arduino.h>
#include <ESP_I2S.h>

#include "config.h"
#include "wav.h"

namespace {

I2SClass i2s;
uint8_t* buffer = nullptr;  // WAV-Header + PCM, im PSRAM
size_t capacity = 0;        // max. PCM-Bytes
volatile size_t pcmBytes = 0;
volatile bool stopRequest = false;
volatile bool running = false;
bool ready = false;  // WAV fertig
uint32_t startedAt = 0;
const char* lastError = "";

// Liest in einem eigenen Task, damit keine Samples verloren gehen, waehrend
// loop() mit WLAN oder Tastatur beschaeftigt ist.
void recordTask(void*) {
  while (!stopRequest && pcmBytes < capacity) {
    size_t chunk = capacity - pcmBytes;
    if (chunk > 1024) chunk = 1024;
    size_t n = i2s.readBytes(reinterpret_cast<char*>(buffer + WAV_HEADER_BYTES + pcmBytes), chunk);
    pcmBytes += n & ~static_cast<size_t>(1);  // nur ganze Samples
  }
  running = false;
  vTaskDelete(nullptr);
}

void freeBuffer() {
  if (buffer) {
    free(buffer);
    buffer = nullptr;
  }
  ready = false;
}

void finish(bool keep) {
  stopRequest = true;
  while (running) delay(5);
  i2s.end();  // Mikrofon aus
  if (!keep) {
    freeBuffer();
    return;
  }
  wavAmplify(reinterpret_cast<int16_t*>(buffer + WAV_HEADER_BYTES), pcmBytes / 2, MIC_GAIN);
  wavHeader(buffer, pcmBytes, MIC_SAMPLE_RATE);
  ready = true;
}

}  // namespace

namespace mic {

bool start() {
  if (running) return true;
  freeBuffer();
  capacity = static_cast<size_t>(MIC_SAMPLE_RATE) * 2 * MIC_MAX_SECONDS;
  buffer = static_cast<uint8_t*>(ps_malloc(WAV_HEADER_BYTES + capacity));
  if (!buffer) {
    lastError = "kein Speicher fuer Aufnahme (PSRAM?)";
    return false;
  }
  i2s.setPinsPdmRx(PIN_MIC_CLK, PIN_MIC_DATA);
  if (!i2s.begin(I2S_MODE_PDM_RX, MIC_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    lastError = "Mikrofon startet nicht";
    freeBuffer();
    return false;
  }
  pcmBytes = 0;
  stopRequest = false;
  running = true;
  startedAt = millis();
  if (xTaskCreate(recordTask, "mic", 4096, nullptr, 5, nullptr) != pdPASS) {
    running = false;
    i2s.end();
    freeBuffer();
    lastError = "Aufnahme-Task startet nicht";
    return false;
  }
  return true;
}

void stop() {
  if (buffer && !ready) finish(true);
}

void cancel() {
  if (buffer && !ready) finish(false);
  freeBuffer();
}

bool recording() { return running; }

uint32_t elapsedMs() { return running ? millis() - startedAt : 0; }

bool wav(const uint8_t*& data, size_t& len) {
  if (!ready) return false;
  data = buffer;
  len = WAV_HEADER_BYTES + pcmBytes;
  return true;
}

void release() { freeBuffer(); }

const char* error() { return lastError; }

}  // namespace mic
