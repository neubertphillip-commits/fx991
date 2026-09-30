#!/usr/bin/env python3
"""Casio-Deck in Originalgroesse: Panel pixelgenau hinter der Gehaeusefront.

Zeigt den Bildschirm des echten XIAO (per USB, Frame-Modus wie deckterm.py) so, wie
er spaeter hinter Display- und Solarfenster aussieht: 640x480-Panel quer, 8x16-Font,
Variante A aus CLAUDE.md (Hauptbild im Displayfenster, Status im Solarfenster).
Massstab aus der EDID des Monitors, also in echten Millimetern.

    python deckview.py [--port /dev/ttyACM0] [--ppmm 6.2]

Tasten wie deckterm/Simulator (Tab MODE, Enter EXE, Esc AC, ...). Dazu:
F1 Originalgroesse  F2 Roentgen (Gehaeuse durchsichtig)  F3/F4 Zoom -/+
F5 Panel-Pixel 1:1  F6 Lineal 50 mm  F12 Screenshot  Strg-Q beendet.
"""
import argparse
import glob
import gzip
import os
import struct
import sys
import time

import pygame

from decklink import CHAR_KEYS, HELP, Deck

# ---------------------------------------------------------------------------
# Geometrie in mm (CLAUDE.md). Ursprung: linke untere Ecke des Displayfensters, y nach oben.
# ---------------------------------------------------------------------------
PITCH = 0.0765                       # mm je Panel-Pixel (48,96 mm / 640 px)
PANEL_W, PANEL_H = 640, 480          # quer
PANEL_X, PANEL_Y = 59.0 - PANEL_W * PITCH, 0.0   # rechts buendig mit Solarfenster, unten buendig
PANEL_OUTER = (58.50, 42.62)         # Aussenkontur quer (Lage des Rands/FPC laut Datenblatt offen)
DISPLAY_WIN = (0.0, 0.0, 62.0, 25.0)
SOLAR_WIN = (24.0, 29.0, 35.0, 15.0)
CASE = (-8.0, -14.0, 77.0, 72.0)     # Ausschnitt der Front um die Fenster (Breite ~77 mm)

CELL_W, CELL_H = 8, 16
MAIN_ROWS = 20                       # 327 px sichtbar -> 20 Zeilen, unten buendig
STRIP = (183, 0, 457, 100)           # Streifen im Solarfenster (px), 57 x 6 Zeichen

FG = (225, 225, 215)
DIM = (130, 130, 125)
STATUS_FG = (120, 210, 255)
CASE_COLOR = (38, 40, 44)
CASE_EDGE = (70, 72, 78)
BG = (18, 18, 20)


# ---------------------------------------------------------------------------
# 8x16-Bitmapfont (Linux-Konsole, PSF2 mit Unicode-Tabelle)
# ---------------------------------------------------------------------------
class PsfFont:
    def __init__(self, path):
        data = gzip.open(path).read() if path.endswith(".gz") else open(path, "rb").read()
        magic, _, hdr, flags, count, size, h, w = struct.unpack("<8I", data[:32])
        if magic != 0x864AB572 or (w, h) != (CELL_W, CELL_H):
            raise ValueError(f"{path}: kein PSF2 8x16")
        self.glyphs = [data[hdr + i * size: hdr + (i + 1) * size] for i in range(count)]
        self.map = {}
        pos = hdr + count * size
        if flags & 1:
            for i in range(count):
                end = data.index(b"\xff", pos)
                entry = data[pos:end].split(b"\xfe")[0]
                for ch in entry.decode("utf-8", "ignore"):
                    self.map.setdefault(ch, i)
                pos = end + 1
        self.cache = {}

    def glyph(self, ch, color):
        key = (ch, color)
        surf = self.cache.get(key)
        if surf is None:
            idx = self.map.get(ch, self.map.get("?", 63))
            bits = self.glyphs[idx]
            surf = pygame.Surface((CELL_W, CELL_H), pygame.SRCALPHA)
            for y in range(CELL_H):
                row = bits[y]
                for x in range(CELL_W):
                    if row & (0x80 >> x):
                        surf.set_at((x, y), color)
            self.cache[key] = surf
        return surf


