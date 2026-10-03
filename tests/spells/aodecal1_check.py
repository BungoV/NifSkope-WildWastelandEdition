#!/usr/bin/env python3
"""The AO-volume twin (lane AODECAL1, 2026-10-03; docs/cloud/AODECAL1_DESIGN.md).

  python3 tests/spells/aodecal1_check.py            the green run, then every red, self-tested
  AODECAL1_RED=<name> python3 ...                   run only that red as the product (must exit 1)
                                                    names: worlddown nofar nodivide stale

A per-model ambient occlusion volume (".ao" in "ao/") for cars and low props, rebuilt in numpy from the
design alone, on a synthetic "car" (a body box on four wheel boxes) built in this file. No game data.

  A  lookup        the volume, sampled in the COPY'S model space (any rotation, uniform scale) with the
                   receiver's own normal, against brute-force cosine-weighted ray occlusion by the model alone,
                   at ground, wall and neighbor points, for upright / tipped 90 / flipped 180 / leaning+scaled /
                   stacked copies. Red "worlddown": the copy is treated as upright (yaw only, normal forced to
                   world up), the classic projected decal -> the rotated placements FAIL.
  B  far field     past the volume box the model is an equivalent sphere per direction (rho^2(u), L2 SH, in the
                   header) out to the cut; it is held against brute force from the fade band to the cut, and
                   what the cut drops stays under the bar. Red "nofar": the volume just ends -> FAIL.
  D  no double     the probe sky term (octant sky shares traced through ground + copy, blended like the
     darkening     relight's grid, six-axis cube) times AO / G, where G = what the probes already captured of
                   the copy (rebuilt from the volume at each probe), against the copy-free probe term times the
                   brute-force occlusion. Red "nodivide": AO multiplied in without dividing G out -> FAIL.
  E  file          the .ao bytes and the index record round-trip through a separate reader (sizes, CRC,
                   fingerprint). Negative controls inside E: one flipped payload byte and a stale model
                   fingerprint must both be refused. Red "stale": the reader skips the fingerprint test -> FAIL.

One line per check ("A PASS ..."), each red's verdict, then the overall line; exit 0 only when every
check passes AND every red fails. python3 + numpy only.
"""
import hashlib
import math
import os
import struct
import sys
import time
import zlib

import numpy as np

# ---------------------------------------------------------------- the synthetic car (model space, z up)
# A NIF's own space: the wheels stand on z = 0, +X forward. Game units.
CAR = np.array([
    [[-225.0, -90.0, 40.0], [225.0, 90.0, 150.0]],   # body
    [[120.0, 60.0, 0.0], [180.0, 90.0, 60.0]],       # wheels
    [[120.0, -90.0, 0.0], [180.0, -60.0, 60.0]],
    [[-180.0, 60.0, 0.0], [-120.0, 90.0, 60.0]],
    [[-180.0, -90.0, 0.0], [-120.0, -60.0, 60.0]],
])

# ---------------------------------------------------------------- the design's constants
VOXEL_BUDGET = 32 * 32 * 16      # the brief's ~32 x 32 x 16
MARGIN_K = 2.0                   # margin = MARGIN_K x the median bbox extent, every side
FADE_K = 0.25                    # the volume cross-fades into the far field over the outer FADE_K of the margin
BAKE_RAYS = 1024                 # per voxel, a Fibonacci sphere
LMAX = 2                         # SH order stored (L2: 9 coefficients a voxel)
A_L = [math.pi, 2.0 * math.pi / 3.0, math.pi / 4.0]   # clamped-cosine kernel per band (Ramamoorthi-Hanrahan)
G_FLOOR = 0.1                    # a probe octant the copy blocks beyond this is not divided further

# bars (set from the design's tolerance budget, see the design doc section 7)
BAR_A_MEAN, BAR_A_P95, BAR_A_MAX = 0.02, 0.05, 0.12   # receivers at least one voxel from the model
BAR_AN_MEAN, BAR_AN_P95 = 0.05, 0.15                  # the contact band (within one voxel); first set 0.03 / 0.10,
                                                      # which the leaning copy failed (L2 cannot hold a contact step)
BAR_B_MEAN, BAR_B_MAX, BAR_B_DROP = 0.01, 0.06, 0.02   # the far field past the box, and what the cut drops
BAR_D_P95, BAR_D_BIAS, BAR_D_WORST = 0.05, -0.02, -0.10


def fib_sphere(n, rot=None):
    i = np.arange(n) + 0.5
    z = 1.0 - 2.0 * i / n
    r = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    ph = i * math.pi * (3.0 - math.sqrt(5.0))
    d = np.stack([r * np.cos(ph), r * np.sin(ph), z], 1)
    return d if rot is None else d @ rot.T


def rand_rot(seed):
    q, _ = np.linalg.qr(np.random.default_rng(seed).normal(size=(3, 3)))
    return q * np.sign(np.linalg.det(q))


def rot(yaw=0.0, pitch=0.0, roll=0.0):
    """Z(yaw) Y(pitch) X(roll), degrees, the game's order for a placed reference's angles is not assumed here."""
    a, b, c = (math.radians(v) for v in (yaw, pitch, roll))
    Rz = np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])
    Ry = np.array([[math.cos(b), 0, math.sin(b)], [0, 1, 0], [-math.sin(b), 0, math.cos(b)]])
    Rx = np.array([[1, 0, 0], [0, math.cos(c), -math.sin(c)], [0, math.sin(c), math.cos(c)]])
    return Rz @ Ry @ Rx


# ---------------------------------------------------------------- rays against boxes (model space)
def hits_any(O, D, boxes, chunk=48):
    """(P, N): does the ray O[p] + t D[n], t > 1e-6, hit any box? An origin inside a box counts as hit."""
    out = np.zeros((len(O), len(D)), bool)
    invD = 1.0 / np.where(np.abs(D) < 1e-12, 1e-12, D)
    for s in range(0, len(O), chunk):
        o = O[s:s + chunk, None, :]
        acc = np.zeros((len(o), len(D)), bool)
        for lo, hi in boxes:
            t1 = (lo - o) * invD[None]
            t2 = (hi - o) * invD[None]
            tn = np.minimum(t1, t2).max(2)
            tf = np.maximum(t1, t2).min(2)
            acc |= tf >= np.maximum(tn, 1e-6)
        out[s:s + chunk] = acc
    return out


