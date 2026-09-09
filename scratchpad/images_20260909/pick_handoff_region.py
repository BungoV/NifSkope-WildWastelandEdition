"""Pick the terrain region for the FO4CS handoff pictures, from VANILLA's own
shipped far LOD and from the MASTER's heights only -- never from anything we
generated (CONSTITUTION rule 4; the circular check bungo caught once).

For every dim-4 tile whose four-by-four cell block lies WHOLLY INSIDE the
painted box (x -36..32, y -41..32, from lodgen --dump-layers), measure on
vanilla's shipped sheets at mip 2:

  colour = mean over texels of (max(RGB) - min(RGB))   -- blank/grey scores ~0
  relief = mean tilt of the _msn normal away from up, degrees (UP IS GREEN)

and from the ESM heights (land_all.bin, VHGT*8 world units, dumped by
`lodgen --dump-land`):

  span  = max - min height over the block, world units
  shore = fraction of height samples below the water plane

A shoreline tile is one where shore is neither 0 nor 1: the block holds both
sea floor and dry land. The score is colour * relief * (a shoreline bonus), and
the whole table is printed so the spread is visible.

  python pick_handoff_region.py [dim]
"""
import os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MOUNT = os.path.join(HERE, '..', 'mountains_20260907')
sys.path.insert(0, MOUNT)
from dds import DDS
from bcnp import decode_rgb

TEX = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
LAND = os.path.join(MOUNT, 'land_all.bin')
PX0, PX1, PY0, PY1 = -36, 32, -41, 32
WATER = -300.0       # Commonwealth's flat sea bed sits at exactly -352 world
                     # units (21,099,747 of 36,864 cells' samples, the single
                     # commonest value below 0); anything under -300 is sea bed.


def load_land():
    b = np.fromfile(LAND, dtype=np.uint8)
    minx, miny, cw, ch = np.frombuffer(b[:16].tobytes(), dtype='<i4')
    off = 16
    present = b[off:off + cw * ch].astype(bool).reshape(ch, cw)
    off += cw * ch
    grid = np.frombuffer(b[off:].tobytes(), dtype='<i2').reshape(ch, cw, 33, 33)
    return int(minx), int(miny), present, grid


def block_heights(land, x, y, dim):
    minx, miny, present, grid = land
    vals = []
    for cy in range(dim):
        for cx in range(dim):
            j, i = y + cy - miny, x + cx - minx
            if 0 <= j < present.shape[0] and 0 <= i < present.shape[1] and present[j, i]:
                vals.append(grid[j, i].astype(np.float64) * 8.0)
    if not vals:
        return None
    return np.concatenate([v.ravel() for v in vals])


def tiles(dim):
    for n in os.listdir(TEX):
        if not n.upper().endswith('.DDS') or '_msn' in n:
            continue
        p = n.split('.')
        if len(p) < 5 or p[1] != str(dim):
            continue
        yield int(p[2]), int(p[3]), n[:-4]


def measure(stem):
    dp = os.path.join(TEX, stem + '.DDS')
    mp = os.path.join(TEX, stem + '_msn.DDS')
    if not (os.path.exists(dp) and os.path.exists(mp)):
        return None
    d = DDS(dp)
    rgb = decode_rgb(d, min(2, d.mips - 1)).astype(np.int16)
    colour = float((rgb.max(2) - rgb.min(2)).mean())
    m = DDS(mp)
    n = decode_rgb(m, min(2, m.mips - 1)).astype(np.float32) / 255.0 * 2.0 - 1.0
    up = np.clip(n[:, :, 1], -1.0, 1.0)
    relief = float(np.degrees(np.arccos(up)).mean())
    return colour, relief


def main():
    dim = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    land = load_land()
    rows = []
    for x, y, stem in tiles(dim):
        if x < PX0 or x + dim - 1 > PX1 or y < PY0 or y + dim - 1 > PY1:
            continue
        mm = measure(stem)
        if mm is None:
            continue
        colour, relief = mm
        h = block_heights(land, x, y, dim)
        if h is None:
            continue
        shore = float((h < WATER).mean())
        span = float(h.max() - h.min())
        bonus = 1.0 if 0.08 <= shore <= 0.65 else 0.25
        rows.append((colour * relief * bonus, colour, relief, shore, span, x, y, stem))
    rows.sort(reverse=True)
    print('%-9s %-7s %-7s %-6s %-9s %s' % ('score', 'colour', 'relief', 'shore', 'span', 'tile'))
    for r in rows:
        print('%-9.1f %-7.2f %-7.2f %-6.3f %-9.0f %s' % (r[0], r[1], r[2], r[3], r[4], r[7]))
    print('rows=%d' % len(rows))


if __name__ == '__main__':
    main()
