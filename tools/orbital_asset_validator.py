"""Validate the eight canonical Orbital PNGs without modifying them."""
from __future__ import annotations

import sys
from pathlib import Path

from player_asset_validator import NES_MASTER_PALETTE, decode_png_rgba

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets' / 'sprites' / 'orbital'
PALETTES = {
    'core': {0x000000, 0xE40058, 0xF85898, 0xFCFCFC},
    'shield': {0x000000, 0xFCA044, 0x0078F8, 0x3CBCFC},
}
SIZES = {'core': 32, 'shield': 16}


def validate() -> list[str]:
    errors = []
    for kind in ('core', 'shield'):
        assert PALETTES[kind] <= NES_MASTER_PALETTE
        frame_pixels = []
        for frame in range(4):
            path = ASSETS / f'{kind}_{frame}.png'
            if not path.is_file():
                errors.append(f'{path.name}: missing')
                continue
            try:
                width, height, pixels = decode_png_rgba(path.read_bytes())
            except Exception as exc:
                errors.append(f'{path.name}: invalid PNG: {exc}')
                continue
            if (width, height) != (SIZES[kind], SIZES[kind]):
                errors.append(f'{path.name}: expected {SIZES[kind]}x{SIZES[kind]}, got {width}x{height}')
            opaque = set()
            for rgba in pixels:
                r, g, b, a = rgba
                if a == 0:
                    if (r, g, b) != (0, 0, 0):
                        errors.append(f'{path.name}: dirty transparent pixel')
                        break
                elif a == 255:
                    opaque.add((r << 16) | (g << 8) | b)
                else:
                    errors.append(f'{path.name}: partial alpha')
                    break
            if not opaque:
                errors.append(f'{path.name}: empty frame')
            if len(opaque) > 4 or not opaque <= PALETTES[kind]:
                errors.append(f'{path.name}: outside four-color NES palette')
            frame_pixels.append(pixels)
        if len(frame_pixels) == 4 and len({tuple(p) for p in frame_pixels}) != 4:
            errors.append(f'{kind}: four animation frames are not distinct')
    return errors


if __name__ == '__main__':
    failures = validate()
    if failures:
        for failure in failures:
            print('FAIL:', failure)
        sys.exit(1)
    print('ORBITAL_ASSET_GATE PASS: 8 native-size, four-color NES frames')
