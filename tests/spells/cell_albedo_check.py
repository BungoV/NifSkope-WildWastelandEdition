#!/usr/bin/env python3
"""The bake's three albedo ways, judged (lane CAPTURE1, 2026-10-03; tests/spells/cell_albedo.sh).

WW_CELL_BAKE_ALBEDO = tri (default) | hit | cube changes ONE thing in the `.tbk` files: each surfel's
albedo and normal. Everything here reads the files with probe_bake.py's own reader and the soup with
its own parser; nothing from NifSkope.

  cell_albedo_check.py same <bake dir A> <bake dir B>
      B  every file of A is in B, byte for byte, and no other (the default way = the pre-lane bake)
  cell_albedo_check.py way <tri run> <way run> [--dump <cube dump> --soup <soup.psp>]
      S  same set: per file the header, probes, links, link and probe tails and room boxes are the tri
         bake's bytes; every surfel (front and back) sits where tri's does with tri's sample count. Counts
         the surfels whose albedo or normal moved.
      D  the way did something: at least 10 % of surfels moved albedo by 2+ (8-bit linear) from tri
         (bar set before the first run; the centroid red reads the triangle's own tri value and must FAIL)
      C  cube only: 600 pixels of a dumped probe face set (fixed seed), each re-traced here through every
         soup triangle (double-sided Moller-Trumbore, numpy): the dump's distance must equal this trace's
         (0.1 % + 0.05 units) on 99 % of them, and sky exactly where the trace meets nothing -- a cube
         that sees a shape the soup left out (the nofilter red) lands nearer than the soup: FAIL
  cell_albedo_check.py judge <judge run> <name>=<run> ...
      numbers per way against the judge (the cube way at 256 px a face): per surfel, the largest channel
      difference in 8-bit sRGB, and the normal angle in degrees (median / p90), over every surfel and over
      the surfels on big flat walls (soup triangles of 20000+ square units, their plane within 2 units)
One line a stage, then "albedo PASS|FAIL"; exit 0 on PASS.
"""
import glob
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import read_tbk  # noqa: E402

def files(d):
    return sorted(os.path.basename(f) for f in glob.glob(os.path.join(d, '*.tbk')))


def stage_same(a, b):
    fa, fb = files(a), files(b)
    if not fa or fa != fb:
        return False, 'B FAIL file lists differ (%d vs %d)' % (len(fa), len(fb))
    diff = [f for f in fa if open(os.path.join(a, f), 'rb').read() != open(os.path.join(b, f), 'rb').read()]
    return not diff, 'B %s %d files, %d differ%s' % ('PASS' if not diff else 'FAIL', len(fa), len(diff),
                                                     (' (' + ', '.join(diff[:4]) + ')') if diff else '')


def to_srgb8(lin8):
    c = np.clip(lin8.astype(np.float64) / 255.0, 0, 1)
    s = np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)
    return s * 255.0


def unit(n):
    n = n.astype(np.float64) / 32767.0
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)


def surfels_of(run):
    """All surfels of a bake: key (file, side, index) order, positions, albedo, normals."""
    P, A, N, S = [], [], [], []
    for f in files(os.path.join(run, 'bake')):
        t = read_tbk(os.path.join(run, 'bake', f))
        for arr in (t['surfels'], t['back']):
            P.append(arr['pos'].astype(np.float64))
            A.append(arr['alb'].astype(np.int32))
            N.append(arr['nrm'].astype(np.int32))
            S.append(arr['samples'].astype(np.int64))
    return np.concatenate(P), np.concatenate(A), np.concatenate(N), np.concatenate(S)


