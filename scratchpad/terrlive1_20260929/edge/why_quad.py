"""TERRLIVE1: inside the painted cells near the edge, split texels by QUADRANT: a quadrant with a real LTEX slot
vs an empty quadrant of a painted cell (drawn with the engine default, and left untouched by the fill because
the fill's 'painted' is per CELL). usage: python why_quad.py cx0 cy0 cx1 cy1"""
import sys, os
import numpy as np
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtread
cx0, cy0, cx1, cy1 = [int(v) for v in sys.argv[1:5]]
ST = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/stage/mod/FO4CSLOD/Commonwealth'
d = np.load('cells.npz'); cls, realq, minX, minY = d['cls'], d['realq'], int(d['minX']), int(d['minY'])
vt = vtread.Vt(f'{ST}/Commonwealth.VT.8.lodt')
m, wW, nN = vt.mosaic(cx0, cy0, cx1, cy1, 1)
upt = 8 * 4096.0 / vt.content
H, W = m.shape[:2]
rr, cc = np.mgrid[0:H, 0:W]
wx = wW * 4096.0 + (cc + 0.5) * upt; wy = nN * 4096.0 - (rr + 0.5) * upt
cxi = np.floor(wx / 4096).astype(int); cyi = np.floor(wy / 4096).astype(int)
qx = np.floor(wx / 2048).astype(int) - 2 * minX; qy = np.floor(wy / 2048).astype(int) - 2 * minY
pc = cls[cyi - minY, cxi - minX] == 2
pq = realq[qy, qx]
din = ndimage.distance_transform_edt(pc) * upt
L = 0.2126 * m[..., 0] + 0.7152 * m[..., 1] + 0.0722 * m[..., 2]
print(f'cells {cx0}..{cx1} x {cy0}..{cy1}: texels in painted cells {int(pc.sum())}, of them in an EMPTY quadrant {int((pc & ~pq).sum())}')
for a, b in ((0, 1024), (1024, 2048), (2048, 4096), (4096, 8192), (8192, 1e9)):
    s = pc & (din > a) & (din <= b)
    s1 = s & pq; s0 = s & ~pq
    print(f' inside {a:5.0f}..{b:>6.0f} u from the edge: painted quadrant n {int(s1.sum()):7d} lum {L[s1].mean() if s1.any() else float("nan"):5.1f} | '
          f'empty quadrant n {int(s0.sum()):6d} lum {L[s0].mean() if s0.any() else float("nan"):5.1f}')