def seg_blocked(A, B, boxes):
    """per pair: does the segment A[i] -> B[i] cross a box (t in (1e-6, 1))?"""
    D = B - A
    invD = 1.0 / np.where(np.abs(D) < 1e-12, 1e-12, D)
    res = np.zeros(len(A), bool)
    for lo, hi in boxes:
        t1 = (lo - A) * invD
        t2 = (hi - A) * invD
        tn = np.minimum(t1, t2).max(1)
        tf = np.maximum(t1, t2).min(1)
        res |= (tf >= np.maximum(tn, 1e-6)) & (tn < 1.0)
    return res


def inside_any(P, boxes, pad=0.0):
    r = np.zeros(len(P), bool)
    for lo, hi in boxes:
        r |= np.all((P > lo - pad) & (P < hi + pad), 1)
    return r


def dist_to_boxes(P, boxes):
    d = np.full(len(P), np.inf)
    for lo, hi in boxes:
        q = np.maximum(np.maximum(lo - P, P - hi), 0.0)
        d = np.minimum(d, np.linalg.norm(q, axis=1))
    return d


# ---------------------------------------------------------------- real SH, l <= 2
def sh(D, lmax=LMAX):
    x, y, z = D[:, 0], D[:, 1], D[:, 2]
    c = [np.full_like(x, 0.282095), 0.488603 * y, 0.488603 * z, 0.488603 * x]
    if lmax >= 2:
        c += [1.092548 * x * y, 1.092548 * y * z, 0.315392 * (3 * z * z - 1), 1.092548 * x * z,
              0.546274 * (x * x - y * y)]
    return np.stack(c, 1)


def band_of(j):
    return 0 if j == 0 else (1 if j < 4 else 2)


# ---------------------------------------------------------------- the bake
class Volume:
    """One model's .ao in memory: box lo/hi (model space), dims, voxel, fade, coefficients k[z, y, x, j]
    (occlusion SH divided by 2 sqrt(pi), so k[..., 0] is the blocked share of the sphere)."""

    def __init__(self, lo, hi, dims, fade, k, lmax, far=None):
        self.lo, self.hi, self.dims, self.fade, self.k, self.lmax = lo, hi, dims, fade, k, lmax
        self.cell = (hi - lo) / np.array(dims, float)
        # the far field: an equivalent sphere per direction from the center c, rho^2(u) as 9 SH coefficients,
        # cut where the largest sphere's occlusion falls under FAR_CUT (radius rcut). None = no far field (red)
        self.far = far

    def with_k(self, k):
        return Volume(self.lo, self.hi, self.dims, self.fade, k, self.lmax, self.far)


def volume_box(boxes, margin_k=MARGIN_K):
    blo, bhi = boxes[:, 0].min(0), boxes[:, 1].max(0)
    ext = bhi - blo
    m = margin_k * float(np.median(ext))
    lo, hi = blo - m, bhi + m
    E = hi - lo
    s = (E.prod() / VOXEL_BUDGET) ** (1.0 / 3.0)
    dims = tuple(int(max(8, round(e / s))) for e in E)
    return lo, hi, dims, m * FADE_K, m


def bake(boxes, margin_k=MARGIN_K, lmax=LMAX, rays=BAKE_RAYS):
    lo, hi, dims, fade, _ = volume_box(boxes, margin_k)
    cell = (hi - lo) / np.array(dims, float)
    gz, gy, gx = np.meshgrid(*[np.arange(d) for d in dims[::-1]], indexing='ij')
    C = lo + (np.stack([gx, gy, gz], -1).reshape(-1, 3) + 0.5) * cell
    dirs = fib_sphere(rays)
    Y = sh(dirs, lmax)
    nb = Y.shape[1]
    k = np.zeros((len(C), nb))
    for s in range(0, len(C), 2048):
        blk = hits_any(C[s:s + 2048], dirs, boxes).astype(np.float64)
        k[s:s + 2048] = (blk @ Y) * (4.0 * math.pi / rays) / (2.0 * math.sqrt(math.pi))
    valid = ~inside_any(C, boxes)
    k = k.reshape(dims[2], dims[1], dims[0], nb)
    valid = valid.reshape(dims[2], dims[1], dims[0])
    # voxels inside the solid have no outside: filled from their valid neighbors, ring by ring
    while not valid.all():
        acc = np.zeros_like(k)
        cnt = np.zeros(valid.shape)
        for ax in range(3):
            for sgn in (-1, 1):
                kv = np.roll(k * valid[..., None], sgn, ax)
                vv = np.roll(valid, sgn, ax).astype(float)
                edge = [slice(None)] * 3
                edge[ax] = 0 if sgn == 1 else -1
                kv[tuple(edge)] = 0
                vv[tuple(edge)] = 0
                acc += kv
                cnt += vv
        grow = (~valid) & (cnt > 0)
        k[grow] = acc[grow] / cnt[grow][:, None]
        valid = valid | grow
    vol = Volume(lo, hi, dims, fade, k, lmax)
    vol.far = fit_far(vol, boxes)
    return vol


FAR_CUT = 0.01   # the far field ends where the widest equivalent sphere blocks this much (cosine-weighted, facing)


