#include "crc32.h"

namespace {

uint32_t table[256];
bool tableReady = false;

void buildTable() {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    table[i] = c;
  }
  tableReady = true;
}

}  // namespace

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  if (!tableReady) buildTable();
  uint32_t c = crc ^ 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}
