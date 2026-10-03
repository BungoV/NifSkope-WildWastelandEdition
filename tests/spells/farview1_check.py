#!/usr/bin/env python3
"""FARVIEW1 twin: distant light from the surfels (a light LOD). docs/cloud/FARVIEW1_DESIGN.md.

  python3 tests/spells/farview1_check.py            # green + every red; exit 0 only when green passes
                                                    # every check AND every red fails the check it targets
  FARVIEW_RED=<name> python3 tests/spells/farview1_check.py   # one red alone: prints its rows, exit 1 when it fails
  FARVIEW_QUICK=1 ...                               # fewer reference points (a smoke run, not the gate)

What it rebuilds, in numpy only, with no game data (the scene is made here, in code):
  * a synthetic street: ground plane + two rows of box buildings with alleys, 50 or 500 point lights
    (street lamps and wall lights), each with the PRTP2 curve of docs/PRTP2_LIGHT_MODEL.md section 1:
        x = saturate(d / r),  atten = pow(1 - saturate(scale * x^exponent + bias), 2.2),  E += c atten max(N.L, 0)
    behind a shadow segment from P + 2N to the light, stopping 24 units short (src/probegi.h fixtureClear)
  * THE REFERENCE at sample points: direct from every light + one diffuse bounce, brute force
    (256 cosine rays a point, each hit relit by every light) -- what the renderer would show if it
    could evaluate every light everywhere
  * THE BAKE: 70-unit surfels keyed by (cell, side), as the .tbk files key them; each lit by every
    light (a second, separately written light loop) plus one bounce gathered from the other surfels
  * THE FAR STAND-IN, three candidates measured side by side: the nearest surfels (the chosen one: each lit
    surfel's total irradiance, read by a fixed 27-cell table probe); a six-axis voxel grid splatted from the
    surfels (64 / 128 / 256 u); a probe grid (irradiance volume, 256 u)
  * THE BLEND: near = real lights (+ near GI), far = the stand-in, w = smoothstep(D0, D1, camera distance)

Checks (each printed PASS/FAIL, with what it measured):
  C0 curve     the doc curve at known x, and the two light evaluators agree on the same points
  A  band      in the band the stand-in agrees with the reference (median / p90 relative error, energy ratio)
  B  far glow  past the band (no real lights) lit points keep their light (sum ratio, share kept >= 50%)
  C  no step   sweeping the camera through the band, no point changes by more than STEP_BAR per 32-unit step (p99)
  D  flat cost the far lookup's work and time do not grow with the light count (50 vs 500)
  E  bounce    points lit mostly by the bounce (direct shadowed) agree too: the far term carries the bounce
Reds (each must FAIL its target):
  nosurfel   the surfel term off: far points get no placed light              -> A, B
  noblend    a hard switch at the band middle instead of the blend           -> C
  perlight   the far term evaluates the lights instead of reading the bake   -> D
  nobounce   the far grid built from direct light only                       -> E
"""
import math
import os
import sys
import time

import numpy as np

# ---------------------------------------------------------------- constants (pre-registered bars)
SURFEL = 70.0          # .tbk surfel cell (docs/PRTP_PLAN.md 2g)
SURF_OFF = 2.0         # shadow segment starts this far off the surface (probegi)
CLEAR = 24.0           # fixtureClear: the last units before the light are not tested
VOX = 128.0            # the voxel-grid candidate's voxel
PROBE = 256.0          # the probe-grid candidate's spacing
D0, D1 = 3072.0, 7168.0   # the blend band, camera distance (design doc section 4): one cell wide
STEP = 32.0            # camera sweep step for check C
LUM = np.array([0.2126, 0.7152, 0.0722])

# Bars: set once from the measured runs (500-point smoke, then 1600 points) at about 1.8x what the chosen far
# term measured, and kept above the reference's own noise (printed: a second bounce seed). Not tuned per red.
BAR_A_MED, BAR_A_P90 = 0.08, 0.25      # band agreement, relative error on lit points
BAR_ENERGY = (0.90, 1.10)              # sum(F) / sum(R) on lit points
BAR_B_SUM, BAR_B_KEPT = 0.85, 0.90     # far: sum ratio, share of lit points keeping >= 50 %
STEP_BAR = 0.02                        # check C: per-step change, relative, p99 (Weber rule of thumb)
BAR_D_OPS, BAR_D_TIME = 1.05, 1.5      # check D: 500-light / 50-light ratios
BAR_E_MED, BAR_E_P90 = 0.15, 0.35      # bounce-dominated points
E_MIN_POINTS = 30                      # check E is vacuous below this many bounce-dominated points

QUICK = os.environ.get('FARVIEW_QUICK') == '1'
N_REF = 500 if QUICK else 1600         # reference sample points
K_REF = 256                            # bounce rays a reference point
K_SURF = 64                            # bounce rays a surfel
K_PROBE = 128

L_X, W_Y, STREET = 8192.0, 1600.0, 400.0
BOUNDS_LO = np.array([0.0, -W_Y, -64.0])
BOUNDS_HI = np.array([L_X, W_Y, 1664.0])
AXES = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)


def lum(x):
    return x @ LUM


# ---------------------------------------------------------------- the scene, made in code
def make_street(seed=7):
    rng = np.random.default_rng(seed)
    boxes, alb = [], []
    for sgn in (1, -1):
        x = 40.0
        while x < L_X - 400:
            w = rng.uniform(300, 800)
            x1 = min(x + w, L_X - 40)
            dep = rng.uniform(400, 1000)
            h = rng.uniform(300, 1400)
            y0, y1 = (STREET + 40, STREET + 40 + dep) if sgn > 0 else (-STREET - 40 - dep, -STREET - 40)
            boxes.append([x, y0, 0.0, x1, y1, h])
            alb.append(rng.uniform(0.25, 0.6) * np.array([1.0, rng.uniform(0.85, 1.0), rng.uniform(0.7, 1.0)]))
            x = x1 + rng.uniform(60, 300)
    B = np.array(boxes)
    return dict(lo=B[:, :3].copy(), hi=B[:, 3:].copy(), alb=np.array(alb))


