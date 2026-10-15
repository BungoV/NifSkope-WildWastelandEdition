"""Lane VOLAIR1 (10-15): does the sun that comes through a room's windows and open shop fronts light the air inside?

bungo, Concord Hardware at noon: "the windows do not let any light into the interior volume". The air's sun term
multiplies each froxel by the sun's cascade-shadow factor (cell_volair.frag airSun -> wwSunShadow). This judge
predicts that factor ITSELF from the cascade map the exe drew (WW_CSM_DUMP, the D16 layers) and the receiver
matrices it uploaded (WW_CSM_ECHO), with the receiver's own law (ww_sunshadow.glsl: the pair select, the 16-tap
Poisson of bilinear compares, the seam blend, the distance fade), and compares the froxels' sun in-scatter to it.

Runs (one camera, sun term only, WW_CELL_AIR_TERMS=1, no depth skip WW_CELL_AIR_OPT=0):
  on      the build under test (alpha-tested casters keep only what their alpha test keeps)
  vis1    WW_CELL_AIR_RED=noshadow   (the air ignores the map: factor 1 everywhere)
  vis0    WW_CSM_FORCE=0             (factor 0 everywhere: what the froxel holds without the sun)
  atnone  WW_CSM_RED=atnone          (no alpha-tested caster at all: the openings' ceiling, glass gone)
  atsolid WW_CSM_RED=atsolid         (THE RED: alpha-tested glass cast solid, as before 10-15)
Per froxel S = L(run) - L(vis0), U = L(vis1) - L(vis0) (luminance): S = f * U where f is the factor
(airEnvColour is linear in the sun's J; the four sub-steps of a froxel are judged only where all four agree).

Stages
  M  the map law (on): froxels the map predicts lit have S/U ~ 1, predicted shadowed ~ 0 (90 % within 0.15), and
     the room's lit / shadow mean ratio is the transmittance-weighted ratio the map predicts (within 25 %; above
     a predicted 50, at least 37.5). Red: run vis1 judged as if it were on (the air ignoring the map) must FAIL.
  O  the openings: lit room froxels (S/U >= 0.5) in on >= 60 % of atnone's, and atnone has >= 200 (the sun does
     enter the room through its openings). Red: atsolid must FAIL O (else glass was not what blocked it: said).
Usage: volair1_window_check.py <dir> [room_t=1200] [prefix=w_]
"""
import os
import re
import sys

import numpy as np

D = sys.argv[1]
ROOM = float(sys.argv[2]) if len(sys.argv) > 2 else 1200.0
PFX = sys.argv[3] if len(sys.argv) > 3 else 'w_'
POISSON = np.array([
    (0.493393, 0.394269), (0.798547, 0.885922), (0.247322, 0.926450), (0.051454, 0.140782),
    (0.831843, 0.009552), (0.428632, 0.017151), (0.015656, 0.749779), (0.758385, 0.496170),
    (0.223487, 0.562151), (0.011628, 0.406995), (0.241462, 0.304636), (0.430311, 0.727226),
    (0.981811, 0.278359), (0.407056, 0.500534), (0.123478, 0.463546), (0.809534, 0.682272)])
LUM = np.array([0.2126, 0.7152, 0.0722])


def air(tag):
    p = os.path.join(D, PFX + tag + '.bin')
    with open(p, 'rb') as f:
        cols, rows, sl = np.frombuffer(f.read(12), np.int32)
        inj = np.frombuffer(f.read(int(cols) * int(rows) * int(sl) * 16), np.float32).reshape(sl, rows, cols, 4)
    txt = {}
    for ln in open(p + '.txt'):
        k, _, v = ln.strip().partition(' ')
        txt[k] = v
    return inj, txt


def echo(tag):
    s = open(os.path.join(D, PFX + tag + '.csm.txt')).read()
    e = {}
    for i in range(3):
        e['m%d' % i] = np.array([float(x) for x in re.search(r' m%d=(\S+)' % i, s).group(1).split(',')]).reshape(4, 4).T
        e['far%d' % i] = float(re.search(r' c%d=(\S+)' % i, s).group(1).split(',')[-1])
    e['D'] = float(re.search(r' D=(\S+)', s).group(1))
    e['split'] = [float(x) for x in re.search(r' split=(\S+)', s).group(1).split(',')]
    cam = re.search(r' cam=(\S+)', s).group(1).split('|')
    e['sc'] = float(cam[4])
    e['recv'] = [float(x) for x in re.search(r' recv=(\S+)', s).group(1).split(',')]
    e['csmat'] = int(re.search(r' csmat=(-?\d+)', s).group(1))
    e['casters'] = int(re.search(r' casters=(\d+)', s).group(1))
    e['line'] = s
    return e


