#!/usr/bin/env python3
"""Volumetric fog lit by the probe GI: the Python twin (lane VOLFOG1, 2026-10-03; docs/cloud/VOLFOG1_DESIGN.md).

  python3 tests/spells/volfog1_check.py              green run, then every red (each must FAIL its check)
  python3 tests/spells/volfog1_check.py --red NAME   one red only (exit 1 when it fails, as it must)
  VOLFOG1_RED=NAME python3 tests/spells/volfog1_check.py   the same as --red NAME

No game data. Every scene is a closed box built here, in game units (512 x 512 x 320, about 7.3 x 7.3 x 4.6 m),
lit by one white point light with the PRTP2 curve (1 - (d/r)^2)^2.2, or by a sun through a hole in the roof.

Two independent paths, sharing only the scene description (box, albedos, light, fog constants):

  REFERENCE (brute force)  exact radiosity of the box from point-to-polygon form factors (about 1,800 patches,
                           solved directly), then for each camera ray 192 points along it, each gathering 4,096
                           directions to the walls (randomly rotated per point) with the exact HG phase and
                           the fog's transmittance to the wall, plus the light (HG, transmittance to the light).
  ESTIMATE (what the       surfels in ~70-unit cells, a probe lattice baked with 2,048 Fibonacci rays (links: weight,
  shader would do)         mean direction, mean distance), the probegi relight with the BOUNCE2 feedback until it
                           settles, an AIR grid (64-unit voxels, probes blended within r by (1 - d^2/r^2)^2), then a
                           camera froxel grid (1080p, 12 px tiles = 160 x 90, 64 exponential slices 16..16384), per
                           froxel  S = direct (HG) + GI from the six-axis cube read as L0 + L1:
                               GI = sigma_s / (4 pi) * max(0, (2/3) sum E_axis + 3 g (E_+ - E_-) . d) * exp(-sigma_t dbar)
                           integrated front to back per slice with  (S - S exp(-sigma_t D)) / sigma_t.

Checks (PASS/FAIL each; bars fixed before the first run, except K8):
  K0 reference   form-factor rows of every patch sum to 1 (closed box)                      |1 - row| <= 1e-6
  K1 furnace     walls glowing 1 everywhere, no lamp: fog along every ray = sigma_s x length
                 (phase normalization, the 2/3 and the units), estimate AND reference          rel <= 0.01
  K2 probes      estimate's probe cubes vs the reference's irradiance at the probe points       median <= 0.05, worst <= 0.12
  K3 radiance    red room: fog radiance (R+G+B) per camera ray, estimate vs reference           rel <= 0.10 every ray
  K4 tint        red room: fog chromaticity r = R/(R+G+B), estimate vs reference                |dr| <= 0.02 every ray
                 floor: the reference's own tint r - 1/3 >= 0.05 (the scene has a tint to keep)
  K5 grey        grey room: no tint in either path                                              |r - 1/3| <= 0.005
  K6 follows     red/green room: per ray (r - g), estimate vs reference                         |d(r-g)| <= 0.03
                 floor: the reference's (r - g) spans >= 0.06 across the rays (the tint moves)
  K7 shafts      dark room, sun through a roof hole, shadow map 8 u texels + bilinear PCF + entry-depth channel,
                 jitter + history 0.95: per ray, worst over the last 16 frames, |est - ref| / max ref  <= 0.10
                 floor: a ray that misses the shaft reads < 0.01 of the brightest (reference)
  K8 temporal    a 32-unit sheet of sun (roof slot), a cheap 24-slice grid, the exact sun test (isolates depth
                 sampling): floor: one fixed sample per slice is off by >= 0.5 (the sheet aliases); jitter (16-frame
                 van der Corput) + history 0.95, worst shown frame of the last 16                  <= 0.25
                 (K8's form and bar were set after its first run; docs/cloud/VOLFOG1_DESIGN.md section 10)

Reds (each must make its check FAIL; the default run proves it):
  gioff     the froxels take no GI (direct only)             -> K3/K4 FAIL (the red haze is gone)
  flat      one GI cube for every froxel (the room's mean)   -> K6 FAIL (the tint no longer follows the walls)
  noshadow  the sun's shadow map is ignored in the fog       -> K7 FAIL
  nojitter  K8's accumulated arm without jitter              -> K8 FAIL
"""
import math
import os
import sys
import time

import numpy as np

BOX = np.array([512.0, 512.0, 320.0])
FACES = [(0, 0), (0, 1), (1, 0), (1, 1), (2, 0), (2, 1)]   # (axis, side): -X wall, +X wall, -Y, +Y, floor, ceiling
AXES = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)

# the fog: 0.02 per metre (1 m = 70 units), single-scatter albedo 0.9, mild forward HG
SIGMA_T = 0.02 / 70.0
SIGMA_S = 0.9 * SIGMA_T
G = 0.3

LIGHT_POS = np.array([256.0, 300.0, 280.0])
LIGHT_C = np.array([1.0, 1.0, 1.0])
LIGHT_R = 1000.0

