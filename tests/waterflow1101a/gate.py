"""Lane FIX1101A flow (LATE ITEM 10): the water FLOW gate on a Commonwealth .lodl (v3/v4), offline.

  A  every reference reach (reaches.json: the 8 proposal rivers + the 71 sea-level channel branches of the WATERMAP
     proposal, C1 = bungo's drawn Neponset bay current, C3/C7/C8 the lower Mystic and the arms he drew INTO it), each
     an upstream -> downstream texel path: at every path texel (3 from either end skipped) the decoded flow direction vs
     the path tangent, the chord p[k+3] - p[k-3] (at most a quarter of the path; the +-10 chord is printed beside
     it); still water counts 180 deg. Bar: mean angle error < 20 deg on every GATED
     reach. Pre-registered (before the first new bake, from the twin on the 10-28 bake): GATED = the proposal rivers,
     the bungo-drawn reaches, and every channel branch at least 2x as long as its median width; a shorter branch is a
     walk ACROSS wide water (C62: 62 m long, 84 m wide), not an arrow: reported, not gated. Path texels on the OPEN
     SEA (the channel rule's ocean on the 10-28 geometry, still by design; reaches.json 'ocean' = their indices) are
     left out of a reach and counted.
  B  zero current: every texel of every lake / swamp / calm body of the sidecar (resolved by its point, as the baker
     does) decodes to speed 0. Bar: 0 moving texels.
  C  the sea body (class sea in the sidecar) keeps no current away from its channels: reported, not gated.

Flow word (src/gl/cellwaterinfo.cpp): 0 = still; conf = w >> 12; conf 0 = the body's table mean (flowX, flowY);
else angle (w & 255) / 256 x 2 pi (x east, y north), speed (w >> 8 & 15) x |mean| / 8.

usage: python -I gate.py <file.lodl> <sidecar .waterflow.txt> [--expect-fail]   rc 0 = the expectation held
"""
import json
import math
import os
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
BAR = 20.0
SKIP = 3
CHORD = 10


def decode(path):
    f = open(path, 'rb')
    h = f.read(0x108)
    assert h[:4] == b'LODT', 'not a .lodl: ' + path
    ver = struct.unpack_from('<I', h, 4)[0]
    assert ver in (3, 4), 'not a v3/v4 .lodl'
    X0, Y0, X1, Y1, _spc = struct.unpack_from('<iiiiI', h, 8)
    (oBody, nBody, recB, _oN, _nN, bodyS, oBodyP, flowS, _enc, oFlow) = struct.unpack_from('<QIIQIIQIIQ', h, 0xA0)
    f.seek(oBody)
    bt = f.read(nBody * recB)
    B = {}
    for k in range(nBody):
        o = k * recB
        bid, c, _fl, wh, form, area = struct.unpack_from('<HBBfII', bt, o)
        outl = struct.unpack_from('<H', bt, o + 0x1A)[0]
        fx, fy = struct.unpack_from('<ff', bt, o + 0x1C)
        B[bid] = dict(cls=c, wh=wh, area=area, outlet=outl, fx=fx, fy=fy, src=bt[o + 0x29])

    def plane(off):
        f.seek(off)
        tX, tY, te, b = struct.unpack('<IIII', f.read(16))
        dOff, _ = struct.unpack('<QQ', f.read(16))
        assert b == 2
        f.seek(dOff)
        dirb = f.read(tX * tY * 16)
        a = np.zeros((tY * te, tX * te), np.uint16)
        for j in range(tY):
            for i in range(tX):
                o, cs, us = struct.unpack_from('<QII', dirb, (j * tX + i) * 16)
                if cs == 0:
                    if us:
                        a[j * te:(j + 1) * te, i * te:(i + 1) * te] = us
                else:
                    f.seek(o)
                    a[j * te:(j + 1) * te, i * te:(i + 1) * te] = np.frombuffer(
                        zlib.decompress(f.read(cs)), '<u2').reshape(te, te)
        return a

    ids = plane(oBodyP)
    flow = plane(oFlow)
    f.close()
    assert bodyS == flowS, 'the gate reads equal body / flow rates'
    return dict(X0=X0, Y0=Y0, S=bodyS, B=B, ids=ids, flow=flow, n=nBody)