def cmap(tag):
    with open(os.path.join(D, PFX + tag + '.csm'), 'rb') as f:
        m, n = np.frombuffer(f.read(8), np.int32)
        return np.frombuffer(f.read(), np.uint16).reshape(n, m, m).astype(np.float64) / 65535.0


def cmp_bilinear(layer, u, v, zref):
    """GL_LINEAR + COMPARE_REF_TO_TEXTURE, LEQUAL, border depth 1 (lit)"""
    n = layer.shape[0]
    x = u * n - 0.5
    y = v * n - 0.5
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    fx = x - x0
    fy = y - y0
    out = 0.0
    for dy, wy in ((0, 1 - fy), (1, fy)):
        for dx, wx in ((0, 1 - fx), (1, fx)):
            xi = x0 + dx
            yi = y0 + dy
            inside = (xi >= 0) & (xi < n) & (yi >= 0) & (yi < n)
            d = np.where(inside, layer[np.clip(yi, 0, n - 1), np.clip(xi, 0, n - 1)], 1.0)
            out = out + wx * wy * (np.clip(zref, 0, 1) <= d)
    return out


def factor(e, M, P):
    """ww_sunshadow.glsl wwSunShadow (no red, no force) at view points P (N x 3)"""
    offA, offB, blend, near = e['recv']
    invsc = 1.0 / e['sc']
    dv = -P[:, 2] * invsc
    a = np.where(dv < e['split'][1], 0, 1)
    bound = np.where(a == 0, e['split'][0], e['split'][1])
    x = np.clip((dv - bound) / blend, 0, 1)
    t = x * x * (3 - 2 * x)
    k = 6.0 / M.shape[1]
    H = np.c_[P, np.ones(len(P))]

    def filt(i, off, sel):
        q = H[sel] @ e['m%d' % i].T
        zr = q[:, 2] - off / (e['far%d' % i] - near)
        s = 0.0
        for p in POISSON:
            s = s + cmp_bilinear(M[i], q[:, 0] + (p[0] - 0.5) * k, q[:, 1] + (p[1] - 0.5) * k, zr)
        return s / 16.0

    sA = np.ones(len(P))
    sB = np.ones(len(P))
    for i in (0, 1):
        sel = (a == i) & (t < 1)
        if sel.any():
            sA[sel] = filt(i, offA, sel)
        sel = (a == i) & (t > 0)
        if sel.any():
            sB[sel] = filt(i + 1, offB, sel)
    sh = sA + (sB - sA) * t
    s = np.clip((P * P).sum(1) * invsc * invsc / (e['D'] ** 2), 0, 1)
    sh = 1 - (1 - sh) * (1 - s ** 4)
    return np.where(dv > e['D'] + blend, 1.0, sh)


inj = {}
for tag in ('on', 'vis1', 'vis0', 'atnone', 'atsolid'):
    inj[tag], txt = air(tag)
sl, rows, cols = inj['on'].shape[:3]
near, logr = [float(x) for x in txt['slices'].split()]
p00, p11 = [float(x) for x in txt['proj'].split()]
dscale = float(txt['fogDistScale'])
print(f'froxels {cols}x{rows}x{sl}  room t < {ROOM:g}  csm in the air: {txt.get("csm")}  terms {txt.get("terms")}')

# the froxel sub-steps exactly as the inject pass places them
jj, ii = np.meshgrid(np.arange(rows), np.arange(cols), indexing='ij')
ndc = np.stack([(ii + 0.5) / cols * 2 - 1, (jj + 0.5) / rows * 2 - 1], -1)
dirv = np.dstack([ndc[..., 0] / p00, ndc[..., 1] / p11, -np.ones((rows, cols))])
dirv /= np.linalg.norm(dirv, axis=2, keepdims=True)
layers = [k for k in range(sl) if near * np.exp(logr * k / sl) < ROOM]