def fit_far(vol, boxes):
    """the outer band's voxels say how big the model looks from each direction: a sphere about c whose blocked
    share of the sphere (1 - cos a) / 2 equals the voxel's k0 has sin^2 a = 1 - (1 - 2 k0)^2, rho^2 = r^2 sin^2 a.
    rho^2(u) is fitted as an L2 SH over the direction u from c (least squares)."""
    c = 0.5 * (boxes[:, 0].min(0) + boxes[:, 1].max(0))
    d = vol.dims
    gz, gy, gx = np.meshgrid(*[np.arange(n) for n in d[::-1]], indexing='ij')
    C = vol.lo + (np.stack([gx, gy, gz], -1).reshape(-1, 3) + 0.5) * vol.cell
    band = fade_of(vol, C) < 1.0
    k0 = vol.k.reshape(-1, vol.k.shape[-1])[band, 0]
    v = C[band] - c
    r = np.linalg.norm(v, axis=1)
    u = v / r[:, None]
    rho2 = r * r * (1.0 - (1.0 - 2.0 * np.clip(k0, 0.0, 0.5)) ** 2)
    Y = sh(u, 2)
    coef = np.linalg.lstsq(Y, rho2, rcond=None)[0]
    probe = fib_sphere(2048)
    rmax = math.sqrt(max(float((sh(probe, 2) @ coef).max()), 1.0))
    return dict(c=c, coef=coef, rcut=rmax / math.sqrt(FAR_CUT))


def far_occ(vol, m, n_m):
    """cosine-weighted occlusion of the equivalent sphere: sin^2 a x max(0, n . toward-center), 0 past rcut"""
    if vol.far is None:
        return np.zeros(len(m))
    v = m - vol.far['c']
    r = np.maximum(np.linalg.norm(v, axis=1), 1e-6)
    u = v / r[:, None]
    rho2 = np.maximum(sh(u, 2) @ vol.far['coef'], 0.0)
    s2 = np.minimum(rho2 / (r * r), 1.0)
    return np.where(r < vol.far['rcut'], s2 * np.maximum(-(n_m * u).sum(1), 0.0), 0.0)


def quantize(vol):
    """the file's bytes for the payload: u8 share, s8 for the rest at one per-file scale"""
    k = vol.k
    cmax = float(max(np.abs(k[..., 1:]).max(), 1e-6))
    q0 = np.clip(np.round(k[..., 0] * 255.0), 0, 255).astype(np.uint8)
    qr = np.clip(np.round(k[..., 1:] / cmax * 127.0), -127, 127).astype(np.int8)
    return q0, qr, cmax


def dequantize(q0, qr, cmax):
    return np.concatenate([q0[..., None].astype(np.float64) / 255.0, qr.astype(np.float64) * (cmax / 127.0)], -1)


# ---------------------------------------------------------------- the runtime lookup
def lookup(vol, m, n_m, lmax=None):
    """AO over the receiver's own hemisphere: m, n_m in the copy's model space. 1 = open, 0 = closed."""
    lmax = vol.lmax if lmax is None else lmax
    nb = (lmax + 1) ** 2
    g = (m - vol.lo) / vol.cell - 0.5
    dims = np.array(vol.dims)
    g = np.clip(g, 0.0, dims - 1.0)
    i0 = np.minimum(np.floor(g).astype(int), dims - 2)
    f = g - i0
    K = np.zeros((len(m), nb))
    for dz in (0, 1):
        for dy in (0, 1):
            for dx in (0, 1):
                w = (f[:, 0] if dx else 1 - f[:, 0]) * (f[:, 1] if dy else 1 - f[:, 1]) * (f[:, 2] if dz else 1 - f[:, 2])
                K += w[:, None] * vol.k[i0[:, 2] + dz, i0[:, 1] + dy, i0[:, 0] + dx, :nb]
    O = K * (2.0 * math.sqrt(math.pi))
    Yn = sh(n_m, 2)[:, :nb]
    occ = np.zeros(len(m))
    for j in range(nb):
        occ += (A_L[band_of(j)] / math.pi) * O[:, j] * Yn[:, j]
    f = fade_of(vol, m)
    return np.clip(1.0 - (occ * f + far_occ(vol, m, n_m) * (1.0 - f)), 0.0, 1.0)


def fade_of(vol, m):
    inner = np.minimum(m - vol.lo, vol.hi - m).min(1)   # distance to the box's nearest face, < 0 outside
    return np.clip(inner / vol.fade, 0.0, 1.0)


def octant_occlusion(vol, m, R):
    """per point: the copy's blocked share of each WORLD octant (the probe's measure), from the volume.
    The world octant's directions are taken into model space (R^T), i.e. the SH rotated by the copy."""
    nb = (vol.lmax + 1) ** 2
    dd = fib_sphere(4096, rand_rot(5))
    Q = np.zeros((8, nb))
    for o in range(8):
        sel = ((dd[:, 0] < 0) * 1 + (dd[:, 1] < 0) * 2 + (dd[:, 2] < 0) * 4) == o
        Q[o] = sh(dd[sel] @ R, vol.lmax).mean(0)      # rows: R^T d for each world d
    g = np.clip((m - vol.lo) / vol.cell - 0.5, 0.0, np.array(vol.dims) - 1.0)
    i0 = np.minimum(np.floor(g).astype(int), np.array(vol.dims) - 2)
    f = g - i0
    K = np.zeros((len(m), nb))
    for dz in (0, 1):
        for dy in (0, 1):
            for dx in (0, 1):
                w = (f[:, 0] if dx else 1 - f[:, 0]) * (f[:, 1] if dy else 1 - f[:, 1]) * (f[:, 2] if dz else 1 - f[:, 2])
                K += w[:, None] * vol.k[i0[:, 2] + dz, i0[:, 1] + dy, i0[:, 0] + dx]
    occ = (K * 2.0 * math.sqrt(math.pi)) @ Q.T
    f = fade_of(vol, m)[:, None]
    far = np.zeros_like(occ)
    if vol.far is not None:   # the equivalent sphere's cap, counted in the world octants it covers
        v = vol.far['c'] - m
        r = np.maximum(np.linalg.norm(v, axis=1), 1e-6)
        w = v / r[:, None]
        rho2 = np.maximum(sh(-w, 2) @ vol.far['coef'], 0.0)
        cosa = np.sqrt(np.maximum(0.0, 1.0 - np.minimum(rho2 / (r * r), 1.0)))
        dm = dd @ R                                       # the same world directions in model space
        oc = (dd[:, 0] < 0) * 1 + (dd[:, 1] < 0) * 2 + (dd[:, 2] < 0) * 4
        inside = (w @ dm.T) >= cosa[:, None]
        inside &= (r < vol.far['rcut'])[:, None]
        far = np.stack([inside[:, oc == o].mean(1) for o in range(8)], 1)
    return np.clip(occ * f + far * (1.0 - f), 0.0, 1.0)


