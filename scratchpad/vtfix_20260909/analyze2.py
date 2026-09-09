#!/usr/bin/env python
"""VTFIX part 2: replicate the V9a difference over every dim-4 chunk in the
fixture, and place it against vanilla's own shipped sheet for the same chunk.

Replication is the independent twin ww-control-calibration asks for: the floor
and the ceiling are built from the subject's own data, so the claim that the
difference is a chunk-boundary property is tested on four different chunks with
four different paint sets rather than asserted from one.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ddsdiff import load_mip0

HERE = os.path.dirname(os.path.abspath(__file__))
B = os.path.join(HERE, 'bake')
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
CHUNKS = ['Commonwealth.4.-24.24', 'Commonwealth.4.-20.24',
          'Commonwealth.4.-24.28', 'Commonwealth.4.-20.28']

def sheet(run, stem, suffix=''):
    return os.path.join(B, run, 'tex', stem + suffix + '.DDS')

def stats(a, b):
    ra, _, w, h, _, _ = load_mip0(a)
    rb, _, w2, h2, _, _ = load_mip0(b)
    assert (w, h) == (w2, h2), (a, b, w, h, w2, h2)
    n = 0; mx = 0; s = 0; band = 0; seammin = 10 ** 9
    for y in range(h):
        for x in range(w):
            pa, pb = ra[y * w + x], rb[y * w + x]
            m = max(abs(pa[k] - pb[k]) for k in range(3))
            if m:
                n += 1; s += m
                if m > mx: mx = m
                d = min(x, w - 1 - x, y, h - 1 - y)
                if d < 4: band += 1
                sm = min(abs(x - w // 2), abs(y - h // 2))
                if sm < seammin: seammin = sm
    return n, w * h, mx, (s / n if n else 0.0), band, (seammin if n else -1)

print('%-24s %-34s %9s %6s %7s %10s %8s' %
      ('chunk', 'comparison', 'differing', 'max', 'mean|d|', 'in 4-band', 'min seam'))
for stem in CHUNKS:
    for label, a, b in (
        ('FLOOR  no-cover asm vs direct', sheet('vt_nc', stem), sheet('dir_nc', stem)),
        ('CEILING direct cover vs nocover', sheet('dir_cover', stem), sheet('dir_nc', stem)),
        ('UNDER TEST cover asm vs direct', sheet('vt_cover', stem), sheet('dir_cover', stem)),
    ):
        n, tot, mx, mn, band, seam = stats(a, b)
        print('%-24s %-34s %5d/%d %6d %7.3f %9d%s %8s' %
              (stem, label, n, tot, mx, mn, band,
               (' (%.0f%%)' % (100.0 * band / n)) if n else '      ', seam))

print()
print('== against vanilla\'s own shipped sheet, same chunk ==')
for stem in CHUNKS:
    v = os.path.join(VAN, stem + '.DDS')
    if not os.path.exists(v):
        print('%-24s no vanilla sheet' % stem)
        continue
    for run in ('vt_cover', 'dir_cover', 'dir_nc'):
        n, tot, mx, mn, band, seam = stats(sheet(run, stem), v)
        print('%-24s %-12s vs vanilla: differing %d/%d (%.1f%%) max %d mean|d| %.3f'
              % (stem, run, n, tot, 100.0 * n / tot, mx, mn))
