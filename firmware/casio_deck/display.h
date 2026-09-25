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

}  // namespace display