HISTORY = 0.95       # the froxel history weight (temporal reprojection; a static camera here)
AIR_RADIUS = 1.5      # the air grid's probe blend radius, x the median probe spacing (probegi's surface grid: 2;
                      # 2 loses a fifth of the tint near a coloured wall, measured: design doc, section 6)
GREY = (0.45, 0.45, 0.45)
RED = (0.75, 0.10, 0.08)
GREEN = (0.10, 0.65, 0.10)
SCENES = {
    'red':   [RED, RED, RED, RED, GREY, GREY],
    'grey':  [GREY] * 6,
    'split': [GREEN, RED, GREY, GREY, GREY, GREY],
}

# the camera (1080p, 90 degrees across), the froxel grid
CAM_POS = np.array([256.0, 20.0, 160.0])
CAM_FWD = np.array([0.0, 1.0, 0.0])
CAM_UP = np.array([0.0, 0.0, 1.0])
W, H = 1920, 1080
TILE = 12
NEAR, FAR, SLICES = 16.0, 16384.0, 64


# ---------------------------------------------------------------- shared geometry helpers (scene description only)
def face_normal(f):
    a, s = FACES[f]
    n = np.zeros(3)
    n[a] = 1.0 if s == 0 else -1.0
    return n


def face_uv(f):
    a, _ = FACES[f]
    return [i for i in range(3) if i != a]


def box_exit(P, D):
    """From points inside the box along unit directions: distance, face, hit point."""
    with np.errstate(divide='ignore', invalid='ignore'):
        tt = np.where(D > 0, (BOX - P) / D, np.where(D < 0, -P / D, np.inf))
    a = np.argmin(tt, axis=1)
    idx = np.arange(len(P))
    t = tt[idx, a]
    side = (D[idx, a] > 0).astype(int)
    return t, a * 2 + side, P + D * t[:, None]


def fibonacci(n):
    i = np.arange(n) + 0.5
    z = 1.0 - 2.0 * i / n
    r = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    phi = i * math.pi * (3.0 - math.sqrt(5.0))
    return np.stack([r * np.cos(phi), r * np.sin(phi), z], 1)


def atten(d):
    x = np.clip(d / LIGHT_R, 0.0, 1.0)
    return (1.0 - x * x) ** 2.2


def hg(cos_t, g=G):
    return (1.0 - g * g) / (4.0 * math.pi * (1.0 + g * g - 2.0 * g * cos_t) ** 1.5)


def camera_rays(tiles):
    right = np.cross(CAM_FWD, CAM_UP)
    tanx = math.tan(math.radians(45.0))
    tany = tanx * H / W
    out = []
    for (i, j) in tiles:
        px = ((i + 0.5) * TILE) / W * 2.0 - 1.0
        py = 1.0 - ((j + 0.5) * TILE) / H * 2.0
        d = CAM_FWD + right * px * tanx + CAM_UP * py * tany
        out.append(d / np.linalg.norm(d))
    return np.array(out)


TILES = [(80, 45), (10, 45), (150, 45), (80, 8), (80, 82), (30, 70), (130, 20)]


