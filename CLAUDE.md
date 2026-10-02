# Casio-Deck

Umbau eines Casio fx-991DE X (ClassWiz EX) zum Cyberdeck: Ein XIAO ESP32S3 ersetzt den Casio-Chip,
ein IPS-Panel ersetzt das LCD, Gehaeuse und Tastatur bleiben. Ueber den Handy-Hotspot
spricht der Rechner mit einer Bridge in Termux, die Claude Code (`claude -p`) aufruft.

Offenes Bastelprojekt: Keine Tarn- oder Anti-Detektionsfunktionen bauen (z.B. gegen
Funkdetektoren oder um bei Kontrollen unentdeckt zu bleiben).

## Hardware (bestellt 25.09.2026)

- Seeed XIAO ESP32S3 Sense (OV3660-Kamera, abnehmbare Sense-Platine, LiPo-Lader onboard, 11 GPIO an der Kante)
- BuyDisplay 2,4" Bar-Type IPS 480x640 (ER-TFT024-5: aussen 42,62 x 58,50 x 2,2 mm, aktiv 36,72 x 48,96 mm, hochkant), SPI+RGB, 40-Pin-ZIF, mit LT7680-Controllerboard (48,2 x 32 x 4,5 mm; SPI -> RGB, eigener Bildspeicher, RA8876-aehnlicher Befehlssatz; LovyanGFX unterstuetzt ihn vermutlich nicht direkt, BuyDisplay-Beispielcode als Basis)
- LiPo 3,7 V 300 mAh, 40x30x3 mm, an BAT-Pads des XIAO
- Kupferlackdraht 0,1 mm zum Anzapfen der Tastaturpads

## Reichelt-Bestellung (Warenkorb 26.09.2026)

- 2x MCP23017-E/SO (SO-28), zweiter als Reserve oder fuer >16 Matrixleitungen (Adresse 0x21)
- Schottky-Diode Vishay 10MQ060NTRPBF (60 V, 1 A, SMA) statt SS14, zwischen Qi-Empfaenger und 5V-Pin
- 100 nF 1206 X7R (12061C104KAT2A), 4,7 kOhm 1206 (WR12X4701FTL, Reserve fuer I2C), Kapton-Band
- Schaltlitze 0,14 mm^2 je 10 m: rot (3V3/Akku+), schwarz (GND), gelb (Signale)
- Kein Steckbrett, keine SOIC-Adapter: direkt loeten, nach jedem Schritt testen (dritte Hand vorhanden)

## Noch zu besorgen

- Qi-Empfaenger (5 V, flach, Spule ~30-40 mm, Datenblatt: Dicke) an 5V-Pin des XIAO, erst zur Endmontage
- Mappe: vorhandenes Qi-Ladepad ausschlachten, vorerst mit einer normalen Powerbank betreiben.
  Spaeter evtl. fest eingebaut: flacher LiPo, Powerbank-Modul mit USB-C, Reed-Kontakt + Magnet
- Vorschlag, offen: 2x 100 kOhm als Spannungsteiler fuer eine Akkuanzeige (an D3 statt LT7680-WAIT)
- Optional: 1N4148/BAT54, falls mehrere gleichzeitig gedrueckte Tasten Probleme machen

## Entscheidungen

- MCP23017: GPA7/GPB7 nur als Ausgaenge (Datenblatt-Aenderung). Tastatur: 16 Leitungen, je eine
  pro MCP-Pin (Belegung in firmware/README.md und config.h). Keine reine Zeilen/Spalten-Matrix
  (0, ., x10^x, Ans, EXE haengen an Leitung A): Treiber einzeln LOW, Eingaenge mit Pull-up lesen,
  dann A allein treiben. . und x10^x wecken nicht per Interrupt.
