// Logische Tasten und Zuordnung Tastenkontakt -> Taste.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum Key : uint8_t {
  K_NONE = 0,
  K_0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
  K_DOT, K_EXP, K_ANS,
  K_ADD, K_SUB, K_MUL, K_DIV, K_POW, K_SQRT,
  K_SIN, K_COS, K_TAN, K_LN, K_LOG,
  K_LPAR, K_RPAR,
  K_EXE,   // "=" bzw. Senden
  K_DEL, K_AC,
  K_SHIFT, K_ALPHA, K_MODE,
  K_UP, K_DOWN, K_LEFT, K_RIGHT,
  K_ON,
  K_COUNT
};

// Ein Tastenkontakt der Casio-Platine (Nummer wie in hardware/tastatur_nummern.jpg)
// verbindet zwei Leitungen (KeyLine aus config.h).
struct KeyContact {
  uint8_t contact;
  uint8_t a;
  uint8_t b;
  Key key;
};
extern const KeyContact KEY_CONTACTS[];
extern const size_t KEY_CONTACT_COUNT;

// Leitungen a/b (Reihenfolge egal) -> logische Taste; K_NONE, wenn unbelegt.
Key keymapLookup(uint8_t a, uint8_t b);
// Leitungen -> Kontaktnummer (1..50), 0 wenn keine Taste diese Leitungen verbindet.
uint8_t keymapContact(uint8_t a, uint8_t b);
// Name einer Leitung ("A", "X4", ...) fuer Debug-Ausgaben.
const char* keyLineName(uint8_t line);

// Kurzname fuer Debug-Ausgaben ("7", "EXE", ...).
const char* keyName(Key k);
