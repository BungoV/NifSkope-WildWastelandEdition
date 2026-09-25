"""GRASSCOL gate on grass_gate.sh output: grass/<tag> (--grass-tint 1.0) against grass/<tag>0 (the SAME exe, --grass-tint 0).
The law is c1 = c0 + (Ttex - c0) * w, w = cover/255, so each texel with cover >= 64 gives Ttex = (c1 - (1-w) c0) / w.
Re-registered 13:53 (the first form wanted cover 255 texels; the region's cover peaks at 159, so it measured nothing).
 RED (the defect): mean Ttex near white -- min channel > 200.
 GREEN: max channel < 170 AND G >= B + 15 in BOTH regions, >= 10000 texels each. (The census's alpha-weighted grass
 means are olive/green: DriedGrassObj01 (65,69,40), PreWarLawnGrass (126,136,82).)
usage: grass_gate.py grass/<tag>"""
import sys, glob, os, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import vtread
def load(d, k):
    f = glob.glob('%s/%s/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt' % (d, k))[0]
    v = vtread.Vt(f); cols, covs = [], []; B, C = v.border, v.content
    for i in range(v.tileCount):
        c = v.sheet(i, 1); m = v.sheet(i, 5)
        if c is None or m is None: continue
        cols.append(c[B:B + C, B:B + C, :3].reshape(-1, 3)); covs.append(m[B:B + C, B:B + C, 3].ravel())
    return np.concatenate(cols).astype(float), np.concatenate(covs).astype(float)
ok = True
for k in ('sanc', 'g248'):
    c1, cv = load(sys.argv[1], k); c0, cv0 = load(sys.argv[1] + '0', k)
    same = bool((cv == cv0).all())
    sel = cv >= 64; w = cv[sel, None] / 255
    T = (c1[sel] - (1 - w) * c0[sel]) / w; n = int(sel.sum()); mean = T.mean(0)
    red = mean.min() > 200; green = same and n >= 10000 and mean.max() < 170 and mean[1] >= mean[2] + 15
    ok &= green
    print('%s texels cover>=64 %d | cover planes identical %s | mean Ttex (%.0f %.0f %.0f) | mean c0 (%.0f %.0f %.0f) c1 (%.0f %.0f %.0f) | RED(white) %s GREEN %s' % (
        k, n, same, *mean, *c0[sel].mean(0), *c1[sel].mean(0), red, green))
print('GRASSCOL gate', 'GREEN' if ok else 'RED')
