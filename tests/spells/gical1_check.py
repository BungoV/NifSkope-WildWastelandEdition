#!/usr/bin/env python3
"""GICAL1 twin: the surfel radial depth map (leak stopper) and the gap fill (docs/cloud/GICAL1_DESIGN.md).

  python3 tests/spells/gical1_check.py            green run + every red, exit 0 only if green passes and every red fails
  GICAL1_RED=depthoff|gapoff|flip python3 ...     run ONE red alone (it must print FAIL and exit 1)

Standalone: python3 + numpy. No game data: the scene is built here from axis-aligned boxes.

THE IMPORTABLE PART (another lane, e.g. SMOOTHN1, imports these; the doc's section 3 is the same spec in words):

  basis(n)                                  -> (t, b)  Duff et al. 2017 tangent frame of unit normal(s) n
  hemioct_encode(dl) / hemioct_decode(uv)   local unit direction (z >= 0) <-> (u, v) in [-1, 1]^2, horizon warp k = 4
  depth_ray_dirs(n, rng, side=8, per_texel=8) -> (dirs[N,3] world, texel[N])  stratified per texel
  depth_map_from_hits(dist, texel, D, side=8) -> moments[side*side, 2] = (mean d, mean d^2), d clamped to D
  pack_depth_record(moments, D) -> 4*side*side bytes ; unpack_depth_record(buf, D, side=8) -> moments
  depth_visibility(p, n, moments, D, q, side=8, lift=LIFT, sigma_min_frac=SIGMA_MIN_FRAC, flip=False) -> vis in [0,1]
      p, n: the surfel's point and unit normal; q: the receiving point(s) (already moved off their own surface by
      the receiver's normal bias); vis = 1 when |q - o| <= mean, else (var / (var + (|q - o| - mean)^2))^3,
      o = p + n * lift; the mean/variance are a bilinear read of the map in the direction o -> q.

Everything else here (the scene, the tracer, the gather, the gates) is the twin.
"""
import os
import struct
import sys
import time

import numpy as np

# ---------------------------------------------------------------- the depth map spec (doc section 3)
# GICAL1_SIDE / GICAL1_WARP override the two layout numbers for a measurement run (doc section 5: 4 x 4 unwarped
# FAILS the leak gate at 2- and 4-unit walls); the shipped spec is 8 and 4.
SIDE = int(os.environ.get('GICAL1_SIDE', '8'))       # texels a side: 8 x 8 = 64 texels over the hemisphere
PER_TEXEL = 8            # depth rays per texel (512 a surfel)
LIFT = 1.0               # the depth rays start this far along the surfel's normal (game units)
SIGMA_MIN_FRAC = 0.01    # variance floor = (0.01 D)^2
EXPONENT = 3             # the Chebyshev weight is cubed (DDGI's contrast step)
WARP = float(os.environ.get('GICAL1_WARP', '4'))     # horizon warp k: the map stores direction (x, y, k z) normalised


def basis(n):
    """Tangent t and bitangent b of unit normal(s) n (..., 3): Duff, Burgess, Christensen, Hery, Kensler, Liani,
    Villemin, "Building an Orthonormal Basis, Revisited", JCGT 6(1) 2017. sign = +1 when n.z >= 0 (so -0.0 counts +)."""
    n = np.asarray(n, float)
    s = np.where(n[..., 2] >= 0.0, 1.0, -1.0)
    a = -1.0 / (s + n[..., 2])
    bb = n[..., 0] * n[..., 1] * a
    t = np.stack([1.0 + s * n[..., 0] ** 2 * a, s * bb, -s * n[..., 0]], -1)
    b = np.stack([bb, s + n[..., 1] ** 2 * a, -n[..., 1]], -1)
    return t, b


def hemioct_encode(dl, warp=None):
    """local direction(s) (..., 3), z clamped to >= 0 -> (u, v) in [-1, 1]^2 (hemi-octahedral, 45-degree turned),
    after the horizon warp z -> k z (k = WARP)."""
    dl = np.asarray(dl, float)
    z = np.maximum(dl[..., 2], 0.0) * (WARP if warp is None else warp)
    s = np.abs(dl[..., 0]) + np.abs(dl[..., 1]) + z
    s = np.where(s > 0.0, s, 1.0)
    px, py = dl[..., 0] / s, dl[..., 1] / s
    return np.stack([px + py, px - py], -1)


