// PDM-Mikrofon der Sense-Platine. Nimmt in den PSRAM auf und liefert eine WAV-Datei
// (16 kHz, 16 Bit, mono), die die Bridge in Text umwandelt.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mic {

bool start();       // Aufnahme beginnen; false bei Fehler (siehe error())
void stop();        // Aufnahme beenden, WAV bereitstellen
void cancel();      // Aufnahme verwerfen
bool recording();   // false, sobald gestoppt oder der Puffer voll ist
uint32_t elapsedMs();

// Nach stop(): fertige WAV-Datei, gueltig bis release() oder zum naechsten start().
bool wav(const uint8_t*& data, size_t& len);
void release();

const char* error();

}  // namespace mic
