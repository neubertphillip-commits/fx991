"""Serielle Verbindung zum echten Casio-Deck (XIAO per USB) fuer deckterm/deckview.

Schaltet die Firmware per `:frame` in den Frame-Modus, liest die Bildschirm-Bloecke
(Format in casio_deck/display.h) und die uebrigen Log-Zeilen, schickt Tasten per
`:key NAME`. Laeuft in einem eigenen Thread und verbindet nach Tiefschlaf/Reset neu.
"""
import queue
import threading
import time
from collections import deque
from dataclasses import dataclass, field

import serial

# Laptop-Taste -> Casio-Tastenname (keyName() in keymap.cpp), wie im PC-Simulator
CHAR_KEYS = {
    **{str(d): str(d) for d in range(10)},
    ".": ".", ",": ".", "+": "+", "-": "-", "*": "x", "/": "/", "^": "^",
    "(": "(", ")": ")", "=": "EXE", "\n": "EXE", "\r": "EXE",
    "\t": "MODE", "s": "SHIFT", "a": "ALPHA", "x": "EXP", "n": "Ans", "w": "sqrt",
    "i": "sin", "o": "cos", "t": "tan", "l": "ln", "g": "log",
}
HELP = [
    "Tab MODE  Enter EXE  Esc AC  Bksp DEL  s SHIFT  a ALPHA  s+Esc aus  Pfeile",
    "x EXP  n Ans  w sqrt  i sin  o cos  t tan  l ln  g log  v Sprache  : Befehl  \" Text",
]


@dataclass
class Frame:
    cols: int
    status: str
    lines: list = field(default_factory=list)
    input: str = ""
    marked: bool = False
    off: bool = False


class Deck:
    def __init__(self, port="/dev/ttyACM0"):
        self.port = port
        self.ser = None
        self.out = queue.Queue()
        self.lock = threading.Lock()
        self.frame = None
        self.log = deque(maxlen=200)
        self.connected = False
        self.dirty = True
        threading.Thread(target=self._run, daemon=True).start()

    def send(self, line):
        self.out.put(line)

    def key(self, name):
        self.send(":key " + name)

    def voice(self):  # Spracheingabe = SHIFT+ALPHA
        self.key("SHIFT")
        self.key("ALPHA")

    def close(self):
        self.send(":frame 0")
        time.sleep(0.2)

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
                self._addlog("[deck] Verbindung weg, warte auf den XIAO ...")
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
        head = block[0].split()
        with self.lock:
            if head[:1] == ["OFF"]:
                self.frame = Frame(cols=self.frame.cols if self.frame else 0, status="", off=True)
            elif head[:1] == ["F"] and len(head) == 3:
                cols, rows = int(head[1]), int(head[2])
                if len(block) == rows + 4:
                    self.frame = Frame(cols, block[1], block[2:2 + rows], block[-2], block[-1] == "1")
            self.dirty = True

    def _addlog(self, line):
        with self.lock:
            self.log.append(line)
            self.dirty = True

    def snapshot(self):
        with self.lock:
            self.dirty = False
            return self.frame, list(self.log)
