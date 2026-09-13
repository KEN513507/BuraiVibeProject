"""Draw the eight canonical Orbital frames directly on 32x32 and 16x16 grids.

No reference pixels are sampled or rescaled. Only NES master-palette colors and
fully transparent pixels are written.
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'assets' / 'sprites' / 'orbital'
BLACK = (0x00, 0x00, 0x00, 0xFF)
MAGENTA = (0xE4, 0x00, 0x58, 0xFF)
PINK = (0xF8, 0x58, 0x98, 0xFF)
WHITE = (0xFC, 0xFC, 0xFC, 0xFF)
GOLD = (0xFC, 0xA0, 0x44, 0xFF)
BLUE = (0x00, 0x78, 0xF8, 0xFF)
CYAN = (0x3C, 0xBC, 0xFC, 0xFF)
CLEAR = (0, 0, 0, 0)


def chunk(tag: bytes, payload: bytes) -> bytes:
    return struct.pack('>I', len(payload)) + tag + payload + struct.pack('>I', zlib.crc32(tag + payload))


def write_png(path: Path, width: int, height: int, pixels: list[tuple[int, int, int, int]]) -> None:
    rows = b''.join(b'\x00' + bytes(channel for pixel in pixels[y * width:(y + 1) * width]
                                   for channel in pixel) for y in range(height))
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr)
                     + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def core(frame: int) -> list[tuple[int, int, int, int]]:
    white_radius = (3.5, 4.2, 5.1, 4.7)[frame]
    image = []
    for y in range(32):
        for x in range(32):
            r2 = (x - 15.5) ** 2 + (y - 15.5) ** 2
            if r2 > 14.0 ** 2:
                color = CLEAR
            elif r2 > 12.7 ** 2:
                color = BLACK
            elif r2 > 10.7 ** 2:
                color = MAGENTA
            elif r2 > 7.1 ** 2:
                color = BLACK
            elif r2 > (white_radius + 1.6) ** 2:
                color = MAGENTA
            elif r2 > white_radius ** 2:
                color = PINK
            else:
                color = WHITE
            image.append(color)
    return image


def shield(frame: int) -> list[tuple[int, int, int, int]]:
    orb_radius = (2.1, 2.5, 2.9, 2.5)[frame]
    image = []
    for y in range(16):
        for x in range(16):
            r2 = (x - 7.5) ** 2 + (y - 7.5) ** 2
            if r2 > 7.2 ** 2:
                color = CLEAR
            elif r2 > 6.1 ** 2:
                color = BLACK
            elif r2 > 4.7 ** 2:
                color = GOLD
            else:
                color = BLACK
            blue_r2 = (x - 8.0) ** 2 + (y - 6.0) ** 2
            if blue_r2 <= orb_radius ** 2:
                color = CYAN if frame in (1, 2) and blue_r2 <= 1.6 ** 2 else BLUE
            if (x - 5.0) ** 2 + (y - 10.0) ** 2 <= 1.0:
                color = GOLD
            image.append(color)
    return image


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for frame in range(4):
        write_png(OUT / f'core_{frame}.png', 32, 32, core(frame))
        write_png(OUT / f'shield_{frame}.png', 16, 16, shield(frame))
    print(f'wrote 8 native-resolution frames to {OUT}')


if __name__ == '__main__':
    main()
