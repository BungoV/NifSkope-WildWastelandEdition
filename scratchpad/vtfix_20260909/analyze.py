#!/usr/bin/env python
"""VTFIX: attribute and bound the V9a colour difference.

FLOOR   the same metric on the --no-cover pair, which must read 0 texels.
CEILING the tint's own amplitude: the direct bake with --cover against the
        direct bake without it -- the most colour the mechanism under test can
        move on this chunk.
UNDER TEST the assembled sheet against the direct one, both --cover.

Locality is reported against the CHUNK'S OUTER boundary (the spec's own bar,
docs V9c) and against the interior dim-2 TILE seams, because those are the two
rival explanations: a ring-vs-clamp difference hugs the outer boundary, a
tile-scoping difference hugs the interior seams.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ddsdiff import load_mip0

B = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'bake')
STEM = 'Commonwealth.4.-24.24'

def sheet(run, suffix=''):
    return os.path.join(B, run, 'tex', STEM + suffix + '.DDS')

def diff(a, b, label):
    ra, aa, w, h, fa, ma = load_mip0(a)
    rb, ab, w2, h2, fb, mb = load_mip0(b)
    assert (w, h) == (w2, h2)
    n = 0
    maxd = [0, 0, 0]
    sumd = 0
    xs = []
    for y in range(h):
        for x in range(w):
            pa, pb = ra[y * w + x], rb[y * w + x]
            d = [abs(pa[k] - pb[k]) for k in range(3)]
            m = max(d)
            if m:
                n += 1
                sumd += m
                for k in range(3):
                    if d[k] > maxd[k]:
                        maxd[k] = d[k]
                xs.append((x, y, m))
    print('%-28s %s %dx%d mips=%d/%d  differing %d/%d (%.3f%%)  maxRGB %s  mean|d| %.3f'
          % (label, fa, w, h, ma, mb, n, w * h, 100.0 * n / (w * h), maxd,
             (sumd / n) if n else 0.0))
    return xs, w, h

def locality(xs, w, h, label):
    if not xs:
        print('    %s: nothing to localise' % label)
        return
    outer = [min(x, w - 1 - x, y, h - 1 - y) for x, y, _ in xs]
    seam = [min(abs(x - w // 2), abs(y - h // 2)) for x, y, _ in xs]
    for name, v in (('distance to the CHUNK OUTER boundary', outer),
                    ('distance to the interior TILE seam', seam)):
        v = sorted(v)
        n = len(v)
        print('    %s: min %d  median %d  p90 %d  max %d' %
              (name, v[0], v[n // 2], v[int(n * 0.9)], v[-1]))
    for band in (1, 2, 4, 8, 16, 32, 64):
        k = sum(1 for d in outer if d < band)
        print('      within %3d texels of the outer boundary: %6d of %6d (%5.1f%%)  '
              '[that band is %.1f%% of the sheet]'
              % (band, k, len(xs), 100.0 * k / len(xs),
                 100.0 * (w * h - max(0, w - 2 * band) * max(0, h - 2 * band)) / (w * h)))

print('== V9a, mip 0, chunk %s ==' % STEM)
print('-- FLOOR: the metric on inputs that must be identical --')
xs0, w, h = diff(sheet('vt_nc'), sheet('dir_nc'), 'no-cover: assembled vs direct')
print('-- CEILING: the tint\'s own amplitude on this chunk --')
xsc, _, _ = diff(sheet('dir_cover'), sheet('dir_nc'), 'direct: cover vs no-cover')
locality(xsc, w, h, 'ceiling')
print('-- UNDER TEST --')
xs1, _, _ = diff(sheet('vt_cover'), sheet('dir_cover'), 'cover: assembled vs direct')
locality(xs1, w, h, 'under test')
print('-- the msn sheets, the operand the cover gate reads --')
xsm, _, _ = diff(sheet('vt_cover', '_msn'), sheet('dir_cover', '_msn'), 'msn: assembled vs direct')
locality(xsm, w, h, 'msn')
