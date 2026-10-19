import unittest
from pathlib import Path
from tools.generate_pixel_font import GLYPHS, generate

class PixelFontTests(unittest.TestCase):
    def test_complete_ascii_and_case_distinction(self):
        self.assertEqual(set(GLYPHS),{chr(n) for n in range(32,127)})
        for rows in GLYPHS.values():
            self.assertEqual(len(rows),7)
            self.assertTrue(all(isinstance(row,int) and 0<=row<32 for row in rows))
        for char in 'abcdefghijklmnopqrstuvwxyz':
            self.assertNotEqual(GLYPHS[char],GLYPHS[char.upper()])

    def test_generated_font_is_current(self):
        source=Path(__file__).resolve().parents[1]/'firmware/main/ui_font_pixel.c'
        self.assertEqual(source.read_text(encoding='utf-8'),generate())
