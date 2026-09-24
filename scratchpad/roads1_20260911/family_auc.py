"""ROADS1 section 1, part 6: every family's footprint scored against vanilla's
own colour sheet, each with ITS OWN floor -- the same mask displaced five ways,
which preserves the mask's area, shape and spatial spectrum and destroys only
its registration with the sheet (ww-control-calibration).

A family vanilla bakes must beat its own displaced twin.  A family vanilla does
not bake must not.

  python family_auc.py <masks.npz> <decals.npz> <vanillaDir> <cx> <cy>
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                    # noqa: E402

SHIFTS = ((64, 64), (-96, 48), (128, -128), (0, 200), (200, 0))


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    return np.array(px, dtype=np.float32).reshape(h, w, 4)


def auc(score, mask):
    s = score.ravel().astype(np.float64)
    y = mask.ravel()
    order = np.argsort(s)
    r = np.empty(len(s))
    r[order] = np.arange(1, len(s) + 1, dtype=np.float64)
    n1 = float(y.sum())
    n0 = float(len(y) - n1)
    if n1 == 0 or n0 == 0:
        return float('nan')
    return (r[y].sum() - n1 * (n1 + 1) / 2.0) / (n1 * n0)


def shift(m, dx, dy):
    return np.roll(np.roll(m, dy, axis=0), dx, axis=1)


def main(argv):
    npz, dnpz, vanDir, cx, cy = argv[0], argv[1], argv[2], int(argv[3]), int(argv[4])
    d = dict(np.load(npz))
    d.update(dict(np.load(dnpz)))
    col = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx, cy)))
    lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
    sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)
    print('family        texels    AUC(bright)  floor(displaced, worst..best)   '
          'AUC(grey)   floor')
    for f in ('road', 'decal', 'trees', 'rocks', 'buildings', 'setdressing'):
        if f not in d:
            continue
        m = d[f]
        a = auc(lum, m)
        ag = auc(-sat, m)
        fl = [auc(lum, shift(m, dx, dy)) for dx, dy in SHIFTS]
        flg = [auc(-sat, shift(m, dx, dy)) for dx, dy in SHIFTS]
        print('%-12s %7d   %.3f        %.3f .. %.3f                 %.3f    '
              '%.3f .. %.3f'
              % (f, int(m.sum()), a, min(fl), max(fl), ag, min(flg), max(flg)))
    print('ceiling: a mask scored by itself = %.3f'
          % auc(d['road'].astype(float), d['road']))


if __name__ == '__main__':
    main(sys.argv[1:])
