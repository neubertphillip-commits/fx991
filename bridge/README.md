# Casio-Deck Bridge

WebSocket-Server auf dem Handy (Termux). Nimmt Prompts/Bilder vom ESP32 an, fragt
`claude -p` und schickt die Antwort auf Displaybreite umgebrochen zurueck.

## Einrichtung in Termux

1. Termux aus **F-Droid** installieren (die Play-Store-Version ist veraltet).
2. Pakete:
   ```sh
   pkg update && pkg upgrade
   pkg install nodejs-lts python
   npm install -g @anthropic-ai/claude-code
   pip install websockets
   ```
3. Anmelden, eine der beiden Varianten:
   - **API-Credits (Console):** Key auf console.anthropic.com -> API Keys anlegen, dann
     ```sh
     echo 'export ANTHROPIC_API_KEY=sk-ant-...' >> ~/.bashrc && source ~/.bashrc
     ```
     Beim ersten `claude`-Start bestaetigen, dass der Key benutzt werden soll.
   - **Abo:** `claude` starten und `/login` durchlaufen.
4. Einmal `claude` interaktiv starten, Trust-Dialog fuer `~/.casio-deck/workspace` bestaetigen.
5. Dateien aufs Handy kopieren (z.B. nach `~/casio-deck/bridge/`) und starten:
   ```sh
   termux-wake-lock          # verhindert, dass Android Termux einschlaeft
   python bridge.py --cols 60
   ```

## Testen ohne Taschenrechner

IP des Handys im Hotspot herausfinden (`ifconfig` in Termux), dann vom Laptop:

```sh
python testclient.py ws://<handy-ip>:8765
```

Eingaben: normaler Text = Frage, `/new` = neue Sitzung, `/img foto.jpg` = Bild schicken.

Oder mit dem PC-Simulator der Firmware, der sich wie der Taschenrechner bedient
(siehe `firmware/README.md`): `firmware/sim/casio-sim --host <handy-ip>`.

## Optionen

| Flag | Bedeutung |
|---|---|
| `--cols 60` | Zeichen pro Displayzeile |
| `--model claude-sonnet-5` | schnelleres/guenstigeres Modell |
| `--allow 192.168.x.y` | nur diese Client-IP zulassen |
| `--auto-image` | Bild sofort auswerten, ohne auf eine Frage zu warten |
| `--stt "BEFEHL {file}"` | Spracherkennung fuer die Spracheingabe (siehe unten), auch per `CASIO_STT` |
| `--files ORDNER` | Ordner fuer den Datei-Viewer (Standard `~/.casio-deck/files`), auch per `CASIO_FILES` |

## Dateien fuer den Viewer

Was im Ordner `--files` liegt, holt sich der Rechner im Modus DATEIEN mit
`[Mit Handy abgleichen]`. Am bequemsten ist ein Ordner im normalen Handyspeicher,
dann lassen sich Dateien mit jedem Dateimanager oder per "Teilen" hineinlegen:

```sh
termux-setup-storage                      # einmal: Zugriff auf den Handyspeicher erlauben
mkdir -p ~/storage/shared/CasioDeck
python bridge.py --files ~/storage/shared/CasioDeck
```

Aufbereitet wird automatisch: Text wird UTF-8, PDFs werden Text, Bilder werden auf
480x640 verkleinert. Dafuer (optional):

```sh
pkg install poppler            # pdftotext fuer PDFs
pip install pillow             # Bilder verkleinern (ohne: nur kleine JPEGs unveraendert)
```

Unterordner und andere Dateitypen werden uebersprungen; die Bridge meldet sie beim Abgleich.
Tests: `python3 -m unittest test_library`.

## Spracheingabe (optional)

Claude nimmt ueber `claude -p` kein Audio an. Der Rechner schickt deshalb seine
Aufnahme als WAV (16 kHz, mono) an die Bridge, die sie mit einem beliebigen Programm
in Text umwandelt und zurueckschickt (`{"t":"text"}`); der Text landet in der
Eingabezeile des Rechners. Ohne `--stt` meldet die Bridge nur einen Fehler, alles
andere laeuft normal.

Empfohlen: [whisper.cpp](https://github.com/ggml-org/whisper.cpp), laeuft offline auf
dem Handy. In Termux (ungetestet, braucht ein paar Minuten zum Bauen):

```sh
pkg install git cmake clang
git clone --depth 1 https://github.com/ggml-org/whisper.cpp ~/whisper.cpp
cd ~/whisper.cpp && cmake -B build && cmake --build build -j4 --config Release
sh ./models/download-ggml-model.sh base      # ~150 MB; "small" ist genauer, aber langsamer
```

Dann die Bridge so starten:

```sh
python bridge.py --stt "~/whisper.cpp/build/bin/whisper-cli -m ~/whisper.cpp/models/ggml-base.bin -l de -nt -np -f {file}"
```

`{file}` wird durch den Pfad der Aufnahme ersetzt; alles, was der Befehl auf stdout
ausgibt, ist der erkannte Text (Markierungen wie `[BLANK_AUDIO]` werden entfernt).
Testen ohne Rechner: `python testclient.py` und `/wav aufnahme.wav`.

## Protokoll

Siehe Docstring in `bridge.py`. Kurz: ESP32 schickt `{"t":"prompt","text":"..."}` oder
ein JPEG als Binaer-Frame, Bridge antwortet mit `busy`, beliebig vielen `line` und `done`.
Ein WAV als Binaer-Frame (erkannt an `RIFF....WAVE`) beantwortet sie mit `busy`,
`text` (erkannte Sprache) und `done`.
Der Datei-Abgleich (`{"t":"sync"}`) wird mit `del` (Datei loeschen), `file` (Name,
Groesse) plus Binaer-Frames mit dem Inhalt, einer Zusammenfassung als `line` und `done`
beantwortet.