# ================================================================ REFERENCE: exact radiosity + brute-force march
class Reference:
    PATCH = 25.6

    def __init__(self, albedo):
        cen, corn, nrm, alb, face, grid = [], [], [], [], [], []
        self.offset = []
        for f in range(6):
            a, s = FACES[f]
            u, v = face_uv(f)
            nu = int(round(BOX[u] / self.PATCH))
            nv = int(round(BOX[v] / self.PATCH))
            self.offset.append((len(cen), nu, nv))
            du, dv = BOX[u] / nu, BOX[v] / nv
            for iu in range(nu):
                for iv in range(nv):
                    c = np.zeros(3)
                    c[a] = BOX[a] * s
                    c[u], c[v] = (iu + 0.5) * du, (iv + 0.5) * dv
                    q = []
                    for (ku, kv) in ((0, 0), (1, 0), (1, 1), (0, 1)):
                        p = c.copy()
                        p[u], p[v] = (iu + ku) * du, (iv + kv) * dv
                        q.append(p)
                    cen.append(c)
                    corn.append(q)
                    nrm.append(face_normal(f))
                    alb.append(albedo[f])
                    face.append(f)
                    grid.append((du, dv))
        self.cen = np.array(cen)
        self.corn = np.array(corn)
        self.nrm = np.array(nrm)
        self.alb = np.array(alb, float)
        self.face = np.array(face)
        self.n = len(cen)
        self.F = self.form_factors()

    def form_factors(self):
        F = np.zeros((self.n, self.n))
        for c0 in range(0, self.n, 192):
            X = self.cen[c0:c0 + 192]
            Nr = self.nrm[c0:c0 + 192]
            acc = np.zeros((len(X), self.n))
            for k in range(4):
                a = self.corn[None, :, k, :] - X[:, None, :]
                b = self.corn[None, :, (k + 1) % 4, :] - X[:, None, :]
                an = a / np.linalg.norm(a, axis=2, keepdims=True)
                bn = b / np.linalg.norm(b, axis=2, keepdims=True)
                cr = np.cross(a, b)
                crl = np.linalg.norm(cr, axis=2, keepdims=True)
                crn = cr / np.maximum(crl, 1e-30)
                th = np.arccos(np.clip(np.einsum('ijk,ijk->ij', an, bn), -1.0, 1.0))
                acc += th * np.einsum('ijk,ik->ij', crn, Nr)
            Fi = np.abs(acc) / (2.0 * math.pi)
            same = self.face[c0:c0 + 192][:, None] == self.face[None, :]
            Fi[same] = 0.0
            F[c0:c0 + 192] = Fi
        return F

    def direct_E(self):
        E = np.zeros(self.n)
        for su in (0.125, 0.375, 0.625, 0.875):
            for sv in (0.125, 0.375, 0.625, 0.875):
                P = self.corn[:, 0] + (self.corn[:, 1] - self.corn[:, 0]) * su + (self.corn[:, 3] - self.corn[:, 0]) * sv
                v = LIGHT_POS - P
                d = np.linalg.norm(v, axis=1)
                nl = np.maximum(np.einsum('ij,ij->i', self.nrm, v / d[:, None]), 0.0)
                E += atten(d) * nl / 16.0
        return E[:, None] * LIGHT_C[None, :]

    def solve(self, B_override=None):
        if B_override is not None:
            self.B = np.full((self.n, 3), float(B_override))
            return
        E = self.direct_E()
        self.B = np.zeros((self.n, 3))
        for c in range(3):
            A = np.eye(self.n) - self.alb[:, c:c + 1] * self.F
            self.B[:, c] = np.linalg.solve(A, self.alb[:, c] * E[:, c])

    def patch_of(self, face, hit):
        out = np.zeros(len(face), int)
        for f in range(6):
            m = face == f
            if not m.any():
                continue
            u, v = face_uv(f)
            o, nu, nv = self.offset[f]
            iu = np.clip((hit[m, u] / (BOX[u] / nu)).astype(int), 0, nu - 1)
            iv = np.clip((hit[m, v] / (BOX[v] / nv)).astype(int), 0, nv - 1)
            out[m] = o + iu * nv + iv
        return out

    def irradiance_axes(self, P, n=16384):
        D = fibonacci(n)
        Om = 4.0 * math.pi / n
        out = []
        for p in P:
            t, f, h = box_exit(np.repeat(p[None], n, 0), D)
            B = self.B[self.patch_of(f, h)]
            out.append([(B * (np.maximum(D @ ax, 0.0) * Om)[:, None]).sum(0) for ax in AXES])
        return np.array(out)

    def fog_ray(self, o, d, rng, ns=192, ndir=4096, light=True, sun=None, sigma_t=SIGMA_T, sigma_s=SIGMA_S):
        tw = box_exit(o[None], d[None])[0][0]
        dt = tw / ns
        t = (np.arange(ns) + 0.5) * dt
        L = np.zeros(3)
        base = fibonacci(ndir)
        Om = 4.0 * math.pi / ndir
        for k in range(ns):
            x = o + d * t[k]
            S = np.zeros(3)
            if light:
                v = LIGHT_POS - x
                dl = float(np.linalg.norm(v))
                S += math.pi * sigma_s * hg(float(v @ d) / dl) * LIGHT_C * atten(dl) * math.exp(-sigma_t * dl)
            if sun is not None:
                S += sun_ref(x, d, sigma_t, sigma_s, sun)
            if self.B.any():
                Q, _ = np.linalg.qr(rng.normal(size=(3, 3)))
                D = base @ Q.T
                th, f, h = box_exit(np.repeat(x[None], ndir, 0), D)
                B = self.B[self.patch_of(f, h)]
                w = hg(D @ d) * np.exp(-sigma_t * th) * Om
                S += sigma_s * (B * w[:, None]).sum(0)
            L += math.exp(-sigma_t * t[k]) * S * dt
        return L


# ---------------------------------------------------------------- the sun scene (shared description + exact test)
SUN_TO = np.array([0.25, 0.10, 1.0]) / np.linalg.norm([0.25, 0.10, 1.0])
SUN_C = np.array([1.0, 0.95, 0.85])
HOLE = (192.0, 320.0, 192.0, 320.0)   # x0 x1 y0 y1 in the roof (K7: a skylight)
SLOT = (64.0, 448.0, 300.0, 332.0)     # K8: a 32-unit slot, a sheet of sun thinner than a cheap grid's slices


def sun_lit_exact(x, hole):
    s = (BOX[2] - x[..., 2]) / SUN_TO[2]
    hx = x[..., 0] + SUN_TO[0] * s
    hy = x[..., 1] + SUN_TO[1] * s
    return (hx >= hole[0]) & (hx <= hole[1]) & (hy >= hole[2]) & (hy <= hole[3]), s


def sun_ref(x, d, sigma_t, sigma_s, hole):
    lit, s = sun_lit_exact(x, hole)
    if not lit:
        return np.zeros(3)
    return math.pi * sigma_s * hg(float(SUN_TO @ d)) * SUN_C * math.exp(-sigma_t * s)


