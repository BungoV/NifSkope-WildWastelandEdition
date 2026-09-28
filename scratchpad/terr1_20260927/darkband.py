"""TERR1 probe: where does the ON bake darken mask B by > 100 vs base, and what is overhead there?"""
import sys, numpy as np, vtmosaic as VM
from canyon import load_objh
C = 'mod/FO4CSLOD/Commonwealth'
V2 = '/Commonwealth.VT.2.lodt'; on, base = VM.Sheets('bakes/on/' + C + V2), VM.Sheets('bakes/base/' + C + V2)
gx0, gy0, gw, gh, cell, HI, LO = load_objh('bakes/objh_on.bin')
a = on.mosaic(5)[..., 2].astype(int); b = base.mosaic(5)[..., 2].astype(int)
H = VM.height_units(on.mosaic(4))
N = a.shape[0]; jj, ii = np.mgrid[0:N, 0:N]
x = VM.WX0 + (ii + .5) * VM.UPT; y = VM.WYTOP - (jj + .5) * VM.UPT
gx = np.clip(np.floor(x / cell).astype(int) - gx0, 0, gw - 1); gy = np.clip(np.floor(y / cell).astype(int) - gy0, 0, gh - 1)
hi = HI[gy, gx]; lo = LO[gy, gx]
dark = (b - a) > 100
print('texels', N * N, 'darkened>100', int(dark.sum()), 'on mean %.1f base mean %.1f' % (a.mean(), b.mean()))
for name, m in (('dark', dark), ('all', np.ones_like(dark))):
    t = H[m]; o = hi[m]
    covered = o > -1e29
    print(name, 'terrain h p10/50/90 %.0f %.0f %.0f' % tuple(np.percentile(t, [10, 50, 90])),
          'under an object top %.1f%%' % (100 * covered.mean()),
          'obj top - terrain p50 %.0f' % (np.median((o - t)[covered]) if covered.any() else float('nan')),
          'terrain < 0: %.1f%%' % (100 * (t < 0).mean()))
# where: per-cell count of darkened texels (cells 256 texels)
cc = dark.reshape(N // 256, 256, N // 256, 256).sum(axis=(1, 3))
for j in range(cc.shape[0]):
    print('cy %3d ' % (-1 - j) + ' '.join('%5d' % v for v in cc[j]))
cov = hi > -1e29
for lab, m in (('dark & open (no object top overhead)', dark & ~cov), ('under an object top', cov)):
    cc = m.reshape(N // 256, 256, N // 256, 256).mean(axis=(1, 3)) * 100
    print(lab, '% of each cell; columns cx', list(range(-8, 4)))
    for j in range(cc.shape[0]):
        print('cy %3d ' % (-1 - j) + ' '.join('%4.0f' % v for v in cc[j]))
