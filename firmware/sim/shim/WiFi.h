// WLAN-Ersatz fuer den PC-Simulator: "verbindet" sich nach kurzer Zeit immer.
#pragma once

#include <stdint.h>

#include "Arduino.h"

enum wl_status_t { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum wifi_mode_t { WIFI_OFF = 0, WIFI_STA = 1 };
enum wifi_ps_type_t { WIFI_PS_NONE, WIFI_PS_MIN_MODEM, WIFI_PS_MAX_MODEM };

class SimIPAddress {
 public:
  struct Text {
    const char* c_str() const { return "127.0.0.1"; }
  };
  Text toString() const { return Text(); }
};

class SimWiFi {
 public:
  void persistent(bool) {}
  void mode(wifi_mode_t m) {
    if (m == WIFI_OFF) begun_ = false;
  }
  void setSleep(bool) {}
  void setSleep(wifi_ps_type_t ps) { sleep_ = ps; }
  wifi_ps_type_t getSleep() const { return sleep_; }
  void begin(const char*, const char*, int32_t = 0, const uint8_t* = nullptr, bool = true) {
    begun_ = true;
    since_ = millis();
  }
  const uint8_t* BSSID() const {
    static const uint8_t bssid[6] = {0x02, 0, 0, 0, 0, 1};
    return bssid;
  }
  int32_t channel() const { return 6; }
  void disconnect(bool = false) { begun_ = false; }
  wl_status_t status() const {
    return begun_ && millis() - since_ > 500 ? WL_CONNECTED : WL_DISCONNECTED;
  }
  SimIPAddress localIP() const { return SimIPAddress(); }

 private:
  bool begun_ = false;
  wifi_ps_type_t sleep_ = WIFI_PS_MIN_MODEM;
  uint32_t since_ = 0;
};

extern SimWiFi WiFi;
