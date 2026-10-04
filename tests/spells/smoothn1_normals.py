"""smoothn1_normals -- lane SMOOTHN1's gate: does the bake's hit normal follow the surface's smooth normal?

  python smoothn1_normals.py vnml   <land.dump>
  python smoothn1_normals.py cube   <land.dump> <cube.dump>              (the cube way's G-buffer, one probe)
  python smoothn1_normals.py tbk    <land.dump> <bake dir>               (stored surfel normals on the ground)
  python smoothn1_normals.py nmap   <land.dump> <cube.dump> <cube nmap-off.dump>

land.dump = WW_CELL_BAKE_LAND_DUMP ('LND1', then per LAND: x, y int32, has-VNML u8, heights f32 33x33 (row =
south->north, column = west->east), VNML 33x33x3 bytes). cube.dump = WW_CELL_BAKE_CUBE_DUMP (+ its .txt).

vnml: the VNML decode (signed bytes, x east, y north, z up) against the heights' own central differences; the
  other plausible decodes are printed beside it so the chosen one is measured, not assumed.
cube: every cube pixel that lands on the ground (within 1 unit of the LAND triangle under it) -- its normal
  against the VNML blended over that triangle (the soup's split: a,b,c / a,c,d). PASS: median <= 0.1 deg and
  p95 <= 1.0 deg (pre-registered). Face normals (the red, WW_CELL_BAKE_SMOOTH=0) must FAIL.
tbk: every stored surfel within 4 units of the ground against VNML at its position (a 70-unit mean of smooth
  normals, so curvature keeps it above zero): median and p95 printed; compare runs.
nmap: non-ground pixels, normal-map run vs the same probe without normal maps (WW_CELL_BAKE_NMAP=0):
  per-pixel angle between them, and each run's spread (angle to its 70-unit cell's mean). PASS: median per-pixel
  angle >= 1 deg over the normal-mapped share and spread ratio >= 1.5 (pre-registered); the red (off vs off) FAILS.
"""
import math
import struct
import sys

import numpy as np

STEP = 4096.0 / 32.0


