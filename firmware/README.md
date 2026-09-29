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
./casio-sim --files ordner          # Dateispeicher des Viewers (Standard: ./sim-files)
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

MCP23017 (SO-28): VDD (9) an 3V3, VSS (10) an GND, SCL (12), SDA (13), A0-A2 (15-17) an GND
(Adresse 0x20), RESET (18) an 3V3, INTA (20) an D0. Tastenleitungen siehe Abschnitt Tastatur.

## Modi

| Taste | Rechner | Terminal | Kamera | Dateien |
|---|---|---|---|---|
| MODE | -> Terminal | -> Kamera | -> Dateien | -> Rechner |
| EXE | ausrechnen | Prompt an Claude | Foto + Eingabe als Frage an Claude | oeffnen / naechste Seite |
| AC | Eingabe loeschen | Eingabe loeschen, bei leerer Eingabe neue Sitzung | | zurueck zur Liste |
| DEL | letztes Zeichen | letztes Zeichen | | |
| UP/DOWN | blaettern, mit SHIFT seitenweise | wie Rechner | wie Rechner | waehlen bzw. blaettern, SHIFT seitenweise |
| LEFT/RIGHT | | | | Seite zurueck/vor |
| SHIFT+MODE | DEG/RAD | WLAN 5 min an (Update) | WLAN 5 min an (Update) | WLAN 5 min an |
| SHIFT+AC | ausschalten | ausschalten | ausschalten | ausschalten |
| ALPHA | Ziffern/Buchstaben umschalten | wie Rechner | wie Rechner | |
| SHIFT+ALPHA | | Spracheingabe | Spracheingabe (Frage zum Foto) | |

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

## Dateien (Viewer)

Texte, PDFs und Bilder vom Handy offline lesen. Auf dem Handy kommen sie in einen
Ordner der Bridge (`--files`, siehe `bridge/README.md`). Im Modus DATEIEN holt EXE
auf `[Mit Handy abgleichen]` sie auf den Rechner: Die Bridge schickt nur neue und
geaenderte Dateien (Vergleich per CRC-32) und loescht auf dem Rechner, was im Ordner
nicht mehr liegt. Das WLAN ist nur waehrend des Abgleichs an.

- **Text** (.txt, .md, .csv, Quelltext ...): Die Bridge macht UTF-8 daraus. Der Rechner
  bricht selbst auf 60 Zeichen um und merkt sich nur, wo jede Anzeigezeile beginnt;
  die Datei bleibt im Flash, auch grosse Texte brauchen kaum RAM.
- **PDF**: Die Bridge zieht den Text heraus (`pdftotext`) und fuegt die Zeilen zu Absaetzen
  zusammen. Eingescannte PDFs ohne Text werden uebersprungen.
- **Bilder** (.jpg, .png, .webp ...): Die Bridge verkleinert sie auf hoechstens 480x640
  als JPEG (Pillow). Anzeigen kann sie erst der LT7680-Treiber (`display::showJpeg`);
  bis dahin zeigt der Viewer Name und Groesse.

Gespeichert wird im LittleFS (Partition `spiffs`, 1,5 MB, bleibt bei Updates per WLAN
erhalten). Passt eine Datei nicht mehr, laesst die Bridge sie aus und meldet es.

## WLAN nur bei Bedarf

Das WLAN ist in allen Modi aus, solange nichts gesendet wird. EXE (Frage oder Foto),
eine Sprachaufnahme oder AC fuer eine neue Sitzung legen die Anfrage in einen
Postausgang und schalten das WLAN ein; sobald die Bridge verbunden ist, geht sie raus.
Das Foto wird sofort aufgenommen, nicht erst nach dem Verbinden; bei der Sprachaufnahme
verbindet der Rechner schon waehrend des Sprechens. Nach der Antwort bleibt das WLAN
nur noch 3 s an (`WIFI_LINGER_MS`), dann geht es aus; eine Rueckfrage verbindet in etwa
1 s neu. Kommt nach 45 s keine Verbindung zustande, wird die Anfrage verworfen
(`NET_GIVEUP_MS`).

Kanal und Zugangspunkt des Hotspots werden gemerkt (auch im Tiefschlaf), damit das
Wiederverbinden ohne Kanalsuche geht. Klappt das nicht innerhalb von 3 s, sucht der
Rechner normal. Die Claude-Sitzung haelt die Bridge, sie ueberlebt das Trennen.

## Strom sparen

Der Akku ist der Engpass, deshalb spart die Firmware auf mehreren Ebenen:

- **CPU mit 80 MHz** statt 240 MHz (`CPU_MHZ`). Reicht fuer alles, auch fuer WLAN.
- **Warten auf Claude:** Waehrend Claude denkt, schlaeft das Funkmodul und hoert nur
  etwa jede Sekunde beim Hotspot nach, ob etwas angekommen ist (`WIFI_LISTEN_INTERVAL`,
  in Beacons zu ~100 ms). Das ist sparsamer als die Verbindung jedes Mal neu aufzubauen,
  und die Antwort kommt hoechstens ~1 s spaeter (gestreamte Zeilen in Schueben). Beim
  Senden (Bild, Sprache) und 2 s danach laeuft der Funk mit vollem Tempo
  (`WIFI_ACTIVE_MS`). Trennt der Hotspot im Wartemodus, `WIFI_LISTEN_INTERVAL` auf 3.
