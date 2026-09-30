"""Erzeugt die Tastaturplatine als KiCad-Datei (KiCad 7, Python-Modul pcbnew).

Ersetzt die Casio-Platine: gleicher Umriss, Tastenkontakte als vergoldete Kaemme
(ENIG) unter den Kohlenoppen der Matte, MCP23017 auf der Rueckseite, echte Matrix
9 Zeilen x 7 Spalten. Geometrie aus tastatur_geometrie.json.

    python3 tastatur_pcb.py              -> tastatur.kicad_pcb (unverdrahtet) + tastatur.dsn
    java -jar freerouting.jar -de tastatur.dsn -do tastatur.ses
    python3 tastatur_pcb.py --ses tastatur.ses  -> Leiterbahnen einlesen, DRC-Bericht

Matrix (Zeilen = MCP-Ausgaenge, Spalten = Eingaenge mit Pull-up):
  Spalten C0..C6 = GPA0..GPA6, Zeilen R0 = GPA7, R1..R8 = GPB0..GPB7.
  Welche Taste wo liegt: MATRIX unten (auch fuer die Firmware).
"""

import json
import math
import re
import sys
from pathlib import Path

import pcbnew

HERE = Path(__file__).parent
MM = pcbnew.FromMM
BOARD_THICKNESS = 0.6
BOARD_W, BOARD_H = 65.45, 98.4  # aus tastatur_geometrie.json, in build() gesetzt

# Tastenkamm: Aussenring je Haelfte + waagerechte Finger, abwechselnd
PAD_MAX = 6.4   # Kontakt-Durchmesser hoechstens (Noppe 4 mm, +-1,2 mm Spiel)
EDGE = 0.6      # Abstand Kontakt - Platinenrand
RING = 0.5      # Breite des Randes
GAP = 0.25      # Abstand zwischen den Haelften
FINGER = 0.35   # Fingerbreite

# Zeilen: physische Reihen; Spalten nach x-Position (siehe Docstring)
ROWS = [
    [1, 2, 6, 5, 7, 3, 4],
    [9, 10, 8, 11, 12],
    [13, 14, 15, 16, 17, 18],
    [19, 20, 21, 22, 23, 24],
    [25, 26, 27, 28, 29, 30],
    [31, 32, 33, 34, 35],
    [36, 37, 38, 39, 40],
    [41, 42, 43, 44, 45],
    [46, 47, 48, 49, 50],
]
COLS_OF_ROW = [
    [0, 1, 2, 3, 4, 5, 6],
    [0, 1, 3, 4, 5],
    [0, 1, 2, 3, 4, 5],
    [0, 1, 2, 3, 4, 5],
    [0, 1, 2, 3, 4, 5],
    [0, 1, 2, 4, 5],
    [0, 1, 2, 4, 5],
    [0, 1, 2, 4, 5],
    [0, 1, 2, 4, 5],
]
MATRIX = {k: (r, c) for r, (keys, cols) in enumerate(zip(ROWS, COLS_OF_ROW))
          for k, c in zip(keys, cols)}
assert len(MATRIX) == 50 and len(set(MATRIX.values())) == 50

# MCP23017 SOIC-28: Pin -> Netz
MCP_PINS = {9: "3V3", 10: "GND", 12: "SCL", 13: "SDA", 15: "GND", 16: "GND", 17: "GND",
            18: "3V3", 20: "INT"}
for i in range(7):
    MCP_PINS[21 + i] = f"C{i}"          # GPA0..GPA6
MCP_PINS[28] = "R0"                     # GPA7
for i in range(8):
    MCP_PINS[1 + i] = f"R{i + 1}"       # GPB0..GPB7
WIRES = ["3V3", "GND", "SDA", "SCL", "INT"]


OX, OY = 115.0, 56.0  # Lage der Platine auf dem A4-Blatt (nur Optik in KiCad)


def v(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))


def va(x, y):
    """Absolute Position: Platinenkoordinaten (mm, Ursprung oben links) auf dem Blatt."""
    return v(x + OX, y + OY)


def place_pad(pad, x, y):
    """Pad relativ zum Footprint setzen (Footprint steht beim Aufbau im Ursprung)."""
    pad.SetPos0(v(x, y))
    pad.SetPosition(v(x, y))


def arc_pts(r, a0, a1, n=24):
    return [(r * math.cos(a0 + (a1 - a0) * i / n), r * math.sin(a0 + (a1 - a0) * i / n))
            for i in range(n + 1)]