# ---------------------------------------------------------------- a placed copy
class Copy:
    def __init__(self, name, R, t, s=1.0):
        self.name, self.R, self.t, self.s = name, R, np.asarray(t, float), float(s)

    def to_model(self, P):
        return (P - self.t) @ self.R / self.s

    def dir_to_model(self, D):
        return D @ self.R

    def world_corners(self, lo, hi):
        c = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
        return c * self.s @ self.R.T + self.t


def rest_on_ground(name, R, s=1.0, base=0.0, yaw_xy=(0.0, 0.0)):
    """translate so the copy's lowest model point touches z = base"""
    pts = np.concatenate([Copy(name, R, [0, 0, 0], s).world_corners(b[0], b[1]) for b in CAR])
    return Copy(name, R, [yaw_xy[0], yaw_xy[1], base - pts[:, 2].min()], s)


def placements():
    return [
        rest_on_ground('upright', rot(yaw=30)),
        rest_on_ground('tipped90', rot(yaw=-20, roll=90)),
        rest_on_ground('flipped180', rot(yaw=60, roll=180)),
        rest_on_ground('leaning', rot(yaw=15, pitch=8, roll=-25), s=1.2),
        rest_on_ground('stacked', rot(yaw=40), base=150.0),     # on the roof of a car below it (see receivers)
    ]


def world_aabb(cp):
    pts = np.concatenate([cp.world_corners(b[0], b[1]) for b in CAR])
    return pts.min(0), pts.max(0)


def receivers(cp, vol):
    """ground, wall and neighbor points (world) with their normals, inside the copy's volume (fade band out)
    and outside the copy's solid. Returns P, N, kind."""
    lo, hi = world_aabb(cp)
    P, N, K = [], [], []
    ground_z = 0.0
    if cp.name == 'stacked':
        # the neighbor below is an upright car at the origin, yaw 0: its roof is the neighbor surface
        xs, ys = np.meshgrid(np.arange(-215.0, 216.0, 18.0), np.arange(-80.0, 81.0, 16.0))
        P.append(np.stack([xs.ravel(), ys.ravel(), np.full(xs.size, 150.0)], 1))
        N.append(np.tile([0.0, 0.0, 1.0], (xs.size, 1)))
        K += ['neighbor'] * xs.size
    # ground around the copy
    xs, ys = np.meshgrid(np.arange(lo[0] - 360, hi[0] + 361, 24.0), np.arange(lo[1] - 360, hi[1] + 361, 24.0))
    gp = np.stack([xs.ravel(), ys.ravel(), np.full(xs.size, ground_z)], 1)
    if cp.name == 'stacked':
        gp = gp[~inside_any(gp, CAR, pad=0.5)]
    P.append(gp)
    N.append(np.tile([0.0, 0.0, 1.0], (len(gp), 1)))
    K += ['ground'] * len(gp)
    # a wall 70 units off the copy's +Y world side, facing it (-Y), up to 300 high
    wy = hi[1] + 70.0
    xs, zs = np.meshgrid(np.arange(lo[0] - 200, hi[0] + 201, 24.0), np.arange(6.0, 300.0, 24.0))
    P.append(np.stack([xs.ravel(), np.full(xs.size, wy), zs.ravel()], 1))
    N.append(np.tile([0.0, -1.0, 0.0], (xs.size, 1)))
    K += ['wall'] * xs.size
    # a neighbor crate off the -Y side: x in [-120, 120], y in [ly - 230, ly - 80], z in [0, 120]; top and the face
    ly = lo[1]
    xs, ys = np.meshgrid(np.arange(-112.0, 113.0, 16.0), np.arange(ly - 222.0, ly - 79.0, 16.0))
    P.append(np.stack([xs.ravel(), ys.ravel(), np.full(xs.size, 120.0)], 1))
    N.append(np.tile([0.0, 0.0, 1.0], (xs.size, 1)))
    K += ['neighbor'] * xs.size
    xs, zs = np.meshgrid(np.arange(-112.0, 113.0, 16.0), np.arange(6.0, 120.0, 16.0))
    P.append(np.stack([xs.ravel(), np.full(xs.size, ly - 80.0), zs.ravel()], 1))
    N.append(np.tile([0.0, 1.0, 0.0], (xs.size, 1)))
    K += ['neighbor'] * xs.size
    P, N, K = np.concatenate(P), np.concatenate(N), np.array(K)
    m = cp.to_model(P)
    keep = (np.linalg.norm(m - vol.far['c'], axis=1) < vol.far['rcut']) & ~inside_any(m, CAR, pad=0.5)
    return P[keep], N[keep], K[keep]


_BRUTE = {}


def brute_ao(cp, P, N, rays=4096, seed=17):
    """cosine-weighted occlusion by the copy alone, its own direction set, in world terms (kept per run: the
    reds reuse the green's truth)"""
    key = (cp.name, rays, seed, hash(P.tobytes()), hash(N.tobytes()), hash(cp.R.tobytes()))
    if key not in _BRUTE:
        _BRUTE[key] = _brute_ao(cp, P, N, rays, seed)
    return _BRUTE[key]


def _brute_ao(cp, P, N, rays, seed):
    D = fib_sphere(rays, rand_rot(seed))                # world directions
    m = cp.to_model(P)
    Dm = cp.dir_to_model(D)
    blk = hits_any(m, Dm, CAR)
    cosw = np.maximum(N @ D.T, 0.0)
    return 1.0 - (blk * cosw).sum(1) / cosw.sum(1)


def lookup_copy(vol, cp, P, N, red_worlddown=False, lmax=None):
    if not red_worlddown:
        return lookup(vol, cp.to_model(P), cp.dir_to_model(N), lmax)
    # the projected decal: keep only the copy's yaw (its model X axis flattened), ignore tilt and scale,
    # and take every receiver as facing world up
    ax = cp.R[:, 0].copy()
    ax[2] = 0.0
    if np.linalg.norm(ax) < 1e-6:
        ax = np.array([1.0, 0.0, 0.0])
    yaw = math.degrees(math.atan2(ax[1], ax[0]))
    up = Copy(cp.name, rot(yaw=yaw), cp.t, 1.0)
    return lookup(vol, up.to_model(P), np.tile([0.0, 0.0, 1.0], (len(P), 1)), lmax)


