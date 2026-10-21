#!/usr/bin/env python3
"""Judge tests/spells/volair1_cell.sh (lane VOLAIR1, 2026-10-13): the Volumetric Air against an independent rebuild.

  python tests/spells/volair1_cell_check.py <out dir> <place...>      (VOLAIR1_RED=<red> when judging a red)

The rebuild shares nothing with the exe but the dumped inputs (the fog constants fogK, the view, the projection, the
directional light, the god-ray record, the scene's top): the medium's fit, the slices, the rays, the optical depths
(here by Gauss-Legendre quadrature of the density, split at its kinks; the exe uses a closed form), the phase
(Rayleigh + two Henyey-Greenstein lobes over the record's summed luminance), the transmittance to the top, the
four-sub-step inject, the front-to-back integrate and the apply are re-done in numpy (float64).

Stages (bars fixed before the first run):
  M  medium    sigma0, zb, 1/H re-fitted from fogK (least squares in optical depth against the game's fog over
               500..5000 units below the band, never thinner than 1 - exp(-sigma0 dFull) = max; the height band
               K5's pair-0 edges, the density at its top = the records' top weight) equal the dumped airMed within
               1e-4 relative; the top = the scene's top in the fog's frame; the far slice = where the air reaches max
  H  haze      the dumped density's 1 - exp(-sigma0 d) within 0.10 of the game's alpha at 2000, 3000, 5000 units
  X  present   every shot the place needs exists and is non-empty (a run that drew nothing FAILS)
  D  sun+sky  (exteriors) the air is lit by the surfaces' sun and sky: the dump's directional colour equals the
             surfaces' 'cell sun:' line within 1e-4, the sky DALC on (red oldsun: the fog-sun glare colour, no DALC)
  R  rebuild   the injected volume: alpha (the slice's transmittance) within 1e-4 on >= 99% of the voxels; at terms
               1 without the cascades also rgb within 1% (+ 1e-5 x max) on >= 99% of the lit voxels, total within 1%
               (terms 2 / 4: alpha alone -- the placed lights and the GI are judged by L G F N and by the twins)
  K  colour    the air's colour is the records' (owner's ruling 10-14): at terms 1 (the sky's share alone) the injected
               light's chromaticity (r, g, b over their sum) per voxel from 2000 units to the far edge against the game's
               fog colour there (near / far / high by hour, mixed toward the fog-sun colour as the game mixes it):
               95th percentile of the largest |difference| <= 0.01. Also: python volair1_cell_check.py --colour
               <dump.bin>... reports it on any dump (all terms: the placed lights keep their own colours there)
  I  integrate the integrated volume = L_k = L_(k-1) + T_(k-1) inj_k, T_k = prod inj.a: rgb as R, alpha 1e-5
  P  apply     the L probe inside the range the dumped volume gives over the fragment's distance +/- 32 units, +/- 2/255,
               on >= 95% of the fogged fragments; the T probe likewise; AND applied: vol.png differs from off.png
               (the game's fog) by >= 2/255 on >= 1% and >= 5000 of the fogged fragments
  W  far tail  (places shot with geod/geoz, the far LOD on) the transmittance probe past the froxels' far edge against
               the last slice's T x exp(-tau) of the closed-form tail (>= 95% within 3/255, the predicted drop must be
               >= 3/255 to judge), and in the overlap (0.5 to 0.98 x far) the froxels' T against the closed form from
               the eye (>= 95% within 4/255): the seam is continuous because both sides are the same medium
  U  sun T     (exteriors) FOGSUN1: probesun (WW_CELL_AIR_PROBE=3, the sun's transmittance through the air to each
               surface) against exp(-sigma0 column(z, top) / mu) from the dump's medium and the geometry probe's z:
               >= 99% of the fogged pixels within the z step +/- 2.5/255, and the predicted dimming must be visible
               (median >= 0.05) for the stage to have teeth
  O  off       off.png (this exe, the row off) == before.png (the exe from before the lane) byte for byte, or within
               the before exe's own flake (rung2); its red: vol.png judged by the same rule must differ
  S  shadows   (sanctuary) the cascades on: the air's light (last slice) drops by >= 2% against the noshadow
               comparator on >= 1% of the columns, never rises by more than 0.1%
  L  lamps     (interiors, terms 2) the placed lights put light in the air: summed L >= 1e-3 x the columns and
               >= 1% of the columns carry >= 1e-3
  G  GI        (interiors) the GI alone (terms 4) puts light in the air: summed >= 1e-3 x the columns
  F  follows   (interiors) the GI's per-column chromaticity spread (std of (r - g) / sum) >= 1.5 x the red flat's
  N  neon      (goodneighbor) the bake WITH the neon emitters against the bake WITHOUT: the air's summed light
               >= 1.02 x, and >= 1% of the columns gain >= 5%
  C  cost      time.bin.txt's ms at 1920 x 1080 (glFinish to glFinish around the pass, all terms): reported
Reds: off -> P; wrongmed -> M; farfit -> H; physcol -> K; noshadow -> S; nolights -> L; gioff -> G (and N when run); flat -> F.
"""
import os
import sys

import numpy as np

try:
    from PIL import Image
except ImportError:
    Image = None

PI = np.pi
LUM = np.array([0.2126, 0.7152, 0.0722])
GLX, GLW = np.polynomial.legendre.leggauss(12)


def read_txt(p):
    d = {}
    for line in open(p, encoding='utf-8'):
        k, _, v = line.strip().partition(' ')
        if not k:
            continue
        try:
            d[k] = np.array([float(x) for x in v.split()])
        except ValueError:
            d[k] = v
    return d


