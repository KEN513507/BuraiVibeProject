"""Tests for tools/player_asset_validator.py. Run: python tests/player_asset_validator_test.py"""
from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import player_asset_validator as gate  # noqa: E402

BLACK, BLUE, WHITE = (0, 0, 0, 255), (0x00, 0x58, 0xF8, 255), (0xFC, 0xFC, 0xFC, 255)
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


def good_sprite(size: int = 32) -> list[tuple[int, int, int, int]]:
    pixels = [CLEAR] * (size * size)
    for y in range(8, 24):
        for x in range(12, 20):
            pixels[y * size + x] = BLUE
    pixels[8 * size + 12], pixels[9 * size + 13] = BLACK, WHITE
    return pixels


class PlayerAssetGateTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)
        self.contract = json.loads((gate.DEFAULT_DIR / "player.json").read_text(encoding="utf-8"))
        self.contract["palette"] = ["#000000", "#0058F8", "#FCFCFC"]
        self.write_contract()
        for name in self.contract["directions"].values():
            self.write_sprite(name, good_sprite())

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write_contract(self) -> None:
        (self.dir / "player.json").write_text(json.dumps(self.contract), encoding="utf-8")

    def write_sprite(self, name: str, pixels, size: int = 32) -> None:
        (self.dir / name).write_bytes(encode_png_rgba(size, size, pixels))

    def assert_fails_with(self, fragment: str) -> None:
        errors = gate.run_gate(self.dir)
        self.assertTrue(any(fragment in e for e in errors), f"expected {fragment!r} in {errors}")

    def test_valid_set_passes(self) -> None:
        self.assertEqual(gate.run_gate(self.dir), [])

    def test_wrong_size_fails(self) -> None:
        self.write_sprite("player_up.png", good_sprite(64), size=64)
        self.assert_fails_with("size 64x64, contract requires 32x32")

    def test_missing_direction_file_fails(self) -> None:
        (self.dir / "player_down_left.png").unlink()
        self.assert_fails_with("player_down_left.png: file not found")

    def test_missing_direction_key_fails(self) -> None:
        del self.contract["directions"]["down"]
        self.write_contract()
        self.assert_fails_with("missing directions ['down']")

    def test_partial_alpha_fails(self) -> None:
        pixels = good_sprite()
        pixels[0] = (0, 0, 0, 128)
        self.write_sprite("player_right.png", pixels)
        self.assert_fails_with("partial alpha")

    def test_remaining_green_key_fails(self) -> None:
        pixels = good_sprite()
        pixels[0] = (0x00, 0xFF, 0x00, 255)
        self.write_sprite("player_right.png", pixels)
        self.assert_fails_with("#00FF00")

    def test_near_green_is_not_treated_as_background(self) -> None:
        pixels = good_sprite()
        pixels[0] = (2, 249, 3, 255)
        self.write_sprite("player_right.png", pixels)
        self.assert_fails_with("non-NES color")

    def test_antialias_color_fails(self) -> None:
        pixels = good_sprite()
        pixels[10 * 32 + 20] = (0x40, 0x80, 0xF0, 255)
        self.write_sprite("player_left.png", pixels)
        self.assert_fails_with("non-NES color")

    def test_too_many_colors_fails(self) -> None:
        pixels = good_sprite()
        pixels[10 * 32 + 20] = (0xA8, 0x00, 0x20, 255)
        self.write_sprite("player_left.png", pixels)
        self.assert_fails_with("4 opaque colors, limit is 3")

    def test_color_outside_player_palette_fails(self) -> None:
        pixels = [p if p != WHITE else (0xA8, 0x00, 0x20, 255) for p in good_sprite()]
        self.write_sprite("player_left.png", pixels)
        self.assert_fails_with("outside player palette")

    def test_empty_image_fails(self) -> None:
        self.write_sprite("player_up_left.png", [CLEAR] * (32 * 32))
        self.assert_fails_with("image is empty")

    def test_empty_palette_fails(self) -> None:
        self.contract["palette"] = []
        self.write_contract()
        self.assert_fails_with("palette is empty")

    def test_shipped_directory_passes_when_art_exists(self) -> None:
        self.assertEqual(gate.run_gate(gate.DEFAULT_DIR), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
