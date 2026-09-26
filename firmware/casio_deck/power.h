// Ein/Aus ohne Schalter, Watchdog und Absicherung von Firmware-Updates.
//
// Aus = Tiefschlaf. Der MCP23017 bleibt versorgt; ein Tastendruck zieht INTA auf LOW
// und weckt den ESP32 (Neustart, Weckgrund wird erkannt).
//
// Updates: Eine neue Firmware startet als "unbestaetigt". Erst wenn sie wieder
// Updates per WLAN annehmen kann, wird sie bestaetigt. Stuerzt sie vorher ab oder
// startet neu, laedt der Bootloader automatisch die vorige Firmware.
#pragma once

#include <stdint.h>

namespace power {

// Frueh in setup(): Watchdog einrichten, Weckgrund und Update-Zustand lesen.
void begin();

// true, wenn der Start ein Aufwachen per Tastendruck war (nicht Kaltstart).
bool wokeByKey();

// Neue Firmware noch nicht bestaetigt; waehrenddessen nicht ausschalten.
bool updatePending();
void confirmUpdate();

// Tiefschlaf bis zum naechsten Tastendruck. Kehrt auf der Hardware nicht zurueck
// (Aufwachen = Neustart); im PC-Simulator kehrt es nach einem Tastendruck zurueck.
void sleep();

}  // namespace power
