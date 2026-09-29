#include "keys.h"

#include "config.h"

// Ausgemessen am fx-991DE CW, siehe hardware/tastatur_messung.md. Kontakt 31-50 ist der
// Zahlenblock, 5-8 die Pfeiltasten. TODO: Tasten 1-4 und 9-30 zuordnen, sobald ihre
// Beschriftung notiert ist; bis dahin meldet der serielle Monitor sie als "Kontakt NN".
const KeyContact KEY_CONTACTS[] = {
    {1, KL_C, KL_Q, K_NONE},   {2, KL_N, KL_Q, K_NONE},   {3, KL_L, KL_Q, K_NONE},
    {4, KL_K, KL_ON, K_NONE},  {5, KL_H, KL_Q, K_UP},     {6, KL_H, KL_X6, K_LEFT},
    {7, KL_M, KL_Q, K_RIGHT},  {8, KL_M, KL_X6, K_DOWN},  {9, KL_C, KL_X6, K_NONE},
    {10, KL_N, KL_X6, K_NONE}, {11, KL_L, KL_X6, K_NONE}, {12, KL_K, KL_X6, K_NONE},
    {13, KL_C, KL_X5, K_NONE}, {14, KL_N, KL_X5, K_NONE}, {15, KL_H, KL_X5, K_NONE},
    {16, KL_M, KL_X5, K_NONE}, {17, KL_L, KL_X5, K_NONE}, {18, KL_K, KL_X5, K_NONE},
    {19, KL_C, KL_X4, K_NONE}, {20, KL_N, KL_X4, K_NONE}, {21, KL_H, KL_X4, K_NONE},
    {22, KL_M, KL_X4, K_NONE}, {23, KL_L, KL_X4, K_NONE}, {24, KL_K, KL_X4, K_NONE},
    {25, KL_C, KL_X3, K_NONE}, {26, KL_N, KL_X3, K_NONE}, {27, KL_H, KL_X3, K_NONE},
    {28, KL_M, KL_X3, K_NONE}, {29, KL_L, KL_X3, K_NONE}, {30, KL_K, KL_X3, K_NONE},
    {31, KL_C, KL_G, K_7},     {32, KL_N, KL_G, K_8},     {33, KL_H, KL_G, K_9},
    {34, KL_M, KL_G, K_DEL},   {35, KL_L, KL_G, K_AC},
    {36, KL_C, KL_F, K_4},     {37, KL_N, KL_F, K_5},     {38, KL_H, KL_F, K_6},
    {39, KL_M, KL_F, K_MUL},   {40, KL_L, KL_F, K_DIV},
    {41, KL_C, KL_B, K_1},     {42, KL_N, KL_B, K_2},     {43, KL_H, KL_B, K_3},
    {44, KL_M, KL_B, K_ADD},   {45, KL_L, KL_B, K_SUB},
    {46, KL_A, KL_X4, K_0},    {47, KL_A, KL_N, K_DOT},   {48, KL_A, KL_H, K_EXP},
    {49, KL_A, KL_F, K_ANS},   {50, KL_A, KL_B, K_EXE},
};
const size_t KEY_CONTACT_COUNT = sizeof(KEY_CONTACTS) / sizeof(KEY_CONTACTS[0]);

static const KeyContact* find(uint8_t a, uint8_t b) {
  for (size_t i = 0; i < KEY_CONTACT_COUNT; i++) {
    const KeyContact& c = KEY_CONTACTS[i];
    if ((c.a == a && c.b == b) || (c.a == b && c.b == a)) return &c;
  }
  return nullptr;
}

Key keymapLookup(uint8_t a, uint8_t b) {
  const KeyContact* c = find(a, b);
  return c ? c->key : K_NONE;
}

uint8_t keymapContact(uint8_t a, uint8_t b) {
  const KeyContact* c = find(a, b);
  return c ? c->contact : 0;
}

const char* keyLineName(uint8_t line) {
  static const char* const NAMES[KEY_LINES] = {"C", "N", "H", "M", "L", "K", "A", "ON",
                                               "Q", "X5", "X4", "X3", "G", "F", "B", "X6"};
  return line < KEY_LINES ? NAMES[line] : "?";
}

const char* keyName(Key k) {
  static const char* const NAMES[] = {
      "-",   "0",    "1",    "2",   "3",    "4",    "5",     "6",     "7",
      "8",   "9",    ".",    "EXP", "Ans",  "+",    "-",     "x",     "/",
      "^",   "sqrt", "sin",  "cos", "tan",  "ln",   "log",   "(",     ")",
      "EXE", "DEL",  "AC",   "SHIFT", "ALPHA", "MODE", "UP",  "DOWN",  "LEFT",
      "RIGHT", "ON",
  };
  static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == K_COUNT, "Namensliste unvollstaendig");
  return k < K_COUNT ? NAMES[k] : "?";
}
