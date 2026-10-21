#!/usr/bin/env python3
"""Independent twin of the cluster light lists (src/gl/cellcluster.cpp, res/shaders/cell_cluster.glsl).

Builds the lists another way (cluster-major brute force over every cluster x light, with its own cone and
hemisphere tests) over a synthetic camera, depth buffer and random omni / spot / hemisphere lights, then for every
pixel checks:
  - no miss: every light whose range touches the pixel is in its cluster's list (or the cluster walks every light);
  - exact: the lighting summed over the list equals the flat loop's sum bit for bit (float32, light order).
Runs at 16, 64, 256 and 1024 lights.

    python3 tests/spells/cell_cluster_check.py           # green, then the red; PASS = green passes and red fails
    python3 tests/spells/cell_cluster_check.py --green   # green only
    python3 tests/spells/cell_cluster_check.py --red     # slices culled at their middle depth only: must FAIL
"""
import sys

import numpy as np

W, H = 960, 540
NEAR, FAR, FOVY = 10.0, 60000.0, 70.0
TILE, SLICES, CAP = 80, 24, 512
PAD_PX, PAD_DEPTH, PAD_RADIUS = 1.0, 1e-3, 1e-4      # the builder's slack (cellcluster.cpp)

SX = 1.0 / np.tan(np.radians(FOVY) / 2.0) * H / W    # projection x, y scale
SY = 1.0 / np.tan(np.radians(FOVY) / 2.0)