def hemioct_decode(uv, warp=None):
    uv = np.asarray(uv, float)
    px, py = (uv[..., 0] + uv[..., 1]) * 0.5, (uv[..., 0] - uv[..., 1]) * 0.5
    z = (1.0 - np.abs(px) - np.abs(py)) / (WARP if warp is None else warp)
    d = np.stack([px, py, z], -1)
    return d / np.linalg.norm(d, axis=-1, keepdims=True)


def depth_ray_dirs(n, rng, side=SIDE, per_texel=PER_TEXEL):
    """World directions for ONE surfel of normal n: per texel, `per_texel` points uniform in the texel's (u, v)
    square (stratified: a 2 x (per_texel / 2) sub-grid, jittered), decoded. Returns (dirs[N, 3], texel[N]),
    texel = j * side + i, i along u, j along v."""
    t, b = basis(np.asarray(n, float))
    sx = 2 if per_texel % 2 == 0 else 1
    sy = per_texel // sx
    out, tex = [], []
    for j in range(side):
        for i in range(side):
            for a in range(sx):
                for c in range(sy):
                    fu = (i + (a + rng.random()) / sx) / side
                    fv = (j + (c + rng.random()) / sy) / side
                    out.append((fu * 2.0 - 1.0, fv * 2.0 - 1.0))
                    tex.append(j * side + i)
    dl = hemioct_decode(np.array(out))
    dw = dl[:, :1] * t + dl[:, 1:2] * b + dl[:, 2:3] * np.asarray(n, float)
    return dw / np.linalg.norm(dw, axis=1, keepdims=True), np.array(tex)


def depth_map_from_hits(dist, texel, D, side=SIDE):
    """moments[side*side, 2]: per texel the mean of d and of d^2, d = min(hit distance, D) (a miss is D).
    The offline bake's plain mean is what an exponential moving average converges to. A texel with no ray: (D, D^2)."""
    d = np.minimum(np.asarray(dist, float), D)
    m = np.zeros((side * side, 2))
    cnt = np.bincount(texel, minlength=side * side).astype(float)
    m[:, 0] = np.bincount(texel, d, side * side)
    m[:, 1] = np.bincount(texel, d * d, side * side)
    empty = cnt == 0
    cnt[empty] = 1.0
    m /= cnt[:, None]
    m[empty] = (D, D * D)
    return m


def pack_depth_record(moments, D):
    """The .tbk v5 depth record: per texel (texel order), uint16 round(65535 mean / D), uint16 round(65535 meanSq / D^2)."""
    q = np.empty(moments.shape, '<u2')
    q[:, 0] = np.clip(np.rint(moments[:, 0] / D * 65535.0), 0, 65535)
    q[:, 1] = np.clip(np.rint(moments[:, 1] / (D * D) * 65535.0), 0, 65535)
    return q.tobytes()


def unpack_depth_record(buf, D, side=SIDE):
    q = np.frombuffer(buf, '<u2', side * side * 2).reshape(side * side, 2).astype(float)
    return np.stack([q[:, 0] / 65535.0 * D, q[:, 1] / 65535.0 * D * D], -1)


def _bilinear(moments, uv, side):
    """bilinear read of map(s) moments (..., side*side, 2) at uv (..., 2); texel centers, clamped at the edges."""
    f = (uv + 1.0) * 0.5 * side - 0.5
    f = np.clip(f, 0.0, side - 1.0)
    i0 = np.minimum(np.floor(f).astype(int), side - 2)
    w = f - i0
    iu, iv = i0[..., 0], i0[..., 1]
    wu, wv = w[..., 0:1], w[..., 1:2]
    moments = np.broadcast_to(moments, uv.shape[:-1] + moments.shape[-2:])

    def at(j, i):
        return np.take_along_axis(moments, (j * side + i)[..., None, None], -2)[..., 0, :]
    return ((1 - wu) * (1 - wv) * at(iv, iu) + wu * (1 - wv) * at(iv, iu + 1)
            + (1 - wu) * wv * at(iv + 1, iu) + wu * wv * at(iv + 1, iu + 1))


