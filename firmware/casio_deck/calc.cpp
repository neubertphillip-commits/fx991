#include "calc.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct Parser {
  const char* p;
  double ans;
  bool degrees;
  const char* error;

  void fail(const char* msg) {
    if (!error) error = msg;
  }

  void skipSpace() {
    while (*p == ' ') p++;
  }

  bool accept(char c) {
    skipSpace();
    if (*p != c) return false;
    p++;
    return true;
  }

  bool acceptWord(const char* w) {
    skipSpace();
    size_t n = strlen(w);
    if (strncasecmp(p, w, n) != 0 || isalpha(static_cast<unsigned char>(p[n]))) return false;
    p += n;
    return true;
  }

  double expr() {
    double v = term();
    while (!error) {
      if (accept('+')) v += term();
      else if (accept('-')) v -= term();
      else break;
    }
    return v;
  }

  double term() {
    double v = factor();
    while (!error) {
      if (accept('*')) {
        v *= factor();
      } else if (accept('/')) {
        double d = factor();
        if (d == 0) fail("Division durch 0");
        v /= d;
      } else {
        break;
      }
    }
    return v;
  }

  double factor() {
    if (accept('-')) return -factor();
    if (accept('+')) return factor();
    return power();
  }

  double power() {
    double base = primary();
    if (!error && accept('^')) return pow(base, factor());
    return base;
  }

  double angleIn(double x) { return degrees ? x * M_PI / 180.0 : x; }
  // sin(180) usw. liefern sonst 1.2e-16 statt 0
  static double snap(double x) { return fabs(x) < 1e-15 ? 0 : x; }
  double angleOut(double x) { return degrees ? x * 180.0 / M_PI : x; }

  double call(const char* name) {
    if (!accept('(')) {
      fail("( erwartet");
      return 0;
    }
    double x = expr();
    if (!accept(')')) fail(") fehlt");
    if (error) return 0;
    if (!strcmp(name, "sqrt")) {
      if (x < 0) fail("Mathe-Fehler");
      return sqrt(x);
    }
    if (!strcmp(name, "sin")) return snap(sin(angleIn(x)));
    if (!strcmp(name, "cos")) return snap(cos(angleIn(x)));
    if (!strcmp(name, "tan")) return snap(tan(angleIn(x)));
    if (!strcmp(name, "asin")) return angleOut(asin(x));
    if (!strcmp(name, "acos")) return angleOut(acos(x));
    if (!strcmp(name, "atan")) return angleOut(atan(x));
    if (!strcmp(name, "ln")) return log(x);
    if (!strcmp(name, "log")) return log10(x);
    if (!strcmp(name, "exp")) return exp(x);
    if (!strcmp(name, "abs")) return fabs(x);
    fail("unbekannte Funktion");
    return 0;
  }

  double primary() {
    skipSpace();
    if (accept('(')) {
      double v = expr();
      if (!accept(')')) fail(") fehlt");
      return v;
    }
    if (isdigit(static_cast<unsigned char>(*p)) || *p == '.') {
      char* end;
      double v = strtod(p, &end);
      if (end == p) fail("Syntax-Fehler");
      p = end;
      return v;
    }
    static const char* const FUNCS[] = {"sqrt", "sin",  "cos", "tan", "asin", "acos",
                                        "atan", "ln",   "log", "exp", "abs"};
    for (const char* f : FUNCS) {
      if (acceptWord(f)) return call(f);
    }
    if (acceptWord("pi")) return M_PI;
    if (acceptWord("ans")) return ans;
    if (acceptWord("e")) return M_E;
    fail("Syntax-Fehler");
    return 0;
  }
};

}  // namespace

CalcResult calcEval(const char* expr, double ans, bool degrees) {
  Parser ps{expr, ans, degrees, nullptr};
  double v = ps.expr();
  ps.skipSpace();
  if (!ps.error && *ps.p != '\0') ps.fail("Syntax-Fehler");
  if (!ps.error && (isnan(v) || isinf(v))) ps.fail("Mathe-Fehler");
  if (ps.error) return {false, 0, ps.error};
  return {true, v, nullptr};
}

void calcFormat(double value, char* out, unsigned size) {
  if (value == 0) value = 0;  // -0 vermeiden
  snprintf(out, size, "%.10g", value);
}
