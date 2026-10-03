#!/usr/bin/env python3
"""SMOOTHN1 twin: noise-driven extra rays and neighbor sharing for a per-surfel bake
(docs/cloud/SMOOTHN1_DESIGN.md). Standalone: python3 + numpy, synthetic scene built here, no game data.

  smoothn1_check.py                 green run, then every red; exit 0 only if green passes and every red fails
  SMOOTHN1_RED=<name> smoothn1_check.py   one variant only (exit 0 if it passes, 1 if it fails)
      reds: corner (normal weight off), wall (depth test off), speed (easy surfels drop below the floor)
  SMOOTHN1_REPEATS=<n>              independent repeats per bake (default 16)

  SMOOTHN1_WORKERS=<n>              worker processes (default min(4, cpus)); 1 = serial
  SMOOTHN1_DEPTH=auto|gical1|standin  the depth test: GICAL1's own functions (tests/spells/gical1_check.py, lane
                                    GICAL1, imported) when present, else this file's stand-in (default auto)

The scene: two closed box rooms side by side, split by a 0.1 m wall (one sharing grid column straddles it).
  Room A (HARD): dim faces (radiance 0.2), one small bright window (0.4 x 0.4 m, radiance 600) high on the north
                 wall near the dividing wall, and a sun pool on the floor (1 x 1 m, radiance 40) at the dividing
                 wall's foot. Most rays from most surfels miss the window.
  Room B (EASY): an open lit room: ceiling 60, walls 10, floor 5. Every ray sees light.
A ray's value is the radiance of what it hits (a one-bounce gather of a known radiance field, as a surfel gather
reads the relight's surfel radiance). The estimate per surfel is the cosine-weighted mean incoming radiance E/pi.

Checks (one line each, PASS/FAIL): R reference; G1 never fewer than the floor (and never over the cap); G2 no surfel
worse than the fixed bake (per-surfel paired test over the repeats); G3 the hard room clearly better; G4 no sharing
weight across the wall or between faces at 90 degrees; G5 sharing makes no surfel worse than its own rays alone;
G6 room B switched off moves no room A estimate (same rays); G7 the floor's sun pool switched off moves no room A
floor estimate (same rays). Reds: corner must FAIL G7, wall must FAIL G6, speed must FAIL G1.

Reference: EXACT, from Lambert's polygon formula (each room is convex and has no occluder inside, so every face is
fully seen). It has no noise. A very-high-ray-count Monte Carlo of 256 surfels (65536 rays each) checks the
analytic code against the tracer, with its own measured standard error.

Counts: the twin's floor F = 256 stands for today's 2048 rays (src/probebake.h:39); every twin count x 8 = the
production count. Cap C = 16 F (production 32768).
"""
import math
import os
import sys
import time

import numpy as np

# ---------------------------------------------------------------- the depth test: GICAL1's own, or the stand-in
# SMOOTHN1_DEPTH=auto (default: GICAL1's functions from tests/spells/gical1_check.py when importable, else the
# stand-in), gical1 (refuse to run without them), standin (this file's own minimal version).
DEPTH_WANT = os.environ.get('SMOOTHN1_DEPTH', 'auto')
G1 = None
if DEPTH_WANT in ('auto', 'gical1'):
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import gical1_check as G1      # noqa: E402  (lane GICAL1, origin/cloud-GICAL1 30201d7, copied byte for byte)
    except ImportError:
        if DEPTH_WANT == 'gical1':
            raise
        G1 = None
DEPTH_MODE = 'gical1' if G1 is not None else 'standin'
UNIT_M = 0.0142875       # one game unit in meters (1.42875 cm): GICAL1's LIFT and normal bias are 1 unit

# ---------------------------------------------------------------- pre-registered constants (set before the first run)
F = 256                 # the floor: today's fixed count, scaled (2048 / 8)
CAP = 16 * F            # hard cap
BATCH = F               # extra rays come in batches of F
EPS_REL = 0.05          # stop when SE <= max(EPS_REL * mean, TAU_ABS)
TAU_FRAC = 0.01         # TAU_ABS = TAU_FRAC x the cell's median surfel mean after the floor pass
SPACING = 0.25          # surfel lattice spacing (m)
DIAM = 2 * SPACING      # surfel diameter: the depth map clamps hit distances to it
R_SHARE = DIAM          # sharing radius (<= DIAM so the clamped depth test can see the whole radius)
GRID = 1.0              # sharing grid cell (m)
GRID_ORIGIN = 0.5       # chosen so one grid column straddles the dividing wall (the worst case, on purpose)
NORMAL_POW = 8          # normal agreement weight max(0, ni.nj)^8: 0 at 90 degrees
BIAS = 0.05 * DIAM      # query point lifted off the neighbor's surface along its normal
VIS_POW = 3             # Chebyshev result cubed (DDGI-style sharpening)
VIS_CUT = 0.05          # a pair whose visibility is under this shares nothing (blocked means zero, not "a little")
DM_RES = 4              # depth map 4 x 4 texels over the hemisphere
Z_GATE = 4.5            # one-sided z for "no surfel worse" (Bonferroni over ~3400 surfels: ~0.01 false alarms)
HARD_RATIO = 0.5        # room A's total squared error, adaptive / fixed, must be <= this
SEED = 20261003
ISO_REPEATS = 2         # repeats of the isolation checks (deterministic: same rays with the light on and off)
ISO_TOL = 1e-9          # relative change allowed there (floating point only)
WORKERS = int(os.environ.get('SMOOTHN1_WORKERS', str(min(4, os.cpu_count() or 1))))
EPS_ORIGIN = 1e-6

