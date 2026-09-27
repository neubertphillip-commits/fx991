"""Dateien fuer den Datei-Viewer des Rechners: ein Ordner auf dem Handy.

Alles, was direkt im Ordner liegt, wird fuer den Rechner aufbereitet:
  Text (.txt, .md, .csv, ...)      -> UTF-8 mit Zeilenenden \\n
  PDF                              -> Text (pdftotext; Termux: pkg install poppler)
  Bilder (.jpg, .png, .webp, ...)  -> JPEG, hoechstens 480x640 (Pillow; pip install pillow)
Andere Dateien werden uebersprungen. Die Namen werden fuer das Dateisystem des
Rechners vereinfacht (ae statt ae-Umlaut, keine Leerzeichen, hoechstens 40 Zeichen).
"""

import re
import subprocess
import unicodedata
import zlib
from dataclasses import dataclass
from pathlib import Path

NAME_LEN = 40  # wie store::NAME_LEN in der Firmware
MAX_W, MAX_H = 480, 640  # Display hochkant
MAX_TEXT = 1024 * 1024
MAX_RAW_JPEG = 400 * 1024  # ohne Pillow werden JPEGs nur bis zu dieser Groesse uebernommen
JPEG_QUALITY = 80

TEXT_EXT = {
    ".txt", ".md", ".markdown", ".csv", ".tsv", ".json", ".yaml", ".yml", ".ini", ".cfg",
    ".log", ".tex", ".py", ".c", ".h", ".cpp", ".hpp", ".ino", ".js", ".ts", ".html", ".css",
    ".xml", ".sh", ".java", ".kt", ".rs", ".go",
}
IMAGE_EXT = {".jpg", ".jpeg", ".png", ".webp", ".gif", ".bmp"}

UMLAUTE = str.maketrans({"ä": "ae", "ö": "oe", "ü": "ue", "Ä": "Ae", "Ö": "Oe", "Ü": "Ue", "ß": "ss"})


class Skip(Exception):
    """Datei kann nicht aufbereitet werden; die Nachricht sagt, warum."""


@dataclass
class Item:
    name: str      # Name auf dem Rechner
    data: bytes
    crc: int       # CRC-32 wie in der Firmware (crc32.cpp)
    image: bool
    source: str    # Dateiname im Ordner


def safe_name(filename: str) -> str:
    """Dateiname, den der Rechner speichern kann: [A-Za-z0-9._-], hoechstens NAME_LEN."""
    s = unicodedata.normalize("NFKD", filename.translate(UMLAUTE))
    s = s.encode("ascii", "ignore").decode()
    s = re.sub(r"[^A-Za-z0-9._-]+", "_", s).strip("._-") or "datei"
    stem, dot, ext = s.rpartition(".")
    if not dot:
        stem, ext = s, ""
    ext = ext[:8]
    room = NAME_LEN - (len(ext) + 1 if ext else 0)
    stem = stem[:room].rstrip("._-") or "datei"
    return f"{stem}.{ext}" if ext else stem


def unique_name(name: str, used: set[str]) -> str:
    if name.lower() not in used:
        return name
    stem, dot, ext = name.rpartition(".")
    if not dot:
        stem, ext = name, ""
    n = 2
    while True:
        tail = f"-{n}" + (f".{ext}" if ext else "")
        candidate = stem[: NAME_LEN - len(tail)] + tail
        if candidate.lower() not in used:
            return candidate
        n += 1