def ground_albedo(p):
    chk = (np.floor(p[:, 0] / 512) + np.floor(p[:, 1] / 512)).astype(int) & 1
    a = 0.18 + 0.08 * chk
    return np.stack([a, a * 0.95, a * 0.85], 1)


def make_lights(scene, n, seed):
    rng = np.random.default_rng(seed)
    pos, rad, col, bse = [], [], [], []
    nb = len(scene['lo'])
    for i in range(n):
        if i % 2 == 0:   # street lamp head
            pos.append([rng.uniform(0, L_X), rng.choice([-1, 1]) * 360.0, 350.0])
            rad.append(rng.uniform(500, 1000))
        else:            # wall light on a street face, 40 units out
            b = rng.integers(nb)
            lo, hi = scene['lo'][b], scene['hi'][b]
            fy = lo[1] - 40 if lo[1] > 0 else hi[1] + 40
            pos.append([rng.uniform(lo[0] + 20, hi[0] - 20), fy, rng.uniform(150, max(160, hi[2] - 80))])
            rad.append(rng.uniform(300, 700))
        c = np.array([1.0, rng.uniform(0.75, 0.9), rng.uniform(0.4, 0.7)]) * rng.uniform(1.0, 2.0)
        col.append(c ** 2.2)          # record color -> shader color: pow(x, 2.2) (PRTP2 section 0)
        u = rng.random()
        bse.append([0.0, 1.0, 2.0] if u < 0.8 else ([0.0, 1.0, 1.0] if u < 0.9 else [0.0, 1.0, 4.0]))
    return dict(pos=np.array(pos), r=np.array(rad), c=np.array(col), bse=np.array(bse))


def curve(x, bse):
    """PRTP2 section 1, exactly: x = saturate(d / r) already; exponent 0 = no distance term."""
    bias, scale, ex = bse[..., 0], bse[..., 1], bse[..., 2]
    xe = np.where(ex > 0, np.power(x, np.where(ex > 0, ex, 1.0)), 1.0)
    return np.power(1.0 - np.clip(scale * xe + bias, 0.0, 1.0), 2.2)


# ---------------------------------------------------------------- faces, surfels, sample points
def faces_of(scene):
    """(axis, side, plane coord, (a0, a1) range on tangent u, (b0, b1) on tangent v, albedo or None, box)"""
    F = [(2, 4, 0.0, (0.0, L_X), (-W_Y, W_Y), None, -1)]   # the ground, +Z
    for b, (lo, hi) in enumerate(zip(scene['lo'], scene['hi'])):
        a = scene['alb'][b]
        F += [(0, 0, hi[0], (lo[1], hi[1]), (lo[2], hi[2]), a, b), (0, 1, lo[0], (lo[1], hi[1]), (lo[2], hi[2]), a, b),
              (1, 2, hi[1], (lo[0], hi[0]), (lo[2], hi[2]), a, b), (1, 3, lo[1], (lo[0], hi[0]), (lo[2], hi[2]), a, b),
              (2, 4, hi[2], (lo[0], hi[0]), (lo[1], hi[1]), a, b)]
    return F


TAN = {0: (1, 2), 1: (0, 2), 2: (0, 1)}


def under_box(scene, p, margin=0.0):
    lo, hi = scene['lo'], scene['hi']
    return np.any((p[:, None, 0] > lo[None, :, 0] - margin) & (p[:, None, 0] < hi[None, :, 0] + margin) &
                  (p[:, None, 1] > lo[None, :, 1] - margin) & (p[:, None, 1] < hi[None, :, 1] + margin), 1)


def make_surfels(scene):
    P, N, A, S, AR = [], [], [], [], []
    for ax, side, c, (u0, u1), (v0, v1), alb, _ in faces_of(scene):
        tu, tv = TAN[ax]
        us = np.arange(math.floor(u0 / SURFEL), math.ceil(u1 / SURFEL))
        vs = np.arange(math.floor(v0 / SURFEL), math.ceil(v1 / SURFEL))
        gu, gv = np.meshgrid(us, vs, indexing='ij')
        a0 = np.maximum(gu.ravel() * SURFEL, u0); a1 = np.minimum((gu.ravel() + 1) * SURFEL, u1)
        b0 = np.maximum(gv.ravel() * SURFEL, v0); b1 = np.minimum((gv.ravel() + 1) * SURFEL, v1)
        ok = (a1 > a0 + 1e-6) & (b1 > b0 + 1e-6)
        p = np.zeros((ok.sum(), 3))
        p[:, ax] = c
        p[:, tu] = (a0[ok] + a1[ok]) / 2
        p[:, tv] = (b0[ok] + b1[ok]) / 2
        if ax == 2 and alb is None:
            # a ground patch a footprint cuts keeps the part outside it: 8 x 8 sub-samples, their mean and share
            A0, A1, B0, B1 = a0[ok], a1[ok], b0[ok], b1[ok]
            f = (np.arange(8) + 0.5) / 8
            su = A0[:, None, None] + (A1 - A0)[:, None, None] * f[None, :, None]
            sv = B0[:, None, None] + (B1 - B0)[:, None, None] * f[None, None, :]
            sub = np.stack([su + 0 * sv, sv + 0 * su, np.zeros_like(su + sv)], -1).reshape(-1, 3)
            out = ~under_box(scene, sub).reshape(len(A0), 64)
            keep = out.any(1)
            cnt = out.sum(1)
            subr = sub.reshape(len(A0), 64, 3)
            p = (subr * out[..., None]).sum(1)[keep] / cnt[keep, None]
            area = ((A1 - A0) * (B1 - B0) * cnt / 64.0)[keep]
            al = ground_albedo(p)
        else:
            area = ((a1 - a0) * (b1 - b0))[ok]
            al = np.repeat(alb[None], len(p), 0)
        P.append(p); N.append(np.repeat(AXES[side][None], len(p), 0)); A.append(al)
        S.append(np.full(len(p), side)); AR.append(area)
    P, N, A, S, AR = map(np.concatenate, (P, N, A, S, AR))
    S = S.astype(int)
    # one surfel a (cell, side), as the .tbk keys them: patches of two faces in one cell merge, area-weighted
    k = np.floor((P - 0.5 * N) / SURFEL).astype(np.int64)
    key = ((k[:, 0] + 4096) * 8192 + (k[:, 1] + 4096)) * 8192 * 8 + (k[:, 2] + 4096) * 8 + S
    u, inv = np.unique(key, return_inverse=True)
    wsum = np.bincount(inv, AR)
    mp = np.stack([np.bincount(inv, AR * P[:, i]) for i in range(3)], 1) / wsum[:, None]
    ma = np.stack([np.bincount(inv, AR * A[:, i]) for i in range(3)], 1) / wsum[:, None]
    ms = np.zeros(len(u), int); ms[inv] = S
    return dict(p=mp, n=AXES[ms], alb=ma, side=ms, area=wsum, merged=len(P) - len(u))