def read_vol(p):
    b = np.fromfile(p, dtype=np.uint8)
    c, r, s = np.frombuffer(b[:12].tobytes(), dtype=np.int32)
    v = np.frombuffer(b[12:].tobytes(), dtype=np.float32).reshape(2, s, r, c, 4)
    return v[0].astype(np.float64), v[1].astype(np.float64)


def game_alpha(K, d, z):
    """the game's fog alpha (lookdev_fog.glsl wwFogEval, re-typed)"""
    ramp = d * K[0][0] - K[0][2]
    f = min(max(ramp, 0.0), 1.0)
    p0 = min(max(z * K[5][0] - K[5][2], 0.0), 1.0)
    p1 = min(max(z * K[5][1] - K[5][3], 0.0), 1.0)
    hb = p0 + (p1 - p0) * f
    mx = K[2][3]
    clampT = min((f - 0.75) * 4.0 * (1.0 - mx) + mx, 1.0) if ramp > 0.75 else mx
    escape = f * 66.666672 if ramp < 0.015 else 1.0
    I = min(clampT, f ** K[1][3]) if f > 0 else 0.0
    return (hb * K[3][3] + (1.0 - hb)) * I * escape


def fit(t):
    """the medium from the fog records, re-derived: (sigma0, zb, invH, dFull, farD); sigma0 by least squares in
    optical depth against the game's fog over 500..5000 units below the band, never thinner than the far-end fit"""
    K = [t['fogK%d' % k] for k in range(6)]
    dfull = (1.0 + K[0][2]) / K[0][0]
    mx = min(max(K[2][3], 0.0), 0.99)
    taumax = -np.log(1.0 - mx) if mx > 0 else 0.0
    sfar = taumax / dfull if dfull > 0 else 0.0
    zb, invh, band = 0.0, 0.0, False
    if K[5][1] > 1e-9 and 0.0 < K[3][3] < 1.0:
        z0, z1 = K[5][3] / K[5][1], (1.0 + K[5][3]) / K[5][1]
        zb = z0
        invh = -np.log(max(K[3][3], 1e-3)) / max(z1 - z0, 1e-3)
        band = True
    zlow = zb - 10.0 if band else 0.0
    ds = 500.0 * 10.0 ** (np.arange(64) / 63.0)
    knee = (0.75 + K[0][2]) / K[0][0]    # past it the game's own end ramp to 1, not the air
    ds = np.concatenate([ds[:1], ds[1:][ds[1:] <= knee]])
    tau = np.array([-np.log(1.0 - min(max(game_alpha(K, d, zlow), 0.0), max(mx, 1e-4))) for d in ds])
    ss = float((ds * tau).sum() / (ds * ds).sum())
    sigma0 = ss if knee <= 5000.0 else max(sfar, ss)    # curve ends inside the street range: street fit alone
    fard = min(dfull, taumax / sigma0) if sigma0 > sfar and sigma0 > 0 else dfull
    return sigma0, zb, invh, dfull, fard


def game_colour(K, d, z):
    """the game's fog colour before the sun term (wwFogEval's fogCol), arrays d, z -> (..., 3)"""
    ramp = d * K[0][0] - K[0][2]
    f = np.clip(ramp, 0.0, 1.0)
    p0 = np.clip(z * K[5][0] - K[5][2], 0.0, 1.0)
    p1 = np.clip(z * K[5][1] - K[5][3], 0.0, 1.0)
    hb = (p0 + (p1 - p0) * f)[..., None]
    mx = K[2][3]
    clampT = np.where(ramp > 0.75, np.minimum((f - 0.75) * 4.0 * (1.0 - mx) + mx, 1.0), mx)
    I = np.where(f > 0, np.minimum(clampT, np.power(np.maximum(f, 1e-30), K[1][3])), 0.0)[..., None]
    lo = K[1][:3] + (K[3][:3] - K[1][:3]) * I
    hi = K[2][:3] + (K[4][:3] - K[2][:3]) * I
    return lo + (hi - lo) * hb


def env_colour(t, d, z, dv):
    """the colour the sky's share of the air takes (cell_volair.frag airEnvColour): the game's fog colour mixed
    toward the fog-sun colour as the game mixes it, at unit luminance (None where it has none)"""
    K = [t['fogK%d' % k] for k in range(6)]
    c = game_colour(K, d, z)
    sun, sc = t['fogSun'], t['fogSunColour']
    w = (np.maximum(dv @ sun[:3], 0.0) ** sc[3]) * sun[3] if sun[3] > 0 else np.zeros(dv.shape[:-1])
    c = np.maximum(c + (sc[:3] - c) * w[..., None], 0.0)
    lc = c @ LUM
    return c / np.where(lc > 1e-8, lc, 1.0)[..., None], lc > 1e-8


# the layered law (lane VOLAIR1 ground fog): rho = (1-w) B(zb, Hb) + w B(zn, Hn) + s B(zg, Hg), B = min(1, exp(-(z-z0)/H)),
# empty above the top. (w, zn, 1/Hn, s, zg, 1/Hg) come from the dump's airLayer / airGround lines (set_law per dump);
# a dump without them (an older exe) is the far band alone.
LAW = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)


def set_law(t):
    global LAW
    a, g = t.get('airLayer'), t.get('airGround')
    if isinstance(a, np.ndarray) and a.size >= 4 and isinstance(g, np.ndarray) and g.size >= 2:
        LAW = (float(a[0]), float(a[1]), float(a[2]), float(a[3]), float(g[0]), float(g[1]))
    else:
        LAW = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    return LAW