- **Leerlauf:** Ist das WLAN aus und keine Taste gedrueckt, geht der ESP32 2 s nach der
  letzten Eingabe in den Leichtschlaf; eine Taste (INTA) weckt ihn in ~1 ms, der Bildschirm
  bleibt stehen (`IDLE_LIGHT_SLEEP`). Nicht bei angestecktem USB (sonst bricht der serielle
  Monitor ab), nicht im Kameramodus. Braucht verdrahtetes INTA; ohne INTA ausschalten.
- **Aus:** Tiefschlaf nach SHIFT+AC oder 10 min (siehe Ein/Aus).

Groesster Verbraucher bleibt vermutlich die Hintergrundbeleuchtung des Displays; das
Dimmen kommt mit dem LT7680-Treiber. Alle Werte sind Schaetzungen, bis am Geraet
gemessen ist (Multimeter in die Akkuleitung).

## Serieller Monitor (115200 Baud)

Jede Zeile wird im aktuellen Modus eingegeben und mit EXE abgeschickt, so laesst sich
alles ohne Tastatur testen. Befehle: `:calc` `:term` `:cam` `:files` (Modus), `:keys`
(alle Tastenereignisse protokollieren), `:new`, `:ping`,
`:key NAME` (Taste druecken, z.B. `:key EXE`, `:key sin`), `:rec` (Spracheingabe
starten/abschicken), `:sync` (Dateien abgleichen), `:ota` (WLAN 5 min an),
`:off` (ausschalten), `:help`.

## Tastatur

Ausgemessen am fx-991DE CW, Protokoll in `hardware/tastatur_messung.md`, Kontaktnummern in
`hardware/tastatur_nummern.jpg`. 50 Tasten an 16 Leitungen, je eine an einem MCP23017-Pin:

| MCP-Pin | SO-28 Pin | Leitung | Tasten (Kontakte) | Rolle |
|---|---|---|---|---|
| GPA0 | 21 | C | 1, 9, 13, 19, 25, 31, 36, 41 | Eingang |
| GPA1 | 22 | N | 2, 10, 14, 20, 26, 32, 37, 42, 47 | Eingang |
| GPA2 | 23 | H | 5, 6, 15, 21, 27, 33, 38, 43, 48 | Eingang |
| GPA3 | 24 | M | 7, 8, 16, 22, 28, 34, 39, 44 | Eingang |
| GPA4 | 25 | L | 3, 11, 17, 23, 29, 35, 40, 45 | Eingang |
| GPA5 | 26 | K | 4, 12, 18, 24, 30 | Eingang |
| GPA6 | 27 | A | 46, 47, 48, 49, 50 | Eingang, wird auch getrieben |
| GPA7 | 28 | ON | 4 | Treiber (nur Ausgang) |
| GPB0 | 1 | Q | 1, 2, 3, 5, 7 | Treiber |
| GPB1 | 2 | X5 | 13-18 | Treiber |
| GPB2 | 3 | X4 | 19-24, 46 | Treiber |
| GPB3 | 4 | X3 | 25-30 | Treiber |
| GPB4 | 5 | G | 31-35 | Treiber |
| GPB5 | 6 | F | 36-40, 49 | Treiber |
| GPB6 | 7 | B | 41-45, 50 | Treiber |
| GPB7 | 8 | X6 | 6, 8, 9, 10, 11, 12 | Treiber (nur Ausgang) |

Jede Leitung braucht nur **einen** Draht, an eine beliebige Durchfuehrung der Leitung.

Es ist keine reine Zeilen/Spalten-Matrix: 0, ., x10^x, Ans und EXE verbinden A mit anderen
Leitungen. Der Scanner treibt deshalb jeden Treiber einzeln LOW und liest die Eingaenge,
danach A allein (fuer . und x10^x). Zwischen zwei Treibern liegt keine Taste, es gibt also
keinen Kurzschluss; `hosttest` prueft das fuer die ganze Tabelle. . und x10^x loesen im
Ruhezustand keinen Interrupt aus: Sie wecken nicht aus dem Tiefschlaf und werden im
Leichtschlaf erst beim naechsten Timer-Wecken (<= 1 s) erkannt.

Die Tasten 1-4 und 9-30 sind in `keymap.cpp` noch nicht zugeordnet. Beim Druecken meldet
der serielle Monitor

```
[key] gedrueckt   Kontakt 17, Leitungen L-X5 (GPA4/GPB1) -> unbelegt
```

`:keys` schaltet diese Meldung fuer alle Tasten ein (auch belegte).

## Offen

- LT7680-Treiber als zweites Display-Backend (`display.h`)
- Kamera auf echter Hardware testen (Ausrichtung `CAM_VFLIP`/`CAM_HMIRROR`)
- Mikrofon auf echter Hardware testen (`MIC_GAIN`, Schall durchs Gehaeuse)
- Cursor in der Eingabezeile (LEFT/RIGHT zum Editieren)
- Stromverbrauch im Tiefschlaf messen; Display/Kamera ggf. hart abschalten
- Bilder im Viewer anzeigen (mit dem LT7680-Treiber)
