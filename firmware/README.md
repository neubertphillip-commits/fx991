# Casio-Deck Firmware

Arduino/C++ fuer den Seeed XIAO ESP32S3 Sense: Zustandsautomat (Rechner / Terminal /
Kamera), WLAN, WebSocket-Client zur Bridge, MCP23017-Tastaturscan, Texteingabe per
Mehrfachtippen, Kamera (OV3660). Das Display ist noch nicht angeschlossen; der
Bildschirminhalt wird bis dahin auf dem seriellen Monitor ausgegeben.

Ohne jede Hardware laesst sich alles im [PC-Simulator](#pc-simulator) ausprobieren.

## Bauen

WLAN-Daten eintragen:

```sh
cp casio_deck/secrets.example.h casio_deck/secrets.h   # SSID, Passwort, IP des Handys
```

**PlatformIO** (im Ordner `firmware/`):

```sh
pio run -t upload && pio device monitor
```

**Arduino-IDE**: `casio_deck/casio_deck.ino` oeffnen. Boardverwalter-URL
`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`,
Board *XIAO_ESP32S3*, PSRAM *OPI PSRAM*. Bibliotheken: *WebSockets* (Markus Sattler)
und *ArduinoJson* (Benoit Blanchon). Getestet mit Core 3.3.12.

**Host-Tests** (Rechner, Textpuffer, Mehrfachtippen): `cd hosttest && make`

## PC-Simulator

`sim/` baut dieselbe Firmware-Logik (`app.cpp`, `net.cpp`, `calc.cpp`, ...) als
Terminalprogramm fuer Linux, macOS oder WSL. Nur Hardware wird ersetzt: WLAN "steht"
sofort, der WebSocket ist ein kleiner eigener Client, die PC-Tastatur spielt die
Casio-Tasten, das Display wird als 60x40-Rahmen gezeichnet (Vorlage fuer das Layout
auf dem echten Panel).

```sh
cd sim && make                      # holt beim ersten Mal ArduinoJson per git
./casio-sim                         # Bridge auf localhost:8765
./casio-sim --host 192.168.43.1     # Bridge auf dem Handy
./casio-sim --cam foto.jpg          # Kameramodus schickt diese Datei
```

Die Bridge laesst sich auch auf dem Laptop starten (`python bridge.py`), dann braucht
es das Handy gar nicht. Tasten im Simulator:

| PC | Casio | PC | Casio |
|---|---|---|---|
| Tab | MODE | s | SHIFT |
| Enter oder = | EXE | a | ALPHA |
| Esc | AC | x / n / w | EXP / Ans / sqrt |
| Backspace | DEL | i / o / t | sin / cos / tan |
| Pfeile | UP/DOWN/LEFT/RIGHT | l / g | ln / log |
| 0-9 . + - * / ^ ( ) | wie beschriftet | | |

`:` oeffnet eine Befehlszeile fuer die seriellen Befehle (`:help`), `"` eine Zeile fuer
freien Text (wird wie vom seriellen Monitor eingegeben). Strg-C beendet.

## Aufbau

| Datei | Inhalt |
|---|---|
| `config.h` | Pinbelegung, Matrix-Zeilen/-Spalten, Displaygroesse, Timeouts |
| `app.cpp` | Zustandsautomat, Tastenbelegung je Modus, serielle Befehle |
| `keypad.cpp` | MCP23017: Zeilen-Scan, Entprellen, INTA-Wakeup |
| `keymap.cpp` | Matrixposition -> Taste (noch leer, siehe unten) |
| `multitap.cpp` | Buchstaben per Mehrfachtippen (ALPHA) |
| `camera.cpp` | OV3660: an/aus, JPEG aufnehmen |
| `net.cpp` | WLAN an/aus, WebSocket, JSON-Protokoll der Bridge |
| `screen.cpp` | Textpuffer 60x40: Status, Scrollback, Eingabezeile (UTF-8) |
| `display_serial.cpp` | Display-Ersatz: gibt den Screen seriell aus |
| `calc.cpp` | Ausdrucksauswerter fuer den Rechnermodus |

## Pinbelegung XIAO

| Pin | GPIO | Funktion |
|---|---|---|
| D0 | 1 | MCP23017 INTA (Open-Drain, Pull-up im ESP32) |
| D1 | 2 | LT7680 CS |
| D2 | 3 | LT7680 RST |
| D3 | 4 | LT7680 WAIT |
| D4 | 5 | I2C SDA |
| D5 | 6 | I2C SCL |
| D6, D7 | 43, 44 | frei |
| D8/D9/D10 | 7/8/9 | SPI SCK/MISO/MOSI (LT7680) |

MCP23017: A0-A2 an GND (0x20), RESET an 3V3.

## Modi

| Taste | Rechner | Terminal | Kamera |
|---|---|---|---|
| MODE | -> Terminal | -> Kamera | -> Rechner |
| EXE | ausrechnen | Prompt an Claude | Foto + Eingabe als Frage an Claude |
| AC | Eingabe loeschen | Eingabe loeschen, bei leerer Eingabe neue Sitzung | |
| DEL | letztes Zeichen | letztes Zeichen | |
| UP/DOWN | blaettern, mit SHIFT seitenweise | wie Rechner | wie Rechner |
| SHIFT+MODE | DEG/RAD | | |
| ALPHA | Ziffern/Buchstaben umschalten | wie Rechner | wie Rechner |

## Texteingabe (ALPHA)

Im Terminal und im Kameramodus ist ALPHA von Anfang an an (Status `abc`), im
Rechner aus (`123`). Buchstaben wie am alten Handy:

| Taste | Zeichen | Taste | Zeichen | Taste | Zeichen |
|---|---|---|---|---|---|
| 1 | . , ? ! : - ' 1 | 2 | a b c ae 2 | 3 | d e f 3 |
| 4 | g h i 4 | 5 | j k l 5 | 6 | m n o oe 6 |
| 7 | p q r s ss 7 | 8 | t u v ue 8 | 9 | w x y z 9 |
| 0 | Leerzeichen 0 | | | | |

Nach 1 s Pause ist das Zeichen fest; RIGHT schliesst es sofort ab (fuer zwei
Buchstaben auf derselben Taste). SHIFT davor gibt einen Grossbuchstaben.
Operatortasten (+, -, ...) schreiben auch im ALPHA-Modus ihr Zeichen.

## Kamera

Beim Wechsel in den Kameramodus geht die Kamera an, beim Verlassen wieder aus. EXE nimmt
ein Foto (SVGA, JPEG) auf, schickt es als Binaer-Frame an die Bridge und danach die
Eingabe als Frage; ohne Eingabe beschreibt Claude das Bild. Aufloesung, Qualitaet und
Spiegelung stehen in `config.h` (`CAM_*`).

WLAN geht beim Wechsel in Terminal oder Kamera an und im Rechnermodus nach 60 s
wieder aus. Ein Prompt, der vor dem Verbindungsaufbau abgeschickt wird, wartet und
geht raus, sobald die Bridge verbunden ist.

## Serieller Monitor (115200 Baud)

Jede Zeile wird im aktuellen Modus eingegeben und mit EXE abgeschickt, so laesst sich
alles ohne Tastatur testen. Befehle: `:calc` `:term` `:cam` (Modus), `:keys`
(alle Tastenereignisse protokollieren), `:wifi` (an/aus), `:new`, `:ping`,
`:key NAME` (Taste druecken, z.B. `:key EXE`, `:key sin`), `:help`.

## Tastaturmatrix ausmessen

Die Zeilen/Spalten in `config.h` sind vorlaeufig (9 Zeilen an GPB0-7 + GPA7,
7 Spalten an GPA0-6). Jede Taste, die in `keymap.cpp` noch nicht belegt ist, meldet
sich beim Druecken mit

```
[key] gedrueckt  Zeile 3 (GPB3) Spalte 5 (GPA5) -> unbelegt
```

Damit Taste fuer Taste die Tabelle `KEYMAP` fuellen. Hat der fx-991 mehr Zeilen als
Spalten (oder umgekehrt), die Pinlisten in `config.h` anpassen; GPA7/GPB7 muessen
Zeilen bleiben.

## Offen

- LT7680-Treiber als zweites Display-Backend (`display.h`)
- Kamera auf echter Hardware testen (Ausrichtung `CAM_VFLIP`/`CAM_HMIRROR`)
- Cursor in der Eingabezeile (LEFT/RIGHT zum Editieren)
- Light-Sleep mit INTA als Wakeup, CPU-Takt reduzieren
