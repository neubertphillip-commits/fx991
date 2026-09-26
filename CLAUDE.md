# Casio-Deck

Umbau eines Casio fx-991DE CW (ClassWiz) zum Cyberdeck: Ein XIAO ESP32S3 ersetzt den Casio-Chip,
ein IPS-Panel ersetzt das LCD, Gehaeuse und Tastatur bleiben. Ueber den Handy-Hotspot
spricht der Rechner mit einer Bridge in Termux, die Claude Code (`claude -p`) aufruft.

Offenes Bastelprojekt: Keine Tarn- oder Anti-Detektionsfunktionen bauen (z.B. gegen
Funkdetektoren oder um bei Kontrollen unentdeckt zu bleiben).

## Hardware (bestellt 25.09.2026)

- Seeed XIAO ESP32S3 Sense (OV3660-Kamera, abnehmbare Sense-Platine, LiPo-Lader onboard, 11 GPIO an der Kante)
- BuyDisplay 2,4" Bar-Type IPS 480x640, SPI+RGB, 40-Pin-ZIF, mit LT7680-Controllerboard (SPI -> RGB, RA8876-aehnlicher Befehlssatz; LovyanGFX unterstuetzt ihn vermutlich nicht direkt, BuyDisplay-Beispielcode als Basis)
- LiPo 3,7 V 300 mAh, 40x30x3 mm, an BAT-Pads des XIAO
- Kupferlackdraht 0,1 mm zum Anzapfen der Tastaturpads

## Noch zu bestellen

- MCP23017 als I/O-Expander fuer die Tastaturmatrix, I2C-Adresse 0x20: 2x MCP23017-E/SO (SO-28)
  + 2x SOIC-28-auf-DIP-Adapter. Zweiter Chip als Reserve oder fuer >16 Matrixleitungen (Adresse 0x21).
  Nicht die SSOP-Variante (-E/SS), die ist kaum von Hand zu loeten.
- ~~Mini-Schiebeschalter~~ entfaellt (keine Loecher): Ausschalten per Software statt Schalter
- Qi-Empfaenger (5 V, flach, Spule ~30-40 mm, Datenblatt: Dicke) an 5V-Pin des XIAO, davor
  Schottky-Diode (z.B. SS14) gegen Rueckspeisung, falls USB gleichzeitig steckt
- Fuer die Safe-Case-Mappe (Lade-Etui, darf Oeffnungen/Schalter haben): Qi-Sender 5 V,
  flacher LiPo (~1000-2000 mAh, Dicke nach Platz), Lade-/Boost-Modul mit USB-C (Powerbank-Modul),
  Schalter oder Reed-Kontakt + Magnet, damit der Sender nur bei geschlossener Mappe laeuft
- Vorschlag, offen: 2x 100 kOhm als Spannungsteiler fuer eine Akkuanzeige (an D3 statt LT7680-WAIT)
- Silikonlitze 28-30 AWG, Steckbrett + Jumperkabel
- Reserve: 4,7 kOhm (I2C), 100 nF (VDD des MCP23017), 1N4148/BAT54 (Matrix), Kapton-Band
- Offen: Versorgung des LT7680-Boards im Datenblatt pruefen; bei 5 V Step-up 3,7 -> 5 V noetig

## Entscheidungen

- MCP23017: GPA7/GPB7 nur als Ausgaenge (Datenblatt-Aenderung), also fuer Zeilen. Spalten als Eingaenge mit internen Pull-ups (GPPU).
- I2C zuerst ohne externe Pull-ups/100 nF: interne ESP32-Pull-ups, 100 kHz. Bei Problemen 4,7 kOhm nachruesten.
- XIAO-Pins: I2C (D4 SDA, D5 SCL), INTA vom MCP23017, SPI zum LT7680 (SCK, MOSI, MISO, CS, ggf. RST/WAIT).
- Eine WebSocket-Verbindung in beide Richtungen statt UDP/TCP-Mix. Verschluesselung macht WPA2.
- Bridge nutzt `claude -p --output-format stream-json`, nicht pexpect auf die TUI.
- Akku: mit WLAN grob 1-2 h Terminalbetrieb; WLAN/Kamera aus, wenn nicht gebraucht.
- Gehaeuse: keine neuen Loecher ausser fuer die Kamera (Kameraloch dient auch als Schallweg fuers Mikro).
  Die Kamera darf hinten etwas herausschauen (Rueckkamera). Dann zaehlt fuer die Bauhoehe nur
  XIAO + Sense-Platine ohne Kamera; die Mappe braucht eine Aussparung fuer den Buckel, damit der
  Rechner flach auf der Qi-Spule liegt. Kamera oben, Qi-Spule weiter unten an der Rueckwand.
  Kein Batteriefach vorhanden. Laden per Qi: Empfaengerspule innen an der Rueckwand (Ferrit zur
  Elektronik hin, kein Metall/Kupfer zwischen den Spulen). Sender + Powerbank in der mitgelieferten
  Safe-Case-Mappe (Rechner liegt mit der Rueckseite darin). USB-C des XIAO ist nach dem Einbau
  nicht erreichbar -> Firmware-Updates per WLAN (OTA) mit automatischem Rollback.
- Aus = Tiefschlaf (SHIFT+AC oder 10 min), Wecken per Taste ueber INTA (D0, RTC-faehig). Watchdog 30 s.
- Spracheingabe: PDM-Mikro der Sense-Platine -> WAV als Binaer-Frame -> Bridge wandelt per `--stt`
  (whisper.cpp) in Text. `claude -p` nimmt kein Audio. Optional: ohne `--stt` laeuft alles andere normal.

## Struktur

- `bridge/` Python-Bridge fuer Termux (lokal getestet), siehe `bridge/README.md` fuer Protokoll und Setup.
- `firmware/` Arduino/C++ fuer den ESP32 (PlatformIO oder Arduino-IDE, Core 3.x), siehe `firmware/README.md`.
  Ohne Display gibt die Firmware den Bildschirm auf dem seriellen Monitor aus; Eingaben gehen auch dort.
  `firmware/sim/` baut dieselbe Logik als PC-Simulator (Terminal) gegen die echte Bridge.
  `firmware/hosttest/` Unit-Tests fuer die hardwareunabhaengigen Teile.

## Offen

1. Tastaturmatrix des fx-991DE CW ausmessen, Zeilen/Spalten dokumentieren. Die CW-Tasten heissen
   teils anders als die logischen Tasten der Firmware (K_MODE, K_ALPHA, ...); Zuordnung beim Ausmessen.
2. ~~Firmware-Grundgeruest~~ steht inkl. ALPHA-Mehrfachtippen (kompiliert, im Simulator getestet, auf Hardware ungetestet). Keymap fuellen, sobald 1. erledigt.
3. LT7680-Treiber, sobald das Panel da ist (zweites Backend fuer `display.h`).
4. ~~Kamera (OV3660) -> Binaer-Frame an Bridge~~ geschrieben, auf Hardware testen.
5. ~~Spracheingabe~~ geschrieben (Simulator + Bridge getestet, whisper.cpp in Termux und Mikro ungetestet).
6. Einbau ohne Loecher: OTA, Watchdog, Tiefschlaf sind geschrieben (kompiliert, Simulator getestet).
   Offen: Gehaeuse-Innenmasse (Hoehe XIAO+Sense ~15 mm!), Platz fuer Qi-Spule + Akku an der
   Rueckwand, Abstand Spule-Mappe (< ~5 mm), Ruhestrom im Tiefschlaf und des Qi-Senders messen.
