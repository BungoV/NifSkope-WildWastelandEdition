#!/usr/bin/env python3
"""VOLAIR1 fog-all and sky gate (bungo 10-14): in fog, does EVERY pixel fade as the air's transmittance says, and do
the sky and the far geometry meet in the fog's colour at infinity?

  volair1_sky_check.py <dir> <prefix> [label] [box x0,y0,x1,y1]

Frames of one camera and weather (notes/volair1/hold_sky.sh shoots them; file = <dir>/<prefix>_<name>):
  on.hdr      the linear frame (WW_CELL_HDR_DUMP: int32 W, H, then RGBA float32, bottom row first), the air on
  vac.hdr     the same frame with the fog row off (no air): the scene as it is before the air
  t.png       WW_CELL_AIR_PROBE=2, the air's transmittance T to each pixel (grey; the sky too), effects and decals hidden
  geod.png    WW_LOOKDEV_FOGPROBE=0,0,9, log2(distance) / 20 in 16 bits (R high, G low); the sky (255, 255, 255)
  on.png      the final picture; nosky.png the same under the red nosky (the sky without the air)
  optional:   <red>.hdr for any red or hide variant (fxfog, fxhide, decnone, ao0): judged like on.hdr

Stage F (fog-all, no draw class exempt): every pair of neighbouring pixels (right and down) on geometry at the same
  distance (within 1%), with the same T (within 2/255) and T <= 0.6, whose scene contrast is an edge
  (|dV| >= 0.1 max(V) in luminance): the air leaves T x the scene plus the same in-scatter on both, so
  |dF| <= T |dV| (the sun's own transmittance only lowers it). A pair VIOLATES when |dF| > 1.25 T |dV| + 0.02 max(F).
  PASS when the violating share is under BAR_F of the judged pairs. Printed: the gain |dF| / (T |dV|) quantiles and
  the violators inside the box (bungo's dark marks at about 1250-1320 x 530-600 on rev1).
Stage S (the sky meets the far geometry): the final picture's median colour over the sky pixels with T <= 0.05 and
  over the geometry pixels with T <= 0.05 differ by at most 10 / 255 in every channel, and the sky/geometry
  boundary pairs (4-neighbours) with both T <= 0.1 differ by at most 16 / 255 on 95% of them.
Stage L (linear, the same at the skyline): the fog colour at infinity read on the sky side, L / (1 - T) of the
  linear frame, against the geometry side, (F - T V) / (1 - T), for every 4-neighbour skyline pair with the
  geometry's T <= 0.8: median ratio within 0.8..1.25 and 80% of the pairs within 0.67..1.5 (nosky.hdr must FAIL).
  Judged on nol.hdr (the lamps off) when the view has one; the lamps-on value is printed as information.
Stage K (the sky takes the air): the sky pixels' T is under 1 where the geometry beside them is fogged (median sky
  T at the horizon band <= 0.1) -- under the red nosky stage S must FAIL.
Reds: the fxfog frame (effects and particles keep the game's fog alone) must FAIL F; nosky must FAIL S.
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

LUM = np.array([0.2126, 0.7152, 0.0722])
BAR_F = float(os.environ.get('VOLAIR1_BAR_F', '0.002'))


def read_hdr(p):
    with open(p, 'rb') as f:
        w, h = struct.unpack('<ii', f.read(8))
        a = np.frombuffer(f.read(w * h * 16), dtype=np.float32).reshape(h, w, 4)
    return a[::-1, :, :3].astype(np.float64)


def read_png(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(np.float64)


def geod(p):
    a = read_png(p)
    sky = (a[..., 0] == 255) & (a[..., 1] == 255) & (a[..., 2] == 255)
    q = a[..., 0] * 256 + a[..., 1]
    d = np.power(2.0, q / 65535.0 * 20.0)
    d[sky] = np.inf
    return d, sky


def pairs(shape):
    h, w = shape
    ys, xs = np.mgrid[0:h, 0:w]
    out = []
    for dy, dx in ((0, 1), (1, 0)):
        a = (ys[:h - dy, :w - dx].ravel(), xs[:h - dy, :w - dx].ravel())
        b = (ys[dy:, dx:].ravel(), xs[dy:, dx:].ravel())
        out.append((a, b))
    return out


def stage_f(F, V, T, d, sky, box):
    Y_F = F @ LUM
    Y_V = V @ LUM
    res = {'judged': 0, 'viol': 0, 'gains': [], 'box_j': 0, 'box_v': 0, 'where': []}
    for a, b in pairs(T.shape):
        ok = ~sky[a] & ~sky[b] & np.isfinite(d[a]) & np.isfinite(d[b])
        with np.errstate(invalid='ignore'):
            ok &= np.abs(d[a] - d[b]) <= 0.01 * np.maximum(d[a], d[b])
        ok &= np.abs(T[a] - T[b]) <= 2.0 / 255
        t = 0.5 * (T[a] + T[b])
        ok &= t <= 0.6
        dV = np.abs(Y_V[a] - Y_V[b])
        ok &= dV >= 0.1 * np.maximum(Y_V[a], Y_V[b])
        ok &= dV > 1e-5
        dF = np.abs(Y_F[a] - Y_F[b])
        tol = 1.25 * t * dV + 0.02 * np.maximum(Y_F[a], Y_F[b])
        v = ok & (dF > tol)
        res['judged'] += int(ok.sum())
        res['viol'] += int(v.sum())
        g = dF[ok] / np.maximum(t[ok] * dV[ok], 1e-9)
        res['gains'].append(g)
        if box:
            x0, y0, x1, y1 = box
            inb = (a[1] >= x0) & (a[1] <= x1) & (a[0] >= y0) & (a[0] <= y1)
            res['box_j'] += int((ok & inb).sum())
            res['box_v'] += int((v & inb).sum())
        vi = np.nonzero(v)[0]
        for i in vi[np.argsort(-(dF[vi] - tol[vi]))][:5]:
            res['where'].append((int(a[1][i]), int(a[0][i]), float(t[i]), float(dF[i]), float(dV[i]), float(d[a][i])))
    g = np.concatenate(res['gains']) if res['gains'] else np.zeros(1)
    res['q'] = np.quantile(g, [0.5, 0.9, 0.99, 0.999]) if g.size else np.zeros(4)
    res['share'] = res['viol'] / max(res['judged'], 1)
    return res


def stage_s(P, T, sky):
    geo = ~sky
    s_far = sky & (T <= 0.05)
    g_far = geo & (T <= 0.05)
    out = {'n_sky': int(s_far.sum()), 'n_geo': int(g_far.sum())}
    if out['n_sky'] < 300 or out['n_geo'] < 300:
        out['skip'] = True
        return out
    ms = np.median(P[s_far], axis=0)
    mg = np.median(P[g_far], axis=0)
    out['ms'] = ms
    out['mg'] = mg
    out['dmax'] = float(np.max(np.abs(ms - mg)))
    nb, nok = 0, 0
    for a, b in pairs(T.shape):
        m = (sky[a] != sky[b]) & (T[a] <= 0.1) & (T[b] <= 0.1)
        diff = np.max(np.abs(P[a][m] - P[b][m]), axis=1) if m.any() else np.zeros(0)
        nb += int(m.sum())
        nok += int((diff <= 16).sum())
    out['n_edge'] = nb
    out['edge_ok'] = nok / max(nb, 1)
    out['pass'] = out['dmax'] <= 10 and (nb < 50 or out['edge_ok'] >= 0.95)
    return out


def stage_l(F, V, T, sky):
    """the fog colour at infinity, linear, on both sides of the skyline: the sky's L / (1 - T) against the
    geometry's (F - T V) / (1 - T)"""
    r = []
    Ys = F @ LUM
    Yv = V @ LUM
    for a, b in pairs(T.shape):
        for s_, g_ in ((a, b), (b, a)):
            m = sky[s_] & ~sky[g_] & (T[s_] < 0.98) & (T[g_] <= 0.8) & (T[g_] >= 0.0)
            if not m.any():
                continue
            ls = Ys[s_][m] / (1.0 - T[s_][m])
            lg = (Ys[g_][m] - T[g_][m] * Yv[g_][m]) / (1.0 - T[g_][m])
            ok = lg > 1e-6
            r.append(ls[ok] / lg[ok])
    r = np.concatenate(r) if r else np.zeros(0)
    if r.size < 50:
        return {'skip': True, 'n': int(r.size)}
    med = float(np.median(r))
    share = float(((r >= 0.67) & (r <= 1.5)).mean())
    return {'n': int(r.size), 'med': med, 'q': np.quantile(r, [0.1, 0.9]), 'share': share,
            'pass': 0.8 <= med <= 1.25 and share >= 0.8}


