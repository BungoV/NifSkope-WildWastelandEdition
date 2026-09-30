"""Prototype outline variants against outline_gate's measure, from the dump's own quadrants."""
import sys
import numpy as np
from scipy import ndimage
raw = open(sys.argv[1], 'rb').read()
wn, hn = np.frombuffer(raw, '<i4', 2, 8)
res, x0, yN = np.frombuffer(raw, '<f4', 3, 16)
qx0, qy0, qw, qh = np.frombuffer(raw, '<i4', 4, 28)
o = 44
wd = np.frombuffer(raw, '<f4', wn * hn, o).reshape(hn, wn); o += wn * hn * 4
pq = np.frombuffer(raw, 'u1', qw * qh, o).reshape(qh, qw)
BAND = 8192.0
ii, jj = np.meshgrid(np.arange(wn), np.arange(hn))
wx = x0 + (ii + 0.5) * res; wy = yN - (jj + 0.5) * res
qx = np.floor(wx / 2048).astype(int) - qx0; qy = np.floor(wy / 2048).astype(int) - qy0
painted = pq[qy, qx] != 0
k = int(round(2048 / res))
fine = np.kron(pq, np.ones((k, k), np.uint8))[::-1]
fi = np.floor((wx - qx0 * 2048.0) / res).astype(int); fj = np.floor(((qy0 + qh) * 2048.0 - wy) / res).astype(int)
coast = ndimage.gaussian_filter(fine.astype(np.float32), 4096.0 / res)[fj, fi]

def share(m, band):
    gy, gx = np.gradient(m); ax, ay = np.abs(gx), np.abs(gy); g = np.hypot(gx, gy)
    axis = np.minimum(ax, ay) <= np.tan(np.radians(5)) * np.maximum(ax, ay)
    sel = band & (g > 1e-6)
    return (g * axis)[sel].sum() / g[sel].sum()

def ss(t):
    t = np.clip(t, 0, 1); return t * t * (3 - 2 * t)

aC = share(coast, (coast > .02) & (coast < .98)); print('coast', round(aC, 3), 'bar', round(aC + .05, 3))
aD = share(wd, painted & (wd > 1e-3) & (wd < .999)); print('dumped', round(aD, 3))
# signed distance on the fine (res) grid, exact to the staircase
din = ndimage.distance_transform_edt(fine != 0) * res - .5 * res
dout = ndimage.distance_transform_edt(fine == 0) * res - .5 * res
sd = np.where(fine != 0, din, -dout)
def report(name, w):
    w = np.where(painted, w, 0.0)
    b = painted & (w > 1e-3) & (w < .999)
    # depth of the band: mean distance of band texels from the staircase
    d = sd[fj, fi]
    print(f'{name:34s} share {share(w, b):.3f}  band depth {np.median(d[b]):6.0f} u  edge w max {w[painted & (d < 600)].max():.3f}')
for sg in (2048, 3072, 4096, 6144):
    f = ndimage.gaussian_filter(np.clip(sd, -30000, 30000), sg / res)
    off = f[fine == 0].max()   # measured, not the bound
    offb = sg * 1.2533141 + 2 * 362.03867
    report(f'blur dist s{sg} off=bound {offb:.0f}', ss((f[fj, fi] - offb) / BAND))
    report(f'blur dist s{sg} off=meas {off:.0f}', ss((f[fj, fi] - off) / BAND))
for sg in (2048, 4096):
    m = ndimage.gaussian_filter(fine.astype(np.float32), sg / res)
    mo = m[fine == 0].max()
    report(f'blur mask s{sg} lo=meas {mo:.2f}', ss((m[fj, fi] - mo) / (1 - mo + 1e-6)))