# ================================================================ ESTIMATE: surfels, probes, relight, air grid, froxels
class Estimate:
    SURF = 70.0

    def __init__(self, albedo, probe_step=(128.0, 128.0, 106.7)):
        cen, nrm, alb = [], [], []
        self.offset = []
        for f in range(6):
            a, s = FACES[f]
            u, v = face_uv(f)
            nu = int(math.ceil(BOX[u] / self.SURF))
            nv = int(math.ceil(BOX[v] / self.SURF))
            self.offset.append((len(cen), nu, nv))
            for iu in range(nu):
                for iv in range(nv):
                    c = np.zeros(3)
                    c[a] = BOX[a] * s
                    c[u], c[v] = (iu + 0.5) * BOX[u] / nu, (iv + 0.5) * BOX[v] / nv
                    cen.append(c)
                    nrm.append(face_normal(f))
                    alb.append(albedo[f])
        self.sp = np.array(cen)
        self.sn = np.array(nrm)
        self.sa = np.array(alb, float)
        g = [np.arange(probe_step[k] / 2, BOX[k], probe_step[k]) for k in range(3)]
        self.pp = np.stack(np.meshgrid(*g, indexing='ij'), -1).reshape(-1, 3)
        dd = np.linalg.norm(self.pp[:, None] - self.pp[None], axis=2)
        np.fill_diagonal(dd, np.inf)
        self.radius = 2.0 * float(np.median(dd.min(1)))
        self.bake()

    def surfel_of(self, face, hit):
        out = np.zeros(len(face), int)
        for f in range(6):
            m = face == f
            if not m.any():
                continue
            u, v = face_uv(f)
            o, nu, nv = self.offset[f]
            iu = np.clip((hit[m, u] / (BOX[u] / nu)).astype(int), 0, nu - 1)
            iv = np.clip((hit[m, v] / (BOX[v] / nv)).astype(int), 0, nv - 1)
            out[m] = o + iu * nv + iv
        return out

    def bake(self, nray=2048):
        D = fibonacci(nray)
        Om = 4.0 * math.pi / nray
        self.links = []
        self.dbar = np.zeros(len(self.pp))
        for i, p in enumerate(self.pp):
            t, f, h = box_exit(np.repeat(p[None], nray, 0), D)
            s = self.surfel_of(f, h)
            self.dbar[i] = t.mean()
            L = []
            for sid in np.unique(s):
                m = s == sid
                md = D[m].sum(0)
                L.append((sid, m.sum() * Om, md / np.linalg.norm(md)))
            self.links.append(L)

    def gather(self, B):
        cube = np.zeros((len(self.pp), 6, 3))
        for i, L in enumerate(self.links):
            for sid, w, dr in L:
                cube[i] += (np.maximum(AXES @ dr, 0.0) * w)[:, None] * B[sid][None, :]
        return cube

    def blend_weights(self, X, radius=None):
        """Rows: the probes each point blends (all visible in a convex box), (1 - d^2/r^2)^2; none in r: the closest."""
        r = self.radius if radius is None else radius
        d = np.linalg.norm(X[:, None] - self.pp[None], axis=2)
        w = np.where(d < r, (1.0 - (d / r) ** 2) ** 2, 0.0)
        empty = w.sum(1) <= 0
        w[empty, np.argmin(d[empty], 1)] = 1.0
        return w / w.sum(1, keepdims=True)

    def relight(self, B_override=None, passes=64, settle=1e-3):
        if B_override is not None:
            self.B = np.full((len(self.sp), 3), float(B_override))
            self.cube = self.gather(self.B)
            self.passes = 0
            return
        v = LIGHT_POS - self.sp
        d = np.linalg.norm(v, axis=1)
        nl = np.maximum(np.einsum('ij,ij->i', self.sn, v / d[:, None]), 0.0)
        B1 = self.sa * (atten(d) * nl)[:, None] * LIGHT_C[None]
        Wf = self.blend_weights(self.sp + 2.0 * self.sn)
        n2 = self.sn ** 2
        B = B1.copy()
        cube = self.gather(B)
        k = 1
        for k in range(2, passes + 1):
            Ep = np.einsum('sp,pac->sac', Wf, cube)
            Ef = np.zeros_like(B)
            for a in range(3):
                pos = self.sn[:, a] >= 0
                Ef += n2[:, a:a + 1] * np.where(pos[:, None], Ep[:, 2 * a], Ep[:, 2 * a + 1])
            Bn = B1 + self.sa * Ef / math.pi
            change = np.abs(Bn - B).max()
            B = Bn
            cube = self.gather(B)
            if change <= settle * B.max():
                break
        self.B, self.cube, self.passes = B, cube, k

    def air_grid(self, voxel=64.0, radius_scale=AIR_RADIUS):
        dims = np.ceil(BOX / voxel).astype(int)
        self.gvox = BOX / dims
        g = [(np.arange(dims[k]) + 0.5) * self.gvox[k] for k in range(3)]
        C = np.stack(np.meshgrid(*g, indexing='ij'), -1).reshape(-1, 3)
        Wv = self.blend_weights(C, self.radius / 2.0 * radius_scale)
        self.gE = np.einsum('vp,pac->vac', Wv, self.cube).reshape(*dims, 6, 3)
        self.gD = (Wv @ self.dbar).reshape(*dims)
        self.gdims = dims

    def sample_air(self, X):
        g = X / self.gvox - 0.5
        g = np.clip(g, 0.0, self.gdims - 1.0 - 1e-9)
        i0 = np.floor(g).astype(int)
        f = g - i0
        i1 = np.minimum(i0 + 1, self.gdims - 1)
        E = np.zeros((len(X), 6, 3))
        Dm = np.zeros(len(X))
        for cx in (0, 1):
            for cy in (0, 1):
                for cz in (0, 1):
                    ix = np.where(cx, i1[:, 0], i0[:, 0])
                    iy = np.where(cy, i1[:, 1], i0[:, 1])
                    iz = np.where(cz, i1[:, 2], i0[:, 2])
                    w = (np.where(cx, f[:, 0], 1 - f[:, 0]) * np.where(cy, f[:, 1], 1 - f[:, 1])
                         * np.where(cz, f[:, 2], 1 - f[:, 2]))
                    E += w[:, None, None] * self.gE[ix, iy, iz]
                    Dm += w * self.gD[ix, iy, iz]
        return E, Dm