def stage_set(tri, way):
    ft, fw = files(os.path.join(tri, 'bake')), files(os.path.join(way, 'bake'))
    if not ft or ft != fw:
        return False, 'S FAIL file lists differ (%d vs %d)' % (len(ft), len(fw)), 0, 0
    bad = []
    moved_a = moved_n = total = 0
    for f in ft:
        bt = open(os.path.join(tri, 'bake', f), 'rb').read()
        bw = open(os.path.join(way, 'bake', f), 'rb').read()
        if len(bt) != len(bw) or bt[:64] != bw[:64]:
            bad.append(f + ' header/size')
            continue
        t, w = read_tbk(os.path.join(tri, 'bake', f)), read_tbk(os.path.join(way, 'bake', f))
        for k in ('probes', 'links', 'lext', 'pext', 'boxes'):
            if t[k].tobytes() != w[k].tobytes():
                bad.append(f + ' ' + k)
        for k in ('surfels', 'back'):
            if (t[k]['pos'].tobytes() != w[k]['pos'].tobytes()
                    or t[k]['samples'].tobytes() != w[k]['samples'].tobytes()):
                bad.append(f + ' ' + k + ' positions')
            total += len(t[k])
            moved_a += int((np.abs(t[k]['alb'].astype(int) - w[k]['alb'].astype(int)).max(1) >= 2).sum()) if len(t[k]) else 0
            moved_n += int((t[k]['nrm'] != w[k]['nrm']).any(1).sum()) if len(t[k]) else 0
    ok = not bad
    line = 'S %s %d files, %d surfels at tri\'s positions; links, probes, tails the same bytes%s; albedo moved on %d, normal on %d' % (
        'PASS' if ok else 'FAIL', len(ft), total, '' if ok else ' EXCEPT ' + ', '.join(bad[:5]), moved_a, moved_n)
    return ok, line, moved_a, total


def read_soup_tris(path):
    with open(path, 'rb') as f:
        magic, ntri, ndoor = struct.unpack('<III', f.read(12))
        return np.frombuffer(f.read(ntri * 36), dtype='<f4').reshape(ntri, 3, 3).astype(np.float64)


def cube_dirs(S):
    y, x = np.mgrid[0:S, 0:S]
    u, v = 2 * (x + 0.5) / S - 1, 1 - 2 * (y + 0.5) / S
    out = []
    for face in range(6):
        if face < 4:
            f = [(1, 0, 0), (0, -1, 0), (-1, 0, 0), (0, 1, 0)][face]
            r, up = (f[1], -f[0], 0), (0, 0, 1)
        elif face == 4:
            f, r, up = (0, 0, 1), (0, -1, 0), (-1, 0, 0)
        else:
            f, r, up = (0, 0, -1), (0, -1, 0), (1, 0, 0)
        q = np.array(f)[None, None] + u[..., None] * np.array(r)[None, None] + v[..., None] * np.array(up)[None, None]
        out.append(q / np.linalg.norm(q, axis=-1, keepdims=True))
    return np.concatenate([a.reshape(-1, 3) for a in out])


def trace_one(tris, e1, e2, o, d):
    # double-sided Moller-Trumbore over every triangle; nearest t > 1e-3, or -1
    pv = np.cross(d[None], e2)
    det = (e1 * pv).sum(1)
    ok = np.abs(det) > 1e-12
    inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
    tv = o[None] - tris[:, 0]
    u = (tv * pv).sum(1) * inv
    qv = np.cross(tv, e1)
    v = (qv * d[None]).sum(1) * inv
    t = (e2 * qv).sum(1) * inv
    hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-3)
    return float(t[hit].min()) if hit.any() else -1.0


def stage_cube(dump, soup):
    txt = open(dump + '.txt').read().split()
    if len(txt) < 5:
        return False, 'C FAIL no dumped probe'
    o = np.array([float(x) for x in txt[1:4]])
    S = int(txt[4])
    N = 6 * S * S
    b = open(dump, 'rb').read()
    dist = np.frombuffer(b, '<f4', N, N * 3).astype(np.float64)
    tris = read_soup_tris(soup)
    # the triangles near enough to matter for the sampled rays: all of them (a cell is ~1.5 M; numpy copes)
    e1, e2 = tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]
    D = cube_dirs(S)
    rng = np.random.default_rng(20261003)
    pick = rng.choice(N, 600, replace=False)
    agree = 0
    worst = []
    for px in pick:
        t = trace_one(tris, e1, e2, o, D[px])
        dd = dist[px]
        same = (t < 0 and dd < 0) or (t >= 0 and dd >= 0 and abs(t - dd) <= 0.001 * t + 0.05)
        agree += same
        if not same:
            worst.append((int(px), round(dd, 2), round(t, 2)))
    ok = agree >= 0.99 * len(pick)
    return ok, 'C %s probe at %s, %d px faces: %d of %d sampled pixels at the soup\'s own distance (bar 99 %%)%s' % (
        'PASS' if ok else 'FAIL', o.round(0).tolist(), S, agree, len(pick),
        '' if ok else '; e.g. (pixel, dump, soup) ' + str(worst[:4]))