# ---------------------------------------------------------------- A
ROTATED = ('tipped90', 'flipped180', 'leaning')   # stacked is upright (yaw only), on another car


def check_a(vol, red_worlddown=False):
    """far set: points at least one voxel (world) from the model; near set: the contact band inside it"""
    lines, ok_all, rotated_fail = [], True, 0
    for cp in placements():
        P, N, K = receivers(cp, vol)
        bf = brute_ao(cp, P, N)
        got = lookup_copy(vol, cp, P, N, red_worlddown)
        e = np.abs(got - bf)
        dm = dist_to_boxes(cp.to_model(P), CAR)
        near = dm < vol.cell.max()
        ef, en = e[~near], e[near]
        ok_far = ef.mean() <= BAR_A_MEAN and np.percentile(ef, 95) <= BAR_A_P95 and ef.max() <= BAR_A_MAX
        ok_near = (not near.any()) or (en.mean() <= BAR_AN_MEAN and np.percentile(en, 95) <= BAR_AN_P95)
        ok = ok_far and ok_near
        ok_all &= ok
        if cp.name in ROTATED and not ok:
            rotated_fail += 1
        kinds = ' '.join('%s %d' % (k, int((K == k).sum())) for k in ('ground', 'wall', 'neighbor'))
        extra = ''
        if not red_worlddown:
            l1 = np.abs(lookup_copy(vol, cp, P, N, lmax=1) - bf)
            extra = '\n             (L1 only, same points: far mean %.4f p95 %.4f max %.3f; near p95 %.3f)' % (
                l1[~near].mean(), np.percentile(l1[~near], 95), l1[~near].max(),
                np.percentile(l1[near], 95) if near.any() else 0.0)
        lines.append('  %-10s %4d pts (%s), brute-force AO %.3f..%.3f\n'
                     '             far  %4d: |err| mean %.4f p95 %.4f max %.3f; near %3d: mean %.4f p95 %.4f max %.3f %s%s'
                     % (cp.name, len(P), kinds, bf.min(), bf.max(), len(ef), ef.mean(), np.percentile(ef, 95),
                        ef.max(), len(en), en.mean() if len(en) else 0.0, np.percentile(en, 95) if len(en) else 0.0,
                        en.max() if len(en) else 0.0, 'ok' if ok else 'BAD', extra))
    head = ('A %s lookup in the copy\'s model space vs brute force (bars far: mean %.2f p95 %.2f max %.2f; near one '
            'voxel: mean %.2f p95 %.2f)%s' % ('PASS' if ok_all else 'FAIL', BAR_A_MEAN, BAR_A_P95, BAR_A_MAX,
                                              BAR_AN_MEAN, BAR_AN_P95,
                                              ' [red worlddown: %d of %d rotated placements fail]' % (rotated_fail, len(ROTATED))
                                              if red_worlddown else ''))
    return ok_all, [head] + lines, rotated_fail


# ---------------------------------------------------------------- B
def check_b(vol, far_on=True):
    """past the volume: the far field against brute force, from the box surface out to the cut, normals toward
    the model and straight up; and what is dropped at the cut"""
    rng = np.random.default_rng(3)
    c, rcut = vol.far['c'], vol.far['rcut']
    U = rng.normal(size=(1500, 3))
    U /= np.linalg.norm(U, axis=1, keepdims=True)
    # the ray from c along U leaves the volume box at t0; sample t in [t0 - fade, rcut]
    with np.errstate(divide='ignore'):
        tb = np.min(np.where(U > 0, (vol.hi - c) / np.where(U > 0, U, 1), (vol.lo - c) / np.where(U < 0, U, 1)), 1)
    t = tb - vol.fade + rng.uniform(0, 1, len(U)) * (rcut - (tb - vol.fade))
    P = c + U * t[:, None]
    toward = -U
    up = np.tile([0.0, 0.0, 1.0], (len(U), 1))
    cp = Copy('model', np.eye(3), [0, 0, 0])
    lv = vol if far_on else Volume(vol.lo, vol.hi, vol.dims, vol.fade, vol.k, vol.lmax, None)   # red nofar
    res = []
    for name, N in (('toward', toward), ('up', up)):
        bf = brute_ao(cp, P, N, rays=2048)
        got = lookup(lv, P, N)
        res.append((name, np.abs(got - bf), 1.0 - bf))
    # what the cut drops: brute-force occlusion just past rcut, facing the model
    Pc = c + U * (rcut * 1.01)
    drop = 1.0 - brute_ao(cp, Pc, -U, rays=2048)
    e_all = np.concatenate([r[1] for r in res])
    ok = e_all.mean() <= BAR_B_MEAN and e_all.max() <= BAR_B_MAX and drop.max() <= BAR_B_DROP
    lines = ['B %s past the volume box (far field%s): %d points from the fade band to the cut (rcut %.0f = %.1f x the '
             'box half-diagonal); bars mean %.3f max %.2f, dropped at the cut %.3f'
             % ('PASS' if ok else 'FAIL', '' if far_on else ' OFF: red nofar', len(P), rcut,
                rcut / (0.5 * np.linalg.norm(vol.hi - vol.lo)), BAR_B_MEAN, BAR_B_MAX, BAR_B_DROP)]
    for name, e, occ in res:
        lines.append('  normal %-6s true occlusion up to %.3f; |err| mean %.4f p95 %.4f max %.3f'
                     % (name, occ.max(), e.mean(), np.percentile(e, 95), e.max()))
    lines.append('  dropped just past the cut (facing the model): max %.4f mean %.4f' % (drop.max(), drop.mean()))
    return ok, lines


# ---------------------------------------------------------------- D: the probes, and dividing out what they saw
PROBE_STEP, PROBE_UP, PROBE_RAYS = 280.0, 120.0, 1024