def decode_text(raw: bytes) -> str:
    for enc in ("utf-8-sig", "cp1252"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return raw.decode("latin-1")


def normalize_text(text: str) -> bytes:
    data = text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")
    if len(data) > MAX_TEXT:
        raise Skip(f"zu gross ({len(data) // 1024} kB Text, max. {MAX_TEXT // 1024} kB)")
    return data


def reflow(text: str) -> str:
    """PDF-Text: harte Zeilenumbrueche innerhalb von Absaetzen entfernen, damit der
    Rechner selbst auf seine Breite umbrechen kann. Trennstriche am Zeilenende vor
    einem Kleinbuchstaben fallen weg."""
    paras = []
    for block in re.split(r"\n\s*\n", text.replace("\f", "\n\n")):
        lines = [ln.strip() for ln in block.splitlines() if ln.strip()]
        if not lines:
            continue
        out = lines[0]
        for ln in lines[1:]:
            if out.endswith("-") and ln[:1].islower():
                out = out[:-1] + ln
            else:
                out += " " + ln
        paras.append(out)
    return "\n\n".join(paras) + "\n"


def pdf_to_text(path: Path) -> bytes:
    try:
        res = subprocess.run(["pdftotext", "-enc", "UTF-8", "-nopgbrk", str(path), "-"],
                             capture_output=True, timeout=120)
    except FileNotFoundError:
        raise Skip("pdftotext fehlt (Termux: pkg install poppler)") from None
    except subprocess.TimeoutExpired:
        raise Skip("PDF braucht zu lange") from None
    if res.returncode != 0:
        raise Skip("PDF nicht lesbar")
    text = reflow(res.stdout.decode("utf-8", errors="replace"))
    if not text.strip():
        raise Skip("PDF enthaelt keinen Text (eingescannt?)")
    return normalize_text(text)


def image_to_jpeg(path: Path) -> bytes:
    try:
        from PIL import Image, ImageOps
    except ImportError:
        if path.suffix.lower() in (".jpg", ".jpeg") and path.stat().st_size <= MAX_RAW_JPEG:
            return path.read_bytes()  # unveraendert; verkleinern kann erst Pillow
        raise Skip("Bilder brauchen Pillow (pip install pillow)") from None
    import io

    try:
        with Image.open(path) as im:
            im = ImageOps.exif_transpose(im)  # Handyfotos richtig herum
            im = im.convert("RGB")
            im.thumbnail((MAX_W, MAX_H))
            buf = io.BytesIO()
            # Baseline-JPEG ohne Extras: das koennen kleine JPEG-Decoder sicher
            im.save(buf, "JPEG", quality=JPEG_QUALITY, optimize=True, progressive=False)
            return buf.getvalue()
    except OSError as e:
        raise Skip(f"Bild nicht lesbar ({e})") from None


def convert(path: Path) -> tuple[str, bytes, bool]:
    """(Name auf dem Rechner, Inhalt, ist Bild) oder Skip."""
    ext = path.suffix.lower()
    if ext in IMAGE_EXT:
        return safe_name(path.stem + ".jpg"), image_to_jpeg(path), True
    if ext == ".pdf":
        return safe_name(path.stem + ".txt"), pdf_to_text(path), False
    if ext in TEXT_EXT:
        if path.stat().st_size > MAX_TEXT * 2:
            raise Skip("zu gross")
        return safe_name(path.name), normalize_text(decode_text(path.read_bytes())), False
    raise Skip("Dateityp wird nicht unterstuetzt")


class Library:
    def __init__(self, folder: Path):
        self.folder = Path(folder)
        self.cache: dict[Path, tuple] = {}  # Pfad -> (mtime, Groesse, Ergebnis oder Skip)

    def scan(self) -> tuple[list[Item], list[str]]:
        """Alle Dateien des Ordners aufbereitet (Ergebnisse gecacht) und die
        Liste der uebersprungenen mit Grund."""
        items: list[Item] = []
        skipped: list[str] = []
        used: set[str] = set()
        seen = set()
        if not self.folder.is_dir():
            return items, [f"Ordner fehlt: {self.folder}"]
        for path in sorted(self.folder.iterdir(), key=lambda p: p.name.lower()):
            if path.name.startswith(".") or not path.is_file():
                continue
            seen.add(path)
            st = path.stat()
            cached = self.cache.get(path)
            if cached and cached[:2] == (st.st_mtime_ns, st.st_size):
                result = cached[2]
            else:
                try:
                    result = convert(path)
                except Skip as e:
                    result = e
                self.cache[path] = (st.st_mtime_ns, st.st_size, result)
            if isinstance(result, Skip):
                skipped.append(f"{path.name}: {result}")
                continue
            name, data, image = result
            name = unique_name(name, used)
            used.add(name.lower())
            items.append(Item(name, data, zlib.crc32(data), image, path.name))
        for gone in set(self.cache) - seen:
            del self.cache[gone]
        return items, skipped