def vec(L, r, c):
    """decoded (vx, vy) of texel (row, col) arrays"""
    w = L['flow'][r, c].astype(np.int64)
    b = L['ids'][r, c].astype(np.int64)
    n = L['n']
    fx = np.zeros(n + 1)
    fy = np.zeros(n + 1)
    for k, v in L['B'].items():
        fx[k] = v['fx']
        fy[k] = v['fy']
    mm = np.hypot(fx, fy)
    conf = (w >> 12) & 15
    a = (w & 255) / 256.0 * 2 * math.pi
    s = ((w >> 8) & 15) * mm[b] / 8.0
    vx = np.where(conf == 0, fx[b], np.cos(a) * s)
    vy = np.where(conf == 0, fy[b], np.sin(a) * s)
    vx = np.where(w == 0, 0.0, vx)
    vy = np.where(w == 0, 0.0, vy)
    return vx, vy


def sidecar(path):
    bodies = []
    for ln in open(path, encoding='utf-8'):
        t = ln.split('#')[0].split()
        if len(t) >= 5 and t[0] == 'body':
            bodies.append((int(t[1]), float(t[2]), float(t[3]), t[4]))
    return bodies


def joins(path):
    """[(x, y)] the first point of every join line (the draining body's own texel at its contact)"""
    out = []
    for ln in open(path, encoding='utf-8'):
        t = ln.split('#')[0].split()
        if len(t) >= 7 and t[0] == 'join':
            out.append((float(t[2]), float(t[3])))
    return out


def cut_at_joins(R, L, side):
    """a proposal river path runs between its two geodesic extremes, which can lie PAST its outlet (body 3's runs
    on 0.9 cell west of the Charles dam D1 into a dead-end bay above it): a river reach with a join line in the
    sidecar ends at the path point nearest that join's contact (pre-registered from the shipped bake's body 3, last
    decile 137 deg: the flow there runs back east to the dam)"""
    S, X0, Y0 = L['S'], L['X0'], L['Y0']
    for e in R:
        if e['kind'] != 'river':
            continue
        rc = np.array(e['rc'], np.int64).reshape(-1, 2)
        own = np.bincount(L['ids'][rc[:, 0], rc[:, 1]].astype(np.int64)).argmax()
        for (x, y) in joins(side):
            if body_at(L, x, y) != own:
                continue
            gx, gy = (x - X0) * S - 0.5, (y - Y0) * S - 0.5
            k = int(np.argmin((rc[:, 1] - gx) ** 2 + (rc[:, 0] - gy) ** 2))
            if k < len(rc) - 1 and k >= 8:
                e['cut'] = '%d of %d' % (k + 1, len(rc))
                e['rc'] = rc[:k + 1].reshape(-1).tolist()


def body_at(L, x, y):
    gx = int(math.floor((x - L['X0']) * L['S']))
    gy = int(math.floor((y - L['Y0']) * L['S']))
    ids = L['ids']
    for r in range(5):
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if max(abs(dx), abs(dy)) != r:
                    continue
                X, Y = gx + dx, gy + dy
                if 0 <= X < ids.shape[1] and 0 <= Y < ids.shape[0] and ids[Y, X]:
                    return int(ids[Y, X])
    return 0


def gated(e):
    return e['kind'] == 'river' or bool(e.get('drawn')) or float(e.get('length_m') or 0) >= 2.0 * float(e.get('width_m') or 1e9)


NB16 = [(dy, dx) for dy in range(-2, 3) for dx in range(-2, 3)
        if (dy or dx) and math.gcd(abs(dy), abs(dx)) == 1]