def read_land(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'LND1', 'not a land dump'
    o, out = 4, {}
    rec = 8 + 1 + 33 * 33 * 4 + 33 * 33 * 3
    while o + rec <= len(b):
        x, y = struct.unpack_from('<ii', b, o)
        hn = b[o + 8]
        h = np.frombuffer(b, '<f4', 33 * 33, o + 9).reshape(33, 33).astype(np.float64)
        raw = np.frombuffer(b, 'u1', 33 * 33 * 3, o + 9 + 33 * 33 * 4).reshape(33, 33, 3)
        out[(x, y)] = (hn, h, raw)
        o += rec
    return out


def decode(raw, how='signed'):
    r = raw.astype(np.float64)
    if how == 'signed':
        v = raw.view(np.int8).astype(np.float64)
    elif how == 'offset128':
        v = r - 128.0
    elif how == 'unit':
        v = r / 127.5 - 1.0
    else:
        raise ValueError(how)
    ln = np.linalg.norm(v, axis=-1, keepdims=True)
    up = np.zeros_like(v)
    up[..., 2] = 1
    return np.where(ln > 0, v / np.where(ln > 0, ln, 1), up)


def derived(h):
    gx = np.zeros_like(h)
    gy = np.zeros_like(h)
    gx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) / (2 * STEP)
    gx[:, 0] = (h[:, 1] - h[:, 0]) / STEP
    gx[:, -1] = (h[:, -1] - h[:, -2]) / STEP
    gy[1:-1] = (h[2:] - h[:-2]) / (2 * STEP)
    gy[0] = (h[1] - h[0]) / STEP
    gy[-1] = (h[-1] - h[-2]) / STEP
    n = np.stack([-gx, -gy, np.ones_like(h)], -1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def ang(a, b):
    a = a / np.linalg.norm(a, axis=-1, keepdims=True)
    b = b / np.linalg.norm(b, axis=-1, keepdims=True)
    return np.degrees(np.arccos(np.clip(np.sum(a * b, -1), -1, 1)))


def stats(a):
    if not len(a):
        return 'n 0'
    return 'n %d median %.3f p95 %.3f mean %.3f deg' % (len(a), np.median(a), np.percentile(a, 95), a.mean())


def ground_at(land, P):
    """Per point: (on-ground flag, |dz|, the VNML blend over the soup's triangle there, its face normal)."""
    n = len(P)
    ok = np.zeros(n, bool)
    dz = np.full(n, np.inf)
    sm = np.zeros((n, 3))
    fn = np.zeros((n, 3))
    cx = np.floor(P[:, 0] / 4096.0).astype(int)
    cy = np.floor(P[:, 1] / 4096.0).astype(int)
    for key in set(zip(cx.tolist(), cy.tolist())):
        if key not in land:
            continue
        hn, h, raw = land[key]
        nv = decode(raw) if hn else derived(h)
        m = (cx == key[0]) & (cy == key[1])
        idx = np.nonzero(m)[0]
        fx = (P[idx, 0] - key[0] * 4096.0) / STEP
        fy = (P[idx, 1] - key[1] * 4096.0) / STEP
        c = np.clip(np.floor(fx).astype(int), 0, 31)
        r = np.clip(np.floor(fy).astype(int), 0, 31)
        u, v = fx - c, fy - r
        # the soup's split: a(r,c) b(r,c+1) cc(r+1,c+1) | a cc d(r+1,c); triangle 1 where u >= v
        t1 = u >= v
        A = np.stack([c * STEP, r * STEP, h[r, c]], -1)
        B = np.stack([(c + 1) * STEP, r * STEP, h[r, c + 1]], -1)
        C = np.stack([(c + 1) * STEP, (r + 1) * STEP, h[r + 1, c + 1]], -1)
        D = np.stack([c * STEP, (r + 1) * STEP, h[r + 1, c]], -1)
        # barycentric weights in the plane (x, y)
        wa = np.where(t1, 1 - u, 1 - v)
        wb = np.where(t1, u - v, 0.0)
        wc = np.where(t1, v, u)
        wd = np.where(t1, 0.0, v - u)
        z = wa * A[:, 2] + wb * B[:, 2] + wc * C[:, 2] + wd * D[:, 2]
        n_ = (wa[:, None] * nv[r, c] + wb[:, None] * nv[r, c + 1] + wc[:, None] * nv[r + 1, c + 1]
              + wd[:, None] * nv[r + 1, c])
        e1 = np.where(t1[:, None], B - A, C - A)
        e2 = np.where(t1[:, None], C - A, D - A)
        f = np.cross(e1, e2)
        dz[idx] = np.abs(P[idx, 2] - z)
        ok[idx] = True
        sm[idx] = n_ / np.linalg.norm(n_, axis=-1, keepdims=True)
        fn[idx] = f / np.linalg.norm(f, axis=-1, keepdims=True)
    return ok, dz, sm, fn


def read_cube(path):
    line = open(path + '.txt').read().split('\n')[0].split('\t')
    ppos, S = np.array([float(v) for v in line[1:4]]), int(line[4])
    N = 6 * S * S
    raw = open(path, 'rb').read()
    dist = np.frombuffer(raw, '<f4', N, N * 3).astype(np.float64)
    nrm = np.frombuffer(raw, '<f4', N * 3, N * 11).reshape(-1, 3).astype(np.float64)
    y, x = np.mgrid[0:S, 0:S]
    u, v = 2 * (x + 0.5) / S - 1, 1 - 2 * (y + 0.5) / S
    o = []
    for face in range(6):
        if face < 4:
            f = [(1, 0, 0), (0, -1, 0), (-1, 0, 0), (0, 1, 0)][face]
            r, up = (f[1], -f[0], 0), (0, 0, 1)
        elif face == 4:
            f, r, up = (0, 0, 1), (0, -1, 0), (-1, 0, 0)
        else:
            f, r, up = (0, 0, -1), (0, -1, 0), (1, 0, 0)
        q = np.array(f)[None, None] + u[..., None] * np.array(r)[None, None] + v[..., None] * np.array(up)[None, None]
        o.append(q / np.linalg.norm(q, axis=-1, keepdims=True))
    Dirs = np.concatenate([a.reshape(-1, 3) for a in o])
    seen = dist >= 0
    P = ppos[None] + Dirs * dist[:, None]
    return ppos, Dirs, dist, nrm, seen, P


def toward(n, d):
    s = np.where(np.sum(n * d, -1) > 0, -1.0, 1.0)
    return n * s[:, None]


def main(a):
    mode = a[0]
    land = read_land(a[1])
    if mode == 'vnml':
        rows = {k: [] for k in ('signed', 'offset128', 'unit')}
        nv = 0
        for (hn, h, raw) in land.values():
            if not hn:
                continue
            nv += 1
            dn = derived(h)[1:-1, 1:-1].reshape(-1, 3)
            for k in rows:
                rows[k].append(ang(decode(raw, k)[1:-1, 1:-1].reshape(-1, 3), dn))
        print('vnml: %d of %d LAND records carry VNML' % (nv, len(land)))
        for k, v in rows.items():
            print('  decode %-9s vs the heights\' central differences: %s' % (k, stats(np.concatenate(v)) if v else 'n 0'))
        return 0
    if mode == 'cube':
        ppos, Dirs, dist, nrm, seen, P = read_cube(a[2])
        ok, dz, sm, fn = ground_at(land, P)
        g = seen & ok & (dz < 1.0)
        exp = toward(sm[g], Dirs[g])
        e = ang(nrm[g], exp)
        ef = ang(toward(fn[g], Dirs[g]), exp)
        print('cube %s: probe %s, ground pixels %d of %d seen' % (a[2], ppos.tolist(), g.sum(), seen.sum()))
        print('  hit normal vs VNML blend: %s' % stats(e))
        print('  (the face normal vs VNML blend, the flat floor: %s)' % stats(ef))
        verdict = len(e) > 1000 and np.median(e) <= 0.1 and np.percentile(e, 95) <= 1.0
        print('  %s (bars: median <= 0.1, p95 <= 1.0 deg, over > 1000 ground pixels)' % ('PASS' if verdict else 'FAIL'))
        return 0 if verdict else 1
    if mode == 'tbk':
        import glob
        import os
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from probe_bake import read_tbk
        pos, nr = [], []
        for f in sorted(glob.glob(os.path.join(a[2], '*.tbk'))):
            t = read_tbk(f)
            for s in (t['surfels'], t['back']):
                pos.append(s['pos'].astype(np.float64))
                nr.append(s['nrm'].astype(np.float64) / 32767.0)
        P, N = np.concatenate(pos), np.concatenate(nr)
        ok, dz, sm, fn = ground_at(land, P)
        g = ok & (dz < 4.0) & (N[:, 2] > 0)
        print('tbk %s: ground surfels %d of %d' % (a[2], g.sum(), len(P)))
        print('  stored normal vs VNML at the surfel: %s' % stats(ang(N[g], sm[g])))
        print('  (face normal there vs VNML: %s)' % stats(ang(fn[g], sm[g])))
        return 0
    if mode == 'nmap':
        ppos, Dirs, dist, nrm, seen, P = read_cube(a[2])
        _, _, _, _, seen2, _ = read_cube(a[3])
        nrm2 = read_cube(a[3])[3]
        ok, dz, sm, fn = ground_at(land, P)
        m = seen & seen2 & ~(ok & (dz < 1.0))
        d = ang(nrm[m], nrm2[m])
        bent = d > 0.01

        def spread(nn):
            key = np.floor(P[m] / 70.0).astype(np.int64)
            k = key[:, 0] * 73856093 ^ key[:, 1] * 19349663 ^ key[:, 2] * 83492791
            _, inv = np.unique(k, return_inverse=True)
            s = np.zeros((inv.max() + 1, 3))
            np.add.at(s, inv, nn[m])
            return ang(nn[m], s[inv])
        s1, s2 = spread(nrm), spread(nrm2)
        print('nmap %s vs %s: non-ground pixels %d, bent by a normal map %d (%.1f%%)'
              % (a[2], a[3], m.sum(), bent.sum(), 100.0 * bent.mean() if m.any() else 0))
        print('  per-pixel angle on vs off, over the bent: %s' % stats(d[bent]))
        print('  spread (angle to the 70-unit cell mean): on %s; off %s' % (stats(s1), stats(s2)))
        ratio = np.median(s1) / max(np.median(s2), 1e-9)
        verdict = bent.sum() > 1000 and np.median(d[bent]) >= 1.0 and ratio >= 1.5
        print('  spread ratio %.2f; %s (bars: > 1000 bent pixels, median angle >= 1 deg, spread ratio >= 1.5)'
              % (ratio, 'PASS' if verdict else 'FAIL'))
        return 0 if verdict else 1
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
