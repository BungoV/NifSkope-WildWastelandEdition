#!/usr/bin/env python3
"""The bake's three albedo ways, judged (lane CAPTURE1, 2026-10-03).

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
  cell_albedo_check.py refract <main run> <repaired run>
      the refraction-only repair (Shader Flags 1 bit 15 shapes leave the soup in every way):
      R  the repaired soup lost triangles against main's and gained none (the keep red loses none: FAIL)
      P  the same probe count; a probe the placer moved stands within 3 cells of a lost triangle
      X  outside the zone (the lost triangles' cells + 1 around, and every cell a moved probe links) every
         surfel and every unmoved probe's link is main's bytes
One line a stage, then "albedo PASS|FAIL"; exit 0 on PASS.
"""
import glob
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import SURFEL, floordiv, read_tbk  # noqa: E402

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


def bake_maps(run):
    """Per bake: surfel bytes by (side, key) (first copy), probes by position -> {(side, key): (w, dir)}."""
    surf, probes, cell = {}, {}, None
    for f in files(os.path.join(run, 'bake')):
        t = read_tbk(os.path.join(run, 'bake', f))
        cell = t['cell']
        for side, arr in ((0, t['surfels']), (1, t['back'])):
            for s in arr:
                k = (side,) + tuple(floordiv(s['pos'][i], cell) for i in range(3))
                surf.setdefault(k, s.tobytes())
        for pr in t['probes']:
            pk = [floordiv(pr['pos'][i], cell) for i in range(3)]
            a0, a1 = int(pr['off']), int(pr['off'] + pr['cnt'])
            lx = t['lext'][a0:a1] if len(t['lext']) else [None] * (a1 - a0)
            lk = {}
            for l, x in zip(t['links'][a0:a1], lx):
                side = int(x['side']) if x is not None else 0
                lk[(side,) + tuple(pk[i] + int(l['delta'][i]) for i in range(3))] = (float(l['w']), l['dir'].tobytes())
            probes[tuple(np.round(np.array(pr['pos'], float), 3))] = lk
    return surf, probes, cell


