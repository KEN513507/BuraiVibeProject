"""PLAYER_ASSET_GATE: validate assets/sprites/player/ before the runtime may load it.

Checks only. This script never resizes, recolors, or rewrites any image.
Standard library only (no Pillow needed).

Run from any directory: python tools/player_asset_validator.py
Exit code 0 = PASS, 1 = FAIL.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / "assets" / "sprites" / "player"

REQUIRED_DIRECTIONS = (
    "right", "up_right", "up", "up_left",
    "left", "down_left", "down", "down_right",
)
ALLOWED_CELL_SIZES = (16, 32)
BACKGROUND_KEY = (0x00, 0xFF, 0x00)

# NES 2C02 master palette (common 54-color RGB rendition). Any opaque pixel outside
# this set is treated as an anti-aliasing / gradient / off-palette color.
NES_MASTER_PALETTE = frozenset(int(h, 16) for h in """
7C7C7C 0000FC 0000BC 4428BC 940084 A80020 A81000 881400 503000 007800 006800 005800 004058 000000
BCBCBC 0078F8 0058F8 6844FC D800CC E40058 F83800 E45C10 AC7C00 00B800 00A800 00A844 008888
F8F8F8 3CBCFC 6888FC 9878F8 F878F8 F85898 F87858 FCA044 F8B800 B8F818 58D854 58F898 00E8D8 787878
FCFCFC A4E4FC B8B8F8 D8B8F8 F8B8F8 F8A4C0 F0D0B0 FCE0A8 F8D878 D8F878 B8F8B8 B8F8D8 00FCFC F8D8F8
""".split())


class PngError(Exception):
    pass


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def decode_png_rgba(data: bytes) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    """Decode an 8-bit, non-interlaced RGB / RGBA / indexed PNG into RGBA pixels."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise PngError("not a PNG file")
    pos, idat, plte, trns, ihdr = 8, bytearray(), None, None, None
    while pos + 8 <= len(data):
        length, ctype = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", body)
        elif ctype == b"PLTE":
            plte = body
        elif ctype == b"tRNS":
            trns = body
        elif ctype == b"IDAT":
            idat += body
        elif ctype == b"IEND":
            break
    if ihdr is None:
        raise PngError("missing IHDR")
    width, height, depth, color_type, _, _, interlace = ihdr
    if depth != 8:
        raise PngError(f"unsupported bit depth {depth} (need 8)")
    if interlace:
        raise PngError("interlaced PNG not supported")
    channels = {2: 3, 3: 1, 6: 4}.get(color_type)
    if channels is None:
        raise PngError(f"unsupported color type {color_type} (need RGB, RGBA or indexed)")
    if color_type == 3 and plte is None:
        raise PngError("indexed PNG without PLTE")

    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    if len(raw) != height * (stride + 1):
        raise PngError("image data size mismatch")
    prev = bytearray(stride)
    pixels: list[tuple[int, int, int, int]] = []
    for y in range(height):
        start = y * (stride + 1)
        ftype, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b, c = prev[i], (prev[i - channels] if i >= channels else 0)
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif ftype == 4:
                line[i] = (line[i] + _paeth(a, b, c)) & 0xFF
            elif ftype != 0:
                raise PngError(f"bad filter type {ftype}")
        for x in range(width):
            px = line[x * channels:(x + 1) * channels]
            if color_type == 6:
                pixels.append((px[0], px[1], px[2], px[3]))
            elif color_type == 2:
                pixels.append((px[0], px[1], px[2], 255))
            else:
                idx = px[0]
                if idx * 3 + 3 > len(plte):
                    raise PngError(f"palette index {idx} out of range")
                alpha = trns[idx] if trns is not None and idx < len(trns) else 255
                pixels.append((plte[idx * 3], plte[idx * 3 + 1], plte[idx * 3 + 2], alpha))
        prev = line
    return width, height, pixels


def _hex(rgb: int) -> str:
    return f"#{rgb:06X}"


def _parse_palette(entries: list, errors: list[str]) -> set[int]:
    palette: set[int] = set()
    for entry in entries:
        text = str(entry).lstrip("#")
        if len(text) != 6:
            errors.append(f"player.json: palette entry {entry!r} is not #RRGGBB")
            continue
        try:
            value = int(text, 16)
        except ValueError:
            errors.append(f"player.json: palette entry {entry!r} is not #RRGGBB")
            continue
        if value not in NES_MASTER_PALETTE:
            errors.append(f"player.json: palette entry {_hex(value)} is not an NES master palette color")
        palette.add(value)
    return palette


