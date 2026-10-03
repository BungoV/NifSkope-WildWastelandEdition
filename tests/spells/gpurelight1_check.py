#!/usr/bin/env python3
"""GPURELIGHT1 twin: relight the baked surfels and probes when the lights change, without rebaking.

  python3 tests/spells/gpurelight1_check.py            the checks, then each red (each must FAIL); exit 0 = all good
  GPURELIGHT1_RED=nobounce python3 ...                 run the checks with the bounce passes skipped (must FAIL, exit 1)
  GPURELIGHT1_RED=noshadow python3 ...                 run the checks with the sun's shadow map ignored (must FAIL, exit 1)

Design: docs/cloud/GPURELIGHT1_DESIGN.md. Standalone (python3 + numpy), synthetic scene built here, no game data.

Two pipelines over one synthetic building (two rooms, a doorway, a window, a pillar, a table; ground outside):

  REBAKE   (the reference, float64, dense matrices): from the geometry, every time the lights change:
           trace the probe rays -> surfels (cell 35 u keyed by position AND face axis), links (cap 256, the
           rest unlinked), sky share per octant; trace the feed lists (each surfel's visible probes, the
           (1 - d^2/r^2)^2 weights of src/probegi.cpp); light every surfel with FRESH shadow segments (PRTP2
           curve, spot cone, 24 u fixture clear) and a fresh sun ray; then the passes of lane BOUNCE2 until
           the largest change <= 1e-3 x the brightest B.
  RELIGHT  (the GPU plan, float32, sparse lists, segmented sums = one thread per row): from ONE bake made
           before any light changed, plus one light-independent precompute (the visible surfel-light pairs
           within each light's baked radius). Per frame only the light parameters change: colour, dimmer,
           on/off, radius (<= baked), sun direction/colour, the six sky colours. Kernels: direct (per surfel
           over its pairs, curve and cone evaluated live), sun (shadow map or cached rays), gather (per probe
           over its links), feed (per surfel over its probes), repeated until settled -- or one pass per frame
           with history (amortized).

Checks (each prints PASS/FAIL with its numbers):
  0 bake      the rebake reproduces the original bake exactly (same surfels, links, feed): the scene did not move
  A toggle    light 0 off:            relight == rebake (B per surfel, probe six-axis E, pass count)
  B colour    light 1 colour changed: relight == rebake
  C flicker   light 2 dimmer over 4 frames: relight == rebake on every frame
  D radius    light 3 radius x 0.7:   relight == rebake (curve evaluated live; pairs kept to the baked radius)
  E sun-ray   three hours of the day (sun dir + colour + sky colours), sun visibility by cached rays: == rebake
  S sun-map   the same hours, sun visibility by an orthographic shadow map: within the stated loose bar
  L linear    relight(c1) + relight(c2) == relight(c1 + c2) at a pinned pass count (the per-light basis option)
  G amortize  one pass per frame with history, warm-started from the previous state: converges to the rebake's
              tight fixed point; frames reported
  R robin     a quarter of the probes regathered per frame: converges to the same point; frames reported
Tolerances (stated, float32 relight vs float64 rebake): |dB| <= 1e-4 x max B and |dE| <= 1e-4 x max E,
same pass count; S: <= 2% of sun-facing surfels change lit state and total B within 1%; L: 1e-5 relative;
G/R: 2e-4 x max B against the rebake settled to 1e-7.
Reds: nobounce (relight runs pass 1 only) must FAIL A-E and G; noshadow (the shadow map treats every surfel as
sunlit) must FAIL S. The plain run self-tests both and exits nonzero unless both fail.
"""
import math
import os
import sys
import time

import numpy as np

CS = 35.0            # surfel cell (game units)
NRAY = 1024          # rays per probe
LINK_CAP = 256       # links kept per probe (FO4CS's in-game bake keeps 256); the rest is unlinked weight
FIXTURE = 24.0       # shadow segment stops this far before the light (probegi.cpp fixtureClear)
SURF_OFF = 2.0       # shadow / feed segments start this far off the surface
SETTLE = 1e-3        # pass bar: largest change <= SETTLE x brightest B (lane BOUNCE2)
CAP = 64
AXES = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)
TOL = 1e-4

