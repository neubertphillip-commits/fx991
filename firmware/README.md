# Casio-Deck Firmware

Arduino/C++ fuer den Seeed XIAO ESP32S3 Sense. Grundgeruest: Zustandsautomat
(Rechner / Terminal / Kamera), WLAN, WebSocket-Client zur Bridge, MCP23017-Tastaturscan.
Das Display ist noch nicht angeschlossen; der Bildschirminhalt wird bis dahin auf dem
seriellen Monitor ausgegeben.

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

**Host-Tests** (Rechner und Textpuffer, ohne Hardware): `cd hosttest && make`

## Aufbau

| Datei | Inhalt |
|---|---|
| `config.h` | Pinbelegung, Matrix-Zeilen/-Spalten, Displaygroesse, Timeouts |
| `app.cpp` | Zustandsautomat, Tastenbelegung je Modus, serielle Befehle |
| `keypad.cpp` | MCP23017: Zeilen-Scan, Entprellen, INTA-Wakeup |
| `keymap.cpp` | Matrixposition -> Taste (noch leer, siehe unten) |
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
| EXE | ausrechnen | Prompt an Claude | (TODO) Foto senden |
| AC | Eingabe loeschen | Eingabe loeschen, bei leerer Eingabe neue Sitzung | |
| DEL | letztes Zeichen | letztes Zeichen | |
| UP/DOWN | blaettern, mit SHIFT seitenweise | wie Rechner | wie Rechner |
| SHIFT+MODE | DEG/RAD | | |

WLAN geht beim Wechsel in Terminal oder Kamera an und im Rechnermodus nach 60 s
wieder aus. Ein Prompt, der vor dem Verbindungsaufbau abgeschickt wird, wartet und
geht raus, sobald die Bridge verbunden ist.

## Serieller Monitor (115200 Baud)

Jede Zeile wird im aktuellen Modus eingegeben und mit EXE abgeschickt, so laesst sich
alles ohne Tastatur testen. Befehle: `:calc` `:term` `:cam` (Modus), `:keys`
(alle Tastenereignisse protokollieren), `:wifi` (an/aus), `:new`, `:ping`, `:help`.

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
- Kamera: OV3660 init, JPEG an die Bridge (`submitCamera()` in `app.cpp`)
- Texteingabe per ALPHA/Mehrfachtippen
- Light-Sleep mit INTA als Wakeup, CPU-Takt reduzieren
