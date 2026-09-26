#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "credentials.h"

namespace {

ota::Notify notify = nullptr;
bool started = false;
bool updating = false;

void say(const char* msg) {
  Serial.printf("[ota] %s\n", msg);
  if (notify) notify(msg);
}

void configure() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
#ifdef OTA_PASSWORD
  ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart([]() {
    updating = true;
    say("Update laeuft, nicht ausschalten ...");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    esp_task_wdt_reset();  // Upload laeuft innerhalb von loop() und dauert
    static unsigned lastPercent = 0;
    unsigned percent = total ? done * 100 / total : 0;
    if (percent / 10 != lastPercent / 10) Serial.printf("[ota] %u %%\n", percent);
    lastPercent = percent;
  });
  ArduinoOTA.onEnd([]() { say("Update fertig, Neustart"); });
  ArduinoOTA.onError([](ota_error_t err) {
    updating = false;
    char buf[40];
    snprintf(buf, sizeof(buf), "Update fehlgeschlagen (%u)", static_cast<unsigned>(err));
    say(buf);
  });
}

}  // namespace

namespace ota {

void begin(Notify n) {
  notify = n;
  configure();
}

void loop() {
  bool wifi = WiFi.status() == WL_CONNECTED;
  if (wifi && !started) {
    ArduinoOTA.begin();
    started = true;
    Serial.printf("[ota] bereit: %s.local / %s\n", OTA_HOSTNAME, WiFi.localIP().toString().c_str());
  } else if (!wifi && started) {
    ArduinoOTA.end();
    started = false;
    updating = false;
  }
  if (started) ArduinoOTA.handle();
}

bool ready() { return started; }

bool running() { return updating; }

}  // namespace ota
