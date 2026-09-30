// Tests fuer die hardwareunabhaengigen Teile (Rechner, Screen) auf dem PC:
//   cd firmware/hosttest && make
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../casio_deck/calc.h"
#include "../casio_deck/config.h"
#include "../casio_deck/crc32.h"
#include "../casio_deck/keys.h"
#include "../casio_deck/multitap.h"
#include "../casio_deck/screen.h"
#include "../casio_deck/viewer.h"
#include "../casio_deck/wav.h"

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      printf("FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      failures++;                                                \
    }                                                            \
  } while (0)

static void checkCalc(const char* expr, double expected, double ans = 0, bool deg = true) {
  CalcResult r = calcEval(expr, ans, deg);
  if (!r.ok || fabs(r.value - expected) > 1e-9 * (1 + fabs(expected))) {
    printf("FEHLER calc '%s': ok=%d wert=%.12g erwartet=%.12g (%s)\n", expr, r.ok, r.value,
           expected, r.error ? r.error : "");
    failures++;
  }
}

static void checkCalcError(const char* expr) {
  CalcResult r = calcEval(expr, 0, true);
  if (r.ok) {
    printf("FEHLER calc '%s' sollte fehlschlagen, ergab %.12g\n", expr, r.value);
    failures++;
  }
}

static void testCalc() {
  checkCalc("3^2", 9);
  checkCalc("2^(-1)", 0.5);
  checkCalc("-3+5", 2);
  checkCalc("1+2*3", 7);
  checkCalc("(1+2)*3", 9);
  checkCalc("-2^2", -4);
  checkCalc("2^3^2", 512);
  checkCalc("2^-1", 0.5);
  checkCalc("10/4", 2.5);
  checkCalc("2E3+1", 2001);
  checkCalc(" 3 - - 2 ", 5);
  checkCalc("sqrt(16)", 4);
  checkCalc("sin(30)", 0.5);
  checkCalc("sin(180)", 0);
  checkCalc("sin(pi/2)", 1, 0, false);
  checkCalc("asin(1)", 90);
  checkCalc("log(1000)", 3);
  checkCalc("ln(e)", 1);
  checkCalc("Ans*2", 42, 21);
  checkCalcError("2pi");  // keine implizite Multiplikation
  checkCalcError("1/0");
  checkCalcError("sqrt(-1)");
  checkCalcError("(1+2");
  checkCalcError("1+");
  checkCalcError("foo(1)");
  checkCalcError("");

  char buf[32];
  calcFormat(1.0 / 3, buf, sizeof(buf));
  CHECK(!strcmp(buf, "0.3333333333"));
  calcFormat(-0.0, buf, sizeof(buf));
  CHECK(!strcmp(buf, "0"));
  calcFormat(1e20, buf, sizeof(buf));
  CHECK(!strcmp(buf, "1e+20"));
}

static void testScreen() {
  static Screen s;
  CHECK(s.totalLines() == 0);
  CHECK(!strcmp(s.viewLine(0), ""));

  s.print("hallo");
  CHECK(s.totalLines() == 1);
  CHECK(!strcmp(s.viewLine(0), "hallo"));

  // harter Umbruch bei COLS Zeichen
  char longText[Screen::COLS + 6];
  memset(longText, 'x', sizeof(longText) - 1);
  longText[sizeof(longText) - 1] = '\0';
  s.print(longText);
  CHECK(s.totalLines() == 3);
  CHECK(strlen(s.lineAt(1)) == Screen::COLS);
  CHECK(!strcmp(s.lineAt(2), "xxxxx"));

  // Umlaute zaehlen als ein Zeichen
  char umlauts[Screen::COLS * 2 + 1] = "";
  for (int i = 0; i < Screen::COLS; i++) strcat(umlauts, "\xC3\xA4");  // "ae"
  s.print(umlauts);
  CHECK(s.totalLines() == 4);
  CHECK(!strcmp(s.lineAt(3), umlauts));

  s.print("a\n\nb");
  CHECK(s.totalLines() == 7);
  CHECK(!strcmp(s.lineAt(5), ""));
  s.print("");
  CHECK(s.totalLines() == 8);

  // Eingabe mit UTF-8-Backspace
  s.inputAppend("ab\xC3\xB6");
  s.inputBackspace();
  CHECK(!strcmp(s.input(), "ab"));
  s.inputClear();
  CHECK(!strcmp(s.input(), ""));

  // Scrollback-Ueberlauf und Blaettern
  static Screen t;
  char buf[16];
  for (int i = 0; i < Screen::SCROLLBACK + 10; i++) {
    snprintf(buf, sizeof(buf), "%d", i);
    t.print(buf);
  }
  CHECK(t.lineAt(0) == nullptr);
  CHECK(!strcmp(t.lineAt(Screen::SCROLLBACK + 9), "129"));
  CHECK(!strcmp(t.viewLine(Screen::VIEW_ROWS - 1), "129"));
  t.scroll(5);
  CHECK(!strcmp(t.viewLine(Screen::VIEW_ROWS - 1), "124"));
  t.print("neu");  // Ausschnitt bleibt beim Zurueckblaettern stehen
  CHECK(!strcmp(t.viewLine(Screen::VIEW_ROWS - 1), "124"));
  t.scroll(-1000);
  CHECK(!strcmp(t.viewLine(Screen::VIEW_ROWS - 1), "neu"));
  t.scroll(1000);
  CHECK(!strcmp(t.viewLine(0), "11"));  // aeltester Eintrag im Puffer
}

// Tippt eine Tastenfolge mit Zeitabstaenden in einen Screen, wie app.cpp es tut.
static void tap(Screen& s, MultiTap& mt, Key k, uint32_t& now, uint32_t gap, bool upper = false) {
  now += gap;
  const char* text;
  bool replace;
  if (mt.feed(k, upper, now, text, replace)) {
    if (replace) s.inputBackspace();
    s.inputAppend(text);
  }
}

static void testMultiTap() {
  static Screen s;
  MultiTap mt;
  uint32_t now = 1000;
  // "hallo": 44 2 555 (Pause) 555 666
  tap(s, mt, K_4, now, 100);
  tap(s, mt, K_4, now, 200);
  tap(s, mt, K_2, now, 200);
  tap(s, mt, K_5, now, 200);
  tap(s, mt, K_5, now, 200);
  tap(s, mt, K_5, now, 200);
  tap(s, mt, K_5, now, MultiTap::TIMEOUT_MS + 1);  // gleiche Taste nach Pause = neues Zeichen
  tap(s, mt, K_5, now, 200);
  tap(s, mt, K_5, now, 200);
  tap(s, mt, K_6, now, 200);
  tap(s, mt, K_6, now, 200);
  tap(s, mt, K_6, now, 200);
  CHECK(!strcmp(s.input(), "hallo"));
  CHECK(mt.pending(now));
  CHECK(!mt.pending(now + MultiTap::TIMEOUT_MS));

  // Leerzeichen, Umlaut, Grossbuchstabe (gilt fuer die ganze Auswahl), Umlauf
  s.inputClear();
  mt.reset();
  tap(s, mt, K_8, now, 100, true);
  tap(s, mt, K_8, now, 100);
  tap(s, mt, K_8, now, 100);
  tap(s, mt, K_8, now, 100);  // T U V Ü
  CHECK(!strcmp(s.input(), "\xC3\x9C"));
  tap(s, mt, K_0, now, 100);
  tap(s, mt, K_7, now, 100);
  for (int i = 0; i < 6; i++) tap(s, mt, K_7, now, 100);  // p q r s ß 7 -> wieder p
  CHECK(!strcmp(s.input(), "\xC3\x9C p"));

  // Andere Taste beendet die Auswahl
  s.inputClear();
  tap(s, mt, K_2, now, 100);
  const char* text;
  bool replace;
  CHECK(!mt.feed(K_ADD, false, now, text, replace));
  tap(s, mt, K_2, now, 100);
  CHECK(!strcmp(s.input(), "aa"));
}

static void testWav() {
  uint8_t h[WAV_HEADER_BYTES];
  wavHeader(h, 32000, 16000);
  CHECK(!memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVEfmt ", 8) && !memcmp(h + 36, "data", 4));
  CHECK(h[4] == ((36 + 32000) & 0xFF) && h[5] == ((36 + 32000) >> 8));
  CHECK(h[24] == (16000 & 0xFF) && h[25] == (16000 >> 8));  // Abtastrate
  CHECK(h[22] == 1 && h[34] == 16);                         // mono, 16 Bit
  CHECK(h[40] == (32000 & 0xFF) && h[41] == (32000 >> 8));  // Datenlaenge

  // Gleichanteil weg, verstaerkt, begrenzt
  int16_t s[4] = {110, 90, 110, 90};
  wavAmplify(s, 4, 4);
  CHECK(s[0] == 40 && s[1] == -40);
  int16_t loud[2] = {20000, -20000};
  wavAmplify(loud, 2, 4);
  CHECK(loud[0] == 32767 && loud[1] == -32768);

  // Hochpass: driftender Gleichanteil verschwindet, 1 kHz bleibt
  static int16_t hp[16000];
  for (int i = 0; i < 16000; i++) {
    double drift = 6000.0 - 3000.0 * i / 16000;
    hp[i] = static_cast<int16_t>(drift + 1000.0 * sin(2 * 3.14159265 * 1000.0 * i / 16000));
  }
  wavHighpass(hp, 16000, 16000);
  double mean = 0, peak = 0;
  for (int i = 8000; i < 16000; i++) {
    mean += hp[i];
    if (fabs(hp[i]) > peak) peak = fabs(hp[i]);
  }
  mean /= 8000;
  CHECK(fabs(mean) < 50);
  CHECK(peak > 900 && peak < 1100);
}

static void testCrc32() {
  const char* s = "123456789";
  CHECK(crc32Update(0, reinterpret_cast<const uint8_t*>(s), 9) == 0xCBF43926u);
  uint32_t c = crc32Update(0, reinterpret_cast<const uint8_t*>(s), 4);
  CHECK(crc32Update(c, reinterpret_cast<const uint8_t*>(s) + 4, 5) == 0xCBF43926u);
  CHECK(crc32Update(0, nullptr, 0) == 0);
}

// Umbricht `text` und liefert die bereinigten Anzeigezeilen; byteweise gefuettert
// muss dasselbe herauskommen wie am Stueck.
static std::vector<std::string> layoutLines(const std::string& text, uint8_t cols, size_t maxBytes) {
  TextLayout whole(cols, maxBytes), bytes(cols, maxBytes);
  const uint8_t* d = reinterpret_cast<const uint8_t*>(text.data());
  whole.feed(d, text.size());
  whole.finish();
  for (size_t i = 0; i < text.size(); i++) bytes.feed(d + i, 1);
  bytes.finish();
  CHECK(whole.lines() == bytes.lines());
  std::vector<std::string> out;
  for (size_t i = 0; i < whole.lines(); i++) {
    CHECK(i >= bytes.lines() || whole.start(i) == bytes.start(i));
    char line[256];
    textLineClean(d + whole.start(i), whole.end(i) - whole.start(i), line, sizeof(line));
    out.push_back(line);
  }
  return out;
}

static bool sameLines(const std::vector<std::string>& got, std::vector<std::string> want) {
  if (got == want) return true;
  printf("  bekommen:");
  for (const auto& l : got) printf(" [%s]", l.c_str());
  printf("\n");
  return false;
}

static void testTextLayout() {
  CHECK(sameLines(layoutLines("hallo welt wie geht es", 10, 20), {"hallo welt", "wie geht", "es"}));
  CHECK(sameLines(layoutLines("abcdefghijklmno", 10, 20), {"abcdefghij", "klmno"}));
  CHECK(sameLines(layoutLines("a\n\nb\n", 10, 20), {"a", "", "b"}));
  CHECK(sameLines(layoutLines("ab\r\ncd", 10, 20), {"ab", "cd"}));
  CHECK(sameLines(layoutLines("", 10, 20), {""}));
  CHECK(sameLines(layoutLines("x\ty  ", 10, 20), {"x y"}));
  // UTF-8: nach Zeichen umbrechen, nie mitten im Zeichen; Byte-Grenze beachten
  CHECK(sameLines(layoutLines("ääääääääääää", 10, 20), {"ääääääääää", "ää"}));
  CHECK(sameLines(layoutLines("ääääääääääää", 10, 15), {"äääääää", "äääää"}));
  // langes Wort nach kurzem: Wort wandert in die naechste Zeile
  CHECK(sameLines(layoutLines("ab cdefghijkl", 10, 20), {"ab", "cdefghijkl"}));

  // Zeile auf Displaybreite, Standardwerte
  std::string longText;
  for (int i = 0; i < 100; i++) longText += "wort ";
  std::vector<std::string> lines = layoutLines(longText, Screen::COLS, Screen::LINE_BYTES - 1);
  // 100 x "wort ": je Zeile passen (COLS + 1) / 5 Woerter (bei 60 Spalten 12, bei 80 16)
  const size_t perLine = (Screen::COLS + 1) / 5;
  CHECK(lines.size() == (100 + perLine - 1) / perLine);
  for (const auto& l : lines) CHECK(l.size() <= Screen::COLS);

  char out[3];
  const uint8_t ae[] = {'a', 0xC3, 0xA4};
  textLineClean(ae, sizeof(ae), out, sizeof(out));
  CHECK(!strcmp(out, "a"));  // halbes 'ae' abgeschnitten
}

static void testJpegSize() {
  const uint8_t jpg[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x04, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x11,
                         0x08, 0x01, 0xE0, 0x02, 0x80, 0x03};
  uint16_t w = 0, h = 0;
  CHECK(jpegSize(jpg, sizeof(jpg), w, h) && w == 640 && h == 480);
  const uint8_t bad[] = {0x89, 'P', 'N', 'G'};
  CHECK(!jpegSize(bad, sizeof(bad), w, h));
  CHECK(!jpegSize(jpg, 8, w, h));  // abgeschnitten vor dem SOF
}

// Die gemessene Tastatur muss zur Scan-Logik in keypad.cpp passen.
static void testKeymap() {
  CHECK(KEY_CONTACT_COUNT == 50);
  bool seenContact[51] = {};
  bool seenKey[K_COUNT] = {};
  const uint16_t onlyOutput = (1u << 7) | (1u << 15);
  for (size_t i = 0; i < KEY_CONTACT_COUNT; i++) {
    const KeyContact& c = KEY_CONTACTS[i];
    CHECK(c.contact >= 1 && c.contact <= 50 && !seenContact[c.contact]);
    if (c.contact <= 50) seenContact[c.contact] = true;
    CHECK(c.a < KEY_LINES && c.b < KEY_LINES && c.a != c.b);
    for (size_t j = i + 1; j < KEY_CONTACT_COUNT; j++) {
      const KeyContact& d = KEY_CONTACTS[j];
      CHECK(!((c.a == d.a && c.b == d.b) || (c.a == d.b && c.b == d.a)));  // eindeutig
    }
    bool aDrive = KEY_DRIVE_MASK & (1u << c.a), bDrive = KEY_DRIVE_MASK & (1u << c.b);
    // Zwei Treiber an einer Taste: Kurzschluss HIGH gegen LOW im Scan
    CHECK(!(aDrive && bDrive));
    // Erkennbar: Treiber gegen Eingang, oder A (einzeln getrieben) gegen eine lesbare Leitung
    bool withA = c.a == KL_A || c.b == KL_A;
    uint8_t other = c.a == KL_A ? c.b : c.a;
    bool detect = (aDrive != bDrive) || (withA && !(onlyOutput & (1u << other)));
    if (!detect) printf("FEHLER Kontakt %u nicht erkennbar\n", c.contact);
    CHECK(detect);
    CHECK(keymapLookup(c.b, c.a) == c.key && keymapContact(c.a, c.b) == c.contact);
    if (c.key != K_NONE) {
      CHECK(!seenKey[c.key]);
      seenKey[c.key] = true;
    }
  }
  CHECK(keymapLookup(KL_A, KL_Q) == K_NONE && keymapContact(KL_A, KL_Q) == 0);
  CHECK(keymapLookup(KL_A, KL_B) == K_EXE && keymapLookup(KL_N, KL_A) == K_DOT);
  CHECK(!strcmp(keyLineName(KL_X6), "X6"));
}

int main() {
  testKeymap();
  testCalc();
  testScreen();
  testMultiTap();
  testWav();
  testCrc32();
  testTextLayout();
  testJpegSize();
  if (failures) {
    printf("%d Fehler\n", failures);
    return 1;
  }
  printf("alle Tests ok\n");
  return 0;
}