def geo_down(rc, ks, ocean_idx, water):
    """DOWNSTREAM direction at the path points ks: -grad of the geodesic distance (16-neighbour Dijkstra, Euclidean
    steps) through water to the reach's mouth (its last point off the open sea). water(r0, r1, c0, c1) -> bool mask"""
    import heapq
    O = set(int(k) for k in ocean_idx)
    off = [k for k in range(len(rc)) if k not in O]
    km = off[-1] if off else len(rc) - 1
    M = 48
    r0, r1 = int(rc[:, 0].min()) - M, int(rc[:, 0].max()) + M + 1
    c0, c1 = int(rc[:, 1].min()) - M, int(rc[:, 1].max()) + M + 1
    m = water(r0, r1, c0, c1)
    h, w = m.shape
    d = np.full(m.shape, np.inf)
    s = (int(rc[km, 0]) - r0, int(rc[km, 1]) - c0)
    if not m[s]:
        m = m.copy()
        m[s] = True
    d[s] = 0.0
    q = [(0.0, s)]
    st = [(dy, dx, math.hypot(dy, dx)) for dy, dx in NB16]
    while q:
        dd, (a, b) = heapq.heappop(q)
        if dd > d[a, b]:
            continue
        for dy, dx, wgt in st:
            y, x = a + dy, b + dx
            if 0 <= y < h and 0 <= x < w and m[y, x]:
                if abs(dy) == 2 or abs(dx) == 2:   # a knight step must not jump a bank: its two middle cells wet
                    hy, hx = (dy // 2, 0) if abs(dy) == 2 else (0, dx // 2)
                    if not (m[a + hy, b + hx] and m[a + dy - hy, b + dx - hx]):
                        continue
                nd = dd + wgt
                if nd < d[y, x]:
                    d[y, x] = nd
                    heapq.heappush(q, (nd, (y, x)))
    gy = np.zeros(len(ks))
    gx = np.zeros(len(ks))
    for i, k in enumerate(ks):
        a, b = int(rc[k, 0]) - r0, int(rc[k, 1]) - c0
        for st_ in (2, 1):
            lo_y, hi_y = d[a - st_, b], d[a + st_, b]
            lo_x, hi_x = d[a, b - st_], d[a, b + st_]
            if np.isfinite(lo_y) and np.isfinite(hi_y) and np.isfinite(lo_x) and np.isfinite(hi_x):
                gy[i] = -(hi_y - lo_y)
                gx[i] = -(hi_x - lo_x)
                break
    return gx, gy   # (east, north) toward the mouth


def score_reaches(R, field, out=print, water=None):
    """field(rows, cols) -> (vx, vy). A reach's 'ocean' = its point indices on the OPEN SEA (the channel rule's
    ocean on the 10-28 geometry, written by the twin): left out. Returns ([per reach dict], [failing gated])"""
    res = []
    for e in R:
        rc = np.array(e['rc'], np.int64).reshape(-1, 2)
        if len(rc) < 2 * SKIP + 3:
            k0, k1 = 1, len(rc) - 1
        else:
            k0, k1 = SKIP, len(rc) - SKIP
        ks = np.arange(k0, k1)
        vx, vy = field(rc[ks, 0], rc[ks, 1])
        sea = np.isin(ks, np.array(e.get('ocean', []), np.int64))
        sp = np.hypot(vx, vy)
        geo = float('nan')
        if water is not None:
            gx, gy = geo_down(rc, ks, e.get('ocean', []), water)
            okg = (np.hypot(gx, gy) > 0) & ~sea
            ag = np.degrees(np.arctan2(np.abs(gx * vy - gy * vx), gx * vx + gy * vy))
            ag = np.where(sp > 0, ag, 180.0)[okg]
            geo = float(ag.mean()) if len(ag) >= 3 else float('nan')

        def errs(sk):
            """angle error vs the chord p[k+sk] - p[k-sk] (clamped at the ends), still = 180"""
            tr = (rc[np.minimum(ks + sk, len(rc) - 1), 0] - rc[np.maximum(ks - sk, 0), 0]).astype(float)
            tc = (rc[np.minimum(ks + sk, len(rc) - 1), 1] - rc[np.maximum(ks - sk, 0), 1]).astype(float)
            ok_ = (np.hypot(tr, tc) > 0) & ~sea
            a_ = np.degrees(np.arctan2(np.abs(tc * vy - tr * vx), tc * vx + tr * vy))
            return np.where(sp > 0, a_, 180.0)[ok_], ok_
        # the gated tangent is the +-3 chord; the +-10 chord (18 m) is printed beside it (tried on the twin before
        # the first bake: longer chords cut the bends and read WORSE, 13 / 17 failing reaches vs 10 at +-3)
        ang, ok = errs(min(SKIP, max(1, (len(rc) - 1) // 4)))
        ang10, _ = errs(max(1, min(CHORD, (len(rc) - 1) // 4)))
        n = len(ang)
        mean = float(ang.mean()) if n >= 3 else float('nan')
        mean3 = float(ang10.mean()) if len(ang10) >= 3 else float('nan')
        still = float((sp[ok] <= 0).mean()) if n else 1.0
        g = gated(e) and n >= 3
        res.append(dict(name=e['name'], kind=e['kind'], gated=g, n=n, sea=int(sea.sum()), mean=mean, mean3=mean3, geo=geo,
                        median=float(np.median(ang)) if n else float('nan'),
                        p90=float(np.percentile(ang, 90)) if n else float('nan'), still=still,
                        drawn=bool(e.get('drawn')), size='%s/%s m' % (e.get('length_m', ''), e.get('width_m', ''))))
    for r in res:
        tag = ('OK  ' if r['mean'] < BAR else 'FAIL') if r['gated'] else 'rep '
        out('  %s %-4s %-7s %4d pts (+%3d on sea)  mean %6.1f (+-10: %6.1f, geo %6.1f)  median %6.1f  p90 %6.1f  still %5.1f %%  %s%s'
            % (tag, r['name'], r['kind'], r['n'], r['sea'], r['mean'], r['mean3'], r['geo'], r['median'], r['p90'], 100 * r['still'],
               r['size'] if r['kind'] == 'channel' else '', '  (bungo drawn)' if r['drawn'] else ''))
    G = [r for r in res if r['gated']]
    bad = [r for r in G if not r['mean'] < BAR]
    w = sorted(G, key=lambda r: -r['mean'])[:5]
    out('  reaches %d, gated %d, failing %d; worst gated %s; not gated %d (median of their means %.1f)'
        % (len(res), len(G), len(bad), ', '.join('%s %.1f' % (r['name'], r['mean']) for r in w),
           len(res) - len(G), float(np.nanmedian([r['mean'] for r in res if not r['gated']] or [float('nan')]))))
    return res, bad


def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    expect_fail = '--expect-fail' in sys.argv
    lodl, side = a[0], a[1]
    L = decode(lodl)
    R = json.load(open(os.path.join(HERE, 'reaches.json')))['reaches']
    cut_at_joins(R, L, side)
    for e in R:
        if e.get('cut'):
            print('   reach %s ends at its join contact: point %s' % (e['name'], e['cut']))
    fails = []
    print('file %s (%d bytes), %d bodies' % (lodl, os.path.getsize(lodl), L['n']))
    print('A  reaches: mean angle error flow vs the upstream->downstream path tangent, bar < %.0f deg' % BAR)
    # --geo adds the geodesic-downstream reference beside each reach (slow, informative: never gated)
    _res, bad = score_reaches(R, lambda r, c: vec(L, r, c),
                              water=(lambda a_, b_, c_, d_: L['ids'][a_:b_, c_:d_] > 0) if '--geo' in sys.argv else None)
    fails += ['A %s %.1f' % (r['name'], r['mean']) for r in bad]
    print('B  zero current on every lake / swamp / calm body of the sidecar (bar: 0 moving texels)')
    still_ids = {}
    sea_ids = set()
    for bid, x, y, cls in sidecar(side):
        k = body_at(L, x, y)
        if cls in ('lake', 'swamp', 'calm') and k:
            still_ids[k] = (bid, cls)
        if cls == 'sea' and k:
            sea_ids.add(k)
    ids = L['ids']
    moving_total = 0
    moving_bodies = []
    m = np.isin(ids, np.array(sorted(still_ids), np.uint16))
    rr, cc = np.nonzero(m)
    vx, vy = vec(L, rr, cc)
    mv = np.hypot(vx, vy) > 0
    moving_total = int(mv.sum())
    if moving_total:
        u, cnt = np.unique(ids[rr[mv], cc[mv]], return_counts=True)
        moving_bodies = sorted(zip(cnt.tolist(), u.tolist()), reverse=True)
    print('  still bodies %d (%d texels): moving texels %d in %d bodies%s'
          % (len(still_ids), len(rr), moving_total, len(moving_bodies),
             (': ' + ', '.join('%d (%s, %d tx)' % (b, still_ids[b][1], c) for c, b in moving_bodies[:8]))
             if moving_bodies else ''))
    if moving_total:
        fails.append('B %d moving texels' % moving_total)
    if sea_ids:
        m = np.isin(ids, np.array(sorted(sea_ids), np.uint16))
        rr, cc = np.nonzero(m)
        vx, vy = vec(L, rr, cc)
        print('C  sea body/ies %s: %d texels, %d moving (report)' % (sorted(sea_ids), len(rr), int((np.hypot(vx, vy) > 0).sum())))
    print('RESULT %s (%d failing: %s)' % ('PASS' if not fails else 'FAIL', len(fails), '; '.join(fails[:12])
                                         + (' ...' if len(fails) > 12 else '')))
    held = bool(fails) == expect_fail
    print('EXPECT %s: %s' % ('FAIL' if expect_fail else 'PASS', 'held' if held else 'NOT HELD'))
    return 0 if held else 1


if __name__ == '__main__':
    sys.exit(main())
