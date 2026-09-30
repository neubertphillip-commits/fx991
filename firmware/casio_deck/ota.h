// Firmware-Update per WLAN (ArduinoOTA). Laeuft automatisch, solange das WLAN
// verbunden ist, z.B. im Terminalmodus. Hochladen: siehe firmware/README.md.
#pragma once

namespace ota {

// Meldet Beginn, Ende und Fehler eines Updates (fuer die Anzeige).
using Notify = void (*)(const char* message);

void begin(Notify notify);
void loop();

bool ready();    // wartet auf ein Update (WLAN verbunden)
bool running();  // Update wird gerade uebertragen

}  // namespace ota
