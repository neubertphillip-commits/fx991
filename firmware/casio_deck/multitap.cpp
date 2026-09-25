#include "multitap.h"

namespace {

// nullptr-terminierte Zeichenlisten je Taste (klein / gross)
const char* const L1[] = {".", ",", "?", "!", ":", "-", "'", "1", nullptr};
const char* const L2[] = {"a", "b", "c", "\xC3\xA4", "2", nullptr};
const char* const U2[] = {"A", "B", "C", "\xC3\x84", "2", nullptr};
const char* const L3[] = {"d", "e", "f", "3", nullptr};
const char* const U3[] = {"D", "E", "F", "3", nullptr};
const char* const L4[] = {"g", "h", "i", "4", nullptr};
const char* const U4[] = {"G", "H", "I", "4", nullptr};
const char* const L5[] = {"j", "k", "l", "5", nullptr};
const char* const U5[] = {"J", "K", "L", "5", nullptr};
const char* const L6[] = {"m", "n", "o", "\xC3\xB6", "6", nullptr};
const char* const U6[] = {"M", "N", "O", "\xC3\x96", "6", nullptr};
const char* const L7[] = {"p", "q", "r", "s", "\xC3\x9F", "7", nullptr};
const char* const U7[] = {"P", "Q", "R", "S", "\xC3\x9F", "7", nullptr};
const char* const L8[] = {"t", "u", "v", "\xC3\xBC", "8", nullptr};
const char* const U8[] = {"T", "U", "V", "\xC3\x9C", "8", nullptr};
const char* const L9[] = {"w", "x", "y", "z", "9", nullptr};
const char* const U9[] = {"W", "X", "Y", "Z", "9", nullptr};
const char* const L0[] = {" ", "0", nullptr};

const char* const* charsFor(Key k, bool upper) {
  switch (k) {
    case K_0: return L0;
    case K_1: return L1;
    case K_2: return upper ? U2 : L2;
    case K_3: return upper ? U3 : L3;
    case K_4: return upper ? U4 : L4;
    case K_5: return upper ? U5 : L5;
    case K_6: return upper ? U6 : L6;
    case K_7: return upper ? U7 : L7;
    case K_8: return upper ? U8 : L8;
    case K_9: return upper ? U9 : L9;
    default: return nullptr;
  }
}

}  // namespace

bool MultiTap::feed(Key k, bool upper, uint32_t now, const char*& text, bool& replace) {
  replace = k == last_ && now - time_ < TIMEOUT_MS;
  if (replace) upper = upper_;  // Gross/klein gilt fuer die ganze Auswahl

  const char* const* chars = charsFor(k, upper);
  if (!chars) {
    reset();
    return false;
  }
  if (replace) {
    index_++;
    if (!chars[index_]) index_ = 0;
  } else {
    index_ = 0;
  }
  last_ = k;
  upper_ = upper;
  time_ = now;
  text = chars[index_];
  return true;
}
