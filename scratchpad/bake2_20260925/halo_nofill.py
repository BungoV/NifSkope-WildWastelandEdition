"""BAKE2 halo: brightness by distance from LAND for a fill-OFF VT (no vanilla LOD loose): one field only.
usage: python halo_nofill.py <land dump> <mod ws dir> <world> <x0> <y0> <x1> <y1>"""
import sys, struct, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread
dump, d_, world = sys.argv[1:4]; x0, y0, x1, y1 = map(int, sys.argv[4:8])
b = open(dump, 'rb').read(); mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
fl = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
W, H = x1 - x0 + 1, y1 - y0 + 1
land = np.array([[0 <= x0 + c - mnx < cw and 0 <= y1 - r - mny < ch and fl[y1 - r - mny, x0 + c - mnx] != 0
                  for c in range(W)] for r in range(H)])
dist = np.full((H, W), 99); dist[land] = 0; cur = land.copy()
for d in range(1, 80):
    p = np.pad(cur, 1); nb = np.zeros_like(cur)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1): nb |= p[1 + dy:1 + dy + H, 1 + dx:1 + dx + W]
    dist[nb & ~cur] = d; cur = nb
    if cur.all(): break
v = vtread.Vt('%s/%s.VT.4.lodt' % (d_, world)); m, wW, nN = v.mosaic(x0, y0, x1, y1); per = v.content // v.levelDim
f = np.zeros((H, W, 3), np.float32)
for r in range(H):
    for c in range(W):
        rr = (nN - 1 - (y1 - r)) * per; cc = (x0 + c - wW) * per
        f[r, c] = m[rr:rr + per, cc:cc + per, :3].reshape(-1, 3).mean(0)
K = np.array([0.2126, 0.7152, 0.0722], np.float32); lo = f @ K; co = f.max(2) - f.min(2)
print('%s %d..%d x %d..%d: LAND %d, no-LAND %d (fill off)' % (world, x0, x1, y0, y1, land.sum(), (~land).sum()))
print(' d  cells   lum  chroma')
for d in [0, 1, 2, 3, 4, 5, 6, 8, 12]:
    s = dist == d
    if s.any(): print('%2d %6d %6.1f %6.1f' % (d, s.sum(), lo[s].mean(), co[s].mean()))
s = dist >= 6; print('>=6 %5d %6.1f %6.1f' % (s.sum(), lo[s].mean(), co[s].mean()))
