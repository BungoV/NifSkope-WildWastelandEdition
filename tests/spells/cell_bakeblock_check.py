#!/usr/bin/env python3
"""cell_bakeblock_check -- lane BAKEBLOCK1 (2026-10-03): an exterior bake sees the 5x5 block + the far LOD.

  cell_bakeblock_check.py <work dir> <cx> <cy> <ref exe> <run> [<before run>] [<neighbor run> <ncx> <ncy>]

Independent of the exe's own census: reads the soups (`WW_CELL_PROBE_SOUP`), the probe tables and the
`.tbk` files the runs wrote, traces with its own Python Moller-Trumbore, and with tests/prtp_reference.cpp
(brute force, every triangle, its own directions). Bars set before the green run (STATUS.md):
  BLOCK    every one of the 25 cells around C holds >= 1024 small triangles (max edge <= 200, by centroid)
  FAR      every 45-degree sector around C holds >= 100 triangles 3..32 cells out
  OCTANTS  the Concord probe's .tbk sky: the 8 octants' sum drops >= 0.05 against the run before the lane,
           no octant rises > 0.01; the reference at the same point agrees with the .tbk within 0.015 an octant
  HORIZON  the high open probes' sky in the band -1..+2 degrees: full soup / the 5x5's part alone <= 0.75
  EDGE     probes within 150 of C's shared edge with the neighbor N: the reference on C's soup vs N's soup,
           same point, same directions: worst octant |diff| <= 0.015 for 95 %, none > 0.05
A red run (WW_CELL_BAKEBLOCK_RED=n1 / nolod) must FAIL: n1 fails BLOCK/FAR/OCTANTS/HORIZON/EDGE, nolod FAR
and HORIZON. One verdict line; exit 0 on PASS.
"""
import glob
import math
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import read_tbk  # noqa: E402

CELL = 4096.0
CONCORD = np.array([-60200.0, 73360.0, 6206.0])


def read_soup(path):
    raw = open(path, 'rb').read()
    magic, n, nd = np.frombuffer(raw, '<u4', 3)
    assert magic == 0x31505350, path
    return np.frombuffer(raw, '<f4', int(n) * 9, 12).reshape(-1, 3, 3).astype(np.float64)


def read_probes(path):
    rows = []
    for ln in open(path):
        if ln.startswith('#') or ln.startswith('id'):
            continue
        f = ln.split('\t')
        rows.append((float(f[3]), float(f[4]), float(f[5])))
    return np.array(rows)


def tbk_probes(run):
    out = []
    for f in sorted(glob.glob(run + '/bake/*.tbk')):
        for pr in read_tbk(f)['probes']:
            out.append((np.array(pr['pos'], float), np.array(pr['sky'], float) / 8.0))
    return out


def reference(exe, soup, pts, work, tag, rays):
    pf, of = os.path.join(work, tag + '.pts'), os.path.join(work, tag + '.tsv')
    with open(pf, 'w') as f:
        for p in pts:
            f.write('%.3f %.3f %.3f\n' % tuple(p))
    subprocess.run([exe, soup, pf, str(rays), '0.3', '-0.5', '0.81', of], check=True, capture_output=True)
    rows = [ln.split('\t') for ln in open(of) if not ln.startswith('#')]
    return np.array([[float(v) for v in r[1:41:5]] for r in rows])


def cell_of(xy):
    return np.floor(xy / CELL).astype(int)


def check_block(T, cx, cy):
    edge = np.max(np.linalg.norm(T - np.roll(T, 1, axis=1), axis=2), axis=1)
    c = cell_of(T.mean(1)[:, :2])
    small = edge <= 200
    worst = None
    for dx in range(-2, 3):
        for dy in range(-2, 3):
            k = int(np.count_nonzero(small & (c[:, 0] == cx + dx) & (c[:, 1] == cy + dy)))
            if worst is None or k < worst[0]:
                worst = (k, dx, dy)
    return worst[0] >= 1024, 'BLOCK fewest small tris %d (cell %+d,%+d)' % worst


