// Display-Backend des Simulators: zeichnet den Screen als Rahmen im Terminal,
// darunter Tastenhilfe und die serielle Ausgabe. Das Layout (Statuszeile invers,
// Inhalt, Eingabezeile mit Cursor) ist die Vorlage fuer den LT7680-Treiber.
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <string>

#include "../casio_deck/display.h"
#include "sim.h"

namespace {

const char* const INV = "\x1b[7m";
const char* const DIM = "\x1b[2m";
const char* const OFF = "\x1b[0m";
const char* const EOL = "\x1b[K\r\n";  // Rest der Zeile loeschen

bool isCont(unsigned char c) { return (c & 0xC0) == 0x80; }

size_t charCount(const char* s) {
  size_t n = 0;
  for (; *s; s++) n += !isCont(static_cast<unsigned char>(*s));
  return n;
}

// Byte-Offset nach `chars` Zeichen
const char* skipChars(const char* s, size_t chars) {
  while (*s && chars) {
    s++;
    while (isCont(static_cast<unsigned char>(*s))) s++;
    chars--;
  }
  return s;
}

// Text auf genau `width` Zeichen kuerzen bzw. mit Leerzeichen auffuellen
std::string fit(const char* text, size_t width) {
  size_t n = charCount(text);
  if (n > width) return std::string(text, skipChars(text, width) - text);
  return std::string(text) + std::string(width - n, ' ');
}

std::string repeat(const char* s, size_t n) {
  std::string out;
  for (size_t i = 0; i < n; i++) out += s;
  return out;
}

int terminalRows() {
  winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) return ws.ws_row;
  return 60;
}

// Eingabezeile: "> " + Ende der Eingabe + Cursor. Ein Zeichen, das noch per
// Mehrfachtippen wechseln kann, wird invers markiert statt eines Blockcursors.
std::string inputLine(const Screen& s) {
  const size_t width = Screen::COLS;
  const char* in = s.input();
  size_t avail = width - 3;
  size_t n = charCount(in);
  const char* start = n > avail ? skipChars(in, n - avail) : in;
  std::string out = "> ";
  size_t used = 2 + charCount(start);
  if (s.inputMarked() && *start) {
    size_t shown = charCount(start);
    const char* last = skipChars(start, shown - 1);
    out += std::string(start, last - start);
    out += INV;
    out += last;
    out += OFF;
  } else {
    out += start;
    out += INV;
    out += " ";
    out += OFF;
    used++;
  }
  if (used < width) out += std::string(width - used, ' ');
  return out;
}

const Screen* shown = nullptr;
uint32_t shownVersion = 0;
uint32_t shownUi = 0;

}  // namespace

namespace display {

void begin() {}

void render(const Screen& s) {
  if (&s == shown && s.version() == shownVersion && sim::uiVersion == shownUi) return;
  shown = &s;
  shownVersion = s.version();
  shownUi = sim::uiVersion;

  const size_t W = Screen::COLS;
  // Rahmen 2 + Status 1 + Eingabe 1 + Hilfe 2 + Log mind. 3 + Befehlszeile 1
  int rows = terminalRows();
  int viewRows = rows - 10;
  if (viewRows > Screen::VIEW_ROWS) viewRows = Screen::VIEW_ROWS;
  if (viewRows < 4) viewRows = 4;

  // Bei kleinem Terminal den unteren Teil des Inhalts zeigen
  int used = 0;
  for (int r = 0; r < Screen::VIEW_ROWS; r++) {
    if (s.viewLine(r)[0]) used = r + 1;
  }
  int first = used > viewRows ? used - viewRows : 0;

  std::string out = "\x1b[H";
  out += "┌" + repeat("─", W) + "┐" + EOL;
  out += "│" + std::string(INV) + fit(s.status(), W) + OFF + "│" + EOL;
  for (int r = first; r < first + viewRows; r++) {
    out += "│" + fit(s.viewLine(r), W) + "│" + EOL;
  }
  out += "│" + inputLine(s) + "│" + EOL;
  out += "└" + repeat("─", W) + "┘" + EOL;
  out += std::string(DIM) +
         "Tab MODE  Enter EXE  Esc AC  Bksp DEL  s SHIFT  a ALPHA  ↑↓ blaettern" + EOL +
         "x EXP  n Ans  w sqrt  i sin  o cos  t tan  l ln  g log  v Sprache  : Befehl  \" Text" +
         OFF + EOL;

  int logRows = rows - (viewRows + 7);
  if (logRows > 12) logRows = 12;
  for (int i = logRows - 1; i >= 0; i--) {
    const char* line = sim::logLine(i);
    out += std::string(DIM) + fit(line ? line : "", W + 20) + OFF + EOL;
  }
  if (const char* edit = sim::lineEdit()) {
    out += std::string("Seriell> ") + edit + INV + " " + OFF;
  }
  out += "\x1b[J";  // Rest des Bildschirms loeschen
  fwrite(out.data(), 1, out.size(), stdout);
  fflush(stdout);
}

}  // namespace display
