// Textmodell eines Bildschirms: Statuszeile, Scrollback, Eingabezeile.
// Unabhaengig von der Hardware (auch auf dem PC testbar); ein Display-Backend
// zeichnet es (vorerst nur serieller Monitor, spaeter LT7680).
//
//   Zeile 0              Statuszeile
//   Zeile 1..ROWS-2      Inhalt (VIEW_ROWS Zeilen aus dem Scrollback)
//   Zeile ROWS-1         Eingabezeile
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"

class Screen {
 public:
  static constexpr uint8_t COLS = SCREEN_COLS;
  static constexpr uint8_t VIEW_ROWS = SCREEN_ROWS - 2;
  static constexpr uint16_t SCROLLBACK = 120;
  // UTF-8: ein Zeichen braucht 1-4 Byte. Reicht fuer Umlaute; passt eine Zeile
  // nicht in den Puffer, wird frueher umgebrochen.
  static constexpr size_t LINE_BYTES = COLS * 2 + 1;
  static constexpr size_t INPUT_BYTES = 256;

  Screen();

  // Text umbrechen und anhaengen. Jedes '\n' beginnt eine neue Zeile,
  // print("") haengt eine Leerzeile an.
  void print(const char* text);
  void clear();

  // Weiter zurueck (delta > 0) oder vor (delta < 0) blaettern.
  void scroll(int delta);
  uint16_t scrollOffset() const { return scroll_; }

  void setStatus(const char* text);
  const char* status() const { return status_; }

  const char* input() const { return input_; }
  void inputAppend(const char* text);
  void inputBackspace();  // entfernt das letzte UTF-8-Zeichen
  void inputClear();

  // Anzahl je angehaengter Zeilen (zaehlt weiter, wenn der Puffer ueberlaeuft).
  uint32_t totalLines() const { return total_; }
  // Zeile mit absolutem Index n oder nullptr, wenn nicht (mehr) im Puffer.
  const char* lineAt(uint32_t n) const;
  // Sichtbare Inhaltszeile r (0 = oben), leerer String, wenn dort nichts steht.
  const char* viewLine(uint8_t r) const;

  // Aendert sich bei jeder Aenderung; fuer Display-Backends.
  uint32_t version() const { return version_; }

 private:
  char* newLine();
  uint16_t available() const;

  char lines_[SCROLLBACK][LINE_BYTES];
  uint32_t total_ = 0;
  uint16_t scroll_ = 0;
  char status_[LINE_BYTES] = "";
  char input_[INPUT_BYTES] = "";
  uint32_t version_ = 0;
};

// Byte-Laenge des UTF-8-Zeichens, das mit `lead` beginnt (1 bei ungueltigen Bytes).
size_t utf8CharLen(uint8_t lead);