def band(z, z0, invh):
    return np.minimum(1.0, np.exp(-(z - z0) * invh)) if invh > 0 else np.ones_like(np.asarray(z, float))


def rho(z, zb, invh, top, ground=True):
    """the density over sigma0; ground=False: the record-derived part alone (no .recx ground term)"""
    w, zn, ihn, sg, zg, ihg = LAW
    r = (1.0 - w) * band(z, zb, invh)
    if w > 0:
        r = r + w * band(z, zn, ihn)
    if ground and sg > 0:
        r = r + sg * band(z, zg, ihg)
    return np.where(z > top, 0.0, r)


def col_int(za, zc, zb, invh, top):
    """the integral of rho dz from za to zc (arrays), by Gauss-Legendre split at the law's kinks (zb, zn, zg, the top)"""
    lo, hi = np.minimum(za, zc), np.maximum(za, zc)
    sgn = np.where(zc >= za, 1.0, -1.0)
    w, zn, _, sg, zg, _ = LAW
    cuts = sorted([zb, top] + ([zn] if w > 0 else []) + ([zg] if sg > 0 else []))
    edges = [lo]
    for c in cuts:
        edges.append(np.clip(c, lo, hi))
    edges.append(hi)
    tot = np.zeros_like(lo)
    for a, b in zip(edges[:-1], edges[1:]):
        h = 0.5 * (b - a)
        m = 0.5 * (a + b)
        for x, w in zip(GLX, GLW):
            tot += w * h * rho(m + h * x, zb, invh, top)
    return sgn * tot


def hg(c, g):
    d = 1.0 + g * g - 2.0 * g * c
    return (1.0 - g * g) / (4.0 * PI * d * np.sqrt(d))


NEUTRAL = True	# set per dump in rebuild(): the red bluescatter (524288) keeps the record's coloured weights


def phase_pi(med, c):
    """pi x the phase, rgb: the record's three scattering colours over the luminance of their sum; 10-15 ruling
    (bungo: "our fog is what vanilla has as the color", commit 92f19a6e): each lobe is its own luminance on all three
    channels, so the air adds no hue (NEUTRAL False under the red bluescatter: the coloured weights)"""
    air, fwd, back = med[0:3], med[3:6], med[6:9]
    lum = float(LUM @ (air + fwd + back))
    inv = 1.0 / lum if lum > 1e-6 else 0.0
    if NEUTRAL:
        air, fwd, back = (np.full(3, float(LUM @ w)) for w in (air, fwd, back))
    c = c[..., None]
    return PI * ((air * inv) * (3.0 / (16.0 * PI)) * (1.0 + c * c) + (fwd * inv) * hg(c, med[9]) + (back * inv) * hg(c, med[10]))


def rebuild(t, with_rgb):
    cols, rows, N = (int(x) for x in t['dims'])
    near, logR = t['slices']
    p00, p11 = t['proj']
    fv, ds = t['fogView'], float(t['fogDistScale'][0])
    sigma0, zb, invh, _, _ = fit(t)
    top = float(t['airMed'][3])
    med = t['medium']
    recol = (int(t['red'][0]) & 256) == 0    # the red physcol keeps the physical colour
    global NEUTRAL
    NEUTRAL = (int(t['red'][0]) & 524288) == 0
    jj, ii = np.meshgrid(np.arange(rows), np.arange(cols), indexing='ij')
    ndc = np.stack([(ii + 0.5) / cols * 2 - 1, (jj + 0.5) / rows * 2 - 1], -1)
    dv = np.stack([ndc[..., 0] / p00, ndc[..., 1] / p11, -np.ones_like(ndc[..., 0])], -1)
    dv /= np.linalg.norm(dv, axis=-1, keepdims=True)
    zc = fv[3]
    dzdt = (dv @ fv[:3]) / ds
    # the directional light's colour x phase (constant along a ray) and the slope toward it
    S = np.zeros((rows, cols, 3))
    mu = 1.0
    if int(t['cellOn'][0]) and int(t['cellHasDir'][0]):
        Rw = np.stack([t['cellRow%d' % k][:3] for k in range(3)])
        dw = dv @ Rw.T
        dw /= np.linalg.norm(dw, axis=-1, keepdims=True)
        to = t['cellDirTo'] / np.linalg.norm(t['cellDirTo'])
        S = t['cellDirColor'] * phase_pi(med, dw @ to)
        mu = to[2]
    elif t['fogSun'][3] > 0:
        sun = t['fogSun']
        S = t['fogSunColour'][:3] * sun[3] * phase_pi(med, dv @ sun[:3])
        mu = float(fv[:3] @ sun[:3]) / np.linalg.norm(fv[:3])
    mu = max(mu, 0.02)

    def tup(z):
        return np.where(z >= top, 1.0, np.exp(-sigma0 * col_int(z, np.full_like(z, top), zb, invh, top) / mu))

    def od(ta, tb):
        za, zz = zc + dzdt * ta, zc + dzdt * tb
        dz = zz - za
        flat = np.abs(dz) < 1e-6
        ci = col_int(za, zz, zb, invh, top)
        mid = rho(0.5 * (za + zz), zb, invh, top)
        return sigma0 * (tb - ta) * np.where(flat, mid, ci / np.where(flat, 1.0, dz))

    inj = np.zeros((N, rows, cols, 4))
    for k in range(N):
        t0 = near * np.exp(logR * k / N)
        t1 = near * np.exp(logR * (k + 1) / N)
        T = np.ones((rows, cols))
        acc = np.zeros((rows, cols, 3))
        for q in range(4):
            ta, tb = t0 + (t1 - t0) * q * 0.25, t0 + (t1 - t0) * (q + 1) * 0.25
            tau = od(ta, tb)
            if with_rgb:
                tm = 0.5 * (ta + tb)
                add = (T * -np.expm1(-tau) * tup(zc + dzdt * tm))[..., None] * S
                if not recol:
                    acc += add
                else:
                    c, ok = env_colour(t, np.full((rows, cols), tm), zc + dzdt * tm, dv)
                    acc += np.where(ok[..., None], (add @ LUM)[..., None] * c, add)
            T = T * np.exp(-tau)
        inj[k, ..., :3] = acc
        inj[k, ..., 3] = T
    return inj


