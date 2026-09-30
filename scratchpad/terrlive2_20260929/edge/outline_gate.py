"""TERRLIVE2 outline gate: does the blend band follow the quadrant staircase?
Reads a WW_BLEND_DUMP (the preview's weight map + its painted quadrants) and measures, over the band
(0 < w < 1), the share of the weight's gradient that runs along a grid axis (within 5 deg).
  dumped   = the binary's own weight map (the head under test)
  law2     = TERRLIVE1's law rebuilt exactly from the same quadrants (distance to the nearest unpainted
             square, smoothstep over 8192) -- the staircase the gate must catch
  coast    = the same share for the painted mask blurred by 4096 u: how axis-aligned the coast truly is
PASS when dumped <= coast + 0.05 AND no unpainted texel carries weight. law2 must FAIL (the gate's own proof).
usage: python outline_gate.py <dump>"""
import sys
import numpy as np
from scipy import ndimage

BAND = 8192.0
raw = open(sys.argv[1], 'rb').read()
assert raw[:4] == b'BLDW'
ver, wn, hn = np.frombuffer(raw, '<i4', 3, 4)
res, x0, yN = np.frombuffer(raw, '<f4', 3, 16)
qx0, qy0, qw, qh = np.frombuffer(raw, '<i4', 4, 28)
o = 44
w = np.frombuffer(raw, '<f4', wn * hn, o).reshape(hn, wn); o += wn * hn * 4
pq = np.frombuffer(raw, 'u1', qw * qh, o).reshape(qh, qw)
ii, jj = np.meshgrid(np.arange(wn), np.arange(hn))
wx = x0 + (ii + 0.5) * res
wy = yN - (jj + 0.5) * res
qx = np.floor(wx / 2048.0).astype(int) - qx0
qy = np.floor(wy / 2048.0).astype(int) - qy0
painted = pq[qy, qx] != 0

# law 2 rebuilt: the painted quadrants rasterised at res over the whole quadrant window, then the
# distance from each texel centre to the unpainted squares (EDT to unpainted texels less half a texel)
k = int(round(2048.0 / res))
fine = np.kron(pq, np.ones((k, k), np.uint8))[::-1]          # row 0 = north
dist = ndimage.distance_transform_edt(fine != 0) * res - 0.5 * res
fx0, fyN = qx0 * 2048.0, (qy0 + qh) * 2048.0
fi = np.floor((wx - fx0) / res).astype(int)
fj = np.floor((fyN - wy) / res).astype(int)
t = np.clip(dist[fj, fi] / BAND, 0, 1)
law2 = np.where(painted, t * t * (3 - 2 * t), 0.0)

coast = ndimage.gaussian_filter(fine.astype(np.float32), 4096.0 / res)[fj, fi]


def axis_share(m, band):
    gy, gx = np.gradient(m)
    ax, ay = np.abs(gx), np.abs(gy)
    g = np.hypot(gx, gy)
    axis = np.minimum(ax, ay) <= np.tan(np.radians(5.0)) * np.maximum(ax, ay)
    sel = band & (g > 1e-6)
    return float((g * axis)[sel].sum() / g[sel].sum()), int(sel.sum())


bandD = painted & (w > 1e-3) & (w < 0.999)
bandL = painted & (law2 > 1e-3) & (law2 < 0.999)
bandC = (coast > 0.02) & (coast < 0.98)
aD, nD = axis_share(w, bandD)
aL, nL = axis_share(law2, bandL)
aC, nC = axis_share(coast, bandC)
leak = float(w[~painted].max()) if (~painted).any() else 0.0
bar = aC + 0.05
print(f'grid {wn}x{hn} @ {res:.0f} u, quadrants {qw}x{qh} painted {int(pq.sum())}')
print(f'axis share: coast {aC:.3f} ({nC} px)  bar {bar:.3f}')
print(f'  law2 (staircase)   {aL:.3f} ({nL} px)  {"PASS" if aL <= bar else "FAIL"}')
print(f'  dumped (this head) {aD:.3f} ({nD} px)  {"PASS" if aD <= bar else "FAIL"}  unpainted max w {leak:.4f}')
print('GATE', 'GREEN' if (aD <= bar and leak == 0.0 and aL > bar) else 'RED')
