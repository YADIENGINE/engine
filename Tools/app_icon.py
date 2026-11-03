"""Draws the tools' application icon from code (design D1: visuals come from code).

Usage:
    python Tools/app_icon.py OUTPUT.ico --name NAME

The icon is the product name's first letter in the tool UI's 5x7 pixel font (ToolUi/ToolPixel.cpp) on a plate in the
primary button's colours (ToolUi/ToolTheme.cpp), flat and on whole pixels (design D23). The editor's build runs it at
configure time (Editor/CMakeLists.txt); its output is never committed. Only the standard library: PNG and ICO are
written by hand.
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path

ICON_SIZES = (16, 24, 32, 48, 64, 128, 256)

# ToolPalette (ToolUi/ToolTheme.cpp)
ACCENT = (0xF4, 0xAF, 0x65)
ACCENT_TEXT = (0x2A, 0x1A, 0x0A)

# 5x7 pixel font: rows top to bottom, '#' = lit.
FONT = {
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."],
    "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "I": [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "J": ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "M": ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "V": ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
    "X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    "Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
    "0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    "3": ["####.", "....#", "....#", ".###.", "....#", "....#", "####."],
    "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    "6": [".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."],
    "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    "9": [".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."],
    "-": [".....", ".....", ".....", "#####", ".....", ".....", "....."],
}
GLYPH_COLUMNS, GLYPH_ROWS = 5, 7
TRANSPARENT = (0, 0, 0, 0)


def draw_icon(size: int, letter: str) -> list[list[tuple[int, int, int, int]]]:
    """RGBA rows: the plate with one stepped pixel at each corner, the letter centred on whole cells."""
    plate, ink = (*ACCENT, 255), (*ACCENT_TEXT, 255)
    pixels = [[plate] * size for _ in range(size)]
    notch = max(1, round(size / 16))
    for top in (0, size - notch):
        for y in range(top, top + notch):
            pixels[y][:notch] = [TRANSPARENT] * notch
            pixels[y][size - notch:] = [TRANSPARENT] * notch

    cell = size // (GLYPH_ROWS + 1)
    left, top = (size - GLYPH_COLUMNS * cell) // 2, (size - GLYPH_ROWS * cell) // 2
    for row, bits in enumerate(FONT.get(letter.upper(), FONT["-"])):
        for column, bit in enumerate(bits):
            if bit == "#":
                for y in range(top + row * cell, top + (row + 1) * cell):
                    pixels[y][left + column * cell:left + (column + 1) * cell] = [ink] * cell
    return pixels


def encode_png(pixels: list[list[tuple[int, int, int, int]]]) -> bytes:
    rows = bytearray()
    for row in pixels:
        rows.append(0)
        for pixel in row:
            rows += bytes(pixel)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    header = struct.pack(">IIBBBBB", len(pixels[0]), len(pixels), 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) +
            chunk(b"IEND", b""))


def encode_ico(images: list[tuple[int, bytes]]) -> bytes:
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries, data = b"", b""
    for size, png in images:
        dimension = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(png), offset + len(data))
        data += png
    return header + entries + data


def write_icon(output: Path, name: str) -> None:
    """Leaves an unchanged file alone, so configuring again does not relink the executable."""
    if not name:
        raise ValueError("the product name is empty")
    data = encode_ico([(size, encode_png(draw_icon(size, name[0]))) for size in ICON_SIZES])
    if output.is_file() and output.read_bytes() == data:
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Draw the tools' application icon")
    parser.add_argument("output", type=Path)
    parser.add_argument("--name", required=True, help="product name (engine manifest)")
    args = parser.parse_args(argv)
    write_icon(args.output, args.name)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
