#include "camera.h"

#include <Arduino.h>
#include <esp_camera.h>

#include "config.h"

namespace {

// Pinbelegung XIAO ESP32S3 Sense (wie CAMERA_MODEL_XIAO_ESP32S3 im CameraWebServer-Beispiel)
constexpr int CAM_XCLK = 10;
constexpr int CAM_SIOD = 40;
constexpr int CAM_SIOC = 39;
constexpr int CAM_Y9 = 48;
constexpr int CAM_Y8 = 11;
constexpr int CAM_Y7 = 12;
constexpr int CAM_Y6 = 14;
constexpr int CAM_Y5 = 16;
constexpr int CAM_Y4 = 18;
constexpr int CAM_Y3 = 17;
constexpr int CAM_Y2 = 15;
constexpr int CAM_VSYNC = 38;
constexpr int CAM_HREF = 47;
constexpr int CAM_PCLK = 13;

bool on = false;
camera_fb_t* frame = nullptr;
const char* lastError = "";

}  // namespace

namespace camera {

bool begin() {
  if (on) return true;
  camera_config_t cfg = {};
  cfg.pin_pwdn = -1;
  cfg.pin_reset = -1;
  cfg.pin_xclk = CAM_XCLK;
  cfg.pin_sccb_sda = CAM_SIOD;
  cfg.pin_sccb_scl = CAM_SIOC;
  cfg.pin_d7 = CAM_Y9;
  cfg.pin_d6 = CAM_Y8;
  cfg.pin_d5 = CAM_Y7;
  cfg.pin_d4 = CAM_Y6;
  cfg.pin_d3 = CAM_Y5;
  cfg.pin_d2 = CAM_Y4;
  cfg.pin_d1 = CAM_Y3;
  cfg.pin_d0 = CAM_Y2;
  cfg.pin_vsync = CAM_VSYNC;
  cfg.pin_href = CAM_HREF;
  cfg.pin_pclk = CAM_PCLK;
  cfg.xclk_freq_hz = 20000000;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = CAM_FRAME_SIZE;
  cfg.jpeg_quality = CAM_JPEG_QUALITY;
  cfg.fb_count = 1;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode = CAMERA_GRAB_LATEST;  // immer das aktuelle Bild, kein altes aus dem Puffer

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    lastError = "Kamera nicht gefunden";
    Serial.printf("[cam] esp_camera_init: 0x%x\n", err);
    return false;
  }
  if (sensor_t* s = esp_camera_sensor_get()) {
    // Das Modul sitzt auf der Sense-Platine gedreht; bei Bedarf hier anpassen.
    s->set_vflip(s, CAM_VFLIP);
    s->set_hmirror(s, CAM_HMIRROR);
  }
  on = true;
  Serial.println("[cam] an");
  return true;
}

void end() {
  if (!on) return;
  release();
  esp_camera_deinit();
  on = false;
  Serial.println("[cam] aus");
}

bool isOn() { return on; }

bool capture(const uint8_t*& jpeg, size_t& len) {
  if (!on && !begin()) return false;
  release();
  frame = esp_camera_fb_get();
  if (!frame) {
    lastError = "Aufnahme fehlgeschlagen";
    return false;
  }
  jpeg = frame->buf;
  len = frame->len;
  return true;
}

void release() {
  if (frame) {
    esp_camera_fb_return(frame);
    frame = nullptr;
  }
}

const char* error() { return lastError; }

}  // namespace camera
