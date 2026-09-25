"""BAKE2: is vanilla's dim-4 sheet registered to our cells? Correlate per-cell luminance of our VT (LAND cells)
with vanilla's at cell shifts -2..2; the best must be 0,0. usage: PHASE=px,py python align_check.py <land> <mod ws dir> <world> x0 y0 x1 y1"""
import sys, os, struct, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread, vanilla_tiles as VT
dump, d_, world = sys.argv[1:4]; x0, y0, x1, y1 = map(int, sys.argv[4:8])
px, py = map(int, os.environ.get('PHASE', '0,0').split(','))
b = open(dump, 'rb').read(); mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
fl = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
X0, Y0, X1, Y1 = x0 - 4, y0 - 4, x1 + 4, y1 + 4
W, H = X1 - X0 + 1, Y1 - Y0 + 1
def cells(m, wW, nN, per):
    o = np.zeros((H, W)); K = np.array([0.2126, 0.7152, 0.0722])
    for r in range(H):
        for c in range(W):
            rr = (nN - 1 - (Y1 - r)) * per; cc = (X0 + c - wW) * per
            blk = m[rr:rr + per, cc:cc + per, :3]
            o[r, c] = (blk.reshape(-1, 3) @ K).mean() if blk.size and rr >= 0 and cc >= 0 else np.nan
    return o
v = vtread.Vt('%s/%s.VT.4.lodt' % (d_, world)); m, wW, nN = v.mosaic(x0, y0, x1, y1)
ours = np.full((H, W), np.nan); oc = cells(m, wW, nN, v.content // v.levelDim)
ax = lambda z: z - ((z - px) % 4); ay = lambda z: z - ((z - py) % 4)
vm, n = VT.mosaic(world, 4, ax(X0), ay(Y0), ax(X1), ay(Y1)); van = cells(vm, ax(X0), ay(Y1) + 4, n // 4)
land = np.array([[0 <= X0 + c - mnx < cw and 0 <= Y1 - r - mny < ch and fl[Y1 - r - mny, X0 + c - mnx] != 0 for c in range(W)] for r in range(H)])
res = []
for dy in range(-2, 3):
    for dx in range(-2, 3):
        a = oc[4:-4, 4:-4]; bb = van[4 + dy:H - 4 + dy, 4 + dx:W - 4 + dx]; s = land[4:-4, 4:-4] & np.isfinite(a) & np.isfinite(bb)
        res.append((np.corrcoef(a[s], bb[s])[0, 1], dx, dy))
res.sort(reverse=True)
print('%s: best shift dx=%d dy=%d r=%.3f; at 0,0 r=%.3f; next %.3f' % (world, res[0][1], res[0][2], res[0][0],
      [r for r, x, y in res if x == 0 and y == 0][0], res[1][0]))
