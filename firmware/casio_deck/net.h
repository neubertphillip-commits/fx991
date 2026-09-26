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
// "err", "pong", "text" (erkannte Sprache); text ist bei Nachrichten ohne Text leer.
using MessageHandler = void (*)(const char* type, const char* text);

void begin(MessageHandler handler);

// Bridge-Adresse aendern (Standard aus secrets.h); z.B. fuer den PC-Simulator.
void setBridge(const char* host, uint16_t port);

void loop();

void enable(bool on);
bool enabled();
State state();
const char* stateName(State s);

// false, wenn die Bridge nicht verbunden ist.
bool sendPrompt(const char* text);
bool sendNew();
bool sendPing();
// Binaer-Frame: JPEG (Kamera) oder WAV (Spracheingabe); die Bridge erkennt es am Inhalt.
bool sendBinary(const uint8_t* data, size_t len);

}  // namespace net