L_WIN = 600.0
L_SUN = 40.0
ROOMS = [  # lo, hi, radiance per face (-x +x -y +y -z +z); patches: (face, center (u, v), half size (u, v), radiance,
           # is a hole with no surfels)
    dict(name='A', lo=np.array([0.0, 0.0, 0.0]), hi=np.array([6.0, 4.0, 3.0]),
         L=np.array([0.2, 0.2, 0.2, 0.2, 0.2, 0.2]),
         patches=[(3, np.array([2.2, 4.5]), np.array([0.2, 0.2]), L_WIN, True),      # the window, north wall
                  (4, np.array([5.5, 1.5]), np.array([0.5, 0.5]), L_SUN, False)]),   # a sun pool on the floor at the
                                                                                       # dividing wall's foot
    dict(name='B', lo=np.array([6.1, 0.0, 0.0]), hi=np.array([12.1, 4.0, 3.0]),
         L=np.array([10.0, 10.0, 10.0, 10.0, 5.0, 60.0]), patches=[]),
]
# the isolation checks switch lights off without touching the rays: room_scale per room, patch_scale per (room, patch)
LIGHT = dict(room_scale=[1.0, 1.0], patch_scale={})
FACE_N = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)  # inward normals
#   face k: axis k//2, at lo (k even) or hi (k odd); inward normal +axis for lo, -axis for hi
FACE_UV = {0: (1, 2), 1: (2, 0), 2: (0, 1)}   # in-face coordinates per axis


# ---------------------------------------------------------------- the scene
def build_surfels():
    P, N, room, face = [], [], [], []
    for ri, R in enumerate(ROOMS):
        for k in range(6):
            ax = k // 2
            ua, va = FACE_UV[ax]
            us = np.arange(R['lo'][ua] + SPACING / 2, R['hi'][ua], SPACING)
            vs = np.arange(R['lo'][va] + SPACING / 2, R['hi'][va], SPACING)
            U, V = np.meshgrid(us, vs, indexing='ij')
            U, V = U.ravel(), V.ravel()
            for pk, c, h, _, hole in R['patches']:
                if pk == k and hole:
                    keep = ~((np.abs(U - c[0]) < h[0]) & (np.abs(V - c[1]) < h[1]))
                    U, V = U[keep], V[keep]
            p = np.zeros((U.size, 3))
            p[:, ua], p[:, va] = U, V
            p[:, ax] = R['lo'][ax] if k % 2 == 0 else R['hi'][ax]
            p += FACE_N[k] * EPS_ORIGIN
            P.append(p)
            N.append(np.repeat(FACE_N[k][None], U.size, 0))
            room.append(np.full(U.size, ri))
            face.append(np.full(U.size, k))
    return np.concatenate(P), np.concatenate(N), np.concatenate(room), np.concatenate(face)


def frames(N):
    a = np.where(np.abs(N[:, :1]) < 0.9, np.array([[1.0, 0, 0]]), np.array([[0, 1.0, 0]]))
    t1 = np.cross(a, N)
    t1 /= np.linalg.norm(t1, axis=1, keepdims=True)
    t2 = np.cross(N, t1)
    return t1, t2