- I2C zuerst ohne externe Pull-ups/100 nF: interne ESP32-Pull-ups, 100 kHz. Bei Problemen 4,7 kOhm nachruesten.
- XIAO-Pins: I2C (D4 SDA, D5 SCL), INTA vom MCP23017, SPI zum LT7680 (SCK, MOSI, MISO, CS, ggf. RST/WAIT).
- LT7680-Board laeuft mit 3,3 V (vom 3V3-Pin des XIAO), kein Step-up noetig.
- Eine WebSocket-Verbindung in beide Richtungen statt UDP/TCP-Mix. Verschluesselung macht WPA2.
- Bridge nutzt `claude -p --output-format stream-json`, nicht pexpect auf die TUI.
- Akku: mit WLAN grob 1-2 h Terminalbetrieb; WLAN/Kamera aus, wenn nicht gebraucht.
- WLAN nur bei Bedarf: an beim Senden (Postausgang), aus 3 s nach der Antwort. Beim Warten
  auf Claude Modem-Sleep mit Listen-Interval ~1 s statt Trennen und Pollen (Neuverbinden kostet
  mehr). CPU 80 MHz, Leichtschlaf im Leerlauf (Wecken per INTA). Akku ist der Engpass.
  Kanal/BSSID im RTC-RAM fuer schnelles Wiederverbinden. SHIFT+MODE im Terminal/Kamera
  haelt es 5 min an (OTA-Update). Die Bridge haelt die Claude-Sitzung ueber Verbindungen hinweg.
- Gehaeuse: keine neuen Loecher ausser fuer die Kamera (Kameraloch dient auch als Schallweg fuers Mikro).
  Die Kamera darf hinten etwas herausschauen (Rueckkamera). Dann zaehlt fuer die Bauhoehe nur
  XIAO + Sense-Platine ohne Kamera; die Mappe braucht eine Aussparung fuer den Buckel, damit der
  Rechner flach auf der Qi-Spule liegt. Kamera oben, Qi-Spule weiter unten an der Rueckwand.
  Batteriefach nur fuer eine Knopfzelle (verschraubter Deckel oben rechts, zu klein fuer den
  LiPo; evtl. Platz fuer eine USB-C-Buchse als Notzugang ohne neues Loch). Laden per Qi: Empfaengerspule innen an der Rueckwand (Ferrit zur
  Elektronik hin, kein Metall/Kupfer zwischen den Spulen). Sender + Powerbank in der mitgelieferten
  Safe-Case-Mappe (Rechner liegt mit der Rueckseite darin). USB-C des XIAO ist nach dem Einbau
  nicht erreichbar -> Firmware-Updates per WLAN (OTA) mit automatischem Rollback.
- Display (Idee, gemessen): Panel quer (aktiv 48,96 x 36,72 mm = 640 x 480 px, ~0,0765 mm/px) hinter
  Display- UND Solarfenster. Dazu innen oben alles wegschneiden/schleifen (Knopfzellenhalter, Stege um
  das Solarfenster), die Front mit dem Steg zwischen den Fenstern bleibt. Gemessen: Displayfenster
  62 x 25 mm, Steg 4 mm, Solarfenster 35 x 15 mm; linker Rand Displayfenster ~8 mm, Solarfenster ~32 mm
  vom Gehaeuserand (Annahme: Solarfenster also 24-59 mm, rechtsbuendig, links das CASIO-Logo).
  Oberkante Solarfenster bis Unterkante Displayfenster 44 mm, Panel aussen 42,6 mm hoch: passt.
  Plan (Variante A): aktive Flaeche unten buendig mit dem Displayfenster und rechts buendig mit dem
  Solarfenster (Panel ~3,5 mm aus der Mitte nach rechts, Aussenkontur ragt rechts ueber das Fenster,
  Rand/FPC-Seite im Datenblatt pruefen). Unten Hauptbild 640 x 327 px (80 x 20 Zeichen bei 8x16,
  Fenster links ~10 mm, rechts ~3 mm schwarz), Steg verdeckt ~52 px, oben im Solarfenster ein Streifen
  ~457 x 100 px (px 183-639, Zeilen 0-99; 57 x 6 Zeichen bei 8x16) als zweiter Bildschirm (Status, Uhr,
  WLAN, Akku). Variante B (Panel hoch bis Oberkante Solarfenster): Streifen 196 px, Hauptbild nur 232 px.
  Nicht sichtbare Pixel bleiben schwarz; genaue Lage nach dem Einbau mit Testbild (Raster) einmessen.
  Firmware steht seit 30.09. auf diesem Layout (80 x 21, Bridge `--cols 80`); Vorschau in Originalgroesse:
  `firmware/tools/deckview.py`. Dort sichtbar: 8x16 ergibt nur 0,61 x 1,22 mm je Zeichen (sehr klein,
  evtl. 12x24-Font fuers Hauptbild pruefen); die Panel-Aussenkontur (58,5 x 42,6 mm, mittig um die aktive
  Flaeche angenommen) ragt ~3 mm unter und ~1,7 mm rechts ueber das Displayfenster hinaus: Platz im
  Gehaeuse dort pruefen, FPC-Seite im Datenblatt klaeren.
