#include "power.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/usb_serial_jtag.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

#include "config.h"

// Die Bestaetigung neuer Firmware uebernimmt power::confirmUpdate() statt des
// Arduino-Cores (der sie sonst sofort beim Start bestaetigt).
extern "C" bool verifyRollbackLater() { return true; }

namespace {
bool woke = false;
bool pending = false;
}  // namespace

namespace power {

void begin() {
  woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;

  esp_ota_img_states_t state;
  const esp_partition_t* running = esp_ota_get_running_partition();
  pending = esp_ota_get_state_partition(running, &state) == ESP_OK &&
            state == ESP_OTA_IMG_PENDING_VERIFY;

  // Watchdog: haengt loop() laenger als WATCHDOG_S, startet der ESP32 neu.
  // (Der Standard von 5 s ist zu knapp fuer Verbindungsaufbau und grosse Uploads.)
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WATCHDOG_S * 1000;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
  esp_task_wdt_reconfigure(&cfg);
  enableLoopWDT();  // der Arduino-Core fuettert ihn nach jedem loop()-Durchlauf
}

bool wokeByKey() { return woke; }

bool updatePending() { return pending; }

void confirmUpdate() {
  if (!pending) return;
  esp_ota_mark_app_valid_cancel_rollback();
  pending = false;
  Serial.println("[power] neue Firmware bestaetigt");
}

void sleep() {
  Serial.println("[power] aus, Taste weckt");
  Serial.flush();
  gpio_num_t pin = static_cast<gpio_num_t>(PIN_MCP_INT);
  // INTA ist Open-Drain: der interne Pull-up muss auch im Tiefschlaf aktiv sein
  rtc_gpio_pullup_en(pin);
  rtc_gpio_pulldown_dis(pin);
  esp_sleep_enable_ext0_wakeup(pin, 0);
  esp_deep_sleep_start();
}

bool nap(uint32_t maxMs) {
  // Leichtschlaf trennt die USB-Verbindung; mit angestecktem Kabel wach bleiben
  if (usb_serial_jtag_is_connected()) return false;
  gpio_num_t pin = static_cast<gpio_num_t>(PIN_MCP_INT);
  gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(maxMs) * 1000);
  esp_light_sleep_start();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  gpio_wakeup_disable(pin);
  return true;
}

}  // namespace power