def sample_points(scene, n, seed):
    rng = np.random.default_rng(seed)
    F = faces_of(scene)
    areas = np.array([(f[3][1] - f[3][0]) * (f[4][1] - f[4][0]) for f in F])
    areas[0] *= 0.6   # the ground is mostly empty wasteland at the far ends; keep facades well sampled
    P, N, A = [], [], []
    while sum(len(p) for p in P) < n:
        m = n
        fi = rng.choice(len(F), m, p=areas / areas.sum())
        for i in np.unique(fi):
            ax, side, c, (u0, u1), (v0, v1), alb, _ = F[i]
            k = int((fi == i).sum())
            tu, tv = TAN[ax]
            p = np.zeros((k, 3)); p[:, ax] = c
            p[:, tu] = rng.uniform(u0, u1, k); p[:, tv] = rng.uniform(v0, v1, k)
            if alb is None:
                p = p[~under_box(scene, p, 1.0)]
                al = ground_albedo(p)
            else:
                al = np.repeat(alb[None], len(p), 0)
            P.append(p); N.append(np.repeat(AXES[side][None], len(p), 0)); A.append(al)
    P, N, A = (np.concatenate(x)[:n] for x in (P, N, A))
    return P, N, A


# ---------------------------------------------------------------- the tracer (shared geometry)
def trace(scene, O, D):
    """Nearest hit: t (inf = escaped), hit side (0..5), albedo rgb. Boxes by slabs, the ground plane z = 0."""
    lo, hi = scene['lo'], scene['hi']
    M = len(O)
    T = np.full(M, np.inf); side = np.full(M, -1); alb = np.zeros((M, 3))
    CH = 8192
    with np.errstate(divide='ignore', invalid='ignore'):
        for s in range(0, M, CH):
            o, d = O[s:s + CH], D[s:s + CH]
            inv = 1.0 / np.where(np.abs(d) < 1e-12, 1e-12, d)
            t0 = (lo[None] - o[:, None]) * inv[:, None]
            t1 = (hi[None] - o[:, None]) * inv[:, None]
            tn = np.minimum(t0, t1); tf = np.maximum(t0, t1)
            tmin = tn.max(2); tmax = tf.min(2)
            hit = (tmax >= tmin) & (tmin > 1e-4)
            tb = np.where(hit, tmin, np.inf)
            bi = tb.argmin(1)
            tbest = tb[np.arange(len(o)), bi]
            ax = tn[np.arange(len(o)), bi].argmax(1)
            sd = ax * 2 + (d[np.arange(len(o)), ax] > 0)
            tg = np.where(d[:, 2] < -1e-9, -o[:, 2] / d[:, 2], np.inf)
            gp = o + d * np.where(np.isfinite(tg), tg, 0)[:, None]
            tg = np.where((gp[:, 0] >= 0) & (gp[:, 0] <= L_X) & (np.abs(gp[:, 1]) <= W_Y) & (tg > 1e-4), tg, np.inf)
            useg = tg < tbest
            t = np.where(useg, tg, tbest)
            T[s:s + CH] = t
            side[s:s + CH] = np.where(np.isfinite(t), np.where(useg, 4, sd), -1)
            a = scene['alb'][bi]
            a = np.where(useg[:, None], ground_albedo(gp), a)
            alb[s:s + CH] = np.where(np.isfinite(t)[:, None], a, 0)
    return T, side, alb


# ---------------------------------------------------------------- light evaluator 1: the reference (pairs x boxes)
COUNT = {'pairs': 0}


def blocked_slab(scene, o, q):
    """segments o -> q, blocked by a box in (1e-4, |q - o| - CLEAR]"""
    d = q - o
    L = np.linalg.norm(d, axis=1)
    d = d / L[:, None]
    tmax_seg = L - CLEAR
    out = np.zeros(len(o), bool)
    CH = 8192
    with np.errstate(divide='ignore', invalid='ignore'):
        for s in range(0, len(o), CH):
            oo, dd = o[s:s + CH], d[s:s + CH]
            inv = 1.0 / np.where(np.abs(dd) < 1e-12, 1e-12, dd)
            t0 = (scene['lo'][None] - oo[:, None]) * inv[:, None]
            t1 = (scene['hi'][None] - oo[:, None]) * inv[:, None]
            tmin = np.minimum(t0, t1).max(2); tmax = np.maximum(t0, t1).min(2)
            hit = (tmax >= tmin) & (tmax > 1e-4) & (tmin <= tmax_seg[s:s + CH, None])
            out[s:s + CH] = hit.any(1)
    return out & (tmax_seg > 1e-3)


