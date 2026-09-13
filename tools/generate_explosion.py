"""Procedurally generate the 4-frame 16x16 explosion effect (deterministic, fixed seed).

No LLM drawing, no hand-authored pixel grids, no reference pixels.
The reference sheet (sprite_output/effects/reference/explosion_reference_sheet.png)
is REFERENCE_ONLY for style and is never opened by this script.

Pipeline per frame:
  build 16x16 canvas -> self-check all frames -> write green-keyed source
  (sprite_output/effects/src/explosion_<n>_src.png) -> decode it back ->
  green_to_alpha.key_to_alpha -> assets/sprites/effects/explosion_<n>.png
Standard library only (no Pillow needed).

Run from any directory: python tools/generate_explosion.py
Exit code 0 = written, non-zero = self-check failed (nothing written).
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

from green_to_alpha import KEY, encode_png_rgba, key_to_alpha
from player_asset_validator import decode_png_rgba

ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "sprite_output" / "effects" / "src"
OUT_DIR = ROOT / "assets" / "sprites" / "effects"

SEED = 0xB0B0
SIZE = 16
MARGIN = 2  # opaque pixels must stay inside [MARGIN, SIZE - 1 - MARGIN]
FRAME_COUNT = 4

CLEAR = (0, 0, 0, 0)
WHITE = (0xFC, 0xFC, 0xFC, 255)
ORANGE = (0xF8, 0x38, 0x00, 255)
DARK = (0xA8, 0x00, 0x20, 255)
OPAQUE = (WHITE, ORANGE, DARK)
ALLOWED = (CLEAR,) + OPAQUE
# Higher wins when shapes overlap, so a later rim never paints over a core.
PRIORITY = {CLEAR: 0, DARK: 1, ORANGE: 2, WHITE: 3}

CENTER = (SIZE - 1) / 2.0  # 7.5: the cell center sits between pixels 7 and 8

Canvas = list[tuple[int, int, int, int]]


def snap(color: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    """Snap any RGBA to the nearest allowed value (alpha decides transparent vs opaque)."""
    if color[3] < 128:
        return CLEAR
    return min(OPAQUE, key=lambda c: sum((a - b) ** 2 for a, b in zip(c[:3], color[:3])))


def inside(x: int, y: int) -> bool:
    return MARGIN <= x < SIZE - MARGIN and MARGIN <= y < SIZE - MARGIN


def put(canvas: Canvas, x: int, y: int, color: tuple[int, int, int, int], force: bool = False) -> None:
    if not inside(x, y):
        return
    color = snap(color)
    i = y * SIZE + x
    if force or PRIORITY[color] > PRIORITY[canvas[i]]:
        canvas[i] = color


def bubble(canvas: Canvas, cx: float, cy: float, radius: float, core: float,
           rim: tuple[int, int, int, int] = ORANGE, fill: tuple[int, int, int, int] = WHITE) -> None:
    """Filled disc: `fill` inside `core`, `rim` from `core` out to `radius`."""
    for y in range(SIZE):
        for x in range(SIZE):
            d = math.hypot(x - cx, y - cy)
            if d <= core:
                put(canvas, x, y, fill)
            elif d <= radius:
                put(canvas, x, y, rim)


def ring_dots(canvas: Canvas, rng: random.Random, radius: float, count: int,
              color: tuple[int, int, int, int], jitter: float) -> None:
    """Isolated dots on a ring; each lands only on an empty pixel."""
    base = rng.uniform(0, math.tau)
    for k in range(count):
        a = base + k * math.tau / count + rng.uniform(-jitter, jitter)
        x = round(CENTER + math.cos(a) * radius)
        y = round(CENTER + math.sin(a) * radius)
        if inside(x, y) and canvas[y * SIZE + x] == CLEAR:
            canvas[y * SIZE + x] = color


def frame_birth(rng: random.Random) -> Canvas:
    canvas: Canvas = [CLEAR] * (SIZE * SIZE)
    # ~4px cluster: one rim disc with a 2px white core (half a pixel up so the core is 2 wide, 1 tall)
    bubble(canvas, CENTER, CENTER - 0.5, radius=2.2, core=0.75)
    return canvas


def frame_growth(rng: random.Random) -> Canvas:
    canvas: Canvas = [CLEAR] * (SIZE * SIZE)
    # ~8px cluster: central bubble plus 4 small lobes, ~3px white core
    bubble(canvas, CENTER, CENTER, radius=2.4, core=1.1)
    base = rng.uniform(0, math.tau)
    for k in range(4):
        a = base + k * math.tau / 4 + rng.uniform(-0.3, 0.3)
        bubble(canvas, CENTER + math.cos(a) * 2.6, CENTER + math.sin(a) * 2.6, radius=1.5, core=0.0)
    ring_dots(canvas, rng, radius=4.6, count=5, color=DARK, jitter=0.25)
    return canvas


def frame_peak(rng: random.Random) -> Canvas:
    canvas: Canvas = [CLEAR] * (SIZE * SIZE)
    # ~12px cluster: large white core, lobed orange rim with 3 gaps, dark outer shell
    bubble(canvas, CENTER, CENTER, radius=4.3, core=3.0)
    base = rng.uniform(0, math.tau)
    for k in range(6):
        a = base + k * math.tau / 6 + rng.uniform(-0.2, 0.2)
        bubble(canvas, CENTER + math.cos(a) * 3.1, CENTER + math.sin(a) * 3.1, radius=1.9, core=0.9)
    # Break the rim: clear outermost orange pixels near 3 angles so the core shows through
    for k in range(3):
        gap = base + math.pi / 6 + k * math.tau / 3 + rng.uniform(-0.15, 0.15)
        for y in range(SIZE):
            for x in range(SIZE):
                i = y * SIZE + x
                if canvas[i] != ORANGE:
                    continue
                a = math.atan2(y - CENTER, x - CENTER)
                da = abs((a - gap + math.pi) % math.tau - math.pi)
                if da < 0.28 and math.hypot(x - CENTER, y - CENTER) >= 4.0:
                    canvas[i] = CLEAR
    ring_dots(canvas, rng, radius=5.6, count=10, color=DARK, jitter=0.12)
    return canvas


def frame_dissipate(rng: random.Random) -> Canvas:
    canvas: Canvas = [CLEAR] * (SIZE * SIZE)
    # 6 separated puffs on a ~12px ring: orange center, dark rim, no white core
    base = rng.uniform(0, math.tau)
    for k in range(6):
        a = base + k * math.tau / 6 + rng.uniform(-0.2, 0.2)
        r = rng.uniform(3.8, 4.6)
        cx = CENTER + math.cos(a) * r
        cy = CENTER + math.sin(a) * r
        # core >= 0.71 guarantees the nearest pixel of any center is orange
        bubble(canvas, cx, cy, radius=1.3, core=0.75, rim=DARK, fill=ORANGE)
    return canvas


BUILDERS = (frame_birth, frame_growth, frame_peak, frame_dissipate)


def self_check(index: int, canvas: Canvas) -> list[str]:
    errors: list[str] = []
    label = f"frame {index}"
    if len(canvas) != SIZE * SIZE:
        errors.append(f"{label}: canvas has {len(canvas)} pixels, expected {SIZE * SIZE}")
        return errors
    bad = {p for p in canvas if p not in ALLOWED}
    if bad:
        errors.append(f"{label}: values outside the 4 allowed: {sorted(bad)[:4]}")
    dirty = sum(1 for p in canvas if p[3] == 0 and p != CLEAR)
    if dirty:
        errors.append(f"{label}: {dirty} transparent pixel(s) not exactly (0,0,0,0)")
    opaque = sum(1 for p in canvas if p[3] == 255)
    if opaque < 4:
        errors.append(f"{label}: only {opaque} opaque pixel(s), need at least 4")
    for x, y in ((0, 0), (SIZE - 1, 0), (0, SIZE - 1), (SIZE - 1, SIZE - 1)):
        if canvas[y * SIZE + x] != CLEAR:
            errors.append(f"{label}: corner ({x},{y}) is not transparent")
    outside = sum(1 for y in range(SIZE) for x in range(SIZE)
                  if canvas[y * SIZE + x] != CLEAR and not inside(x, y))
    if outside:
        errors.append(f"{label}: {outside} opaque pixel(s) inside the {MARGIN}px margin")
    return errors


def main() -> int:
    rng = random.Random(SEED)
    frames = [build(rng) for build in BUILDERS]
    assert len(frames) == FRAME_COUNT

    errors = [e for i, canvas in enumerate(frames) for e in self_check(i, canvas)]
    if errors:
        raise SystemExit("SELF-CHECK FAILED, nothing written:\n  " + "\n  ".join(errors))

    SRC_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for i, canvas in enumerate(frames):
        src_path = SRC_DIR / f"explosion_{i}_src.png"
        out_path = OUT_DIR / f"explosion_{i}.png"
        keyed = [KEY if p == CLEAR else p for p in canvas]
        src_path.write_bytes(encode_png_rgba(SIZE, SIZE, keyed))

        # Convert from the written source, exactly as green_to_alpha.py would.
        width, height, pixels = decode_png_rgba(src_path.read_bytes())
        out_path.write_bytes(encode_png_rgba(width, height, key_to_alpha(pixels)))

        opaque = [p for p in canvas if p[3] == 255]
        print(f"{out_path.relative_to(ROOT).as_posix()}  opaque={len(opaque)}  colors={len(set(opaque))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
