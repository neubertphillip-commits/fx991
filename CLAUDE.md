# Casio-Deck

Umbau eines alten Casio fx-991 zum Cyberdeck: Ein XIAO ESP32S3 ersetzt den Casio-Chip,
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
- Mini-Schiebeschalter in die Akku-Plusleitung (XIAO hat keinen Schalter)
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

## Struktur

- `bridge/` Python-Bridge fuer Termux (lokal getestet), siehe `bridge/README.md` fuer Protokoll und Setup.
- `firmware/` Arduino/C++ fuer den ESP32 (PlatformIO oder Arduino-IDE, Core 3.x), siehe `firmware/README.md`.
  Ohne Display gibt die Firmware den Bildschirm auf dem seriellen Monitor aus; Eingaben gehen auch dort.
  `firmware/sim/` baut dieselbe Logik als PC-Simulator (Terminal) gegen die echte Bridge.
  `firmware/hosttest/` Unit-Tests fuer die hardwareunabhaengigen Teile.

## Offen

1. Tastaturmatrix des fx-991 ausmessen (Modell noch unklar), Zeilen/Spalten dokumentieren.
2. ~~Firmware-Grundgeruest~~ steht inkl. ALPHA-Mehrfachtippen (kompiliert, im Simulator getestet, auf Hardware ungetestet). Keymap fuellen, sobald 1. erledigt.
3. LT7680-Treiber, sobald das Panel da ist (zweites Backend fuer `display.h`).
4. ~~Kamera (OV3660) -> Binaer-Frame an Bridge~~ geschrieben, auf Hardware testen.
