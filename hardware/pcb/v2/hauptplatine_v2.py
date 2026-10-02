"""Hauptplatine V2 (Entwurf): Tastaturplatine + XIAO ESP32S3 + Kamera + Mikro auf einer Platine.

Ersetzt die Sense-Platine: Der XIAO liegt mit der Unterseite zur Platine auf deren Rueckseite
und steckt mit seinem B2B-Stecker in einer Hirose-DF40-Buchse (1,5 mm Stapelhoehe). Kamera
(OV3660) per 24-pol. FPC-Buchse, PDM-Mikro und die zwei Kamera-LDOs sitzen auf der Platine.
Tasten, Umriss, Loecher und MCP23017 wie V1 (../tastatur_pcb.py, Funktionen werden importiert).

Koordinaten: mm, Platine von vorn (Tastenseite), Ursprung oben links, y nach unten. Bauteile der
Rueckseite werden mit ihrer Lage *von vorn gesehen* angegeben (nicht geflippt, siehe V1).

    python3 hauptplatine_v2.py                      -> v2.kicad_pcb + v2.dsn
    java -jar freerouting.jar -de v2.dsn -do v2.ses -mp 60
    python3 hauptplatine_v2.py --ses v2.ses         -> Leiterbahnen einlesen

ENTWURF: Vor einer Bestellung muessen die Punkte in README.md ("Vor der Bestellung") geklaert sein.
"""

import json
import sys
from pathlib import Path

import pcbnew

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent))
import tastatur_pcb as tp  # noqa: E402

MM = pcbnew.FromMM
v, va = tp.v, tp.va

# --- XIAO-Lage (von vorn gesehen = XIAO von unten gesehen) -------------------------------
# Seeed-Footprint XIAO-ESP32-S3-SMD: x 0..17,8 (Draufsicht XIAO), y 0 (Ende gegenueber USB)
# bis -21 (USB). Von vorn gesehen ist x gespiegelt: Platine x = XR - fx, y = YB + fy.
XR = 32.725 + 8.9   # rechte Kante des XIAO (von vorn), XIAO mittig
YB = 24.0           # Ende gegenueber USB; USB zeigt nach oben (Platinenoberkante)
XIAO_W, XIAO_L = 17.8, 21.0

# B2B-Buchse: Mitte laut Seeeds Sense-Platine 11,05 mm vom linken, 2,31 mm vom unteren Rand
# (Draufsicht Sense = XIAO von unten). ANNAHME: "unten" = Ende gegenueber USB. Nachmessen!
B2B_X = XR - XIAO_W + 11.05
B2B_Y = YB - 2.31
B2B_PITCH = 0.4
B2B_ROW = 2.2       # Abstand der Padreihen (Mitte-Mitte) - ANNAHME, Hirose-Zeichnung pruefen
B2B_PAD = (0.23, 0.8)

# B2B-Belegung (Seeed Sense JA3), Pin -> Netz (None = frei)
B2B_PINS = {
    1: None, 2: "GND", 3: "GND", 4: "PDM_CLK", 5: "PDM_DATA", 6: "CAM_SDA", 7: "CAM_SCL",
    8: "VSYNC", 9: "HREF", 10: "Y9", 11: None, 12: "3V3", 13: "3V3", 14: "GND", 15: None,
    16: None, 17: "Y4", 18: "Y3", 19: "Y5", 20: "Y2", 21: "Y6", 22: "PCLK", 23: "Y7",
    24: "Y8", 25: "XCLK", 26: "MOSI", 27: "MISO", 28: "SCK", 29: "LCD_RST", 30: "GND",
}
# Pin 1/16 = VIN, 15 = IO21 (LED), 29 = D2 (bei Seeed SD_CS, hier LT7680-Reset)

# XIAO-Kantenpads, die per Drahtbruecke (1,5 mm Spalt) auf die Platine gehen: Footprint-Pad -> Netz
XIAO_BRIDGES = {1: "INT", 2: "LCD_CS", 4: "VBAT_SENSE", 5: "SDA", 6: "SCL", 14: "5V",
                23: "BAT1", 24: "BAT2"}
XIAO_PAD_POS = {i: (0.835, -18.12 + (i - 1) * 2.54) for i in range(1, 8)}
XIAO_PAD_POS.update({i: (17.0, -2.88 - (i - 8) * 2.54) for i in range(8, 15)})
XIAO_PAD_POS.update({23: (4.445, -10.882), 24: (4.445, -12.787)})