def comb_halves(d):
    """Polygone (lokale Koordinaten, Mitte = 0,0) der zwei Kammhaelften."""
    R = d / 2
    Ri = R - RING
    halves = {0: [], 1: []}  # 0 = links (Zeile), 1 = rechts (Spalte)
    t0, ti = math.acos(-GAP / 2 / R), math.acos(-GAP / 2 / Ri)
    left = arc_pts(R, t0, 2 * math.pi - t0) + arc_pts(Ri, 2 * math.pi - ti, ti)
    halves[0].append(left)
    halves[1].append([(-x, y) for x, y in left])
    pitch = FINGER + GAP
    n = int((2 * Ri - GAP) // pitch)
    y = -(n - 1) * pitch / 2
    for i in range(n):
        y0, y1 = y - FINGER / 2, y + FINGER / 2
        yo, yi = max(abs(y0), abs(y1)), min(abs(y0), abs(y1))
        if yo < Ri:
            if yo >= Ri - GAP:
                y += pitch
                continue
            reach = math.sqrt((Ri - GAP) ** 2 - yo * yo)   # radial GAP bis zum anderen Rand
            root = math.sqrt(max(Ri * Ri - yi * yi, 0)) + 0.05  # in den eigenen Rand
            if reach + root > 1.0:
                side = i % 2
                s = -1 if side == 0 else 1
                xs = [s * root, -s * reach]
                halves[side].append([(xs[0], y0), (xs[1], y0), (xs[1], y1), (xs[0], y1)])
        y += pitch
    return halves


OUTLINE = []  # Umriss (Platinenkoordinaten), in build() gesetzt


def dist_to_outline(x, y):
    best = 1e9
    for (x0, y0), (x1, y1) in zip(OUTLINE, OUTLINE[1:] + OUTLINE[:1]):
        dx, dy = x1 - x0, y1 - y0
        t = max(0, min(1, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy or 1)))
        best = min(best, math.hypot(x - x0 - t * dx, y - y0 - t * dy))
    return best


def pad_diameter(key):
    edge = dist_to_outline(key["x"], key["y"]) - EDGE
    return round(min(key["d"], PAD_MAX, 2 * edge), 2)