def depth_visibility(p, n, moments, D, q, side=SIDE, lift=LIFT, sigma_min_frac=SIGMA_MIN_FRAC, flip=False):
    """Chebyshev visibility of receiving point(s) q (..., 3) from surfel (p, n) with depth map `moments`.
    flip=True reads the opposite direction (a deliberate defect for the twin's red)."""
    p = np.asarray(p, float)
    n = np.asarray(n, float)
    q = np.asarray(q, float)
    o = p + n * lift
    v = q - o
    t = np.linalg.norm(v, axis=-1)
    dw = v / np.maximum(t, 1e-9)[..., None]
    if flip:
        dw = -dw
    tt, bb = basis(n)
    dl = np.stack([(dw * tt).sum(-1), (dw * bb).sum(-1), (dw * n).sum(-1)], -1)
    mm = _bilinear(moments, hemioct_encode(dl), side)
    mean, m2 = mm[..., 0], mm[..., 1]
    var = np.maximum(m2 - mean * mean, (sigma_min_frac * D) ** 2)
    ex = np.maximum(t - mean, 0.0)
    cheb = (var / (var + ex * ex)) ** EXPONENT
    return np.where(t <= mean, 1.0, cheb)


# ---------------------------------------------------------------- the twin's scene and tracer
class Scene:
    """Solid axis-aligned boxes (lo, hi). Two rooms sharing one wall of thickness T, room A open to the sun,
    room B roofed with a window north; an L corridor off room A's south door, roofed; ground slab."""

    def __init__(self, T):
        self.T = T
        bx = []

        def box(x0, y0, z0, x1, y1, z1):
            bx.append((min(x0, x1), min(y0, y1), min(z0, z1), max(x0, x1), max(y0, y1), max(z0, z1)))
        H = 192.0
        box(-600, -700, -8, 1200, 900, 0)                       # ground
        # room A: x 0..256, y 0..256, open roof; room B: x 256+T..512+T, roofed
        box(-T, -T, 0, 0, 256 + T, H)                           # A west
        # A+B north wall, with room B's window x 336..496, z 32..184 (the sun comes in from the north and lands
        # on B's floor by its north-east corner: room B is dim, not black)
        box(-T, 256, 0, 336, 256 + T, H)
        box(496, 256, 0, 512 + 2 * T, 256 + T, H)
        box(336, 256, 0, 496, 256 + T, 32)
        box(336, 256, 184, 496, 256 + T, H)
        box(256, 0, 0, 256 + T, 256, H)                         # THE SHARED WALL
        # A+B south wall with the door x 96..160 (z < 160) into the corridor
        box(-T, -T, 0, 96, 0, H)
        box(160, -T, 0, 512 + 2 * T, 0, H)
        box(96, -T, 160, 160, 0, H)
        box(512 + T, -T, 0, 512 + 2 * T, 256 + T, H)            # B east
        box(256, -T, H, 512 + 2 * T, 256 + T, H + T)            # B roof
        # the corridor: leg 1 x 96..160, y -192..-T; leg 2 x 160..416, y -192..-128; height 160, roofed
        Hc = 160.0
        box(96 - T, -192 - T, 0, 96, -T, Hc)                    # leg 1 west
        box(160, -128, 0, 160 + T, -T, Hc)                      # leg 1 east = the inner corner wall
        box(160, -128, 0, 416 + T, -128 + T, Hc)                # leg 2 north
        box(96 - T, -192 - T, 0, 416 + T, -192, Hc)             # south
        box(416, -192, 0, 416 + T, -128, Hc)                    # leg 2 end
        box(96 - T, -192 - T, Hc, 160 + T, -T, Hc + T)          # roof leg 1
        box(160, -192 - T, Hc, 416 + T, -128 + T, Hc + T)       # roof leg 2
        b = np.array(bx, float)
        self.lo, self.hi = b[:, :3], b[:, 3:]
        self.sun = np.array([-0.35, 0.25, 0.9])
        self.sun /= np.linalg.norm(self.sun)
        self.sunE = 3.0
        self.albedo = 0.6

    def trace(self, o, d, tmax=np.inf, chunk=1 << 15):
        """first hit of rays o + t d, t in (1e-4, tmax): (t, normal); t = inf on a miss."""
        o = np.asarray(o, float)
        d = np.asarray(d, float)
        N = len(o)
        T = np.full(N, np.inf)
        Nn = np.zeros((N, 3))
        tmax = np.broadcast_to(np.asarray(tmax, float), (N,))
        with np.errstate(divide='ignore', invalid='ignore'):
            for s in range(0, N, chunk):
                oo, dd = o[s:s + chunk, None, :], d[s:s + chunk, None, :]
                inv = 1.0 / dd
                t0 = (self.lo[None] - oo) * inv
                t1 = (self.hi[None] - oo) * inv
                tn = np.minimum(t0, t1)
                tn = np.where(np.isnan(tn), -np.inf, tn)
                tf = np.maximum(t0, t1)
                tf = np.where(np.isnan(tf), np.inf, tf)
                tnear = tn.max(2)
                tfar = tf.min(2)
                ok = (tnear <= tfar) & (tfar > 1e-4) & (tnear > 1e-4) & (tnear < tmax[s:s + chunk, None])
                tt = np.where(ok, tnear, np.inf)
                k = tt.argmin(1)
                best = tt[np.arange(len(k)), k]
                T[s:s + chunk] = best
                ax = tn[np.arange(len(k)), k].argmax(1)
                sg = -np.sign(dd[:, 0, :][np.arange(len(k)), ax])
                nn = np.zeros((len(k), 3))
                nn[np.arange(len(k)), ax] = sg
                Nn[s:s + chunk] = nn
        return T, Nn

    def inside(self, p, eps=1e-6):
        p = np.asarray(p, float)
        return ((p[:, None, :] > self.lo[None] + eps) & (p[:, None, :] < self.hi[None] - eps)).all(2).any(1)

    def radiosity(self, x, n):
        """B at surface points: albedo x sun x max(0, n.L) behind a shadow ray (the one-bounce source)."""
        nl = n @ self.sun
        B = np.zeros(len(x))
        m = nl > 0
        if m.any():
            th, _ = self.trace(x[m] + n[m] * 0.01, np.broadcast_to(self.sun, (m.sum(), 3)))
            B[np.where(m)[0][np.isinf(th)]] = 1.0
        return self.albedo * self.sunE * np.maximum(nl, 0.0) * B

    def irradiance(self, x, n, rays, rng):
        """brute-force bounce irradiance at points x, normals n: cosine rays, E = mean B(hit) (sky = 0)."""
        P = len(x)
        t, b = basis(n)
        u1, u2 = rng.random((P, rays)), rng.random((P, rays))
        r, ph = np.sqrt(u1), 2 * np.pi * u2
        lx, ly, lz = r * np.cos(ph), r * np.sin(ph), np.sqrt(np.maximum(1 - u1, 0))
        d = lx[..., None] * t[:, None] + ly[..., None] * b[:, None] + lz[..., None] * n[:, None]
        o = np.repeat(x + n * LIFT, rays, 0)
        d = d.reshape(-1, 3)
        th, nh = self.trace(o, d)
        hit = np.isfinite(th)
        L = np.zeros(len(o))
        if hit.any():
            L[hit] = self.radiosity(o[hit] + d[hit] * th[hit, None], nh[hit])
        return L.reshape(P, rays).mean(1)