def colour_check(t, inj, dmin=2000.0):
    """K: per lit voxel from dmin to the far edge, the largest |chromaticity difference| between the injected light
    and the records' colour at the slice's middle; returns (p50, p95, voxels, record chroma at the far edge, centre)"""
    cols, rows, N = (int(x) for x in t['dims'])
    near, logR = t['slices']
    p00, p11 = t['proj']
    fv = t['fogView']
    jj, ii = np.meshgrid(np.arange(rows), np.arange(cols), indexing='ij')
    dv = np.stack([((ii + 0.5) / cols * 2 - 1) / p00, ((jj + 0.5) / rows * 2 - 1) / p11, -np.ones((rows, cols))], -1)
    dv /= np.linalg.norm(dv, axis=-1, keepdims=True)
    dzdt = (dv @ fv[:3]) / float(t['fogDistScale'][0])
    mx = np.abs(inj[..., :3]).max()
    out = []
    for k in range(N):
        t0, t1 = near * np.exp(logR * k / N), near * np.exp(logR * (k + 1) / N)
        tm = 0.5 * (t0 + t1)
        if tm < dmin:
            continue
        c, ok = env_colour(t, np.full((rows, cols), tm), fv[3] + dzdt * tm, dv)
        g = inj[k, ..., :3]
        sg = g.sum(-1)
        lit = ok & (sg > 1e-4 * max(mx, 1e-12)) & (g.min(-1) >= 0)
        if not lit.any():
            continue
        ch = g[lit] / sg[lit][..., None]
        cr = c[lit] / c[lit].sum(-1)[..., None]
        out.append(np.abs(ch - cr).max(-1))
    if not out:
        return None
    e = np.concatenate(out)
    cf, _ = env_colour(t, np.array([near * np.exp(logR)]), np.array([fv[3]]), np.array([[0.0, 0.0, -1.0]]))
    return float(np.percentile(e, 50)), float(np.percentile(e, 95)), e.size, cf[0] / cf[0].sum()


def integrate(inj):
    out = np.zeros_like(inj)
    L = np.zeros(inj.shape[1:3] + (3,))
    T = np.ones(inj.shape[1:3])
    for k in range(inj.shape[0]):
        L = L + T[..., None] * inj[k, ..., :3]
        T = T * inj[k, ..., 3]
        out[k, ..., :3] = L
        out[k, ..., 3] = T
    return out


def agree(got, want, rel=0.01):
    mx = np.abs(want).max()
    if mx <= 0:
        return 0.0, 0.0, mx
    lit = np.abs(want) > 1e-3 * mx
    ok = np.abs(got - want) <= rel * np.abs(want) + 1e-5 * mx
    frac = ok[lit].mean() if lit.any() else 0.0
    tot = got.sum() / want.sum() if want.sum() != 0 else 0.0
    return frac, tot, mx