def stage_refract(main_run, new_run):
    """lane CAPTURE1: the refraction-only repair, against main's bake of the same cell. Every difference
    must trace back to a lost triangle: its own cells, a probe the placer moved, a ray that now passes
    where a lost triangle stood (the probe's sphere changed: its other weights renormalize by one common
    factor), and the cells those changed spheres sample (their surfel and its side for every linker)."""
    def rows(t):
        return {r.tobytes() for r in t.reshape(len(t), 9).astype('<f4')}
    sm = rows(read_soup_tris(os.path.join(main_run, 'soup.psp')))
    sn = rows(read_soup_tris(os.path.join(new_run, 'soup.psp')))
    lost, gained = sm - sn, sn - sm
    out = []
    okr = len(lost) > 0 and not gained
    out.append('R %s soup lost %d triangles against main, gained %d (bar: lost > 0, gained 0)' % (
        'PASS' if okr else 'FAIL', len(lost), len(gained)))
    surf_m, pr_m, cell = bake_maps(main_run)
    surf_n, pr_n, _ = bake_maps(new_run)
    tri = [np.frombuffer(x, '<f4').reshape(3, 3).astype(float) for x in lost]
    lv = np.array([t.mean(0) for t in tri]) if tri else np.zeros((0, 3))
    lr = np.array([np.sqrt(((t - t.mean(0)) ** 2).sum(1)).max() for t in tri]) if tri else np.zeros(0)
    # 1. the lost triangles' own cells, one cell around
    zone = set()
    for t in tri:
        lo = [floordiv(t[:, i].min(), cell) - 1 for i in range(3)]
        hi = [floordiv(t[:, i].max(), cell) + 1 for i in range(3)]
        for x in range(lo[0], hi[0] + 1):
            for y in range(lo[1], hi[1] + 1):
                for z in range(lo[2], hi[2] + 1):
                    zone.add((x, y, z))
    # 2. probes the placer moved (it stands on the same soup): only near a lost triangle
    moved = set(pr_m) ^ set(pr_n)
    far = [p for p in moved if not len(lv) or np.sqrt(((lv - np.array(p)) ** 2).sum(1)).min() > 3 * cell]
    okp = len(pr_m) == len(pr_n) and not far
    out.append('P %s %d probes; %d moved, %d of them farther than 3 cells from a lost triangle' % (
        'PASS' if okp else 'FAIL', len(pr_n), len(set(pr_m) - set(pr_n)), len(far)))

    def through(p, k):   # the probe's segment to the cell's center passes a lost triangle's sphere
        a, b = np.array(p, float), (np.array(k, float) + 0.5) * cell
        d = b - a
        t = ((lv - a) @ d) / max(d @ d, 1e-9)
        r = np.sqrt((((a + np.clip(t, 0, 1)[:, None] * d) - lv) ** 2).sum(1))
        return bool(len(lv)) and bool(((r <= 0.87 * cell + lr) & (t > 0) & (t < 1)).any())
    # 3. changed spheres: moved probes, and unmoved probes with a changed link into the zone or through
    changed_sphere = set(moved)
    for p in set(pr_m) & set(pr_n):
        lm, ln = pr_m[p], pr_n[p]
        if any(lm.get(k) != ln.get(k) and (k[1:] in zone or through(p, k[1:])) for k in set(lm) | set(ln)):
            changed_sphere.add(p)
    # 4. the cells a changed sphere samples (main or repaired) may change their surfel and side
    reach = set(zone)
    for p in changed_sphere:
        for prs in (pr_m, pr_n):
            for k in prs.get(p, ()):
                reach.add(k[1:])
    keys = set(surf_m) | set(surf_n)
    diff_s = [k for k in keys if surf_m.get(k) != surf_n.get(k)]
    out_s = [k for k in diff_s if k[1:] not in reach]
    # 5. a cell left over that keeps its surfel's place, normal and sample count byte for byte had the same
    # rays hit the same points: only the triangle taken there changed. The soup's tree is rebuilt without
    # the lost triangles, so where two triangles meet a ray at the same distance (coincident surfaces, a
    # shared edge) it may take the other one, with its own albedo bytes
    def tie(k):
        a, b = surf_m.get(k), surf_n.get(k)
        if a is None or b is None:
            return False
        x, y = np.frombuffer(a, SURFEL)[0], np.frombuffer(b, SURFEL)[0]
        return (x['pos'].tobytes() == y['pos'].tobytes() and x['nrm'].tobytes() == y['nrm'].tobytes()
                and x['samples'] == y['samples'])
    ties = sum(1 for k in out_s if tie(k))
    out_s = [k for k in out_s if not tie(k)]
    # what is left is the same tie on a thinly sampled cell (a few rays, one taking another triangle moves
    # it far): bounded at half a per mille of the cell's surfels -- any wider change (a wrong shape dropped, a
    # way's albedo) fails
    bad_s = len(out_s)
    cap_s = int(0.0005 * len(keys))
    for k in sorted(out_s)[:6]:   # what moved there, and how far from the nearest lost triangle
        c = (np.array(k[1:], float) + 0.5) * cell
        out.append('  outside: side %d cell %s main %s repaired %s, %.0f units from a lost triangle' % (
            k[0], k[1:], k in surf_m, k in surf_n, np.sqrt(((lv - c) ** 2).sum(1)).min()))
    bad_l = rescaled = 0
    for p in set(pr_m) & set(pr_n):
        lm, ln = pr_m[p], pr_n[p]
        ratios = []
        for k in set(lm) | set(ln):
            if lm.get(k) == ln.get(k) or k[1:] in reach:
                continue
            if p in changed_sphere and k in lm and k in ln and lm[k][1] == ln[k][1]:
                ratios.append(ln[k][0] / max(lm[k][0], 1e-30))
            else:
                bad_l += 1
        if ratios and max(ratios) - min(ratios) <= 1e-4 * max(ratios):
            rescaled += len(ratios)
        else:
            bad_l += len(ratios)
    okx = bad_s <= cap_s and not bad_l
    out.append('X %s %d surfels differ from main, %d of them outside the reach (bar %d) (lost cells + 1 around: %d cells; '
               'every cell a changed probe sphere links: %d cells; %d spheres changed, %d of them moved; a tie '
               'outside it, the same hits on another triangle (albedo only): %d); links outside it: %d differ, %d rescaled by their '
               'probe\'s one common factor' % (
                   'PASS' if okx else 'FAIL', len(diff_s), bad_s, cap_s, len(zone), len(reach), len(changed_sphere),
                   len(moved) // 2, ties, bad_l, rescaled))
    return okr and okp and okx, out


def main(a):
    if a and a[0] == 'refract':
        ok, lines = stage_refract(a[1], a[2])
        print('\n'.join(lines))
        print('albedo', 'PASS' if ok else 'FAIL')
        return 0 if ok else 1
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
