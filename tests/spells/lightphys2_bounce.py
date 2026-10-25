#!/usr/bin/env python3
"""Lane LIGHTPHYS2: the light physics rows in the bounce light, checked against an independent rebuild.

Reads <out>/twin from `NifSkope -no-gui gpurelight --out <out>` (src/proberelight.cpp). For every state whose
"phys" entry is not the game's curve (mode 0, curve 1), it re-traces the direct light B1 here in numpy (double):
the shadow ray through the soup triangles, the falloff and the intensity curve transcribed from the FO4CS
formulas written out below (NOT from src/gl/lightphys.h), and compares with the CPU relight's b1_cpu_<state>.f32.

  L1 <state>   twin = CPU to 1e-4 of the brightest, all but <= 0.5 % of the surfels (edge-grazing rays)
  L1 red       the twin on the game's curve against the CPU's physical state must FAIL
  L2 calib     the twin's advanced B1 at calibration 2 = 2 x at calibration 1 to 1e-7 (scene.json prints the
               numerator to 9 digits) and the CPU's does the same to 1e-6

Doors are open in these states (no door or glass triangles).

Lane LIGHTPHYS4 adds twin/lp4.json (the far light's all-on B1 per rows, a solid door shut under the rows):
  F/D3 <state> twin = CPU as L1 (a shut solid door's triangles block the twin's rays)
  F red farcurve   the twin's physical far light against the CPU's game far light must FAIL
  D3 red dooropen  the door state re-traced with the door open must FAIL
  D3 red doorrows  the twin's physical door against the CPU's door on the game's curve must FAIL

usage: python lightphys2_bounce.py <out dir>
exit 0 all pass, 1 a gate failed, 2 bad input
"""
import json
import math
import os
import sys

import numpy as np

EPS_REL = 1e-4
EDGE_SHARE = 0.005
UNITS_PER_METRE = 69.99   # FO4CS shader: 1 / 69.99 m a unit
WINDOW_START = 0.8


def load(d, name, dt, cols=None):
    a = np.fromfile(os.path.join(d, name), dtype=dt)
    return a.reshape(-1, cols) if cols else a


def blocked(o, dirs, tmax, tris):
    """any hit along each ray before tmax (Moller-Trumbore, two-sided), in chunks of triangles."""
    out = np.zeros(len(o), bool)
    for s in range(0, len(tris), 256):
        t9 = tris[s:s + 256]
        v0 = t9[:, 0:3][None]
        e1 = (t9[:, 3:6] - t9[:, 0:3])[None]
        e2 = (t9[:, 6:9] - t9[:, 0:3])[None]
        d = dirs[:, None, :]
        p = np.cross(d, e2)
        det = np.sum(e1 * p, axis=2)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        sv = o[:, None, :] - v0
        u = np.sum(sv * p, axis=2) * inv
        q = np.cross(sv, e1)
        v = np.sum(d * q, axis=2) * inv
        t = np.sum(e2 * q, axis=2) * inv
        out |= (ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-4) & (t <= tmax[:, None])).any(axis=1)
    return out & (tmax > 1e-3)


def game(x, b, s, e):
    xe = x ** e if e > 0 else np.ones_like(x)
    return (1.0 - np.clip(s * xe + b, 0.0, 1.0)) ** 2.2


def bulb(x, src):
    """FO4CS physical bulb: windowed (1 - x^4)^2 over x^2 + s^2."""
    w = np.clip(1.0 - x ** 4, 0.0, 1.0)
    return w * w / (x * x + src * src)


def curve_is_falloff(anchor, b, s, e):
    one = lambda v: float(game(np.array([v]), b, s, e)[0])
    if not e > 0 or not min(max(s + b, 0), 1) > min(max(b, 0), 1):
        return False
    if one(1.0) >= 1.0 / 255.0:
        return False
    return one(anchor) <= (1.0 - anchor * anchor) ** 1.1


