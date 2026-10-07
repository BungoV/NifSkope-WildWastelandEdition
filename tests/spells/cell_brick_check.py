#!/usr/bin/env python3
"""The brick check (lane BRICK1, 2026-10-07; tests/spells/cell_brick.sh, src/probegi.h).

  cell_brick_check.py <run dir> [dump folder name]

The Division deck (Stefanov, GDC 2016, s25 + s38): surfels are combined into 4 m "irradiance bricks", keyed by
their facing; a brick's light is the MEAN of its surfels' light; a probe gathers bricks, not surfels, through six
stored weights per probe-brick pair:  probe.irradiance[dir] += brick.radiance x probe.brick_weights[brick][dir].
NifSkope's relight gathers its links (one per surfel) instead. This file rebuilds the bricks and the weights from
the .tbk files and the dump's surfels with its OWN reader and arithmetic (no line shared with probegi.cpp) and checks:

  K  the bricks     the dump's gi_bricks.bin is this file's own grouping: every surfel in the brick
                    floor(p / edge) x its facing (the dominant axis of its normal, with its sign), the
                    brick's light the mean of its surfels' last-pass B (gi_bounce.bin); same count, same
                    surfel counts, each light within 1e-4 relative
  W  the factors    each probe's six weights per brick = sum over its links into that brick of
                    weight x scale x 4 pi x max(axis . dir, 0); the glass tint the omega-weighted mean of the
                    links' tints; brick-gathered cube = the unlinked renormalisation x sum brick light x tint x
                    weight (+ an exterior's sky). The dump's gi_brickcube.bin must agree on >= 99% of the probes
  G  the gather     the brick gather is the link gather regrouped: with bricks of edge 1 (about one surfel
                    each) it gives this file's link gather back to <= 0.1%; and the exe's own brick-against-link
                    figure (gi_brickcube.bin against gi_linkcube.bin) is this file's within 0.2 points
  T  the terms      factors (probe-brick pairs) against resolved links: the exe's census count equals this
                    file's, and the gather has fewer terms than the links
  D  the deck's bar sum |E_brick - E_link| / sum E_link over every probe, axis and channel, against the 2% of
                    DECK_MATRIX F (BRICK1), with the same figure at edges 280 / 140 / 70 / 35. A MEASUREMENT of
                    what 4 m bricks cost, WITHIN or ABOVE; never part of the verdict (the code can be right and
                    the approximation coarse)
One line a stage, then "brick PASS" or "brick FAIL" (K W G T); exit 0 when they all pass.
"""
import math
import os
import re
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import read_tbk, unpack_dir, floordiv  # noqa: E402

AXES = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)