# ------------------------------------------------------------------ the scene: solid axis-aligned boxes
WALL = (0.62, 0.58, 0.52)
GREEN = (0.30, 0.50, 0.32)
BOXES = [  # (lo, hi, albedo linear)
    ((-1400, -1400, -40), (2100, 1890, 0), (0.22, 0.20, 0.16)),        # ground outside
    ((0, 0, 0), (700, 490, 10), (0.35, 0.25, 0.15)),                   # wooden floor
    ((-20, -20, 280), (720, 510, 300), (0.70, 0.70, 0.68)),            # ceiling
    ((-20, -20, 0), (0, 510, 280), WALL),                              # west wall
    ((700, -20, 0), (720, 510, 280), GREEN),                           # east wall
    ((-20, 490, 0), (720, 510, 280), WALL),                            # north wall
    ((-20, -20, 0), (150, 0, 280), WALL),                              # south wall, left of the window
    ((300, -20, 0), (720, 0, 280), WALL),                              # south wall, right of the window
    ((150, -20, 0), (300, 0, 100), WALL),                              # under the window
    ((150, -20, 220), (300, 0, 280), WALL),                            # over the window
    ((380, 0, 0), (400, 180, 280), GREEN),                             # partition, south of the doorway
    ((380, 280, 0), (400, 490, 280), GREEN),                           # partition, north of the doorway
    ((380, 180, 210), (400, 280, 280), GREEN),                         # lintel
    ((150, 300, 10), (190, 340, 280), (0.60, 0.12, 0.10)),             # red pillar
    ((480, 100, 70), (600, 200, 80), (0.45, 0.30, 0.18)),              # table top
]
LO = np.array([b[0] for b in BOXES], float)
HI = np.array([b[1] for b in BOXES], float)
ALB = np.array([b[2] for b in BOXES], float)


def lin(rgb8):
    return (np.asarray(rgb8, float) / 255.0) ** 2.2


# the lights as baked: pos, radius, colour (sRGB bytes), spot dir (None = omni), FOV deg, falloff exponent
LIGHTS0 = [
    dict(pos=(150, 150, 240), radius=420.0, rgb=(255, 220, 180), dimmer=1.6, spot=None),
    dict(pos=(560, 360, 240), radius=460.0, rgb=(200, 220, 255), dimmer=1.6, spot=None),
    dict(pos=(540, 150, 140), radius=320.0, rgb=(255, 150, 60), dimmer=1.2, spot=None),
    dict(pos=(290, 400, 260), radius=520.0, rgb=(255, 255, 255), dimmer=1.4, spot=(0, 0, -1), fov=90.0, fe=2.0),
]


def state(lights=None, sun=None, amb=None):
    return dict(lights=[dict(l) for l in (lights or LIGHTS0)], sun=sun, amb=amb)


# ------------------------------------------------------------------ ray / segment against the boxes
def trace(O, D, tmax=None, chunk=16384):
    """first hit of each ray: t (inf = miss), box index, face id (2 x axis + negative)"""
    R = len(O)
    T = np.full(R, np.inf)
    Bi = np.full(R, -1, np.int64)
    F = np.full(R, -1, np.int64)
    Dz = np.where(np.abs(D) < 1e-12, 1e-12, D)
    inv = 1.0 / Dz
    for s in range(0, R, chunk):
        o, iv = O[s:s + chunk, None, :], inv[s:s + chunk, None, :]
        t1 = (LO[None] - o) * iv
        t2 = (HI[None] - o) * iv
        tn = np.minimum(t1, t2)
        tf = np.maximum(t1, t2)
        tnear = tn.max(-1)
        tfar = tf.min(-1)
        ok = (tnear <= tfar) & (tnear > 1e-6)
        if tmax is not None:
            ok &= tnear < tmax[s:s + chunk, None]
        tt = np.where(ok, tnear, np.inf)
        b = tt.argmin(1)
        t = tt[np.arange(len(b)), b]
        ax = tn[np.arange(len(b)), b].argmax(-1)
        d = D[s:s + chunk][np.arange(len(b)), ax]
        hit = np.isfinite(t)
        T[s:s + chunk] = t
        Bi[s:s + chunk] = np.where(hit, b, -1)
        F[s:s + chunk] = np.where(hit, 2 * ax + (d > 0), -1)   # a ray going +axis hits the face whose normal is -axis
    return T, Bi, F


def blocked(P, Q, clear_end=0.0):
    """segment P -> Q blocked (stopping clear_end short of Q); Q may be at infinity via a direction"""
    D = Q - P
    L = np.linalg.norm(D, axis=1)
    D = D / L[:, None]
    T, _, _ = trace(P, D, tmax=L - clear_end)
    return np.isfinite(T)


def fib_dirs(n):
    i = np.arange(n) + 0.5
    z = 1.0 - 2.0 * i / n
    r = np.sqrt(1.0 - z * z)
    ph = i * math.pi * (3.0 - math.sqrt(5.0))
    return np.stack([r * np.cos(ph), r * np.sin(ph), z], 1)


def probe_positions():
    g = [(x, y, z) for x in np.arange(55, 700, 98) for y in np.arange(55, 490, 95) for z in (70.0, 190.0)]
    P = np.array(g, float)
    inside = np.zeros(len(P), bool)
    for lo, hi in zip(LO, HI):
        inside |= np.all((P > lo - 15) & (P < hi + 15), 1)
    return P[~inside]


