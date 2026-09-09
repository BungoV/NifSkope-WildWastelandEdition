"""Sweep every level-4 vanilla LOD diffuse tile into a per-cell pixel cube.

Output cells.npz:
  pix   (192,192,32,32,3) uint8 -- each cell's mip-2 diffuse, ROW 0 = NORTH,
                                   COL 0 = WEST (texture order, not VHGT order)
  have  (192,192) bool         -- a tile existed for this cell
  vclr  (192,192,32,32,3) float32 -- the VCLR multiplier resampled onto those
                                   texels; 1.0 where the cell has no VCLR

Orientation is the brief's measured one: tile Commonwealth.4.X.Y covers cells
X..X+3, Y..Y+3, and cell (X+dx, Y+dy) is sub-block column dx, row 3-dy.

Quadrants (0 BL, 1 BR, 2 TL, 3 TR) therefore slice as
  q2 = rows  0:16, cols  0:16      q3 = rows  0:16, cols 16:32
  q0 = rows 16:32, cols  0:16      q1 = rows 16:32, cols 16:32
"""
import os
import re
import sys
import time

import numpy as np

from dds import DDS
from bcnp import decode_rgb

HERE = os.path.dirname(os.path.abspath(__file__))
VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
LEVEL = 4
MIP = 2            # 128x128 per tile -> 32x32 per cell -> 16x16 per quadrant
CELLPX = 32
MINX = MINY = -96
N = 192


def main():
    pat = re.compile(r'^Commonwealth\.%d\.(-?\d+)\.(-?\d+)\.DDS$' % LEVEL, re.I)
    tiles = {}
    for n in os.listdir(VAN):
        m = pat.match(n)
        if m:
            tiles[(int(m.group(1)), int(m.group(2)))] = os.path.join(VAN, n)
    print('level-%d diffuse tiles: %d' % (LEVEL, len(tiles)))

    pix = np.zeros((N, N, CELLPX, CELLPX, 3), dtype=np.uint8)
    have = np.zeros((N, N), dtype=bool)
    t0 = time.time()
    for k, ((tx, ty), path) in enumerate(sorted(tiles.items())):
        img = decode_rgb(DDS(path), MIP)          # (128,128,3), row 0 = north
        for dy in range(4):
            for dx in range(4):
                cx, cy = tx + dx, ty + dy
                if not (MINX <= cx < MINX + N and MINY <= cy < MINY + N):
                    continue
                r0 = (3 - dy) * CELLPX
                c0 = dx * CELLPX
                pix[cy - MINY, cx - MINX] = img[r0:r0 + CELLPX, c0:c0 + CELLPX]
                have[cy - MINY, cx - MINX] = True
        if k % 400 == 0:
            print('  %4d/%d  %.1fs' % (k, len(tiles), time.time() - t0))
    print('cells covered by a tile: %d of %d  (%.1fs)'
          % (have.sum(), N * N, time.time() - t0))

    # VCLR -> per-texel multiplier.  The 33x33 vertex grid is row 0 SOUTH,
    # col 0 WEST; the texels are row 0 NORTH.  Texel i's centre sits at vertex
    # coordinate i+0.5, i.e. the mean of vertices i and i+1, so a 2x2 box over
    # the 33x33 grid resamples it exactly.
    z = np.load(os.path.join(HERE, 'lens2', 'land3C.npz'))
    col = z['colors'].astype(np.float32)          # (192,192,33,33,3)
    pv = z['present_vclr'].astype(bool)
    g = col[:, :, :32, :32] + col[:, :, 1:33, :32] + col[:, :, :32, 1:33] + col[:, :, 1:33, 1:33]
    g /= 4.0 * 255.0
    g = g[:, :, ::-1, :, :]                        # south-up -> north-down
    vclr = np.ones((N, N, 32, 32, 3), dtype=np.float32)
    vclr[pv] = g[pv]
    print('vclr multiplier where present: mean %.4f  min %.4f'
          % (vclr[pv].mean(), vclr[pv].min()))

    out = os.path.join(HERE, 'cells.npz')
    np.savez(out, pix=pix, have=have, vclr=vclr.astype(np.float32),
             minx=MINX, miny=MINY, mip=MIP)
    print('wrote %s  (%.1f MB)' % (out, os.path.getsize(out) / 1e6))


if __name__ == '__main__':
    sys.exit(main())
