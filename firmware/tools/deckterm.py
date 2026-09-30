#!/usr/bin/env python3
"""Laptop als Display und Tastatur fuer den echten Casio-Deck (XIAO per USB), im Terminal.

Der XIAO laeuft mit der normalen Firmware (Kamera, Mikro, WLAN, Bridge echt);
dieses Programm zeigt seinen Bildschirm als Text und schickt Tastendruecke per
`:key NAME`. Tastenbelegung wie im PC-Simulator (firmware/sim). Pixelgenau in
Originalgroesse: deckview.py.

    python deckterm.py [--port /dev/ttyACM0]

Braucht pyserial. Strg-C beendet (schaltet den Frame-Modus wieder ab).
"""
import argparse
import curses
import time

from decklink import CHAR_KEYS, HELP, Deck

SPECIAL = {
    curses.KEY_UP: "UP", curses.KEY_DOWN: "DOWN", curses.KEY_LEFT: "LEFT",
    curses.KEY_RIGHT: "RIGHT", curses.KEY_BACKSPACE: "DEL", curses.KEY_DC: "DEL",
    127: "DEL", 8: "DEL", 27: "AC",
}


def fit(text, width):
    return text[:width].ljust(width)


def draw(scr, deck, edit):
    scr.erase()
    h, w = scr.getmaxyx()
    frame, log = deck.snapshot()
    cols = frame.cols if frame and frame.cols else 80
    rows = len(frame.lines) if frame and not frame.off else 20
    view = max(4, min(rows, h - 10))

    def put(y, x, text, attr=0):
        if 0 <= y < h and x < w:
            try:
                scr.addstr(y, x, text[:max(0, w - x - 1)], attr)
            except curses.error:
                pass

    put(0, 0, "┌" + "─" * cols + "┐")
    if frame and frame.off:
        lines = ["", "  Casio-Deck ist aus (Tiefschlaf). Taste weckt."]
        put(1, 0, "│" + " " * cols + "│")
        for i in range(view):
            put(2 + i, 0, "│" + fit(lines[i] if i < len(lines) else "", cols) + "│")
        y = view + 2
        put(y, 0, "│" + " " * cols + "│")
    else:
        if frame is None:
            status = "(warte auf den XIAO)" if not deck.connected else "(warte auf Bild)"
            lines, inp, marked = [], "", False
        else:
            status, lines, inp, marked = frame.status, frame.lines, frame.input, frame.marked
        put(1, 0, "│")
        put(1, 1, fit(status, cols), curses.A_REVERSE)
        put(1, cols + 1, "│")
        used = max((i + 1 for i, l in enumerate(lines) if l), default=0)
        first = max(0, used - view)
        for i in range(view):
            r = first + i
            put(2 + i, 0, "│" + fit(lines[r] if r < len(lines) else "", cols) + "│")
        y = view + 2
        # Eingabezeile: "> " + Ende der Eingabe, Cursor bzw. markiertes Zeichen invers
        shown = inp[-(cols - 3):]
        put(y, 0, "│" + fit("> " + shown, cols) + "│")
        if marked and shown:
            put(y, 2 + len(shown), shown[-1], curses.A_REVERSE)
        else:
            put(y, 3 + len(shown), " ", curses.A_REVERSE)
    put(y + 1, 0, "└" + "─" * cols + "┘")
    put(y + 2, 0, HELP[0], curses.A_DIM)
    put(y + 3, 0, HELP[1], curses.A_DIM)
    log_rows = max(0, h - (y + 5))
    for i, line in enumerate(log[-log_rows:] if log_rows else []):
        put(y + 4 + i, 0, line, curses.A_DIM)
    if edit is not None:
        put(h - 1, 0, "Seriell> " + edit, curses.A_BOLD)
    scr.refresh()


def main(scr, port):
    curses.curs_set(0)
    curses.set_escdelay(25)
    scr.nodelay(True)
    scr.keypad(True)
    deck = Deck(port)
    edit = None
    last = 0.0
    while True:
        try:
            ch = scr.get_wch()
        except curses.error:
            ch = None
        except KeyboardInterrupt:
            break
        if ch is not None:
            if edit is not None:
                if ch in ("\n", "\r", curses.KEY_ENTER):
                    deck.send(edit)
                    edit = None
                elif ch in (27, "\x1b"):
                    edit = None
                elif ch in (curses.KEY_BACKSPACE, 127, 8, "\x7f", "\b"):
                    edit = edit[:-1] if len(edit) > 1 or not edit.startswith(":") else edit
                elif isinstance(ch, str) and ch.isprintable():
                    edit += ch
                deck.dirty = True
            elif ch in (":", '"'):
                edit = ":" if ch == ":" else ""
                deck.dirty = True
            elif ch == "v":
                deck.voice()
            elif ch == "\x03":
                break
            else:
                code = ord(ch) if isinstance(ch, str) and len(ch) == 1 and ord(ch) < 32 and ch not in "\t\n\r" else ch
                name = SPECIAL.get(code) or (CHAR_KEYS.get(ch) if isinstance(ch, str) else None)
                if name:
                    deck.key(name)
        if deck.dirty or time.time() - last > 1:
            draw(scr, deck, edit)
            last = time.time()
        if ch is None:
            time.sleep(0.02)
    deck.close()


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", default="/dev/ttyACM0")
    args = ap.parse_args()
    try:
        curses.wrapper(main, args.port)
    except KeyboardInterrupt:
        pass
