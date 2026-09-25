# "Trees on grey": which placement cells stand on the flat grey that unpainted LAND (no BTXT/ATXT) used to get.
# Per cell of a terrain-only top-down render: mean chroma (max-min of RGB) and mean colour over the central half of
# the cell's pixel footprint. Calibrated on the BEFORE render with the ESM's own painted/unpainted split (SEAM1's
# pickle of Fallout4.esm), then the same threshold is applied to both pictures.
# usage: python grey_cells.py <T_before.png> <T_after.png> <x0> <y0> <x1> <y1> <ortho half-width>
import sys, pickle
import numpy as np
from PIL import Image
sys.path.insert(0, r'E:/Projects/NifskopeWWE-extent1/scratchpad/extent1_20260925')
TB, TA = sys.argv[1:3]
x0, y0, x1, y1 = map(int, sys.argv[3:7]); ORT = float(sys.argv[7])
import composite_cells as cc
cells = cc.placement_cells()
d = pickle.load(open(r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/fo4esm_cw.pkl', 'rb'))
lands = set(d['lands'])
painted = {k for k, Ld in d['lands'].items() if any(Ld['base']) or any(len(q) for q in Ld['layers'])}
def cellstats(p):
    im = np.asarray(Image.open(p).convert('RGB')).astype(np.float64)
    h, w, _ = im.shape
    upp = 2 * ORT / w
    CX = (x0 + x1 + 1) * 2048.0; CY = (y0 + y1 + 1) * 2048.0
    r = max(1, int(1024 / upp))
    out = {}
    for x in range(x0, x1 + 1):
        for y in range(y0, y1 + 1):
            u = int(round(w / 2 + ((x + 0.5) * 4096 - CX) / upp)); v = int(round(h / 2 - ((y + 0.5) * 4096 - CY) / upp))
            patch = im[max(0, v - r):v + r + 1, max(0, u - r):u + r + 1].reshape(-1, 3)
            out[(x, y)] = (float((patch.max(1) - patch.min(1)).mean()), patch.mean(0))
    return out
SB = cellstats(TB); SA = cellstats(TA)
pc = [SB[k][0] for k in painted if k in SB]; uc = [SB[k][0] for k in lands - painted if k in SB]
q = lambda a: ' '.join('%.1f' % v for v in np.percentile(a, [1, 5, 50, 95, 99]))
print('calibration (BEFORE picture), per-cell chroma percentiles 1/5/50/95/99:')
print('   painted LAND   %5d cells: %s' % (len(pc), q(pc)))
print('   unpainted LAND %5d cells: %s' % (len(uc), q(uc)))
thr = (np.percentile(uc, 99) + np.percentile(pc, 1)) / 2 if np.percentile(uc, 99) < np.percentile(pc, 1) else None
if thr is None:
    thr = np.percentile(uc, 95)
    print('   classes OVERLAP (unpainted p99 >= painted p1); threshold = unpainted p95 %.1f, read with care' % thr)
else:
    print('   classes separate; threshold (midpoint of unpainted p99 and painted p1) = %.1f' % thr)
for name, S in (('BEFORE', SB), ('AFTER', SA)):
    g = [(k, n) for k, n in cells.items() if S[k][0] <= thr]
    gl = [k for k in lands if S[k][0] <= thr]
    print('%s: placement cells on grey (chroma <= %.1f): %d of %d cells, %d of %d placements; LAND cells on grey %d of %d'
          % (name, thr, len(g), len(cells), sum(n for _, n in g), sum(cells.values()), len(gl), len(lands)))
    print('   of those placement cells, painted LAND in the ESM (grey by its own texture): %d; unpainted: %d'
          % (sum(k in painted for k, _ in g), sum(k not in painted for k, _ in g)))
