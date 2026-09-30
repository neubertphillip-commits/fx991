// Texteingabe per Mehrfachtippen wie am alten Handy:
//   1 . , ? ! : - ' 1     2 a b c ä 2    3 d e f 3
//   4 g h i 4             5 j k l 5      6 m n o ö 6
//   7 p q r s ß 7         8 t u v ü 8    9 w x y z 9
//   0 Leerzeichen 0
// Dieselbe Taste innerhalb von TIMEOUT_MS erneut -> naechstes Zeichen ersetzt das letzte.
#pragma once

#include <stdint.h>

#include "keys.h"

class MultiTap {
 public:
  static constexpr uint32_t TIMEOUT_MS = 1000;

  // Verarbeitet Taste `k`. Ist es eine Buchstabentaste, liefert die Funktion true,
  // `text` ist das einzufuegende Zeichen (UTF-8) und `replace` sagt, ob es das
  // zuletzt eingegebene Zeichen ersetzt. Andere Tasten beenden die Auswahl.
  bool feed(Key k, bool upper, uint32_t now, const char*& text, bool& replace);

  // Auswahl beenden (z.B. nach DEL, AC, EXE, Moduswechsel).
  void reset() { last_ = K_NONE; }

  // true, solange das letzte Zeichen noch durch erneutes Tippen wechseln kann.
  bool pending(uint32_t now) const { return last_ != K_NONE && now - time_ < TIMEOUT_MS; }

 private:
  Key last_ = K_NONE;
  bool upper_ = false;
  uint8_t index_ = 0;
  uint32_t time_ = 0;
};
