"""The tools' application icon (Tools/app_icon.py); no compiler."""

import struct
import tempfile
import unittest
from pathlib import Path

from Tools.app_icon import ACCENT, ACCENT_TEXT, ICON_SIZES, TRANSPARENT, draw_icon, write_icon


class AppIconTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(ignore_cleanup_errors=True)
        self.addCleanup(self.temp.cleanup)
        self.output = Path(self.temp.name) / "Brand" / "Icon.ico"

    def test_ico_lists_every_size_as_png(self):
        write_icon(self.output, "Name")
        data = self.output.read_bytes()
        self.assertEqual(struct.unpack_from("<HHH", data), (0, 1, len(ICON_SIZES)))
        for index, size in enumerate(ICON_SIZES):
            width, *_, length, offset = struct.unpack_from("<BBBBHHII", data, 6 + 16 * index)
            self.assertEqual(width, size % 256)
            self.assertEqual(data[offset:offset + 4], b"\x89PNG")
            self.assertLessEqual(offset + length, len(data))

    def test_plate_has_stepped_corners_and_the_letter(self):
        for size in ICON_SIZES:
            pixels = draw_icon(size, "y")
            self.assertEqual(len(pixels), size)
            self.assertTrue(all(len(row) == size for row in pixels))
            self.assertEqual(pixels[0][0], TRANSPARENT)
            self.assertEqual(pixels[size - 1][size - 1], TRANSPARENT)
            self.assertEqual(pixels[0][size // 2], (*ACCENT, 255))
            self.assertTrue(any((*ACCENT_TEXT, 255) in row for row in pixels))

    def test_letter_comes_from_the_name(self):
        self.assertNotEqual(draw_icon(32, "A"), draw_icon(32, "B"))
        self.assertEqual(draw_icon(32, "a"), draw_icon(32, "A"))

    def test_unchanged_icon_is_not_rewritten(self):
        write_icon(self.output, "Name")
        before = self.output.stat().st_mtime_ns
        self.output.touch()
        touched = self.output.stat().st_mtime_ns
        write_icon(self.output, "Name")
        self.assertGreaterEqual(touched, before)
        self.assertEqual(self.output.stat().st_mtime_ns, touched)

    def test_empty_name_is_refused(self):
        with self.assertRaises(ValueError):
            write_icon(self.output, "")


if __name__ == "__main__":
    unittest.main()