def subpoints(k):
    t0 = near * np.exp(logr * k / sl)
    t1 = near * np.exp(logr * (k + 1) / sl)
    return [(t0 + (t1 - t0) * (q + 0.5) / 4) for q in range(4)], 0.5 * (t0 + t1)


def predict(tag):
    e = echo(tag)
    M = cmap(tag)
    F = np.zeros((len(layers), rows, cols, 4))
    for n, k in enumerate(layers):
        ts, _ = subpoints(k)
        for q, t in enumerate(ts):
            P = (dirv * (t / dscale)).reshape(-1, 3)
            F[n, :, :, q] = factor(e, M, P).reshape(rows, cols)
    return e, F


def lum(a):
    return (a[layers, :, :, :3] * LUM).sum(-1)


Z = lum(inj['vis0'])
U = lum(inj['vis1']) - Z
use = U > 1e-3 * max(U.max(), 1e-30)
res = {}


def stage_m(name, S, F):
    lit = use & (F.min(-1) >= 0.95)
    shd = use & (F.max(-1) <= 0.05)
    r = np.where(use, S / np.where(use, U, 1), 0)
    fmean = F.mean(-1)
    ok_lit = (np.abs(r[lit] - 1) <= 0.15).mean() if lit.any() else 0
    ok_shd = (np.abs(r[shd]) <= 0.15).mean() if shd.any() else 0
    meas = (S[lit].mean() / max(S[shd].mean(), 1e-30)) if lit.any() and shd.any() else float('nan')
    pred = ((fmean * U)[lit].mean() / max((fmean * U)[shd].mean(), 1e-30)) if lit.any() and shd.any() else float('nan')
    if not (lit.sum() >= 50 and shd.sum() >= 50):
        ratio_ok = False
    elif pred <= 50:
        ratio_ok = 0.75 <= meas / pred <= 1.0 / 0.75
    else:
        ratio_ok = meas >= 37.5
    ok = lit.sum() >= 50 and shd.sum() >= 50 and ok_lit >= 0.9 and ok_shd >= 0.9 and ratio_ok
    print(f'  M[{name}] predicted lit {int(lit.sum())} froxels (S/U within 0.15 of 1: {ok_lit:.3f}), predicted shadow '
          f'{int(shd.sum())} (within 0.15 of 0: {ok_shd:.3f}); lit/shadow mean ratio measured {meas:.3g} vs the '
          f'map\'s transmittance-weighted {pred:.3g}  {"PASS" if ok else "FAIL"}')
    return ok


def lit_count(tag):
    S = lum(inj[tag]) - Z
    return int((use & (S >= 0.5 * U)).sum())


print('== M: the air follows the sun map (run on)')
e_on, F_on = predict('on')
print(f'  on: casters {e_on["casters"]}, alpha-tested casters {e_on["csmat"]}')
res['M'] = stage_m('on', lum(inj['on']) - Z, F_on)
red_m = stage_m('red vis1 as on', U, F_on)
print('== O: the sun comes in through the openings')
n_on, n_none, n_solid = lit_count('on'), lit_count('atnone'), lit_count('atsolid')
e_solid = echo('atsolid')
print(f'  lit room froxels: on {n_on}, atnone (no alpha-tested casters) {n_none}, atsolid (glass solid) {n_solid}; '
      f'atsolid casters {e_solid["casters"]} csmat {e_solid["csmat"]}')
res['O'] = n_none >= 200 and n_on >= 0.6 * n_none
red_o = n_none >= 200 and n_solid >= 0.6 * n_none
print(f'  O: on/atnone {n_on / max(n_none, 1):.3f} (bar 0.6), atnone {n_none} (bar 200)  {"PASS" if res["O"] else "FAIL"}')
print(f'  red atsolid: {n_solid / max(n_none, 1):.3f} -> {"FAIL (as it must)" if not red_o else "PASSES: glass was not the blocker here"}')
print(f'  red M (vis1 as on): {"FAIL (as it must)" if not red_m else "PASSES: the law cannot tell"}')
reds_ok = (not red_m) and (not red_o)
v = all(res.values()) and reds_ok
print('WINDOW VERDICT ' + ('PASS' if v else 'FAIL') + '  ' + ' '.join(f'{k}={"PASS" if x else "FAIL"}' for k, x in res.items())
      + f' reds={"ok" if reds_ok else "NOT-OK"}')
sys.exit(0 if v else 1)
