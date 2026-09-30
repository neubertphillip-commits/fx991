// Tastatur des fx-991DE X ueber MCP23017 (I2C), Leitungen siehe config.h.
//
// Ruhezustand: alle Treiber-Leitungen LOW, Interrupt-on-change auf den Eingaengen. INTA
// (oder ein gelegentliches Nachsehen, falls INTA nicht verdrahtet ist) weckt den Scanner;
// dann wird Leitung fuer Leitung gescannt und entprellt, bis alles losgelassen ist.
#pragma once

#include <stdint.h>

// Eine Taste verbindet zwei Leitungen (a < b, Werte aus KeyLine in config.h).
struct KeyEvent {
  uint8_t a;
  uint8_t b;
  bool pressed;  // false = losgelassen
};

namespace keypad {

// Initialisiert I2C und den MCP23017. false, wenn der Baustein nicht antwortet.
bool begin();

// Regelmaessig aus loop() aufrufen. Liefert true und ein Ereignis, solange welche anstehen.
bool poll(KeyEvent& ev);

// true, solange mindestens eine Taste gedrueckt ist (Scanner aktiv).
bool active();

// Vor dem Tiefschlaf: Ruhezustand herstellen, Interrupt quittieren. false, wenn gerade
// eine Taste gedrueckt ist (der ESP32 wuerde sofort wieder aufwachen) oder der
// MCP23017 fehlt (dann koennte ihn nichts wecken).
// Hinweis: . und x10^x liegen zwischen zwei Eingaengen und loesen keinen Interrupt aus;
// sie wecken weder aus dem Tief- noch aus dem Leichtschlaf (dort nach spaetestens 1 s).
bool armWake();

}  // namespace keypad