def read_dump(d):
    b = open(os.path.join(d, 'gi_surfels.bin'), 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    S = np.frombuffer(b, '<f4', n * 12, 4).reshape(n, 12).astype(np.float64)
    b = open(os.path.join(d, 'gi_bounce.bin'), 'rb').read()
    n, passes = struct.unpack_from('<2i', b)
    Bk = np.frombuffer(b, '<f4', n * 3, 8).reshape(n, 3).astype(np.float64)
    return S, Bk


def read_bricks(d):
    """gi_bricks.bin: f32 edge, i32 n, then n x (i32 x, y, z, facing, count, f32 r, g, b);
    gi_brickcube.bin: i32 n, then n x 21 f32 (gi_probes.bin's layout), gathered from the bricks;
    gi_linkcube.bin: the same, gathered from the links (both from the last pass's B)"""
    p = os.path.join(d, 'gi_bricks.bin')
    if not os.path.exists(p):
        return None
    b = open(p, 'rb').read()
    edge = struct.unpack_from('<f', b, 0)[0]
    n = struct.unpack_from('<i', b, 4)[0]
    rec = np.frombuffer(b, np.dtype([('k', '<i4', 5), ('L', '<f4', 3)]), n, 8)
    cubes = {}
    for name in ('gi_brickcube.bin', 'gi_linkcube.bin'):
        c = open(os.path.join(d, name), 'rb').read()
        m = struct.unpack_from('<i', c)[0]
        cubes[name] = np.frombuffer(c, '<f4', m * 21, 4).reshape(m, 21).astype(np.float64)
    return dict(edge=float(edge), keys=rec['k'].astype(np.int64), L=rec['L'].astype(np.float64),
                cube=cubes['gi_brickcube.bin'], linkcube=cubes['gi_linkcube.bin'])


def facing(nrm):
    a = int(np.argmax(np.abs(nrm)))
    return 2 * a + (0 if nrm[a] >= 0 else 1)


def close(a, b, rel=0.02, ab=1e-3):
    return bool(np.all(np.abs(a - b) <= ab + rel * np.maximum(np.abs(a), np.abs(b))))


def group(S, Bk, edge):
    """this file's bricks: key (floor(p / edge) per axis, facing); light = the mean of the members' B"""
    own = {}
    brick_of = np.zeros(len(S), np.int64)
    for i in range(len(S)):
        k = (floordiv(S[i, 0], edge), floordiv(S[i, 1], edge), floordiv(S[i, 2], edge), facing(S[i, 3:6]))
        brick_of[i] = own.setdefault(k, len(own))
    cnt = np.bincount(brick_of, minlength=len(own)).astype(np.float64)
    L = np.zeros((len(own), 3))
    np.add.at(L, brick_of, Bk)
    L /= cnt[:, None]
    return brick_of, L, cnt, own


def read_links(S, bake):
    """every resolved link of every probe (stage B's reading of the .tbk: cell keys by side, octahedral dir,
    tint), as flat arrays; a link resolves to the dump's surfel row by its float32 position and its normal"""
    row_of = {}
    for i in range(len(S)):
        row_of.setdefault(np.float32(S[i, 0:3]).astype('<f4').tobytes(), []).append(i)
    tbks = [read_tbk(os.path.join(bake, f)) for f in sorted(os.listdir(bake)) if f.endswith('.tbk')]
    prb, rows, oms, coss, tints, kks = [], [], [], [], [], []
    k = 0
    for t in tbks:
        cs = float(t['cell'])
        keys = tuple({} for _ in t['bysides'])
        for side, arr in enumerate(t['bysides']):
            for j, s in enumerate(arr):
                keys[side].setdefault(tuple(floordiv(s['pos'][a], cs) for a in range(3)), j)
        for pr in t['probes']:
            pk = [floordiv(pr['pos'][a], cs) for a in range(3)]
            linked = 0.0
            a0 = int(pr['off'])
            for li, lk in enumerate(t['links'][pr['off']:pr['off'] + pr['cnt']]):
                x = t['lext'][a0 + li]
                side = int(x['side'])
                j = keys[side].get(tuple(int(pk[a]) + int(lk['delta'][a]) for a in range(3)))
                if j is None:
                    continue
                s = t['bysides'][side][j]
                nv = np.array(s['nrm'], float)
                nv /= np.linalg.norm(nv)
                row = None
                for i in row_of.get(np.float32(s['pos']).astype('<f4').tobytes(), []):
                    if np.allclose(S[i, 3:6], nv, atol=1e-5):
                        row = i
                if row is None:
                    continue
                w = float(lk['w']) * float(pr['scale'])
                linked += w
                prb.append(k)
                rows.append(row)
                oms.append(w * 4 * math.pi)
                coss.append(np.maximum(AXES @ unpack_dir(lk['dir']), 0.0))
                tints.append(x['tint'].astype(np.float64) / 255.0)
            kks.append((linked + float(pr['unl'])) / linked if linked > 0 and pr['unl'] > 0 else 1.0)
            k += 1
    return dict(n=k, prb=np.array(prb, np.int64), row=np.array(rows, np.int64), om=np.array(oms),
                cos=np.array(coss).reshape(-1, 6), tint=np.array(tints).reshape(-1, 3), kk=np.array(kks))


def with_sky(E, sky):
    return E + sky[:len(E)] if sky is not None else E


def link_gather(LK, Bk, sky):
    E = np.zeros((LK['n'], 6, 3))
    c = (LK['cos'] * LK['om'][:, None])[:, :, None] * (Bk[LK['row']] * LK['tint'])[:, None, :]
    np.add.at(E, LK['prb'], c)
    E *= LK['kk'][:, None, None]
    return with_sky(E, sky)


def brick_gather(LK, brick_of, L, sky):
    """the deck's gather: per probe-brick pair six weights (sum of omega x cos), the omega-weighted tint;
    E = sum W x brick light x tint"""
    nb = int(brick_of.max()) + 1
    pair, inv = np.unique(LK['prb'] * nb + brick_of[LK['row']], return_inverse=True)
    inv = inv.reshape(-1)
    W = np.zeros((len(pair), 6))
    T = np.zeros((len(pair), 3))
    O = np.zeros(len(pair))
    np.add.at(W, inv, LK['cos'] * LK['om'][:, None])
    np.add.at(T, inv, LK['tint'] * LK['om'][:, None])
    np.add.at(O, inv, LK['om'])
    T = np.where(O[:, None] > 0, T / np.maximum(O, 1e-300)[:, None], 1.0)
    E = np.zeros((LK['n'], 6, 3))
    np.add.at(E, pair // nb, W[:, :, None] * (L[pair % nb] * T)[:, None, :])
    E *= LK['kk'][:, None, None]
    return with_sky(E, sky), len(pair)


def spread(Eb, El):
    rel = float(np.abs(Eb - El).sum()) / max(float(El.sum()), 1e-300)
    per = np.abs(Eb - El).sum((1, 2)) / np.maximum(El.sum((1, 2)), 1e-9)
    lit = El.sum((1, 2)) > 1e-3
    p95 = float(np.percentile(per[lit], 95)) if lit.any() else 0.0
    p50 = float(np.percentile(per[lit], 50)) if lit.any() else 0.0
    return rel, p50, p95, int(lit.sum())


def main(run, dumpname='dump'):
    dump = os.path.join(run, dumpname)
    lines = []
    BR = read_bricks(dump)
    if BR is None:
        lines.append('K FAIL bricks: no gi_bricks.bin in %s (the exe builds no bricks)' % dump)
        return lines
    S, Bk = read_dump(dump)
    edge = BR['edge']
    # ---- K: this file's bricks from the dump's surfels (position, normal) and the last pass's B
    brick_of, L, cnt, own = group(S, Bk, edge)
    theirs = {tuple(int(x) for x in BR['keys'][i, 0:4]): i for i in range(len(BR['keys']))}
    good = 0
    for k, j in own.items():
        t = theirs.get(k)
        if t is not None and int(BR['keys'][t, 4]) == int(cnt[j]) and close(BR['L'][t], L[j], 1e-4, 1e-6):
            good += 1
    okK = len(theirs) == len(own) and good == len(own)
    lines.append('K %s bricks: edge %.0f, %d bricks (the exe %d), %d surfels, %.1f surfels a brick; %d agree'
                 % ('PASS' if okK else 'FAIL', edge, len(own), len(theirs), len(S), len(S) / max(len(own), 1), good))
    # ---- W: the factors, and this file's brick-gathered cube against the exe's
    LK = read_links(S, os.path.join(run, 'bake'))
    sky = None
    sp = os.path.join(dump, 'gi_sky.bin')
    if os.path.exists(sp):
        b = open(sp, 'rb').read()
        sky = np.frombuffer(b, '<f4', struct.unpack_from('<i', b)[0] * 18, 4).reshape(-1, 6, 3).astype(np.float64)
    El = link_gather(LK, Bk, sky)
    Eb, nfac = brick_gather(LK, brick_of, L, sky)
    nlink = len(LK['row'])
    if len(Eb) != len(BR['cube']):
        lines.append('W FAIL factors: the dump holds %d probes, the bake %d' % (len(BR['cube']), len(Eb)))
        return lines
    agree = sum(1 for i in range(len(Eb)) if close(BR['cube'][i, 3:21].reshape(6, 3), Eb[i]))
    okW = agree >= 0.99 * len(Eb)
    lines.append('W %s factors: %d probes, %d probe-brick factors; the exe\'s brick-gathered cube agrees on %.1f%%'
                 % ('PASS' if okW else 'FAIL', len(Eb), nfac, 100.0 * agree / max(len(Eb), 1)))
    # ---- G: the brick gather is the link gather regrouped
    rel, p50, p95, nlit = spread(Eb, El)
    exe_rel = float(np.abs(BR['cube'][:, 3:] - BR['linkcube'][:, 3:]).sum() / max(BR['linkcube'][:, 3:].sum(), 1e-9))
    tiny = 1.0
    bo1, L1, c1, _ = group(S, Bk, tiny)
    r1 = spread(brick_gather(LK, bo1, L1, sky)[0], El)[0]
    single = float((c1 == 1).mean())
    okG = abs(exe_rel - rel) <= 0.002 and r1 <= 1e-3
    lines.append('G %s the gather: brick against link %.3f%% of the light (the exe says %.3f%%); bricks of edge %g '
                 '(%.2f%% of them one surfel) give the link gather back to %.5f%%'
                 % ('PASS' if okG else 'FAIL', 100 * rel, 100 * exe_rel, tiny, 100 * single, 100 * r1))
    # ---- T: the term count
    meta = open(os.path.join(dump, 'gi_meta.txt'), encoding='utf-8').read()
    m = re.search(r'gi bricks: (\d+) bricks .*?, (\d+) factors for (\d+) links', meta)
    exe_fac = int(m.group(2)) if m else -1
    exe_links = int(m.group(3)) if m else -1
    okT = exe_fac == nfac and exe_links == nlink and nfac < nlink
    lines.append('T %s terms: %d factors for %d links (%.2fx fewer gather terms); the exe\'s census %d / %d'
                 % ('PASS' if okT else 'FAIL', nfac, nlink, nlink / max(nfac, 1), exe_fac, exe_links))
    # ---- D: the deck's 2% bar -- a measurement of the approximation, printed, never part of the verdict
    curve = []
    for e in (280.0, 140.0, 70.0, 35.0):
        bo, Le, _, _ = group(S, Bk, e)
        curve.append('%g: %.2f%%' % (e, 100 * spread(brick_gather(LK, bo, Le, sky)[0], El)[0]))
    lines.append('D %s the deck\'s 2%% bar at edge %.0f: %.3f%% (per lit probe median %.2f%%, 95th percentile '
                 '%.2f%%, %d lit probes); by edge %s'
                 % ('WITHIN' if rel <= 0.02 else 'ABOVE', edge, 100 * rel, 100 * p50, 100 * p95, nlit,
                    ', '.join(curve)))
    return lines


if __name__ == '__main__':
    out = main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else 'dump')
    for line in out:
        print(line)
    verdict = [line for line in out if not line.startswith('D ')]
    ok = len(verdict) == 4 and all(line.split()[1] == 'PASS' for line in verdict)
    print('brick PASS' if ok else 'brick FAIL')
    sys.exit(0 if ok else 1)
