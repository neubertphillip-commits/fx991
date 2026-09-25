#include <Arduino.h>
#include <string.h>

#include "display.h"

namespace {

const Screen* shown = nullptr;
uint32_t shownVersion = 0;
uint32_t printedTotal = 0;
char lastStatus[Screen::LINE_BYTES];
char lastInput[Screen::INPUT_BYTES];

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

void render(const Screen& s) {
  if (&s == shown && s.version() == shownVersion) return;

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
