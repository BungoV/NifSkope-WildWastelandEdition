"""TERRLIVE1 rule paint: how far does the rule paint drift from vanilla outside our painted area?
usage: python drift.py <off VT.N.lodt> <on VT.N.lodt>
The OFF bake writes vanilla's dim-4 diffuse outside (law 2), so on - off outside = the rule paint's drift
from what Bethesda painted there (plus BC1 noise, which the two share). Texels are classed by distance to
the nearest painted quadrant: band (0..8192 u, the join), near (8..32 km) and far (> 32 km).
Prints mean |d lum|, mean |d rgb|, mean signed d lum and the 95th percentile, 0..255 units."""
import sys, os
import numpy as np
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtread
H = os.path.dirname(os.path.abspath(__file__))
d = np.load(os.path.join(H, 'cells.npz')); realq, minX, minY = d['realq'], int(d['minX']), int(d['minY'])
a, b = vtread.Vt(sys.argv[1]), vtread.Vt(sys.argv[2])
x0, y0 = b.west, b.south
x1, y1 = b.east, b.north
ma, wW, nN = a.mosaic(x0, y0, x1, y1, 1)
mb, _, _ = b.mosaic(x0, y0, x1, y1, 1)
upt = b.levelDim * 4096.0 / b.content
Hh, Ww = mb.shape[:2]
# distance of each texel to the nearest painted quadrant, on the quadrant grid then sampled
qd = ndimage.distance_transform_edt(~realq) * 2048.0
rr, cc = np.mgrid[0:Hh, 0:Ww]
wx = wW * 4096.0 + (cc + 0.5) * upt; wy = nN * 4096.0 - (rr + 0.5) * upt
qy = np.clip(np.floor(wy / 2048).astype(int) - 2 * minY, 0, realq.shape[0] - 1)
qx = np.clip(np.floor(wx / 2048).astype(int) - 2 * minX, 0, realq.shape[1] - 1)
dist = qd[qy, qx]
present = (ma[..., 3] > 0) | (ma[..., :3].sum(-1) > 0)
lum = lambda c: 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]
A, B = ma[..., :3].astype(np.float32), mb[..., :3].astype(np.float32)
dl = lum(B) - lum(A)
drgb = np.abs(B - A).mean(-1)
print('drift %s: level %d, %d x %d texels at %.0f u' % (os.path.basename(sys.argv[2]), b.levelDim, Ww, Hh, upt))
for name, sel in (('painted', dist == 0), ('band 0-8 km', (dist > 0) & (dist <= 8192)),
                  ('near 8-32 km', (dist > 8192) & (dist <= 32768)), ('far > 32 km', dist > 32768),
                  ('all outside', dist > 0)):
    s = sel & present
    n = int(s.sum())
    if n == 0:
        print('  %-13s none' % name); continue
    ad = np.abs(dl[s])
    print('  %-13s %9d texels  |d lum| %6.2f  |d rgb| %6.2f  signed %+6.2f  p95 %6.2f  changed(>2) %5.1f%%' % (
        name, n, ad.mean(), drgb[s].mean(), dl[s].mean(), np.percentile(ad, 95), 100.0 * (ad > 2).mean()))
