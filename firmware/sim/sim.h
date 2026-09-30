// Gemeinsamer Zustand des PC-Simulators (Terminal-UI, Log, Testbild).
#pragma once

#include <stdint.h>

namespace sim {

// Zaehlt bei jeder Aenderung ausserhalb des Screens hoch (Log, Eingabezeile).
extern uint32_t uiVersion;

// Letzte Zeilen der seriellen Ausgabe; n = 0 ist die neueste. nullptr, wenn keine.
const char* logLine(unsigned n);
void logAppend(const char* text, unsigned len);

// Zeile, die gerade als "serielle Eingabe" getippt wird (nullptr = keine).
const char* lineEdit();

// Datei, die die simulierte Kamera als Foto liefert (nullptr = keine).
extern const char* cameraImage;
// WAV-Datei, die das simulierte Mikrofon "aufnimmt" (nullptr = keine).
extern const char* micFile;

// "Tiefschlaf": blockiert bis zum naechsten Tastendruck (oder Strg-C).
void waitForWake();

}  // namespace sim