# ------------------------------------------------------------------ the bake (geometry only, no light)
def bake():
    P = probe_positions()
    M = len(P)
    D0 = fib_dirs(NRAY)
    O = np.repeat(P, NRAY, 0)
    D = np.tile(D0, (M, 1))
    T, Bi, F = trace(O, D)
    hit = np.isfinite(T)
    owner = np.repeat(np.arange(M), NRAY)
    # sky per octant (bit 0 x<0, bit 1 y<0, bit 2 z<0): the share of the octant's rays that miss
    oc = (D[:, 0] < 0) * 1 + (D[:, 1] < 0) * 2 + (D[:, 2] < 0) * 4
    sky = np.zeros((M, 8))
    tot = np.zeros((M, 8))
    np.add.at(tot, (owner, oc), 1.0)
    np.add.at(sky, (owner[~hit], oc[~hit]), 1.0)
    skyVis = np.where(tot > 0, sky / np.maximum(tot, 1), 0.0)
    # surfels: key = (cell, face)
    H = O[hit] + D[hit] * T[hit, None]
    key = np.concatenate([np.floor(H / CS).astype(np.int64), F[hit, None]], 1)
    uk, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    N = len(uk)
    cnt = np.bincount(inv, minlength=N).astype(float)
    pos = np.stack([np.bincount(inv, H[:, c], N) for c in range(3)], 1) / cnt[:, None]
    nrm = AXES[uk[:, 3]]
    alb = np.stack([np.bincount(inv, ALB[Bi[hit], c], N) for c in range(3)], 1) / cnt[:, None]
    # links: per probe, per surfel: weight = rays / NRAY, dir = mean ray dir; cap, rest unlinked
    ho = owner[hit]
    pk = ho * N + inv
    ul, li = np.unique(pk, return_inverse=True)
    li = li.reshape(-1)
    w = np.bincount(li, minlength=len(ul)) / NRAY
    dsum = np.stack([np.bincount(li, D[hit][:, c], len(ul)) for c in range(3)], 1)
    ldir = dsum / np.linalg.norm(dsum, axis=1)[:, None]
    lp, ls = ul // N, ul % N
    order = np.lexsort((ls, -w, lp))      # per probe: weight descending, then surfel
    lp, ls, w, ldir = lp[order], ls[order], w[order], ldir[order]
    rank = np.arange(len(lp)) - np.searchsorted(lp, lp)
    keep = rank < LINK_CAP
    unl = np.bincount(lp[~keep], w[~keep], M)
    lp, ls, w, ldir = lp[keep], ls[keep], w[keep], ldir[keep]
    # feed lists: each surfel's visible probes within r (BOUNCE2), closest visible within 2r as fallback
    dd = np.linalg.norm(P[:, None] - P[None], axis=2)
    np.fill_diagonal(dd, np.inf)
    r = 2.0 * float(np.median(dd.min(1)))
    S0 = pos + nrm * SURF_OFF
    dsp = np.linalg.norm(S0[:, None] - P[None], axis=2)
    cs_, cp_ = np.nonzero(dsp < 2 * r)
    vis = ~blocked(S0[cs_], P[cp_])
    cs_, cp_ = cs_[vis], cp_[vis]
    dv = dsp[cs_, cp_]
    inr = dv < r
    fs, fp, fw = [cs_[inr]], [cp_[inr]], [(1 - (dv[inr] / r) ** 2) ** 2]
    has = np.zeros(N, bool)
    has[cs_[inr]] = True
    out = ~inr & ~has[cs_]
    if out.any():
        c2, p2, d2 = cs_[out], cp_[out], dv[out]
        o2 = np.lexsort((d2, c2))
        first = np.r_[True, c2[o2][1:] != c2[o2][:-1]]
        fs.append(c2[o2][first]); fp.append(p2[o2][first]); fw.append(np.ones(first.sum()))
    fs, fp, fw = np.concatenate(fs), np.concatenate(fp), np.concatenate(fw)
    o = np.lexsort((fp, fs))
    fs, fp, fw = fs[o], fp[o], fw[o]
    return dict(P=P, M=M, N=N, pos=pos, nrm=nrm, alb=alb, face=uk[:, 3], skyVis=skyVis,
                lp=lp, ls=ls, w=w, ldir=ldir, unl=unl, fs=fs, fp=fp, fw=fw, radius=r)


# ------------------------------------------------------------------ PRTP2 light terms
def atten(d, radius):
    x = np.clip(d / radius, 0, 1)
    return (1.0 - np.clip(x * x, 0, 1)) ** 2.2


def cone(Lto, l):
    """Lto: unit vectors surface -> light"""
    if l.get('spot') is None:
        return np.ones(len(Lto))
    sd = np.asarray(l['spot'], float)
    sd = sd / np.linalg.norm(sd)
    co = math.cos(math.radians(l['fov']) / 2)
    base = np.clip(1 - (1 - (-Lto) @ sd) / (1 - co), 0, 1)
    return np.minimum(base ** max(l['fe'], 1e-3), 1.0)


def sky_cube(skyVis, amb):
    """probesky.cpp probeSkyCube: E[a] = pi x amb[a] x 0.25 x sum of the four octants on side a"""
    M = len(skyVis)
    E = np.zeros((M, 6, 3))
    if amb is None:
        return E
    amb = np.asarray(amb, float)
    for a in range(6):
        ax, neg = a >> 1, a & 1
        sel = [o for o in range(8) if ((o >> ax) & 1) == neg]
        E[:, a, :] = math.pi * amb[a][None, :] * 0.25 * skyVis[:, sel].sum(1)[:, None]
    return E


