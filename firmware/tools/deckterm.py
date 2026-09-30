#!/usr/bin/env python3
"""Laptop als Display und Tastatur fuer den echten Casio-Deck (XIAO per USB).

Der XIAO laeuft mit der normalen Firmware (Kamera, Mikro, WLAN, Bridge echt);
dieses Programm zeigt seinen Bildschirm wie das spaetere Panel und schickt
Tastendruecke per `:key NAME`. Tastenbelegung wie im PC-Simulator (firmware/sim).

    python deckterm.py [--port /dev/ttyACM0]

Braucht pyserial. Strg-C beendet (schaltet den Frame-Modus wieder ab).
"""
import argparse
import curses
import queue
import threading
import time
from collections import deque

import serial

COLS, VIEW_ROWS = 60, 38

# Taste -> Casio-Tastenname (keyName() in keymap.cpp)
KEYS = {
    **{str(d): str(d) for d in range(10)},
    ".": ".", ",": ".", "+": "+", "-": "-", "*": "x", "/": "/", "^": "^",
    "(": "(", ")": ")", "=": "EXE", "\n": "EXE", "\r": "EXE",
    "\t": "MODE", "s": "SHIFT", "a": "ALPHA", "x": "EXP", "n": "Ans", "w": "sqrt",
    "i": "sin", "o": "cos", "t": "tan", "l": "ln", "g": "log",
}
SPECIAL = {
    curses.KEY_UP: "UP", curses.KEY_DOWN: "DOWN", curses.KEY_LEFT: "LEFT",
    curses.KEY_RIGHT: "RIGHT", curses.KEY_BACKSPACE: "DEL", curses.KEY_DC: "DEL",
    127: "DEL", 8: "DEL", 27: "AC",
}
HELP = [
    "Tab MODE  Enter EXE  Esc AC  Bksp DEL  s SHIFT  a ALPHA  s+Esc aus  Pfeile",
    "x EXP  n Ans  w sqrt  i sin  o cos  t tan  l ln  g log  v Sprache  : Befehl  \" Text",
]


class Deck:
    """Serielle Verbindung in eigenem Thread; verbindet nach Tiefschlaf/Reset neu."""

    def __init__(self, port):
        self.port = port
        self.ser = None
        self.out = queue.Queue()
        self.lock = threading.Lock()
        self.frame = None          # (status, zeilen, eingabe, markiert) oder "OFF"
        self.log = deque(maxlen=200)
        self.connected = False
        self.dirty = True
        threading.Thread(target=self._run, daemon=True).start()

    def send(self, line):
        self.out.put(line)

    def _open(self):
        try:
            s = serial.Serial(self.port, 115200, timeout=0.05)
        except (serial.SerialException, OSError):
            return None
        s.write(b"\n:frame\n")
        return s

    def _run(self):
        buf = b""
        block = None
        while True:
            if self.ser is None:
                self.ser = self._open()
                self.connected = self.ser is not None
                self.dirty = True
                if self.ser is None:
                    time.sleep(0.5)
                    continue
            try:
                while not self.out.empty():
                    self.ser.write((self.out.get() + "\n").encode())
                buf += self.ser.read(4096)
            except (serial.SerialException, OSError):
                self.ser = None
                self._addlog("[deckterm] Verbindung weg, warte auf den XIAO ...")
                continue
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").rstrip("\r")
                if block is not None:
                    if line == "\x03":
                        self._frame(block)
                        block = None
                    else:
                        block.append(line)
                elif line.startswith("\x02"):
                    block = [line[1:]]
                elif line.strip():
                    self._addlog(line)

    def _frame(self, block):
        with self.lock:
            if block[0] == "OFF":
                self.frame = "OFF"
            elif block[0] == "F" and len(block) == VIEW_ROWS + 4:
                self.frame = (block[1], block[2:2 + VIEW_ROWS], block[-2], block[-1] == "1")
            self.dirty = True

    def _addlog(self, line):
        with self.lock:
            self.log.append(line)
            self.dirty = True


def fit(text, width):
    return text[:width].ljust(width)


def draw(scr, deck, edit):
    scr.erase()
    h, w = scr.getmaxyx()
    view = max(4, min(VIEW_ROWS, h - 10))
    with deck.lock:
        frame, log = deck.frame, list(deck.log)
        deck.dirty = False

    def put(y, x, text, attr=0):
        if 0 <= y < h and x < w:
            try:
                scr.addstr(y, x, text[:max(0, w - x - 1)], attr)
            except curses.error:
                pass

    put(0, 0, "┌" + "─" * COLS + "┐")
    if frame == "OFF":
        put(1, 0, "│" + fit("", COLS) + "│")
        put(2, 0, "│" + fit("  Casio-Deck ist aus (Tiefschlaf). Taste weckt.", COLS) + "│")
        for y in range(3, view + 2):
            put(y, 0, "│" + " " * COLS + "│")
        y = view + 2
        put(y, 0, "│" + " " * COLS + "│")
    else:
        status, lines, inp, marked = frame or (
            "(warte auf den XIAO)" if not deck.connected else "(warte auf Bild)", [""] * VIEW_ROWS, "", False)
        put(1, 0, "│")
        put(1, 1, fit(status, COLS), curses.A_REVERSE)
        put(1, COLS + 1, "│")
        used = max((i + 1 for i, l in enumerate(lines) if l), default=0)
        first = max(0, used - view)
        for i in range(view):
            r = first + i
            put(2 + i, 0, "│" + fit(lines[r] if r < len(lines) else "", COLS) + "│")
        y = view + 2
        # Eingabezeile: "> " + Ende der Eingabe, Cursor bzw. markiertes Zeichen invers
        shown = inp[-(COLS - 3):]
        put(y, 0, "│" + fit("> " + shown, COLS) + "│")
        if marked and shown:
            put(y, 2 + len(shown), shown[-1], curses.A_REVERSE)
        else:
            put(y, 3 + len(shown), " ", curses.A_REVERSE)
    put(y + 1, 0, "└" + "─" * COLS + "┘")
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
            elif ch == "v":  # Spracheingabe = SHIFT+ALPHA
                deck.send(":key SHIFT")
                deck.send(":key ALPHA")
            elif ch == "\x03":
                break
            else:
                code = ord(ch) if isinstance(ch, str) and len(ch) == 1 and ord(ch) < 32 and ch not in "\t\n\r" else ch
                name = SPECIAL.get(code) or (KEYS.get(ch) if isinstance(ch, str) else None)
                if name:
                    deck.send(":key " + name)
        if deck.dirty or time.time() - last > 1:
            draw(scr, deck, edit)
            last = time.time()
        if ch is None:
            time.sleep(0.02)
    deck.send(":frame 0")
    time.sleep(0.2)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", default="/dev/ttyACM0")
    args = ap.parse_args()
    try:
        curses.wrapper(main, args.port)
    except KeyboardInterrupt:
        pass