def big_wall_mask(P, soup, area=20000.0, tol=2.0):
    tris = read_soup_tris(soup)
    e1, e2 = tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]
    cr = np.cross(e1, e2)
    ar = 0.5 * np.linalg.norm(cr, axis=1)
    big = np.nonzero(ar >= area)[0]
    m = np.zeros(len(P), bool)
    for i in big:
        n = cr[i] / (2 * ar[i])
        dpl = (P - tris[i, 0]) @ n
        near = np.abs(dpl) <= tol
        if not near.any():
            continue
        q = P[near] - np.outer(dpl[near], n)
        # barycentric inside test
        v0, v1, v2 = e1[i], e2[i], q - tris[i, 0]
        d00, d01, d11 = v0 @ v0, v0 @ v1, v1 @ v1
        d20, d21 = v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01
        if abs(den) < 1e-12:
            continue
        b1 = (d11 * d20 - d01 * d21) / den
        b2 = (d00 * d21 - d01 * d20) / den
        inside = (b1 >= -0.01) & (b2 >= -0.01) & (b1 + b2 <= 1.01)
        idx = np.nonzero(near)[0][inside]
        m[idx] = True
    return m, len(big)


def judge(jrun, ways):
    Pj, Aj, Nj, _ = surfels_of(jrun)
    mask, nbig = big_wall_mask(Pj, os.path.join(jrun, 'soup.psp'))
    sj, nj = to_srgb8(Aj), unit(Nj)
    rows = ['judge %s: %d surfels, %d on big flat walls (%d soup triangles of 20000+ sq units)' % (
        os.path.basename(jrun), len(Pj), int(mask.sum()), nbig)]
    for name, run in ways:
        P, A, N, _ = surfels_of(run)
        if P.shape != Pj.shape or not np.array_equal(P, Pj):
            rows.append('%-8s surfel set differs from the judge: not comparable' % name)
            continue
        ea = np.abs(to_srgb8(A) - sj).max(1)
        en = np.degrees(np.arccos(np.clip((unit(N) * nj).sum(1), -1, 1)))
        def q(x):
            return '%5.1f / %5.1f' % (np.median(x), np.percentile(x, 90)) if len(x) else '  n/a'
        rows.append('%-8s albedo err sRGB8 med/p90 all %s  walls %s | normal deg med/p90 all %s  walls %s' % (
            name, q(ea), q(ea[mask]), q(en), q(en[mask])))
    return rows


def main(a):
    if a and a[0] == 'same':
        ok, line = stage_same(a[1], a[2])
        print(line)
        print('albedo', 'PASS' if ok else 'FAIL')
        return 0 if ok else 1
    if a and a[0] == 'way':
        tri, way = a[1], a[2]
        dump = a[a.index('--dump') + 1] if '--dump' in a else None
        soup = a[a.index('--soup') + 1] if '--soup' in a else os.path.join(way, 'soup.psp')
        ok, line, moved, total = stage_set(tri, way)
        print(line)
        share = moved / max(total, 1)
        okd = share >= 0.10
        print('D %s albedo moved on %.1f %% of %d surfels (bar 10 %%)' % ('PASS' if okd else 'FAIL', 100 * share, total))
        allok = ok and okd
        if dump:
            okc, linec = stage_cube(dump, soup)
            print(linec)
            allok = allok and okc
        print('albedo', 'PASS' if allok else 'FAIL')
        return 0 if allok else 1
    if a and a[0] == 'judge':
        for r in judge(a[1], [x.split('=', 1) for x in a[2:]]):
            print(r)
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