def place_probes(cp):
    """FO4CS's lattice (280-unit columns on a global grid), a probe 120 over each surface that opens to an air
    gap of at least 140 going up (the column descent), over ground + the copy"""
    lo, hi = world_aabb(cp)
    xs = np.arange(math.floor((lo[0] - 700) / PROBE_STEP), math.ceil((hi[0] + 700) / PROBE_STEP) + 1) * PROBE_STEP
    ys = np.arange(math.floor((lo[1] - 700) / PROBE_STEP), math.ceil((hi[1] + 700) / PROBE_STEP) + 1) * PROBE_STEP
    out = []
    for x in xs:
        for y in ys:
            o = np.array([x, y, 5000.0])
            m = cp.to_model(o[None])[0]
            dm = cp.dir_to_model(np.array([[0.0, 0.0, -1.0]]))[0]
            spans = []
            for blo, bhi in CAR:
                inv = 1.0 / np.where(np.abs(dm) < 1e-12, 1e-12, dm)
                t1, t2 = (blo - m) * inv, (bhi - m) * inv
                tn, tf = np.minimum(t1, t2).max(), np.maximum(t1, t2).min()
                if tf >= max(tn, 0.0):
                    spans.append((5000.0 - tf * cp.s, 5000.0 - tn * cp.s))   # world z interval (lo, hi)
            spans.sort(key=lambda s: -s[1])
            merged = []
            for s in spans:
                if merged and s[1] >= merged[-1][0]:
                    merged[-1] = (min(merged[-1][0], s[0]), merged[-1][1])
                else:
                    merged.append(s)
            top = 5000.0
            floors = []
            for zlo, zhi in merged:
                floors.append((zhi, top))
                top = zlo
            floors.append((0.0, top))
            for k, (fl, ce) in enumerate(floors):
                if k == 0 or ce - fl >= 140.0:
                    out.append([x, y, fl + min(PROBE_UP, 0.5 * (ce - fl) if k else PROBE_UP)])
    return np.array(out)


def probe_octants(cp, Pp, with_copy=True):
    D = fib_sphere(PROBE_RAYS, rand_rot(23))
    oc = (D[:, 0] < 0) * 1 + (D[:, 1] < 0) * 2 + (D[:, 2] < 0) * 4
    open_ = np.tile(D[:, 2] > 0, (len(Pp), 1))                 # the ground plane closes the lower half
    if with_copy:
        open_ &= ~hits_any(cp.to_model(Pp), cp.dir_to_model(D), CAR)
    return np.stack([open_[:, oc == o].mean(1) for o in range(8)], 1)


AX = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)


def axis_sky(oct8):
    """the relight's six-axis sky share: the mean of the four octants on the axis's side"""
    out = np.zeros((len(oct8), 6))
    for a in range(6):
        ax, sgn = a // 2, (1 if a % 2 == 0 else -1)
        sel = [o for o in range(8) if ((o >> ax) & 1) == (0 if sgn > 0 else 1)]
        out[:, a] = oct8[:, sel].mean(1)
    return out


def blend(cp, Pp, cubes, P, N):
    """the grid's blend evaluated at the receiver: probes within r it can see, (1 - d^2/r^2)^2, then the
    ambient cube on the normal (n_x^2 of +X or -X ...)"""
    r = 2.0 * PROBE_STEP
    E = np.zeros(len(P))
    W = np.zeros(len(P))
    for i in range(len(P)):
        d2 = ((Pp - P[i]) ** 2).sum(1)
        j = np.nonzero(d2 < r * r)[0]
        if len(j) == 0:
            continue
        A = np.repeat(cp.to_model(P[i:i + 1] + N[i:i + 1] * 2.0), len(j), 0)
        vis = ~seg_blocked(A, cp.to_model(Pp[j]), CAR)
        j = j[vis]
        if len(j) == 0:
            continue
        w = (1.0 - d2[j] / (r * r)) ** 2
        c = (w[:, None] * cubes[j]).sum(0) / w.sum()
        n = N[i]
        E[i] = sum(n[a] ** 2 * c[2 * a + (0 if n[a] >= 0 else 1)] for a in range(3))
        W[i] = w.sum()
    return E, W


