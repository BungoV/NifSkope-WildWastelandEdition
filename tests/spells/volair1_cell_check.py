#!/usr/bin/env python3
"""Judge tests/spells/volair1_cell.sh (lane VOLAIR1, 2026-10-13): the Volumetric Air against an independent rebuild.

  python tests/spells/volair1_cell_check.py <out dir> <place...>      (VOLAIR1_RED=<red> when judging a red)

The rebuild shares nothing with the exe but the dumped inputs (the fog constants fogK, the view, the projection, the
directional light, the god-ray record, the scene's top): the medium's fit, the slices, the rays, the optical depths
(here by Gauss-Legendre quadrature of the density, split at its kinks; the exe uses a closed form), the phase
(Rayleigh + two Henyey-Greenstein lobes over the record's summed luminance), the transmittance to the top, the
four-sub-step inject, the front-to-back integrate and the apply are re-done in numpy (float64).

Stages (bars fixed before the first run):
  M  medium    sigma0, zb, 1/H re-fitted from fogK (1 - exp(-sigma0 dFull) = the records' max alpha; the height band
               K5's pair-0 edges, the density at its top = the records' top weight) equal the dumped airMed within
               1e-4 relative; the top = the scene's top in the fog's frame; the far slice = the full distance
  R  rebuild   the injected volume: alpha (the slice's transmittance) within 1e-4 on >= 99% of the voxels; at terms
               1 without the cascades also rgb within 1% (+ 1e-5 x max) on >= 99% of the lit voxels, total within 1%
               (terms 2 / 4: alpha alone -- the placed lights and the GI are judged by L G F N and by the twins)
  I  integrate the integrated volume = L_k = L_(k-1) + T_(k-1) inj_k, T_k = prod inj.a: rgb as R, alpha 1e-5
  P  apply     the L probe inside the range the dumped volume gives over the fragment's distance +/- 32 units, +/- 2/255,
               on >= 95% of the fogged fragments; the T probe likewise; AND applied: vol.png differs from off.png
               (the game's fog) by >= 2/255 on >= 1% and >= 5000 of the fogged fragments
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
Reds: off -> P; wrongmed -> M; noshadow -> S; nolights -> L; gioff -> G (and N when run); flat -> F.
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


def fit(t):
    """the medium from the fog records, re-derived: (sigma0, zb, invH, dFull)"""
    K = [t['fogK%d' % k] for k in range(6)]
    dfull = (1.0 + K[0][2]) / K[0][0]
    mx = min(max(K[2][3], 0.0), 0.99)
    sigma0 = -np.log(1.0 - mx) / dfull if mx > 0 and dfull > 0 else 0.0
    zb, invh = 0.0, 0.0
    if K[5][1] > 1e-9 and 0.0 < K[3][3] < 1.0:
        z0, z1 = K[5][3] / K[5][1], (1.0 + K[5][3]) / K[5][1]
        zb = z0
        invh = -np.log(max(K[3][3], 1e-3)) / max(z1 - z0, 1e-3)
    return sigma0, zb, invh, dfull


def rho(z, zb, invh, top):
    r = np.minimum(1.0, np.exp(-(z - zb) * invh)) if invh > 0 else np.ones_like(z)
    return np.where(z > top, 0.0, r)


def col_int(za, zc, zb, invh, top):
    """the integral of rho dz from za to zc (arrays), by Gauss-Legendre split at zb and the top (rho's kinks)"""
    lo, hi = np.minimum(za, zc), np.maximum(za, zc)
    sgn = np.where(zc >= za, 1.0, -1.0)
    cuts = sorted([zb, top])
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


def phase_pi(med, c):
    """pi x the phase, rgb: the record's three scattering colours over the luminance of their sum"""
    air, fwd, back = med[0:3], med[3:6], med[6:9]
    lum = float(LUM @ (air + fwd + back))
    inv = 1.0 / lum if lum > 1e-6 else 0.0
    c = c[..., None]
    return PI * ((air * inv) * (3.0 / (16.0 * PI)) * (1.0 + c * c) + (fwd * inv) * hg(c, med[9]) + (back * inv) * hg(c, med[10]))


def rebuild(t, with_rgb):
    cols, rows, N = (int(x) for x in t['dims'])
    near, logR = t['slices']
    p00, p11 = t['proj']
    fv, ds = t['fogView'], float(t['fogDistScale'][0])
    sigma0, zb, invh, _ = fit(t)
    top = float(t['airMed'][3])
    med = t['medium']
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
                acc += (T * -np.expm1(-tau) * tup(zc + dzdt * tm))[..., None] * S
            T = T * np.exp(-tau)
        inj[k, ..., :3] = acc
        inj[k, ..., 3] = T
    return inj


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
    out = sys.argv[1]
    places = sys.argv[2:]
    red = os.environ.get('VOLAIR1_RED', '')
    G = Gate()
    for p in places:
        run = os.path.join(out, p)
        has = lambda *fs: all(os.path.exists(os.path.join(run, f)) for f in fs)
        print('== %s' % p)
        vb = os.path.join(run, 'vol.bin')
        t = None
        if has('vol.bin', 'vol.bin.txt'):
            t = read_txt(vb + '.txt')
            inj, integ = read_vol(vb)
            terms, csm = int(t['terms'][0]), int(t['csm'][0])
            # M
            s0, zb, invh, dfull = fit(t)
            am = t['airMed']
            rel = lambda a, b: abs(a - b) <= 1e-4 * max(abs(b), 1e-9) + 1e-9
            top = float(t['sceneTopW'][0]) + t['fogView'][3] - float(t['camZw'][0])
            far = t['slices'][0] * np.exp(t['slices'][1])
            okm = rel(am[0], s0) and abs(am[1] - zb) <= 1e-3 * max(1.0, abs(zb)) and rel(am[2], invh) \
                and abs(am[3] - top) <= 1e-3 * max(1.0, abs(top)) and abs(far - max(dfull, 64.0)) <= 1e-3 * far
            G.put('M', okm, 'fit sigma0 %.6g (dump %.6g) zb %.1f (%.1f) 1/H %.6g (%.6g) top %.1f (%.1f) far %.1f (dFull %.1f; red %d)'
                  % (s0, am[0], zb, am[1], invh, am[2], top, am[3], far, dfull, int(t['red'][0])))
            # R
            with_rgb = terms == 1 and csm == 0
            rinj = rebuild(t, with_rgb)
            fa = (np.abs(inj[..., 3] - rinj[..., 3]) <= 1e-4).mean()
            if with_rgb:
                fr, tot, mx = agree(inj[..., :3], rinj[..., :3])
                G.put('R', fr >= 0.99 and abs(tot - 1) <= 0.01 and fa >= 0.99,
                      'inject vs rebuild: %.4f of the lit voxels within 1%%, total %.5f, transmittance %.4f within 1e-4 (max %.4g; medium %s)'
                      % (fr, tot, fa, mx, t.get('medNote', '?')))
            else:
                G.put('R', fa >= 0.99, 'transmittance %.4f within 1e-4 (rgb judged elsewhere at terms %d, cascades %d)' % (fa, terms, csm))
            # I
            ri = integrate(inj)
            fi, ti, _ = agree(integ[..., :3], ri[..., :3])
            fia = (np.abs(integ[..., 3] - ri[..., 3]) <= 1e-5).mean()
            G.put('I', fi >= 0.99 and abs(ti - 1) <= 0.01 and fia >= 0.99,
                  'integrated = front-to-back: %.4f within 1%%, total %.5f, transmittance %.4f within 1e-5' % (fi, ti, fia))
            # P
            if Image and has('probe.png', 'probet.png', 'geo.png'):
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
        elif red in ('', 'off', 'wrongmed', 'nolights') and p != 'goodneighbor':
            G.put('R', False, 'no vol.bin dump (the pass did not run: %s)' % run)
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
    want = {'off': ['P'], 'wrongmed': ['M'], 'noshadow': ['S'], 'nolights': ['L'], 'gioff': ['G', 'N'], 'flat': ['F']}
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
