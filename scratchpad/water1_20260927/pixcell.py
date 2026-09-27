#!/usr/bin/env python3
"""WATER1 task 3: the flat pictures must not change except where the sloped water is.

  pixcell.py <before.png> <after.png> <after.cam.log> <cells "x,y,zmin,zmax;..."> [--margin N] [--floor]

Pre-registered (written before the first run): every pixel that differs between the two pictures lies
inside the projected footprint of the named cells (convex hull of each cell's 8 box corners, cell x/y
and its ground zmin..zmax, projected with the render's own .cam.log, orthographic, the convention of
skill ww-picture-point-to-cell), dilated by --margin px (default 3). Prints the differing pixel count,
how many are outside, and the diff's bbox. --floor paints a 5x5 block into the after-picture far from
the cells: must FAIL. The projection itself is checked first: the pictures' size must be the cam line's
vp, or the row offset between them is printed and used.
"""
import re
import sys

import numpy as np
from PIL import Image, ImageDraw


def R(ax, deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    return {'x': np.array([[1, 0, 0], [0, c, -s], [0, s, c]]), 'z': np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])}[ax]


def main():
    a = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(np.int16)
    b = np.asarray(Image.open(sys.argv[2]).convert('RGB')).astype(np.int16).copy()
    cam = open(sys.argv[3]).read()
    cells = [tuple(float(v) for v in c.split(',')) for c in sys.argv[4].split(';') if c]
    margin = int(sys.argv[sys.argv.index('--margin') + 1]) if '--margin' in sys.argv else 3
    if a.shape != b.shape:
        print('FAIL sizes differ %s %s' % (a.shape, b.shape))
        sys.exit(1)
    rot = [float(v) for v in re.search(r'rot=([-\d.]+),([-\d.]+),([-\d.]+)', cam).groups()]
    look = np.array([float(v) for v in re.search(r'lookat=([-\d.]+),([-\d.]+),([-\d.]+)', cam).groups()])
    vw, vh = [int(v) for v in re.search(r'vp=(\d+)x(\d+)', cam).groups()]
    upp = float(re.search(r'upp=([\d.]+)', cam).group(1))
    H, Wd = a.shape[:2]
    dy = vh - H   # the picture is the viewport's lower rows when shorter (title-bar allowance)
    M = R('x', rot[0]) @ R('z', rot[2])
    mask = Image.new('L', (Wd, H), 0)
    dr = ImageDraw.Draw(mask)
    for cx, cy, z0, z1 in cells:
        pts = []
        for wx in (cx * 4096, (cx + 1) * 4096):
            for wy in (cy * 4096, (cy + 1) * 4096):
                for wz in (z0, z1):
                    p = M @ (np.array([wx, wy, wz]) - look)
                    pts.append((vw / 2 + p[0] / upp, vh / 2 - p[1] / upp - dy))
        pts = np.array(pts)
        # convex hull (monotone chain)
        P = sorted(map(tuple, pts))

        def cross(o, p, q):
            return (p[0] - o[0]) * (q[1] - o[1]) - (p[1] - o[1]) * (q[0] - o[0])
        lo, hi = [], []
        for p in P:
            while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
                lo.pop()
            lo.append(p)
        for p in reversed(P):
            while len(hi) >= 2 and cross(hi[-2], hi[-1], p) <= 0:
                hi.pop()
            hi.append(p)
        hull = lo[:-1] + hi[:-1]
        dr.polygon(hull, fill=255)
        print('cell %d,%d hull x %.0f..%.0f y %.0f..%.0f' % (cx, cy, min(h[0] for h in hull), max(h[0] for h in hull),
                                                            min(h[1] for h in hull), max(h[1] for h in hull)))
    m = np.asarray(mask) > 0
    if margin:
        from PIL import ImageFilter
        m = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * margin + 1))) > 0
    if '--floor' in sys.argv:
        ys, xs = np.nonzero(~m)
        k = len(ys) // 2
        b[ys[k]:ys[k] + 5, xs[k]:xs[k] + 5] ^= 0x40
    d = np.any(a != b, axis=2)
    n = int(d.sum())
    out = int((d & ~m).sum())
    ys, xs = np.nonzero(d)
    bbox = (xs.min(), ys.min(), xs.max(), ys.max()) if n else None
    print('differing pixels %d, outside the cells\' footprint %d, inside %d, diff bbox %s, footprint %d px'
          % (n, out, n - out, bbox, int(m.sum())))
    print('FAIL' if out else 'PASS')
    sys.exit(1 if out else 0)


main()
