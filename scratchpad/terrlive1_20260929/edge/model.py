"""TERRLIVE1 law 2, offline before the build: colour = mix(V, ours, w), w = smoothstep(0, B, d_in), d_in = distance
from the texel to the nearest texel of an UNPAINTED QUADRANT (0 outside the painted quadrants), V = vanilla's
dim-4 diffuse untouched. Ours = the staged MERGE1 VT.8 (painted quadrants are untouched by the old fill).
Prints per band the luminance of the model and of V, and the worst dip: model below BOTH neighbours.
usage: python model.py cx0 cy0 cx1 cy1"""
import sys, os
import numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtread
cx0, cy0, cx1, cy1 = [int(v) for v in sys.argv[1:5]]
ST = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/stage/mod/FO4CSLOD/Commonwealth'
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
d = np.load('cells.npz'); realq, minX, minY = d['realq'], int(d['minX']), int(d['minY'])
vt = vtread.Vt(f'{ST}/Commonwealth.VT.8.lodt')
m, wW, nN = vt.mosaic(cx0, cy0, cx1, cy1, 1)
upt = 8 * 4096.0 / vt.content
H, W = m.shape[:2]
rr, cc = np.mgrid[0:H, 0:W]
wx = wW * 4096.0 + (cc + 0.5) * upt; wy = nN * 4096.0 - (rr + 0.5) * upt
pq = realq[np.floor(wy / 2048).astype(int) - 2 * minY, np.floor(wx / 2048).astype(int) - 2 * minX]
din = ndimage.distance_transform_edt(pq) * upt
dout = ndimage.distance_transform_edt(~pq) * upt
sd = np.where(pq, -din, dout)
ours = m[..., :3].astype(np.float32)
V = np.zeros_like(ours)
chx = np.floor(wx / 16384.0).astype(int) * 4; chy = np.floor(wy / 16384.0).astype(int) * 4
for key in set(zip(chx.ravel().tolist(), chy.ravel().tolist())):
    p = f'{VAN}/Commonwealth.4.{key[0]}.{key[1]}.DDS'
    sel = (chx == key[0]) & (chy == key[1])
    if not os.path.exists(p): V[sel] = ours[sel]; continue
    a = np.asarray(Image.open(p).convert('RGB')).astype(np.float32)
    tx = np.clip(((wx[sel] - key[0] * 4096.0) / 32.0).astype(int), 0, 510)
    ty = np.clip((((key[1] + 4) * 4096.0 - wy[sel]) / 32.0).astype(int), 0, 510)
    V[sel] = (a[ty, tx] + a[ty, tx + 1] + a[ty + 1, tx] + a[ty + 1, tx + 1]) / 4
lum = lambda c: 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]
bins = [-16384, -12288, -8192, -6144, -4096, -3072, -2048, -1024, 0, 1024, 2048, 4096, 8192]
print(f'cells {cx0}..{cx1} x {cy0}..{cy1}; painted-quadrant texels {int(pq.sum())} of {pq.size}')
print('B (u)  | ' + ' '.join(f'{a:+6d}' for a in bins[:-1]) + ' | worst dip | ours-share lost')
LV = lum(V); LO = lum(ours)
rowV = [LV[(sd >= a) & (sd < b)].mean() for a, b in zip(bins[:-1], bins[1:])]
print('V      | ' + ' '.join(f'{v:6.1f}' for v in rowV))
rowO = [LO[(sd >= a) & (sd < b)].mean() for a, b in zip(bins[:-1], bins[1:])]
print('ours   | ' + ' '.join(f'{v:6.1f}' for v in rowO) + '   (outside the quadrants = the old default ground)')
for B in (4096, 8192, 12288, 16384):
    t = np.clip(din / B, 0, 1); w = t * t * (3 - 2 * t)
    M = V + (ours - V) * w[..., None]
    LM = lum(M)
    row = [LM[(sd >= a) & (sd < b)].mean() for a, b in zip(bins[:-1], bins[1:])]
    dip = max(0.0, max(min(row[i - 1], row[i + 1]) - row[i] for i in range(1, len(row) - 1)))
    lost = 1 - w[pq].mean()
    print(f'{B:6d} | ' + ' '.join(f'{v:6.1f}' for v in row) + f' | {dip:5.2f}     | {lost*100:4.1f}%')
