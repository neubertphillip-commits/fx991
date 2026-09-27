// CRC-32 (wie zlib/Python zlib.crc32), zum Abgleich der Dateien mit der Bridge.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Fortsetzbar: crc32Update(crc32Update(0, a, n), b, m) == CRC ueber a und b.
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len);
