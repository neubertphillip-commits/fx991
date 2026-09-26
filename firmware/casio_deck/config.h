// Zentrale Konfiguration: Pins, Tastaturmatrix, Display, Netzwerk.
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// XIAO ESP32S3 Pinbelegung (Dx = Kantenpin, Wert = GPIO)
//
//   D0  GPIO1   MCP23017 INTA (low-aktiv, RTC-faehig -> Wakeup aus Sleep)
//   D1  GPIO2   LT7680 CS
//   D2  GPIO3   LT7680 RST
//   D3  GPIO4   LT7680 WAIT (optional)
//   D4  GPIO5   I2C SDA  (MCP23017)
//   D5  GPIO6   I2C SCL  (MCP23017)
//   D6  GPIO43  frei
//   D7  GPIO44  frei
//   D8  GPIO7   SPI SCK  (LT7680, geteilt mit SD-Slot der Sense-Platine)
//   D9  GPIO8   SPI MISO
//   D10 GPIO9   SPI MOSI
// ---------------------------------------------------------------------------
constexpr int PIN_MCP_INT = 1;
constexpr int PIN_LCD_CS = 2;
constexpr int PIN_LCD_RST = 3;
constexpr int PIN_LCD_WAIT = 4;
constexpr int PIN_SDA = 5;
constexpr int PIN_SCL = 6;
constexpr int PIN_SPI_SCK = 7;
constexpr int PIN_SPI_MISO = 8;
constexpr int PIN_SPI_MOSI = 9;

// ---------------------------------------------------------------------------
// I2C / MCP23017
// Zuerst ohne externe Pull-ups: interne ESP32-Pull-ups, 100 kHz.
// Bei Problemen 4,7 kOhm nach 3V3 nachruesten.
// ---------------------------------------------------------------------------
constexpr uint8_t MCP_ADDR = 0x20;
constexpr uint32_t I2C_FREQ = 100000;

// ---------------------------------------------------------------------------
// Tastaturmatrix (VORLAEUFIG, bis die Matrix des fx-991 ausgemessen ist)
//
// MCP-Pinnummern: 0..7 = GPA0..GPA7, 8..15 = GPB0..GPB7.
// GPA7/GPB7 duerfen laut Datenblatt nur Ausgaenge sein -> Zeilen.
// Spalten sind Eingaenge mit internen Pull-ups (GPPU).
// ---------------------------------------------------------------------------
constexpr uint8_t KEY_ROW_PINS[] = {8, 9, 10, 11, 12, 13, 14, 15, 7};  // GPB0..GPB7, GPA7
constexpr uint8_t KEY_COL_PINS[] = {0, 1, 2, 3, 4, 5, 6};              // GPA0..GPA6
constexpr uint8_t KEY_ROWS = sizeof(KEY_ROW_PINS);
constexpr uint8_t KEY_COLS = sizeof(KEY_COL_PINS);

constexpr uint32_t KEY_SCAN_MS = 5;       // Scanintervall, solange eine Taste gedrueckt ist
constexpr uint8_t KEY_DEBOUNCE_SCANS = 4;  // so viele gleiche Scans = stabil (~20 ms)

// ---------------------------------------------------------------------------
// Display: 480x640 IPS, Textkonsole mit 8x16-Font -> 60x40 Zeichen.
// Muss zu `bridge.py --cols` passen.
// ---------------------------------------------------------------------------
constexpr uint8_t SCREEN_COLS = 60;
constexpr uint8_t SCREEN_ROWS = 40;

// ---------------------------------------------------------------------------
// Netzwerk
// ---------------------------------------------------------------------------
constexpr uint16_t BRIDGE_PORT = 8765;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
// Schnellverbindung mit gespeichertem Kanal/Zugangspunkt; klappt sie nicht, normale Suche.
constexpr uint32_t WIFI_FAST_TIMEOUT_MS = 3000;
// WLAN ist nur an, solange es gebraucht wird: Es geht beim Senden an und nach der
// letzten Antwort (oder Aktion) nach WIFI_LINGER_MS wieder aus.
constexpr uint32_t WIFI_LINGER_MS = 30000;
// Kommt keine Verbindung zur Bridge zustande, wird die Anfrage nach dieser Zeit verworfen.
constexpr uint32_t NET_GIVEUP_MS = 45000;
// SHIFT+MODE im Terminal/Kamera (oder ":ota"): WLAN so lange an, fuer Updates per WLAN.
constexpr uint32_t OTA_WINDOW_MS = 5UL * 60 * 1000;

// ---------------------------------------------------------------------------
// Strom und Updates
// ---------------------------------------------------------------------------
constexpr uint32_t AUTO_OFF_MS = 10UL * 60 * 1000;  // ohne Eingabe nach 10 min aus
constexpr uint32_t WATCHDOG_S = 30;                 // haengt loop() laenger: Neustart
#define OTA_HOSTNAME "casio-deck"                   // -> casio-deck.local

// ---------------------------------------------------------------------------
// Kamera (OV3660). Makros, weil die Typen aus esp_camera.h kommen.
// SVGA 800x600 reicht Claude zum Erkennen und gibt ~30-60 kB JPEG.
// ---------------------------------------------------------------------------
#define CAM_FRAME_SIZE FRAMESIZE_SVGA
#define CAM_JPEG_QUALITY 12  // 0-63, kleiner = besser/groesser
#define CAM_VFLIP 1
#define CAM_HMIRROR 0

// ---------------------------------------------------------------------------
// Mikrofon (PDM auf der Sense-Platine, interne Pins) und Spracheingabe
// ---------------------------------------------------------------------------
constexpr int PIN_MIC_CLK = 42;
constexpr int PIN_MIC_DATA = 41;
constexpr uint32_t MIC_SAMPLE_RATE = 16000;  // passt direkt zu whisper.cpp
constexpr uint32_t MIC_MAX_SECONDS = 30;     // ~1 MB PSRAM
constexpr int MIC_GAIN = 4;                  // Software-Verstaerkung, PDM ist leise
// true: erkannten Text sofort an Claude schicken, statt ihn zum Korrigieren
// in die Eingabezeile zu schreiben.
constexpr bool VOICE_AUTO_SEND = false;

// WLAN-Zugangsdaten und Bridge-Adresse: secrets.h (nicht im Repo, siehe net.cpp).
