// Zustandsautomat: Rechnermodus / Terminal / Kamera.
#pragma once

#include "keys.h"

namespace app {

void begin();
void loop();

// Taste so verarbeiten, als waere sie auf der Tastatur gedrueckt worden
// (serieller Befehl ":key", PC-Simulator).
void injectKey(Key k);

}  // namespace app
