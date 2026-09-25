// Tastaturmatrix ueber MCP23017 (I2C).
//
// Ruhezustand: alle Zeilen LOW, Interrupt-on-change auf den Spalten. INTA (oder
// ein gelegentliches Lesen der Spalten, falls INTA nicht verdrahtet ist) weckt den
// Scanner; dann wird zeilenweise gescannt und entprellt, bis alles losgelassen ist.
#pragma once

#include <stdint.h>

struct KeyEvent {
  uint8_t row;
  uint8_t col;
  bool pressed;  // false = losgelassen
};

namespace keypad {

// Initialisiert I2C und den MCP23017. false, wenn der Baustein nicht antwortet.
bool begin();

// Regelmaessig aus loop() aufrufen. Liefert true und ein Ereignis, solange welche anstehen.
bool poll(KeyEvent& ev);

// true, solange mindestens eine Taste gedrueckt ist (Scanner aktiv).
bool active();

}  // namespace keypad