def validate_contract(contract: dict) -> tuple[list[str], set[int]]:
    errors: list[str] = []
    cw, ch = contract.get("cell_width"), contract.get("cell_height")
    if cw not in ALLOWED_CELL_SIZES or cw != ch:
        errors.append(f"player.json: cell size {cw}x{ch} must be 32x32 or 16x16")

    max_colors = contract.get("max_opaque_colors_per_sprite")
    if not isinstance(max_colors, int) or not 1 <= max_colors <= 4:
        errors.append("player.json: max_opaque_colors_per_sprite must be an integer 1..4")

    palette_entries = contract.get("palette")
    if not isinstance(palette_entries, list) or not palette_entries:
        errors.append("player.json: palette is empty (define the player sprite palette first)")
        palette: set[int] = set()
    else:
        palette = _parse_palette(palette_entries, errors)

    directions = contract.get("directions")
    if not isinstance(directions, dict):
        errors.append("player.json: directions must be an object")
        return errors, palette
    missing = [d for d in REQUIRED_DIRECTIONS if d not in directions]
    extra = [d for d in directions if d not in REQUIRED_DIRECTIONS]
    if missing:
        errors.append(f"player.json: missing directions {missing}")
    if extra:
        errors.append(f"player.json: unknown directions {extra}")
    names = list(directions.values())
    for name in names:
        if not isinstance(name, str) or Path(name).name != name or not name.endswith(".png"):
            errors.append(f"player.json: file name {name!r} must be a bare .png name")
    if len(set(map(str, names))) != len(names):
        errors.append("player.json: each direction must use its own file")
    return errors, palette


def validate_sprite(path: Path, contract: dict, palette: set[int]) -> list[str]:
    label = path.name
    if not path.is_file():
        return [f"{label}: file not found"]
    try:
        width, height, pixels = decode_png_rgba(path.read_bytes())
    except (PngError, zlib.error, struct.error) as exc:
        return [f"{label}: cannot decode PNG ({exc})"]

    errors: list[str] = []
    cw, ch = contract.get("cell_width"), contract.get("cell_height")
    if (width, height) != (cw, ch):
        # No resize fallback: a wrong-size image is rejected, never scaled.
        errors.append(f"{label}: size {width}x{height}, contract requires {cw}x{ch}")

    semi_alpha = sum(1 for p in pixels if p[3] not in (0, 255))
    if semi_alpha:
        errors.append(f"{label}: {semi_alpha} pixel(s) with partial alpha (only 0 or 255 allowed)")

    dirty_transparent = sum(1 for p in pixels if p[3] == 0 and p[:3] != (0, 0, 0))
    if dirty_transparent:
        errors.append(f"{label}: {dirty_transparent} transparent pixel(s) not exactly RGBA(0,0,0,0)")

    key_left = sum(1 for p in pixels if p[3] == 255 and p[:3] == BACKGROUND_KEY)
    if key_left:
        errors.append(f"{label}: {key_left} opaque #00FF00 pixel(s) remain (background key not removed)")

    opaque = {(p[0] << 16) | (p[1] << 8) | p[2] for p in pixels if p[3] == 255}
    opaque.discard(0x00FF00)
    if not opaque:
        errors.append(f"{label}: image is empty (no opaque pixels)")

    off_nes = sorted(c for c in opaque if c not in NES_MASTER_PALETTE)
    if off_nes:
        shown = ", ".join(_hex(c) for c in off_nes[:8])
        more = f" (+{len(off_nes) - 8} more)" if len(off_nes) > 8 else ""
        errors.append(f"{label}: {len(off_nes)} non-NES color(s), likely anti-aliasing: {shown}{more}")

    max_colors = contract.get("max_opaque_colors_per_sprite")
    if isinstance(max_colors, int) and len(opaque) > max_colors:
        errors.append(f"{label}: {len(opaque)} opaque colors, limit is {max_colors}")

    if palette:
        outside = sorted(c for c in opaque if c not in palette)
        if outside:
            errors.append(f"{label}: colors outside player palette: {', '.join(_hex(c) for c in outside[:8])}")
    return errors


def run_gate(asset_dir: Path) -> list[str]:
    contract_path = asset_dir / "player.json"
    if not contract_path.is_file():
        return [f"player.json not found in {asset_dir}"]
    try:
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"player.json: invalid JSON ({exc})"]

    errors, palette = validate_contract(contract)
    directions = contract.get("directions") if isinstance(contract.get("directions"), dict) else {}
    for direction in REQUIRED_DIRECTIONS:
        name = directions.get(direction)
        if isinstance(name, str) and Path(name).name == name:
            errors += validate_sprite(asset_dir / name, contract, palette)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("asset_dir", nargs="?", type=Path, default=DEFAULT_DIR)
    args = parser.parse_args()

    errors = run_gate(args.asset_dir)
    print(f"PLAYER_ASSET_GATE: {args.asset_dir}")
    for error in errors:
        print(f"  FAIL  {error}")
    print("RESULT: PASS" if not errors else f"RESULT: FAIL ({len(errors)} issue(s)) - runtime load forbidden")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