def check_d(vol, red_nodivide=False):
    """pooled over the placements: the receivers whose probes saw the copy (G < 0.95) carry the verdict"""
    lines, errs, saws, exact_saws = [], [], [], []
    p95_ok = True
    for cp in placements()[:4]:
        P, N, K = receivers(cp, vol)
        rng = np.random.default_rng(29)
        pick = rng.choice(len(P), size=min(360, len(P)), replace=False)
        P, N, K = P[pick], N[pick], K[pick]
        Pp = place_probes(cp)
        s_with = probe_octants(cp, Pp, True)
        s_free = probe_octants(cp, Pp, False)                   # the truth: the probes with the copy taken out
        # what the probes captured of the copy, rebuilt from the copy's own volume at each probe
        occ8 = octant_occlusion(vol, cp.to_model(Pp), cp.R)
        s_rec = np.where(s_with > 0, np.minimum(1.0, s_with / np.maximum(1.0 - occ8, G_FLOOR)), 0.0)
        Ew, Wt = blend(cp, Pp, axis_sky(s_with), P, N)
        Er, _ = blend(cp, Pp, axis_sky(s_rec), P, N)
        Ef, _ = blend(cp, Pp, axis_sky(s_free), P, N)
        ao = lookup_copy(vol, cp, P, N)
        ao_bf = brute_ao(cp, P, N)
        G = np.where(Er > 1e-6, Ew / np.maximum(Er, 1e-6), 1.0)
        Gx = np.where(Ef > 1e-6, Ew / np.maximum(Ef, 1e-6), 1.0)   # the exact divisor (a second bake trace)
        final = Ew * (ao if red_nodivide else ao / np.maximum(G, 0.05))
        final_x = Ew * ao / np.maximum(Gx, 0.05)
        target = Ef * ao_bf
        sel = (Wt > 0) & (Ef > 0.02)
        e = (final - target)[sel]
        saw = (G < 0.95)[sel]
        errs.append(e)
        saws.append(saw)
        exact_saws.append((final_x - target)[sel][saw])
        p95 = np.percentile(np.abs(e), 95)
        p95_ok &= p95 <= BAR_D_P95
        lines.append('  %-10s %3d probes (%3d see the copy), %3d receivers (%3d whose probes saw it, G min %.2f): '
                     '|final - target| mean %.4f p95 %.4f max %.3f; signed mean where probes saw it %+.4f\n'
                     '             copy-free probe sky rebuilt from the volume vs traced (per axis): mean %.4f max %.3f'
                     % (cp.name, len(Pp), int((s_with.sum(1) < s_free.sum(1) - 1e-9).sum()), int(sel.sum()),
                        int(saw.sum()), G[sel].min(), np.abs(e).mean(), p95, np.abs(e).max(),
                        e[saw].mean() if saw.any() else 0.0,
                        np.abs(axis_sky(s_rec) - axis_sky(s_free)).mean(),
                        np.abs(axis_sky(s_rec) - axis_sky(s_free)).max()))
    e, saw = np.concatenate(errs), np.concatenate(saws)
    bias = float(e[saw].mean())
    worst = float(e[saw].min())
    ok = p95_ok and saw.sum() >= 100 and bias >= BAR_D_BIAS and worst >= BAR_D_WORST
    ex = np.concatenate(exact_saws)
    head = ('D %s no double darkening: probe sky x AO / G vs copy-free probe sky x brute-force AO%s\n'
            '  pooled: %d receivers whose probes saw the copy: signed error mean %+.4f (bar >= %+.2f), most negative '
            '%+.3f (bar >= %+.2f); every placement p95 |err| <= %.2f: %s\n'
            '  information: with the exact divisor (copy-free sky from a second bake trace) the same receivers: '
            'mean %+.4f, most negative %+.3f'
            % ('PASS' if ok else 'FAIL', ' [red nodivide]' if red_nodivide else '', int(saw.sum()), bias,
               BAR_D_BIAS, worst, BAR_D_WORST, BAR_D_P95, 'yes' if p95_ok else 'NO', ex.mean(), ex.min()))
    return ok, [head] + lines


# ---------------------------------------------------------------- E: the .ao file and the index
AO_MAGIC, AOI_MAGIC = 0x4F415757, 0x49415757   # 'WWAO', 'WWAI' little endian
AO_HEAD = 128


