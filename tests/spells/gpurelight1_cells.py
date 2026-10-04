#!/usr/bin/env python3
"""Lane GPURELIGHT1: the independent twin of `NifSkope -no-gui gpurelight --out <dir>` (src/proberelight.cpp).

Runs on Windows (numpy only; no resource module, no fork). Reads <dir>/twin and <dir>/records and checks:

  T1  direct light re-traced here, in double, from the scene's own triangles: for every surfel and light, the
      shadow ray through the soup, then -- for the closed doors -- through each door's REAL triangles with its
      alpha-test mask (a texel under the threshold passes) and its glass panes (tint t/255 a pane). B1 per state
      must equal the CPU relight's (b1_cpu_<state>.f32) to 1e-4 of the brightest, all but <= 0.5 % of the surfels
      (a ray grazing a triangle edge may fall either way in float vs double).
  T1 reds: the same twin against the CPU's "glassopaque" and "doorblanket" runs of the closed state must FAIL.
  R1  the shared light record (lights_X_Y.wlt + relight_X_Y.wlp, FARVIEW1b 3.6 + proberelight.h): the headers,
      sizes, offsets, hash (FNV-1a 64 over plugin table, groups, lights), pairStart monotone, every pair's surfel id
      inside its sector, the pair total = the recorder's; and B1 of the baked state rebuilt from the records ALONE
      (f16 N.L) = the CPU B1 to 2e-3 relative (what FARVIEW1 will read).
  R1 red: the same rebuild with the pairs of one light dropped must FAIL.

usage: python gpurelight1_cells.py <out dir>      exit 0 all pass, 1 a gate failed, 2 bad input
"""
import json
import os
import struct
import sys

import numpy as np

EPS_REL = 1e-4
EDGE_SHARE = 0.005


def load(d, name, dt, cols=None):
    a = np.fromfile(os.path.join(d, name), dtype=dt)
    return a.reshape(-1, cols) if cols else a


def ray_tris(o, dirs, tmax, tris):
    """any-hit per ray: (hit bool [R, T], t, u, v); Moller-Trumbore, two-sided. o, dirs: [R,3]; tris [T,9]."""
    if len(tris) == 0:
        z = np.zeros((len(o), 0))
        return z.astype(bool), z, z, z
    v0 = tris[:, 0:3][None]
    e1 = (tris[:, 3:6] - tris[:, 0:3])[None]
    e2 = (tris[:, 6:9] - tris[:, 0:3])[None]
    d = dirs[:, None, :]
    p = np.cross(d, e2)
    det = np.sum(e1 * p, axis=2)
    ok = np.abs(det) > 1e-12
    inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
    s = o[:, None, :] - v0
    u = np.sum(s * p, axis=2) * inv
    q = np.cross(s, e1)
    v = np.sum(d * q, axis=2) * inv
    t = np.sum(e2 * q, axis=2) * inv
    hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-4) & (t <= tmax[:, None])
    return hit, t, u, v


def radial(d, r, bias, scale, ex):
    x = np.clip(d / max(r, 0.001), 0.0, 1.0)
    xe = x ** ex if ex > 0 else np.ones_like(x)
    k = 1.0 - np.clip(scale * xe + bias, 0.0, 1.0)
    return k ** 2.2


