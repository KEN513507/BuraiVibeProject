"""Convert a #00FF00-keyed 32x32 source sprite into a transparent production PNG.

Pixel rules only. This script never resizes, recolors, or filters any image.
- (0,255,0,255) becomes (0,0,0,0).
- Every other pixel keeps its RGB and gets alpha 255 (binary alpha).
Standard library only (no Pillow needed).

Run: python tools/green_to_alpha.py <in.png> <out.png>
Exit code 0 = written, 1 = rejected.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

from player_asset_validator import PngError, decode_png_rgba

REQUIRED_SIZE = (32, 32)
KEY = (0x00, 0xFF, 0x00, 0xFF)
CLEAR = (0, 0, 0, 0)


def encode_png_rgba(width: int, height: int, pixels: list[tuple[int, int, int, int]]) -> bytes:
    def chunk(ctype: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + ctype + body + struct.pack(">I", zlib.crc32(ctype + body))

    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for p in pixels[y * width:(y + 1) * width]:
            raw.extend(p)
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw)))
            + chunk(b"IEND", b""))


def key_to_alpha(pixels: list[tuple[int, int, int, int]]) -> list[tuple[int, int, int, int]]:
    return [CLEAR if p == KEY else (p[0], p[1], p[2], 255) for p in pixels]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("src", type=Path)
    parser.add_argument("dst", type=Path)
    args = parser.parse_args()

    try:
        width, height, pixels = decode_png_rgba(args.src.read_bytes())
    except (OSError, PngError, zlib.error, struct.error) as exc:
        print(f"REJECT  {args.src}: cannot read PNG ({exc})")
        return 1
    if (width, height) != REQUIRED_SIZE:
        # No resize fallback: a wrong-size source is rejected, never scaled.
        print(f"REJECT  {args.src}: size {width}x{height}, must be {REQUIRED_SIZE[0]}x{REQUIRED_SIZE[1]}")
        return 1

    out = key_to_alpha(pixels)
    args.dst.parent.mkdir(parents=True, exist_ok=True)
    args.dst.write_bytes(encode_png_rgba(width, height, out))
    keyed = sum(1 for p in out if p == CLEAR)
    print(f"WROTE   {args.dst}  ({keyed} transparent, {len(out) - keyed} opaque)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
