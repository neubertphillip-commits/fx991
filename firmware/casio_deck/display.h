// Display-Backend. Solange das LT7680-Panel fehlt, wird der Bildschirm als
// Protokoll auf dem seriellen Monitor ausgegeben (neue Zeilen, Status, Eingabe).
// TODO (CLAUDE.md, Offen 3): LT7680-Treiber, zeichnet Screen zeilenweise per SPI.
#pragma once

#include "screen.h"

namespace display {

void begin();

// Zeichnet `s`, falls es sich seit dem letzten Aufruf geaendert hat oder ein
// anderer Screen aktiv geworden ist.
void render(const Screen& s);

// JPEG ganzflaechig zeigen (Datei-Viewer), bis zum naechsten render(). false, wenn
// das Backend keine Bilder kann (serieller Monitor, Simulator).
bool showJpeg(const uint8_t* data, size_t len);

// Anzeige (und spaeter Hintergrundbeleuchtung) aus- bzw. wieder einschalten.
void power(bool on);

// Serielles Backend: statt des Zeilenprotokolls bei jeder Aenderung den ganzen
// Bildschirm als Block senden (fuer tools/deckterm.py, Laptop als Display).
// Block: "\x02F\n" Status "\n" VIEW_ROWS Zeilen "\n" Eingabe "\n" 0/1 (markiert) "\n\x03\n".
// Ausgeschaltet: "\x02OFF\n\x03\n". Andere Backends ignorieren es.
void setFrames(bool on);

}  // namespace display
