// Hardwareunabhaengige Teile des Datei-Viewers (auf dem PC testbar).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "screen.h"

// Bricht einen Text beliebiger Laenge fuer die Anzeige um, ohne ihn im Speicher zu
// halten: Die Datei wird stueckweise hineingereicht, gemerkt werden nur die
// Startpositionen der Anzeigezeilen (4 Byte je Zeile). Umbruch am letzten
// Leerzeichen, sonst hart; UTF-8-Zeichen werden nie zerteilt.
class TextLayout {
 public:
  explicit TextLayout(uint8_t cols = Screen::COLS, size_t maxBytes = Screen::LINE_BYTES - 1);

  void reset();
  void feed(const uint8_t* data, size_t len);
  void finish();  // nach dem letzten feed()

  size_t lines() const { return starts_.size(); }
  // Byte-Bereich [start, end) der Zeile i; enthaelt ggf. noch '\n', '\r' oder
  // das Leerzeichen am Umbruch (textLineClean entfernt sie).
  uint32_t start(size_t i) const { return starts_[i]; }
  uint32_t end(size_t i) const { return i + 1 < starts_.size() ? starts_[i + 1] : size_; }

 private:
  void newLine(uint32_t at);

  uint8_t cols_;
  size_t maxBytes_;
  std::vector<uint32_t> starts_;
  uint32_t size_ = 0;       // bisher gelesene Bytes
  uint32_t lineCols_ = 0;   // Zeichen in der aktuellen Zeile
  uint32_t lineBytes_ = 0;  // Bytes in der aktuellen Zeile
  // Letzte Stelle hinter einem Leerzeichen, an der umgebrochen werden kann
  bool canBreak_ = false;
  uint32_t breakAt_ = 0;
  uint32_t colsAtBreak_ = 0;
  uint32_t bytesAtBreak_ = 0;
};

// Rohbytes einer Anzeigezeile -> druckbarer Text: Tabs werden Leerzeichen,
// Zeilenenden, Steuerzeichen und Leerzeichen am Ende fallen weg.
void textLineClean(const uint8_t* in, size_t len, char* out, size_t cap);

// Breite und Hoehe aus dem JPEG-Header (SOF-Marker); false, wenn keiner gefunden wird.
bool jpegSize(const uint8_t* data, size_t len, uint16_t& width, uint16_t& height);