def gi_inscatter(E, Dm, d, sigma_t=SIGMA_T, sigma_s=SIGMA_S):
    """The six-axis cube read as L0 + L1 under HG (first-order): display units."""
    fluence = (2.0 / 3.0) * E.sum(1)
    moment = np.stack([E[:, 0] - E[:, 1], E[:, 2] - E[:, 3], E[:, 4] - E[:, 5]], 1)   # (n, axis, c)
    l1 = np.einsum('nac,a->nc', moment, d)
    return sigma_s / (4.0 * math.pi) * np.maximum(fluence + 3.0 * G * l1, 0.0) * np.exp(-sigma_t * Dm)[:, None]


# ---------------------------------------------------------------- the sun's shadow map (light space, 8-unit texels)
class ShadowMap:
    TEXEL = 8.0

    def __init__(self, hole=HOLE):

        w = -SUN_TO
        u = np.cross(w, [0, 0, 1.0])
        u /= np.linalg.norm(u)
        v = np.cross(w, u)
        self.u, self.v, self.w = u, v, w
        corners = np.array([[x, y, z] for x in (0, BOX[0]) for y in (0, BOX[1]) for z in (0, BOX[2])])
        cu, cv = corners @ u, corners @ v
        self.u0, self.v0 = cu.min() - 2 * self.TEXEL, cv.min() - 2 * self.TEXEL
        self.nu = int((cu.max() - self.u0) / self.TEXEL) + 4
        self.nv = int((cv.max() - self.v0) / self.TEXEL) + 4
        uu = self.u0 + (np.arange(self.nu) + 0.5) * self.TEXEL
        vv = self.v0 + (np.arange(self.nv) + 0.5) * self.TEXEL
        U, V = np.meshgrid(uu, vv, indexing='ij')
        P0 = U[..., None] * u + V[..., None] * v      # a point of each texel's ray (w = 0)
        # where each texel's ray meets the roof plane
        s = (BOX[2] - P0[..., 2]) / w[2]
        R = P0 + s[..., None] * w
        inhole = (R[..., 0] >= hole[0]) & (R[..., 0] <= hole[1]) & (R[..., 1] >= hole[2]) & (R[..., 1] <= hole[3])
        depth = np.where(np.isfinite(s), s, -np.inf)      # the roof (or the outside of the box) occludes there
        # through the hole: the first occluder is where the ray leaves the box (floor or a wall)
        hp = R[inhole] + w * 1e-3
        te, _, _ = box_exit(hp, np.repeat(w[None], len(hp), 0))
        depth[inhole] = s[inhole] + te
        self.depth = depth
        # a second channel: where the sun's ray enters the room's air (the roof plane). The fog in the room dims the
        # sun from there to the froxel (a uniform-density volumetric shadow map, Hillaire 2015 reduced to one number)
        self.entry = s

    def lit(self, X, bias=1.0, sigma_t=SIGMA_T):
        """Bilinear PCF of the depth test, and the sun's transmittance through the room's air to X."""
        du = (X @ self.u - self.u0) / self.TEXEL - 0.5
        dv = (X @ self.v - self.v0) / self.TEXEL - 0.5
        dz = X @ self.w
        i0 = np.floor(du).astype(int)
        j0 = np.floor(dv).astype(int)
        fu, fv = du - i0, dv - j0
        out = np.zeros(len(X))
        ent = np.zeros(len(X))
        for a in (0, 1):
            for b in (0, 1):
                ii = np.clip(i0 + a, 0, self.nu - 1)
                jj = np.clip(j0 + b, 0, self.nv - 1)
                wgt = np.where(a, fu, 1 - fu) * np.where(b, fv, 1 - fv)
                out += wgt * (dz <= self.depth[ii, jj] + bias)
                ent += wgt * self.entry[ii, jj]
        return out * np.exp(-sigma_t * np.maximum(dz - ent, 0.0))