- Aus = Tiefschlaf (SHIFT+AC oder 10 min), Wecken per Taste ueber INTA (D0, RTC-faehig). Watchdog 30 s.
- Datei-Viewer (Modus DATEIEN): Ordner auf dem Handy (`bridge.py --files`), Abgleich per
  CRC-32 ins LittleFS (1,5 MB). Bridge macht PDF -> Text, Bilder -> JPEG 480x640.
  Rechner bricht Text selbst um (nur Zeilenanfaenge im RAM). Bilder erst mit LT7680-Treiber.
- Tastatur: Casio-Platine (PWB-CY230-CL, vernickelt) bleibt als Tastatur, Silikonmatte mit
  Kohlenoppen darueber. Anzapfen an den Durchkontaktierungen auf der Rueckseite (Karte hardware/loetpunkte.jpg;
  Ringe sind vermutlich Silberpaste und nehmen kein Zinn: Draht mit Silber-Leitkleber aufkleben, Heisskleber/Kapton als Zugentlastung) (0,1-mm-Lackdraht,
  flach mit Kapton). Casio-Chip (COB, U101) abgekoppelt: Klecks liess sich nicht abhebeln, stattdessen
  alle Bahnen entlang des weissen Rings um den Klecks durchgeritzt; Chip-Umwege weg, Leitungen intakt
  (nachgemessen). Batterie und Solarzelle abgeloetet (Pads P170-P173).
- Spracheingabe: PDM-Mikro der Sense-Platine -> WAV als Binaer-Frame -> Bridge wandelt per `--stt`
  (whisper.cpp) in Text. `claude -p` nimmt kein Audio. Optional: ohne `--stt` laeuft alles andere normal.

## Struktur

- `bridge/` Python-Bridge fuer Termux (lokal getestet), siehe `bridge/README.md` fuer Protokoll und Setup.
- `firmware/` Arduino/C++ fuer den ESP32 (PlatformIO oder Arduino-IDE, Core 3.x), siehe `firmware/README.md`.
  Ohne Display gibt die Firmware den Bildschirm auf dem seriellen Monitor aus; Eingaben gehen auch dort.
  `firmware/sim/` baut dieselbe Logik als PC-Simulator (Terminal) gegen die echte Bridge.
  `firmware/hosttest/` Unit-Tests fuer die hardwareunabhaengigen Teile.

## Offen

1. ~~Tastaturmatrix ausmessen~~ erledigt: `hardware/tastatur_messung.md`, Karte `hardware/tastatur_nummern.jpg`,
   Keymap in `keymap.cpp` mit Beschriftung (fx-991DE X, bestaetigt).
2. ~~Firmware-Grundgeruest~~ steht inkl. ALPHA-Mehrfachtippen (kompiliert, im Simulator getestet, auf Hardware ungetestet). Keymap fuellen, sobald 1. erledigt.
3. LT7680-Treiber, sobald das Panel da ist (zweites Backend fuer `display.h`, inkl. `showJpeg`).
4. ~~Kamera (OV3660) -> Binaer-Frame an Bridge~~ geschrieben, auf Hardware testen.
5. ~~Spracheingabe~~ geschrieben (Simulator + Bridge getestet, whisper.cpp in Termux und Mikro ungetestet).
6. Einbau ohne Loecher: OTA, Watchdog, Tiefschlaf sind geschrieben (kompiliert, Simulator getestet).
   Innentiefe ca. 8-10 mm (gemessen); oben Panel 2,2 + LT7680-Board 4,5 = 6,7 mm -> XIAO passt dort
   nicht dahinter. Hinter der Tastaturplatine ca. 4 mm (gemessen, Rueckseite Platine bis Gehaeuserand;
   Tiefe der Rueckdeckel-Wanne offen): MCP 1,75, Akku 3, XIAO allein ~3,5 mm passen, XIAO+Sense nicht (~15 mm mit Kamera), Platz fuer Qi-Spule + Akku an der
   Rueckwand, Abstand Spule-Mappe (< ~5 mm), Ruhestrom im Tiefschlaf und des Qi-Senders messen.
