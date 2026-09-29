# Tastaturmatrix fx-991DE X: Messprotokoll

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

| Kontakt | Taste (Layout fx-991DE X) | Haelfte 1 | Haelfte 2 |
|---|---|---|---|
| 1 | SHIFT | C | Q |
| 2 | ALPHA | Q | N |
| 3 | MENU/SETUP | Q | R |
| 4 | ON | K | EB |
| 5 | hoch | H | Q |
| 6 | links | H | S |
| 7 | rechts | M | Q |
| 8 | runter | M | U |
| 9 | OPTN | C | S |
| 10 | CALC | N | S |
| 11 | ∫ | R | U |
| 12 | x | K | U |
| 13 | Bruch | C | EB |
| 14 | √ | N | P |
| 15 | x² | H | P |
| 16 | x^n | M | T |
| 17 | log | L | T |
| 18 | ln | K | T |
| 19 | (−) | C | EB |
| 20 | °'" | N | O |
| 21 | x⁻¹ | H | O |
| 22 | sin | M | EB |
| 23 | cos | L | V |
| 24 | tan | K | V |
| 25 | STO | C | EB |
| 26 | ENG | I | N |
| 27 | ( | H | I |
| 28 | ) | J | M |
| 29 | S⇔D | J | L |
| 30 | M+ | J | K |
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
| 47 | , | A | EB |
| 48 | ×10ˣ | A | EB |
| 49 | Ans | A | EB |
| 50 | = | A | B |

Vorderseite komplett.

## Verbindungen ueber die Rueckseite (gemessen)

| Gruppe | verbunden | Anmerkung |
|---|---|---|
| X1 | EB 32, 37, 42 | Spalte 2-5-8; 47 (.) nicht genannt |
| X2 | EB 34, 39, 44 | Spalte +, ×, DEL. Verbindung zu D war ein Umweg ueber den Casio-Chip (hoher Wert) |
| X3 | I + J + EB 25 (Kontakte 25-30) | Zeile 25-30, bestaetigt |
| X4 | O + V + EB 19, 22 (Kontakte 19-24) | Zeile 19-24, bestaetigt |
| X5 | P + T + EB 13 (Kontakte 13-18) | Zeile 13-18, bestaetigt |
| X6 | S + U (Kontakte 6, 8, 9, 10, 11, 12) | Zeile oben, bestaetigt |
| N | + X1 (EB 32, 37, 42) + EB 47 | Spalte 2-5-8-. (42-14, 47-26 gemessen) |
| M | + X2 (EB 34, 39, 44) | Spalte DEL-×-+ (44-16 gemessen) |
| H | + D (38, 43) + EB 48 | Spalte 9-6-3-×10ˣ (38-33, 48-33 gemessen) |
| X4 | + EB 46 | 46 (0) haengt an Zeile 19-24 (46-19/20/21 gemessen) |
| L | = R = E | 11-17-40 gemessen: eine gemeinsame Spalte |

## Leitungen (Stand)

16 Leitungen, passt genau auf einen MCP23017 (16 Pins).

| Leitung | besteht aus | Tasten (Kontakte) |
|---|---|---|
| A | A | 46-50 |
| B | B | 41-45, 50 |
| F | F + EB 49 | 36-40, 49 |
| G | G | 31-35 |
| X3 | I + J + EB 25 | 25-30 |
| X4 | O + V + EB 19, 22, 46 | 19-24, 46 |
| X5 | P + T + EB 13 | 13-18 |
| Q | Q | 1, 2, 3, 5, 7 |
| X6 | S + U | 6, 8, 9, 10, 11, 12 |
| C | C | 1, 9, 13, 19, 25, 31, 36, 41 |
| N | N + X1 + EB 47 | 2, 10, 14, 20, 26, 32, 37, 42, 47 |
| H | H + D + EB 48 | 5, 6, 15, 21, 27, 33, 38, 43, 48 |
| M | M + X2 | 7, 8, 16, 22, 28, 34, 39, 44 |
| L | L + R + E | 3, 11, 17, 23, 29, 35, 40, 45 |
| K | K | 4, 12, 18, 24, 30 |
| ON | EB 4 | 4 (eigene Leitung, vermutlich die ON-Taste) |

Keine reine Zeilen/Spalten-Matrix: 0 (A-X4), . (A-N), Ans (A-F) und EXE (A-B) verbinden Leitungen, die sonst
beide Zeilen waeren (A-B-N bilden ein Dreieck). Die Firmware muss deshalb jede Leitung einzeln
treiben und alle anderen lesen.