# ------------------------------------------------------------------ REBAKE: everything from the geometry, float64, dense
def rebake(st, settle=SETTLE, cap=CAP, b=None):
    b = b or bake()
    N, M = b['N'], b['M']
    pos, nrm, alb = b['pos'], b['nrm'], b['alb']
    O = pos + nrm * SURF_OFF
    E = np.zeros((N, 3))
    for l in st['lights']:
        col = lin(l['rgb']) * l['dimmer']
        if not np.any(col):
            continue
        lp = np.asarray(l['pos'], float)
        v = lp[None] - pos
        d = np.linalg.norm(v, axis=1)
        Lto = v / d[:, None]
        nl = np.maximum((nrm * Lto).sum(1), 0)
        a = atten(d, l['radius']) * cone(Lto, l) * nl
        idx = np.nonzero(a > 0)[0]
        if len(idx):
            blk = blocked(O[idx], np.repeat(lp[None], len(idx), 0), FIXTURE)
            a[idx[blk]] = 0.0
            a[np.setdiff1d(np.arange(N), idx)] = 0.0
        E += a[:, None] * col[None]
    sun_vis = None
    if st['sun'] is not None:
        sto = np.asarray(st['sun']['to'], float)
        sto = sto / np.linalg.norm(sto)
        nl = np.maximum(nrm @ sto, 0)
        sun_vis = np.zeros(N, bool)
        idx = np.nonzero(nl > 0)[0]
        sun_vis[idx] = ~blocked(O[idx], O[idx] + sto[None] * 400000.0)
        E += (nl * sun_vis)[:, None] * np.asarray(st['sun']['rgb'], float)[None]
    B1 = alb * E
    # dense transfer: G[j, a, s] = omega cosA kUnl ; W[s, j] normalized feed weights
    G = np.zeros((M, 6, N))
    linked = np.bincount(b['lp'], b['w'], M)
    kU = np.where((linked > 0) & (b['unl'] > 0), (linked + b['unl']) / np.maximum(linked, 1e-300), 1.0)
    cosA = np.maximum(b['ldir'] @ AXES.T, 0)
    for a in range(6):
        np.add.at(G[:, a, :], (b['lp'], b['ls']), b['w'] * 4 * math.pi * cosA[:, a] * kU[b['lp']])
    W = np.zeros((N, M))
    np.add.at(W, (b['fs'], b['fp']), b['fw'])
    ws = W.sum(1)
    W = np.where(ws[:, None] > 0, W / np.maximum(ws, 1e-300)[:, None], 0.0)
    Sky = sky_cube(b['skyVis'], st['amb'])
    n2 = nrm * nrm

    def gather(B):
        return np.einsum('jas,sc->jac', G, B) + Sky

    def feed(Ep):
        out = np.zeros((N, 3))
        for a in range(3):
            pick = np.where(nrm[:, a] >= 0, 2 * a, 2 * a + 1)
            Epos, Eneg = W @ Ep[:, 2 * a, :], W @ Ep[:, 2 * a + 1, :]
            out += n2[:, a:a + 1] * np.where((pick == 2 * a)[:, None], Epos, Eneg)
        return out
    B, Ep, passes = B1.copy(), gather(B1), 1
    for _ in range(2, cap + 1):
        Bn = B1 + alb * feed(Ep) / math.pi
        ch = float(np.abs(Bn - B).max())
        B, Ep, passes = Bn, gather(Bn), passes + 1
        if ch <= settle * B.max():
            break
    return dict(B=B, E=Ep, passes=passes, B1=B1, sun_vis=sun_vis, bake=b)


# ------------------------------------------------------------------ RELIGHT: one bake + a light-independent precompute
def precompute(b):
    """once, after the bake: everything the relight reads that does not depend on any light's colour"""
    f32 = np.float32
    N, M = b['N'], b['M']
    pos, nrm = b['pos'], b['nrm']
    O = pos + nrm * SURF_OFF
    # visible surfel-light pairs within each light's BAKED radius (facing and cone evaluated live)
    ps, pl = [], []
    for k, l in enumerate(LIGHTS0):
        lp = np.asarray(l['pos'], float)
        d = np.linalg.norm(lp[None] - pos, axis=1)
        idx = np.nonzero(d < l['radius'])[0]
        vis = ~blocked(O[idx], np.repeat(lp[None], len(idx), 0), FIXTURE)
        ps.append(idx[vis]); pl.append(np.full(vis.sum(), k))
    ps, pl = np.concatenate(ps), np.concatenate(pl)
    o = np.lexsort((pl, ps))
    ps, pl = ps[o], pl[o]
    linked = np.bincount(b['lp'], b['w'], M)
    kU = np.where((linked > 0) & (b['unl'] > 0), (linked + b['unl']) / np.maximum(linked, 1e-300), 1.0)
    coef = (np.maximum(b['ldir'] @ AXES.T, 0) * (b['w'] * 4 * math.pi * kU[b['lp']])[:, None]).astype(f32)
    ws = np.bincount(b['fs'], b['fw'], N)
    fwn = (b['fw'] / ws[b['fs']]).astype(f32)
    return dict(N=N, M=M, pos=pos.astype(f32), nrm=nrm.astype(f32), alb=b['alb'].astype(f32), O=O,
                pair_s=ps, pair_l=pl, pair_start=np.searchsorted(ps, np.arange(N + 1)),
                lp=b['lp'], ls=b['ls'], coef=coef, link_start=np.searchsorted(b['lp'], np.arange(M + 1)),
                fs=b['fs'], fp=b['fp'], fw=fwn, feed_start=np.searchsorted(b['fs'], np.arange(N + 1)),
                skyVis=b['skyVis'].astype(f32), face=b['face'])