# ---------------------------------------------------------------- surfels
SPACING = 32.0          # the surfel lattice
RADIUS = 32.0           # the surfel's gather radius r (> every wall thickness tested)
DCLAMP = 2.0 * RADIUS   # D: depth rays are clamped to the surfel's diameter
FILL_RADIUS = DCLAMP    # the gap fill's reach (no surfel's map vouches for a point beyond D)
FILL_VIS_POW = 3        # the gap fill takes vis^3 (a second contrast step: far surfels are the least sure; doc 5)
SURF_RAYS = 1024        # each surfel's irradiance rays
REF_RAYS = 2048         # the reference's rays per sample point
ROI = (np.array([-64.0, -260.0, -1.0]), np.array([720.0, 330.0, 250.0]))


def make_surfels(sc):
    """a surfel at every lattice point (SPACING/2 + k SPACING) of every box face that faces air inside the ROI."""
    P, N = [], []
    for k in range(len(sc.lo)):
        for ax in range(3):
            for sgn in (-1.0, 1.0):
                plane = sc.hi[k, ax] if sgn > 0 else sc.lo[k, ax]
                o1, o2 = [a for a in range(3) if a != ax]
                g1 = np.arange(np.ceil((sc.lo[k, o1] - SPACING / 2) / SPACING), np.floor((sc.hi[k, o1] - SPACING / 2) / SPACING) + 1) * SPACING + SPACING / 2
                g2 = np.arange(np.ceil((sc.lo[k, o2] - SPACING / 2) / SPACING), np.floor((sc.hi[k, o2] - SPACING / 2) / SPACING) + 1) * SPACING + SPACING / 2
                if len(g1) == 0 or len(g2) == 0:
                    continue
                G1, G2 = np.meshgrid(g1, g2, indexing='ij')
                p = np.zeros((G1.size, 3))
                p[:, ax] = plane
                p[:, o1] = G1.ravel()
                p[:, o2] = G2.ravel()
                n = np.zeros(3)
                n[ax] = sgn
                keep = ~sc.inside(p + n * 0.5) & (p >= ROI[0]).all(1) & (p <= ROI[1]).all(1)
                P.append(p[keep])
                N.append(np.repeat(n[None], keep.sum(), 0))
    P, N = np.concatenate(P), np.concatenate(N)
    # one surfel per (point, normal)
    key = np.round(np.concatenate([P, N], 1), 3)
    _, idx = np.unique(key, axis=0, return_index=True)
    idx.sort()
    return P[idx], N[idx]


