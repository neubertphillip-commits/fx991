# Casio-Deck Firmware

Arduino/C++ fuer den Seeed XIAO ESP32S3 Sense: Zustandsautomat (Rechner / Terminal /
Kamera), WLAN, WebSocket-Client zur Bridge, MCP23017-Tastaturscan, Texteingabe per
Mehrfachtippen, Spracheingabe, Kamera (OV3660), Ein/Aus per Tiefschlaf und Updates per WLAN. Das Display ist noch nicht angeschlossen; der
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

**Host-Tests** (Rechner, Textpuffer, Mehrfachtippen, WAV): `cd hosttest && make`

## Update per WLAN (OTA)

Nach dem Einbau ist der USB-C-Anschluss nicht mehr erreichbar. Die erste Firmware
kommt per USB drauf, danach geht es per WLAN:

1. Im Terminal- oder Kameramodus SHIFT+MODE druecken: das WLAN bleibt 5 min an.
   Laptop in denselben Hotspot.
2. `CASIO_OTA_PASSWORD=<OTA_PASSWORD aus secrets.h> pio run -e xiao_esp32s3_ota -t upload`
   (Arduino-IDE: Netzwerk-Port `casio-deck` waehlen, Passwort eingeben.)
3. Der Rechner zeigt "Update laeuft", startet neu und meldet "Neue Firmware bestaetigt".

**Absicherung:** Eine neue Firmware gilt erst als gut, wenn sie nach dem Neustart wieder
ins WLAN kommt und selbst Updates annehmen kann. Stuerzt sie vorher ab oder startet neu
(auch durch den Watchdog nach 30 s Haenger), laedt der Bootloader automatisch die vorige
Firmware. Solange sie unbestaetigt ist, laesst sich der Rechner nicht ausschalten.
Nur eine Firmware, die laeuft und WLAN kann, aber das Update-Modul kaputt hat, wuerde
das Oeffnen des Gehaeuses erfordern.

## Ein/Aus

Kein Schalter: SHIFT+AC schaltet aus (wie beim Casio), ebenso 10 min ohne Eingabe
(`AUTO_OFF_MS`). Aus heisst Tiefschlaf; der MCP23017 bleibt versorgt, jede Taste zieht
INTA auf LOW und weckt den ESP32. Die Wecktaste wird nicht als Eingabe gewertet. Modus,
Ans und DEG/RAD bleiben erhalten, der Bildschirminhalt nicht. Nicht ausgeschaltet wird,
solange eine Anfrage oder Aufnahme laeuft, ein Update laeuft/unbestaetigt ist oder der
MCP23017 fehlt (dann koennte nichts wecken). Der Stromverbrauch im Tiefschlaf (Kamera,
LT7680-Board) ist noch zu messen.

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
./casio-sim --mic sprache.wav       # Spracheingabe "nimmt" diese Datei auf (16 kHz mono)
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
| 0-9 . + - * / ^ ( ) | wie beschriftet | v | SHIFT+ALPHA (Sprache) |

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
| `mic.cpp` | PDM-Mikrofon: Aufnahme in den PSRAM (eigener Task) |
| `wav.cpp` | WAV-Header, Verstaerkung |
| `power.cpp` | Tiefschlaf/Wecken, Watchdog, Bestaetigung neuer Firmware |
| `ota.cpp` | Update per WLAN (ArduinoOTA) |
| `credentials.h` | bindet `secrets.h` ein (WLAN, Bridge, OTA-Passwort) |
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
| SHIFT+MODE | DEG/RAD | WLAN 5 min an (Update) | WLAN 5 min an (Update) |
| SHIFT+AC | ausschalten | ausschalten | ausschalten |
| ALPHA | Ziffern/Buchstaben umschalten | wie Rechner | wie Rechner |
| SHIFT+ALPHA | | Spracheingabe | Spracheingabe (Frage zum Foto) |

## Spracheingabe

Das PDM-Mikrofon der Sense-Platine (GPIO42 Takt, GPIO41 Daten, keine Kantenpins).
SHIFT+ALPHA startet die Aufnahme (Status `REC 3s`), EXE oder nochmal ALPHA beendet und
schickt sie als WAV an die Bridge, AC verwirft. Nach max. 30 s wird automatisch
abgeschickt. Der erkannte Text wird an die Eingabezeile angehaengt und laesst sich mit
Mehrfachtippen korrigieren; EXE schickt ihn dann an Claude (`VOICE_AUTO_SEND` in
`config.h` schickt sofort). Die Umwandlung in Text macht die Bridge, siehe
`bridge/README.md`. Damit der Schall ankommt, sollte das Kameraloch nah am Mikrofon liegen.

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

## WLAN nur bei Bedarf

Das WLAN ist in allen Modi aus, solange nichts gesendet wird. EXE (Frage oder Foto),
eine Sprachaufnahme oder AC fuer eine neue Sitzung legen die Anfrage in einen
Postausgang und schalten das WLAN ein; sobald die Bridge verbunden ist, geht sie raus.
Das Foto wird sofort aufgenommen, nicht erst nach dem Verbinden; bei der Sprachaufnahme
verbindet der Rechner schon waehrend des Sprechens. Nach der letzten Antwort bleibt das
WLAN noch 30 s an (`WIFI_LINGER_MS`), fuer schnelle Rueckfragen, dann geht es aus.
Kommt nach 45 s keine Verbindung zustande, wird die Anfrage verworfen (`NET_GIVEUP_MS`).

Kanal und Zugangspunkt des Hotspots werden gemerkt (auch im Tiefschlaf), damit das
Wiederverbinden ohne Kanalsuche geht. Klappt das nicht innerhalb von 3 s, sucht der
Rechner normal. Die Claude-Sitzung haelt die Bridge, sie ueberlebt das Trennen.

## Serieller Monitor (115200 Baud)

Jede Zeile wird im aktuellen Modus eingegeben und mit EXE abgeschickt, so laesst sich
alles ohne Tastatur testen. Befehle: `:calc` `:term` `:cam` (Modus), `:keys`
(alle Tastenereignisse protokollieren), `:new`, `:ping`,
`:key NAME` (Taste druecken, z.B. `:key EXE`, `:key sin`), `:rec` (Spracheingabe
starten/abschicken), `:ota` (WLAN 5 min an), `:off` (ausschalten), `:help`.

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
- Mikrofon auf echter Hardware testen (`MIC_GAIN`, Schall durchs Gehaeuse)
- Cursor in der Eingabezeile (LEFT/RIGHT zum Editieren)
- Stromverbrauch im Tiefschlaf messen; Display/Kamera ggf. hart abschalten
- CPU-Takt reduzieren, Light-Sleep im Rechnermodus