def seg_sum(vals, start, n):
    """a GPU 'one thread per row' loop: row i sums vals[start[i]:start[i+1]] (float32)"""
    out = np.zeros((n,) + vals.shape[1:], np.float32)
    nz = np.nonzero(start[1:] > start[:-1])[0]
    if len(nz):
        out[nz] = np.add.reduceat(vals, start[:-1][nz], axis=0)
    return out


def shadow_map(pc, sto, res=768):
    """orthographic depth map along the sun, rays per texel (stands in for the GPU's depth raster)"""
    sto = sto / np.linalg.norm(sto)
    up = np.array([0, 0, 1.0]) if abs(sto[2]) < 0.9 else np.array([1.0, 0, 0])
    u = np.cross(up, sto); u /= np.linalg.norm(u)
    v = np.cross(sto, u)
    pts = pc['O'].astype(float)
    pu, pv = pts @ u, pts @ v
    lo_u, hi_u, lo_v, hi_v = pu.min() - 10, pu.max() + 10, pv.min() - 10, pv.max() + 10
    gu = lo_u + (np.arange(res) + 0.5) * (hi_u - lo_u) / res
    gv = lo_v + (np.arange(res) + 0.5) * (hi_v - lo_v) / res
    UU, VV = np.meshgrid(gu, gv, indexing='ij')
    far = 20000.0
    O = UU.reshape(-1, 1) * u + VV.reshape(-1, 1) * v + sto * far
    T, _, _ = trace(O, np.repeat(-sto[None], len(O), 0))
    depth = T.reshape(res, res)
    return dict(u=u, v=v, sto=sto, lo=(lo_u, lo_v), step=((hi_u - lo_u) / res, (hi_v - lo_v) / res),
                depth=depth, far=far, res=res)


def sun_visibility(pc, sto, mode, red=''):
    sto = np.asarray(sto, float)
    sto = sto / np.linalg.norm(sto)
    nl = np.maximum(pc['nrm'].astype(float) @ sto, 0)
    vis = np.zeros(pc['N'], bool)
    idx = np.nonzero(nl > 0)[0]
    if red == 'noshadow':
        vis[idx] = True
        return vis
    if mode == 'ray':    # GPU ray query against the soup's BVH, re-run when the sun moves
        vis[idx] = ~blocked(pc['O'][idx], pc['O'][idx] + sto[None] * 400000.0)
        return vis
    sm = shadow_map(pc, sto)
    p = pc['O'][idx]
    iu = np.clip(((p @ sm['u'] - sm['lo'][0]) / sm['step'][0]).astype(int), 0, sm['res'] - 1)
    iv = np.clip(((p @ sm['v'] - sm['lo'][1]) / sm['step'][1]).astype(int), 0, sm['res'] - 1)
    dz = sm['far'] - p @ sm['sto']                     # this point's depth from the map's plane
    bias = 3.0 + 2.0 * max(sm['step'])                 # constant + one texel's slope
    vis[idx] = dz <= sm['depth'][iu, iv] + bias
    return vis


