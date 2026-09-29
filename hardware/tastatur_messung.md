# Tastaturmatrix fx-991DE CW: Messprotokoll

Kontaktnummern siehe `tastatur_nummern.jpg` (Platine von der Kammseite, LCD-Anschluss oben).
Gemessen im 2k-Bereich auf den hellgrauen Kohlekontakten (die schwarzen Bahnen sind lackiert).
Echte Verbindung: kleiner Wert (~0.05-0.60 im 2k-Bereich). Hohe Werte (> ~1.00) sind Umwege ueber den
noch angeschlossenen Casio-Chip und zaehlen nicht.

Jede Taste hat zwei Haelften, also zwei Eintraege. Buchstaben = Leitungen auf der Vorderseite,
**EB** = Einzelblock: Haelfte mit keiner anderen verbunden, geht ueber ihre Durchfuehrung auf die
Rueckseite (dort noch zu messen, welche EB zusammengehoeren).

## Leitungen Vorderseite

| Leitung | Kontakte |
|---|---|
| A | 46, 47, 48, 49, 50 |
| B | 41, 42, 43, 44, 45, 50 |
| C | 1, 9, 13, 19, 25, 31, 36, 41 |
| D | 38, 43 |
| E | 35, 40, 45 |
| F | 36, 37, 38, 39, 40 |
| G | 31, 32, 33, 34, 35 |
| H | 5, 6, 15, 21, 27, 33 |
| I | 26, 27 |
| J | 28, 29, 30 |
| K | 4, 12, 18, 24, 30 |
| L | 17, 23, 29 |
| M | 7, 8, 16, 22, 28 |
| N | 2, 10, 14, 20, 26 |
| O | 20, 21 |
| P | 14, 15 |
| Q | 1, 2, 3, 5, 7 |
| R | 3, 11 |
| S | 6, 9, 10 |
| T | 16, 17, 18 |
| U | 8, 11, 12 |
| V | 23, 24 |
| EB | 4, 13, 19, 22, 25, 32, 34, 37, 39, 42, 44, 46, 47, 48, 49 |

## Tasten (50 von 50 vollstaendig)

| Kontakt | Taste (vermutlich) | Haelfte 1 | Haelfte 2 |
|---|---|---|---|
| 1 |  | C | Q |
| 2 |  | Q | N |
| 3 |  | Q | R |
| 4 |  | K | EB |
| 5 | hoch | H | Q |
| 6 | links | H | S |
| 7 | rechts | M | Q |
| 8 | runter | M | U |
| 9 |  | C | S |
| 10 |  | N | S |
| 11 |  | R | U |
| 12 |  | K | U |
| 13 |  | C | EB |
| 14 |  | N | P |
| 15 |  | H | P |
| 16 |  | M | T |
| 17 |  | L | T |
| 18 |  | K | T |
| 19 |  | C | EB |
| 20 |  | N | O |
| 21 |  | H | O |
| 22 |  | M | EB |
| 23 |  | L | V |
| 24 |  | K | V |
| 25 |  | C | EB |
| 26 |  | I | N |
| 27 |  | H | I |
| 28 |  | J | M |
| 29 |  | J | L |
| 30 |  | J | K |
| 31 | 7 | C | G |
| 32 | 8 | G | EB |
| 33 | 9 | G | H |
| 34 | DEL | G | EB |
| 35 | AC | E | G |
| 36 | 4 | C | F |
| 37 | 5 | F | EB |
| 38 | 6 | D | F |
| 39 | × | F | EB |
| 40 | ÷ | E | F |
| 41 | 1 | B | C |
| 42 | 2 | B | EB |
| 43 | 3 | B | D |
| 44 | + | B | EB |
| 45 | − | B | E |
| 46 | 0 | A | EB |
| 47 | . | A | EB |
| 48 | ×10ˣ | A | EB |
| 49 | Ans | A | EB |
| 50 | EXE | A | B |

Vorderseite komplett.

## Verbindungen ueber die Rueckseite (gemessen)

| Gruppe | verbunden | Anmerkung |
|---|---|---|
| X1 | EB 32, 37, 42 | Spalte 2-5-8; 47 (.) nicht genannt |
| X2 | EB 34, 39, 44 | Spalte +, ×, DEL. Verbindung zu D war ein Umweg ueber den Casio-Chip (hoher Wert) |
| X3 | I + J + EB 25 (Kontakte 25-30) | Zeile 25-30, bestaetigt |

## Vermutete Matrix (noch zu bestaetigen)

Viele Vorderseiten-Leitungen sind vermutlich ueber die Rueckseite verbunden. Aus dem Muster:

- Spalten: C (1/4/7-Spalte), N (+ 2; X1 = EB 32, 37, 42; 47?), H (+ D, EB 48?), M? (X2 = EB 34, 39, 44; 49?),
  L, K, dazu E und R (evtl. mit L oder K verbunden). EB 46 vermutlich an C.
- Zeilen: A (0-Reihe), B (1-Reihe), F (4-Reihe), G (7-Reihe), P+T+EB13, O+V+EB19+EB22,
  I+J+EB25 (= X3, bestaetigt), Q, S, U.
- Kontakt 4: K + EB (zuerst als EB/EB notiert, dann an K gefunden).
- EXE (50) liegt zwischen A und B.

Damit waeren es etwa 16-18 Leitungen: ein MCP23017 hat 16, der zweite (0x21) ist vorhanden.
