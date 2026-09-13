from pathlib import Path
from PIL import Image
import warnings
warnings.filterwarnings('ignore')

MAPPING = {(248, 88, 152): (252, 252, 252)}

def fix_png(path, mapping):
    im = Image.open(path).convert('RGBA')
    pixels = list(im.getdata())
    changed = 0
    for i, (r, g, b, a) in enumerate(pixels):
        if a == 0: continue
        key = (r, g, b)
        if key in mapping:
            nr, ng, nb = mapping[key]
            pixels[i] = (nr, ng, nb, a)
            changed += 1
    if changed == 0: return 0
    im.putdata(pixels)
    im.save(path)
    return changed

root = Path('assets/sprites/orbital')
backup = Path('_archive/orbital_before_white_restore')
backup.mkdir(parents=True, exist_ok=True)

for p in sorted(root.rglob('core_*.png')):
    bk = backup / p.name
    if not bk.exists(): bk.write_bytes(p.read_bytes())
    changed = fix_png(p, MAPPING)
    im = Image.open(p).convert('RGBA')
    opaque = {px for px in im.getdata() if px[3] > 0}
    status = 'OK' if len(opaque) <= 3 else 'NG'
    print(f'[{status}] {p.name:15s}  changed={changed:3d}px  colors={len(opaque)}')
