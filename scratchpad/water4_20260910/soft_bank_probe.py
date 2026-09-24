# -*- coding: utf-8 -*-
"""soft_bank_probe.py -- does a conductance taper at the bank (shallow water)
remove the near-bank roughness the staircase mask leaves in the potential?"""
import sys, math
import numpy as np
sys.path.insert(0, 'scratchpad/water4_20260910')
from lodl_np import body_region, stroke_points
import disc_metric as dm
import flow_proto as fp

d, b, ids, flow, (px0, py0) = body_region('scratchpad/water3_20260910/work/charles_marked.lodl', 3)
wet = ids == 3; H, W = wet.shape
u = 4096.0 / d.bodyS
s = [q for q in stroke_points(d) if q['body'] == 3 and q['kind'] == 0][0]
pts = [((x - d.minX * 4096.0) / u - px0, (y - d.minY * 4096.0) / u - py0) for (x, y) in s['pts']]
halfW = s['width'] * 0.5 / u
yy, xx = np.mgrid[0:H, 0:W]
# chamfer distance to dry, texels
dist = np.where(wet, 1e9, 0.0)
for sweep in range(2):
    for y in (range(H) if sweep == 0 else range(H - 1, -1, -1)):
        for x in (range(W) if sweep == 0 else range(W - 1, -1, -1)):
            if not wet[y, x]:
                continue
            v = dist[y, x]
            for dx, dy, w in ((-1, 0, 1), (1, 0, 1), (0, -1, 1), (0, 1, 1), (-1, -1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (1, 1, 1.414)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H:
                    v = min(v, dist[ny, nx] + w)
            dist[y, x] = v
bump = np.ones((H, W))
for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
    vx, vy = bx - ax, by - ay; l2 = vx * vx + vy * vy
    t = np.clip(((xx + 0.5 - ax) * vx + (yy + 0.5 - ay) * vy) / max(l2, 1e-9), 0, 1)
    dd = np.hypot(xx + 0.5 - (ax + t * vx), yy + 0.5 - (ay + t * vy))
    bump = np.maximum(bump, 1.0 + 3.0 * np.clip(1.0 - (dd / halfW) ** 2, 0, 1) ** 2)
srcM = wet & (np.hypot(xx + 0.5 - pts[0][0], yy + 0.5 - pts[0][1]) <= halfW)
snkM = wet & (np.hypot(xx + 0.5 - pts[-1][0], yy + 0.5 - pts[-1][1]) <= halfW)
dry = ~wet; bank = np.zeros_like(wet)
bank[:, :-1] |= dry[:, 1:]; bank[:, 1:] |= dry[:, :-1]; bank[:-1, :] |= dry[1:, :]; bank[1:, :] |= dry[:-1, :]; bank &= wet


def run(taper, floor_k, freeBank, label):
    k = bump * np.maximum(floor_k, np.minimum(1.0, (dist / taper) ** 2)) if taper > 0 else bump
    g = fp.Grid(wet, k)
    rhs = np.zeros(g.n); rhs[g.idx[srcM]] += 1.0 / srcM.sum(); rhs[g.idx[snkM]] -= 1.0 / snkM.sum()
    phi, it, res = g.solve(rhs); ux, uy = g.velocity(phi)
    sp = np.hypot(ux, uy); mean = sp.mean()
    moving = sp >= 0.02 * mean
    if freeBank:
        moving &= ~bank[g.ys, g.xs]
    dx = np.where(moving, ux / np.maximum(sp, 1e-300), 0.0); dy = np.where(moving, uy / np.maximum(sp, 1e-300), 0.0)
    nb = [[] for _ in range(g.n)]
    for a, c in zip(g.fi, g.fj):
        nb[a].append(c); nb[c].append(a)
    freeIdx = np.nonzero(~moving)[0]
    for itf in range(4000):
        worst = 0.0
        for i in freeIdx:
            if not nb[i]:
                continue
            sx = sum(dx[j] for j in nb[i]) / len(nb[i]); sy = sum(dy[j] for j in nb[i]) / len(nb[i])
            nx, ny = dx[i] + 1.9 * (sx - dx[i]), dy[i] + 1.9 * (sy - dy[i])
            worst = max(worst, abs(nx - dx[i]), abs(ny - dy[i])); dx[i], dy[i] = nx, ny
        if worst < 1e-4:
            break
    m = np.hypot(dx, dy); dx = np.where(m > 1e-9, dx / np.maximum(m, 1e-300), 0); dy = np.where(m > 1e-9, dy / np.maximum(m, 1e-300), 0)
    ang = np.arctan2(g.field(dy), g.field(dx)); d2 = ((np.mod(ang, 2 * math.pi) / (2 * math.pi) * 256 + 0.5).astype(int) & 0xFF); d2[~wet] = 0
    hp = wet[:, :-1] & wet[:, 1:]; vp = wet[:-1, :] & wet[1:, :]
    dd = np.concatenate([dm.angdiff(d2[:, :-1], d2[:, 1:])[hp], dm.angdiff(d2[:-1, :], d2[1:, :])[vp]])
    a = d2[wet] / 256.0 * 2 * math.pi; R = math.hypot(np.cos(a).mean(), np.sin(a).mean())
    print('%-34s p90 %5.2f  p99 %5.2f  seam %.3f%%  R %.3f  %d it' % (label, np.percentile(dd, 90), np.percentile(dd, 99), 100 * float((dd > 10).sum()) / len(dd), R, it))
    return d2


if __name__ == '__main__':
    run(0, 1, True, 'hard bank, bank-free fill')
    run(3, 0.05, False, 'taper 3, floor .05, bank kept')
    run(3, 0.05, True, 'taper 3, floor .05, bank-free fill')
    run(4, 0.02, True, 'taper 4, floor .02, bank-free fill')
    run(6, 0.02, True, 'taper 6, floor .02, bank-free fill')
