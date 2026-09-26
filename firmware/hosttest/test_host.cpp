// Tests fuer die hardwareunabhaengigen Teile (Rechner, Screen) auf dem PC:
//   cd firmware/hosttest && make
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../casio_deck/calc.h"
#include "../casio_deck/multitap.h"
#include "../casio_deck/screen.h"
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
}

int main() {
  testCalc();
  testScreen();
  testMultiTap();
  testWav();
  if (failures) {
    printf("%d Fehler\n", failures);
    return 1;
  }
  printf("alle Tests ok\n");
  return 0;
}
