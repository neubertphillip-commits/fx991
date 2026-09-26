#!/usr/bin/env python3
"""Casio-Deck Bridge: WebSocket-Server zwischen Taschenrechner (ESP32) und Claude Code.

Laeuft in Termux auf dem Handy. Pro Anfrage wird `claude -p` headless gestartet,
die Antwort auf Displaybreite umgebrochen und zeilenweise zurueckgeschickt.

Protokoll (ESP32 -> Bridge):
  Text-Frame  {"t":"prompt","text":"..."}   Frage stellen
  Text-Frame  {"t":"new"}                   neue Sitzung (Kontext vergessen)
  Text-Frame  {"t":"ping"}                  Verbindungstest
  Binaer-Frame  JPEG-Bild; wird mit dem naechsten (oder Default-)Prompt ausgewertet
  Binaer-Frame  WAV (16 kHz mono); wird per --stt in Text umgewandelt (Spracheingabe)

Protokoll (Bridge -> ESP32), immer Text-Frames:
  {"t":"busy"}                 Anfrage laeuft
  {"t":"line","text":"..."}    eine fertig umgebrochene Displayzeile
  {"t":"done"}                 Antwort komplett
  {"t":"text","text":"..."}    erkannte Sprache (geht in die Eingabezeile des Rechners)
  {"t":"err","text":"..."}     Fehler
  {"t":"pong"}
"""

import argparse
import asyncio
import json
import os
import re
import shlex
import textwrap
import time
from pathlib import Path

import websockets

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
BASE = Path(os.environ.get("CASIO_HOME", Path.home() / ".casio-deck"))
SNAP_DIR = BASE / "snaps"
AUDIO_DIR = BASE / "audio"
WORKDIR = BASE / "workspace"

SYSTEM_HINT = (
    "Du antwortest auf einem sehr kleinen Display ({cols} Zeichen breit). "
    "Antworte knapp, ohne Markdown-Tabellen und ohne Codebloecke, wenn es nicht noetig ist."
)
IMAGE_PROMPT = "Beschreibe knapp, was auf dem Bild {path} zu sehen ist."
STT_TIMEOUT = 120  # Sekunden
# Markierungen, die whisper.cpp statt Text ausgibt, z.B. [BLANK_AUDIO] oder (Musik)
STT_NOISE = re.compile(r"\[[^\]]*\]|\([^)]*\)")


def is_wav(data: bytes) -> bool:
    return len(data) > 44 and data[:4] == b"RIFF" and data[8:12] == b"WAVE"


def clean_transcript(raw: str) -> str:
    """Macht aus der Ausgabe des Spracherkenners eine Zeile Text."""
    text = " ".join(line.strip() for line in ANSI.sub("", raw).splitlines())
    return re.sub(r"\s+", " ", STT_NOISE.sub(" ", text)).strip()


def wrap(text: str, cols: int) -> list[str]:
    """Bricht Text hart auf `cols` Zeichen um, Leerzeilen bleiben erhalten."""
    out = []
    for para in ANSI.sub("", text).splitlines():
        if not para.strip():
            out.append("")
            continue
        indent = len(para) - len(para.lstrip())
        out.extend(
            textwrap.wrap(
                para,
                width=cols,
                subsequent_indent=" " * min(indent, 4),
                break_long_words=True,
                replace_whitespace=False,
            )
        )
    return out


