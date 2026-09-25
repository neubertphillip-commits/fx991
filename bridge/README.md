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

## Protokoll

Siehe Docstring in `bridge.py`. Kurz: ESP32 schickt `{"t":"prompt","text":"..."}` oder
ein JPEG als Binaer-Frame, Bridge antwortet mit `busy`, beliebig vielen `line` und `done`.
