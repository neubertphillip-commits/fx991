// WLAN (Handy-Hotspot) und WebSocket-Verbindung zur Bridge.
// Protokoll siehe bridge/bridge.py. WLAN ist nur an, wenn ein Modus es braucht.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace net {

enum class State : uint8_t {
  Off,         // WLAN aus
  Connecting,  // WLAN verbindet
  WifiUp,      // WLAN steht, WebSocket noch nicht
  Online,      // Bridge verbunden
};

// Wird fuer jede Nachricht der Bridge aufgerufen: type = "busy", "line", "done",
// "err", "pong", "text" (erkannte Sprache), beim Abgleich "del" und "file";
// text ist bei Nachrichten ohne Text leer, size nur bei "file" gesetzt.
using MessageHandler = void (*)(const char* type, const char* text, uint32_t size);
// Binaer-Frames der Bridge (Inhalt der zuletzt angekuendigten Datei, in Stuecken).
using BinaryHandler = void (*)(const uint8_t* data, size_t len);

void begin(MessageHandler handler, BinaryHandler binary);

// Bridge-Adresse aendern (Standard aus secrets.h); z.B. fuer den PC-Simulator.
void setBridge(const char* host, uint16_t port);

void loop();

void enable(bool on);
bool enabled();

// WLAN-Netze in Reichweite seriell ausgeben (Diagnose, * = eigenes Netz).
void scan();

// Sparsamer Wartemodus des Funkmoduls erlaubt (nur waehrend auf Claude gewartet wird,
// nicht bei OTA-Updates, die sonst sehr langsam wuerden).
void allowLowPower(bool allowed);
State state();
const char* stateName(State s);

// false, wenn die Bridge nicht verbunden ist.
bool sendPrompt(const char* text);
bool sendNew();
bool sendPing();
// Binaer-Frame: JPEG (Kamera) oder WAV (Spracheingabe); die Bridge erkennt es am Inhalt.
bool sendBinary(const uint8_t* data, size_t len);

// Datei-Abgleich: meldet die vorhandenen Dateien (Name, Groesse, CRC-32) und den
// freien Platz; die Bridge antwortet mit "del"/"file"-Nachrichten und "done".
struct FileInfo {
  const char* name;
  uint32_t size;
  uint32_t crc;
};
bool sendSync(const FileInfo* files, size_t count, uint32_t freeBytes);

}  // namespace net
