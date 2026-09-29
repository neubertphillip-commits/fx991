"""Druckvorlage 1:1 fuer den Papiertest der neuen Tastaturplatine.

Liest tastatur_geometrie.json und schreibt vorlage.pdf (A4). Drucken mit
"Tatsaechliche Groesse" / 100 %, dann den Massstabsbalken nachmessen (50 mm).
Die alte Platine auf den Umriss legen: Loecher und Tastenkreise muessen passen.
Dann die Silikonmatte auflegen: jede Kohlenoppe muss mittig in ihrem Kreis sitzen.

    python vorlage.py   (braucht matplotlib)
"""

import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Polygon, Rectangle

HERE = Path(__file__).parent
A4_W, A4_H = 210.0, 297.0
MM = 1 / 25.4  # Zoll pro mm


def main():
    geo = json.loads((HERE / "tastatur_geometrie.json").read_text())
    bw, bh = geo["platine"]["breite"], geo["platine"]["hoehe"]
    ox, oy = (A4_W - bw) / 2, 40.0  # Platine oben mittig auf der Seite

    fig = plt.figure(figsize=(A4_W * MM, A4_H * MM))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, A4_W)
    ax.set_ylim(A4_H, 0)  # y nach unten wie in der JSON
    ax.set_aspect("equal")
    ax.axis("off")

    def P(x, y):
        return ox + x, oy + y

    ax.add_patch(Polygon([P(x, y) for x, y in geo["umriss"]], closed=True,
                         fill=False, lw=0.4, ec="black"))
    for h in geo["loecher"]:
        ax.add_patch(Circle(P(h["x"], h["y"]), h["d"] / 2, fill=False, lw=0.4, ec="black"))
        cx, cy = P(h["x"], h["y"])
        ax.plot([cx - 1, cx + 1], [cy, cy], lw=0.2, c="black")
        ax.plot([cx, cx], [cy - 1, cy + 1], lw=0.2, c="black")
    for k in geo["tasten"]:
        col = "red" if k["unsicher"] else "blue"
        cx, cy = P(k["x"], k["y"])
        ax.add_patch(Circle((cx, cy), k["d"] / 2, fill=False, lw=0.4, ec=col,
                            ls="--" if k["unsicher"] else "-"))
        ax.plot([cx - 0.6, cx + 0.6], [cy, cy], lw=0.2, c=col)
        ax.plot([cx, cx], [cy - 0.6, cy + 0.6], lw=0.2, c=col)
        ax.text(cx, cy + k["d"] / 2 - 0.9, str(k["nr"]), ha="center", va="center",
                fontsize=5, color=col)

    # Massstab 50 mm und 10-mm-Quadrat zum Nachmessen
    y = oy + bh + 15
    ax.add_patch(Rectangle((ox, y), 50, 1.5, color="black"))
    for i in range(6):
        ax.plot([ox + 10 * i] * 2, [y - 1.5, y], lw=0.3, c="black")
    ax.text(ox + 52, y + 1, "50 mm - nachmessen! Drucken mit 100 % / tatsaechlicher Groesse",
            fontsize=7, va="center")
    ax.add_patch(Rectangle((ox, y + 6), 10, 10, fill=False, lw=0.4, ec="black"))
    ax.text(ox + 12, y + 11, "10 x 10 mm", fontsize=7, va="center")

    ax.text(A4_W / 2, 15, "Casio-Deck Tastaturplatine - Papiertest (Entwurf 1)",
            ha="center", fontsize=11, weight="bold")
    ax.text(A4_W / 2, 22, "Tastenseite von vorn. Blau: gemessen, rot gestrichelt: geschaetzt "
            "(im Foto im Schatten). Genauigkeit ca. 0,5-1 mm.", ha="center", fontsize=7)
    ax.text(A4_W / 2, 27, "1. Alte Platine auf den Umriss legen: sitzen Loecher und Tasten?  "
            "2. Matte auflegen: Noppen mittig im Kreis?", ha="center", fontsize=7)
    ax.text(A4_W / 2, 32, "Abweichungen je Taste in mm notieren (z. B. '46: 1 mm nach rechts').",
            ha="center", fontsize=7)

    fig.savefig(HERE / "vorlage.pdf")
    fig.savefig(HERE / "vorlage.png", dpi=150)
    print("vorlage.pdf geschrieben")


if __name__ == "__main__":
    main()