# Kamera-FPC (24 pol., 0,5 mm), Pin -> Netz. Standardbelegung OV2640/OV3660-Module.
FPC_PINS = {
    1: None, 2: "GND", 3: "CAM_SDA", 4: "AVDD", 5: "CAM_SCL", 6: "CAM_RST", 7: "VSYNC",
    8: "CAM_PWDN", 9: "HREF", 10: "DVDD", 11: "2V8", 12: "Y9", 13: "XCLK", 14: "Y8",
    15: "GND", 16: "Y7", 17: "PCLK", 18: "Y6", 19: "Y2", 20: "Y5", 21: "Y3", 22: "Y4",
    23: None, 24: None,
}

LCD_PADS = ["3V3", "GND", "SCK", "MOSI", "MISO", "LCD_CS", "LCD_RST", None]  # None = WAIT (frei)
LCD_LABELS = ["3V3", "GND", "SCK", "MO", "MI", "CS", "RST", "WT"]


def bpad(fp, num, dx, dy, w, h, net, nets, shape="rr", layer="B"):
    """Pad relativ zum Footprint, Lage von vorn gesehen (keine Spiegelung)."""
    pad = pcbnew.PAD(fp)
    pad.SetNumber(str(num))
    pad.SetAttribute(pcbnew.PAD_ATTRIB_SMD)
    if shape == "rr":
        pad.SetShape(pcbnew.PAD_SHAPE_ROUNDRECT)
        pad.SetRoundRectRadiusRatio(0.25)
    else:
        pad.SetShape(pcbnew.PAD_SHAPE_RECT)
    pad.SetSize(v(w, h))
    lset = pcbnew.LSET()
    for lay in ((pcbnew.B_Cu, pcbnew.B_Paste, pcbnew.B_Mask) if layer == "B" else
                (pcbnew.F_Cu, pcbnew.F_Paste, pcbnew.F_Mask)):
        lset.AddLayer(lay)
    pad.SetLayerSet(lset)
    tp.place_pad(pad, dx, dy)
    if net:
        pad.SetNet(nets[net])
    fp.Add(pad)
    return pad


def comp(board, ref, value, x, y, pads, nets):
    """Bauteil auf der Rueckseite: pads = [(nr, dx, dy, w, h, netz)]."""
    fp = tp.part(board, ref, value, x, y)
    for nr, dx, dy, w, h, net in pads:
        bpad(fp, nr, dx, dy, w, h, net, nets)
    tp.finish_back(board, fp, 0)
    return fp


def two(board, ref, value, x, y, n1, n2, nets, vertical=False, size="0603"):
    d, (w, h) = {"0603": (0.8, (0.9, 0.95)), "0805": (0.95, (1.0, 1.3))}[size]
    if vertical:
        pads = [(1, 0, -d, h, w, n1), (2, 0, d, h, w, n2)]
    else:
        pads = [(1, -d, 0, w, h, n1), (2, d, 0, w, h, n2)]
    return comp(board, ref, value, x, y, pads, nets)


def sot23_5(board, ref, value, x, y, vin, vout, nets):
    """LDO (z. B. ME6211): 1 VIN, 2 GND, 3 EN, 4 NC, 5 VOUT."""
    pads = [(1, -0.95, 1.3, 0.6, 1.1, vin), (2, 0, 1.3, 0.6, 1.1, "GND"),
            (3, 0.95, 1.3, 0.6, 1.1, vin), (4, 0.95, -1.3, 0.6, 1.1, None),
            (5, -0.95, -1.3, 0.6, 1.1, vout)]
    return comp(board, ref, value, x, y, pads, nets)


def silk_rect(board, x0, y0, x1, y1, layer=pcbnew.B_SilkS, w=0.12):
    for (a, b), (c, d) in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)),
                           ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(va(a, b))
        s.SetEnd(va(c, d))
        s.SetLayer(layer)
        s.SetWidth(MM(w))
        board.Add(s)