def scene():
    """Per pixel view-space point P (camera looking down -z) and unit normal N."""
    py, px = np.mgrid[0:H, 0:W].astype(np.float64)
    u, v = (px + 0.5) / W, (py + 0.5) / H
    nx, ny = 2.0 * u - 1.0, 2.0 * v - 1.0
    depth = 300.0 + 20000.0 * u ** 2 + 5000.0 * (1.0 - v)          # a back wall leaning away
    N = np.zeros((H, W, 3))
    N[..., 2] = 1.0
    floor = 30.0 / np.maximum(0.5 - v, 1e-3)                        # a floor under the horizon
    on = (v < 0.5) & (floor < depth)
    depth = np.where(on, floor, depth)
    N[on] = (0.0, 1.0, 0.0)
    pillar = ((px // 70) % 4 == 1)                                  # pillars in front
    pd = 120.0 + 3000.0 * v * (1.0 + (px // 70) % 3)
    on = pillar & (pd < depth)
    depth = np.where(on, pd, depth)
    N[on] = (-1.0, 0.0, 0.0)
    P = np.stack([nx * depth / SX, ny * depth / SY, -depth], -1)
    return P.astype(np.float32), N.astype(np.float32)


def lights(n, P, rng):
    """Random lights near random surface points: (pos, radius, shape 0 omni / 1 spot / 2 hemi, dir, cosOuter, color)."""
    out = []
    for i in range(n):
        p = P[rng.integers(H), rng.integers(W)] * np.float32(rng.uniform(0.4, 1.0))
        d = rng.normal(size=3)
        d /= np.linalg.norm(d)
        shape = int(rng.integers(3))
        out.append(dict(pos=p.astype(np.float32), r=np.float32(64.0 * 40.0 ** rng.uniform()), shape=shape,
                        dir=d.astype(np.float32),
                        cos=np.float32(np.cos(np.radians(rng.uniform(10, 80)))) if shape == 1 else np.float32(-2.0),
                        col=rng.uniform(0.2, 1.2, 3).astype(np.float32)))
    return out


def slice_bounds(red):
    k = np.arange(SLICES)
    a = NEAR * (FAR / NEAR) ** (k / SLICES)
    b = NEAR * (FAR / NEAR) ** ((k + 1) / SLICES)
    if red:
        mid = 0.5 * (a + b)
        return mid, mid
    lo, hi = a * (1 - PAD_DEPTH), b * (1 + PAD_DEPTH)
    lo[0], hi[-1] = 0.0, 1e30
    return lo, hi


def cluster_boxes(red):
    """AABB per cluster in view space, index = (slice * tilesY + ty) * tilesX + tx."""
    tx_n, ty_n = -(-W // TILE), -(-H // TILE)
    lo_d, hi_d = slice_bounds(red)
    boxes = []
    for s in range(SLICES):
        for ty in range(ty_n):
            for tx in range(tx_n):
                pts = []
                for xp in (tx * TILE - PAD_PX, (tx + 1) * TILE + PAD_PX):
                    for yp in (ty * TILE - PAD_PX, (ty + 1) * TILE + PAD_PX):
                        for d in (lo_d[s], hi_d[s]):
                            ndx, ndy = 2.0 * xp / W - 1.0, 2.0 * yp / H - 1.0
                            pts.append((ndx * d / SX, ndy * d / SY, -d))
                pts = np.array(pts)
                boxes.append((pts.min(0), pts.max(0)))
    lo = np.array([b[0] for b in boxes])
    hi = np.array([b[1] for b in boxes])
    return lo, hi, tx_n, ty_n


def membership(L, lo, hi):
    """bool [clusters, lights]: the light can reach some point of the cluster (conservative)."""
    m = np.zeros((lo.shape[0], len(L)), bool)
    centre, half = 0.5 * (lo + hi), 0.5 * (hi - lo)
    brad = np.linalg.norm(half, axis=1)
    for j, l in enumerate(L):
        p, r = l['pos'].astype(np.float64), float(l['r']) * (1 + PAD_RADIUS)
        q = np.clip(p, lo, hi)
        ok = ((q - p) ** 2).sum(1) <= r * r
        if l['shape'] == 2:
            corners = np.stack([np.where(np.array([(c >> k) & 1 for k in range(3)], bool), hi, lo) for c in range(8)], 1)
            ok &= ((corners - p) @ l['dir'].astype(np.float64)).max(1) >= -1e-4 * r
        elif l['shape'] == 1 and l['cos'] > 0:
            # angle from the axis to the box's bounding sphere, less the sphere's own angular radius
            v = centre - p
            dist = np.linalg.norm(v, axis=1)
            inside = dist <= brad * (1 + PAD_RADIUS) + 1e-4 * r
            cosang = (v @ l['dir'].astype(np.float64)) / np.maximum(dist, 1e-12)
            ang = np.arccos(np.clip(cosang, -1, 1))
            spread = np.arcsin(np.clip((brad * (1 + PAD_RADIUS) + 1e-4 * r) / np.maximum(dist, 1e-12), 0, 1))
            ok &= inside | (ang - spread <= np.arccos(float(l['cos'])) + 1e-9)
        m[:, j] = ok
    return m


def cluster_of(P, tx_n, ty_n):
    """The shader's rule (cell_cluster.glsl cellClusterOf), in float32."""
    py, px = np.mgrid[0:H, 0:W]
    fx, fy = px.astype(np.float32) + np.float32(0.5), py.astype(np.float32) + np.float32(0.5)
    tx = np.clip(np.floor(fx / np.float32(TILE)).astype(int), 0, tx_n - 1)
    ty = np.clip(np.floor(fy / np.float32(TILE)).astype(int), 0, ty_n - 1)
    log_near = np.float32(np.log(NEAR))
    scale = np.float32(SLICES / np.log(FAR / NEAR))
    depth = np.maximum(-P[..., 2], np.float32(1e-6))
    s = np.floor((np.log(depth).astype(np.float32) - log_near) * scale)
    s = np.clip(s.astype(int), 0, SLICES - 1)
    return (s * ty_n + ty) * tx_n + tx


def light_terms(l, P, N):
    """Per pixel: reaches (the range test the culling must respect) and the float32 contribution."""
    Lv = l['pos'] - P
    d = np.sqrt((Lv * Lv).sum(-1))
    reach = d < l['r']
    L = Lv / np.maximum(d, np.float32(0.001))[..., None]
    if l['shape'] == 2:
        reach &= (-(Lv @ l['dir'])) >= 0
    cone = np.ones_like(d)
    if l['shape'] == 1:
        c = -(L @ l['dir'])
        base = np.clip(np.float32(1) - (np.float32(1) - c) / max(np.float32(1) - l['cos'], np.float32(1e-4)), 0, 1)
        reach &= base > 0
        cone = np.minimum(base * base, np.float32(1))
    ndl = (N * L).sum(-1)
    a = np.maximum(np.float32(1) - d / l['r'], np.float32(0)) ** np.float32(2.2) * cone
    on = reach & (ndl > 0)
    e = np.where(on[..., None], l['col'] * (a * ndl)[..., None], np.float32(0)).astype(np.float32)
    return reach, e


def run(n, red, P, N):
    rng = np.random.default_rng(1000 + n)
    L = lights(n, P, rng)
    lo, hi, tx_n, ty_n = cluster_boxes(red)
    m = membership(L, lo, hi)
    counts = m.sum(1)
    over = counts > CAP
    m[over] = True                                   # past the cap the cluster walks every light
    cid = cluster_of(P, tx_n, ty_n)
    flat = np.zeros((H, W, 3), np.float32)
    clus = np.zeros((H, W, 3), np.float32)
    misses = 0
    for j, l in enumerate(L):
        reach, e = light_terms(l, P, N)
        listed = m[cid, j]
        misses += int((reach & ~listed).sum())
        flat += e
        clus += np.where(listed[..., None], e, np.float32(0))
    unequal = int((flat.view(np.uint32) != clus.view(np.uint32)).any(-1).sum())
    walked = np.where(over[cid], n, counts[cid]).mean()
    ok = misses == 0 and unequal == 0
    print('  %4d lights: missed %d, sums unequal %d px, mean list %.1f, clusters over cap %d  %s'
          % (n, misses, unequal, walked, int(over.sum()), 'ok' if ok else 'FAIL'))
    return ok


def gate(red):
    print('cell cluster twin, %dx%d, tiles %d px x %d slices: %s' % (W, H, TILE, SLICES, 'RED slice-middle' if red else 'green'))
    P, N = scene()
    ok = all([run(n, red, P, N) for n in (16, 64, 256, 1024)])
    print('PASS' if ok else 'FAIL')
    return ok


if __name__ == '__main__':
    arg = sys.argv[1] if len(sys.argv) > 1 else ''
    if arg == '--green':
        sys.exit(0 if gate(False) else 1)
    if arg == '--red':
        sys.exit(0 if gate(True) else 1)
    g, r = gate(False), gate(True)
    print('GATE %s (green %s, red %s)' % ('PASS' if g and not r else 'FAIL', 'PASS' if g else 'FAIL', 'FAIL' if not r else 'PASS'))
    sys.exit(0 if g and not r else 1)
