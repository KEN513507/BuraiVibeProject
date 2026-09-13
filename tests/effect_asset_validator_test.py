"""Tests for tools/effect_asset_validator.py. Run: python tests/effect_asset_validator_test.py"""
from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import effect_asset_validator as gate  # noqa: E402

WHITE, ORANGE, DARK = (0xFC, 0xFC, 0xFC, 255), (0xF8, 0x38, 0x00, 255), (0xA8, 0x00, 0x20, 255)
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


def good_frame(size: int = 16) -> list[tuple[int, int, int, int]]:
    pixels = [CLEAR] * (size * size)
    for y in range(5, 11):
        for x in range(5, 11):
            pixels[y * size + x] = ORANGE
    pixels[7 * size + 7], pixels[4 * size + 4] = WHITE, DARK
    return pixels


class EffectAssetGateTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self._tmp.name)
        self.contract = json.loads((gate.DEFAULT_DIR / gate.CONTRACT_FILE).read_text(encoding="utf-8"))
        self.write_contract()
        for name in self.contract["frames"]:
            self.write_frame(name, good_frame())

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write_contract(self) -> None:
        (self.dir / gate.CONTRACT_FILE).write_text(json.dumps(self.contract), encoding="utf-8")

    def write_frame(self, name: str, pixels, size: int = 16) -> None:
        (self.dir / name).write_bytes(encode_png_rgba(size, size, pixels))

    def assert_fails_with(self, fragment: str) -> None:
        errors = gate.run_gate(self.dir)
        self.assertTrue(any(fragment in e for e in errors), f"expected {fragment!r} in {errors}")

    def test_valid_set_passes(self) -> None:
        self.assertEqual(gate.run_gate(self.dir), [])

    def test_wrong_size_fails(self) -> None:
        self.write_frame("explosion_1.png", good_frame(32), size=32)
        self.assert_fails_with("size 32x32, contract requires 16x16")

    def test_wrong_contract_cell_size_fails(self) -> None:
        self.contract["cell_width"] = self.contract["cell_height"] = 32
        self.write_contract()
        self.assert_fails_with("must be 16x16")

    def test_missing_frame_fails(self) -> None:
        (self.dir / "explosion_2.png").unlink()
        self.assert_fails_with("explosion_2.png: file not found")

    def test_duplicate_frame_name_fails(self) -> None:
        self.contract["frames"] = ["explosion_0.png", "explosion_0.png"]
        self.write_contract()
        self.assert_fails_with("must not repeat")

    def test_partial_alpha_fails(self) -> None:
        pixels = good_frame()
        pixels[0] = (0, 0, 0, 128)
        self.write_frame("explosion_0.png", pixels)
        self.assert_fails_with("partial alpha")

    def test_dirty_transparent_fails(self) -> None:
        pixels = good_frame()
        pixels[0] = (10, 20, 30, 0)
        self.write_frame("explosion_0.png", pixels)
        self.assert_fails_with("not exactly RGBA(0,0,0,0)")

    def test_remaining_green_key_fails(self) -> None:
        pixels = good_frame()
        pixels[0] = (0x00, 0xFF, 0x00, 255)
        self.write_frame("explosion_0.png", pixels)
        self.assert_fails_with("#00FF00")

    def test_non_nes_color_fails(self) -> None:
        pixels = good_frame()
        pixels[6 * 16 + 6] = (0xF0, 0x40, 0x10, 255)
        self.write_frame("explosion_3.png", pixels)
        self.assert_fails_with("non-NES color")

    def test_too_many_colors_fails(self) -> None:
        self.contract["palette"].append("#0000BC")
        self.write_contract()
        pixels = good_frame()
        pixels[6 * 16 + 6] = (0x00, 0x00, 0xBC, 255)
        self.write_frame("explosion_3.png", pixels)
        self.assert_fails_with("4 opaque colors, limit is 3")

    def test_color_outside_palette_fails(self) -> None:
        pixels = [p if p != DARK else (0x00, 0x00, 0xBC, 255) for p in good_frame()]
        self.write_frame("explosion_3.png", pixels)
        self.assert_fails_with("outside effect palette")

    def test_empty_palette_fails(self) -> None:
        self.contract["palette"] = []
        self.write_contract()
        self.assert_fails_with("palette is empty")

    def test_palette_entry_without_hash_fails(self) -> None:
        self.contract["palette"] = ["FCFCFC", "#F83800", "#A80020"]
        self.write_contract()
        self.assert_fails_with("not a bare #RRGGBB string")

    def test_empty_image_fails(self) -> None:
        self.write_frame("explosion_1.png", [CLEAR] * (16 * 16))
        self.assert_fails_with("image is empty")

    def test_shipped_directory_passes(self) -> None:
        self.assertEqual(gate.run_gate(gate.DEFAULT_DIR), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