# ---------------------------------------------------------------- the froxel integration along one tile's ray
def slice_bounds(n):
    k = np.arange(n + 1)
    z = NEAR * (FAR / NEAR) ** (k / n)
    z[0] = 0.0          # the first slice runs from the eye
    return z


def froxel_ray(d, est=None, light=True, smap=None, red='', slices=SLICES, jitter=None, flat_cube=None, sigma_t=SIGMA_T):
    """Fog radiance along ray d as the shader's integrated volume would return it at the wall's depth."""
    cosa = float(d @ CAM_FWD)
    tw = box_exit(CAM_POS[None], d[None])[0][0]
    zb = slice_bounds(slices)
    zw = tw * cosa
    L = np.zeros(3)
    T = 1.0
    Lb, zs = [np.zeros(3)], [zb[0]]
    if zw <= zb[0]:
        return L
    k = 0
    while zb[k] < zw and k < slices:
        z0, z1 = zb[k], zb[k + 1]
        zc = z0 + (z1 - z0) * (0.5 if jitter is None else jitter)
        x = CAM_POS + d * (zc / cosa)
        x = np.clip(x, 1e-3, BOX - 1e-3)
        S = np.zeros(3)
        if light:
            v = LIGHT_POS - x
            dl = float(np.linalg.norm(v))
            S += math.pi * SIGMA_S * hg(float(v @ d) / dl) * LIGHT_C * atten(dl) * math.exp(-sigma_t * dl)
        if smap is not None:
            if red == 'noshadow':
                vis = 1.0
            elif isinstance(smap, tuple):      # an exact sun test (K8 isolates the depth sampling from the map's texels)
                lit, sd = sun_lit_exact(x, smap)
                vis = float(lit) * math.exp(-sigma_t * float(sd))
            else:
                vis = float(smap.lit(x[None])[0])
            S += math.pi * SIGMA_S * hg(float(SUN_TO @ d)) * SUN_C * vis
        if est is not None and red != 'gioff':
            if flat_cube is not None:
                E, Dm = flat_cube
            else:
                E, Dm = est.sample_air(x[None])
            S += gi_inscatter(E, Dm, d, sigma_t=sigma_t)[0]
        D = (z1 - z0) / cosa
        a = math.exp(-sigma_t * D)
        L = L + T * S * (-math.expm1(-sigma_t * D)) / sigma_t     # (S - S exp(-sigma_t D)) / sigma_t
        T *= a
        Lb.append(L.copy())
        zs.append(z1)
        k += 1
    # the integrated volume read at the pixel's depth: linear between the slice bounds around it
    zs = np.array(zs)
    Lb = np.array(Lb)
    return np.array([np.interp(zw, zs, Lb[:, c]) for c in range(3)])


# ================================================================ the checks
def chroma(L):
    return L / max(L.sum(), 1e-30)


class Run:
    def __init__(self):
        self.res = {}
        self.lines = []

    def put(self, key, ok, text):
        self.res[key] = ok
        self.lines.append('%s %s %s' % (key, 'PASS' if ok else 'FAIL', text))
        print(self.lines[-1], flush=True)


def scene(name, cache={}):
    if name not in cache:
        ref = Reference(SCENES[name])
        ref.solve()
        est = Estimate(SCENES[name])
        est.relight()
        est.air_grid()
        cache[name] = (ref, est)
    return cache[name]


def ref_rays(ref, rays, seed, cache={}, **kw):
    key = (id(ref), seed)
    if key not in cache:
        rng = np.random.default_rng(seed)
        cache[key] = np.array([ref.fog_ray(CAM_POS, d, rng, **kw) for d in rays])
    return cache[key]