def bake_surfels(sc, P, N, rng):
    E = sc.irradiance(P, N, SURF_RAYS, rng)
    maps = np.zeros((len(P), SIDE * SIDE, 2))
    dirs, tex = [], []
    for i in range(len(P)):
        d, t = depth_ray_dirs(N[i], rng)
        dirs.append(d)
        tex.append(t)
    dirs = np.stack(dirs)
    o = np.repeat(P + N * LIFT, dirs.shape[1], 0)
    th, _ = sc.trace(o, dirs.reshape(-1, 3), tmax=DCLAMP)
    th = th.reshape(len(P), -1)
    for i in range(len(P)):
        # the maps go through the file's 16-bit record: what a reader gets back
        maps[i] = unpack_depth_record(pack_depth_record(depth_map_from_hits(th[i], tex[i], DCLAMP), DCLAMP), DCLAMP)
    return E, maps


def gather(x, n, P, N, E, maps, depth=True, gap=True, flip=False, exact=None):
    """the apply: per point, w_i = (1 - (d/r)^2)^2 x max(0, n_i . n) x vis_i over surfels within r; W = sum w.
    W >= 1: sum w E / W. W < 1: sum w E + (1 - W) x E_cell, E_cell = the surfels of the point's cell (within D),
    weighted by (1 - (d/D)^2)^2 x max(0, n_i . n) x vis_i. Returns (E, W).
    exact = a Scene: vis_i is the true segment test (surfel's lifted point -> q) instead of the map: the control."""
    q = x + n * LIFT
    out = np.zeros(len(x))
    Ws = np.zeros(len(x))
    d2 = ((q[:, None, :] - P[None]) ** 2).sum(2)
    for k in range(len(x)):
        cand = np.where(d2[k] < DCLAMP * DCLAMP)[0]
        nd = np.maximum(N[cand] @ n[k], 0.0)
        cand, nd = cand[nd > 0], nd[nd > 0]
        vis = np.ones(len(cand))
        if exact is not None and len(cand):
            o = P[cand] + N[cand] * LIFT
            v = q[k][None] - o
            L = np.linalg.norm(v, axis=1)
            th, _ = exact.trace(o, v / np.maximum(L, 1e-9)[:, None], tmax=L)
            vis = np.isinf(th).astype(float)
        elif depth and len(cand):
            vis = depth_visibility(P[cand], N[cand], maps[cand], DCLAMP, q[k][None], flip=flip)
        dd = d2[k, cand] / (RADIUS * RADIUS)
        ker = np.where(dd < 1.0, (1.0 - dd) ** 2, 0.0)
        w = ker * nd * vis
        W = w.sum()
        Ws[k] = W
        if W >= 1.0:
            out[k] = (w * E[cand]).sum() / W
            continue
        out[k] = (w * E[cand]).sum()
        if gap:
            dD = d2[k, cand] / (FILL_RADIUS * FILL_RADIUS)
            wf = (1.0 - np.minimum(dD, 1.0)) ** 2 * nd * vis ** FILL_VIS_POW
            if wf.sum() > 0:
                out[k] += (1.0 - W) * (wf * E[cand]).sum() / wf.sum()
    return out, Ws


def grid_pts(xs, ys, zs, normal):
    X, Y, Z = np.meshgrid(xs, ys, zs, indexing='ij')
    p = np.stack([X.ravel(), Y.ravel(), Z.ravel()], 1).astype(float)
    return p, np.repeat(np.array(normal, float)[None], len(p), 0)


