// OV3660 auf der Sense-Platine. Nur an, solange der Kameramodus aktiv ist.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace camera {

bool begin();  // Kamera einschalten; false bei Fehler (siehe error())
void end();    // abschalten, Framebuffer freigeben
bool isOn();

// Nimmt ein JPEG auf. Der Puffer gilt bis release().
bool capture(const uint8_t*& jpeg, size_t& len);
void release();

const char* error();

}  // namespace camera