def twin_b1(tw, sc, state, red=None):
    surf = load(tw, 'surf.f32', np.float32, 9).astype(np.float64)
    P, N, A = surf[:, 0:3], surf[:, 3:6], surf[:, 6:9]
    soup = load(tw, 'soup_tris.f32', np.float32, 9).astype(np.float64)
    dt = load(tw, 'door_tris.f32', np.float32, 9).astype(np.float64)
    did = load(tw, 'door_id.i32', np.int32)
    dm = load(tw, 'door_mask.f32', np.float32, 8).astype(np.float64)
    glass = load(tw, 'glass.f32', np.float32, 9).astype(np.float64) if os.path.getsize(os.path.join(tw, 'glass.f32')) else np.zeros((0, 9))
    gT = load(tw, 'glass_t.u8', np.uint8, 3).astype(np.float64) / 255.0 if len(glass) else np.zeros((0, 3))
    gdoor = load(tw, 'glass_door.i32', np.int32) if len(glass) else np.zeros(0, np.int32)
    maps = {}
    st = sc['states'][state]
    closed = [i for i, c in enumerate(st['closed']) if c]
    dsel = np.isin(did, closed)
    gsel = np.isin(gdoor, closed)
    dts, dms = dt[dsel], dm[dsel]
    gls, gTs = glass[gsel], gT[gsel]
    clear = sc['fixtureClear']
    E = np.zeros_like(P)
    for li, L in enumerate(sc['lights']):
        col = np.array(st['color'][li * 3:li * 3 + 3], dtype=np.float64)
        if not col.any():
            continue
        r = float(st['radius'][li])
        lp = np.array(L['pos'], dtype=np.float64)
        Lv = lp[None] - P
        d = np.linalg.norm(Lv, axis=1)
        Ld = Lv / np.maximum(d, 0.001)[:, None]
        nl = np.sum(N * Ld, axis=1)
        a = radial(d, r, L['bias'], L['scale'], L['exponent'])
        live = (d < r) & (nl > 0) & (a * nl > 0)
        idx = np.nonzero(live)[0]
        if len(idx) == 0:
            continue
        o = P[idx] + 2.0 * N[idx]
        seg = lp[None] - o
        ln = np.linalg.norm(seg, axis=1)
        dirs = seg / ln[:, None]
        tmax = ln - clear
        T = np.ones((len(idx), 3))
        hit, _, _, _ = ray_tris(o, dirs, tmax, soup)
        blocked = hit.any(axis=1) & (tmax > 1e-3)
        T[blocked] = 0.0
        if red != 'dooropen' and len(dts):
            hit, _, u, v = ray_tris(o, dirs, tmax, dts)
            hit &= (tmax > 1e-3)[:, None]
            # the alpha test: a hole lets the ray on
            for k in range(len(dts)):
                if dms[k, 0] < 0 or not hit[:, k].any():
                    continue
                m = int(dms[k, 0])
                if m not in maps:
                    maps[m] = load(tw, 'map%d.u8' % m, np.uint8, 64)
                mp = maps[m]
                uv = dms[k, 2:8]
                b0 = 1.0 - u[:, k] - v[:, k]
                tu = b0 * uv[0] + u[:, k] * uv[2] + v[:, k] * uv[4]
                tv = b0 * uv[1] + u[:, k] * uv[3] + v[:, k] * uv[5]
                tu -= np.floor(tu)
                tv -= np.floor(tv)
                x = np.clip((tu * mp.shape[1]).astype(int), 0, mp.shape[1] - 1)
                y = np.clip((tv * mp.shape[0]).astype(int), 0, mp.shape[0] - 1)
                hole = mp[y, x] < dms[k, 1]
                hit[:, k] &= ~hole
            solid = hit.any(axis=1)
            T[solid] = 0.0
        if red != 'dooropen' and len(gls):
            hit, _, _, _ = ray_tris(o, dirs, tmax, gls)
            hit &= (tmax > 1e-3)[:, None]
            for k in range(len(gls)):
                T[hit[:, k]] *= (0.0 if red == 'glassopaque' else gTs[k])
        E[idx] += col[None] * (a[idx] * nl[idx])[:, None] * T
    return A * E


def gate_b1(name, mine, theirs, red=False):
    m = max(np.abs(theirs).max(), 1e-30)
    bad = np.abs(mine - theirs).max(axis=1) > EPS_REL * m
    share = bad.mean()
    ok = share <= EDGE_SHARE
    passed = (not ok) if red else ok
    print('%s %s %s: %d of %d surfels off by more than %g of the brightest (%.3f %%; gate %.1f %%), worst %.3g' % (
        'OK  ' if passed else 'FAIL', 'red  ' if red else 'green', name, bad.sum(), len(bad), EPS_REL, 100 * share,
        100 * EDGE_SHARE, np.abs(mine - theirs).max() / m))
    return passed