7. Eigene Tastaturplatine statt Anzapfen (Ringe der Casio-Platine nehmen kein Zinn): gleicher Umriss,
   ENIG-Kontakte fuer die Matte, MCP23017 bestueckt, echte Matrix. `hardware/pcb/`: Geometrie aus Fotos
   (tastatur_geometrie.json, ca. 0,5-1 mm genau), Druckvorlage fuer den Papiertest (vorlage.py/.pdf).
   Mit Mattenfoto abgeglichen (Noppen 4 mm, Zapfen 3 mm, Loecher 4,8-6 mm, wird angeklebt).
   Entwurf 1 steht: tastatur_pcb.py (KiCad 7 + Freerouting), 0,6 mm, ENIG, Matrix 9x7, DRC ok,
   Fertigungsdaten fuer JLCPCB in fertigung/. Entscheidung: Version 1 zuerst mit der Casio-Platine und
   Silber-Leitkleber (Firmware bleibt bei den 16 Casio-Leitungen), eigene Platine erst danach bestellen.
   Idee fuer Version 2: Platine uebernimmt die Sense-Platine (XIAO flach per B2B-Stecker, Kamera-FPC, PDM-Mikro).
   B2B ist Hirose DF40 (30 pol., min. 1,5 mm Stapelhoehe), Belegung und Kamera-LDOs in hardware/pcb/README.md.
   Hoehe dann ~5 mm hinter der Platine: passt nur mit ~1 mm aus der Rueckdeckel-Wanne (messen).
   V2 gezeichnet (hardware/pcb/v2/, 02.10.): Tasten+MCP+DF40-B2B+Kamera-FPC+LDOs+Mikro+Pads fuer LT7680,
   Akku, Qi (Schottky), Akkuteiler an D3; verdrahtet, DRC ohne Fehler. Entwurf: 9 offene Punkte in v2/README.md.
8. Spaeter, wenn Hardware und Firmware laufen: eine eigene Android-App (.apk) statt Termux,
   fuer alles, nicht eine APK pro Aufgabe. Bis dahin Termux-Bridge.
   - Aufbau: Kern (Hintergrunddienst, Anmeldung, ein gesicherter Kanal) + Module, per
     App-Update erweiterbar, einzeln ein-/ausschaltbar mit eigener Erlaubnis. Laptop-Seite:
     kleines CLI oder Weboberflaeche (`deck push datei.pdf`, `deck standort`, ...).
   - Module: Casio-Bridge (ruft die Claude-API direkt: API-Key noetig, Abo geht nur mit
     Claude Code; Spracherkennung neu loesen; spaeter Bluetooth LE zum Rechner), Dateien in
     beide Richtungen, Sensoren/Standort, Zeitachse (eigene Standortaufzeichnung als Ersatz
     fuer Google Maps; Export an Dawarich im OwnTracks-Format, Google-Export importieren,
     Claude kann per Werkzeug darauf zugreifen), Handy-Infos/Verwalten.
   - Grenzen ohne Root: keine stillen App-Installationen/Systemeinstellungen, keine Daten
     anderer Apps, Kamera/Mikro im Hintergrund nur mit sichtbarer Benachrichtigung.
   - Verbindung Laptop <-> Handy: Handy im Mobilfunk ist von aussen nicht erreichbar, es
     baut immer selbst die Verbindung zu einem Treffpunkt auf. Anschluss zu Hause ist Kabel
     (vermutlich DS-Lite, Fritzbox dann nur per IPv6 erreichbar). Optionen: Tailscale,
     WireGuard ueber Fritzbox (nur mit IPv6 ueberall), kleiner VPS, Syncthing fuer Dateien.
     Nur im eigenen VPN erreichbar, nur mit Schluessel, nie offene Ports.
   - Akku (wie beim Rechner, nur online wenn noetig): Casio-Bridge nur bei eingeschaltetem
     Hotspot (sonst Wake-Lock frei). Fernzugriff aus, Push weckt (FCM am sparsamsten,
     UnifiedPush/ntfy ohne Google), dann Live-Modus mit Zeitlimit (~10 min), beim Laden
     optional dauerhaft. Briefkasten: Auftraege alle 15-30 min abholen. Grosse Uebertragungen
     (Fotos, Zeitachse, Sensoren) gesammelt, nur im WLAN oder beim Laden. Wirkung mit der
     Akku-Statistik von Android messen.
   - Updates: GitHub Actions baut Firmware und APK; die App holt sie und gibt Firmware-Updates
     ueber den Hotspot an den Rechner (Bestaetigung am Rechner, Rollback vorhanden).