def term(ph, x, r, b, s, e):
    """the radial term as the state's rows want it (None = the game's curve)."""
    mode = ph['mode']
    if mode == 0 or not curve_is_falloff(ph['anchor'], b, s, e):
        return None
    if mode == 2:   # advanced: N x window / max(x0, x)^2 over its own integral, N = flux x calibration
        near = max(max(ph['advMin'], 0.001), max(0.1 * r / UNITS_PER_METRE, 0.2))
        x0 = min(max(near * UNITS_PER_METRE / r, 1e-4), 0.5) if r > 0 else 0.5
        den = 0.5 * (1.0 + WINDOW_START) - (2.0 / 3.0) * x0
        t = np.clip((x - WINDOW_START) / (1.0 - WINDOW_START), 0.0, 1.0)
        win = np.ones_like(x) if ph['red'] & 2 else 1.0 - t * t * (3.0 - 2.0 * t)
        return ph['adv'] / den * win / np.maximum(x0, x) ** 2 if den > 0 else np.zeros_like(x)
    a = ph['anchor']
    k = 1.0 if ph['red'] & 1 else float(game(np.array([a]), b, s, e)[0]) / float(bulb(np.array([a]), ph['s'])[0])
    if not k > 0:
        return None
    return k * bulb(x, ph['s'])


def scale_of(ph, col):
    if ph['curve'] == 1.0:
        return 1.0
    lum = 0.2126 * max(col[0], 0) + 0.7152 * max(col[1], 0) + 0.0722 * max(col[2], 0)
    piv = 1.0 if ph['red'] & 8 else ph['pivot']
    return (lum / piv) ** (ph['curve'] - 1.0) if lum > 0 and piv > 0 else 1.0


def twin_b1(tw, sc, state, ph, extra=None):
    surf = load(tw, 'surf.f32', np.float32, 9).astype(np.float64)
    P, N, A = surf[:, 0:3], surf[:, 3:6], surf[:, 6:9]
    soup = load(tw, 'soup_tris.f32', np.float32, 9).astype(np.float64)
    if extra is not None:
        soup = np.vstack([soup, extra])
    st = sc['states'][state]
    E = np.zeros_like(P)
    for li, L in enumerate(sc['lights']):
        col = np.array(st['color'][li * 3:li * 3 + 3], dtype=np.float64)
        if not col.any():
            continue
        r = float(st['radius'][li])
        lp = np.array(L['pos'], dtype=np.float64)
        Lv = lp[None] - P
        d = np.linalg.norm(Lv, axis=1)
        nl = np.sum(N * (Lv / np.maximum(d, 0.001)[:, None]), axis=1)
        x = np.clip(d / max(r, 0.001), 0.0, 1.0)
        a = term(ph, x, r, L['bias'], L['scale'], L['exponent'])
        if a is None:
            a = game(x, L['bias'], L['scale'], L['exponent'])
        idx = np.nonzero((d < r) & (nl > 0) & (a * nl > 0))[0]
        if len(idx) == 0:
            continue
        o = P[idx] + 2.0 * N[idx]
        seg = lp[None] - o
        ln = np.linalg.norm(seg, axis=1)
        T = (~blocked(o, seg / ln[:, None], ln - sc['fixtureClear'], soup)).astype(np.float64)
        E[idx] += (col * scale_of(ph, col))[None] * (a[idx] * nl[idx] * T)[:, None]
    return A * E


def gate(name, mine, theirs, red=False):
    m = max(np.abs(theirs).max(), 1e-30)
    bad = np.abs(mine - theirs).max(axis=1) > EPS_REL * m
    ok = bad.mean() <= EDGE_SHARE
    passed = (not ok) if red else ok
    print('%s %s %s: %d of %d surfels off by more than %g of the brightest (%.3f %%), worst %.3g' % (
        'OK  ' if passed else 'FAIL', 'red  ' if red else 'green', name, bad.sum(), len(bad), EPS_REL,
        100 * bad.mean(), np.abs(mine - theirs).max() / m))
    return passed


