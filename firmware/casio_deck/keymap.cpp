#include "keys.h"

#include "config.h"

// TODO: nach dem Ausmessen der fx-991-Matrix befuellen (siehe CLAUDE.md, Offen 1).
// Unbelegte Tasten werden auf dem seriellen Monitor mit Zeile/Spalte und
// MCP-Pin gemeldet; so laesst sich die Tabelle Taste fuer Taste ausfuellen.
static const Key KEYMAP[KEY_ROWS][KEY_COLS] = {
    // C0      C1      C2      C3      C4      C5      C6
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R0
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R1
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R2
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R3
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R4
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R5
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R6
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R7
    {K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE, K_NONE},  // R8
};

Key keymapLookup(uint8_t row, uint8_t col) {
  if (row >= KEY_ROWS || col >= KEY_COLS) return K_NONE;
  return KEYMAP[row][col];
}

const char* keyName(Key k) {
  static const char* const NAMES[] = {
      "-",   "0",    "1",    "2",   "3",    "4",    "5",     "6",     "7",
      "8",   "9",    ".",    "EXP", "Ans",  "+",    "-",     "x",     "/",
      "^",   "sqrt", "sin",  "cos", "tan",  "ln",   "log",   "(",     ")",
      "EXE", "DEL",  "AC",   "SHIFT", "ALPHA", "MODE", "UP",  "DOWN",  "LEFT",
      "RIGHT",
  };
  static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == K_COUNT, "Namensliste unvollstaendig");
  return k < K_COUNT ? NAMES[k] : "?";
}