def direct_ref(scene, lights, P, N, count=True):
    E = np.zeros((len(P), 3))
    CH = max(1, 400000 // max(1, len(lights['r'])))
    for s in range(0, len(P), CH):
        p, n = P[s:s + CH], N[s:s + CH]
        v = lights['pos'][None] - p[:, None]
        d = np.linalg.norm(v, axis=2)
        if count:
            COUNT['pairs'] += d.size
        pi_, li = np.nonzero(d < lights['r'][None])
        dd = d[pi_, li]
        l = v[pi_, li] / np.maximum(dd, 1e-3)[:, None]
        ndl = np.einsum('ij,ij->i', n[pi_], l)
        a = curve(np.clip(dd / lights['r'][li], 0, 1), lights['bse'][li]) * np.maximum(ndl, 0)
        k = a > 0
        pi_, li, a = pi_[k], li[k], a[k]
        o = p[pi_] + n[pi_] * SURF_OFF
        bl = blocked_slab(scene, o, lights['pos'][li])
        w = (a * ~bl)[:, None] * lights['c'][li]
        np.add.at(E[s:s + CH], pi_, w)
    return E


# ---------------------------------------------------------------- light evaluator 2: the bake (light loop, Liang-Barsky)
def direct_bake(scene, lights, P, N):
    E = np.zeros((len(P), 3))
    o_all = P + N * SURF_OFF
    for i in range(len(lights['r'])):
        q = lights['pos'][i]
        v = q - P
        d = np.sqrt((v * v).sum(1))
        idx = np.nonzero(d < lights['r'][i])[0]
        if len(idx) == 0:
            continue
        ndl = (N[idx] * v[idx]).sum(1) / np.maximum(d[idx], 1e-3)
        x = np.minimum(d[idx] / lights['r'][i], 1.0)
        bias, scale, ex = lights['bse'][i]
        a = (1.0 - np.clip(scale * (x ** ex if ex > 0 else 1.0) + bias, 0, 1)) ** 2.2 * np.maximum(ndl, 0)
        keep = a > 0
        idx, a = idx[keep], a[keep]
        o = o_all[idx]
        seg = q - o
        Lseg = np.sqrt((seg * seg).sum(1))
        u = seg / Lseg[:, None]
        t_end = Lseg - CLEAR
        vis = t_end > 1e-3
        free = np.ones(len(idx), bool)
        for b in range(len(scene['lo'])):
            lo, hi = scene['lo'][b], scene['hi'][b]
            t_in = np.full(len(idx), 1e-4); t_out = t_end.copy()
            ok = np.ones(len(idx), bool)
            for ax in range(3):
                du = u[:, ax]; oa = o[:, ax]
                par = np.abs(du) < 1e-12
                ok &= ~(par & ((oa < lo[ax]) | (oa > hi[ax])))
                with np.errstate(divide='ignore', invalid='ignore'):
                    ta = np.where(par, -np.inf, (lo[ax] - oa) / du)
                    tb = np.where(par, np.inf, (hi[ax] - oa) / du)
                t_in = np.maximum(t_in, np.minimum(ta, tb))
                t_out = np.minimum(t_out, np.maximum(ta, tb))
            free &= ~(ok & (t_in <= t_out))
        E[idx] += (a * (free | ~vis))[:, None] * lights['c'][i]
    return E


# ---------------------------------------------------------------- directions
GOLD = math.pi * (3 - math.sqrt(5))


def frame(n):
    a = np.where(np.abs(n[:, 2:3]) < 0.9, np.array([[0, 0, 1.0]]), np.array([[1.0, 0, 0]]))
    t = np.cross(a, n); t /= np.linalg.norm(t, axis=1, keepdims=True)
    return t, np.cross(n, t)


def cos_dirs(N, K, rng):
    """K cosine-weighted Fibonacci directions about each normal, a random turn a point: (len(N) * K, 3)"""
    i = np.arange(K) + 0.5
    r = np.sqrt(i / K); z = np.sqrt(1 - i / K)
    phi = i * GOLD
    rot = rng.uniform(0, 2 * math.pi, len(N))
    ang = phi[None] + rot[:, None]
    t, b = frame(N)
    d = (r[None, :, None] * (np.cos(ang)[..., None] * t[:, None] + np.sin(ang)[..., None] * b[:, None])
         + z[None, :, None] * N[:, None])
    return d.reshape(-1, 3)


def sphere_dirs(K):
    i = np.arange(K) + 0.5
    z = 1 - 2 * i / K; r = np.sqrt(1 - z * z); phi = i * GOLD
    return np.stack([r * np.cos(phi), r * np.sin(phi), z], 1)


# ---------------------------------------------------------------- the reference: direct + one bounce, brute force
def reference(scene, lights, P, N, seed, K=K_REF):
    rng = np.random.default_rng(seed)
    Ed = direct_ref(scene, lights, P, N, count=False)
    Eb = np.zeros_like(Ed)
    CH = 200
    for s in range(0, len(P), CH):
        p, n = P[s:s + CH], N[s:s + CH]
        D = cos_dirs(n, K, rng)
        O = np.repeat(p + n * 0.5, K, 0)
        t, side, alb = trace(scene, O, D)
        hit = np.isfinite(t)
        H = O[hit] + D[hit] * t[hit, None]
        Hn = AXES[side[hit]]
        Eh = direct_ref(scene, lights, H, Hn, count=False)
        Lo = np.zeros((len(O), 3)); Lo[hit] = alb[hit] * Eh          # B = albedo x E; E = (1/K) sum B
        Eb[s:s + CH] = Lo.reshape(len(p), K, 3).mean(1)
    return Ed, Eb


# ---------------------------------------------------------------- the bake: surfels lit, one bounce through the surfel table
class SurfelTable:
    def __init__(self, S):
        self.lo = np.floor(BOUNDS_LO / SURFEL).astype(int) - 1
        self.dims = (np.floor(BOUNDS_HI / SURFEL).astype(int) + 2 - self.lo)
        self.idx = np.full(tuple(self.dims) + (6,), -1, np.int64)
        k = self.key(S['p'], S['n'])
        self.idx[k[:, 0], k[:, 1], k[:, 2], S['side']] = np.arange(len(S['p']))
        if int((self.idx >= 0).sum()) != len(k):   # one surfel a (cell, side)
            raise SystemExit('surfel key collision')

    def key(self, p, n):
        return np.floor((p - 0.5 * n) / SURFEL).astype(int) - self.lo

    def find(self, p, side):
        k = np.clip(self.key(p, AXES[side]), 0, self.dims - 1)
        return self.idx[k[:, 0], k[:, 1], k[:, 2], side]


def bake(scene, lights, S, seed):
    t0 = time.time()
    Ed = direct_bake(scene, lights, S['p'], S['n'])
    tab = SurfelTable(S)
    B = S['alb'] * Ed
    rng = np.random.default_rng(seed)
    Eb = np.zeros_like(Ed)
    miss = 0
    CH = 1500
    for s in range(0, len(S['p']), CH):
        p, n = S['p'][s:s + CH], S['n'][s:s + CH]
        D = cos_dirs(n, K_SURF, rng)
        O = np.repeat(p + n * 0.5, K_SURF, 0)
        t, side, _ = trace(scene, O, D)
        hit = np.isfinite(t)
        H = O[hit] + D[hit] * t[hit, None]
        j = tab.find(H, side[hit])
        miss += int((j < 0).sum())
        Lo = np.zeros((len(O), 3))
        Lo[np.nonzero(hit)[0][j >= 0]] = B[j[j >= 0]]
        Eb[s:s + CH] = Lo.reshape(len(p), K_SURF, 3).mean(1)
    return dict(Ed=Ed, Eb=Eb, E=Ed + Eb, tab=tab, miss=miss, sec=time.time() - t0)


# ---------------------------------------------------------------- far stand-in candidates
FETCH = {'n': 0}


class VoxelGrid:
    """Six-axis grid (+X -X +Y -Y +Z -Z), each voxel S = sum w E and W = sum w over the surfels within one
    voxel of its center whose normal faces that axis, w = (1 - d^2 / R^2)^2. Lookup: trilinear S and W at
    the point, E_axis = S / W, then the n^2 blend of the facing axes (the probegi grid's own blend)."""

    def __init__(self, S, E, vox=VOX):
        self.v = vox
        self.lo = BOUNDS_LO - vox
        self.dims = np.ceil((BOUNDS_HI + vox - self.lo) / vox).astype(int)
        self.S = np.zeros((6,) + tuple(self.dims) + (3,))
        self.W = np.zeros((6,) + tuple(self.dims))
        R = vox
        base = np.floor((S['p'] - self.lo) / vox - 0.5).astype(int)
        for dx in range(-1, 3):
            for dy in range(-1, 3):
                for dz in range(-1, 3):
                    c = base + np.array([dx, dy, dz])
                    cen = self.lo + (c + 0.5) * vox
                    d2 = ((cen - S['p']) ** 2).sum(1)
                    w = np.clip(1 - d2 / (R * R), 0, None) ** 2
                    ok = (w > 0) & np.all((c >= 0) & (c < self.dims), 1)
                    for a in range(6):
                        fa = ok & ((S['n'] @ AXES[a]) > 0.5)
                        if fa.any():
                            np.add.at(self.S[a], (c[fa, 0], c[fa, 1], c[fa, 2]), w[fa, None] * E[fa])
                            np.add.at(self.W[a], (c[fa, 0], c[fa, 1], c[fa, 2]), w[fa])

    def lookup(self, P, N):
        f = (P - self.lo) / self.v - 0.5
        i0 = np.floor(f).astype(int); fr = f - i0
        out = np.zeros((len(P), 3))
        for a in range(6):
            wa = np.maximum(N @ AXES[a], 0) ** 2
            sel = wa > 0
            if not sel.any():
                continue
            Ssum = np.zeros((sel.sum(), 3)); Wsum = np.zeros(sel.sum())
            for dx in (0, 1):
                for dy in (0, 1):
                    for dz in (0, 1):
                        c = np.clip(i0[sel] + np.array([dx, dy, dz]), 0, self.dims - 1)
                        tw = (np.where(dx, fr[sel, 0], 1 - fr[sel, 0]) * np.where(dy, fr[sel, 1], 1 - fr[sel, 1])
                              * np.where(dz, fr[sel, 2], 1 - fr[sel, 2]))
                        Ssum += tw[:, None] * self.S[a][c[:, 0], c[:, 1], c[:, 2]]
                        Wsum += tw * self.W[a][c[:, 0], c[:, 1], c[:, 2]]
                        FETCH['n'] += int(sel.sum())
            out[sel] += wa[sel, None] * Ssum / np.maximum(Wsum, 1e-9)[:, None]
        return out

    def storage(self, E_lit_mask_S=None, brick=4):
        """sparse bricks of brick^3 voxels holding any weight: count and bytes (6 axes x (RGB9E5 4 B + coverage 1 B))"""
        any_w = (self.W > 0).any(0)
        bd = np.ceil(self.dims / brick).astype(int)
        pad = np.zeros(tuple(bd * brick), bool)
        pad[:self.dims[0], :self.dims[1], :self.dims[2]] = any_w
        bricks = pad.reshape(bd[0], brick, bd[1], brick, bd[2], brick).any((1, 3, 5))
        nb = int(bricks.sum())
        return nb, nb * (brick ** 3 * 6 * 5 + 8)


class SurfelLight:
    """THE CHOSEN FAR TERM: the bake's lit surfels and their total irradiance E (direct + bounce), keyed by
    (70-unit cell, side) as the .tbk keys them. A far point reads the same-side surfels of the 27 cells around
    it, w = (1 - d^2 / R^2)^2, R = 1.5 cells, normalized: 27 table probes a point whatever the light count."""

    def __init__(self, S, E, tab):
        self.S, self.E, self.tab = S, E, tab

    def lookup(self, P, N):
        FETCH['n'] += 27 * len(P)
        return surfel_knn(self.S, self.E, self.tab, P, N)


def surfel_knn(S, E, tab, P, N, R=1.5 * SURFEL):
    """the nearest-surfels lookup: same-side surfels in the 27 cells around, w = (1 - d^2/R^2)^2"""
    side = np.argmax(np.stack([N @ AXES[a] for a in range(6)], 1), 1)
    k0 = tab.key(P, AXES[side])
    Ssum = np.zeros((len(P), 3)); Wsum = np.zeros(len(P))
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                k = np.clip(k0 + np.array([dx, dy, dz]), 0, tab.dims - 1)
                j = tab.idx[k[:, 0], k[:, 1], k[:, 2], side]
                ok = j >= 0
                d2 = ((S['p'][np.maximum(j, 0)] - P) ** 2).sum(1)
                w = np.where(ok, np.clip(1 - d2 / (R * R), 0, None) ** 2, 0)
                Ssum += w[:, None] * E[np.maximum(j, 0)]
                Wsum += w
    return Ssum / np.maximum(Wsum, 1e-9)[:, None]


class ProbeGrid:
    """the irradiance-volume candidate: probes on a PROBE lattice in the air, each the six-axis irradiance at its
    own point (every light, shadowed, plus the surfels' light by 128 sphere rays); trilinear over the probes
    outside the boxes, n^2 blend. No visibility test from the surface (as a plain probe volume)."""

    def __init__(self, scene, lights, S, B, tab):
        xs = np.arange(0, L_X + 1, PROBE); ys = np.arange(-W_Y, W_Y + 1, PROBE); zs = np.arange(128, 1665, PROBE)
        g = np.stack(np.meshgrid(xs, ys, zs, indexing='ij'), -1)
        self.shape = g.shape[:3]
        self.org = np.array([0, -W_Y, 128.0])
        p = g.reshape(-1, 3)
        inside = np.any(np.all((p[:, None] > scene['lo'][None]) & (p[:, None] < scene['hi'][None]), 2), 1)
        self.valid = (~inside).astype(float)
        E = np.zeros((len(p), 6, 3))
        for a in range(6):
            E[:, a] = direct_ref(scene, lights, p, np.repeat(AXES[a][None], len(p), 0), count=False)
        dirs = sphere_dirs(K_PROBE)
        CH = 400
        for s in range(0, len(p), CH):
            pp = p[s:s + CH]
            O = np.repeat(pp, K_PROBE, 0); D = np.tile(dirs, (len(pp), 1))
            t, side, _ = trace(scene, O, D)
            hit = np.isfinite(t)
            j = np.full(len(O), -1); j[hit] = tab.find(O[hit] + D[hit] * t[hit, None], side[hit])
            Bo = np.where((j >= 0)[:, None], B[np.maximum(j, 0)], 0).reshape(len(pp), K_PROBE, 3)
            for a in range(6):
                c = np.maximum(dirs @ AXES[a], 0)
                E[s:s + CH, a] += (4.0 / K_PROBE) * np.einsum('pkc,k->pc', Bo, c)
        self.E = E.reshape(self.shape + (6, 3)) * self.valid.reshape(self.shape)[..., None, None]
        self.V = self.valid.reshape(self.shape)

    def lookup(self, P, N):
        f = (P - self.org) / PROBE
        i0 = np.clip(np.floor(f).astype(int), 0, np.array(self.shape) - 2); fr = np.clip(f - i0, 0, 1)
        Es = np.zeros((len(P), 6, 3)); Ws = np.zeros(len(P))
        for dx in (0, 1):
            for dy in (0, 1):
                for dz in (0, 1):
                    c = i0 + np.array([dx, dy, dz])
                    tw = (np.where(dx, fr[:, 0], 1 - fr[:, 0]) * np.where(dy, fr[:, 1], 1 - fr[:, 1])
                          * np.where(dz, fr[:, 2], 1 - fr[:, 2]))
                    Es += tw[:, None, None] * self.E[c[:, 0], c[:, 1], c[:, 2]]
                    Ws += tw * self.V[c[:, 0], c[:, 1], c[:, 2]]
        Es /= np.maximum(Ws, 1e-9)[:, None, None]
        w = np.maximum(N @ AXES.T, 0) ** 2
        return np.einsum('pa,pac->pc', w, Es)


# ---------------------------------------------------------------- the blend
def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def blend_w(d, red):
    if red == 'noblend':
        return (d >= 0.5 * (D0 + D1)).astype(float)
    return smoothstep(D0, D1, d)


# ---------------------------------------------------------------- metrics
def rel_err(F, R, floor):
    return np.abs(lum(F) - lum(R)) / np.maximum(lum(R), floor)


def build(n_lights, seed):
    scene = make_street()
    lights = make_lights(scene, n_lights, seed)
    S = make_surfels(scene)
    P, N, A = sample_points(scene, N_REF, 11)
    t0 = time.time()
    Rd, Rb = reference(scene, lights, P, N, seed=101)
    t_ref = time.time() - t0
    bk = bake(scene, lights, S, seed=202)
    return dict(scene=scene, lights=lights, S=S, P=P, N=N, Rd=Rd, Rb=Rb, R=Rd + Rb, bk=bk, t_ref=t_ref)


def far_term(W, grid, red):
    """the far stand-in the renderer reads; reds swap it"""
    if red == 'nosurfel':
        return lambda P, N: np.zeros((len(P), 3))
    if red == 'perlight':
        return lambda P, N: direct_ref(W['scene'], W['lights'], P, N) + grid['bounce_only'].lookup(P, N)
    if red == 'nobounce':
        return grid['direct_only'].lookup
    return grid['full'].lookup


def run(worlds, grids, red):
    """all checks for one configuration; returns {check: (pass, line)}"""
    res = {}
    out = []

    def row(name, ok, text):
        res[name] = ok
        out.append('%-3s %-4s %s' % (name, 'PASS' if ok else 'FAIL', text))

    # C0 -- the curve and the two light evaluators
    x = np.array([0.0, 0.5, 1.0, 0.25])
    c2 = curve(x, np.array([0, 1, 2.0]))
    want = np.array([1.0, 0.75 ** 2.2, 0.0, (1 - 0.0625) ** 2.2])
    W = worlds[50]
    k = np.random.default_rng(3).choice(len(W['S']['p']), 400, replace=False)
    ea = direct_ref(W['scene'], W['lights'], W['S']['p'][k], W['S']['n'][k], count=False)
    eb = W['bk']['Ed'][k]
    dev = float(np.max(np.abs(ea - eb)) / max(np.max(np.abs(ea)), 1e-12))
    lit_frac = float((lum(eb) > 0).mean())
    row('C0', bool(np.allclose(c2, want, atol=1e-12) and dev < 1e-9 and lit_frac > 0.2),
        'curve at x=0,.5,1,.25 (bias 0 scale 1 exp 2) = %s (doc %s); evaluators max dev %.1e of max on 400 surfels '
        '(%.0f%% lit)' % (np.round(c2, 6).tolist(), np.round(want, 6).tolist(), dev, 100 * lit_frac))

    for nl in (50, 500):
        W, G = worlds[nl], grids[nl]
        R = W['R']
        floor = 0.02 * np.percentile(lum(R), 95)
        lit = lum(R) > floor
        COUNT['pairs'] = 0
        F = far_term(W, G, red)(W['P'], W['N'])
        e = rel_err(F, R, floor)[lit]
        en = float(lum(F)[lit].sum() / lum(R)[lit].sum())
        # A: in the band (w = 0.5) the output is 0.5 near + 0.5 far: what matters is the far term against the reference
        row('A%d' % nl, bool(np.median(e) <= BAR_A_MED and np.percentile(e, 90) <= BAR_A_P90
                            and BAR_ENERGY[0] <= en <= BAR_ENERGY[1]),
            '%d lights, %d lit points: far vs reference rel err median %.3f (bar %.2f) p90 %.3f (bar %.2f), '
            'energy %.3f (bar %.2f..%.2f)' % (nl, lit.sum(), np.median(e), BAR_A_MED, np.percentile(e, 90),
                                               BAR_A_P90, en, *BAR_ENERGY))
        # B: past D1 there are no real lights; the output is the far term alone
        w_far = blend_w(np.full(len(R), D1 + 512.0), red)
        Out = w_far[:, None] * F + (1 - w_far[:, None]) * 0.0
        sr = float(lum(Out)[lit].sum() / lum(R)[lit].sum())
        kept = float((lum(Out)[lit] >= 0.5 * lum(R)[lit]).mean())
        row('B%d' % nl, bool(sr >= BAR_B_SUM and kept >= BAR_B_KEPT),
            'past the band (%.0f u): sum ratio %.3f (bar %.2f), lit points keeping >= 50%%: %.3f (bar %.2f)'
            % (D1 + 512, sr, BAR_B_SUM, kept, BAR_B_KEPT))
        # C: sweep the camera distance through the band; near = the reference (real lights + near GI)
        ds = np.arange(D0 - 256, D1 + 256 + 1, STEP)
        steps = np.zeros(lit.sum())
        Rl, Fl = lum(R)[lit], lum(F)[lit]
        prev = None
        for d in ds:
            w = blend_w(np.full(1, d), red)[0]
            o = (1 - w) * Rl + w * Fl
            if prev is not None:
                steps = np.maximum(steps, np.abs(o - prev) / np.maximum(Rl, floor))
            prev = o
        p99 = float(np.percentile(steps, 99))
        edge = float(np.max(np.abs(((1 - blend_w(np.array([D0]), red)[0]) * Rl + blend_w(np.array([D0]), red)[0] * Fl) - Rl)))
        row('C%d' % nl, bool(p99 <= STEP_BAR and edge == 0.0),
            'camera sweep %.0f..%.0f u by %.0f u: largest per-step change p99 %.4f (bar %.3f), max %.4f; at D0 the '
            'picture is the near one (dev %.1e)' % (ds[0], ds[-1], STEP, p99, STEP_BAR, steps.max(), edge))
        # E: points lit mostly by the bounce
        bshare = lum(W['Rb']) / np.maximum(lum(R), 1e-12)
        bd = lit & (bshare >= 0.5)
        eb_ = rel_err(F, R, floor)[bd]
        nbd = int(bd.sum())
        okE = nbd >= E_MIN_POINTS and np.median(eb_) <= BAR_E_MED and np.percentile(eb_, 90) <= BAR_E_P90
        row('E%d' % nl, bool(okE),
            '%d bounce-dominated lit points (floor %d): rel err median %.3f (bar %.2f) p90 %.3f (bar %.2f)'
            % (nbd, E_MIN_POINTS, np.median(eb_) if nbd else float('nan'), BAR_E_MED,
               np.percentile(eb_, 90) if nbd else float('nan'), BAR_E_P90))

    # D: cost of the far lookup, 50 vs 500 lights: work counted, and timed
    ops, tm = {}, {}
    for nl in (50, 500):
        W, G = worlds[nl], grids[nl]
        f = far_term(W, G, red)
        COUNT['pairs'] = 0; FETCH['n'] = 0
        f(W['P'], W['N'])
        ops[nl] = (COUNT['pairs'] + FETCH['n']) / len(W['P'])
        reps, t0 = 0, time.perf_counter()
        while time.perf_counter() - t0 < 0.4:
            f(W['P'], W['N']); reps += 1
        tm[nl] = (time.perf_counter() - t0) / reps / len(W['P']) * 1e6
    # contrast: the near path (every light within reach) at both counts
    near = {}
    for nl in (50, 500):
        W = worlds[nl]
        COUNT['pairs'] = 0
        direct_ref(W['scene'], W['lights'], W['P'][:200], W['N'][:200])
        near[nl] = COUNT['pairs'] / 200
    ro, rt = ops[500] / max(ops[50], 1e-9), tm[500] / max(tm[50], 1e-9)
    row('D', bool(ro <= BAR_D_OPS and rt <= BAR_D_TIME),
        'far lookup work a point %.1f -> %.1f (ratio %.2f, bar %.2f); time %.2f -> %.2f us a point (ratio %.2f, bar '
        '%.1f); near path light tests a point %.0f -> %.0f for contrast'
        % (ops[50], ops[500], ro, BAR_D_OPS, tm[50], tm[500], rt, BAR_D_TIME, near[50], near[500]))
    return res, out


TARGET = {'nosurfel': ['A50', 'A500', 'B50', 'B500'], 'noblend': ['C50', 'C500'], 'perlight': ['D'],
          'nobounce': ['E50', 'E500']}


def main():
    t_start = time.time()
    worlds, grids = {}, {}
    for nl in (50, 500):
        W = build(nl, seed=1000 + nl)
        S, bk = W['S'], W['bk']
        tab = bk['tab']
        grids[nl] = dict(full=SurfelLight(S, bk['E'], tab), direct_only=SurfelLight(S, bk['Ed'], tab),
                         bounce_only=SurfelLight(S, bk['Eb'], tab), voxel=VoxelGrid(S, bk['E']))
        worlds[nl] = W
        nlit = int((lum(bk['E']) > 1e-3 * np.percentile(lum(bk['E']), 99)).sum())
        nbk, byt = grids[nl]['voxel'].storage()
        print('scene %3d lights: %d boxes, %d surfels (%d lit), %d reference points; bake %.1f s (bounce rays off the '
              'table: %d), reference %.1f s; 128-u voxel grid would be %d bricks of 4^3 = %.0f KB; far-light file (16 B a lit '
              'surfel, docs section 5) = %.0f KB' % (nl, len(W['scene']['lo']), len(S['p']), nlit, len(W['P']), bk['sec'],
                                         bk['miss'], W['t_ref'], nbk, byt / 1024, nlit * 16 / 1024))

    # the candidates, measured (not gated): which lookup the far field should use
    print('\ncandidates: far term vs reference on lit points (rel err median / p90 / p99; energy)')
    for nl in (50, 500):
        W = worlds[nl]
        R = W['R']; floor = 0.02 * np.percentile(lum(R), 95); lit = lum(R) > floor
        PG = ProbeGrid(W['scene'], W['lights'], W['S'], W['S']['alb'] * W['bk']['Ed'], W['bk']['tab'])
        rng = np.random.default_rng(5)
        off = W['N'] * rng.uniform(-48, 48, len(R))[:, None]
        sk = lambda P, R=1.5 * SURFEL: surfel_knn(W['S'], W['bk']['E'], W['bk']['tab'], P, W['N'], R)
        cands = [('nearest surfels, 70 u cells, R 105 (chosen)', sk(W['P'])),
                 ('nearest surfels, point moved +-48 u along N (LOD mesh error)', sk(W['P'] + off)),
                 ('nearest surfels, R 70', sk(W['P'], 70.0)),
                 ('voxel grid 64 u', VoxelGrid(W['S'], W['bk']['E'], 64.0).lookup(W['P'], W['N'])),
                 ('voxel grid 128 u', VoxelGrid(W['S'], W['bk']['E'], 128.0).lookup(W['P'], W['N'])),
                 ('voxel grid 128 u, point moved +-48 u along N',
                  VoxelGrid(W['S'], W['bk']['E'], 128.0).lookup(W['P'] + off, W['N'])),
                 ('voxel grid 256 u', VoxelGrid(W['S'], W['bk']['E'], 256.0).lookup(W['P'], W['N'])),
                 ('probe grid 256 u (irradiance volume)', PG.lookup(W['P'], W['N']))]
        for name, F in cands:
            e = rel_err(F, R, floor)[lit]
            print('  %3d lights  %-62s %.3f / %.3f / %.3f; %.3f' % (nl, name, np.median(e), np.percentile(e, 90),
                  np.percentile(e, 99), lum(F)[lit].sum() / lum(R)[lit].sum()))
    # the reference's own noise: a second bounce seed on 400 points
    W = worlds[50]
    m = 400
    Rd2, Rb2 = reference(W['scene'], W['lights'], W['P'][:m], W['N'][:m], seed=999)
    R = W['R'][:m]; floor = 0.02 * np.percentile(lum(W['R']), 95); lit = lum(R) > floor
    e = rel_err(Rd2 + Rb2, R, floor)
    bd = lit & (lum(W['Rb'][:m]) >= 0.5 * lum(R))
    print('  reference against itself (another bounce seed, 50 lights): %d lit %.3f / %.3f / %.3f; %d bounce-dominated '
          '%.3f / %.3f -- the floor under any bar' % (lit.sum(), np.median(e[lit]), np.percentile(e[lit], 90),
                                                      np.percentile(e[lit], 99), bd.sum(), np.median(e[bd]),
                                                      np.percentile(e[bd], 90)))

    only = os.environ.get('FARVIEW_RED', '')
    if only:
        if only not in TARGET:
            raise SystemExit('unknown FARVIEW_RED %r (one of %s)' % (only, ', '.join(TARGET)))
        res, out = run(worlds, grids, only)
        print('\nred %s:' % only)
        print('\n'.join('  ' + o for o in out))
        failed = [c for c in TARGET[only] if not res[c]]
        print('red %s: %s (target checks failing: %s)' % (only, 'FAILS as it must' if failed else 'DID NOT FAIL',
                                                          ', '.join(failed) or 'none'))
        sys.exit(1 if failed else 0)

    print('\ngreen:')
    res, out = run(worlds, grids, '')
    print('\n'.join('  ' + o for o in out))
    green_ok = all(res.values())
    reds_ok = True
    for red, targets in TARGET.items():
        r, o = run(worlds, grids, red)
        failed = [c for c in targets if not r[c]]
        print('\nred %s (must FAIL %s):' % (red, ', '.join(targets)))
        print('\n'.join('  ' + x for x in o if x.split()[0] in targets))
        print('  -> %s' % ('FAILS as it must (%s)' % ', '.join(failed) if failed else 'DID NOT FAIL: the gate is blind'))
        reds_ok &= len(failed) == len(targets)
    verdict = green_ok and reds_ok
    print('\nFARVIEW1 %s: green %s, reds %s; %.0f s' % ('PASS' if verdict else 'FAIL', 'PASS' if green_ok else 'FAIL',
                                                    'all fail' if reds_ok else 'NOT all fail', time.time() - t_start))
    sys.exit(0 if verdict else 1)


if __name__ == '__main__':
    main()