def relight(pc, st, red='', sun_mode='ray', passes=0, B_hist=None, frames=0, robin=1, stop=1e-5):
    """the GPU plan in float32. passes=0: until settled (or CAP); frames>0: amortized, one pass a frame from
    B_hist (warm start), probes regathered 1/robin per frame; returns per-frame log too"""
    f32 = np.float32
    N, M = pc['N'], pc['M']
    pos, nrm, alb = pc['pos'], pc['nrm'], pc['alb']
    # kernel 1: direct, per surfel over its visible pairs (curve, facing, cone live)
    s, k = pc['pair_s'], pc['pair_l']
    contrib = np.zeros((len(s), 3), f32)
    for li, l in enumerate(st['lights']):
        m = k == li
        if not m.any():
            continue
        col = (lin(l['rgb']) * l['dimmer']).astype(f32)
        v = np.asarray(l['pos'], f32)[None] - pos[s[m]]
        d = np.linalg.norm(v, axis=1)
        Lto = v / d[:, None]
        nl = np.maximum((nrm[s[m]] * Lto).sum(1), 0)
        rad = min(float(l['radius']), float(LIGHTS0[li]['radius']))   # the pairs reach the baked radius only
        a = (atten(d, rad) * cone(Lto.astype(float), l) * nl).astype(f32)
        contrib[m] = a[:, None] * col[None]
    E = seg_sum(contrib, pc['pair_start'], N)
    if st['sun'] is not None:
        sto = np.asarray(st['sun']['to'], float)
        sto = sto / np.linalg.norm(sto)
        vis = sun_visibility(pc, sto, sun_mode, red)
        nl = np.maximum(nrm.astype(float) @ sto, 0) * vis
        E = E + (nl[:, None] * np.asarray(st['sun']['rgb'], float)[None]).astype(f32)
    B1 = (alb * E).astype(f32)
    Sky = sky_cube(pc['skyVis'].astype(float), st['amb']).astype(f32)
    n2 = nrm * nrm
    pick = np.stack([np.where(nrm[:, a] >= 0, 2 * a, 2 * a + 1) for a in range(3)], 1)
    owner = pc['fs']

    def gather(B, only=None):   # kernel 3: per probe over its links
        vals = pc['coef'][:, :, None] * B[pc['ls']][:, None, :]
        return seg_sum(vals, pc['link_start'], M) + Sky

    def feed(Ep):               # kernel 4: per surfel over its probes, the n^2 blend of the facing axes
        e = np.zeros((len(owner), 3), f32)
        for a in range(3):
            e += n2[owner, a:a + 1] * Ep[pc['fp'], pick[owner, a]]
        return seg_sum(pc['fw'][:, None] * e, pc['feed_start'], N)
    log = []
    if frames > 0:
        B = B_hist.astype(f32)
        Ep = gather(B)
        for fr in range(frames):
            Bn = (B1 + alb * feed(Ep) / f32(math.pi)).astype(f32)
            ch = float(np.abs(Bn - B).max())
            B = Bn
            En = gather(B)
            if robin > 1:
                sel = (np.arange(M) % robin) == (fr % robin)
                Ep = np.where(sel[:, None, None], En, Ep)
            else:
                Ep = En
            log.append(ch / max(float(B.max()), 1e-30))
            if red == 'nobounce':
                B, Ep = B1, gather(B1)
                break
            if ch <= stop * float(B.max()) and fr >= robin:
                break
        return dict(B=B, E=Ep, passes=len(log), log=log)
    B, Ep, n = B1.copy(), gather(B1), 1
    cap = 1 if red == 'nobounce' else (passes if passes > 0 else CAP)
    for _ in range(2, cap + 1):
        Bn = (B1 + alb * feed(Ep) / f32(math.pi)).astype(f32)
        ch = float(np.abs(Bn - B).max())
        B, Ep, n = Bn, gather(Bn), n + 1
        if passes <= 0 and ch <= SETTLE * float(B.max()):
            break
    return dict(B=B, E=Ep, passes=n, B1=B1)


# ------------------------------------------------------------------ the checks
def cmp(tag, rt, rb, extra=''):
    dB = float(np.abs(rt['B'] - rb['B']).max()) / max(float(rb['B'].max()), 1e-30)
    dE = float(np.abs(rt['E'] - rb['E']).max()) / max(float(rb['E'].max()), 1e-30)
    ok = dB <= TOL and dE <= TOL and rt['passes'] == rb['passes']
    bounce = float((rb['B'] - rb['B1']).sum() / max(rb['B'].sum(), 1e-30))
    return ok, ('%s %s max |dB| %.2e, max |dE| %.2e of the brightest (bar %.0e); passes relight %d rebake %d; '
                'bounce share of the rebake %.1f%%%s' % (tag, 'PASS' if ok else 'FAIL', dB, dE, TOL, rt['passes'],
                                                          rb['passes'], 100 * bounce, extra))


HOURS = [  # (label, sun dir to, sun rgb linear, six sky colours +X -X +Y -Y +Z -Z linear)
    ('08:00', (0.55, -0.70, 0.35), (2.2, 1.6, 1.0), [(0.20, 0.18, 0.16), (0.12, 0.12, 0.14), (0.10, 0.11, 0.13),
                                                     (0.22, 0.19, 0.15), (0.30, 0.34, 0.40), (0.06, 0.05, 0.04)]),
    ('12:00', (0.10, -0.55, 0.83), (3.2, 3.0, 2.7), [(0.25, 0.27, 0.30), (0.25, 0.27, 0.30), (0.22, 0.25, 0.30),
                                                     (0.26, 0.28, 0.31), (0.45, 0.52, 0.62), (0.09, 0.08, 0.07)]),
    ('17:30', (-0.60, -0.68, 0.30), (2.0, 1.2, 0.6), [(0.08, 0.08, 0.10), (0.24, 0.17, 0.12), (0.10, 0.10, 0.12),
                                                      (0.20, 0.15, 0.11), (0.25, 0.26, 0.33), (0.05, 0.04, 0.04)]),
]