def find_font():
    for name in ("default8x16.psfu.gz", "lat1-16.psfu.gz", "lat9u-16.psfu.gz"):
        path = os.path.join("/usr/share/kbd/consolefonts", name)
        if os.path.exists(path):
            return PsfFont(path)
    sys.exit("Kein 8x16-Konsolenfont gefunden (/usr/share/kbd/consolefonts).")


def monitor_ppmm():
    """Pixel je mm des eingebauten/ersten Monitors aus der EDID (erster Timing-Block)."""
    for path in sorted(glob.glob("/sys/class/drm/card*-*/edid"), key=lambda p: "eDP" not in p):
        try:
            if open(os.path.join(os.path.dirname(path), "status")).read().strip() != "connected":
                continue
            e = open(path, "rb").read()
        except OSError:
            continue
        if len(e) >= 128:
            hact = e[56] | ((e[58] & 0xF0) << 4)
            hmm = e[66] | ((e[68] & 0xF0) << 4)
            if hact and hmm:
                return hact / hmm, f"{path.split('/')[-2]}: {hact} px / {hmm} mm"
    return 96 / 25.4, "unbekannt, 96 dpi angenommen"


# ---------------------------------------------------------------------------
# Panel zeichnen (was der LT7680-Treiber spaeter tun soll)
# ---------------------------------------------------------------------------
def draw_text(surf, font, x, y, text, color, max_cols):
    for i, ch in enumerate(text[:max_cols]):
        if ch != " ":
            surf.blit(font.glyph(ch, color), (x + i * CELL_W, y))


def render_panel(font, frame, connected, blink):
    panel = pygame.Surface((PANEL_W, PANEL_H))
    panel.fill((0, 0, 0))
    cols = PANEL_W // CELL_W
    top = PANEL_H - MAIN_ROWS * CELL_H
    sx, sy, sw, sh = STRIP
    strip_cols, strip_rows = sw // CELL_W, sh // CELL_H

    if frame is None or frame.off:
        msg = "Casio-Deck ist aus (Tiefschlaf)" if frame else (
            "warte auf den XIAO ..." if not connected else "warte auf Bild ...")
        draw_text(panel, font, 8, top + 3 * CELL_H, msg, DIM, cols)
        return panel

    # Streifen im Solarfenster: Statusfelder untereinander, dazu die Uhrzeit
    fields = [f.strip() for f in frame.status.split("|") if f.strip()]
    fields.append(time.strftime("%H:%M"))
    for r, text in enumerate(fields[:strip_rows]):
        draw_text(panel, font, sx, sy + r * CELL_H + 2, text, STATUS_FG, strip_cols)

    # Hauptbild: Inhalt (untere MAIN_ROWS-1 Zeilen des Frames) + Eingabezeile
    content = frame.lines
    used = max((i + 1 for i, l in enumerate(content) if l), default=0)
    first = max(0, used - (MAIN_ROWS - 1))
    for r in range(MAIN_ROWS - 1):
        i = first + r
        if i < len(content):
            draw_text(panel, font, 0, top + r * CELL_H, content[i], FG, cols)
    y = top + (MAIN_ROWS - 1) * CELL_H
    shown = frame.input[-(cols - 3):]
    draw_text(panel, font, 0, y, "> " + shown, FG, cols)
    cx = (2 + len(shown)) * CELL_W
    if frame.marked and shown:
        cx -= CELL_W
        pygame.draw.rect(panel, FG, (cx, y, CELL_W, CELL_H))
        panel.blit(font.glyph(shown[-1], (0, 0, 0)), (cx, y))
    elif blink:
        pygame.draw.rect(panel, FG, (cx, y + CELL_H - 3, CELL_W, 2))
    return panel