def add_key(board, nets, key):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(f"K{key['nr']}")
    fp.SetValue("Taste")
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    r, c = MATRIX[key["nr"]]
    d = pad_diameter(key)
    halves = comb_halves(d)
    R = d / 2
    for side, net in ((0, f"R{r}"), (1, f"C{c}")):
        pad = pcbnew.PAD(fp)
        pad.SetNumber(str(side + 1))
        pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
        pad.SetShape(pcbnew.PAD_SHAPE_CUSTOM)
        pad.SetAnchorPadShape(pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(v(0.3, 0.3))
        lset = pcbnew.LSET()
        lset.AddLayer(pcbnew.F_Cu)
        lset.AddLayer(pcbnew.F_Mask)
        pad.SetLayerSet(lset)
        ax = -(R - RING / 2) if side == 0 else (R - RING / 2)
        place_pad(pad, ax, 0)
        for poly in halves[side]:
            pts = pcbnew.VECTOR_VECTOR2I()
            for x, y in poly:
                pts.append(v(x - ax, y))
            pad.AddPrimitivePoly(pts, 0, True)
        pad.SetNet(nets[net])
        fp.Add(pad)
    fp.SetPosition(va(key["x"], key["y"]))
    board.Add(fp)


def add_hole(board, h, ref):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    pad = pcbnew.PAD(fp)
    pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
    pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    d = h.get("d_entwurf", h["d"])
    pad.SetSize(v(d, d))
    pad.SetDrillSize(v(d, d))
    pad.SetLayerSet(pad.UnplatedHoleMask())
    place_pad(pad, 0, 0)
    fp.Add(pad)
    fp.SetPosition(va(h["x"], h["y"]))
    board.Add(fp)
    # Sperrflaeche um das Loch, sonst legt Freerouting Bahnen hindurch
    zone = pcbnew.ZONE(board)
    zone.SetIsRuleArea(True)
    zone.SetDoNotAllowTracks(True)
    zone.SetDoNotAllowVias(True)
    zone.SetDoNotAllowCopperPour(True)
    zone.SetDoNotAllowPads(False)
    zone.SetDoNotAllowFootprints(False)
    lset = pcbnew.LSET()
    lset.AddLayer(pcbnew.F_Cu)
    lset.AddLayer(pcbnew.B_Cu)
    zone.SetLayerSet(lset)
    r = d / 2 + 0.5
    outline = zone.Outline()
    outline.NewOutline()
    for i in range(24):
        a = 2 * math.pi * i / 24
        outline.Append(va(h["x"] + r * math.cos(a), h["y"] + r * math.sin(a)))
    board.Add(zone)


def smd(fp, num, x, y, w, h, net, nets, back=True):
    """SMD-Pad. back=True: Pad auf der Rueckseite, Lage wie von vorn durch die Platine
    gesehen (gespiegelt). Die Bauteile werden nicht per Flip umgedreht, weil
    Freerouting die Lage der Pads umgedrehter Bauteile falsch uebernimmt."""
    if back:
        y = -y
    pad = pcbnew.PAD(fp)
    pad.SetNumber(str(num))
    pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
    pad.SetShape(pcbnew.PAD_SHAPE_ROUNDRECT)
    pad.SetRoundRectRadiusRatio(0.25)
    pad.SetSize(v(w, h))
    lset = pcbnew.LSET()
    for layer in ((pcbnew.B_Cu, pcbnew.B_Paste, pcbnew.B_Mask) if back else
                  (pcbnew.F_Cu, pcbnew.F_Paste, pcbnew.F_Mask)):
        lset.AddLayer(layer)
    pad.SetLayerSet(lset)
    place_pad(pad, x, y)
    if net:
        pad.SetNet(nets[net])
    fp.Add(pad)
    return pad


def part(board, ref, value, x, y):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    fp.target = (x, y)
    return fp


def finish_back(board, fp, rot):
    """Bauteil an seinen Platz setzen (Pads liegen schon auf B.Cu, siehe smd())."""
    fp.SetPosition(va(*fp.target))
    fp.SetOrientationDegrees(rot)
    board.Add(fp)


def silk_text(board, text, x, y, size=1.0, layer=pcbnew.B_SilkS, mirror=True):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(va(x, y))
    t.SetLayer(layer)
    t.SetTextSize(v(size, size))
    t.SetTextThickness(MM(size * 0.15))
    t.SetMirrored(mirror)
    board.Add(t)


def build():
    global BOARD_W, BOARD_H
    geo = json.loads((HERE / "tastatur_geometrie.json").read_text())
    BOARD_W, BOARD_H = geo["platine"]["breite"], geo["platine"]["hoehe"]
    OUTLINE[:] = [tuple(p) for p in geo["umriss"]]
    board = pcbnew.BOARD()
    ds = board.GetDesignSettings()
    ds.SetBoardThickness(MM(BOARD_THICKNESS))
    ds.m_TrackMinWidth = MM(0.15)  # JLCPCB kann 0,127
    ds.m_MinClearance = MM(0.2)
    ds.m_ViasMinSize = MM(0.6)
    ds.m_MinThroughDrill = MM(0.3)
    ds.m_CopperEdgeClearance = MM(0.4)
    ds.SetCopperLayerCount(2)
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetTrackWidth(MM(0.25))
    nc.SetClearance(MM(0.2))
    nc.SetViaDiameter(MM(0.6))
    nc.SetViaDrill(MM(0.3))

    names = ["GND", "3V3", "SDA", "SCL", "INT"] + [f"R{i}" for i in range(9)] + \
            [f"C{i}" for i in range(7)]
    nets = {}
    for n in names:
        ni = pcbnew.NETINFO_ITEM(board, n)
        board.Add(ni)
        nets[n] = ni

    # Umriss
    pts = geo["umriss"]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(va(x0, y0))
        seg.SetEnd(va(x1, y1))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(MM(0.1))
        board.Add(seg)

    for i, h in enumerate(geo["loecher"]):
        add_hole(board, h, f"H{i + 1}")
    for k in geo["tasten"]:
        add_key(board, nets, k)

    W = geo["platine"]["breite"]
    # MCP23017 (SOIC-28 breit) waagerecht auf der Rueckseite, oberes Drittel
    # Pads gleich in Endlage (waagerecht) anlegen, Drehung 0: KiCad und Freerouting
    # rechnen 90-Grad-Drehungen auf der Rueckseite unterschiedlich.
    mcp = part(board, "U1", "MCP23017-E/SO", W / 2, 24)
    for p in range(1, 29):
        if p <= 14:
            x, y = -8.255 + (p - 1) * 1.27, 4.65
        else:
            x, y = 8.255 - (p - 15) * 1.27, -4.65
        smd(mcp, p, x, y, 0.6, 2.0, MCP_PINS.get(p), nets)
    finish_back(board, mcp, 0)
    dot = pcbnew.PCB_SHAPE(board)
    dot.SetShape(pcbnew.SHAPE_T_CIRCLE)
    dot.SetCenter(va(W / 2 - 8.255 - 1.2, 24 - 6.2))
    dot.SetEnd(va(W / 2 - 8.255 - 1.2 + 0.3, 24 - 6.2))
    dot.SetFilled(True)
    dot.SetLayer(pcbnew.B_SilkS)
    board.Add(dot)
    silk_text(board, "U1 Pin 1", W / 2 - 8.255 + 2.8, 24 - 6.9, 0.8)

    c1 = part(board, "C1", "100nF", W / 2 + 12.5, 24)
    smd(c1, 1, 0, -0.95, 1.3, 1.0, "3V3", nets)
    smd(c1, 2, 0, 0.95, 1.3, 1.0, "GND", nets)
    finish_back(board, c1, 0)
    for ref, net, dx in (("R1", "SDA", -12.5), ("R2", "SCL", -15)):
        r = part(board, ref, "4.7k", W / 2 + dx, 24)
        smd(r, 1, 0, -0.95, 1.3, 1.0, "3V3", nets)
        smd(r, 2, 0, 0.95, 1.3, 1.0, net, nets)
        finish_back(board, r, 0)

    # Kabelpads zum XIAO, Rueckseite, oberer Rand
    j = part(board, "J1", "XIAO", W / 2, 5)
    for i, net in enumerate(WIRES):
        smd(j, i + 1, (i - 2) * 3.5, 0, 2.2, 3.0, net, nets)
    finish_back(board, j, 0)
    for i, net in enumerate(WIRES):
        silk_text(board, net, W / 2 + (i - 2) * 3.5, 8.2, 0.9)
    silk_text(board, "Casio-Deck Tastatur v1", W / 2, 40, 1.2)
    silk_text(board, "MCP23017 0x20", W / 2, 43, 0.9)
    return board


def sexpr(text):
    """Minimaler S-Ausdruck-Parser fuer die SES-Datei von Freerouting."""
    tokens = re.findall(r'\(|\)|"[^"]*"|[^\s()]+', text)
    stack = [[]]
    for t in tokens:
        if t == "(":
            stack.append([])
        elif t == ")":
            done = stack.pop()
            stack[-1].append(done)
        else:
            stack[-1].append(t.strip('"'))
    return stack[0][0]


def find(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]


def import_ses(board, text):
    """Leiterbahnen und Vias aus Freeroutings SES uebernehmen (KiCad 7 kann das per
    Python nur im GUI). DSN/SES: y nach oben, Einheit resolution (um 10 = 0,1 um)."""
    for t in list(board.GetTracks()):
        board.Remove(t)
    root = sexpr(text)
    routes = find(root, "routes")[0]
    res = find(routes, "resolution")[0]
    per_mm = float(res[2]) * (1000.0 if res[1] == "um" else 1.0)
    layers = {"F.Cu": pcbnew.F_Cu, "B.Cu": pcbnew.B_Cu}

    def pt(x, y):
        return pcbnew.VECTOR2I(MM(float(x) / per_mm), MM(-float(y) / per_mm))

    n_tr = n_via = 0
    for net in find(find(routes, "network_out")[0], "net"):
        ni = board.FindNet(net[1])
        for wire in find(net, "wire"):
            path = find(wire, "path")[0]
            layer, width, coords = path[1], float(path[2]) / per_mm, path[3:]
            xy = [pt(coords[i], coords[i + 1]) for i in range(0, len(coords) - 1, 2)]
            for a, b in zip(xy, xy[1:]):
                tr = pcbnew.PCB_TRACK(board)
                tr.SetStart(a)
                tr.SetEnd(b)
                tr.SetWidth(MM(width))
                tr.SetLayer(layers[layer])
                tr.SetNet(ni)
                board.Add(tr)
                n_tr += 1
        for via in find(net, "via"):
            m = re.search(r"(\d+):(\d+)_um", via[1])
            vi = pcbnew.PCB_VIA(board)
            vi.SetPosition(pt(via[2], via[3]))
            vi.SetWidth(MM(int(m.group(1)) / 1000 if m else 0.6))
            vi.SetDrill(MM(int(m.group(2)) / 1000 if m else 0.3))
            vi.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            vi.SetNet(ni)
            board.Add(vi)
            n_via += 1
    return n_tr, n_via


def main():
    if "--ses" in sys.argv:
        board = pcbnew.LoadBoard(str(HERE / "tastatur.kicad_pcb"))
        ses = sys.argv[sys.argv.index("--ses") + 1]
        n_tr, n_via = import_ses(board, Path(ses).read_text())
        print(f"SES eingelesen: {n_tr} Leiterbahnen, {n_via} Vias")
        pcbnew.SaveBoard(str(HERE / "tastatur.kicad_pcb"), board)
        return
    board = build()
    pcbnew.SaveBoard(str(HERE / "tastatur.kicad_pcb"), board)
    ok = pcbnew.ExportSpecctraDSN(board, str(HERE / "tastatur.dsn"))
    print("tastatur.kicad_pcb geschrieben, DSN:", ok)


if __name__ == "__main__":
    main()
