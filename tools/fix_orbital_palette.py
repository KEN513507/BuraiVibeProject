#!/usr/bin/env python3
"""Orbital敵スプライトの色数を3色に削減する（NES制約準拠）"""
from pathlib import Path
from PIL import Image

# 置換マッピング: {元のRGB: 置換先RGB}
# ファイル名パターンで分岐
REPLACEMENTS = {
    # core: 白 → 明マゼンタ
    'core':  {(252, 252, 252): (248, 88, 152)},
    # shield: 明青 → 青
    'shield': {(60, 188, 252): (0, 120, 248)},
}


def fix_png(path: Path, mapping: dict):
    im = Image.open(path).convert('RGBA')
    pixels = list(im.getdata())
    changed = 0

    for i, (r, g, b, a) in enumerate(pixels):
        if a == 0:
            continue
        key = (r, g, b)
        if key in mapping:
            nr, ng, nb = mapping[key]
            pixels[i] = (nr, ng, nb, a)
            changed += 1

    if changed == 0:
        return 0

    im.putdata(pixels)
    im.save(path)
    return changed


def count_colors(path: Path):
    im = Image.open(path).convert('RGBA')
    opaque = {p for p in im.getdata() if p[3] > 0}
    return len(opaque), opaque


def main():
    root = Path('assets/sprites/orbital')
    if not root.exists():
        print(f'[ERROR] {root} が見つかりません')
        return

    # バックアップ
    backup = Path('_archive/orbital_pre_3color_backup')
    backup.mkdir(parents=True, exist_ok=True)
    for p in root.rglob('*.png'):
        dst = backup / p.relative_to(root)
        dst.parent.mkdir(parents=True, exist_ok=True)
        if not dst.exists():
            dst.write_bytes(p.read_bytes())
    print(f'[BACKUP] {backup}')

    # 修正
    for p in sorted(root.rglob('*.png')):
        name = p.stem.lower()
        if name.startswith('core'):
            mapping = REPLACEMENTS['core']
        elif name.startswith('shield'):
            mapping = REPLACEMENTS['shield']
        else:
            print(f'[SKIP] {p.name}  (unknown prefix)')
            continue

        changed = fix_png(p, mapping)
        n_colors, colors = count_colors(p)

        status = 'OK ' if n_colors <= 3 else 'NG '
        print(f'[{status}] {p.name:15s}  changed={changed:3d}px  colors={n_colors}')


if __name__ == '__main__':
    main()