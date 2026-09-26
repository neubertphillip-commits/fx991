// Ersatz fuer Hardware-Module im Simulator: Zeit, Serial, WLAN, Tastatur, Kamera.
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <vector>

#include "../casio_deck/camera.h"
#include "../casio_deck/keypad.h"
#include "../casio_deck/mic.h"
#include "../casio_deck/ota.h"
#include "../casio_deck/power.h"
#include "Arduino.h"
#include "WiFi.h"
#include "sim.h"

SimSerial Serial;
SimWiFi WiFi;

static const auto startTime = std::chrono::steady_clock::now();

uint32_t millis() {
  auto d = std::chrono::steady_clock::now() - startTime;
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(d).count());
}

void delay(uint32_t ms) { usleep(ms * 1000); }

// ---------------------------------------------------------------------------
// Serielle Ausgabe -> Log
// ---------------------------------------------------------------------------

namespace sim {

uint32_t uiVersion = 0;
const char* cameraImage = nullptr;
const char* micFile = nullptr;

namespace {
constexpr unsigned LOG_LINES = 64;
std::string logBuf[LOG_LINES];
unsigned logCount = 0;
std::string partial;
}  // namespace

void logAppend(const char* text, unsigned len) {
  for (unsigned i = 0; i < len; i++) {
    if (text[i] == '\n') {
      logBuf[logCount++ % LOG_LINES] = partial;
      partial.clear();
      uiVersion++;
    } else if (text[i] != '\r') {
      partial += text[i];
    }
  }
}

const char* logLine(unsigned n) {
  if (n >= logCount || n >= LOG_LINES) return nullptr;
  return logBuf[(logCount - 1 - n) % LOG_LINES].c_str();
}

}  // namespace sim

size_t SimSerial::write(const char* s, size_t len) {
  sim::logAppend(s, static_cast<unsigned>(len));
  return len;
}

// ---------------------------------------------------------------------------
// Tastatur: kommt im Simulator ueber app::injectKey() vom PC-Keyboard
// ---------------------------------------------------------------------------

namespace keypad {
bool begin() { return true; }
bool poll(KeyEvent&) { return false; }
bool active() { return false; }
bool armWake() { return true; }
}  // namespace keypad

// ---------------------------------------------------------------------------
// Updates gibt es im Simulator nicht; "Aus" wartet auf eine Taste
// ---------------------------------------------------------------------------

namespace ota {
void begin(Notify) {}
void loop() {}
bool ready() { return false; }
bool running() { return false; }
}  // namespace ota

namespace power {
void begin() {}
bool wokeByKey() { return false; }
bool updatePending() { return false; }
void confirmUpdate() {}
void sleep() { sim::waitForWake(); }
bool nap(uint32_t) { return false; }
}  // namespace power

// ---------------------------------------------------------------------------
// Kamera: liefert die Datei aus --cam
// ---------------------------------------------------------------------------

namespace camera {

static bool on = false;
static std::vector<uint8_t> image;
static const char* lastError = "";

bool begin() {
  if (!sim::cameraImage) {
    lastError = "Simulator: keine Testbild-Datei (--cam foto.jpg)";
    return false;
  }
  on = true;
  return true;
}

void end() {
  on = false;
  image.clear();
}

bool isOn() { return on; }

bool capture(const uint8_t*& jpeg, size_t& len) {
  if (!on && !begin()) return false;
  FILE* f = fopen(sim::cameraImage, "rb");
  if (!f) {
    lastError = "Simulator: Testbild nicht lesbar";
    return false;
  }
  image.clear();
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) image.insert(image.end(), buf, buf + n);
  fclose(f);
  jpeg = image.data();
  len = image.size();
  return true;
}

void release() {}

const char* error() { return lastError; }

}  // namespace camera

// ---------------------------------------------------------------------------
// Mikrofon: "nimmt" die WAV-Datei aus --mic auf
// ---------------------------------------------------------------------------

namespace mic {

static bool running = false;
static uint32_t startedAt = 0;
static std::vector<uint8_t> recorded;
static const char* lastError = "";

bool start() {
  if (!sim::micFile) {
    lastError = "Simulator: keine Aufnahme-Datei (--mic sprache.wav)";
    return false;
  }
  running = true;
  startedAt = millis();
  recorded.clear();
  return true;
}

void stop() {
  if (!running) return;
  running = false;
  FILE* f = fopen(sim::micFile, "rb");
  if (!f) {
    lastError = "Simulator: Aufnahme-Datei nicht lesbar";
    return;
  }
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) recorded.insert(recorded.end(), buf, buf + n);
  fclose(f);
}

void cancel() {
  running = false;
  recorded.clear();
}

bool recording() { return running; }

uint32_t elapsedMs() { return running ? millis() - startedAt : 0; }

bool wav(const uint8_t*& data, size_t& len) {
  if (recorded.empty()) return false;
  data = recorded.data();
  len = recorded.size();
  return true;
}

void release() { recorded.clear(); }

const char* error() { return lastError; }

}  // namespace mic