def check_far(T, cx, cy):
    m = T.mean(1)[:, :2]
    ctr = np.array([(cx + 0.5) * CELL, (cy + 0.5) * CELL])
    d = m - ctr
    r = np.max(np.abs(d), 1) / CELL
    sel = (r >= 3) & (r <= 32.5)
    sec = (np.floor((np.degrees(np.arctan2(d[sel, 1], d[sel, 0])) % 360) / 45)).astype(int)
    cnt = np.bincount(sec, minlength=8)
    return int(cnt.min()) >= 100, 'FAR min sector %d (sectors %s)' % (cnt.min(), '/'.join(map(str, cnt)))


def check_octants(run, before, exe, work):
    P = tbk_probes(run)
    B = tbk_probes(before)
    pa = min(P, key=lambda q: np.linalg.norm(q[0] - CONCORD))
    pb = min(B, key=lambda q: np.linalg.norm(q[0] - CONCORD))
    ref = reference(exe, os.path.join(run, 'soup.psp'), [pa[0]], work, 'octref', 2048)[0]
    drop = pb[1].sum() - pa[1].sum()
    rise = float(np.max(pa[1] - pb[1]))
    agree = float(np.max(np.abs(ref - pa[1])))
    ok = drop >= 0.05 and rise <= 0.01 and agree <= 0.015
    txt = ('OCTANTS probe %s sky before %s (sum %.3f) after %s (sum %.3f) drop %.3f, max rise %.3f, ref vs tbk %.3f'
           % (np.round(pa[0]).astype(int).tolist(), ' '.join('%.3f' % v for v in pb[1]), pb[1].sum(),
              ' '.join('%.3f' % v for v in pa[1]), pa[1].sum(), drop, rise, agree))
    return ok, txt


def band_sky(T, p, az_n=360, els=(-1.0, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0)):
    """Share of band rays (azimuth x elevation grid) that miss every triangle of T from p."""
    V = T - p
    R = np.linalg.norm(V, axis=2)
    el = np.degrees(np.arcsin(np.clip(V[..., 2] / np.maximum(R, 1e-9), -1, 1)))
    az = np.degrees(np.arctan2(V[..., 1], V[..., 0])) % 360
    span = np.degrees(np.max(np.linalg.norm(V[:, [0, 1, 2]] / R[..., None]
                                             - V[:, [1, 2, 0]] / R[:, [1, 2, 0], None], axis=2), axis=1))
    pad = span + 0.5
    keep = (el.min(1) - pad <= els[-1]) & (el.max(1) + pad >= els[0])
    V, az, pad = V[keep], az[keep], pad[keep]
    a0 = az.min(1); a1 = az.max(1)
    wrap = (a1 - a0) > 180
    # a wrapped triangle covers [a1 .. 360) u [0 .. a0] in the other order: store as rotated interval
    az2 = np.where(az > 180, az - 360, az)
    b0 = np.where(wrap, az2.min(1), a0) - pad
    b1 = np.where(wrap, az2.max(1), a1) + pad
    allaz = ((b1 - b0) >= 360 - 1e-6) | (wrap & ((az2.max(1) - az2.min(1)) > 180))
    E = V[:, 1] - V[:, 0]; F = V[:, 2] - V[:, 0]
    sky = hit = 0
    for i in range(az_n):
        a = (i + 0.5) * 360.0 / az_n
        inb = allaz | ((b0 <= a) & (b1 >= a)) | ((b0 <= a - 360) & (b1 >= a - 360)) | ((b0 <= a + 360) & (b1 >= a + 360))
        if not inb.any():
            sky += len(els)
            continue
        v0, e, f = V[inb, 0], E[inb], F[inb]
        for elv in els:
            d = np.array([math.cos(math.radians(elv)) * math.cos(math.radians(a)),
                          math.cos(math.radians(elv)) * math.sin(math.radians(a)), math.sin(math.radians(elv))])
            pv = np.cross(d, f)
            det = (e * pv).sum(1)
            ok = np.abs(det) > 1e-12
            inv = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
            tv = -v0
            u = (tv * pv).sum(1) * inv
            qv = np.cross(tv, e)
            v = (qv @ d) * inv
            t = (f * qv).sum(1) * inv
            h = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-3)
            if h.any():
                hit += 1
            else:
                sky += 1
    return sky / float(sky + hit)


