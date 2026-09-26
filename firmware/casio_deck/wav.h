// WAV-Hilfen fuer die Spracheingabe (16 Bit, mono). Hardwareunabhaengig.
#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t WAV_HEADER_BYTES = 44;

// Schreibt einen 44-Byte-WAV-Header fuer `dataBytes` PCM-Daten (16 Bit, mono).
void wavHeader(uint8_t* out, uint32_t dataBytes, uint32_t sampleRate);

// Verstaerkt die Samples um `gain` (mit Begrenzung) und entfernt den Gleichanteil.
void wavAmplify(int16_t* samples, size_t count, int gain);