def lp4(tw, sc):
    # lane LIGHTPHYS4: the far light and the live door under the rows
    p = os.path.join(tw, 'lp4.json')
    if not os.path.exists(p):
        print('FAIL: the dump has no lp4.json (an exe before lane LIGHTPHYS4?)')
        return False
    s4 = dict(sc, states=json.load(open(p))['states'])
    door = load(tw, 'door_tris.f32', np.float32, 9).astype(np.float64)
    did = load(tw, 'door_id.i32', np.int32)
    cpu = lambda s: load(tw, 'b1_lp4_%s.f32' % s, np.float32, 3).astype(np.float64)
    need = ('fargame', 'farphysical', 'faradvanced', 'doorphys', 'doorgame')
    if any(s not in s4['states'] for s in need):
        print('FAIL: lp4.json lacks a state of %s' % (need,))
        return False
    ok, tw4 = True, {}
    for state, st in s4['states'].items():
        shut = [k for k, c in enumerate(st['closed']) if c]
        tw4[state] = twin_b1(tw, s4, state, st['phys'], door[np.isin(did, shut)] if shut else None)
        ok &= gate('%s %s' % ('F' if state.startswith('far') else 'D3', state), tw4[state], cpu(state))
    ok &= gate('F red farcurve', tw4['farphysical'], cpu('fargame'), red=True)
    ph = s4['states']['doorphys']['phys']
    ok &= gate('D3 red dooropen', twin_b1(tw, s4, 'doorphys', ph), cpu('doorphys'), red=True)
    ok &= gate('D3 red doorrows', tw4['doorphys'], cpu('doorgame'), red=True)
    return ok


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    tw = os.path.join(sys.argv[1], 'twin')
    sc = json.load(open(os.path.join(tw, 'scene.json')))
    ok, n, twins = True, 0, {}
    for state, st in sc['states'].items():
        ph = st.get('phys')
        if not ph or (ph['mode'] == 0 and ph['curve'] == 1.0) or any(st['closed']):
            continue
        cpu = load(tw, 'b1_cpu_%s.f32' % state, np.float32, 3).astype(np.float64)
        twins[state] = twin_b1(tw, sc, state, ph)
        ok &= gate('L1 %s' % state, twins[state], cpu)
        n += 1
    if 'physical' not in twins or 'advanced' not in twins or 'advcalib2' not in twins:
        print('FAIL: the dump has no physical / advanced / advcalib2 state (an exe before lane LIGHTPHYS2?)')
        return 2
    van = dict(sc['states']['physical']['phys'], mode=0)
    cpu = load(tw, 'b1_cpu_physical.f32', np.float32, 3).astype(np.float64)
    ok &= gate('L1 red gamecurve', twin_b1(tw, sc, 'physical', van), cpu, red=True)
    d_twin = np.abs(twins['advcalib2'] - 2.0 * twins['advanced']).max() / max(np.abs(twins['advcalib2']).max(), 1e-30)
    c1 = load(tw, 'b1_cpu_advanced.f32', np.float32, 3).astype(np.float64)
    c2 = load(tw, 'b1_cpu_advcalib2.f32', np.float32, 3).astype(np.float64)
    d_cpu = np.abs(c2 - 2.0 * c1).max() / max(np.abs(c2).max(), 1e-30)
    k = d_twin <= 1e-7 and d_cpu <= 1e-6 and np.abs(c2).max() > 0
    print('%s green L2 calib: calibration 2 / 1 = 2 (twin %.3g, CPU %.3g of the brightest)' % ('OK  ' if k else 'FAIL', d_twin, d_cpu))
    ok &= k
    ok &= lp4(tw, sc)
    print('%d states re-traced; %s' % (n, 'PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
