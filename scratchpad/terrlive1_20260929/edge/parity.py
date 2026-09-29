"""TERRLIVE1: live-vs-baked parity per quadrant on a close-up pair (same camera, overhead ortho).
usage: python parity.py <baked.png> <dynamic.png> cx cy half
Prints the per-quadrant |lum baked - lum dynamic| split by the quadrant's class and its weight zone."""
import sys, os, numpy as np
from PIL import Image
from scipy import ndimage
H = os.path.dirname(os.path.abspath(__file__))
d = np.load(os.path.join(H, 'cells.npz')); realq, minX, minY = d['realq'], int(d['minX']), int(d['minY'])
a = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(np.float32)[60:]
b = np.asarray(Image.open(sys.argv[2]).convert('RGB')).astype(np.float32)[60:]
cx, cy, half = map(float, sys.argv[3:6])
Hh, Ww = a.shape[:2]; upp = 2 * half / max(Hh, Ww)
lum = lambda c: 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]
rr, cc = np.mgrid[0:Hh, 0:Ww]
wx = cx + (cc + 0.5 - Ww / 2) * upp; wy = cy - (rr + 0.5 - Hh / 2) * upp
qx = np.floor(wx / 2048).astype(int); qy = np.floor(wy / 2048).astype(int)
pq = realq.astype(bool)
din = ndimage.distance_transform_edt(pq) * 2048.0   # quadrant steps inside, coarse
Q = pq[qy - 2 * minY, qx - 2 * minX]; DI = din[qy - 2 * minY, qx - 2 * minX]
diff = np.abs(lum(a) - lum(b))
print(f'{os.path.basename(sys.argv[1])} vs dynamic: {upp:.2f} u/px, mean |lum diff| {diff.mean():.2f}')
for name, m in (('unpainted (vanilla)', ~Q), ('painted, band (<=8 km)', Q & (DI <= 8192)), ('painted, deep (>8 km)', Q & (DI > 8192))):
    if m.sum():
        print(f'  {name}: {int(m.sum())} px, mean |diff| {diff[m].mean():.2f}, lum baked {lum(a)[m].mean():.1f}, dynamic {lum(b)[m].mean():.1f}')
# the worst quadrants: dynamic darker than baked by the most
key = (qy - qy.min()) * (qx.max() - qx.min() + 1) + (qx - qx.min())
n = np.bincount(key.ravel()); sd = np.bincount(key.ravel(), (lum(b) - lum(a)).ravel())
k = np.argsort(sd / np.maximum(n, 1))[:6]
for kk in k:
    sel = key == kk
    print(f'  darkest quad q=({qx[sel][0]},{qy[sel][0]}) painted={bool(Q[sel][0])} in-dist {DI[sel][0]:.0f}: dyn-baked {(sd[kk]/n[kk]):.1f}')