def sample_sets(T):
    near = np.array([2.0, 6.0, 12.0, 20.0, 28.0])
    ys = np.arange(24.0, 89.0, 16.0)            # south half of the rooms, away from the window (y 96..160)
    zs = np.arange(24.0, 169.0, 36.0)
    S = {}
    # dark side: room B floor and its south wall's inner face beside the shared wall
    a = grid_pts(256 + T + near, np.concatenate([ys, ys + 144]), [0.0], (0, 0, 1))
    b = grid_pts(256 + T + near, [0.0], zs, (0, 1, 0))
    c = grid_pts(256 + T + near, [256.0], zs, (0, -1, 0))
    S['dark'] = tuple(np.concatenate(v) for v in zip(a, b, c))
    a = grid_pts(256 - near, np.concatenate([ys, ys + 144]), [0.0], (0, 0, 1))
    b = grid_pts(256 - near, [0.0], zs, (0, 1, 0))
    c = grid_pts(256 - near, [256.0], zs, (0, -1, 0))
    S['bright'] = tuple(np.concatenate(v) for v in zip(a, b, c))
    # corridor: leg 2 just past the corner (floor + its north wall), and leg 1 beside the inner corner wall
    a = grid_pts(160 + np.array([4.0, 12.0, 24.0, 40.0]), np.arange(-188.0, -131.0, 14.0), [0.0], (0, 0, 1))
    b = grid_pts(160 + np.array([8.0, 20.0, 36.0]), [-128.0], zs[:4], (0, -1, 0))
    c = grid_pts(160 - np.array([2.0, 6.0, 12.0, 20.0]), np.arange(-120.0, -20.0, 20.0), [0.0], (0, 0, 1))
    S['corridor'] = tuple(np.concatenate(v) for v in zip(a, b, c))
    # the gap: room A floor (bounce light, smooth) where the surfels are removed (below)
    S['gap'] = grid_pts(np.arange(118.0, 139.0, 4.0), np.arange(118.0, 139.0, 4.0), [0.0], (0, 0, 1))
    return S


GAP_RECT = (100.0, 150.0, 100.0, 150.0)   # floor surfels with x, y inside are removed (4 of them)


def relerr(E, R):
    return float(np.abs(E - R).mean() / max(R.mean(), 1e-12))


# ---------------------------------------------------------------- unit checks of the importable part
def unit_checks(rng):
    res = []
    # 1. hemi-oct round trip
    d = rng.normal(size=(2000, 3))
    d[:, 2] = np.abs(d[:, 2])
    d /= np.linalg.norm(d, axis=1, keepdims=True)
    e = np.abs(hemioct_decode(hemioct_encode(d)) - d).max()
    res.append(('U1 hemi-oct round trip', e < 1e-12, 'max err %.2e (bar 1e-12)' % e))
    # 2. basis orthonormal for every axis normal and random ones (incl. n.z = -0.0 and -1)
    ns = np.concatenate([np.eye(3), -np.eye(3), d, [[0.0, 0.0, -1.0], [1.0, 0.0, -0.0]]])
    t, b = basis(ns)
    o = max(np.abs((t * ns).sum(1)).max(), np.abs((b * ns).sum(1)).max(), np.abs((t * b).sum(1)).max(),
            np.abs(np.linalg.norm(t, axis=1) - 1).max(), np.abs(np.linalg.norm(b, axis=1) - 1).max())
    res.append(('U2 tangent frame orthonormal', o < 1e-12, 'max err %.2e' % o))
    # 3. record round trip
    D = 64.0
    m = np.stack([rng.random(SIDE * SIDE) * D, rng.random(SIDE * SIDE) * D * D], 1)
    buf = pack_depth_record(m, D)
    back = unpack_depth_record(buf, D)
    e1 = np.abs(back[:, 0] - m[:, 0]).max()
    res.append(('U3 %d-byte record round trip' % (4 * SIDE * SIDE), len(buf) == 4 * SIDE * SIDE and e1 <= D / 65535.0 * 0.5 + 1e-12,
                '%d bytes, mean err %.2e (bar half a step %.2e)' % (len(buf), e1, D / 65535 * 0.5)))
    # 4. known answer: a floor surfel 10 units from a 4-unit wall (x 0..4); point behind the wall -> ~0, in front -> 1
    class Wall(Scene):
        def __init__(self):
            self.lo = np.array([[-500.0, -500.0, -8.0], [0.0, -500.0, 0.0]])
            self.hi = np.array([[500.0, 500.0, 0.0], [4.0, 500.0, 300.0]])
    w = Wall()
    p, n = np.array([-10.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0])
    dirs, tex = depth_ray_dirs(n, rng)
    th, _ = w.trace(np.repeat((p + n * LIFT)[None], len(dirs), 0), dirs, tmax=D)
    mm = depth_map_from_hits(th, tex, D)
    behind = depth_visibility(p, n, mm, D, np.array([[4.0 + 1.0, 0.0, 1.0], [12.0, 5.0, 1.0], [20.0, -8.0, 1.0]]))
    front = depth_visibility(p, n, mm, D, np.array([[-3.0, 0.0, 1.0], [-25.0, 10.0, 1.0], [-10.0, 20.0, 1.0]]))
    ok = behind.max() < 0.01 and front.min() == 1.0
    res.append(('U4 known answer: wall in between', ok, 'behind the wall max %.4f (bar < 0.01), open side min %.3f (bar 1)'
                % (behind.max(), front.min())))
    # 4r. its own red: the map read the wrong way round must NOT pass
    fl = depth_visibility(p, n, mm, D, np.array([[5.0, 0.0, 1.0]]), flip=True)
    res.append(('U4r red flip lets the wall through', fl.min() > 0.5, 'flipped vis behind the wall %.3f (must be > 0.5)' % fl.min()))
    return res


