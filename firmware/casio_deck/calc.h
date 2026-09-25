// Einfacher Ausdrucksauswerter fuer den Rechnermodus.
//
// Grammatik (^ rechtsassoziativ, bindet staerker als Vorzeichen: -2^2 = -4):
//   expr    = term { ("+" | "-") term }
//   term    = factor { ("*" | "/") factor }
//   factor  = ("-" | "+") factor | power
//   power   = primary [ "^" factor ]
//   primary = Zahl | "(" expr ")" | Funktion "(" expr ")" | "pi" | "e" | "Ans"
// Funktionen: sqrt sin cos tan asin acos atan ln log exp abs
#pragma once

struct CalcResult {
  bool ok;
  double value;
  const char* error;  // bei ok == false
};

// `ans` ist der Wert fuer "Ans", `degrees` schaltet Winkelfunktionen auf Grad.
CalcResult calcEval(const char* expr, double ans, bool degrees);

// Ergebnis wie am Taschenrechner formatieren (max. 10 signifikante Stellen).
void calcFormat(double value, char* out, unsigned size);
