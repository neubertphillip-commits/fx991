"""Tests fuer library.py:  python3 -m unittest test_library"""

import os
import stat
import tempfile
import unittest
import zlib
from pathlib import Path

import library
from library import Library, reflow, safe_name, unique_name


class NamesTest(unittest.TestCase):
    def test_safe_name(self):
        self.assertEqual(safe_name("Übung 1 (neu).txt"), "Uebung_1_neu.txt")
        self.assertEqual(safe_name(".versteckt"), "versteckt")
        self.assertEqual(safe_name("çafé.md"), "cafe.md")
        long = safe_name("a" * 80 + ".txt")
        self.assertEqual(len(long), library.NAME_LEN)
        self.assertTrue(long.endswith(".txt"))

    def test_unique_name(self):
        used = {"notiz.txt"}
        self.assertEqual(unique_name("notiz.txt", used), "notiz-2.txt")
        self.assertEqual(unique_name("Notiz.TXT", used), "Notiz-2.TXT")
        self.assertEqual(unique_name("neu.txt", used), "neu.txt")


class ReflowTest(unittest.TestCase):
    def test_reflow(self):
        raw = "Erste Zeile\nzweite Zei-\nle hier\n\nNeuer Absatz\f\nSeite 2\n"
        self.assertEqual(reflow(raw), "Erste Zeile zweite Zeile hier\n\nNeuer Absatz\n\nSeite 2\n")


class LibraryTest(unittest.TestCase):
    def test_scan(self):
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            (folder / "a.txt").write_bytes("Größe\r\n".encode("cp1252"))
            (folder / "A.md").write_text("x")
            (folder / "skript.pdf").write_bytes(b"%PDF")
            (folder / "prog.exe").write_bytes(b"MZ")
            # pdftotext durch ein Skript ersetzen
            fake = folder / "bin" / "pdftotext"
            fake.parent.mkdir()
            fake.write_text("#!/bin/sh\nprintf 'Satz ueber\\nzwei Zeilen\\n'\n")
            fake.chmod(fake.stat().st_mode | stat.S_IEXEC)
            old_path = os.environ["PATH"]
            os.environ["PATH"] = f"{fake.parent}:{old_path}"
            try:
                items, skipped = Library(folder).scan()
            finally:
                os.environ["PATH"] = old_path
            by_name = {it.name: it for it in items}
            self.assertEqual(sorted(by_name), ["A.md", "a.txt", "skript.txt"])
            self.assertEqual(by_name["a.txt"].data, "Größe\n".encode())
            self.assertEqual(by_name["a.txt"].crc, zlib.crc32("Größe\n".encode()))
            self.assertEqual(by_name["skript.txt"].data, b"Satz ueber zwei Zeilen\n")
            self.assertEqual(len(skipped), 1)
            self.assertIn("prog.exe", skipped[0])


if __name__ == "__main__":
    unittest.main()