# ---------------------------------------------------------------- the run
def bake_scene(T, seed):
    """the scene's surfels (irradiance + depth maps through the file's record) and the reference at every sample point."""
    rng = np.random.default_rng(seed)
    sc = Scene(T)
    P, N = make_surfels(sc)
    E, maps = bake_surfels(sc, P, N, rng)
    S = sample_sets(T)
    gx0, gx1, gy0, gy1 = GAP_RECT
    rem = (N[:, 2] > 0.5) & (np.abs(P[:, 2]) < 1e-6) & (P[:, 0] > gx0) & (P[:, 0] < gx1) & (P[:, 1] > gy0) & (P[:, 1] < gy1)
    rref = np.random.default_rng(seed + 1000)
    ref = {name: sc.irradiance(x, n, REF_RAYS, rref) for name, (x, n) in S.items()}
    exact = {}
    for name, (x, n) in S.items():
        k = ~rem if name == 'gap' else np.ones(len(P), bool)
        exact[name] = gather(x, n, P[k], N[k], E[k], maps[k], exact=sc)[0]
    # the gap's control: the same pixels gathered with every surfel still there
    exact['gap_full'] = gather(S['gap'][0], S['gap'][1], P, N, E, maps)[0]
    return dict(T=T, P=P, N=N, E=E, maps=maps, S=S, keep=~rem, removed=int(rem.sum()), ref=ref, exact=exact)


def run_mode(ctx, mode):
    """mode: '' green, or the red's name. Per sample set: (reference, gathered, surfel weight sum, gathered with the
    true segment test instead of the map: the control)."""
    out = {'surfels': len(ctx['P']), 'removed': ctx['removed'], 'gap_full': ctx['exact']['gap_full']}
    for name, (x, n) in ctx['S'].items():
        k = ctx['keep'] if name == 'gap' else np.ones(len(ctx['P']), bool)
        P, N, E, M = ctx['P'][k], ctx['N'][k], ctx['E'][k], ctx['maps'][k]
        Eg, W = gather(x, n, P, N, E, M, depth=mode != 'depthoff', gap=mode != 'gapoff', flip=mode == 'flip')
        out[name] = (ctx['ref'][name], Eg, W, ctx['exact'][name])
    return out


