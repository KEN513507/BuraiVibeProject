"""Convert native-resolution enemy art into NES-style sheets and metadata.

Requires Pillow: python -m pip install Pillow
Run from any directory: python tools/process_enemies.py
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
# Opaque colors sampled from NES-like fixed swatches; transparency is a fourth entry.
NES_COLORS = (
    (0x00, 0x00, 0x00), (0x3C, 0x3C, 0x3C), (0x7C, 0x7C, 0x7C),
    (0xFC, 0xFC, 0xFC), (0x00, 0x58, 0xF8), (0x00, 0x88, 0x88),
    (0x00, 0xA8, 0x44), (0x58, 0xD8, 0x54), (0xB8, 0xF8, 0xD8),
    (0xAC, 0x7C, 0x00), (0xF8, 0x78, 0x58), (0xE4, 0x00, 0x58),
    (0xA8, 0x00, 0x20), (0x68, 0x28, 0xA0), (0xFC, 0xBC, 0xB0),
)


def is_background(r: int, g: int, b: int, a: int) -> bool:
    return a < 128 or (g > 200 and r < 100 and b < 100) or (
        r > 240 and g > 240 and b > 240
    )


def nearest_color(rgb: tuple[int, int, int],
                  choices: tuple[tuple[int, int, int], ...]) -> tuple[int, int, int]:
    return min(choices, key=lambda c: sum((v - t) ** 2 for v, t in zip(rgb, c)))


def process_enemy_sprite(
    input_path: str | Path,
    output_png_path: str | Path,
    meta_json_path: str | Path,
    frame_size: tuple[int, int] = (32, 32),
) -> dict:
    """Keep the source pixel grid; reject dimensions that would require resizing."""
    input_path = Path(input_path)
    output_png_path = Path(output_png_path)
    meta_json_path = Path(meta_json_path)
    fw, fh = frame_size
    if fw not in (16, 32) or fh not in (16, 32):
        raise ValueError("frame dimensions must each be 16 or 32 pixels")
    with Image.open(input_path) as source:
        image = source.convert("RGBA")
    width, height = image.size
    if width == 0 or height == 0 or width % fw or height % fh:
        raise ValueError(f"{input_path}: {width}x{height} is not a {fw}x{fh} grid")

    result = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    columns, rows = width // fw, height // fh
    frames = []
    for row in range(rows):
        for col in range(columns):
            box = (col * fw, row * fh, (col + 1) * fw, (row + 1) * fh)
            tile_source = image.crop(box)
            raw = list(tile_source.get_flattened_data() if hasattr(tile_source, "get_flattened_data")
                       else tile_source.getdata())
            mapped = [
                None if is_background(*pixel) else nearest_color(pixel[:3], NES_COLORS)
                for pixel in raw
            ]
            # NES sprites have at most three opaque colors plus transparency.
            counts = Counter(color for color in mapped if color is not None)
            chosen = tuple(color for color, _ in counts.most_common(3))
            if chosen:
                pixels = [
                    (*nearest_color(color, chosen), 255) if color is not None else (0, 0, 0, 0)
                    for color in mapped
                ]
            else:
                pixels = [(0, 0, 0, 0)] * len(mapped)
            tile = Image.new("RGBA", (fw, fh))
            tile.putdata(pixels)
            result.paste(tile, box)
            frames.append({"x": box[0], "y": box[1], "width": fw, "height": fh})

    metadata = {
        "frame_width": fw, "frame_height": fh,
        "total_frames": columns * rows, "columns": columns, "rows": rows,
        "frames": frames,
    }
    output_png_path.parent.mkdir(parents=True, exist_ok=True)
    meta_json_path.parent.mkdir(parents=True, exist_ok=True)
    result.save(output_png_path, "PNG")
    meta_json_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
    return metadata


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=ROOT / "assets/raw_enemies")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "assets/sprites")
    parser.add_argument("--frame-size", type=int, choices=(16, 32), default=32)
    args = parser.parse_args()
    if not args.input_dir.is_dir():
        raise SystemExit(f"input directory not found: {args.input_dir}")
    inputs = sorted(p for p in args.input_dir.iterdir() if p.suffix.lower() == ".png")
    for path in inputs:
        metadata = process_enemy_sprite(
            path, args.output_dir / path.name,
            args.output_dir / f"{path.stem}.json",
            (args.frame_size, args.frame_size),
        )
        print(f"{path.name}: {metadata['total_frames']} frame(s)")
    if not inputs:
        print(f"no PNG files in {args.input_dir}")


if __name__ == "__main__":
    main()
