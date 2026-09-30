#include "viewer.h"

TextLayout::TextLayout(uint8_t cols, size_t maxBytes) : cols_(cols), maxBytes_(maxBytes) {
  reset();
}

void TextLayout::reset() {
  starts_.clear();
  starts_.push_back(0);
  size_ = 0;
  lineCols_ = lineBytes_ = 0;
  canBreak_ = false;
}

void TextLayout::newLine(uint32_t at) {
  starts_.push_back(at);
  lineCols_ = lineBytes_ = 0;
  canBreak_ = false;
}

void TextLayout::feed(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i];
    uint32_t pos = size_++;
    if (b == '\n') {
      newLine(pos + 1);
      continue;
    }
    if ((b & 0xC0) == 0x80 || b == '\r') {  // Folgebyte eines UTF-8-Zeichens, CR
      lineBytes_++;
      continue;
    }
    size_t clen = utf8CharLen(b);
    if (lineCols_ + 1 > cols_ || lineBytes_ + clen > maxBytes_) {
      if (b == ' ') {  // Leerzeichen genau am Umbruch: faellt weg
        newLine(pos + 1);
        continue;
      }
      if (canBreak_ && breakAt_ > starts_.back()) {
        // am letzten Leerzeichen umbrechen, das angefangene Wort wandert mit
        uint32_t cols = lineCols_ - colsAtBreak_;
        uint32_t bytes = lineBytes_ - bytesAtBreak_;
        newLine(breakAt_);
        lineCols_ = cols;
        lineBytes_ = bytes;
      }
      if (lineCols_ + 1 > cols_ || lineBytes_ + clen > maxBytes_) newLine(pos);  // hart
    }
    lineCols_++;
    lineBytes_++;
    if (b == ' ') {
      canBreak_ = true;
      breakAt_ = pos + 1;
      colsAtBreak_ = lineCols_;
      bytesAtBreak_ = lineBytes_;
    }
  }
}

void TextLayout::finish() {
  // Zeilenumbruch am Dateiende ergibt keine eigene (leere) Zeile
  if (starts_.size() > 1 && starts_.back() == size_) starts_.pop_back();
}

void textLineClean(const uint8_t* in, size_t len, char* out, size_t cap) {
  if (cap == 0) return;
  size_t o = 0;
  size_t i = 0;
  for (; i < len && o + 1 < cap; i++) {
    uint8_t c = in[i];
    if (c == '\t') c = ' ';
    else if (c < 0x20 || c == 0x7F) continue;
    out[o++] = static_cast<char>(c);
  }
  if (i < len) {
    // abgeschnitten: kein halbes UTF-8-Zeichen stehen lassen
    size_t lead = o;
    while (lead > 0 && (static_cast<uint8_t>(out[lead - 1]) & 0xC0) == 0x80) lead--;
    if (lead > 0 && static_cast<uint8_t>(out[lead - 1]) >= 0xC0 &&
        lead - 1 + utf8CharLen(static_cast<uint8_t>(out[lead - 1])) > o) {
      o = lead - 1;
    }
  }
  while (o > 0 && out[o - 1] == ' ') o--;
  out[o] = '\0';
}

bool jpegSize(const uint8_t* d, size_t len, uint16_t& width, uint16_t& height) {
  if (len < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;
  size_t i = 2;
  while (i + 4 <= len) {
    if (d[i] != 0xFF) return false;
    uint8_t m = d[i + 1];
    if (m == 0xFF) {  // Fuellbyte
      i++;
      continue;
    }
    if (m == 0x01 || (m >= 0xD0 && m <= 0xD8)) {  // Marker ohne Laenge
      i += 2;
      continue;
    }
    size_t seg = (static_cast<size_t>(d[i + 2]) << 8) | d[i + 3];
    bool sof = m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC;
    if (sof) {
      if (i + 9 > len) return false;
      height = static_cast<uint16_t>((d[i + 5] << 8) | d[i + 6]);
      width = static_cast<uint16_t>((d[i + 7] << 8) | d[i + 8]);
      return true;
    }
    if (m == 0xDA || seg < 2) return false;  // Bilddaten ohne SOF davor
    i += 2 + seg;
  }
  return false;
}