def main(red):
    t0 = time.time()
    run = Run()
    rays = camera_rays(TILES)
    print('VOLFOG1 twin: box %s, fog sigma_t %.3g /u (0.02 /m), albedo 0.9, g %.2f; froxels %dx%dx%d; red=%s'
          % (BOX.astype(int).tolist(), SIGMA_T, G, W // TILE, H // TILE, SLICES, red or 'none'), flush=True)

    # K0 + K1: known answers -------------------------------------------------------------------------------
    ref_r, est_r = scene('red')
    rows = ref_r.F.sum(1)
    run.put('K0', bool(np.abs(rows - 1).max() <= 1e-6),
            'reference form factors: %d patches, worst |1 - row sum| %.2e (bar 1e-6)' % (ref_r.n, np.abs(rows - 1).max()))

    fur_ref = Reference(SCENES['grey'])
    fur_ref.solve(B_override=1.0)
    fur_est = Estimate(SCENES['grey'])
    fur_est.relight(B_override=1.0)
    fur_est.air_grid()
    tiny = 1e-9
    worst_r = worst_e = 0.0
    for d in rays[:3]:
        tw = box_exit(CAM_POS[None], d[None])[0][0]
        want = SIGMA_S * tw
        rng = np.random.default_rng(1)
        Lr = fur_ref.fog_ray(CAM_POS, d, rng, ns=24, ndir=4096, light=False, sigma_t=tiny)
        Le = froxel_ray(d, est=fur_est, light=False, sigma_t=tiny)
        worst_r = max(worst_r, float(np.abs(Lr / want - 1).max()))
        worst_e = max(worst_e, float(np.abs(Le / want - 1).max()))
    run.put('K1', worst_r <= 0.01 and worst_e <= 0.01,
            'furnace (walls glow 1, no lamp): fog / (sigma_s x length) off by %.4f (reference), %.4f (estimate); bar 0.01'
            % (worst_r, worst_e))

    # K2: probe cubes ----------------------------------------------------------------------------------------
    Eref = ref_r.irradiance_axes(est_r.pp)
    rel = np.abs(est_r.cube - Eref).sum(2) / np.maximum(Eref.sum(2), 1e-9)
    run.put('K2', float(np.median(rel)) <= 0.05 and float(rel.max()) <= 0.12,
            'probe cubes (red room, %d probes, %d relight passes): median %.4f, worst %.4f (bars 0.05 / 0.12)'
            % (len(est_r.pp), est_r.passes, float(np.median(rel)), float(rel.max())))

    # K3 + K4: the red room ------------------------------------------------------------------------------------
    Lref = ref_rays(ref_r, rays, 11)
    Lest = np.array([froxel_ray(d, est=est_r, red=red) for d in rays])
    Ldir = np.array([froxel_ray(d, est=None) for d in rays])
    tot = np.abs(Lest.sum(1) - Lref.sum(1)) / Lref.sum(1)
    rr = np.array([chroma(x)[0] for x in Lref])
    re = np.array([chroma(x)[0] for x in Lest])
    gi_share = 1.0 - Ldir[:, 0] / Lref[:, 0]
    for i in range(len(rays)):
        print('   ray %d tile %s: ref %.4e r %.3f | est %.4e r %.3f | direct-only r %.3f | GI share of red %.2f'
              % (i, TILES[i], Lref[i].sum(), rr[i], Lest[i].sum(), re[i], chroma(Ldir[i])[0], gi_share[i]))
    run.put('K3', bool(tot.max() <= 0.10),
            'red room fog radiance per ray: worst rel %.4f, median %.4f (bar 0.10)' % (tot.max(), np.median(tot)))
    run.put('K4', bool(np.abs(re - rr).max() <= 0.02 and (rr - 1 / 3).min() >= 0.05),
            'red room tint r=R/(R+G+B): ref %.3f..%.3f, worst |est - ref| %.4f (bar 0.02); ref tint floor %.3f (bar >= 0.05)'
            % (rr.min(), rr.max(), np.abs(re - rr).max(), (rr - 1 / 3).min()))

    # K5: grey room ------------------------------------------------------------------------------------------
    ref_g, est_g = scene('grey')
    sel = rays[:3]
    Lrg = ref_rays(ref_g, sel, 12)
    Leg = np.array([froxel_ray(d, est=est_g, red=red) for d in sel])
    dev = max(np.abs(np.array([chroma(x)[0] for x in Lrg]) - 1 / 3).max(),
              np.abs(np.array([chroma(x)[0] for x in Leg]) - 1 / 3).max())
    run.put('K5', bool(dev <= 0.005), 'grey room: worst |r - 1/3| %.5f over both paths (bar 0.005)' % dev)

    # K6: the tint follows the walls (red +X, green -X) -----------------------------------------------------------
    ref_s, est_s = scene('split')
    Lrs = ref_rays(ref_s, rays, 13)
    flat = None
    if red == 'flat':
        allE, allD = est_s.gE.reshape(-1, 6, 3).mean(0, keepdims=True), np.array([est_s.gD.mean()])
        flat = (allE, allD)
    Les = np.array([froxel_ray(d, est=est_s, red=red, flat_cube=flat) for d in rays])
    rgr = np.array([chroma(x)[0] - chroma(x)[1] for x in Lrs])
    rge = np.array([chroma(x)[0] - chroma(x)[1] for x in Les])
    for i in range(len(rays)):
        print('   split ray %d tile %s: ref r-g %+.4f | est r-g %+.4f' % (i, TILES[i], rgr[i], rge[i]))
    run.put('K6', bool(np.abs(rge - rgr).max() <= 0.03 and rgr.max() - rgr.min() >= 0.06),
            'red/green room: worst |d(r-g)| %.4f (bar 0.03); reference spread %.4f (bar >= 0.06)'
            % (np.abs(rge - rgr).max(), rgr.max() - rgr.min()))

    # K7: shafts through a roof hole --------------------------------------------------------------------------
    dark = Reference([(0.0, 0.0, 0.0)] * 6)
    dark.B = np.zeros((dark.n, 3))

    def shaft(hole, tiles, slices, frames, jitter, exact=False):
        """Reference per ray, and the displayed fog of the last 16 frames (history HISTORY, 16-frame jitter cycle)."""
        smap = hole if exact else ShadowMap(hole)
        rr = camera_rays(tiles)
        ref = np.array([dark.fog_ray(CAM_POS, d, np.random.default_rng(5), ns=2048, light=False, sun=hole)
                        for d in rr])
        cyc = []
        for k in range(16):
            j = None
            if jitter:
                j, b, q = 0.0, 0.5, k + 1          # van der Corput, base 2
                while q:
                    j += b * (q & 1)
                    q >>= 1
                    b *= 0.5
            cyc.append(np.array([froxel_ray(d, light=False, smap=smap, red=red, slices=slices, jitter=j) for d in rr]))
        hist = cyc[0]
        shown = []
        for fr in range(1, frames):
            hist = HISTORY * hist + (1.0 - HISTORY) * cyc[fr % 16]
            shown.append(hist)
        return ref, cyc[0], np.array(shown[-16:])

    shaft_tiles = [(80, 45), (80, 20), (100, 30), (10, 45), (150, 60), (60, 10), (120, 45)]
    Lsr, _, shown = shaft(HOLE, shaft_tiles, SLICES, 128, True)
    Lse = shown[-1]
    mx = Lsr.sum(1).max()
    err = np.abs(shown.sum(2) - Lsr.sum(1)[None]).max(0) / mx
    miss = Lsr.sum(1).min() / mx
    for i in range(len(shaft_tiles)):
        print('   shaft ray %d tile %s: ref %.4e | est %.4e | worst err/max over 16 frames %.4f'
              % (i, shaft_tiles[i], Lsr[i].sum(), Lse[i].sum(), err[i]))
    run.put('K7', bool(err.max() <= 0.10 and miss < 0.01),
            'sun shafts (64 slices, 8 u shadow texels, jitter + history %.2f): worst |est - ref| / max %.4f (bar 0.10); '
            'darkest ref ray %.4f of max (bar < 0.01)' % (HISTORY, err.max(), miss))

    # K8: temporal jitter + history against depth aliasing: a thin sheet of sun, a cheap 24-slice grid --------------------
    slot_tiles = [(80, 40), (80, 36), (80, 32), (80, 28), (70, 34), (90, 30), (60, 38), (100, 26)]
    Lr8, one, shown8 = shaft(SLOT, slot_tiles, 24, 128, red != 'nojitter', exact=True)
    if red != 'nojitter':
        _, one, _ = shaft(SLOT, slot_tiles, 24, 2, False, exact=True)
    mx8 = Lr8.sum(1).max()
    e1 = float((np.abs(one.sum(1) - Lr8.sum(1)) / mx8).max())
    ej = float((np.abs(shown8.sum(2) - Lr8.sum(1)[None]) / mx8).max())
    em = float((np.abs(shown8.sum(2).mean(0) - Lr8.sum(1)) / mx8).max())
    for i in range(len(slot_tiles)):
        print('   slot ray %d tile %s: ref %.4e | one fixed sample %.4e | shown %.4e..%.4e'
              % (i, slot_tiles[i], Lr8[i].sum(), one[i].sum(), shown8[:, i].sum(1).min(), shown8[:, i].sum(1).max()))
    run.put('K8', bool(e1 >= 0.5 and ej <= 0.25),
            'temporal (32-unit sheet, 24 slices, exact sun test): one fixed sample off by %.4f (floor >= 0.5: the sheet '
            'aliases); %s + history %.2f, worst shown frame %.4f (bar 0.25), 16-frame mean %.4f'
            % (e1, 'no jitter' if red == 'nojitter' else 'jitter', HISTORY, ej, em))

    ok = all(run.res.values())
    print('VOLFOG1 %s (%d checks, %d failed; red=%s; %.1f s)'
          % ('PASS' if ok else 'FAIL', len(run.res), sum(not v for v in run.res.values()), red or 'none', time.time() - t0),
          flush=True)
    return run


REDS = {'gioff': ['K3', 'K4'], 'flat': ['K6'], 'noshadow': ['K7'], 'nojitter': ['K8']}


if __name__ == '__main__':
    red = os.environ.get('VOLFOG1_RED', '')
    if '--red' in sys.argv:
        red = sys.argv[sys.argv.index('--red') + 1]
    if red:
        if red not in REDS:
            print('unknown red %s (known: %s)' % (red, ', '.join(REDS)))
            sys.exit(2)
        r = main(red)
        sys.exit(0 if all(r.res.values()) else 1)
    g = main('')
    good = all(g.res.values())
    for name, want in REDS.items():
        print('--- red %s (must FAIL %s)' % (name, '/'.join(want)), flush=True)
        r = main(name)
        failed = [k for k, v in r.res.items() if not v]
        hit = any(k in failed for k in want)
        print('red %s: %s (failed %s)' % (name, 'FAILS AS IT MUST' if hit else 'DID NOT FAIL -- the check is blind',
                                         ', '.join(failed) or 'nothing'), flush=True)
        good = good and hit
    print('VOLFOG1 SELF-TEST %s' % ('PASS' if good else 'FAIL'))
    sys.exit(0 if good else 1)
