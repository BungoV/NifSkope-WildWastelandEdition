"""Lane LATTICE -- shade the sheets OFFLINE, one sun, no renderer.

The lattice was diagnosed in a render, which has a mesh, a tangent basis and
perspective in it.  Shading the sheet directly at 1:1 texels removes all three:
whatever squares survive here are IN THE SHEET.

    python lattice_shade.py            # ours / vanilla / the variants
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lattice import (bc_roundtrip, chunk_hgt, find_ours, load, load_land,
                     make_msn, VAN)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'images')
# a sun that grazes, so a crease in the normal field is at its most visible;
# east-ish and 25 degrees up, in the sheet's own (east, up, north) frame
SUN = np.array([0.80, 0.42, 0.43])
SUN = SUN / np.linalg.norm(SUN)


def shade(rgb):
    n = np.asarray(rgb, dtype=np.float64) / 255.0 * 2.0 - 1.0
    n /= np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-6)
    d = np.clip(n[:, :, 0] * SUN[0] + n[:, :, 1] * SUN[1] + n[:, :, 2] * SUN[2], 0, 1)
    return np.clip(0.15 + 0.85 * d, 0, 1)


def panel(img, box, zoom, label):
    x0, y0, w, h = box
    a = (shade(img)[y0:y0 + h, x0:x0 + w] * 255.0).astype(np.uint8)
    im = Image.fromarray(a, 'L').convert('RGB').resize((w * zoom, h * zoom), Image.NEAREST)
    return im, label


def strip(panels, path, gap=8):
    w = sum(p.width for p, _ in panels) + gap * (len(panels) + 1)
    h = max(p.height for p, _ in panels) + 34 + gap
    out = Image.new('RGB', (w, h), (26, 26, 30))
    from PIL import ImageDraw
    d = ImageDraw.Draw(out)
    x = gap
    for p, lab in panels:
        out.paste(p, (x, 30))
        d.text((x + 2, 10), lab, fill=(235, 235, 235))
        x += p.width + gap
    out.save(path)
    print('wrote %s  (%dx%d)' % (path, out.width, out.height))


def main(argv):
    tile = argv[0] if argv else '4.-60.36'
    dim, cx, cy = (int(v) for v in tile.split('.'))
    land = load_land()
    hgt = chunk_hgt(land, cx, cy, dim)
    box = (150, 150, 128, 128)      # a quiet interior patch, same box everywhere
    zoom = 4

    van = load(os.path.join(VAN, 'Commonwealth.%s_msn.DDS' % tile), 0)
    our = load(find_ours(tile), 0)
    ps = [panel(van, box, zoom, 'vanilla (shipped)'),
          panel(our, box, zoom, 'ours (shipped, bilinear)'),
          panel(bc_roundtrip(make_msn(hgt, dim, mode='quintic')), box, zoom,
                'ours + quintic ease'),
          panel(make_msn(hgt, dim, mode='bilinear'), box, zoom,
                'bilinear, no codec'),
          panel(make_msn(hgt, dim, mode='quintic'), box, zoom,
                'quintic, no codec')]
    strip(ps, os.path.join(OUT, 'lattice_shade_%s.png' % tile))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