def run(red=''):
    lines, ok_all = [], True
    t0 = time.time()
    b = bake()
    pc = precompute(b)
    base = state()

    def rec(ok, line):
        nonlocal ok_all
        ok_all &= ok
        lines.append(line)
        print(line, flush=True)
    # 0: the rebake reproduces the bake
    b2 = bake()
    same = all(np.array_equal(b[k], b2[k]) for k in ('pos', 'nrm', 'alb', 'lp', 'ls', 'w', 'fs', 'fp', 'fw', 'skyVis'))
    rec(same, '0 bake %s %d surfels, %d probes, %d links (%.0f a probe, unlinked mean %.3f), %d feed entries '
        '(%.1f a surfel), %d visible surfel-light pairs (%.1f a surfel), blend radius %.0f u'
        % ('PASS' if same else 'FAIL', b['N'], b['M'], len(b['lp']), len(b['lp']) / b['M'], b['unl'].mean(),
           len(b['fs']), len(b['fs']) / b['N'], len(pc['pair_s']), len(pc['pair_s']) / b['N'], b['radius']))
    # A toggle
    st = state(); st['lights'][0]['dimmer'] = 0.0
    rec(*cmp('A toggle (light 0 off)', relight(pc, st, red), rebake(st)))
    # B colour
    st = state(); st['lights'][1]['rgb'] = (255, 60, 40)
    rec(*cmp('B colour (light 1 -> red)', relight(pc, st, red), rebake(st)))
    # C flicker
    worst, okc, pr = 0.0, True, []
    for fr, f in enumerate((0.35, 1.25, 0.8, 0.05)):
        st = state(); st['lights'][2]['dimmer'] = LIGHTS0[2]['dimmer'] * f
        ok, line = cmp('C', relight(pc, st, red), rebake(st))
        okc &= ok
        pr.append(line.split(' ', 2)[2].split(';')[0])
    rec(okc, 'C flicker (light 2 dimmer x 0.35, 1.25, 0.8, 0.05) %s: %s' % ('PASS' if okc else 'FAIL', ' | '.join(pr)))
    # D radius
    st = state(); st['lights'][3]['radius'] = LIGHTS0[3]['radius'] * 0.7
    rec(*cmp('D radius (spot light 3 radius x 0.7)', relight(pc, st, red), rebake(st)))
    # E sun by rays, S sun by shadow map
    oke, oks, pe, ps_ = True, True, [], []
    for lab, sto, srgb, amb in HOURS:
        st = state(sun=dict(to=sto, rgb=srgb), amb=amb)
        rb = rebake(st)
        ok, line = cmp('E', relight(pc, st, red, 'ray'), rb)
        oke &= ok
        pe.append(lab + ' ' + line.split(' ', 2)[2].split(';')[0] + ';' + line.split(';')[1])
        vis = sun_visibility(pc, np.asarray(sto, float), 'map', red)
        facing = np.asarray(pc['nrm'], float) @ (np.asarray(sto) / np.linalg.norm(sto)) > 0
        flips = int(np.sum(vis[facing] != rb['sun_vis'][facing]))
        rt = relight(pc, st, red, 'map')
        tot = abs(float(rt['B'].sum()) / float(rb['B'].sum()) - 1)
        ok_s = flips <= 0.02 * facing.sum() and tot <= 0.01
        oks &= ok_s
        ps_.append('%s %d of %d sun-facing surfels change lit state (%.2f%%; rays: %d lit), total B %+.3f%%'
                   % (lab, flips, int(facing.sum()), 100 * flips / max(facing.sum(), 1), int(rb['sun_vis'].sum()),
                      100 * (float(rt['B'].sum()) / float(rb['B'].sum()) - 1)))
    rec(oke, 'E sun-ray (3 hours) %s: %s' % ('PASS' if oke else 'FAIL', ' | '.join(pe)))
    rec(oks, 'S sun-map (768^2 map, bias 3 u + a texel) %s (bar: <= 2%% flips, |total B| <= 1%%): %s'
        % ('PASS' if oks else 'FAIL', ' | '.join(ps_)))
    # L linearity (pinned passes, no sun / sky): the per-light basis option
    s1 = state(); s2 = state()
    for i, l in enumerate(s1['lights']):
        l['dimmer'] = l['dimmer'] if i % 2 == 0 else 0.0
    for i, l in enumerate(s2['lights']):
        l['dimmer'] = l['dimmer'] if i % 2 == 1 else 0.0
    r1, r2, r12 = (relight(pc, s, red, passes=16) for s in (s1, s2, base))
    lin_err = float(np.abs(r1['B'] + r2['B'] - r12['B']).max() / r12['B'].max())
    ok = lin_err <= 1e-5
    rec(ok, 'L linear %s relight(even lights) + relight(odd lights) vs relight(all), 16 passes: %.2e of the brightest '
        '(bar 1e-05)' % ('PASS' if ok else 'FAIL', lin_err))
    # G amortized: the base state settled, then light 0 off; one pass a frame from history
    tight_base = rebake(base, settle=1e-7, cap=400)
    st = state(); st['lights'][0]['dimmer'] = 0.0
    tight = rebake(st, settle=1e-7, cap=400)
    am = relight(pc, st, red, B_hist=tight_base['B'], frames=200)
    dG = float(np.abs(am['B'] - tight['B']).max() / tight['B'].max())
    f3 = next((i + 1 for i, c in enumerate(am['log']) if c <= SETTLE), -1)
    ok = dG <= 2e-4 and am['passes'] < 200
    rec(ok, 'G amortize (one pass a frame, warm start, light 0 off) %s: |dB| %.2e of the brightest vs the rebake settled '
        'to 1e-7 (bar 2e-4); change under 1e-3 after %d frames, under 1e-5 after %d (tight rebake %d passes)'
        % ('PASS' if ok else 'FAIL', dG, f3, am['passes'], tight['passes']))
    rr = relight(pc, st, red, B_hist=tight_base['B'], frames=400, robin=4)
    dR = float(np.abs(rr['B'] - tight['B']).max() / tight['B'].max())
    f3 = next((i + 1 for i, c in enumerate(rr['log']) if c <= SETTLE), -1)
    ok = dR <= 2e-4 and rr['passes'] < 400
    rec(ok, 'R robin (a quarter of the probes regathered a frame) %s: |dB| %.2e of the brightest (bar 2e-4); change under '
        '1e-3 after %d frames, under 1e-5 after %d' % ('PASS' if ok else 'FAIL', dR, f3, rr['passes']))
    # timing of the float32 relight on this CPU, and the counts the budget scales from
    t1 = time.time()
    for _ in range(3):
        relight(pc, base, passes=8)
    ms = (time.time() - t1) / 3 * 1000
    print('info: numpy relight (8 passes, %d surfels, %d probes) %.1f ms on this CPU; whole run so far %.1f s'
          % (b['N'], b['M'], ms, time.time() - t0))
    return ok_all, b, pc