def radiance_at(ri, k, hit):
    """radiance of room ri's face k at hit points (n x 3)"""
    R = ROOMS[ri]
    L = np.full(len(hit), R['L'][k] * LIGHT['room_scale'][ri])
    for pi, (pk, c, h, Lp, _) in enumerate(R['patches']):
        if pk == k:
            Lp = R['L'][k] * LIGHT['room_scale'][ri] + (Lp - R['L'][k]) * LIGHT['patch_scale'].get((ri, pi), 1.0)
            ua, va = FACE_UV[pk // 2]
            inp = (np.abs(hit[:, ua] - c[0]) < h[0]) & (np.abs(hit[:, va] - c[1]) < h[1])
            L = np.where(inp, Lp, L)
    return L


def trace(o, d, ri):
    """o, d: n x 3 rays starting inside room(s) ri (n,). Returns radiance, hit distance."""
    lo = np.stack([ROOMS[r]['lo'] for r in range(len(ROOMS))])[ri]
    hi = np.stack([ROOMS[r]['hi'] for r in range(len(ROOMS))])[ri]
    with np.errstate(divide='ignore', invalid='ignore'):
        t = np.where(d > 0, (hi - o) / d, np.where(d < 0, (lo - o) / d, np.inf))
    ax = np.argmin(t, axis=1)
    tt = t[np.arange(len(t)), ax]
    k = ax * 2 + (d[np.arange(len(d)), ax] > 0)
    hit = o + d * tt[:, None]
    L = np.empty(len(o))
    for r in range(len(ROOMS)):
        m = ri == r
        if m.any():
            L[m] = np.choose(k[m], [radiance_at(r, kk, hit[m]) for kk in range(6)])
    return L, tt


def cos_dirs(u1, u2):
    r = np.sqrt(u1)
    ph = 2 * np.pi * u2
    return np.stack([r * np.cos(ph), r * np.sin(ph), np.sqrt(np.maximum(0.0, 1 - u1))], -1)


def hemi_texel(loc):
    s = np.abs(loc[:, 0]) + np.abs(loc[:, 1]) + np.maximum(loc[:, 2], 0)
    s = np.maximum(s, 1e-12)
    u, v = loc[:, 0] / s, loc[:, 1] / s
    a, b = u + v, u - v
    ia = np.clip(((a + 1) * 0.5 * DM_RES).astype(int), 0, DM_RES - 1)
    ib = np.clip(((b + 1) * 0.5 * DM_RES).astype(int), 0, DM_RES - 1)
    return ia * DM_RES + ib


# ---------------------------------------------------------------- the exact reference (Lambert's polygon formula)
def face_polys(ri):
    R = ROOMS[ri]
    out = []
    for k in range(6):
        ax = k // 2
        ua, va = FACE_UV[ax]
        w = R['lo'][ax] if k % 2 == 0 else R['hi'][ax]

        def quad(u0, u1, v0, v1):
            q = np.zeros((4, 3))
            q[:, ax] = w
            q[:, ua] = [u0, u1, u1, u0]
            q[:, va] = [v0, v0, v1, v1]
            return q
        out.append((k, R['L'][k], quad(R['lo'][ua], R['hi'][ua], R['lo'][va], R['hi'][va])))
        for pk, c, h, Lp, _ in R['patches']:
            if pk == k:
                out.append((k, Lp - R['L'][k], quad(c[0] - h[0], c[0] + h[0], c[1] - h[1], c[1] + h[1])))
    return out


def form_factor(p, n, poly):
    """point (m x 3) with normal (m x 3) to a convex polygon in front of it: (1/2pi) |sum gamma_k n.unit(v_k x v_k+1)|"""
    v = poly[None, :, :] - p[:, None, :]
    v /= np.linalg.norm(v, axis=2, keepdims=True)
    tot = np.zeros(len(p))
    for i in range(len(poly)):
        a, b = v[:, i], v[:, (i + 1) % len(poly)]
        c = np.cross(a, b)
        cn = np.linalg.norm(c, axis=1)
        g = np.arctan2(cn, np.sum(a * b, 1))
        tot += g * np.sum(n * c, 1) / np.maximum(cn, 1e-15)
    return np.abs(tot) / (2 * np.pi)


def exact_reference(P, N, room, face):
    ref = np.zeros(len(P))
    for ri in range(len(ROOMS)):
        m = room == ri
        for k, L, poly in face_polys(ri):
            mm = m & (face != k)    # a surfel's own face is behind its plane
            ref[mm] += L * form_factor(P[mm], N[mm], poly)
    return ref


# ---------------------------------------------------------------- the bake
class Scene:
    def __init__(self):
        self.P, self.N, self.room, self.face = build_surfels()
        self.n = len(self.P)
        self.t1, self.t2 = frames(self.N)
        self.ref = exact_reference(self.P, self.N, self.room, self.face)
        g = np.floor((self.P - GRID_ORIGIN) / GRID).astype(np.int64)
        self.cell = (g[:, 0] * 1000 + g[:, 1]) * 1000 + g[:, 2]
        # same-cell neighbor pairs within the sharing radius (both orders)
        order = np.argsort(self.cell, kind='stable')
        cs = self.cell[order]
        starts = np.flatnonzero(np.r_[True, cs[1:] != cs[:-1]])
        ends = np.r_[starts[1:], len(cs)]
        I, J = [], []
        for s, e in zip(starts, ends):
            idx = order[s:e]
            ii, jj = np.meshgrid(idx, idx, indexing='ij')
            ii, jj = ii.ravel(), jj.ravel()
            keep = (ii != jj) & (np.linalg.norm(self.P[ii] - self.P[jj], axis=1) <= R_SHARE)
            I.append(ii[keep])
            J.append(jj[keep])
        self.I, self.J = np.concatenate(I), np.concatenate(J)
        d = np.linalg.norm(self.P[self.I] - self.P[self.J], axis=1)
        self.w_dist = (1 - (d / R_SHARE) ** 2) ** 2
        self.w_norm = np.maximum(0.0, np.sum(self.N[self.I] * self.N[self.J], 1)) ** NORMAL_POW
        self.ndot = np.sum(self.N[self.I] * self.N[self.J], 1)

    def rays(self, rep, rnd):
        """the uniforms of round rnd for EVERY surfel (so a surfel's stream does not depend on who else is active)"""
        g = np.random.default_rng([SEED, rep, rnd])
        return g.random((self.n, BATCH, 2))


class Acc:
    def __init__(self, n):
        self.cnt = np.zeros(n, np.int64)
        self.s1 = np.zeros(n)
        self.s2 = np.zeros(n)
        self.dm = np.zeros((n, DM_RES * DM_RES, 3))   # count, sum min(d, D), sum min(d, D)^2

    def mean(self):
        return self.s1 / np.maximum(self.cnt, 1)

    def var(self):
        m = self.mean()
        return np.maximum(self.s2 / np.maximum(self.cnt, 1) - m * m, 0) * self.cnt / np.maximum(self.cnt - 1, 1)


def trace_batch(S, acc, idx, U, nrays):
    """trace the first nrays of uniforms U (n x BATCH x 2) for surfels idx; accumulate"""
    CH = max(1, 1_000_000 // nrays)
    for c0 in range(0, len(idx), CH):
        ii = idx[c0:c0 + CH]
        u = U[ii, :nrays].reshape(-1, 2)
        loc = cos_dirs(u[:, 0], u[:, 1])
        s = np.repeat(ii, nrays)
        d = loc[:, :1] * S.t1[s] + loc[:, 1:2] * S.t2[s] + loc[:, 2:3] * S.N[s]
        L, t = trace(S.P[s], d, S.room[s])
        acc.cnt[ii] += nrays
        acc.s1 += np.bincount(s, L, S.n)
        acc.s2 += np.bincount(s, L * L, S.n)
        tex = s * DM_RES * DM_RES + hemi_texel(loc)
        tc = np.minimum(t, DIAM)
        nt = S.n * DM_RES * DM_RES
        acc.dm[..., 0] += np.bincount(tex, None, nt).reshape(S.n, -1)
        acc.dm[..., 1] += np.bincount(tex, tc, nt).reshape(S.n, -1)
        acc.dm[..., 2] += np.bincount(tex, tc * tc, nt).reshape(S.n, -1)


# ---- stand-in for GICAL1's test; replace with GICAL1's when merged --------------------------------------------
def gical1_visibility(S, acc, i, q):
    """visibility of world points q (m x 3) from surfels i (m,), through surfel i's radial depth map.
    Assumed interface (docs/cloud/SMOOTHN1_DESIGN.md): 4 x 4 hemi-octahedral texels in the surfel's frame, each
    holding the mean and mean square of hit distances clamped to the surfel diameter; Chebyshev's bound
    p = var / (var + (t - mean)^2) when t > mean, else 1; an empty texel or a point behind the surfel = 0."""
    v = q - S.P[i]
    t = np.linalg.norm(v, axis=1)
    vn = v / np.maximum(t, 1e-12)[:, None]
    loc = np.stack([np.sum(vn * S.t1[i], 1), np.sum(vn * S.t2[i], 1), np.sum(vn * S.N[i], 1)], -1)
    tex = hemi_texel(loc)
    c = acc.dm[i, tex, 0]
    m1 = acc.dm[i, tex, 1] / np.maximum(c, 1)
    m2 = acc.dm[i, tex, 2] / np.maximum(c, 1)
    var = np.maximum(m2 - m1 * m1, (0.01 * DIAM) ** 2)
    tc = np.minimum(t, DIAM)
    p = np.where(tc <= m1, 1.0, var / (var + (tc - m1) ** 2))
    p = np.where((c > 0) & (loc[:, 2] > 0), p, 0.0)
    return p ** VIS_POW
# ---- end of the stand-in ---------------------------------------------------------------------------------------


def gical1_maps(S):
    """GICAL1's depth maps for every surfel, built its way (doc 3.x of GICAL1_DESIGN.md): its own stratified depth
    rays (8 x 8 texels x 8 rays, horizon warp 4) from p + n LIFT, distances clamped to D = the surfel diameter, the
    map passed through its 16-bit record. Geometry only, so built once per scene (fixed seed)."""
    rng = np.random.default_rng([SEED, 7])
    lift = G1.LIFT * UNIT_M
    dirs, tex = [], []
    for i in range(S.n):
        d, t = G1.depth_ray_dirs(S.N[i], rng)
        dirs.append(d)
        tex.append(t)
    dirs = np.stack(dirs)
    k = dirs.shape[1]
    s = np.repeat(np.arange(S.n), k)
    _, th = trace(S.P[s] + S.N[s] * lift, dirs.reshape(-1, 3), S.room[s])
    th = th.reshape(S.n, k)
    maps = np.zeros((S.n, G1.SIDE * G1.SIDE, 2))
    for i in range(S.n):
        maps[i] = G1.unpack_depth_record(G1.pack_depth_record(G1.depth_map_from_hits(th[i], tex[i], DIAM), DIAM), DIAM)
    return maps


def gical1_pair_vis(S):
    """GICAL1 3.6 for sharing: both ways, multiplied; each receiver moved 1 unit off its surface along its normal"""
    if getattr(S, 'g1vis', None) is None:
        if getattr(S, 'g1maps', None) is None:
            S.g1maps = gical1_maps(S)
        lift = G1.LIFT * UNIT_M
        qj = S.P[S.J] + UNIT_M * S.N[S.J]
        qi = S.P[S.I] + UNIT_M * S.N[S.I]
        a = G1.depth_visibility(S.P[S.I], S.N[S.I], S.g1maps[S.I], DIAM, qj, lift=lift)
        b = G1.depth_visibility(S.P[S.J], S.N[S.J], S.g1maps[S.J], DIAM, qi, lift=lift)
        S.g1vis = a * b
    return S.g1vis


def pair_weights(S, acc, red):
    """sharing weight of neighbor J for surfel I (both orders listed)"""
    if red == 'wall':
        vis = np.ones(len(S.I))
    elif DEPTH_MODE == 'gical1':
        vis = gical1_pair_vis(S)
    else:
        qj = S.P[S.J] + BIAS * S.N[S.J]
        qi = S.P[S.I] + BIAS * S.N[S.I]
        vis = gical1_visibility(S, acc, S.I, qj) * gical1_visibility(S, acc, S.J, qi)   # both ways, product
    wn = np.ones(len(S.I)) if red == 'corner' else S.w_norm
    return wn * S.w_dist * np.where(vis >= VIS_CUT, vis, 0.0), vis


def neighbor_prior(S, acc, vis):
    """the noise floor of the stop rule, as a RELATIVE variance: the mean over a surfel's same-cell, same-facing,
    visible neighbors of their floor-pass s^2 / mean^2. The stop rule uses max(own s^2, this x own mean^2): it guards
    a surfel whose rays all missed a small light by luck (its own s^2 is then ~0 and it would stop at the floor,
    wrong). Relative, so a bright neighbor's large absolute variance does not keep a dimmer, settled surfel going;
    a mean, because a median is 0 when fewer than half of the neighbors hit the light."""
    m = acc.mean()
    cv2 = acc.var() / np.maximum(m * m, 1e-30)
    ok = (S.ndot > 0.9) & (vis > 0.5)
    num = np.bincount(S.I[ok], cv2[S.J[ok]], S.n)
    cnt = np.bincount(S.I[ok], None, S.n)
    return num / np.maximum(cnt, 1)


def bake(S, rep, mode, red='', tau_fixed=None):
    """mode 'fixed' (F rays each) or 'adaptive' (floor F, extra batches by variance, cap, then sharing)."""
    acc = Acc(S.n)
    U0 = S.rays(rep, 0)
    if mode == 'fixed':
        trace_batch(S, acc, np.arange(S.n), U0, F)
        return acc.mean(), acc.cnt.copy(), dict(shared=0)
    if red == 'speed':
        # the "speed" variant: start at F/4 and let surfels already under the bar stop there
        trace_batch(S, acc, np.arange(S.n), U0, F // 4)
        tau = TAU_FRAC * np.median(acc.mean())
        se = np.sqrt(acc.var() / acc.cnt)
        easy = se <= np.maximum(EPS_REL * acc.mean(), tau)
        rest = np.flatnonzero(~easy)
        acc2 = Acc(S.n)
        trace_batch(S, acc2, rest, U0, F)
        for a in ('cnt', 's1', 's2', 'dm'):
            getattr(acc, a)[rest] = getattr(acc2, a)[rest]
    else:
        trace_batch(S, acc, np.arange(S.n), U0, F)
    tau = TAU_FRAC * np.median(acc.mean()) if tau_fixed is None else tau_fixed
    _, vis = pair_weights(S, acc, '')
    prior = neighbor_prior(S, acc, vis)     # from the floor pass, once
    rnd = 1
    while True:
        sig2 = np.maximum(acc.var(), prior * acc.mean() ** 2)
        se = np.sqrt(sig2 / acc.cnt)
        need = (se > np.maximum(EPS_REL * acc.mean(), tau)) & (acc.cnt < CAP) & (acc.cnt >= F)
        idx = np.flatnonzero(need)
        if len(idx) == 0:
            break
        trace_batch(S, acc, idx, S.rays(rep, rnd), BATCH)
        rnd += 1
    # sharing: only surfels still over the bar at the cap
    w, vis = pair_weights(S, acc, red)
    sig2 = np.maximum(acc.var(), prior * acc.mean() ** 2)
    se = np.sqrt(sig2 / acc.cnt)
    noisy = se > np.maximum(EPS_REL * acc.mean(), tau)
    use = noisy[S.I] & (w > 0)
    I, J, wu = S.I[use], S.J[use], w[use]
    own = acc.mean()
    W = np.bincount(I, wu * acc.cnt[J], S.n)                       # neighbor rays, weighted
    m_nb = np.bincount(I, wu * acc.s1[J], S.n) / np.maximum(W, 1e-300)
    a = sig2 / acc.cnt                                             # own estimate's variance
    b = np.bincount(I, wu * wu * acc.cnt[J] * sig2[J], S.n) / np.maximum(W * W, 1e-300)  # the pool's
    diff2 = (m_nb - own) ** 2                                      # estimates a + b + bias^2
    lam = np.where(W > 0, a / np.maximum(np.maximum(diff2, a + b), 1e-300), 0.0)
    shared = own + lam * (m_nb - own)
    cross = np.sum(w[use & (S.room[S.I] != S.room[S.J])])
    perp = np.sum(w[use & (np.abs(S.ndot) < 0.5)])
    return shared, acc.cnt.copy(), dict(shared=int((W > 0).sum()), tau=tau, rounds=rnd, own=own,
                                        lam_mean=float(lam[W > 0].mean()) if (W > 0).any() else 0.0,
                                           cross=float(cross), perp=float(perp))


# ---------------------------------------------------------------- checks
def mc_reference_check(S, out):
    """very-high-count reference on 256 surfels, checked against the exact one with its own noise"""
    g = np.random.default_rng([SEED, 999])
    pick = np.sort(np.r_[g.choice(np.flatnonzero(S.room == 0), 192, replace=False),
                         g.choice(np.flatnonzero(S.room == 1), 64, replace=False)])
    NR = 65536
    s1 = np.zeros(len(pick))
    s2 = np.zeros(len(pick))
    CHK = 2048
    for c in range(0, NR, CHK):
        u = g.random((len(pick), CHK, 2)).reshape(-1, 2)
        loc = cos_dirs(u[:, 0], u[:, 1])
        s = np.repeat(pick, CHK)
        k = np.repeat(np.arange(len(pick)), CHK)
        d = loc[:, :1] * S.t1[s] + loc[:, 1:2] * S.t2[s] + loc[:, 2:3] * S.N[s]
        L, _ = trace(S.P[s], d, S.room[s])
        s1 += np.bincount(k, L, len(pick))
        s2 += np.bincount(k, L * L, len(pick))
    m = s1 / NR
    se = np.sqrt(np.maximum(s2 / NR - m * m, 0) / (NR - 1))
    z = np.abs(m - S.ref[pick]) / np.maximum(se, 1e-12)
    zmax = float(z.max())
    ok = zmax <= Z_GATE
    relse = se / S.ref[pick]
    out.append(('R reference', ok,
                f'MC {len(pick)} surfels x {NR} rays vs exact: worst |z| {zmax:.2f} (bar {Z_GATE}); MC relative SE '
                f'median {np.median(relse):.4f} max {relse.max():.4f}; exact reference noise 0'))
    return ok


FIXED_CACHE = {}
_S = None       # the scene, inherited by the worker processes (fork)


def _job(args):
    """one bake in a worker: (rep, mode, red, light, tau_fixed) -> (mean, counts, info, seconds)"""
    rep, mode, red, light, tau_fixed = args
    LIGHT['room_scale'] = list(light[0])
    LIGHT['patch_scale'] = dict(light[1])
    t = time.time()
    m, n, inf = bake(_S, rep, mode, red, tau_fixed)
    return m, n, inf, time.time() - t


def run_jobs(jobs):
    if WORKERS <= 1:
        return [_job(j) for j in jobs]
    import multiprocessing as mp
    with mp.get_context('fork').Pool(WORKERS) as pool:
        return pool.map(_job, jobs, chunksize=1)


LIGHT_ON = ((1.0, 1.0), ())


def run_variant(S, red, R):
    t0 = time.time()
    eF, eA, eO, cF, cA, info = [], [], [], [], [], []
    tF = tA = 0.0
    need = [rep for rep in range(R) if rep not in FIXED_CACHE]
    for rep, r in zip(need, run_jobs([(rep, 'fixed', '', LIGHT_ON, None) for rep in need])):
        FIXED_CACHE[rep] = ((r[0], r[1], r[2]), r[3])
    # the adaptive bakes, plus the isolation checks' light-off bakes (same rays, tau pinned to the light-on bake's)
    ad = run_jobs([(rep, 'adaptive', red, LIGHT_ON, None) for rep in range(R)])
    iso_jobs = []
    for rep in range(ISO_REPEATS):
        tau = ad[rep][2]['tau']
        iso_jobs.append((rep, 'adaptive', red, ((1.0, 0.0), ()), tau))          # room B dark
        iso_jobs.append((rep, 'adaptive', red, ((1.0, 1.0), (((0, 1), 0.0),)), tau))   # the floor's sun pool dark
    iso = run_jobs(iso_jobs)
    for rep in range(R):
        (mF, nF, _), tf = FIXED_CACHE[rep]
        mA, nA, inf, ta = ad[rep]
        tF += tf
        tA += ta
        eF.append(mF - S.ref)
        eA.append(mA - S.ref)
        eO.append(inf['own'] - S.ref)
        cF.append(nF)
        cA.append(nA)
        info.append(inf)
    eF, eA, eO, cF, cA = map(np.array, (eF, eA, eO, cF, cA))
    out = []
    # G1 never fewer
    mn = int(cA.min())
    below = int((cA < F).sum())
    out.append(('G1 never-fewer', mn >= F and int(cA.max()) <= CAP,
                f'fewest rays on any surfel in any repeat {mn} (floor {F}); surfel-bakes under the floor {below}; '
                f'most {int(cA.max())} (cap {CAP})'))
    # G2 no surfel worse than the fixed bake; G5 sharing makes no surfel worse than its own rays alone
    msF = (eF ** 2).mean(0)
    msA = (eA ** 2).mean(0)
    msO = (eO ** 2).mean(0)
    for name, eX, msX, what in (('G2 no surfel worse than fixed', eF, msF, 'adaptive/fixed'),
                                ('G5 sharing never hurts', eO, msO, 'shared/own rays')):
        d = eA ** 2 - eX ** 2
        md = d.mean(0)
        sd = d.std(0, ddof=1) / math.sqrt(R)
        # a surfel whose error is the same in every repeat (no ray ever hit anything but one radiance) has sd 0:
        # it is worse only if the difference is more than floating point
        tiny = 1e-12 * (msX + S.ref ** 2)
        z = np.where(sd > tiny, md / np.maximum(sd, 1e-300), np.where(md > tiny, np.inf, 0.0))
        worse = np.flatnonzero(z > Z_GATE)
        ratio = np.sqrt(msA / np.maximum(msX, 1e-300))
        wi = int(np.argmax(z))
        out.append((name, len(worse) == 0,
                    f'{len(worse)} of {S.n} surfels significantly worse (paired z > {Z_GATE} over {R} repeats); '
                    f'largest z {z[wi]:.2f} at room {"AB"[S.room[wi]]} face {S.face[wi]} p=({S.P[wi,0]:.3f},'
                    f'{S.P[wi,1]:.3f},{S.P[wi,2]:.3f}), RMSE {what} there {ratio[wi]:.3f}; '
                    f'worst RMSE ratio anywhere {ratio.max():.3f}'))
    # G3 the hard case gets better
    A = S.room == 0
    B = S.room == 1
    rA = float(msA[A].sum() / msF[A].sum())
    rB = float(msA[B].sum() / msF[B].sum())
    relF = np.sqrt(msF) / S.ref
    relA = np.sqrt(msA) / S.ref
    out.append(('G3 hard case lower', rA <= HARD_RATIO,
                f'room A total squared error adaptive/fixed {rA:.3f} (bar {HARD_RATIO}); relative RMSE room A '
                f'median {np.median(relF[A]):.3f} -> {np.median(relA[A]):.3f}, p95 {np.percentile(relF[A],95):.3f} -> '
                f'{np.percentile(relA[A],95):.3f}; room B ratio {rB:.3f}, relative RMSE median '
                f'{np.median(relF[B]):.4f} -> {np.median(relA[B]):.4f}'))
    # G4 sharing never crosses a wall or a corner (structural: weight actually used)
    cross = max(i.get('cross', 0.0) for i in info)
    perp = max(i.get('perp', 0.0) for i in info)
    out.append(('G4 sharing stays on its face', cross <= 1e-3 and perp <= 1e-3,
                f'summed sharing weight across the dividing wall {cross:.4g}, between faces at 90 degrees '
                f'{perp:.4g} (bar 1e-3 each, worst repeat)'))
    # G6 / G7 isolation: the same rays, a light switched off where this surfel cannot see it -> its estimate must not
    # move at all. Room B dark -> no room A surfel may change (the wall); the floor's sun pool dark -> no room A floor
    # surfel may change (a floor never sees its own floor: the pool reaches the floor only round the corner).
    leakW = leakC = 0.0
    whereW = whereC = -1
    A_floor = A & (S.face == 4)
    for rep in range(ISO_REPEATS):
        m0 = ad[rep][0]
        for kind, m1 in (('wall', iso[2 * rep][0]), ('corner', iso[2 * rep + 1][0])):
            sel = A if kind == 'wall' else A_floor
            rel = np.where(sel, np.abs(m1 - m0) / np.maximum(np.abs(m0), 1e-12), 0.0)
            k = int(np.argmax(rel))
            if kind == 'wall' and rel[k] > leakW:
                leakW, whereW = float(rel[k]), k
            if kind == 'corner' and rel[k] > leakC:
                leakC, whereC = float(rel[k]), k

    def at(k):
        return 'nowhere' if k < 0 else f'p=({S.P[k,0]:.3f},{S.P[k,1]:.3f},{S.P[k,2]:.3f}) face {S.face[k]}'
    out.append(('G6 no light through the wall', leakW <= ISO_TOL,
                f'room B switched off: largest relative change of a room A surfel {leakW:.3g} at {at(whereW)} '
                f'(bar {ISO_TOL}, {ISO_REPEATS} repeats, same rays)'))
    out.append(('G7 no light round the corner', leakC <= ISO_TOL,
                f'floor sun pool switched off: largest relative change of a room A floor surfel {leakC:.3g} at '
                f'{at(whereC)} (bar {ISO_TOL}, {ISO_REPEATS} repeats, same rays)'))
    totF = cF.sum(1).mean()
    totA = cA.sum(1).mean()
    stats = dict(totF=totF, totA=totA, totA_A=cA[:, A].sum(1).mean(), totA_B=cA[:, B].sum(1).mean(),
                 capped=float((cA >= CAP).sum(1).mean()), shared=np.mean([i['shared'] for i in info]),
                 rounds=max(i.get('rounds', 0) for i in info), tF=tF / R, tA=tA / R, secs=time.time() - t0)
    return out, stats


def report(name, out, stats):
    print(f'--- {name}')
    for k, ok, msg in out:
        print(f'{k}: {"PASS" if ok else "FAIL"}  {msg}')
    print(f'rays per repeat: fixed {stats["totF"]:.0f}, adaptive {stats["totA"]:.0f} '
          f'(x{stats["totA"]/stats["totF"]:.2f}; room A {stats["totA_A"]:.0f}, room B {stats["totA_B"]:.0f}); '
          f'surfels at the cap {stats["capped"]:.0f}, surfels sharing {stats["shared"]:.0f}, rounds <= {stats["rounds"]}; '
          f'bake seconds per repeat fixed {stats["tF"]:.2f} adaptive {stats["tA"]:.2f}; variant {stats["secs"]:.0f} s')
    return all(ok for _, ok, _ in out)


def main():
    R = int(os.environ.get('SMOOTHN1_REPEATS', '16'))
    if R < 16:
        # G2 / G5 take each surfel's SE from the repeats; with fewer than 16 the tail at z 4.5 is far too heavy
        # (3 repeats: green "fails" on noise). Refuse rather than print a verdict that means nothing.
        print(f'SMOOTHN1_REPEATS={R}: at least 16 repeats are needed for the per-surfel tests; no verdict')
        return 2
    only = os.environ.get('SMOOTHN1_RED', None)
    global _S
    t0 = time.time()
    S = Scene()
    if DEPTH_MODE == 'gical1':
        a = time.time()
        gical1_pair_vis(S)
        print(f'depth test: GICAL1 ({G1.SIDE}x{G1.SIDE} texels, warp {G1.WARP:g}, D = {DIAM} m, lift and bias 1 unit = '
              f'{UNIT_M} m), maps built in {time.time()-a:.1f} s')
    else:
        print(f'depth test: STAND-IN ({DM_RES}x{DM_RES} texels from the bake rays, D = {DIAM} m, bias {BIAS} m)')
    _S = S
    nA, nB = int((S.room == 0).sum()), int((S.room == 1).sum())
    print(f'scene: {S.n} surfels (room A hard {nA}, room B easy {nB}), {len(S.I)} same-cell neighbor pairs within '
          f'{R_SHARE} m; floor F={F} (production 2048), cap {CAP} (production {CAP*8}), bar SE <= max({EPS_REL} x mean, '
          f'{TAU_FRAC} x median mean), {R} repeats, seed {SEED}')
    print(f'reference (exact) mean radiance: room A median {np.median(S.ref[S.room==0]):.3f} '
          f'[{S.ref[S.room==0].min():.3f}, {S.ref[S.room==0].max():.3f}], room B median {np.median(S.ref[S.room==1]):.3f}')
    if only is not None:
        out, st = run_variant(S, only, R)
        ok = report(f'variant {only or "green"}', out, st)
        print(f'VERDICT {only or "green"}: {"PASS" if ok else "FAIL"}')
        return 0 if ok else 1
    checks = []
    rout = []
    rok = mc_reference_check(S, rout)
    for k, ok, msg in rout:
        print(f'{k}: {"PASS" if ok else "FAIL"}  {msg}')
    out, st = run_variant(S, '', R)
    gok = report('green', out, st) and rok
    checks.append(('green', gok, True))
    expect = {'corner': 'G7 no light round the corner', 'wall': 'G6 no light through the wall',
              'speed': 'G1 never-fewer'}
    for red, must in expect.items():
        o, s = run_variant(S, red, R)
        report(f'red {red}', o, s)
        failed = {k for k, ok, _ in o if not ok}
        good = must in failed
        print(f'red {red}: {"FAILS as it must" if good else "DID NOT FAIL its check"} ({must}); failing checks: '
              f'{", ".join(sorted(failed)) or "none"}')
        checks.append((red, good, False))
    allok = all(c[1] for c in checks)
    import resource
    me = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024
    ch = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss / 1024
    print(f'peak memory: main {me:.0f} MB, largest worker {ch:.0f} MB, {WORKERS} workers: bound '
          f'{me + WORKERS * ch:.0f} MB')
    print(f'VERDICT:{"PASS" if allok else "FAIL"} (green {"PASS" if gok else "FAIL"}; reds '
          f'{", ".join(c[0] + (" fail" if c[1] else " DID NOT FAIL") for c in checks if not c[2])}) '
          f'in {time.time()-t0:.0f} s')
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