def fnv(b, h=1469598103934665603):
    for c in b:
        h ^= c
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def read_records(rd):
    """{sector: (lights [(refkey, pos, radius, color, bias, scale, exp, group, flags)], pairs [(light, sid, d, nl)], doors)}"""
    out = {}
    for fn in sorted(os.listdir(rd)):
        if not fn.startswith('lights_') or not fn.endswith('.wlt'):
            continue
        sec = fn[7:-4]
        w = open(os.path.join(rd, fn), 'rb').read()
        p = open(os.path.join(rd, 'relight_%s.wlp' % sec), 'rb').read()
        assert w[:4] == b'WLT1' and p[:4] == b'WLP1', sec
        ver, sx, sy, nl, ng, npl, toff = struct.unpack_from('<IiiIIII', w, 4)
        h, flags = struct.unpack_from('<QI', w, 32)
        assert ver == 1 and toff == 64 + 16 * ng + 64 * nl and len(w) == toff + 64 * npl, ('wlt size', sec)
        groups = w[64:64 + 16 * ng]
        lights = w[64 + 16 * ng:toff]
        table = w[toff:]
        assert fnv(lights, fnv(groups, fnv(table))) == h, ('hash', sec)
        L = []
        for i in range(nl):
            rk, x, y, z, rad, cr, cg, cb, bias, scale, ex, octd, cosO, fall, gi, fl, dot = struct.unpack_from(
                '<Q10fIeeHHI', lights, 64 * i)
            L.append(dict(ref=rk, pos=(x, y, z), radius=rad, color=(cr, cg, cb), bias=bias, scale=scale, exp=ex, group=gi,
                          flags=fl))
        for g in range(ng):
            key, first, cnt, kind, on, _ = struct.unpack_from('<QHHBBH', groups, 16 * g)
            assert kind == key >> 62 and all(L[first + k]['group'] == g for k in range(cnt)), ('group', sec, g)
        pver, psx, psy, pnl, npair = struct.unpack_from('<IiiII', p, 4)
        ph, nsurf, pflags, ndoor = struct.unpack_from('<QIII', p, 24)
        assert pver == 1 and (psx, psy) == (sx, sy) and pnl == nl and ph == h, ('wlp header', sec)
        o = 64
        start = struct.unpack_from('<%dI' % (nl + 1), p, o)
        o += 4 * (nl + 1)
        assert start[0] == 0 and start[-1] == npair and all(start[i] <= start[i + 1] for i in range(nl)), ('pairStart', sec)
        assert len(p) == o + 24 * npair + 4 * ndoor, ('wlp size', sec)
        pairs = []
        for k in range(npair):
            sid, d, nlv, dl, d0, d1 = struct.unpack_from('<IfeeHH', p, o + 24 * k)
            T = np.frombuffer(p[o + 24 * k + 16:o + 24 * k + 22], dtype=np.uint8).reshape(2, 3) / 255.0
            li = next(i for i in range(nl) if start[i] <= k < start[i + 1])
            pairs.append((li, sid, d, float(nlv), (d0, d1), T))
        out[sec] = (L, pairs, ndoor, (sx, sy), nsurf)
    return out


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    tw = os.path.join(root, 'twin')
    sc = json.load(open(os.path.join(tw, 'scene.json')))
    ok = True
    for state in sc['states']:
        cpu = load(tw, 'b1_cpu_%s.f32' % state, np.float32, 3).astype(np.float64)
        ok &= gate_b1('T1 %s' % state, twin_b1(tw, sc, state), cpu)
    twc = twin_b1(tw, sc, 'doorsclosed')
    for red in ('glassopaque', 'doorblanket'):
        cpu = load(tw, 'b1_cpu_doorsclosed_%s.f32' % red, np.float32, 3).astype(np.float64)
        ok &= gate_b1('T1 red %s' % red, twc, cpu, red=True)
    # R1: the records
    recs = read_records(os.path.join(root, 'records'))
    total = sum(len(v[1]) for v in recs.values())
    r_ok = total == sc['pairs']
    print('%s green R1 records: %d sectors, %d pairs (recorder %d), headers, sizes, hashes and groups consistent' % (
        'OK  ' if r_ok else 'FAIL', len(recs), total, sc['pairs']))
    ok &= r_ok
    sid = load(tw, 'sid.i32', np.int32, 2)
    files = sc['files']
    surf = load(tw, 'surf.f32', np.float32, 9).astype(np.float64)
    import re
    secOf = {}
    for f, name in enumerate(files):
        m = re.search(r'sector_([+-]?\d+)_([+-]?\d+)\.tbk$', name)
        if m:
            secOf[(int(m.group(1)), int(m.group(2)))] = f
    key = {}
    for i, (f, s) in enumerate(sid):
        key[(int(f), int(s))] = i
    def rebuild(state, drop=None):
        st = sc['states'][state]
        closed = st['closed']
        E = np.zeros((len(surf), 3))
        for sec, (L, pairs, _, (sx, sy), _) in recs.items():
            fi = secOf[(sx, sy)]
            for li, s, d, nl, doors, T in pairs:
                lt = L[li]
                if drop is not None and lt['ref'] == drop:
                    continue
                # the live color of this light: the recorder's light with the same position
                k = next(j for j, l2 in enumerate(sc['lights']) if tuple(np.float32(l2['pos'])) == tuple(np.float32(lt['pos'])))
                col = np.array(st['color'][3 * k:3 * k + 3])
                if not col.any() or d >= lt['radius']:
                    continue
                a = radial(np.array([d]), lt['radius'], lt['bias'], lt['scale'], lt['exp'])[0]
                if a * nl <= 0:
                    continue
                t = np.ones(3)
                for k2 in range(2):
                    if doors[k2] != 0xFFFF and closed[doors[k2]]:
                        t = t * T[k2]
                E[key[(fi, s)]] += col * a * nl * t
        return surf[:, 6:9] * E

    def rel(b, state):
        cpu = load(tw, 'b1_cpu_%s.f32' % state, np.float32, 3).astype(np.float64)
        return np.abs(b - cpu).max() / max(np.abs(cpu).max(), 1e-30)

    for state in ('baked', 'doorsclosed'):
        d0 = rel(rebuild(state), state)
        g = d0 <= 2e-3
        print('%s green R1 rebuild %s: B1 from the records alone vs the CPU relight %.3g (gate 2e-3; f16 N.L, u8 T)' % (
            'OK  ' if g else 'FAIL', state, d0))
        ok &= g
    first = next(iter(recs.values()))[0][0]['ref']
    d1 = rel(rebuild('baked', drop=first), 'baked')
    g = not (d1 <= 2e-3)
    print('%s red   R1 rebuild without one light: %.3g' % ('OK  ' if g else 'FAIL', d1))
    ok &= g
    d2 = rel(rebuild('baked'), 'doorsclosed')
    g = not (d2 <= 2e-3)
    print('%s red   R1 doors closed rebuilt with the door bytes ignored: %.3g' % ('OK  ' if g else 'FAIL', d2))
    ok &= g
    print('gpurelight1_cells: %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