def fnv1a64(b):
    h = 0xcbf29ce484222325
    for c in b:
        h = ((h ^ c) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def norm_path(p):
    return p.replace('/', '\\').lower().lstrip('\\')


def write_ao(vol, fingerprint, rays):
    q0, qr, cmax = quantize(vol)
    nb = (vol.lmax + 1) ** 2
    pay = np.concatenate([q0[..., None].view(np.uint8), qr.view(np.uint8)], -1).tobytes()   # z, y, x, coeff
    head = struct.pack('<IHHHHHH', AO_MAGIC, 1, 2 if vol.lmax == 2 else 1, vol.dims[0], vol.dims[1], vol.dims[2], nb)
    head += struct.pack('<6f', *vol.lo, *vol.hi)
    head += struct.pack('<ffI', vol.fade, cmax, rays)
    head += struct.pack('<3f', *vol.far['c'])
    head += struct.pack('<9f', *vol.far['coef'])
    head += struct.pack('<f', vol.far['rcut'])
    head += struct.pack('<QI', fingerprint, zlib.crc32(pay))
    assert len(head) == 116
    head += b'\0' * (AO_HEAD - len(head))
    return head + pay


def footprint(vol):
    """the model-space box a copy can darken: the volume box and the far field's cut sphere"""
    c, r = vol.far['c'], vol.far['rcut']
    return np.minimum(vol.lo, c - r), np.maximum(vol.hi, c + r)


def write_index(entries):
    """entries: (model path, fingerprint, lo, hi, ao bytes). Records sorted by path hash."""
    strings = b''
    recs = []
    for path, fp, lo, hi, ao in entries:
        p = norm_path(path).encode('utf-8')
        recs.append((fnv1a64(p), fp, len(strings), len(p), lo, hi, len(ao), zlib.crc32(ao)))
        strings += p + b'\0'
    recs.sort(key=lambda r: r[0])
    out = struct.pack('<IHHII', AOI_MAGIC, 1, 0, len(recs), 32 + 64 * len(recs)) + b'\0' * 16
    for h, fp, off, ln, lo, hi, sz, crc in recs:
        out += struct.pack('<QQIHH6fII', h, fp, off, ln, 0, *lo, *hi, sz, crc) + b'\0' * 8
    return out + strings


def read_index(b):
    """a separate reader: the record table by offsets, nothing shared with the writer but the layout"""
    magic, ver, _, n, soff = struct.unpack_from('<IHHII', b, 0)
    assert magic == AOI_MAGIC and ver == 1 and soff == 32 + 64 * n
    out = {}
    for i in range(n):
        o = 32 + 64 * i
        h, fp = struct.unpack_from('<QQ', b, o)
        off, ln = struct.unpack_from('<IH', b, o + 16)
        lo = struct.unpack_from('<3f', b, o + 24)
        hi = struct.unpack_from('<3f', b, o + 36)
        sz, crc = struct.unpack_from('<II', b, o + 48)
        path = b[soff + off:soff + off + ln].decode('utf-8')
        assert fnv1a64(path.encode('utf-8')) == h
        out[path] = dict(fp=fp, lo=np.array(lo), hi=np.array(hi), size=sz, crc=crc)
    return out


def read_ao(b, rec, nif_fingerprint, red_stale=False):
    """refusals: wrong size, CRC, or a model whose fingerprint is not the one baked (None = refused)"""
    if len(b) != rec['size'] or zlib.crc32(b) != rec['crc']:
        return None, 'size/crc against the index'
    magic, ver, enc, dx, dy, dz, nb = struct.unpack_from('<IHHHHHH', b, 0)
    if magic != AO_MAGIC or ver != 1:
        return None, 'magic'
    lo = np.array(struct.unpack_from('<3f', b, 16))
    hi = np.array(struct.unpack_from('<3f', b, 28))
    fade, cmax, rays = struct.unpack_from('<ffI', b, 40)
    c = np.array(struct.unpack_from('<3f', b, 52))
    coef = np.array(struct.unpack_from('<9f', b, 64))
    rcut, = struct.unpack_from('<f', b, 100)
    fp, crc = struct.unpack_from('<QI', b, 104)
    pay = b[AO_HEAD:]
    if len(pay) != dx * dy * dz * nb or zlib.crc32(pay) != crc:
        return None, 'payload'
    if not red_stale and (fp != nif_fingerprint or fp != rec['fp']):
        return None, 'stale: the model changed since the bake'
    a = np.frombuffer(pay, np.uint8).reshape(dz, dy, dx, nb)
    k = dequantize(a[..., 0], a[..., 1:].view(np.int8), cmax)
    return Volume(lo, hi, (dx, dy, dz), fade, k, 2 if enc == 2 else 1, dict(c=c, coef=coef, rcut=rcut)), 'ok'


def check_e(vol, red_stale=False):
    nif_bytes = CAR.astype('<f4').tobytes()                       # the synthetic "model file"
    fp = int.from_bytes(hashlib.sha256(nif_bytes).digest()[:8], 'little')
    path = 'Meshes\\SetDressing\\SynthCar01.nif'                  # a made-up path, no game data
    ao = write_ao(vol, fp, BAKE_RAYS)
    flo, fhi = footprint(vol)
    idx = write_index([(path, fp, flo, fhi, ao)])
    nb = (vol.lmax + 1) ** 2
    want = AO_HEAD + int(np.prod(vol.dims)) * nb
    rec = read_index(idx)[norm_path(path)]
    v2, why = read_ao(ao, rec, fp, red_stale)
    good = v2 is not None and len(ao) == want and np.allclose(v2.lo, vol.lo, atol=1e-3) \
        and np.allclose(rec['lo'], flo, atol=1e-2) and np.allclose(rec['hi'], fhi, atol=1e-2)
    err = 0.0
    if v2 is not None:
        q = dequantize(*quantize(vol))
        err = float(np.abs(v2.k - q).max())
        good &= err < 1e-6 and np.allclose(v2.far['coef'], vol.far['coef'], rtol=1e-6, atol=1e-3)
        # the read-back volume answers the same lookups (float32 header fields only)
        m = np.random.default_rng(1).uniform(rec['lo'], rec['hi'], size=(500, 3))
        n = np.random.default_rng(2).normal(size=(500, 3))
        n /= np.linalg.norm(n, axis=1, keepdims=True)
        err = max(err, float(np.abs(lookup(v2, m, n) - lookup(vol.with_k(q), m, n)).max()))
        good &= err < 1e-4
    # negative controls: a flipped byte, a stale model
    bad = bytearray(ao)
    bad[AO_HEAD + 1000] ^= 0x40
    r1 = read_ao(bytes(bad), dict(rec, crc=zlib.crc32(bytes(bad))), fp, red_stale)[0] is None
    r2 = read_ao(ao, rec, fp ^ 1, red_stale)[0] is None
    ok = good and r1 and r2
    return ok, ['E %s file: .ao %d bytes (128 + %d voxels x %d) = %.1f KB, index %d bytes; read back %s, max '
                'coefficient / lookup diff %.1e; flipped payload byte refused: %s; stale model fingerprint refused: %s%s'
                % ('PASS' if ok else 'FAIL', len(ao), int(np.prod(vol.dims)), nb, len(ao) / 1024.0, len(idx), why,
                   err, 'yes' if r1 else 'NO', 'yes' if r2 else 'NO', ' [red stale]' if red_stale else '')]


# ---------------------------------------------------------------- the run
_BAKED = {}


def run(red=None, quiet=False):
    t0 = time.time()
    if 'vol' not in _BAKED:
        tb = time.time()
        full = bake(CAR, MARGIN_K)
        _BAKED['full'] = full
        _BAKED['vol'] = full.with_k(dequantize(*quantize(full)))   # every check reads the file's quantized values
        _BAKED['sec'] = time.time() - tb
    vol_full, vol = _BAKED['full'], _BAKED['vol']
    out, oks = [], []
    if red in (None, 'worlddown'):
        ok, lines, rf = check_a(vol, red == 'worlddown')
        if red == 'worlddown':
            ok = rf < len(ROTATED)   # the red is caught only when every rotated placement fails
        oks.append(ok)
        out += lines
    if red in (None, 'nofar'):
        ok, lines = check_b(vol, far_on=red is None)
        oks.append(ok)
        out += lines
    if red in (None, 'nodivide'):
        ok, lines = check_d(vol, red == 'nodivide')
        oks.append(ok)
        out += lines
    if red in (None, 'stale'):
        ok, lines = check_e(vol, red == 'stale')
        oks.append(ok)
        out += lines
    if not quiet:
        for l in out:
            print(l)
    return all(oks), time.time() - t0


REDS = ['worlddown', 'nofar', 'nodivide', 'stale']

if __name__ == '__main__':
    one = os.environ.get('AODECAL1_RED')
    if one:
        if one not in REDS:
            print('unknown red %s (%s)' % (one, ' '.join(REDS)))
            sys.exit(2)
        ok, sec = run(one)
        print('red %s: %s (%.0f s)' % (one, 'PASS (the red did not fail: BAD)' if ok else 'FAIL (as it must)', sec))
        sys.exit(0 if ok else 1)
    print('== green')
    ok, sec = run(None)
    print('green %s (%.0f s)' % ('PASS' if ok else 'FAIL', sec))
    reds_ok = True
    for r in REDS:
        print('== red %s' % r)
        rok, rsec = run(r)
        print('red %s: %s (%.0f s)' % (r, 'FAIL as it must' if not rok else 'PASSED: the check cannot see it (BAD)',
                                       rsec))
        reds_ok &= not rok
    print('aodecal1 %s: green %s, reds %s' % ('PASS' if ok and reds_ok else 'FAIL', 'PASS' if ok else 'FAIL',
                                             'all fail' if reds_ok else 'NOT all fail'))
    sys.exit(0 if ok and reds_ok else 1)
