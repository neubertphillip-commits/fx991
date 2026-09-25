// Tests fuer die hardwareunabhaengigen Teile (Rechner, Screen) auf dem PC:
//   cd firmware/hosttest && make
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../casio_deck/calc.h"
#include "../casio_deck/screen.h"

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

int main() {
  testCalc();
  testScreen();
  if (failures) {
    printf("%d Fehler\n", failures);
    return 1;
  }
  printf("alle Tests ok\n");
  return 0;
}
