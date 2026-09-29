# Tastaturplatine (Ersatz fuer die Casio-Platine)

Gleicher Umriss wie PWB-CY230-CL, vorn 50 vergoldete Kontaktkaemme unter den Kohlenoppen der
Silikonmatte, hinten MCP23017 (0x20) mit 100 nF und 4,7-kOhm-Pull-ups fuer I2C. Fuenf Loetpads
oben auf der Rueckseite fuer die Kabel zum XIAO: 3V3, GND, SDA, SCL, INT (INTA).

Stand: Entwurf 1, verdrahtet, DRC ohne Fehler (nur Hinweise "Footprint nicht aus Bibliothek").
Vorschau: `vorschau.png` (links vorn, rechts Rueckseite von hinten gesehen).

## Dateien

| Datei | Inhalt |
|---|---|
| `tastatur_geometrie.json` | Masse aus Fotos (Umriss, Loecher, Tastenmitten), mit Matte abgeglichen |
| `vorlage.py`, `vorlage.pdf` | Druckvorlage 1:1 fuer den Papiertest |
| `tastatur_pcb.py` | erzeugt `tastatur.kicad_pcb` und `tastatur.dsn`, liest `tastatur.ses` ein |
| `tastatur.ses` | Leiterbahnen von Freerouting |
| `drc.txt` | DRC-Bericht |
| `fertigung/` | Gerber + Bohrdateien (`tastatur_gerber.zip`), `bom_jlc.csv`, `cpl_jlc.csv` |

## Matrix (fuer die Firmware)

9 Zeilen (MCP-Ausgaenge) x 7 Spalten (Eingaenge mit Pull-up). Spalten C0..C6 = GPA0..GPA6,
Zeilen R0 = GPA7, R1..R8 = GPB0..GPB7. Tasten (Nummern wie `hardware/tastatur_nummern.jpg`):

| Zeile | Tasten (Spalte) |
|---|---|
| R0 | 1 (C0), 2 (C1), 6 (C2), 5 (C3), 7 (C4), 3 (C5), 4 (C6) |
| R1 | 9 (C0), 10 (C1), 8 (C3), 11 (C4), 12 (C5) |
| R2 | 13..18 (C0..C5) |
| R3 | 19..24 (C0..C5) |
| R4 | 25..30 (C0..C5) |
| R5 | 31, 32, 33, 34, 35 (C0, C1, C2, C4, C5) |
| R6 | 36..40 (wie R5) |
| R7 | 41..45 (wie R5) |
| R8 | 46..50 (wie R5) |

Echte Matrix: jede Taste weckt per Interrupt (Zeilen im Ruhezustand LOW).

## Neu erzeugen

```
python3 tastatur_pcb.py                                   # KiCad 7 (pcbnew)
java -jar freerouting-2.4.1-executable.jar -de tastatur.dsn -do tastatur.ses -mp 60   # Java 25
python3 tastatur_pcb.py --ses tastatur.ses
kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts --subtract-soldermask -o fertigung/gerber/ tastatur.kicad_pcb
kicad-cli pcb export drill --format excellon --excellon-separate-th -o fertigung/gerber/ tastatur.kicad_pcb
```

Freerouting gibt es auf Maven Central (`app.freerouting:freerouting`). Bauteile der Rueckseite
werden nicht per Flip umgedreht, sondern direkt mit B.Cu-Pads angelegt (sonst sieht Freerouting
die Pads auf der falschen Lage). Um die Loecher liegen Sperrflaechen fuer Bahnen.

## Bestellen (z. B. JLCPCB)

- `fertigung/tastatur_gerber.zip` hochladen: 2 Lagen, 0,6 mm, **ENIG** (Pflicht fuer die Kontakte).
- Bestueckung: Seite **Bottom**, `bom_jlc.csv` und `cpl_jlc.csv`. LCSC-Nummern in der Teilesuche
  bestaetigen, fuer den MCP23017-E/SO (SOIC-28 breit) selbst waehlen.
- In der Bestueckungsvorschau die Drehung pruefen: Pin 1 von U1 muss am Punkt "U1 Pin 1" liegen.

## Vor der Bestellung pruefen

- Papiertest mit `vorlage.pdf` und der Matte.
- Die Loecher sind absichtlich groesser (4,8-6 mm fuer 3-mm-Zapfen); die Platine wird beim Einbau
  an den Tasten ausgerichtet und festgeklebt.
- Die Firmware braucht fuer diese Platine einen Matrix-Scan (Tabelle oben) statt der 16 Casio-Leitungen.
