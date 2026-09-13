"""EFFECT_ASSET_GATE: validate assets/sprites/effects/ before the runtime may load it.

Checks only. This script never resizes, recolors, or rewrites any image.
Standard library only (no Pillow needed).

Run from any directory: python tools/effect_asset_validator.py
Exit code 0 = PASS, 1 = FAIL.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
import zlib
from pathlib import Path

from player_asset_validator import BACKGROUND_KEY, NES_MASTER_PALETTE, PngError, decode_png_rgba

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / "assets" / "sprites" / "effects"

CONTRACT_FILE = "effects.json"
REQUIRED_CELL_SIZE = (16, 16)
MAX_OPAQUE_COLORS = 3
HEX_COLOR = re.compile(r"#[0-9A-Fa-f]{6}")


def _hex(rgb: int) -> str:
    return f"#{rgb:06X}"


def validate_contract(contract: dict) -> tuple[list[str], set[int]]:
    errors: list[str] = []
    if contract.get("contract_version") != 1:
        errors.append(f"{CONTRACT_FILE}: contract_version must be 1")

    cw, ch = contract.get("cell_width"), contract.get("cell_height")
    if (cw, ch) != REQUIRED_CELL_SIZE:
        errors.append(f"{CONTRACT_FILE}: cell size {cw}x{ch} must be "
                      f"{REQUIRED_CELL_SIZE[0]}x{REQUIRED_CELL_SIZE[1]}")

    max_colors = contract.get("max_opaque_colors_per_sprite")
    if not isinstance(max_colors, int) or isinstance(max_colors, bool) or not 1 <= max_colors <= MAX_OPAQUE_COLORS:
        errors.append(f"{CONTRACT_FILE}: max_opaque_colors_per_sprite must be an integer 1..{MAX_OPAQUE_COLORS}")

    palette: set[int] = set()
    entries = contract.get("palette")
    if not isinstance(entries, list) or not entries:
        errors.append(f"{CONTRACT_FILE}: palette is empty (define the effect palette first)")
    else:
        for entry in entries:
            if not isinstance(entry, str) or not HEX_COLOR.fullmatch(entry):
                errors.append(f"{CONTRACT_FILE}: palette entry {entry!r} is not a bare #RRGGBB string")
                continue
            value = int(entry[1:], 16)
            if value not in NES_MASTER_PALETTE:
                errors.append(f"{CONTRACT_FILE}: palette entry {_hex(value)} is not an NES master palette color")
            palette.add(value)

    frames = contract.get("frames")
    if not isinstance(frames, list) or not frames:
        errors.append(f"{CONTRACT_FILE}: frames must be a list of at least one .png name")
        return errors, palette
    for name in frames:
        if not isinstance(name, str) or Path(name).name != name or not name.endswith(".png"):
            errors.append(f"{CONTRACT_FILE}: frame name {name!r} must be a bare .png name")
    if len(set(map(str, frames))) != len(frames):
        errors.append(f"{CONTRACT_FILE}: frame names must not repeat")
    return errors, palette


def validate_frame(path: Path, contract: dict, palette: set[int]) -> list[str]:
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

    if palette:
        outside = sorted(c for c in opaque if c not in palette)
        if outside:
            errors.append(f"{label}: colors outside effect palette: {', '.join(_hex(c) for c in outside[:8])}")

    max_colors = contract.get("max_opaque_colors_per_sprite")
    if isinstance(max_colors, int) and len(opaque) > max_colors:
        errors.append(f"{label}: {len(opaque)} opaque colors, limit is {max_colors}")
    return errors


def run_gate(asset_dir: Path) -> list[str]:
    contract_path = asset_dir / CONTRACT_FILE
    if not contract_path.is_file():
        return [f"{CONTRACT_FILE} not found in {asset_dir}"]
    try:
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"{CONTRACT_FILE}: invalid JSON ({exc})"]
    if not isinstance(contract, dict):
        return [f"{CONTRACT_FILE}: top level must be an object"]

    errors, palette = validate_contract(contract)
    frames = contract.get("frames") if isinstance(contract.get("frames"), list) else []
    for name in frames:
        if isinstance(name, str) and Path(name).name == name:
            errors += validate_frame(asset_dir / name, contract, palette)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("asset_dir", nargs="?", type=Path, default=DEFAULT_DIR)
    args = parser.parse_args()

    errors = run_gate(args.asset_dir)
    print(f"EFFECT_ASSET_GATE: {args.asset_dir}")
    for error in errors:
        print(f"  FAIL  {error}")
    print("RESULT: PASS" if not errors else f"RESULT: FAIL ({len(errors)} issue(s)) - runtime load forbidden")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