# ---------------------------------------------------------------------------
# Szene in mm -> Fensterpixel
# ---------------------------------------------------------------------------
class View:
    def __init__(self, ppmm):
        self.real = ppmm
        self.scale = ppmm          # Fensterpixel je mm
        self.xray = False
        self.ruler = False

    def to_px(self, x, y, origin):
        """mm (y nach oben) -> Fensterkoordinaten; origin = Fensterpos. der Fensterunterkante links."""
        return origin[0] + (x - CASE[0]) * self.scale, origin[1] - (y - CASE[1]) * self.scale

    def rect(self, x, y, w, h, origin):
        px, py = self.to_px(x, y + h, origin)
        return pygame.Rect(round(px), round(py), round(w * self.scale), round(h * self.scale))


def draw_scene(screen, view, panel, origin):
    s = view.scale
    case = view.rect(*CASE, origin)
    pygame.draw.rect(screen, CASE_COLOR, case, border_radius=int(3 * s))

    # Panel skaliert an seine echte Lage (bei Originalgroesse ~0,5 Fensterpixel je Panel-Pixel)
    prect = view.rect(PANEL_X, PANEL_Y, PANEL_W * PITCH, PANEL_H * PITCH, origin)
    scaled = pygame.transform.smoothscale(panel, prect.size) if prect.w < PANEL_W else \
        pygame.transform.scale(panel, prect.size)

    windows = [view.rect(*DISPLAY_WIN, origin), view.rect(*SOLAR_WIN, origin)]
    if view.xray:
        screen.blit(scaled, prect)
        ow, oh = PANEL_OUTER
        ox = PANEL_X - (ow - PANEL_W * PITCH) / 2
        oy = PANEL_Y - (oh - PANEL_H * PITCH) / 2
        pygame.draw.rect(screen, (200, 80, 80), view.rect(ox, oy, ow, oh, origin), 1)
        pygame.draw.rect(screen, (80, 200, 80), prect, 1)
        for w in windows:
            pygame.draw.rect(screen, (230, 230, 90), w, 1)
    else:
        for w in windows:
            pygame.draw.rect(screen, (0, 0, 0), w)
            inter = w.clip(prect)
            if inter.w > 0 and inter.h > 0:
                screen.blit(scaled, inter, inter.move(-prect.x, -prect.y))
            pygame.draw.rect(screen, CASE_EDGE, w.inflate(2, 2), 1)
    if view.ruler:
        x0, y0 = view.to_px(0, -8, origin)
        pygame.draw.line(screen, (230, 230, 230), (x0, y0), (x0 + 50 * s, y0), 2)
        for mm in range(0, 51, 10):
            pygame.draw.line(screen, (230, 230, 230), (x0 + mm * s, y0 - 5), (x0 + mm * s, y0 + 5), 1)
    return case


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--ppmm", type=float, help="Monitorpixel je mm (Standard: aus der EDID)")
    ap.add_argument("--run", help="Testablauf: 'k:TASTEN|w:SEK|shot:DATEI|F2|F5|q' (Tasten wie getippt)")
    args = ap.parse_args()

    ppmm, source = (args.ppmm, "--ppmm") if args.ppmm else monitor_ppmm()
    pygame.init()
    pygame.key.set_repeat(400, 40)
    font = find_font()
    ui = pygame.font.SysFont("DejaVu Sans Mono", 13)
    screen = pygame.display.set_mode((1100, 760), pygame.RESIZABLE)
    pygame.display.set_caption("Casio-Deck")
    view = View(ppmm)
    deck = Deck(args.port)
    edit = None
    steps = args.run.split("|") if args.run else []
    wait_until = time.time() + 3 if steps else 0
    pending = []  # Tasten aus --run, wie Tastatureingaben

    def press(ch=None, special=None):
        nonlocal edit
        if edit is not None:
            if special == "RETURN":
                deck.send(edit)
                edit = None
            elif special == "ESCAPE":
                edit = None
            elif special == "BACKSPACE":
                edit = edit[:-1] if len(edit) > 1 or not edit.startswith(":") else edit
            elif ch:
                edit += ch
            return
        if ch in (":", '"'):
            edit = ":" if ch == ":" else ""
        elif ch == "v":
            deck.voice()
        elif special:
            name = {"RETURN": "EXE", "TAB": "MODE", "ESCAPE": "AC", "BACKSPACE": "DEL",
                    "DELETE": "DEL", "UP": "UP", "DOWN": "DOWN", "LEFT": "LEFT",
                    "RIGHT": "RIGHT"}.get(special)
            if name:
                deck.key(name)
        elif ch in CHAR_KEYS:
            deck.key(CHAR_KEYS[ch])

    def fkey(n):
        if n == 1:
            view.scale = view.real
        elif n == 2:
            view.xray = not view.xray
        elif n == 3:
            view.scale /= 1.25
        elif n == 4:
            view.scale *= 1.25
        elif n == 5:
            view.scale = 1 / PITCH
        elif n == 6:
            view.ruler = not view.ruler

    clock = pygame.time.Clock()
    running = True
    while running:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            elif ev.type == pygame.TEXTINPUT:
                for ch in ev.text:
                    press(ch=ch)
            elif ev.type == pygame.KEYDOWN:
                if ev.key == pygame.K_q and ev.mod & pygame.KMOD_CTRL:
                    running = False
                elif pygame.K_F1 <= ev.key <= pygame.K_F6:
                    fkey(ev.key - pygame.K_F1 + 1)
                elif ev.key == pygame.K_F12:
                    pygame.image.save(screen, time.strftime("deckview_%H%M%S.png"))
                else:
                    special = {pygame.K_RETURN: "RETURN", pygame.K_KP_ENTER: "RETURN",
                               pygame.K_TAB: "TAB", pygame.K_ESCAPE: "ESCAPE",
                               pygame.K_BACKSPACE: "BACKSPACE", pygame.K_DELETE: "DELETE",
                               pygame.K_UP: "UP", pygame.K_DOWN: "DOWN", pygame.K_LEFT: "LEFT",
                               pygame.K_RIGHT: "RIGHT"}.get(ev.key)
                    if special:
                        press(special=special)

        # Testablauf (--run)
        if pending:
            ch = pending.pop(0)
            special = {"\n": "RETURN", "\r": "RETURN", "\t": "TAB", "\x1b": "ESCAPE",
                       "\x7f": "BACKSPACE"}.get(ch)
            press(ch=None if special else ch, special=special)
            time.sleep(0.25)
        elif steps and time.time() >= wait_until:
            step = steps.pop(0)
            kind, _, arg = step.partition(":")
            if kind == "k":
                pending = list(arg.encode().decode("unicode_escape"))
            elif kind == "w":
                wait_until = time.time() + float(arg)
            elif kind.startswith("F"):
                fkey(int(kind[1:]))
            elif kind == "shot":
                pygame.image.save(screen, arg)  # Stand des letzten Durchlaufs
            elif kind == "q":
                running = False

        frame, log = deck.snapshot()
        panel = render_panel(font, frame, deck.connected, int(time.time() * 2) % 2 == 0)
        screen.fill(BG)
        w, h = screen.get_size()
        case_w, case_h = CASE[2] * view.scale, CASE[3] * view.scale
        origin = (max(10, (w - case_w) / 2), 20 + case_h)
        draw_scene(screen, view, panel, origin)

        mode = "Originalgroesse" if abs(view.scale - view.real) < 1e-6 else (
            "Panel-Pixel 1:1" if abs(view.scale - 1 / PITCH) < 1e-6 else f"Zoom {view.scale / view.real:.2f}x")
        info = [f"{mode}  |  Monitor {view.real:.2f} px/mm ({source})  |  "
                f"{'Roentgen  |  ' if view.xray else ''}F1 echt  F2 Roentgen  F3/F4 Zoom  F5 1:1  F6 Lineal  F12 Bild",
                *HELP]
        y = origin[1] + 16
        for line in info:
            screen.blit(ui.render(line, True, DIM), (10, y))
            y += 17
        if edit is not None:
            screen.blit(ui.render("Seriell> " + edit, True, FG), (10, y))
            y += 17
        rows = max(0, int(h - y - 6) // 16)
        for line in log[-rows:] if rows else []:
            screen.blit(ui.render(line[:160], True, (95, 95, 95)), (10, y))
            y += 16
        pygame.display.flip()
        clock.tick(30)

    deck.close()
    pygame.quit()


if __name__ == "__main__":
    main()