def evaluate(results, mode):
    """the gates (doc section 6). X = the same gather with the true segment test in place of the map (the control:
    what a perfect depth map would give at this surfel density); R = the brute-force reference."""
    rows = []
    for T, r in results:
        Rb, Eb, Wb, Xb = r['bright']
        Rd, Ed, Wd, Xd = r['dark']
        Rc, Ec, Wc, Xc = r['corridor']
        Rg, Eg, Wg, Xg = r['gap']
        ed, xd = relerr(Ed, Rd), relerr(Xd, Rd)
        leak = float((Ed - Xd).mean() / max(Rd.mean(), 1e-12))
        rows.append(('L T=%g leak, dark side' % T, ed <= xd + 0.10,
                     'rel err %.3f vs control %.3f (bar control + 0.10); dark ref mean %.4f, gathered %.4f, control %.4f; '
                     'excess over control %.3f of the dark ref; bright ref mean %.4f'
                     % (ed, xd, Rd.mean(), Ed.mean(), Xd.mean(), leak, Rb.mean())))
        eb, xb = relerr(Eb, Rb), relerr(Xb, Rb)
        sh = abs(Eb.mean() - Xb.mean()) / max(Xb.mean(), 1e-12)
        rows.append(('B T=%g bright side unchanged' % T, eb <= xb + 0.02 and sh <= 0.05,
                     'rel err %.3f vs control %.3f (bar control + 0.02); mean off the control %.4f (bar 0.05); ref mean %.4f'
                     % (eb, xb, sh, Rb.mean())))
        ec, xc = relerr(Ec, Rc), relerr(Xc, Rc)
        ratio = Ec.mean() / max(Xc.mean(), 1e-12)
        rows.append(('C T=%g corridor corner' % T, ec <= xc + 0.10 and ratio >= 0.9,
                     'rel err %.3f vs control %.3f (bar control + 0.10); gathered/control mean %.3f (bar >= 0.9: not '
                     'over-blocked); ref mean %.4f, gathered %.4f' % (ec, xc, ratio, Rc.mean(), Ec.mean())))
        eg, fg = relerr(Eg, Rg), relerr(r['gap_full'], Rg)
        ratio = Eg.mean() / max(Rg.mean(), 1e-12)
        rows.append(('G T=%g gap fill' % T, Wg.max() < 0.05 and eg <= fg + 0.10 and 0.8 <= ratio <= 1.25,
                     'gap pixels %d, max surfel weight %.3f (bar < 0.05: a real gap); rel err %.3f vs %.3f with every surfel '
                     '(bar + 0.10); filled/ref mean %.3f (bar 0.8..1.25); ref mean %.4f; %d surfels removed'
                     % (len(Rg), Wg.max(), eg, fg, ratio, Rg.mean(), r['removed'])))
    return rows


def main():
    t0 = time.time()
    red = os.environ.get('GICAL1_RED', '').strip()
    if red not in ('', 'depthoff', 'gapoff', 'flip'):
        print('unknown GICAL1_RED=%s (depthoff | gapoff | flip)' % red)
        return 2
    rng = np.random.default_rng(7)
    allok = True
    if not red:
        for name, ok, msg in unit_checks(rng):
            print('%s %s  %s' % ('PASS' if ok else 'FAIL', name, msg))
            allok &= ok
    thick = (2.0, 4.0, 8.0)
    modes = [red] if red else ['', 'depthoff', 'gapoff', 'flip']
    verdicts = {}
    ctxs = [bake_scene(T, 11 + int(T)) for T in thick]
    print('baked %d scenes (%.1f s)' % (len(ctxs), time.time() - t0))
    for mode in modes:
        res = [(c['T'], run_mode(c, mode)) for c in ctxs]
        if mode == '':
            print('scene: %d surfels (T=2), radius %g, spacing %g, D %g, %dx%d texels x %d rays, %d surfel rays, %d ref rays'
                  % (res[0][1]['surfels'], RADIUS, SPACING, DCLAMP, SIDE, SIDE, PER_TEXEL, SURF_RAYS, REF_RAYS))
        rows = evaluate(res, mode)
        tag = 'green' if mode == '' else 'red ' + mode
        print('--- %s' % tag)
        for name, ok, msg in rows:
            print('%s %s  %s' % ('PASS' if ok else 'FAIL', name, msg))
        verdicts[mode] = all(ok for _, ok, _ in rows)
    print('--- verdict (%.1f s)' % (time.time() - t0))
    if red:
        print('red %s: %s' % (red, 'PASS (the red did NOT fail: the check is blind)' if verdicts[red] else 'FAIL (as it must)'))
        return 0 if verdicts[red] else 1
    good = allok and verdicts['']
    for m in ('depthoff', 'gapoff', 'flip'):
        print('red %-8s %s' % (m, 'fails, as it must' if not verdicts[m] else 'PASSES: the gate cannot see it'))
        good &= not verdicts[m]
    print('GICAL1 %s' % ('PASS' if good else 'FAIL'))
    return 0 if good else 1


if __name__ == '__main__':
    sys.exit(main())
