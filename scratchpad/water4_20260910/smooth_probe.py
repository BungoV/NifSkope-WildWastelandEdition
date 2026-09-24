# -*- coding: utf-8 -*-
"""smooth_probe.py -- how many in-mask 3x3 vector-averaging passes over the
solved DIRECTION (speed untouched) reach the pre-registered F5 numbers, on the
Charles (body 3, the harness's stroke out of the file) and, held out, on the
marsh (body 2, the refuter, with the same centreline stroke rule the harness
uses)."""
import sys, math
import numpy as np
sys.path.insert(0, 'scratchpad/water4_20260910')
from lodl_np import body_region, stroke_points
import disc_metric as dm
import flow_proto as fp

PATH = 'scratchpad/water3_20260910/work/charles_marked.lodl'


def centreline(wet):
    H, W = wet.shape
    alongY = H >= W
    pts = []
    for a in range(H if alongY else W):
        row = wet[a, :] if alongY else wet[:, a]
        xs = np.nonzero(row)[0]
        if len(xs) == 0:
            continue
        m = xs.mean() + 0.5
        p = (m, a + 0.5) if alongY else (a + 0.5, m)
        if pts and math.hypot(p[0] - pts[-1][0], p[1] - pts[-1][1]) < 16:
            continue
        pts.append(p)
    return pts


def solve_body(body, pts=None):
    d, b, ids, flow, (px0, py0) = body_region(PATH, body)
    wet = ids == body; H, W = wet.shape
    u = 4096.0 / d.bodyS
    if pts is None:
        s = [q for q in stroke_points(d) if q['body'] == body and q['kind'] == 0][0]
        pts = [((x - d.minX * 4096.0) / u - px0, (y - d.minY * 4096.0) / u - py0) for (x, y) in s['pts']]
    halfW = 16.0
    yy, xx = np.mgrid[0:H, 0:W]
    bump = np.ones((H, W))
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        vx, vy = bx - ax, by - ay; l2 = vx * vx + vy * vy
        t = np.clip(((xx + 0.5 - ax) * vx + (yy + 0.5 - ay) * vy) / max(l2, 1e-9), 0, 1)
        dd = np.hypot(xx + 0.5 - (ax + t * vx), yy + 0.5 - (ay + t * vy))
        bump = np.maximum(bump, 1.0 + 3.0 * np.clip(1.0 - (dd / halfW) ** 2, 0, 1) ** 2)
    srcM = wet & (np.hypot(xx + 0.5 - pts[0][0], yy + 0.5 - pts[0][1]) <= halfW)
    snkM = wet & (np.hypot(xx + 0.5 - pts[-1][0], yy + 0.5 - pts[-1][1]) <= halfW)
    g = fp.Grid(wet, bump)
    rhs = np.zeros(g.n); rhs[g.idx[srcM]] += 1.0 / srcM.sum(); rhs[g.idx[snkM]] -= 1.0 / snkM.sum()
    phi, it, res = g.solve(rhs); ux, uy = g.velocity(phi)
    return g, wet, ux, uy, it, res, pts


def direction_plane(g, wet, ux, uy, passes):
    H, W = wet.shape
    dry = ~wet; bank = np.zeros_like(wet)
    bank[:, :-1] |= dry[:, 1:]; bank[:, 1:] |= dry[:, :-1]; bank[:-1, :] |= dry[1:, :]; bank[1:, :] |= dry[:-1, :]; bank &= wet
    sp = np.hypot(ux, uy); mean = sp.mean()
    moving = (sp >= 0.02 * mean) & ~bank[g.ys, g.xs]
    cx = g.field(np.where(moving, ux / np.maximum(sp, 1e-300), 0.0)); cy = g.field(np.where(moving, uy / np.maximum(sp, 1e-300), 0.0))
    # seed every free texel from its nearest fixed one by a breadth-first walk
    from collections import deque
    fixedM = np.zeros_like(wet); fixedM[g.ys[moving], g.xs[moving]] = True
    seen = fixedM.copy(); dq = deque(zip(*np.nonzero(fixedM)))
    while dq:
        y, x = dq.popleft()
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < W and 0 <= ny < H and wet[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True; cx[ny, nx] = cx[y, x]; cy[ny, nx] = cy[y, x]; dq.append((ny, nx))
    wm = wet.astype(float)
    def sl(a, dx, dy):
        return np.roll(np.roll(a, dy, 0), dx, 1)
    # the passes: each free or bank cell, and every cell, takes the in-mask 3x3 vector mean
    for _ in range(passes):
        sx = np.zeros_like(cx); sy = np.zeros_like(cy); n = np.zeros_like(cx)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                w = 1.0 if (dx == 0 or dy == 0) else 0.5
                m = sl(wm, dx, dy)
                sx += w * sl(cx, dx, dy) * m; sy += w * sl(cy, dx, dy) * m; n += w * m
        cx = np.where(wet, sx / np.maximum(n, 1e-9), 0); cy = np.where(wet, sy / np.maximum(n, 1e-9), 0)
    ang = np.arctan2(cy, cx); d2 = ((np.mod(ang, 2 * math.pi) / (2 * math.pi) * 256 + 0.5).astype(int) & 0xFF); d2[~wet] = 0
    d2[wet & (np.hypot(cx, cy) == 0)] = -1      # a texel no direction reached, marked, never 0
    return d2


def structure(wet, d2):
    hp = wet[:, :-1] & wet[:, 1:]; vp = wet[:-1, :] & wet[1:, :]
    dd = np.concatenate([dm.angdiff(d2[:, :-1], d2[:, 1:])[hp], dm.angdiff(d2[:-1, :], d2[1:, :])[vp]])
    a = d2[wet] / 256.0 * 2 * math.pi
    return np.percentile(dd, 90), np.percentile(dd, 99), 100 * float((dd > 10).sum()) / len(dd), math.hypot(np.cos(a).mean(), np.sin(a).mean()), math.degrees(math.atan2(np.sin(a).mean(), np.cos(a).mean()))


if __name__ == '__main__':
    for body in (3, 2):
        d, b, ids, flow, _ = body_region(PATH, body)
        wet0 = ids == body
        pts = None if body == 3 else centreline(wet0)
        g, wet, ux, uy, it, res, pts = solve_body(body, pts)
        print('body %d: %d wet, %d it, res %.1e, stroke %d points' % (body, wet.sum(), it, res, len(pts)))
        for passes in (0, 2, 4, 6, 8, 12):
            d2 = direction_plane(g, wet, ux, uy, passes)
            p90, p99, seam, R, mean = structure(wet, d2)
            print('   passes %2d: p90 %5.2f  p99 %5.2f  seam %.3f%%  R %.3f  mean %.1f' % (passes, p90, p99, seam, R, mean))
