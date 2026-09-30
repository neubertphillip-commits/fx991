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
// Tastatur des fx-991DE X (ausgemessen, siehe hardware/tastatur_messung.md):
// 16 Leitungen, jede an einem MCP-Pin (0..7 = GPA0..GPA7, 8..15 = GPB0..GPB7).
//
// Keine reine Zeilen/Spalten-Matrix: 0, ., x10^x, Ans und EXE verbinden Leitung A mit
// anderen Leitungen. Deshalb treibt der Scanner jede Treiber-Leitung einzeln LOW und
// liest alle anderen.
//
//   Treiber, im Ruhezustand LOW:  Q X5 X4 X3 G F B X6 (GPB0..GPB7) und ON (GPA7)
//   Eingaenge mit Pull-up + Interrupt: C N H M L K A (GPA0..GPA6)
//
// GPA7/GPB7 duerfen laut Datenblatt nur Ausgaenge sein, deshalb liegen dort ON und X6.
// A ist Eingang und wird zusaetzlich einzeln getrieben (fuer . und x10^x, die zwischen
// A und den Eingaengen N bzw. H liegen).
// ---------------------------------------------------------------------------
enum KeyLine : uint8_t {
  KL_C = 0, KL_N, KL_H, KL_M, KL_L, KL_K, KL_A, KL_ON,  // GPA0..GPA7
  KL_Q = 8, KL_X5, KL_X4, KL_X3, KL_G, KL_F, KL_B, KL_X6,  // GPB0..GPB7
  KEY_LINES = 16
};
constexpr uint16_t KEY_DRIVE_MASK = 0xFF80;  // Treiber (Ausgaenge), im Ruhezustand LOW
constexpr uint16_t KEY_SENSE_MASK = 0x007F;  // Eingaenge mit Pull-up und Interrupt

constexpr uint32_t KEY_SCAN_MS = 5;       // Scanintervall, solange eine Taste gedrueckt ist
constexpr uint8_t KEY_DEBOUNCE_SCANS = 3;  // so viele gleiche Scans = stabil (~25 ms)

// ---------------------------------------------------------------------------
// Display: 640x480 IPS quer hinter Display- und Solarfenster (CLAUDE.md, Variante A),
// 8x16-Font. Sichtbar im Displayfenster: 640x327 px = 80 x 20 Zeichen (19 Inhalt +
// Eingabe); die Statuszeile zeigt der Streifen im Solarfenster (457x100 px, 57 x 6).
// Hochkant ohne Fenster waeren es 60 x 40. Muss zu `bridge.py --cols` passen.
// ---------------------------------------------------------------------------
constexpr uint8_t SCREEN_COLS = 80;
constexpr uint8_t SCREEN_ROWS = 21;

// ---------------------------------------------------------------------------
// Netzwerk
// ---------------------------------------------------------------------------
constexpr uint16_t BRIDGE_PORT = 8765;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
// Schnellverbindung mit gespeichertem Kanal/Zugangspunkt; klappt sie nicht, normale Suche.
constexpr uint32_t WIFI_FAST_TIMEOUT_MS = 3000;
// WLAN ist nur an, solange es gebraucht wird: Es geht beim Senden an und kurz nach der
// Antwort wieder aus (Nachlauf fuer letzte Nachrichten der Bridge). Die naechste
// Anfrage verbindet dank gespeichertem Kanal in etwa 1 s neu.
constexpr uint32_t WIFI_LINGER_MS = 3000;
// Beim Warten auf die Antwort schlaeft das Funkmodul und hoert nur jeden n-ten Beacon
// des Hotspots (~100 ms Abstand) ab: 10 = etwa jede Sekunde. Der Hotspot puffert so
// lange. Trennt der Hotspot die Verbindung oder laesst keine Anmeldung zu: 3 probieren.
constexpr uint8_t WIFI_LISTEN_INTERVAL = 10;
// Nach dem letzten Senden so lange mit vollem Tempo, damit Uploads (Bild, Sprache)
// nicht auf die Schlafpausen warten; danach sparsamer Wartemodus.
constexpr uint32_t WIFI_ACTIVE_MS = 2000;
// Kommt keine Verbindung zur Bridge zustande, wird die Anfrage nach dieser Zeit verworfen.
constexpr uint32_t NET_GIVEUP_MS = 45000;
// SHIFT+MODE im Terminal/Kamera (oder ":ota"): WLAN so lange an, fuer Updates per WLAN.
constexpr uint32_t OTA_WINDOW_MS = 5UL * 60 * 1000;

// ---------------------------------------------------------------------------
// Strom und Updates
// ---------------------------------------------------------------------------
constexpr uint32_t AUTO_OFF_MS = 10UL * 60 * 1000;  // ohne Eingabe nach 10 min aus
constexpr uint32_t WATCHDOG_S = 30;                 // haengt loop() laenger: Neustart
// 80 MHz reichen fuer Rechner, Terminal, WLAN, Kamera und Mikro und brauchen deutlich
// weniger Strom als die 240 MHz des Cores (WLAN braucht mindestens 80).
constexpr uint32_t CPU_MHZ = 80;
// Leichtschlaf zwischen Tastendruecken, solange WLAN, Kamera und Mikro aus sind:
// der ESP32 haelt an, eine Taste (INTA) oder der Timer weckt ihn in ~1 ms wieder.
// Nicht bei angestecktem USB (serieller Monitor) und nur mit verdrahtetem INTA.
constexpr bool IDLE_LIGHT_SLEEP = true;
constexpr uint32_t IDLE_NAP_AFTER_MS = 2000;  // erst so lange nach der letzten Eingabe
constexpr uint32_t IDLE_NAP_MAX_MS = 1000;    // laengstens am Stueck (fuer Auto-Aus)
#define OTA_HOSTNAME "casio-deck"                   // -> casio-deck.local

// ---------------------------------------------------------------------------
// Kamera (OV3660). Makros, weil die Typen aus esp_camera.h kommen.
// SVGA 800x600 reicht Claude zum Erkennen und gibt ~30-60 kB JPEG.
// ---------------------------------------------------------------------------
#define CAM_FRAME_SIZE FRAMESIZE_SVGA
#define CAM_JPEG_QUALITY 12  // 0-63, kleiner = besser/groesser
#define CAM_VFLIP 1
#define CAM_HMIRROR 0
#define CAM_WARMUP_MS 1500  // so lange vor jedem Foto Bilder verwerfen (Weissabgleich)

// ---------------------------------------------------------------------------
// Mikrofon (PDM auf der Sense-Platine, interne Pins) und Spracheingabe
// ---------------------------------------------------------------------------
constexpr int PIN_MIC_CLK = 42;
constexpr int PIN_MIC_DATA = 41;
constexpr uint32_t MIC_SAMPLE_RATE = 16000;  // passt direkt zu whisper.cpp
constexpr uint32_t MIC_MAX_SECONDS = 30;     // ~1 MB PSRAM
constexpr int MIC_GAIN = 4;                  // Software-Verstaerkung, PDM ist leise
constexpr uint32_t MIC_SETTLE_MS = 250;      // Einschwingen des PDM-Filters verwerfen
// true: erkannten Text sofort an Claude schicken, statt ihn zum Korrigieren
// in die Eingabezeile zu schreiben.
constexpr bool VOICE_AUTO_SEND = false;

// WLAN-Zugangsdaten und Bridge-Adresse: secrets.h (nicht im Repo, siehe net.cpp).