Gemessen: 49-EB an 39/40 (= F), EXE an 41-45 (= B), 4-EB mit keiner Leitung verbunden (eigene Leitung ON).
Gegenmessung (gegenmessung.md): keine weiteren Leitungen gehoeren zusammen. **Matrix komplett.**

## Vermutete Matrix (noch zu bestaetigen)

Viele Vorderseiten-Leitungen sind vermutlich ueber die Rueckseite verbunden. Aus dem Muster:

- Spalten: C (1/4/7-Spalte), N (+ 2; X1 = EB 32, 37, 42; 47?), H (+ D, EB 48?), M? (X2 = EB 34, 39, 44; 49?),
  L, K, dazu E und R (evtl. mit L oder K verbunden). EB 46 vermutlich an C.
- Zeilen: A (0-Reihe), B (1-Reihe), F (4-Reihe), G (7-Reihe), P+T+EB13 (= X5), O+V+EB19+EB22 (= X4),
  I+J+EB25 (= X3, bestaetigt), Q, S+U (= X6).
- Kontakt 4: K + EB (zuerst als EB/EB notiert, dann an K gefunden).
- EXE (50) liegt zwischen A und B.

Damit waeren es etwa 16-18 Leitungen: ein MCP23017 hat 16, der zweite (0x21) ist vorhanden.

## Loetpunkte Rueckseite

Karten: `rueckseite_punkte.jpg` (alle 57 Durchkontaktierungen nummeriert), `loetpunkte.jpg` (die 16 zum Loeten).
Der silberne Ring leitet, nimmt aber kein Zinn (vermutlich Silberleitpaste, nicht mit Hitze
qualen). Loeten stattdessen auf die Leiterbahn, die vom Ring weggeht: 2-3 mm neben dem Ring den gruenen
Lack abkratzen, blankes Kupfer mit Flussmittel verzinnen, Draht anloeten, danach Durchgang zur Taste pruefen.

Zuordnung aus den Fotos berechnet: Rueckseite gespiegelt auf das Vorderseitenfoto (volle Aufloesung)
gelegt, dort die Kohleflaechen zwischen den gruenen Trennlinien segmentiert (Tastenscheiben ausgespart)
und fuer jeden Punkt geschaut, welche Tasten seine Flaeche beruehrt. Die gemeinsame Leitung dieser
Tasten ist die Leitung des Punkts; beruehrt die Flaeche nur eine Taste, ist es deren andere Haelfte (EB).
Jede Taste hat genau zwei Flaechen, die Mengen passen exakt zur Tabelle oben.

| Leitung | MCP | Punkt | Flaeche beruehrt Tasten | Alternativen |
|---|---|---|---|---|
| C | GPA0 | 12 | 1, 9, 13, 19, 25, 31, 36, 41 | - |
| N | GPA1 | 21 | 10, 14, 20, 26 | 5, 3 (EB 2), 29 (EB 32), 37 (EB 37), 44 (EB 42), 56 (EB 47) |
| H | GPA2 | 28 | 5, 6, 15, 21, 27, 33 | 38 (D: 38, 43), 51 (EB 48) |
| M | GPA3 | 17 | 7, 8, 16, 22, 28 | 19, 26 (EB 34), 33 (EB 39), 46 (EB 44) |
| L | GPA4 | 9 | 17, 23, 29 | 16, 4 (R: 3, 11), 32 (E: 35, 40, 45) |
| K | GPA5 | 13 | 12, 18, 24, 30 | 1 oder 2 (Haelfte K von 4) |
| A | GPA6 | 57 | 46, 47, 48, 49, 50 | 50 |
| ON | GPA7 | 1 oder 2 | nur 4 | der von beiden, der NICHT mit Punkt 13 piept |
| Q | GPB0 | 58 | 1, 2, 3, 5, 7 | - (links neben dem Loch bei Taste 1; im Rueckseitenfoto unter der Klemme) |
| X5 | GPB1 | 7 | 16, 17, 18 | 10 (P: 14, 15), 11 (EB 13) |
| X4 | GPB2 | 14 | 23, 24 | 20 (O: 20, 21), 22/24 (EB 19) |
| X3 | GPB3 | 15 | 28, 29, 30 | 25 (I: 26, 27), 23 (EB 25) |
| G | GPB4 | 30 | 31, 32, 33, 34, 35 | 31 |
| F | GPB5 | 36 | 36, 37, 38, 39, 40 | 39, 49 (EB 49) |
| B | GPB6 | 45 | 41, 42, 43, 44, 45, 50 | - |
| X6 | GPB7 | 8 | 8, 11, 12 | 6 (S: 6, 9, 10) |

Vor dem Loeten je Punkt einmal mit dem Durchgangspruefer gegen eine der genannten Tasten bestaetigen.