def check_horizon(T, run, cx, cy):
    P = read_probes(os.path.join(run, 'probes.tsv'))
    lo = np.array([(cx - 2) * CELL, (cy - 2) * CELL]); hi = lo + 5 * CELL
    m = T.mean(1)[:, :2]
    near = T[np.all((m >= lo) & (m < hi), 1)]
    # the highest probes first, kept while the 5x5 alone leaves them >= 0.25 band sky
    order = np.argsort(-P[:, 2])
    full_s = near_s = 0.0
    used = []
    for i in order[:48]:
        sn = band_sky(near, P[i])
        if sn < 0.25:
            continue
        sf = band_sky(T, P[i])
        full_s += sf; near_s += sn
        used.append('%.0f:%.2f/%.2f' % (P[i][2], sf, sn))
        if len(used) == 8:
            break
    if not used:
        return False, 'HORIZON no probe with open band sky in the 5x5 alone'
    ratio = full_s / near_s
    return ratio <= 0.75, 'HORIZON band -1..+2 deg sky, full / 5x5-only %.3f over %d probes (z:full/near %s)' % (
        ratio, len(used), ' '.join(used))


def check_edge(run, nrun, cx, cy, ncx, ncy, exe, work):
    P = read_probes(os.path.join(run, 'probes.tsv'))
    if ncy == cy + 1:
        y = (cy + 1) * CELL; sel = np.abs(P[:, 1] - y) <= 150
    elif ncy == cy - 1:
        y = cy * CELL; sel = np.abs(P[:, 1] - y) <= 150
    elif ncx == cx + 1:
        x = (cx + 1) * CELL; sel = np.abs(P[:, 0] - x) <= 150
    else:
        x = cx * CELL; sel = np.abs(P[:, 0] - x) <= 150
    pts = P[sel][:24]
    if not len(pts):
        return False, 'EDGE no probe within 150 of the shared edge'
    a = reference(exe, os.path.join(run, 'soup.psp'), pts, work, 'edgeC', 2048)
    b = reference(exe, os.path.join(nrun, 'soup.psp'), pts, work, 'edgeN', 2048)
    w = np.max(np.abs(a - b), 1)
    p95 = float(np.percentile(w, 95))
    ok = p95 <= 0.015 and float(w.max()) <= 0.05
    return ok, 'EDGE %d probes, worst-octant |C - N| median %.4f p95 %.4f max %.4f' % (
        len(w), np.median(w), p95, w.max())


def main():
    a = sys.argv[1:]
    work, cx, cy, exe, run = a[0], int(a[1]), int(a[2]), os.path.abspath(a[3]), a[4]
    before = a[5] if len(a) > 5 and a[5] != '-' else None
    nrun = a[6] if len(a) > 8 else None
    os.makedirs(work, exist_ok=True)
    T = read_soup(os.path.join(run, 'soup.psp'))
    res = [check_block(T, cx, cy), check_far(T, cx, cy)]
    if before:
        res.append(check_octants(run, before, exe, work))
    res.append(check_horizon(T, run, cx, cy))
    if nrun:
        res.append(check_edge(run, nrun, cx, cy, int(a[7]), int(a[8]), exe, work))
    for ok, txt in res:
        print(('ok   ' if ok else 'FAIL ') + txt)
    bad = [t.split()[0] for ok, t in res if not ok]
    print('VERDICT %s %s (%d tris)' % ('PASS' if not bad else 'FAIL', ','.join(bad) or 'all', len(T)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