def png(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(np.int32)


def trilinear(vol, u, v, f):
    """GL_LINEAR on a (slices, rows, cols, 4) volume, texel centres at +0.5, clamped to edge; f in texels"""
    N, R, C = vol.shape[:3]
    x = np.clip(u * C - 0.5, 0, C - 1)
    y = np.clip(v * R - 0.5, 0, R - 1)
    z = np.clip(f, 0, N - 1)
    x0, y0, z0 = np.floor(x).astype(int), np.floor(y).astype(int), np.floor(z).astype(int)
    x1, y1, z1 = np.minimum(x0 + 1, C - 1), np.minimum(y0 + 1, R - 1), np.minimum(z0 + 1, N - 1)
    fx, fy, fz = (x - x0)[..., None], (y - y0)[..., None], (z - z0)[..., None]
    out = 0
    for zi, wz in ((z0, 1 - fz), (z1, fz)):
        for yi, wy in ((y0, 1 - fy), (y1, fy)):
            for xi, wx in ((x0, 1 - fx), (x1, fx)):
                out = out + vol[zi, yi, xi, :] * wz * wy * wx
    return out


def vol_at(integ, t, d, u, v):
    """the air at distance d: (L rgb, T)"""
    near, logR = t['slices']
    N = integ.shape[0]
    f = np.log(np.maximum(d, 1e-6) / near) / logR * N
    lo = trilinear(integ, u, v, np.zeros_like(d))
    full = trilinear(integ, u, v, np.minimum(f, N) - 1.0)
    fe = f[..., None]
    first = np.concatenate([lo[..., :3] * fe, 1.0 - (1.0 - lo[..., 3:]) * fe], -1)
    out = np.where((f < 1.0)[..., None], first, full)
    out = np.where((d <= near)[..., None], np.array([0.0, 0.0, 0.0, 1.0]), out)
    out[..., :3] = np.maximum(out[..., :3], 0.0)
    out[..., 3] = np.clip(out[..., 3], 0.0, 1.0)
    return out


class Gate:
    def __init__(self):
        self.res = {}

    def put(self, key, ok, text):
        self.res.setdefault(key, []).append(bool(ok))
        print('  %s  %s  %s' % ('PASS' if ok else 'FAIL', key, text))


def last_light(p):
    return read_vol(p)[1][-1, ..., :3]


def main():
    if sys.argv[1:2] == ['--colour']:
        for b in sys.argv[2:]:
            t = read_txt(b + '.txt')
            inj, _ = read_vol(b)
            kc = colour_check(t, inj)
            print('%s: %s' % (os.path.basename(b), 'no lit voxel past 2000' if kc is None else
                  'chromaticity diff median %.4f, 95th %.4f over %d voxels; record far colour (centre) %.3f %.3f %.3f; terms %d red %d'
                  % (kc[0], kc[1], kc[2], kc[3][0], kc[3][1], kc[3][2], int(t['terms'][0]), int(t['red'][0]))))
        return 0
    out = sys.argv[1]
    places = sys.argv[2:]
    red = os.environ.get('VOLAIR1_RED', '')
    G = Gate()
    for p in places:
        run = os.path.join(out, p)
        has = lambda *fs: all(os.path.exists(os.path.join(run, f)) for f in fs)
        print('== %s' % p)
        # X: the shots this place must have left; a run that drew nothing must not pass by having nothing to check
        need = []
        if p == 'goodneighbor':
            if red in ('', 'gioff'):
                need = ['neon_veil.png', 'neon_veil.bin', 'neon_veil.bin.txt', 'neon_off.png', 'neon_off.bin', 'neon_off.bin.txt']
        elif p == 'sanctfar':
            need = ['vol.bin', 'vol.bin.txt', 'probet.png'] + ([] if red else ['geod.png', 'geoz.png'])
        elif not red:
            need = ['vol.bin', 'vol.bin.txt', 'time.bin', 'time.bin.txt']
        if need:
            miss = [f for f in need if not (os.path.exists(os.path.join(run, f)) and os.path.getsize(os.path.join(run, f)) > 0)]
            G.put('X', not miss, 'the shots present and non-empty: %d of %d%s'
                  % (len(need) - len(miss), len(need), (' (missing: %s)' % ', '.join(miss)) if miss else ''))
        vb = os.path.join(run, 'vol.bin')
        t = None
        if has('vol.bin', 'vol.bin.txt'):
            t = read_txt(vb + '.txt')
            set_law(t)
            inj, integ = read_vol(vb)
            terms, csm = int(t['terms'][0]), int(t['csm'][0])
            # M
            s0, zb, invh, dfull, fard = fit(t)
            am = t['airMed']
            rel = lambda a, b: abs(a - b) <= 1e-4 * max(abs(b), 1e-9) + 1e-9
            top = float(t['sceneTopW'][0]) + t['fogView'][3] - float(t['camZw'][0])
            far = t['slices'][0] * np.exp(t['slices'][1])
            okm = rel(am[0], s0) and abs(am[1] - zb) <= 1e-3 * max(1.0, abs(zb)) and rel(am[2], invh) \
                and abs(am[3] - top) <= 1e-3 * max(1.0, abs(top)) and abs(far - max(fard, 64.0)) <= 1e-3 * far
            G.put('M', okm, 'fit sigma0 %.6g (dump %.6g) zb %.1f (%.1f) 1/H %.6g (%.6g) top %.1f (%.1f) far %.1f (fit %.1f, dFull %.1f; red %d)'
                  % (s0, am[0], zb, am[1], invh, am[2], top, am[3], far, fard, dfull, int(t['red'][0])))
            # H: the dumped air's haze along a horizontal ray below the band against the game's alpha there
            K = [t['fogK%d' % k] for k in range(6)]
            zl = (am[1] - 10.0) if am[2] > 0 else 0.0
            # judged up to the curve's end ramp (ramp 0.75): past it the game ramps to 1 on its own, the air does not
            knee = (0.75 + K[0][2]) / K[0][0]
            rz = float(rho(np.array([zl]), float(am[1]), float(am[2]), float(am[3]), ground=False)[0])
            hs = [(d, 1.0 - np.exp(-am[0] * d * rz), game_alpha(K, d, zl)) for d in (2000.0, 3000.0, 5000.0) if d <= knee]
            hw = max(abs(a - g) for _, a, g in hs)
            G.put('H', hw <= 0.10, 'street haze, air vs the game fog alpha: %s; worst |diff| %.3f (bar 0.10)'
                  % ('; '.join('%.0f: %.3f vs %.3f' % h for h in hs), hw))
            # D (LAMPAIR1): outdoors the air is lit by the surfaces' sun and sky (the weather's light x Sunlight Scale,
            # its DALC), not the fog-sun glare colour: the dump's directional equals the surfaces' 'cell sun:' line
            if int(t['cellOn'][0]) and not int(t['cellInterior'][0]):
                sl = None
                if has('vol.notes'):
                    for line in open(os.path.join(run, 'vol.notes'), encoding='utf-8', errors='replace'):
                        if 'cell sun: exterior directional=weather' in line and 'color=' in line:
                            sl = [float(x) for x in line.split('color=')[1].split()[0].split(',')]
                dc = t['cellDirColor']
                okd = sl is not None and int(t['cellHasDir'][0]) == 1 and int(t['cellExtDalc'][0]) == 1                     and all(abs(a - b) <= 1e-4 * max(abs(b), 1.0) for a, b in zip(dc, sl))
                G.put('D', okd, "the air's sun %s (has %d, sky DALC %d) vs the surfaces' %s"
                      % (' '.join('%.5f' % x for x in dc), int(t['cellHasDir'][0]), int(t['cellExtDalc'][0]),
                         'none in vol.notes' if sl is None else ' '.join('%.5f' % x for x in sl)))
            # R
            with_rgb = terms == 1 and csm == 0
            rinj = rebuild(t, with_rgb)
            # 10-19: the depth skip (opt line depthskip=1, the shipped OPT 3) leaves every froxel behind the column's
            # farthest surface at exactly (0, 0, 0, 1), never read (opt gate Z); R judges the froxels the pass computed.
            # Without depthskip=1 a held froxel is judged like any other (it fails).
            ds1 = 'depthskip=1' in str(t.get('opt', ''))
            held = np.all(inj == np.array([0.0, 0.0, 0.0, 1.0]), axis=-1) if ds1 else np.zeros(inj.shape[:-1], bool)
            jm = ~held
            sk = ' (depth skip: %.1f%% held, judged the rest)' % (100.0 * held.mean()) if ds1 else ''
            fa = (np.abs(inj[..., 3] - rinj[..., 3]) <= 1e-4)[jm].mean()
            if with_rgb:
                fr, tot, mx = agree(inj[..., :3][jm], rinj[..., :3][jm])
                G.put('R', fr >= 0.99 and abs(tot - 1) <= 0.01 and fa >= 0.99,
                      'inject vs rebuild: %.4f of the lit voxels within 1%%, total %.5f, transmittance %.4f within 1e-4 (max %.4g; medium %s)%s'
                      % (fr, tot, fa, mx, t.get('medNote', '?'), sk))
            else:
                G.put('R', fa >= 0.99, 'transmittance %.4f within 1e-4 (rgb judged elsewhere at terms %d, cascades %d)%s' % (fa, terms, csm, sk))
            # K
            if terms == 1:
                kc = colour_check(t, inj)
                if kc is None:
                    G.put('K', False, 'no lit voxel past 2000 units to judge the colour by')
                else:
                    G.put('K', kc[1] <= 0.01, "colour vs the records' fog colour past 2000 units: |chromaticity diff| median %.4f, "
                          "95th %.4f over %d voxels (bar 0.01; the record's at the far edge, centre ray: %.3f %.3f %.3f; red %d)"
                          % (kc[0], kc[1], kc[2], kc[3][0], kc[3][1], kc[3][2], int(t['red'][0])))
            # I
            ri = integrate(inj)
            fi, ti, _ = agree(integ[..., :3], ri[..., :3])
            fia = (np.abs(integ[..., 3] - ri[..., 3]) <= 1e-5).mean()
            G.put('I', fi >= 0.99 and abs(ti - 1) <= 0.01 and fia >= 0.99,
                  'integrated = front-to-back: %.4f within 1%%, total %.5f, transmittance %.4f within 1e-5' % (fi, ti, fia))
            # P (not at sanctfar: that place shoots only the far-tail frames W reads, by design; see its X list)
            if p == 'sanctfar':
                pass
            elif Image and has('probe.png', 'probet.png', 'geo.png'):
                ge = png(os.path.join(run, 'geo.png'))
                H, W = ge.shape[:2]
                vx, vy, vw, vh = t['viewport']
                fogged = (ge[..., 2] == 0) & (ge[..., 0] > 0)
                d = ge[..., 0] / 255.0 * 16384.0
                yy, xx = np.mgrid[0:H, 0:W]
                u = (xx + 0.5 - vx) / vw
                v = ((H - 1 - yy) + 0.5 - vy) / vh
                lo = np.full((H, W, 4), np.inf)
                hi = np.full((H, W, 4), -np.inf)
                for dd in (-32.0, -16.0, 0.0, 16.0, 32.0):
                    e = np.clip(vol_at(integ, t, np.maximum(d + dd, 0.0), u, v), 0.0, 1.0) * 255.0
                    lo, hi = np.minimum(lo, e), np.maximum(hi, e)
                n = int(fogged.sum())
                pl = png(os.path.join(run, 'probe.png'))
                pt = png(os.path.join(run, 'probet.png'))[..., 0]
                inl = ((pl >= lo[..., :3] - 2.0) & (pl <= hi[..., :3] + 2.0)).all(-1)
                intt = (pt >= lo[..., 3] - 2.0) & (pt <= hi[..., 3] + 2.0)
                fl = inl[fogged].mean() if n else 0.0
                ft = intt[fogged].mean() if n else 0.0
                lit = (fogged & (hi[..., :3].max(-1) >= 4.0)).sum()
                G.put('P', n > 1000 and fl >= 0.95 and ft >= 0.95,
                      'probes in the volume\'s range: L on %.4f, T on %.4f of %d fogged fragments (%d carry >= 4/255 of light)'
                      % (fl, ft, n, int(lit)))
                if has('vol.png', 'off.png'):
                    on, off = png(os.path.join(run, 'vol.png')), png(os.path.join(run, 'off.png'))
                    dif = (np.abs(on - off).max(-1) >= 2) & fogged
                    nd = int(dif.sum())
                    G.put('P', nd >= 5000 and nd >= 0.01 * n,
                          'applied: the air differs from the game\'s fog by >= 2/255 on %d of %d fogged fragments (%.3f)'
                          % (nd, n, nd / n if n else 0.0))
                else:
                    G.put('P', False, 'no vol.png / off.png')
            else:
                G.put('P', False, 'no probe.png / probet.png / geo.png')
            # L
            if int(t['cellInterior'][0]) and terms & 2:
                L = integ[-1, ..., :3]
                s = L.sum(-1)
                ncol = s.size
                G.put('L', L.sum() >= 1e-3 * ncol and (s >= 1e-3).mean() >= 0.01,
                      'the placed lights in the air: summed %.5g over %d columns (bar %.3g), %.4f of the columns lit (%d lights; red %d)'
                      % (L.sum(), ncol, 1e-3 * ncol, (s >= 1e-3).mean(), int(t['cellLightCount'][0]), int(t['red'][0])))
        elif red in ('', 'off', 'wrongmed', 'farfit', 'physcol', 'oldsun', 'nolights') and p != 'goodneighbor':
            G.put('R', False, 'no vol.bin dump (the pass did not run: %s)' % run)
        # W
        if Image and t is not None and has('probet.png', 'geod.png', 'geoz.png'):
            gd, gz = png(os.path.join(run, 'geod.png')), png(os.path.join(run, 'geoz.png'))
            Tp = png(os.path.join(run, 'probet.png'))[..., 0] / 255.0
            valid = (gd[..., 2] == 0) & (gz[..., 2] == 0) & ((gd[..., 0] + gd[..., 1]) > 0)
            dd = 2.0 ** ((gd[..., 0] * 256 + gd[..., 1]) / 65535.0 * 20.0)
            zz = (gz[..., 0] * 256 + gz[..., 1]) / 65535.0 * 131072.0 - 65536.0
            H_, W_ = Tp.shape
            vx, vy, vw, vh = t['viewport']
            yy, xx = np.mgrid[0:H_, 0:W_]
            uu, vv = (xx + 0.5 - vx) / vw, ((H_ - 1 - yy) + 0.5 - vy) / vh
            am = t['airMed']
            near, logR = t['slices']
            far = near * np.exp(logR)
            zc = float(t['fogView'][3])

            def seg_tau(za, zb_, ln):
                dz = zb_ - za
                flat = np.abs(dz) < 0.5
                ci = col_int(za, zb_, float(am[1]), float(am[2]), float(am[3]))
                mid = rho(0.5 * (za + zb_), float(am[1]), float(am[2]), float(am[3]))
                return float(am[0]) * ln * np.where(flat, mid, np.abs(ci) / np.where(flat, 1.0, np.abs(dz)))
            past = valid & (dd > 1.02 * far)
            ov = valid & (dd >= 0.5 * far) & (dd < 0.98 * far)
            msg, okw = [], True
            if past.any():
                d1, z1 = dd[past], zz[past]
                Te = vol_at(integ, t, np.full(d1.shape, far), uu[past], vv[past])[..., 3]
                z0 = zc + (z1 - zc) * far / d1
                tt = np.exp(-seg_tau(z0, z1, d1 - far))
                e = np.abs(Tp[past] - Te * tt)
                fr = (e <= 3.0 / 255).mean()
                teeth = float(np.median(Te * (1.0 - tt)))
                okw = okw and fr >= 0.95 and teeth >= 3.0 / 255
                msg.append('past the far edge (%.0f): %d px, %.4f within 3/255 of T_edge x exp(-tau_tail), predicted drop median %.4f (>= 3/255 to judge)'
                           % (far, d1.size, fr, teeth))
            else:
                okw = False
                msg.append('no pixel past the far edge (%.0f): nothing to judge' % far)
            if ov.any():
                d1, z1 = dd[ov], zz[ov]
                ta = np.exp(-seg_tau(np.full(d1.shape, zc), z1, d1))
                e = np.abs(Tp[ov] - ta)
                fo = (e <= 4.0 / 255).mean()
                okw = okw and fo >= 0.95
                msg.append('overlap (0.5-0.98 far): %d px, %.4f within 4/255 of the closed form from the eye' % (d1.size, fo))
            G.put('W', okw, 'the far tail: %s; red %d' % ('; '.join(msg), int(t['red'][0])))
        # U
        if Image and has('probesun.png', 'probesun.bin.txt', 'geo.png'):
            tu = read_txt(os.path.join(run, 'probesun.bin.txt'))
            set_law(tu)
            am, fv, sun = tu['airMed'], tu['fogView'], tu['fogSun']
            ge = png(os.path.join(run, 'geo.png'))
            pr = png(os.path.join(run, 'probesun.png'))[..., 0] / 255.0
            fogged = (ge[..., 2] == 0) & (ge[..., 0] > 0)
            z = (ge[..., 1] / 255.0 - 0.5) * 32768.0
            mu = max(float(fv[:3] @ sun[:3]) / float(np.linalg.norm(fv[:3])), 0.02)
            top = float(am[3])

            def tsun(zz):
                c = col_int(zz, np.full_like(zz, top), float(am[1]), float(am[2]), top)
                return np.where(zz >= top, 1.0, np.exp(-float(am[0]) * np.maximum(c, 0.0) / mu))
            zf = z[fogged]
            lo, hi = tsun(zf - 72.0), tsun(zf + 72.0)    # the geometry's z is 128-unit steps
            got = pr[fogged]
            ok = (got >= np.minimum(lo, hi) - 2.5 / 255) & (got <= np.maximum(lo, hi) + 2.5 / 255)
            dim = 1.0 - 0.5 * (lo + hi)
            G.put('U', zf.size > 0 and ok.mean() >= 0.99 and np.median(dim) >= 0.05,
                  "the surfaces' sun through the air: probe vs exp(-sigma0 column(z, top) / mu) on %d fogged pixels, %.4f within "
                  "the geometry's step +/- 2.5/255; predicted dimming median %.3f (must be >= 0.05 to judge; mu %.3f, red %d)"
                  % (zf.size, ok.mean() if zf.size else 0.0, float(np.median(dim)) if zf.size else 0.0, mu, int(tu['red'][0])))
        # O
        if Image and has('off.png', 'before.png'):
            a, b = png(os.path.join(run, 'off.png')), png(os.path.join(run, 'before.png'))
            flake = np.zeros(b.shape[:2], bool)
            if has('rung2.png'):
                flake = (png(os.path.join(run, 'rung2.png')) != b).any(-1)

            def same(x):
                m = (x != b).any(-1)
                if not m.any():
                    return True, 0, 0
                near = np.zeros_like(flake)
                ys, xs = np.nonzero(flake)
                for y, xx in zip(ys, xs):
                    near[max(0, y - 32):y + 33, max(0, xx - 32):xx + 33] = True
                return bool(np.abs(x - b).max() <= 1 and (m <= near).all()), int(m.sum()), int(np.abs(x - b).max())
            ok, nd, mx = same(a)
            G.put('O', ok, 'off vs before exe: %d pixels differ by <= %d (the before exe against itself: %d px)' % (nd, mx, int(flake.sum())))
            if has('vol.png') and not red:
                blind, nb, mb = same(png(os.path.join(run, 'vol.png')))
                G.put('O', not blind, 'its red: vol.png judged by the same rule must differ (%d px, max %d)' % (nb, mb))
        # S
        if has('shad.bin', 'shadref.bin', 'shad.bin.txt'):
            ts = read_txt(os.path.join(run, 'shad.bin.txt'))
            a = last_light(os.path.join(run, 'shad.bin')).sum(-1)
            b = last_light(os.path.join(run, 'shadref.bin')).sum(-1)
            m = b > 1e-6
            drop = (a[m] <= 0.98 * b[m]).mean() if m.any() else 0.0
            rise = (a[m] > 1.001 * b[m]).mean() if m.any() else 1.0
            G.put('S', int(ts['csm'][0]) == 1 and drop >= 0.01 and rise == 0.0,
                  'cascades %d: %.4f of the columns lose >= 2%% to the shadows, %.4f gain (red %d)'
                  % (int(ts['csm'][0]), drop, rise, int(ts['red'][0])))
        # G / F
        if has('gi.bin', 'gi.bin.txt'):
            tg = read_txt(os.path.join(run, 'gi.bin.txt'))
            last = last_light(os.path.join(run, 'gi.bin'))
            ncol = last.shape[0] * last.shape[1]
            G.put('G', int(tg['cellGiOn'][0]) == 1 and last.sum() >= 1e-3 * ncol,
                  'GI alone: summed %.5g over %d columns (bar %.3g; GI on %d, red %d)'
                  % (last.sum(), ncol, 1e-3 * ncol, int(tg['cellGiOn'][0]), int(tg['red'][0])))
            if has('giflat.bin'):
                lf = last_light(os.path.join(run, 'giflat.bin'))

                def spread(L):
                    s = L.sum(-1)
                    m = s > 1e-3 * max(s.max(), 1e-12)
                    rg = (L[..., 0] - L[..., 1]) / np.maximum(s, 1e-12)
                    return rg[m].std() if m.any() else 0.0
                sg, sf = spread(last), spread(lf)
                G.put('F', sg > 0 and sg >= 1.5 * sf, 'chromaticity spread (r - g): GI %.5f, flat %.5f, ratio %.2f'
                      % (sg, sf, sg / sf if sf > 0 else float('inf')))
        # N
        if has('neon_veil.bin', 'neon_off.bin'):
            a = last_light(os.path.join(run, 'neon_veil.bin')).sum(-1)
            b = last_light(os.path.join(run, 'neon_off.bin')).sum(-1)
            ratio = a.sum() / b.sum() if b.sum() > 0 else (float('inf') if a.sum() > 0 else 0.0)
            gain = (a >= 1.05 * b + 1e-6).mean()
            G.put('N', ratio >= 1.02 and gain >= 0.01,
                  'the air near the neon: summed light with the emitters / without %.4f, %.4f of the columns gain >= 5%%'
                  % (ratio, gain))
        # C
        if has('time.bin.txt'):
            t2 = read_txt(os.path.join(run, 'time.bin.txt'))
            ms = float(t2['ms'][0])
            G.put('C', ms > 0, 'cost: %.3f ms for %d x %d x %d froxels at %d x %d, all terms (%d lights, GI %d, cascades %d)'
                  % (ms, *(int(x) for x in t2['dims']), int(t2['viewport'][2]), int(t2['viewport'][3]),
                     int(t2['cellLightCount'][0]), int(t2['cellGiOn'][0]), int(t2['csm'][0])))
    want = {'off': ['P'], 'wrongmed': ['M'], 'farfit': ['H'], 'physcol': ['K'], 'oldsun': ['D'], 'nosunt': ['U'], 'froxonly': ['W'], 'noshadow': ['S'], 'nolights': ['L'], 'gioff': ['G', 'N'], 'flat': ['F']}
    failed = sorted(k for k, v in G.res.items() if not all(v))
    if red:
        hit = [k for k in want[red] if k in failed]
        print('RED %s: %s' % (red, 'FAILED as it must (%s)' % ', '.join(hit) if hit else 'DID NOT FAIL (%s all passed) -- the gate is blind'
                                % ', '.join(want[red])))
        return 1 if hit else 0
    print('VERDICT %s' % ('PASS' if not failed else 'FAIL (%s)' % ', '.join(failed)))
    return 0 if not failed else 1


if __name__ == '__main__':
    sys.exit(main())
