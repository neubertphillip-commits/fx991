#!/usr/bin/env python3
"""Simuliert den Taschenrechner: Prompts eintippen, Antwortzeilen anzeigen.

  python testclient.py ws://192.168.x.x:8765
  Eingabe  /new        neue Sitzung
  Eingabe  /img datei  JPEG schicken, danach Frage eintippen
  Eingabe  /wav datei  WAV (16 kHz mono) schicken, zeigt den erkannten Text
"""

import asyncio
import json
import sys

import websockets


async def main(url):
    async with websockets.connect(url, max_size=8 * 1024 * 1024) as ws:
        loop = asyncio.get_running_loop()
        while True:
            try:
                text = await loop.run_in_executor(None, input, "> ")
            except EOFError:
                return
            if text == "/new":
                await ws.send(json.dumps({"t": "new"}))
            elif text.startswith("/img "):
                with open(text[5:].strip(), "rb") as f:
                    await ws.send(f.read())
                print("(Bild gesendet, jetzt Frage eingeben)")
                continue
            elif text.startswith("/wav "):
                with open(text[5:].strip(), "rb") as f:
                    await ws.send(f.read())
            else:
                await ws.send(json.dumps({"t": "prompt", "text": text}))
            while True:
                msg = json.loads(await ws.recv())
                if msg["t"] == "line":
                    print("|", msg["text"])
                elif msg["t"] == "text":
                    print("erkannt:", msg["text"])
                elif msg["t"] == "err":
                    print("! ", msg["text"])
                elif msg["t"] == "done":
                    break


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1] if len(sys.argv) > 1 else "ws://127.0.0.1:8765"))
