#include <Arduino.h>
#include <string.h>

#include "display.h"

namespace {

const Screen* shown = nullptr;
uint32_t shownVersion = 0;
uint32_t printedTotal = 0;
char lastStatus[Screen::LINE_BYTES];
char lastInput[Screen::INPUT_BYTES];

bool frames = false;  // ganze Bildschirme statt Zeilenprotokoll (setFrames)

constexpr uint8_t CONTEXT_LINES = 8;  // beim Moduswechsel so viele alte Zeilen zeigen

void printLines(const Screen& s, uint32_t from) {
  for (uint32_t n = from; n < s.totalLines(); n++) {
    const char* line = s.lineAt(n);
    if (line) Serial.printf("  | %s\n", line);
  }
  printedTotal = s.totalLines();
}

}  // namespace

namespace display {

void begin() {
  // Ohne angeschlossenen Monitor nicht auf USB warten
  Serial.setTxTimeoutMs(0);
}

bool showJpeg(const uint8_t*, size_t) { return false; }

void power(bool on) {
  Serial.println(on ? "[display] an" : "[display] aus");
  if (!on && frames) Serial.print("\x02OFF\n\x03\n");
  shown = nullptr;  // nach dem Einschalten alles neu ausgeben
}

void setFrames(bool on) {
  frames = on;
  // Ganze Bloecke duerfen nicht verloren gehen: kurz warten statt verwerfen,
  // solange jemand zuhoert (ohne Monitor wieder sofort weiter).
  Serial.setTxTimeoutMs(on ? 100 : 0);
  shown = nullptr;
}

void render(const Screen& s) {
  if (&s == shown && s.version() == shownVersion) return;

  if (frames) {
    shown = &s;
    shownVersion = s.version();
    Serial.printf("\x02" "F %u %u\n%s\n", Screen::COLS, Screen::VIEW_ROWS, s.status());
    for (uint8_t r = 0; r < Screen::VIEW_ROWS; r++) Serial.printf("%s\n", s.viewLine(r));
    Serial.printf("%s\n%d\n\x03\n", s.input(), s.inputMarked() ? 1 : 0);
    return;
  }

  if (&s != shown) {
    shown = &s;
    Serial.printf("\n==== %s ====\n", s.status());
    uint32_t total = s.totalLines();
    printLines(s, total > CONTEXT_LINES ? total - CONTEXT_LINES : 0);
    strcpy(lastStatus, s.status());
    lastInput[0] = '\0';
  }

  if (strcmp(lastStatus, s.status()) != 0) {
    Serial.printf("== %s ==\n", s.status());
    strcpy(lastStatus, s.status());
  }
  if (s.totalLines() < printedTotal) {
    Serial.println("  ---- geloescht ----");
    printedTotal = 0;
  }
  if (s.totalLines() > printedTotal) {
    uint32_t total = s.totalLines();
    uint32_t oldest = total > Screen::SCROLLBACK ? total - Screen::SCROLLBACK : 0;
    printLines(s, printedTotal > oldest ? printedTotal : oldest);
  }
  if (strcmp(lastInput, s.input()) != 0) {
    Serial.printf("> %s\n", s.input());
    strcpy(lastInput, s.input());
  }
  shownVersion = s.version();
}

}  // namespace display
