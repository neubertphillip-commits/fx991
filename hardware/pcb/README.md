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

## Idee Version 2: Platine ersetzt die Sense-Platine (Variante C, geprueft 30.09.2026)

Ziel: XIAO flach auf der Rueckseite, Kamera und Mikro direkt auf unserer Platine statt der
Sense-Platine (die mit Kamera ~15 mm hoch ist; hinter der Tastaturplatine sind nur ~4 mm).

Was die Sense-Platine ueber den B2B-Stecker vom XIAO bekommt (Seeed-Doku, oshw-xiao-series):

| Funktion | GPIO |
|---|---|
| Kamera XMCLK | 10 |
| Kamera PCLK / VSYNC / HREF | 13 / 38 / 47 |
| Kamera Y2..Y9 | 15, 17, 18, 16, 14, 12, 11, 48 |
| Kamera SCCB SCL / SDA | 39 / 40 |
| PDM-Mikro CLK / DATA | 42 / 41 |
| SD-Karte CS (SCK/MISO/MOSI = D8/D9/D10) | 21 (brauchen wir nicht) |

Dazu 3V3/GND. Fuer unsere Platine waeren noetig: B2B-Gegenstecker, 24-pol. 0,5-mm-FPC-Buchse fuer
die OV3660, PDM-Mikrofon, Entkopplung. Footprint fuer den XIAO (SMD): Seeed-Bibliothek
`XIAO-ESP32-S3-SMD.kicad_mod` (oshw-xiao-series).

### Aus Seeeds Sense-Platine (Eagle-Dateien, geprueft 30.09.2026)

Quelle: `XIAO_ESP32S3_ExpBoard_v1.0_SCH&PCB_230324.zip` von
files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/res/ (lokal geladen, in der Cloud gesperrt).

Bauteile:

| Ref | Teil | Zweck |
|---|---|---|
| JA3 | Hirose DF40HC(3.0)-30DS-0.4V(51), 30 pol., 0,4 mm, 8,6 x 3,38 mm | B2B zum XIAO (Buchse, Stapelhoehe 3,0 mm) |
| JA1 | AFC01-S24FCC-00, 24 pol., 0,5 mm | FPC-Buchse OV3660 |
| MIC1 | MSM261D3526H1CPM (3,5 x 2,65 x 0,94 mm) | PDM-Mikro, Daten/Takt ueber Loetbruecken JP1/JP2 |
| U1 / U2 | SGM2036 (X2SON-4), Netze VCC_2V8 / VCC_1V8 | Kamera-LDOs, 2,8 V (AVDD ueber FB2) und 1,8 V (DOVDD) |
| JA2 | microSD-Halter | brauchen wir nicht |

Die LDOs haengen ueber R14 (0 Ohm) an VIN; R15 (nicht bestueckt) waere die Einspeisung aus 3V3.
Das Bauteil U2 heisst laut Wert `SGM2036S-1.3`, das Netz aber VCC_1V8: vor dem Nachbau im
Schaltplan-PDF klaeren. Kamera-FPC: Pin 6 = RESET (10 kOhm an 3V3 + 100 nF), Pin 8 ueber R10
(10 kOhm, vermutlich PWDN nach GND), Pin 24 ueber Diode D6 (MSK4005) mit C14/C15.

B2B-Belegung (JA3, Pads 1-15 eine Reihe, 16-30 gegenueber, 0,4 mm Raster):

| Pin | Netz | Pin | Netz |
|---|---|---|---|
| 1 | VIN | 16 | VIN |
| 2, 3 | GND | 17 | IO18 Y4 |
| 4 | IO42 PDM_CLK | 18 | IO17 Y3 |
| 5 | IO41 PDM_DATA | 19 | IO16 Y5 |
| 6 | IO40 CAM_SDA | 20 | IO15 Y2 |
| 7 | IO39 CAM_SCL | 21 | IO14 Y6 |
| 8 | IO38 VSYNC | 22 | IO13 PCLK |
| 9 | IO47 HREF | 23 | IO12 Y7 |
| 10 | IO48 Y9 | 24 | IO11 Y8 |
| 11 | (frei) | 25 | IO10 XMCLK |
| 12, 13 | 3V3 | 26 / 27 / 28 | D10 MOSI / D9 MISO / D8 SCK |
| 14 | GND | 29 | D2 SD_CS |
| 15 | IO21 USER_LED | 30 | GND |

Lage: Die Sense-Platine ist 17,78 x 15,37 mm, also so breit wie der XIAO. JA3 sitzt auf ihrer
Unterseite (gespiegelt), Mitte 11,05 mm vom linken und 2,31 mm vom unteren Rand in Seeeds
Draufsicht. Seeeds KiCad-Footprint `XIAO-ESP32-S3-SMD.kicad_mod` enthaelt den B2B nicht (nur Rand-
und Unterseitenpads), die Lage muss daher am echten XIAO nachgemessen werden (Messschieber, Foto).

Offen / Risiken:
- Hoehe: DF40 hat keine 0,6-1 mm Stapelhoehe. Seeed nutzt 3,0 mm; die flachste DF40-Buchse
  (DF40C-30DS-0.4V, Gegenstueck zum Stecker am XIAO) ergibt 1,5 mm. Mit XIAO ~3,5 mm also
  ~5 mm ueber unserer Platine, verfuegbar ~4 mm plus Rueckdeckel-Wanne (noch messen). So wie
  gedacht passt Variante C nur, wenn die Wanne mindestens ~1 mm bringt.
- Die Kantenpads des XIAO (I2C, SPI, Strom) muessen den Spalt von 1,5 mm zur Platine ueberbruecken
  (kurze Drahtbruecken oder Stiftleiste 1,5 mm).
- Zusaetzlich zu B2B, FPC und Mikro braucht die Platine die beiden Kamera-LDOs mit Beschaltung.
