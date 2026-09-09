"""Independent corroboration of the brief's tile orientation, by seam continuity.

The brief settled the orientation against the _msn normals (mean error 0.0719
vs 0.24+).  This is a different test with no shared assumptions: terrain LOD is
continuous across a cell boundary, so if cells are placed into the world grid
correctly the assembled image has no step at the 32-texel cell seams.  If a
sub-block is mapped to the wrong cell, seams appear.

Score = mean |d| across cell-boundary rows/cols divided by mean |d| across
interior rows/cols.  1.0 is perfect continuity; a misplacement lifts it.
Only WITHIN-tile seams are scored, since a cross-tile seam is continuous under
every candidate mapping and would dilute the signal.
"""
import os
import re

import numpy as np

from dds import DDS
from bcnp import decode_rgb

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
MIP = 2
CP = 32

# (name, row-of-cell(dx,dy), col-of-cell(dx,dy)) for the four candidates
CANDS = [
    ('row0=NORTH col0=WEST  (brief)', lambda dx, dy: (3 - dy, dx)),
    ('row0=SOUTH col0=WEST', lambda dx, dy: (dy, dx)),
    ('row0=NORTH col0=EAST', lambda dx, dy: (3 - dy, 3 - dx)),
    ('row0=SOUTH col0=EAST', lambda dx, dy: (dy, 3 - dx)),
]


def tiles():
    pat = re.compile(r'^Commonwealth\.4\.(-?\d+)\.(-?\d+)\.DDS$', re.I)
    out = []
    for n in os.listdir(VAN):
        m = pat.match(n)
        if m:
            out.append((int(m.group(1)), int(m.group(2)), os.path.join(VAN, n)))
    return sorted(out)


def main():
    ts = tiles()
    rng = np.random.RandomState(7)
    pick = [ts[i] for i in rng.choice(len(ts), 200, replace=False)]

    for name, place in CANDS:
        bnd = []
        itr = []
        for tx, ty, p in pick:
            img = decode_rgb(DDS(p), MIP).astype(np.int16)
            # reassemble the 4x4 cells into a world-oriented image:
            # world row 0 = NORTH, world col 0 = WEST, cell (dx,dy) at
            # world block (3-dy, dx).
            w = np.zeros_like(img)
            for dy in range(4):
                for dx in range(4):
                    r, c = place(dx, dy)
                    w[(3 - dy) * CP:(4 - dy) * CP, dx * CP:(dx + 1) * CP] = \
                        img[r * CP:(r + 1) * CP, c * CP:(c + 1) * CP]
            dv = np.abs(np.diff(w.astype(np.int16), axis=0)).mean(axis=(1, 2))
            dh = np.abs(np.diff(w.astype(np.int16), axis=1)).mean(axis=(0, 2))
            rows = np.arange(len(dv))
            cols = np.arange(len(dh))
            bmask_r = np.isin(rows, [CP - 1, 2 * CP - 1, 3 * CP - 1])
            bmask_c = np.isin(cols, [CP - 1, 2 * CP - 1, 3 * CP - 1])
            bnd.append(np.r_[dv[bmask_r], dh[bmask_c]].mean())
            itr.append(np.r_[dv[~bmask_r], dh[~bmask_c]].mean())
        b, i = float(np.mean(bnd)), float(np.mean(itr))
        print('%-32s  seam %6.3f  interior %6.3f  ratio %6.3f'
              % (name, b, i, b / i))
    print('(200 random level-4 tiles, mip %d; ratio 1.0 = seamless)' % MIP)


if __name__ == '__main__':
    main()
