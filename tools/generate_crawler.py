"""Draw the crawler directly on a 32x32 pixel grid; no source downscaling."""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

W = H = 32
GREEN = (0, 255, 0)
BLACK = (0, 0, 0)
TEAL = (0, 136, 136)
MINT = (184, 248, 216)
pixels = [[GREEN for _ in range(W)] for _ in range(H)]


def put(x: int, y: int, color: tuple[int, int, int]) -> None:
    if 0 <= x < W and 0 <= y < H:
        pixels[y][x] = color


def line(x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int]) -> None:
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    error = dx + dy
    while True:
        put(x0, y0, color)
        if x0 == x1 and y0 == y1:
            break
        twice = 2 * error
        if twice >= dy:
            error += dy
            x0 += sx
        if twice <= dx:
            error += dx
            y0 += sy


def polygon(points: list[tuple[int, int]], color: tuple[int, int, int]) -> None:
    for y in range(H):
        for x in range(W):
            inside = False
            j = len(points) - 1
            for i, (xi, yi) in enumerate(points):
                xj, yj = points[j]
                if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
                    inside = not inside
                j = i
            if inside:
                put(x, y, color)


def border(points: list[tuple[int, int]], color: tuple[int, int, int]) -> None:
    for a, b in zip(points, points[1:] + points[:1]):
        line(*a, *b, color)


# Six articulated legs and two antennae, behind the segmented carapace.
for base, tip in [
    ((7, 19), (4, 31)), ((11, 19), (9, 31)), ((15, 19), (15, 31)),
    ((20, 19), (22, 31)), ((25, 18), (28, 31)),
]:
    line(*base, base[0] - 1, base[1] + 4, BLACK)
    line(base[0] - 1, base[1] + 4, *tip, BLACK)
line(5, 16, 1, 18, BLACK)
line(5, 18, 2, 21, BLACK)
body = [(3, 17), (6, 13), (10, 10), (15, 8), (21, 9), (26, 12),
        (30, 16), (31, 18), (28, 21), (24, 20), (19, 22), (13, 21),
        (8, 22), (4, 20)]
polygon(body, TEAL)
border(body, BLACK)
highlight = [(7, 14), (11, 11), (16, 10), (21, 11), (26, 14),
             (28, 16), (24, 15), (20, 13), (16, 12), (11, 13)]
polygon(highlight, MINT)
for x0, y0, x1, y1 in [(9, 12, 11, 20), (14, 9, 16, 21),
                       (19, 10, 21, 20), (24, 12, 26, 19)]:
    line(x0, y0, x1, y1, BLACK)
put(29, 17, BLACK)


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(
        ">I", zlib.crc32(kind + data) & 0xFFFFFFFF
    )


scanlines = b"".join(b"\x00" + bytes(channel for color in row for channel in color)
                     for row in pixels)
png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(scanlines, 9))
png += chunk(b"IEND", b"")
output = Path(__file__).resolve().parents[1] / "assets/raw_enemies/enemy_crawler.png"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(png)
print(output)
