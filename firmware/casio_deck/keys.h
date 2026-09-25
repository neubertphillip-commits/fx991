// Logische Tasten und Zuordnung Matrixposition -> Taste.
#pragma once

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
  K_COUNT
};

// Matrixposition (Zeile, Spalte) -> logische Taste; K_NONE, wenn unbelegt.
Key keymapLookup(uint8_t row, uint8_t col);

// Kurzname fuer Debug-Ausgaben ("7", "EXE", ...).
const char* keyName(Key k);
