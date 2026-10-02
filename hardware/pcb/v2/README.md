# Hauptplatine V2 (Entwurf, nicht bestellfertig)

Tastaturplatine wie V1 plus alles, was sonst lose verdrahtet waere: Der XIAO ESP32S3 liegt mit
der Unterseite zur Platine auf deren Rueckseite und steckt per B2B in einer DF40-Buchse; Kamera,
Mikro und Kamera-Spannungsregler sitzen auf der Platine (Ersatz fuer die Sense-Platine, die mit
~15 mm zu hoch ist). Generator: `hauptplatine_v2.py` (nutzt `../tastatur_pcb.py`).
Vorschau: `vorschau_v2.png` (links vorn, rechts Rueckseite von hinten).

## Was drauf ist

| Ref | Teil | Funktion |
|---|---|---|
| K1-K50 | Kontaktkaemme (ENIG) | Tasten, Matrix 9x7 wie V1 |
| U1, C1, R1, R2 | MCP23017, 100 nF, 2x 4,7 kOhm | Tastatur-Scanner, I2C-Pull-ups |
| J10 | Hirose DF40C-30DS-0.4V (1,5 mm Stapelhoehe) | B2B zum XIAO: 3V3, GND, Kamera, Mikro, SPI, D2 |
| J11 | 8 Bruecken-Pads unter den XIAO-Kantenpads | D0 (INT), D1 (LCD-CS), D3 (Akku-Messung), D4/D5 (I2C), 5V, BAT-Pads |
| J12, U2, U3, FB1, R3, R4, C2-C7 | 24-pol. FPC (0,5 mm), 2x LDO | Kamera OV3660 |
| MIC1, C8 | MSM261D3526H1CPM | PDM-Mikro (GPIO 42 Takt, 41 Daten) |
| J13 | 8 Pads | LT7680-Displayboard: 3V3, GND, SCK, MOSI, MISO, CS (D1), RST (D2), WAIT (frei) |
| J14 | 2 Pads | LiPo (geht ueber J11 an die BAT-Pads des XIAO) |
| J15, D1 | 2 Pads, Schottky 10MQ060N | Qi-Empfaenger -> 5V-Pin des XIAO |
| R5, R6 | 2x 100 kOhm | Akkuspannung halbiert an D3 (statt LT7680-WAIT) |

Montage: XIAO mit Akku-Draehten zuerst auf die DF40-Buchse stecken; die Kantenpads und BAT-Pads
ueberbruecken den 1,5-mm-Spalt mit kurzen Drahtstuecken (Bauteilbeinchen) zu J11.

## Vor der Bestellung (offen, ohne das nicht bestellen)

1. **Lage der B2B-Buchse unter dem XIAO**: aus Seeeds Sense-Platine abgeleitet (Mitte 11,05 / 2,31 mm),
   aber angenommen, dass der Stecker am Ende gegenueber USB sitzt. Am echten XIAO nachmessen
   (Foto der Unterseite mit Lineal genuegt).
2. **DF40-Footprint und Pin-1-Lage**: Reihenabstand (hier 2,2 mm) und Padmasse aus der Hirose-Zeichnung,
   Pin 1 der Buchse gegen Seeeds Sense-Layout pruefen (Belegung siehe `../README.md`).
3. **Kamera-Spannungen**: AVDD 2,8 V ist sicher. DOVDD (FPC-Pin 11) hier 2,8 V, DVDD (Pin 10) ueber U3;
   Seeed nutzt laut Stueckliste ein 1,3-V-LDO, das Netz heisst aber VCC_1V8. Mit Seeeds Schaltplan-PDF
   klaeren (ESP32-S3 braucht >= 2,5 V High-Pegel, also DOVDD nicht 1,8 V). FPC-Pin 24 hat bei Seeed
   eine Diode (D6) - Funktion klaeren.
4. **FPC-Buchse und Pin 1**: Footprint AFC01-S24FCC-00 angenaehert; Pin-Reihenfolge zur Kamera pruefen.
5. **Mikro-Footprint** ist ein Platzhalter (Padlage aus dem Datenblatt uebernehmen; Schalloeffnung oben?).
6. **BAT-Polaritaet** der XIAO-Unterseitenpads (BAT1/BAT2) am XIAO ablesen und J14 beschriften.
7. **Hoehe**: XIAO ueber der Platine ~5 mm (1,5 mm Buchse + ~3,5 mm XIAO); hinter der Platine sind
   ~4 mm gemessen. Rueckdeckel-Wanne messen; sonst Rueckdeckel innen ausfraesen.
8. **Teile bei JLCPCB/LCSC**: DF40C-30DS-0.4V, AFC01-S24FCC-00, MSM261D3526H1CPM, ME6211-LDOs
   auf Verfuegbarkeit pruefen; sonst Ersatzteile mit gleichem Footprint.
9. Firmware: Matrix-Scan fuer V1/V2 (statt der 16 Casio-Leitungen), D3 als Akku-Messung.

## Neu erzeugen

```
python3 hauptplatine_v2.py
java -jar freerouting-2.4.1-executable.jar -de v2.dsn -do v2.ses -mp 60   # Java 25
python3 hauptplatine_v2.py --ses v2.ses
```
