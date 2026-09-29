"""TERRLIVE1 rule paint: the look difference in a rendered view, switch off vs on (same camera).
usage: python pic_drift.py <off.png> <on.png> <panels>   (60 px title bar; panels side by side)
Ground pixels = pixels where either render differs from the sky colour (top-left of the panel).
Per panel: mean |d lum| and |d rgb| over ground, split into the upper and lower half of the ground rows
(upper = farther for a low view), plus the mean colour off and on."""
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(np.float32)[60:]
b = np.asarray(Image.open(sys.argv[2]).convert('RGB')).astype(np.float32)[60:]
n = int(sys.argv[3]); W = a.shape[1] // n
lum = lambda c: 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]
for p in range(n):
    A, B = a[:, p * W:(p + 1) * W - 1], b[:, p * W:(p + 1) * W - 1]
    sky = A[2, 2]
    g = (np.abs(A - sky).sum(-1) > 6) | (np.abs(B - sky).sum(-1) > 6)
    rows = np.where(g.any(1))[0]; mid = (rows.min() + rows.max()) // 2
    dl = np.abs(lum(B) - lum(A)); dr = np.abs(B - A).mean(-1)
    out = []
    for name, sel in (('all', g), ('upper/far', g & (np.arange(g.shape[0])[:, None] < mid)),
                      ('lower/near', g & (np.arange(g.shape[0])[:, None] >= mid))):
        out.append('%s |d lum| %.2f |d rgb| %.2f' % (name, dl[sel].mean(), dr[sel].mean()))
    print('panel %d: %s; mean rgb off %s on %s' % (p, '; '.join(out), np.round(A[g].mean(0), 1), np.round(B[g].mean(0), 1)))