def budget(b, pc):
    """bytes and threads per frame for a mid GPU; per-element counts from this scene, scaled"""
    links_pp = len(b['lp']) / b['M']
    feed_ps = len(b['fs']) / b['N']
    pairs_ps = len(pc['pair_s']) / b['N']
    print('info: budget per-element counts from the twin: links/probe %.0f (cap %d), feed/surfel %.1f, '
          'visible pairs/surfel %.1f (K=%d)' % (links_pp, LINK_CAP, feed_ps, pairs_ps, len(LIGHTS0)))
    BW = 224e9 * 0.35    # a mid GPU's DRAM bandwidth (224 GB/s class) at 35% for scattered reads
    launch = 10e-6
    print('info: budget model (mid GPU 224 GB/s at 35%% = %.0f GB/s effective, %d us a dispatch):' % (BW / 1e9, launch * 1e6))
    for N, M, K, lpp, fps, pps in ((30000, 1000, 64, 512, 8, 6), (100000, 2500, 128, 512, 8, 8),
                                    (300000, 5000, 256, 768, 10, 10)):
        direct = N * (20 + 8) + N * pps * 4 + K * 64
        gather = M * lpp * (8 + 8) + M * 6 * 8
        feed = N * fps * (4 + 2 + 24) + N * 8
        per_pass = gather + feed
        t_direct = direct / BW + launch
        t_pass = per_pass / BW + 2 * launch
        print('info:   N=%6d M=%5d K=%3d links/probe=%d: direct %.3f ms, one bounce pass %.3f ms, settled (8 passes) '
              '%.2f ms, amortized (1 pass a frame) %.3f ms; resident %.1f MB'
              % (N, M, K, lpp, t_direct * 1e3, t_pass * 1e3, (t_direct + 8 * t_pass) * 1e3,
                 (t_direct + t_pass) * 1e3, (direct + gather + feed + N * 12) / 1e6))


def main():
    red = os.environ.get('GPURELIGHT1_RED', '')
    if red:
        print('RED %s: the checks below must FAIL' % red)
        ok, _, _ = run(red)
        print('gpurelight1 %s (red %s)' % ('PASS' if ok else 'FAIL', red))
        sys.exit(0 if ok else 1)
    ok, b, pc = run()
    budget(b, pc)
    # the reds, self-tested: each must turn its checks FAIL
    reds_ok = True
    for r, must in (('nobounce', ('A', 'B', 'C', 'D', 'E', 'G')), ('noshadow', ('S',))):
        print('--- red %s (must FAIL %s)' % (r, ' '.join(must)))
        import io
        import contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            run(r)
        out = buf.getvalue().splitlines()
        for line in out:
            if line[:1] in 'ABCDEGLRS0' and (' PASS' in line or ' FAIL' in line):
                print('    ' + line[:200])
        failed = {line[0] for line in out if ' FAIL' in line[:60]}
        good = all(m in failed for m in must)
        reds_ok &= good
        print('red %s: %s' % (r, ('FAILS as required (%s)' % ' '.join(sorted(failed))) if good else
                               'DID NOT FAIL %s -- the checks are blind' % ' '.join(m for m in must if m not in failed)))
    print('gpurelight1 %s' % ('PASS' if ok and reds_ok else 'FAIL'))
    sys.exit(0 if ok and reds_ok else 1)


if __name__ == '__main__':
    main()