def main(d, pre, label='', box=None):
    f = lambda n: os.path.join(d, f'{pre}_{n}')
    lines = []
    say = lambda s: (print(s), lines.append(s))
    T = read_png(f('t.png'))[..., 0] / 255.0
    dist, sky = geod(f('geod.png'))
    say(f'== VOLAIR1 SKY/FOG-ALL {label} {pre}: {T.shape[1]}x{T.shape[0]}, sky {sky.mean() * 100:.1f}%, '
        f'T median geometry {np.median(T[~sky]):.3f} sky {np.median(T[sky]) if sky.any() else float("nan"):.3f}')
    verdict = True
    V = read_hdr(f('vac.hdr'))
    if V.shape[:2] != T.shape:
        say(f'  size mismatch: hdr {V.shape[:2]} probes {T.shape}')
        return 'FAIL (size)'
    reds = []
    for name in ('on', 'fxfog', 'fxhide', 'decnone', 'ao0'):
        if not os.path.exists(f(name + '.hdr')):
            continue
        r = stage_f(read_hdr(f(name + '.hdr')), V, T, dist, sky, box)
        ok = r['share'] < BAR_F
        q = r['q']
        say(f'  F {name:8s} judged {r["judged"]:7d} violating {r["viol"]:6d} ({r["share"] * 100:.3f}%, bar {BAR_F * 100:.2f}%)'
            f' gain q50 {q[0]:.2f} q90 {q[1]:.2f} q99 {q[2]:.2f} q99.9 {q[3]:.2f}'
            + (f' | box judged {r["box_j"]} violating {r["box_v"]}' if box else '') + f' -> {"PASS" if ok else "FAIL"}')
        for w in r['where'][:6]:
            say(f'      worst x{w[0]} y{w[1]} T {w[2]:.3f} dF {w[3]:.4f} dV {w[4]:.4f} d {w[5]:.0f}')
        if name == 'on':
            verdict &= ok
        elif name == 'fxfog':
            reds.append(('fxfog', not ok))
    # 10-15 (ruled): L is a fog/sky stage. Where the view has a lamps-off frame (nol.hdr, WW_CELL_AIR_RED=nolights)
    # L is judged on it; the lamps' in-scatter brightens the geometry side and belongs to the lamp gate. The
    # lamps-on L is still printed, as information only.
    lj = 'nol' if os.path.exists(f('nol.hdr')) else 'on'
    for name in ('on', 'nol', 'nosky'):
        if not os.path.exists(f(name + '.hdr')) or (name == 'nol' and lj != 'nol'):
            continue
        s = stage_l(read_hdr(f(name + '.hdr')), V, T, sky)
        if s.get('skip'):
            say(f'  L {name:8s} SKIP ({s["n"]} skyline pairs)')
            continue
        say(f'  L {name:8s} fog colour at infinity, sky / geometry across the skyline: median {s["med"]:.3f} '
            f'(q10 {s["q"][0]:.3f} q90 {s["q"][1]:.3f}), within 0.67..1.5 {s["share"] * 100:.1f}% of {s["n"]} pairs '
            f'(bars 0.8..1.25, 80%) -> {"PASS" if s["pass"] else "FAIL"}'
            + ('' if name == 'nosky' or name == lj else '  (information only: L is judged on the lamps-off frame)'))
        if name == lj:
            verdict &= s['pass']
        elif name == 'nosky':
            reds.append(('nosky L', not s['pass']))
    for name in ('on', 'nosky'):
        if not os.path.exists(f(name + '.png')):
            continue
        s = stage_s(read_png(f(name + '.png')), T, sky)
        if s.get('skip'):
            say(f'  S {name:8s} SKIP (sky T<=0.05 {s["n_sky"]} px, geometry {s["n_geo"]} px)')
            continue
        say(f'  S {name:8s} sky {np.round(s["ms"]).astype(int).tolist()} far geometry {np.round(s["mg"]).astype(int).tolist()}'
            f' max diff {s["dmax"]:.0f}/255 (bar 10); edge pairs {s["n_edge"]} within 16: {s["edge_ok"] * 100:.1f}% (bar 95)'
            f' -> {"PASS" if s["pass"] else "FAIL"}')
        if name == 'on':
            verdict &= s['pass']
        else:
            reds.append(('nosky S', not s['pass']))
    if sky.any():
        h = T.shape[0]
        rows = np.nonzero(sky.any(axis=1))[0]
        bottom = rows.max()
        band = sky.copy()
        band[:max(bottom - 20, 0)] = False
        k = float(np.median(T[band])) if band.any() else float('nan')
        say(f'  K sky T at its lowest 20 rows {k:.3f}, at the top row {np.median(T[rows.min()][sky[rows.min()]]):.3f}')
    for n, failed in reds:
        say(f'  RED {n}: {"fails as it must" if failed else "PASSES (the gate cannot see it)"}')
        verdict &= failed
    v = f'VOLAIR1 SKY VERDICT {label} {pre}: {"PASS" if verdict else "FAIL"}'
    say(v)
    return v


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) < 2:
        print(__doc__)
        sys.exit(2)
    box = tuple(int(x) for x in a[3].split(',')) if len(a) > 3 else None
    v = main(a[0], a[1], a[2] if len(a) > 2 else '', box)
    sys.exit(0 if 'PASS' in v else 1)
