"""numpy port of OCTF1's f1sim.py -- must reproduce its 1.78 / 0.71 / 1.29."""
import math
import sys
import time
import numpy as np

n, tw, th = 8, 64, 64
fw = fh = 456.40
half = 256.2811
SS = 12


def axes(i, j, n):
    u = i / (n - 1) * 2.0 - 1.0
    v = j / (n - 1) * 2.0 - 1.0
    dx, dy = (u + v) * 0.5, (u - v) * 0.5
    dz = 1.0 - abs(dx) - abs(dy)
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    dx, dy, dz = dx / L, dy / L, dz / L
    elev = math.asin(max(-1.0, min(1.0, dz)))
    azim = math.atan2(dy, dx)
    r = (math.sin(azim), -math.cos(azim), 0.0)
    up = (math.sin(elev) * math.cos(azim), math.sin(elev) * math.sin(azim), math.cos(elev))
    return r, up


def hull(pts):
    pts = sorted(set(pts))
    def half_(ps):
        st = []
        for p in ps:
            while len(st) >= 2 and (st[-1][0] - st[-2][0]) * (p[1] - st[-2][1]) \
                    - (st[-1][1] - st[-2][1]) * (p[0] - st[-2][0]) <= 0:
                st.pop()
            st.append(p)
        return st
    lo = half_(pts)
    hi = half_(pts[::-1])
    return lo[:-1] + hi[:-1]


def run(cov_thr):
    worst = 0.0
    where = None
    tx = 2.0 * fw / tw
    ty = 2.0 * fh / th
    sub = (np.arange(SS) + 0.5) / SS
    gx = (-fw + (np.arange(tw)[:, None] + sub[None, :]) * tx).ravel()   # tw*SS
    gy = (-fh + (np.arange(th)[:, None] + sub[None, :]) * ty).ravel()   # th*SS
    X = gx[None, :]
    Y = gy[:, None]
    for j in range(n):
        for i in range(n):
            r, up = axes(i, j, n)
            hr = half * (abs(r[0]) + abs(r[1]) + abs(r[2]))
            hu = half * (abs(up[0]) + abs(up[1]) + abs(up[2]))
            px = 2.0 * hr * tw / (2.0 * fw)
            py = 2.0 * hu * th / (2.0 * fh)
            pts = []
            for sx in (-half, half):
                for sy in (-half, half):
                    for sz in (-half, half):
                        pts.append((sx * r[0] + sy * r[1] + sz * r[2],
                                    sx * up[0] + sy * up[1] + sz * up[2]))
            P = hull(pts)
            inside = np.ones((th * SS, tw * SS), bool)
            for k in range(len(P)):
                ax, ay = P[k]
                bx, by = P[(k + 1) % len(P)]
                inside &= ((bx - ax) * (Y - ay) - (by - ay) * (X - ax)) >= 0
            cov = inside.reshape(th, SS, tw, SS).mean(axis=(1, 3))
            mk = cov >= cov_thr
            cols = np.flatnonzero(mk.any(axis=0))
            rows = np.flatnonzero(mk.any(axis=1))
            mx = int(cols[-1] - cols[0] + 1) if cols.size else 0
            my = int(rows[-1] - rows[0] + 1) if rows.size else 0
            d = max(abs(mx - px), abs(my - py))
            if d > worst:
                worst = d
                where = (i, j, px, py, mx, my)
    return worst, where


t0 = time.time()
for thr, name in ((16 / 255.0, 'reader 128 == coverage floor 16/255 = 6.27%'),
                  (0.5, 'a true HALF-COVERAGE test'),
                  (0.20, 'the pending 0.20 ruling')):
    w, k = run(thr)
    print('%-46s worst %.2f  at frame %d,%d  pred %.2f,%.2f  meas %d,%d'
          % (name, w, k[0], k[1], k[2], k[3], k[4], k[5]))
print('%.1f s' % (time.time() - t0))