class Bridge:
    def __init__(self, args):
        self.args = args
        self.session_id: str | None = None
        self.pending_image: Path | None = None
        self.lock = asyncio.Lock()

    async def send(self, ws, **msg):
        await ws.send(json.dumps(msg, ensure_ascii=False))

    async def run_claude(self, ws, prompt: str):
        cmd = [
            self.args.claude, "-p", prompt,
            "--output-format", "stream-json", "--verbose",
            "--append-system-prompt", SYSTEM_HINT.format(cols=self.args.cols),
            # Nur Lesen erlaubt (fuer Bilder); keine Shell, keine Dateiaenderungen.
            "--allowedTools", "Read",
            # Keine MCP-Server/Connectoren laden: schneller, keine Nebengeraeusche.
            "--strict-mcp-config",
        ]
        if self.args.model:
            cmd += ["--model", self.args.model]
        if self.session_id:
            cmd += ["--resume", self.session_id]

        proc = await asyncio.create_subprocess_exec(
            *cmd,
            cwd=WORKDIR,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            limit=16 * 1024 * 1024,
        )
        got_text = False
        async for raw in proc.stdout:
            try:
                ev = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if ev.get("session_id"):
                self.session_id = ev["session_id"]
            if ev.get("type") == "assistant":
                for block in ev.get("message", {}).get("content", []):
                    if block.get("type") == "text" and block.get("text"):
                        if got_text:
                            await self.send(ws, t="line", text="")
                        got_text = True
                        for line in wrap(block["text"], self.args.cols):
                            await self.send(ws, t="line", text=line)
            elif ev.get("type") == "result" and ev.get("is_error"):
                await self.send(ws, t="err", text=str(ev.get("result", "Fehler"))[:200])

        rc = await proc.wait()
        if rc != 0 and not got_text:
            err = (await proc.stderr.read()).decode(errors="replace").strip()
            await self.send(ws, t="err", text=(err or f"claude exit {rc}")[-200:])

    async def handle_prompt(self, ws, text: str):
        if self.pending_image:
            prompt = f"{text}\n\nBild: {self.pending_image}" if text else IMAGE_PROMPT.format(path=self.pending_image)
            self.pending_image = None
        else:
            prompt = text
        if not prompt.strip():
            await self.send(ws, t="err", text="leerer Prompt")
            return
        async with self.lock:
            await self.send(ws, t="busy")
            try:
                await self.run_claude(ws, prompt)
            except FileNotFoundError:
                await self.send(ws, t="err", text=f"'{self.args.claude}' nicht gefunden")
            await self.send(ws, t="done")

    async def transcribe(self, ws, path: Path):
        """Wandelt eine Sprachaufnahme mit dem --stt-Befehl in Text um."""
        async with self.lock:
            await self.send(ws, t="busy")
            try:
                if not self.args.stt:
                    await self.send(ws, t="err", text="Spracherkennung nicht eingerichtet (--stt)")
                    return
                cmd = [os.path.expanduser(a) for a in
                       shlex.split(self.args.stt.replace("{file}", shlex.quote(str(path))))]
                proc = await asyncio.create_subprocess_exec(
                    *cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
                try:
                    out, err = await asyncio.wait_for(proc.communicate(), STT_TIMEOUT)
                except asyncio.TimeoutError:
                    proc.kill()
                    await self.send(ws, t="err", text="Spracherkennung: Zeitueberschreitung")
                    return
                text = clean_transcript(out.decode(errors="replace"))
                if proc.returncode != 0 and not text:
                    msg = err.decode(errors="replace").strip() or f"exit {proc.returncode}"
                    await self.send(ws, t="err", text=f"Spracherkennung: {msg}"[-200:])
                elif not text:
                    await self.send(ws, t="err", text="nichts verstanden")
                else:
                    print(f"erkannt: {text}")
                    await self.send(ws, t="text", text=text)
            except FileNotFoundError:
                await self.send(ws, t="err", text=f"Spracherkennung: '{cmd[0]}' nicht gefunden")
            except ValueError as e:  # --stt nicht zerlegbar, z.B. offenes Anfuehrungszeichen
                await self.send(ws, t="err", text=f"Spracherkennung: --stt ungueltig ({e})")
            finally:
                await self.send(ws, t="done")

    async def handler(self, ws):
        peer = ws.remote_address[0] if ws.remote_address else "?"
        if self.args.allow and peer not in self.args.allow:
            print(f"abgelehnt: {peer}")
            await ws.close(code=1008, reason="not allowed")
            return
        print(f"verbunden: {peer}")
        try:
            async for msg in ws:
                if isinstance(msg, bytes) and is_wav(msg):
                    path = AUDIO_DIR / f"rec_{int(time.time())}.wav"
                    path.write_bytes(msg)
                    print(f"Sprache empfangen: {path} ({len(msg)} B)")
                    await self.transcribe(ws, path)
                    continue
                if isinstance(msg, bytes):
                    path = SNAP_DIR / f"snap_{int(time.time())}.jpg"
                    path.write_bytes(msg)
                    self.pending_image = path
                    print(f"Bild empfangen: {path} ({len(msg)} B)")
                    if self.args.auto_image:
                        await self.handle_prompt(ws, "")
                    continue
                try:
                    data = json.loads(msg)
                except json.JSONDecodeError:
                    data = {"t": "prompt", "text": msg}  # Rohtext als Prompt akzeptieren
                kind = data.get("t")
                if kind == "prompt":
                    await self.handle_prompt(ws, data.get("text", ""))
                elif kind == "new":
                    self.session_id = None
                    await self.send(ws, t="done")
                elif kind == "ping":
                    await self.send(ws, t="pong")
        except websockets.ConnectionClosed:
            pass
        print(f"getrennt: {peer}")


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--cols", type=int, default=60, help="Displaybreite in Zeichen")
    ap.add_argument("--claude", default="claude", help="Pfad zur Claude-Code-CLI")
    ap.add_argument("--model", default=None, help="z.B. claude-sonnet-5 fuer schnellere Antworten")
    ap.add_argument("--allow", nargs="*", default=[], help="erlaubte Client-IPs (leer = alle)")
    ap.add_argument("--auto-image", action="store_true", help="Bild sofort auswerten statt auf Prompt warten")
    ap.add_argument("--stt", default=os.environ.get("CASIO_STT"),
                    help="Befehl fuer die Spracherkennung, {file} = WAV-Datei, Text auf stdout "
                         "(z.B. \"whisper-cli -m ~/models/ggml-base.bin -l de -nt -np -f {file}\")")
    args = ap.parse_args()

    SNAP_DIR.mkdir(parents=True, exist_ok=True)
    AUDIO_DIR.mkdir(parents=True, exist_ok=True)
    WORKDIR.mkdir(parents=True, exist_ok=True)
    bridge = Bridge(args)
    async with websockets.serve(bridge.handler, args.host, args.port, max_size=8 * 1024 * 1024):
        print(f"Bridge laeuft auf ws://{args.host}:{args.port} ({args.cols} Spalten)")
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