def build():
    geo = json.loads((HERE.parent / "tastatur_geometrie.json").read_text())
    tp.BOARD_W, tp.BOARD_H = geo["platine"]["breite"], geo["platine"]["hoehe"]
    tp.OUTLINE[:] = [tuple(p) for p in geo["umriss"]]
    W = tp.BOARD_W
    board = pcbnew.BOARD()
    ds = board.GetDesignSettings()
    ds.SetBoardThickness(MM(0.8))
    ds.m_TrackMinWidth = MM(0.13)
    ds.m_MinClearance = MM(0.13)       # JLCPCB: 0,127 mm (5 mil)
    ds.m_ViasMinSize = MM(0.5)
    ds.m_MinThroughDrill = MM(0.3)
    ds.m_CopperEdgeClearance = MM(0.4)
    ds.SetCopperLayerCount(2)
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetTrackWidth(MM(0.2))
    nc.SetClearance(MM(0.13))
    nc.SetViaDiameter(MM(0.5))
    nc.SetViaDrill(MM(0.3))

    names = {"GND", "3V3", "SDA", "SCL", "INT", "5V", "QI", "BAT1", "BAT2", "VBAT_SENSE",
             "AVDD", "DVDD", "2V8", "CAM_RST", "CAM_PWDN"}
    names |= {f"R{i}" for i in range(9)} | {f"C{i}" for i in range(7)}
    names |= {n for n in B2B_PINS.values() if n} | {n for n in FPC_PINS.values() if n}
    names |= {n for n in LCD_PADS if n}
    nets = {}
    for n in sorted(names):
        ni = pcbnew.NETINFO_ITEM(board, n)
        board.Add(ni)
        nets[n] = ni

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
        tp.add_hole(board, h, f"H{i + 1}")
    for k in geo["tasten"]:
        tp.add_key(board, nets, k)

    # --- MCP23017 unter dem XIAO (wie V1, nur tiefer) ---
    my = 36.5
    mcp = tp.part(board, "U1", "MCP23017-E/SO", W / 2, my)
    for p in range(1, 29):
        if p <= 14:
            x, y = -8.255 + (p - 1) * 1.27, 4.65
        else:
            x, y = 8.255 - (p - 15) * 1.27, -4.65
        tp.smd(mcp, p, x, y, 0.6, 2.0, tp.MCP_PINS.get(p), nets)
    tp.finish_back(board, mcp, 0)
    tp.silk_text(board, "U1 Pin 1", W / 2 - 8.255 + 2.8, my - 6.9, 0.8)
    dot = pcbnew.PCB_SHAPE(board)
    dot.SetShape(pcbnew.SHAPE_T_CIRCLE)
    dot.SetCenter(va(W / 2 - 8.255 - 1.2, my - 6.2))
    dot.SetEnd(va(W / 2 - 8.255 - 0.9, my - 6.2))
    dot.SetFilled(True)
    dot.SetLayer(pcbnew.B_SilkS)
    board.Add(dot)
    two(board, "C1", "100nF", W / 2 + 12.0, my, "3V3", "GND", nets, vertical=True)
    two(board, "R1", "4.7k", W / 2 - 11.5, my, "3V3", "SDA", nets, vertical=True)
    two(board, "R2", "4.7k", W / 2 - 13.5, my, "3V3", "SCL", nets, vertical=True)

    # --- XIAO: Umriss, B2B-Buchse, Bruecken-Pads unter den Kantenpads ---
    silk_rect(board, XR - XIAO_W, YB - XIAO_L, XR, YB)
    tp.silk_text(board, "XIAO ESP32S3 (Unterseite zur Platine, USB oben)", XR - XIAO_W / 2,
                 YB - XIAO_L / 2 + 3.5, 0.8)
    b2b = tp.part(board, "J10", "DF40C-30DS-0.4V", B2B_X, B2B_Y)
    for p, net in B2B_PINS.items():
        col = (p - 1) % 15
        row = 0 if p <= 15 else 1
        bpad(b2b, p, (col - 7) * B2B_PITCH, (-1 if row == 0 else 1) * B2B_ROW / 2,
             B2B_PAD[0], B2B_PAD[1], net, nets, shape="rect")
    tp.finish_back(board, b2b, 0)
    tp.silk_text(board, "J10 B2B Pin1", B2B_X - 3.0, B2B_Y - 2.3, 0.8)
    br = tp.part(board, "J11", "XIAO-Bruecken", XR, YB)
    for p, net in XIAO_BRIDGES.items():
        fx, fy = XIAO_PAD_POS[p]
        w, h = (1.6, 1.0) if p >= 23 else (1.4, 1.8)
        bpad(br, p, -fx, fy, w, h, net, nets)
    tp.finish_back(board, br, 0)

    # --- Kamera: FPC-Buchse oben rechts, LDOs, Reset/PWDN ---
    fx0, fy0 = 52.0, 22.0
    fpc = tp.part(board, "J12", "AFC01-S24FCC-00", fx0, fy0)
    for p, net in FPC_PINS.items():
        bpad(fpc, p, (p - 12.5) * 0.5, 0, 0.3, 1.2, net, nets, shape="rect")
    bpad(fpc, "MP1", -7.55, -2.6, 1.6, 2.0, "GND", nets)
    bpad(fpc, "MP2", 7.55, -2.6, 1.6, 2.0, "GND", nets)
    tp.finish_back(board, fpc, 0)
    tp.silk_text(board, "J12 Kamera (Pin 1 rechts)", fx0, fy0 + 2.0, 0.8)
    sot23_5(board, "U2", "LDO 2,8V (ME6211C28)", 47.0, 30.0, "3V3", "2V8", nets)
    sot23_5(board, "U3", "LDO DVDD (pruefen)", 56.0, 30.0, "3V3", "DVDD", nets)
    two(board, "C2", "1uF", 44.6, 30.0, "3V3", "GND", nets, vertical=True)
    two(board, "C3", "1uF", 49.4, 30.0, "2V8", "GND", nets, vertical=True)
    two(board, "C4", "1uF", 53.6, 30.0, "3V3", "GND", nets, vertical=True)
    two(board, "C5", "1uF", 58.4, 30.0, "DVDD", "GND", nets, vertical=True)
    two(board, "FB1", "Ferrit 600R", 47.0, 26.3, "2V8", "AVDD", nets)
    two(board, "C6", "1uF", 50.5, 26.3, "AVDD", "GND", nets)
    two(board, "R3", "10k", 55.0, 26.3, "3V3", "CAM_RST", nets)
    two(board, "C7", "100nF", 58.5, 26.3, "CAM_RST", "GND", nets)
    two(board, "R4", "10k", 61.0, 33.5, "CAM_PWDN", "GND", nets, vertical=True)

    # --- PDM-Mikrofon (Platzhalter-Footprint, MSM261D3526H1CPM 3,5 x 2,65 mm) ---
    mic = comp(board, "MIC1", "MSM261D3526H1CPM", 47.0, 40.0, [
        (1, -1.15, -0.8, 0.55, 0.55, "PDM_DATA"), (2, 0, -0.8, 0.55, 0.55, "GND"),
        (3, 1.15, -0.8, 0.55, 0.55, "GND"), (4, -1.15, 0.8, 0.55, 0.55, "PDM_CLK"),
        (5, 1.15, 0.8, 0.55, 0.55, "3V3")], nets)
    two(board, "C8", "100nF", 51.0, 40.0, "3V3", "GND", nets, vertical=True)
    tp.silk_text(board, "MIC1 (Footprint pruefen)", 47.0, 42.6, 0.8)
    two(board, "C9", "10uF", B2B_X + 5.0, B2B_Y + 4.0, "3V3", "GND", nets, size="0805")

    # --- Display (LT7680-Board) oben links ---
    lcd = tp.part(board, "J13", "LT7680", 0, 0)
    for i, net in enumerate(LCD_PADS):
        bpad(lcd, i + 1, 3.6 + i * 2.5, 6.0, 1.7, 2.6, net, nets)
    tp.finish_back(board, lcd, 0)
    for i, lab in enumerate(LCD_LABELS):
        tp.silk_text(board, lab, 3.6 + i * 2.5, 8.4, 0.8)
    tp.silk_text(board, "J13 Display (LT7680)", 12.3, 3.2, 0.8)

    # --- Strom: Akku, Qi-Empfaenger mit Schottky, Akku-Spannungsteiler ---
    comp(board, "J14", "Akku", 18.0, 60.0, [(1, -2.0, 0, 2.5, 3.0, "BAT1"),
                                             (2, 2.0, 0, 2.5, 3.0, "BAT2")], nets)
    tp.silk_text(board, "AKKU (Polaritaet = XIAO BAT-Pads)", 18.0, 63.0, 0.8)
    comp(board, "J15", "Qi", 47.0, 60.0, [(1, -2.0, 0, 2.5, 3.0, "QI"),
                                           (2, 2.0, 0, 2.5, 3.0, "GND")], nets)
    tp.silk_text(board, "QI+ 5V   QI-", 47.0, 63.0, 0.8)
    comp(board, "D1", "10MQ060NTRPBF", 47.0, 54.0, [(1, -2.15, 0, 1.9, 2.4, "5V"),
                                                     (2, 2.15, 0, 1.9, 2.4, "QI")], nets)
    two(board, "R5", "100k", 44.5, 15.0, "BAT1", "VBAT_SENSE", nets, vertical=True)
    two(board, "R6", "100k", 46.5, 15.0, "VBAT_SENSE", "GND", nets, vertical=True)

    tp.silk_text(board, "Casio-Deck Hauptplatine V2 (Entwurf)", W / 2, 47.0, 1.1)
    return board


def main():
    pcb, dsn = HERE / "v2.kicad_pcb", HERE / "v2.dsn"
    if "--ses" in sys.argv:
        board = pcbnew.LoadBoard(str(pcb))
        n_tr, n_via = tp.import_ses(board, Path(sys.argv[sys.argv.index("--ses") + 1]).read_text())
        print(f"SES eingelesen: {n_tr} Leiterbahnen, {n_via} Vias")
        pcbnew.SaveBoard(str(pcb), board)
        return
    board = build()
    pcbnew.SaveBoard(str(pcb), board)
    print("v2.kicad_pcb geschrieben, DSN:", pcbnew.ExportSpecctraDSN(board, str(dsn)))


if __name__ == "__main__":
    main()
