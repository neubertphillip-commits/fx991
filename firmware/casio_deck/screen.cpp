#include "screen.h"

#include <string.h>

size_t utf8CharLen(uint8_t lead) {
  if (lead < 0x80) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;  // Folgebyte oder ungueltig: einzeln uebernehmen
}

// Laenge des naechsten Zeichens, gekuerzt, falls der String mittendrin endet.
static size_t nextCharLen(const char* s) {
  size_t n = utf8CharLen(static_cast<uint8_t>(*s));
  for (size_t i = 1; i < n; i++) {
    if (s[i] == '\0') return i;
  }
  return n;
}

Screen::Screen() { memset(lines_, 0, sizeof(lines_)); }

char* Screen::newLine() {
  char* line = lines_[total_ % SCROLLBACK];
  line[0] = '\0';
  total_++;
  // Beim Zurueckblaettern bleibt der sichtbare Ausschnitt stehen.
  if (scroll_ > 0 && available() > VIEW_ROWS && scroll_ < available() - VIEW_ROWS) scroll_++;
  return line;
}

uint16_t Screen::available() const {
  return total_ < SCROLLBACK ? static_cast<uint16_t>(total_) : SCROLLBACK;
}

void Screen::print(const char* text) {
  char* cur = newLine();
  size_t bytes = 0;
  uint8_t chars = 0;
  while (*text) {
    if (*text == '\n') {
      cur = newLine();
      bytes = chars = 0;
      text++;
      continue;
    }
    size_t n = nextCharLen(text);
    if (chars == COLS || bytes + n > LINE_BYTES - 1) {
      cur = newLine();
      bytes = chars = 0;
    }
    memcpy(cur + bytes, text, n);
    bytes += n;
    cur[bytes] = '\0';
    chars++;
    text += n;
  }
  version_++;
}

void Screen::clear() {
  total_ = 0;
  scroll_ = 0;
  version_++;
}

void Screen::scroll(int delta) {
  int maxOff = available() > VIEW_ROWS ? available() - VIEW_ROWS : 0;
  int off = scroll_ + delta;
  if (off < 0) off = 0;
  if (off > maxOff) off = maxOff;
  if (off != scroll_) {
    scroll_ = static_cast<uint16_t>(off);
    version_++;
  }
}

void Screen::setStatus(const char* text) {
  if (strncmp(status_, text, sizeof(status_)) == 0) return;
  strncpy(status_, text, sizeof(status_) - 1);
  status_[sizeof(status_) - 1] = '\0';
  version_++;
}

void Screen::inputAppend(const char* text) {
  size_t len = strlen(input_);
  while (*text) {
    size_t n = nextCharLen(text);
    if (len + n >= INPUT_BYTES) break;
    memcpy(input_ + len, text, n);
    len += n;
    text += n;
  }
  input_[len] = '\0';
  version_++;
}

void Screen::inputBackspace() {
  size_t len = strlen(input_);
  if (len == 0) return;
  // Folgebytes (10xxxxxx) mit entfernen
  do {
    len--;
  } while (len > 0 && (static_cast<uint8_t>(input_[len]) & 0xC0) == 0x80);
  input_[len] = '\0';
  version_++;
}

void Screen::inputClear() {
  if (input_[0] == '\0') return;
  input_[0] = '\0';
  version_++;
}

void Screen::setInputMarked(bool on) {
  if (on == marked_) return;
  marked_ = on;
  version_++;
}

const char* Screen::lineAt(uint32_t n) const {
  if (n >= total_ || total_ - n > SCROLLBACK) return nullptr;
  return lines_[n % SCROLLBACK];
}

const char* Screen::viewLine(uint8_t r) const {
  if (r >= VIEW_ROWS) return "";
  uint32_t first = available() <= VIEW_ROWS ? total_ - available() : total_ - VIEW_ROWS - scroll_;
  const char* line = lineAt(first + r);
  return line ? line : "";
}
