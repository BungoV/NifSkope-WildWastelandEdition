#!/usr/bin/env python3
"""Judge tests/spells/volfog1_cell.sh (lane VOLFOG1, 2026-10-04): the lit medium against an independent rebuild.

  python tests/spells/volfog1_cell_check.py <out dir> <place...>      (VOLFOG1_RED=<red> when judging a red)

The rebuild shares nothing with the exe but the dumped inputs (the fog constants fogK, the view, the projection,
the sun, the god-ray record's medium): the slices, the rays, the fog alpha (the FOG1-gated formula), the phase
(Rayleigh + two Henyey-Greenstein lobes weighted by the record's colours over their summed luminance), the 4-step
in-scatter S x d(alpha), the front-to-back sum and the apply are all re-done here in numpy (float64).

Stages (bars fixed before the first run):
  R  rebuild   the injected volume: rgb within 1% (+ 1e-5 x the volume's max) on >= 99% of the voxels that carry
               light, and the summed light within 1%; alpha within 1e-4 on >= 99%
               (interiors run terms 2, the placed shaft lights: the game's interiors carry no directional light;
               there R judges the alpha alone)
  I  integrate the integrated volume = the running sum of the injected one: same bars
  P  apply     the probe picture (the volume term each fragment added, raw, 8-bit) inside the range the dumped
               volume gives over the fragment's distance +/- 32 units (the geometry probe's step), +/- 2/255,
               on >= 95% of the fogged fragments AND on >= 95% of the lit ones (expected >= 4/255), and there is
               light: >= 1% and >= 5000 lit fragments
  A  added     on.png >= off.png - 1/255 on >= 99.5% of the fogged fragments, and on - off >= 2/255 on >= 1% and
               >= 5000 of them
               (REVISED 09:30 after gate5, logged in STATUS: the first bars, 10% lit / 10% gained, were written for
               the sun, which lights every froxel; an interior's one shaft light lights its cone, 1.6% of the frame.
               P gained the lit-fragment clause so the red off still fails it.)
  O  off       off.png (this exe, the row off) == before.png (the exe from before the lane, its own shaders) byte
               for byte; or, when the before exe is not byte-stable itself (rung2), differing pixels <= its own
  S  shadows   (sanctuary) the cascades on: the medium's light (last slice) drops by >= 2% against the noshadow
               comparator on >= 1% of the columns, and never rises by more than 0.1%
  G  GI        (interiors) the GI alone (terms 4) puts light in the medium: summed >= 1e-3 x the column count
  F  follows   (interiors) the GI's per-column chromaticity spread (std of r - g over the lit columns) >= 1.5 x the
               spread of the red flat comparator (one cube everywhere)
  C  cost      time.bin.txt's ms (glFinish to glFinish around the pass, all terms); reported against the GPURELIGHT1 budget (no bar here: the report)
  K  read cost (lane VOLFOG1b, interiors) time.bin.txt's ms under time6.bin.txt's (the same camera, red sixreads: the
               VOLFOG1 six surface reads a froxel)
Reds: off -> P A; wrongsrc -> R; noshadow -> S; gioff -> G; flat -> F.
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


def fog_alpha(K, d, z):
    ramp = d * K[0][0] - K[0][2]
    f = np.clip(ramp, 0.0, 1.0)
    pair0 = np.clip(z * K[5][0] - K[5][2], 0.0, 1.0)
    pair1 = np.clip(z * K[5][1] - K[5][3], 0.0, 1.0)
    hb = pair0 + (pair1 - pair0) * f
    mx = K[2][3]
    clampT = np.where(ramp > 0.75, np.minimum((f - 0.75) * 4.0 * (1.0 - mx) + mx, 1.0), mx)
    escape = np.where(ramp < 0.015, f * 66.666672, 1.0)
    I = np.where(f > 0.0, np.minimum(clampT, np.power(np.maximum(f, 1e-30), K[1][3])), 0.0)
    weight = hb * K[3][3] + (1.0 - hb)
    return weight * I * escape


def hg(c, g):
    d = 1.0 + g * g - 2.0 * g * c
    return (1.0 - g * g) / (4.0 * PI * d * np.sqrt(d))


def phase_pi(med, c):
    """pi x the phase, rgb: the record's three scattering colours normalised so their sum has a luminance of 1"""
    air, fwd, back = med[0:3], med[3:6], med[6:9]
    lum = float(LUM @ (air + fwd + back))
    inv = 1.0 / lum if lum > 1e-6 else 0.0
    c = c[..., None]
    return PI * ((air * inv) * (3.0 / (16.0 * PI)) * (1.0 + c * c) + (fwd * inv) * hg(c, med[9]) + (back * inv) * hg(c, med[10]))


def rebuild(t):
    cols, rows, N = (int(x) for x in t['dims'])
    near, logR = t['slices']
    p00, p11 = t['proj']
    K = [t['fogK%d' % k] for k in range(6)]
    fv, sun, sunc, ds = t['fogView'], t['fogSun'], t['fogSunColour'], float(t['fogDistScale'][0])
    med = t['medium']
    terms = int(t['terms'][0])
    jj, ii = np.meshgrid(np.arange(rows), np.arange(cols), indexing='ij')
    ndc = np.stack([(ii + 0.5) / cols * 2 - 1, (jj + 0.5) / rows * 2 - 1], -1)
    dv = np.stack([ndc[..., 0] / p00, ndc[..., 1] / p11, -np.ones_like(ndc[..., 0])], -1)
    dv /= np.linalg.norm(dv, axis=-1, keepdims=True)
    S = np.zeros((rows, cols, 3))
    if sun[3] > 0 and terms & 1:
        S += sunc[:3] * sun[3] * phase_pi(med, dv @ sun[:3])
    if int(t['cellOn'][0]) and int(t['cellInterior'][0]) and int(t['cellHasDir'][0]) and terms & 1:
        Rw = np.stack([t['cellRow%d' % k][:3] for k in range(3)])
        dw = dv @ Rw.T
        dw /= np.linalg.norm(dw, axis=-1, keepdims=True)
        to = t['cellDirTo'] / np.linalg.norm(t['cellDirTo'])
        S += t['cellDirColor'] * phase_pi(med, dw @ to)
    S *= float(t['post'][0])  # the record's intensity
    zdir = dv @ fv[:3]

    def alpha(tt):
        return fog_alpha(K, tt, zdir * (tt / ds) + fv[3])

    inj = np.zeros((N, rows, cols, 4))
    for k in range(N):
        t0 = near * np.exp(logR * k / N)
        t1 = near * np.exp(logR * (k + 1) / N)
        a0 = alpha(t0)
        acc = np.zeros((rows, cols))
        for q in range(4):
            a1 = alpha(t0 + (t1 - t0) * (q + 1) * 0.25)
            acc += np.maximum(a1 - a0, 0.0)
            a0 = a1
        inj[k, ..., :3] = acc[..., None] * S  # the directional S is constant along a ray (no shadows here)
        inj[k, ..., 3] = a0
    integ = inj.copy()
    integ[..., :3] = np.cumsum(inj[..., :3], axis=0)
    return inj, integ


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
    """GL_LINEAR on the integrated volume (slices, rows, cols, 4), texel centres at +0.5, clamped to edge"""
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
                out = out + vol[zi, yi, xi, :3] * wz * wy * wx
    return out


def vol_at(integ, t, d, u, v):
    near, logR = t['slices']
    N = integ.shape[0]
    f = np.log(np.maximum(d, 1e-6) / near) / logR * N
    lo = trilinear(integ, u, v, np.zeros_like(d))  # layer 0's centre
    full = trilinear(integ, u, v, np.minimum(f, N) - 1.0)  # GL coordinate (f - 0.5) / N = texel f - 1
    out = np.where((f < 1.0)[..., None], lo * f[..., None], full)
    return np.where((d <= near)[..., None], 0.0, np.maximum(out, 0.0))


class Gate:
    def __init__(self):
        self.res = {}

    def put(self, key, ok, text):
        self.res.setdefault(key, []).append(ok)
        print('  %s  %s  %s' % ('PASS' if ok else 'FAIL', key, text))


def main():
    out = sys.argv[1]
    places = sys.argv[2:]
    red = os.environ.get('VOLFOG1_RED', '')
    G = Gate()
    for p in places:
        run = os.path.join(out, p)
        print('== %s' % p)
        vb = os.path.join(run, 'vol.bin')
        if os.path.exists(vb) and os.path.exists(vb + '.txt'):
            t = read_txt(vb + '.txt')
            inj, integ = read_vol(vb)
            rinj, rint = rebuild(t)
            fr, tot, mx = agree(inj[..., :3], rinj[..., :3])
            fa = (np.abs(inj[..., 3] - rinj[..., 3]) <= 1e-4).mean()
            if int(t['terms'][0]) & ~1:
                # the placed lights / the GI are not rebuilt here (the twin and S/G/F gate them): the medium's
                # opacity (alpha, the fog records') is, and must still agree
                G.put('R', fa >= 0.99,
                      'alpha %.4f within 1e-4 (rgb not rebuilt at terms %d: placed lights / GI; medium %s, red %d)'
                      % (fa, int(t['terms'][0]), t.get('medNote', '?'), int(t['red'][0])))
            else:
                G.put('R', fr >= 0.99 and abs(tot - 1) <= 0.01 and fa >= 0.99,
                      'inject vs rebuild: %.4f of the lit voxels within 1%%, total %.5f, alpha %.4f within 1e-4 (max %.4g; medium %s, terms %d, red %d)'
                      % (fr, tot, fa, mx, t.get('medNote', '?'), int(t['terms'][0]), int(t['red'][0])))
            cs = np.cumsum(inj[..., :3], axis=0)
            fi, ti, _ = agree(integ[..., :3], cs)
            fia = (np.abs(integ[..., 3] - inj[..., 3]) <= 1e-6).mean()
            G.put('I', fi >= 0.99 and abs(ti - 1) <= 0.01 and fia >= 0.99,
                  'integrated = running sum: %.4f within 1%%, total %.5f, alpha %.4f' % (fi, ti, fia))
            # P: the probe picture against the dumped volume at each fragment's distance
            if Image and all(os.path.exists(os.path.join(run, f)) for f in ('probe.png', 'geo.png')):
                pr, ge = png(os.path.join(run, 'probe.png')), png(os.path.join(run, 'geo.png'))
                H, W = ge.shape[:2]
                vx, vy, vw, vh = t['viewport']
                fogged = (ge[..., 2] == 0) & (ge[..., 0] > 0)
                d = ge[..., 0] / 255.0 * 16384.0
                yy, xx = np.mgrid[0:H, 0:W]
                u = (xx + 0.5 - vx) / vw
                v = ((H - 1 - yy) + 0.5 - vy) / vh  # the picture's rows run top-down, GL's bottom-up
                lo = np.full((H, W, 3), np.inf)
                hi = np.full((H, W, 3), -np.inf)
                for dd in (-32.0, -16.0, 0.0, 16.0, 32.0):
                    e = np.clip(vol_at(integ, t, np.maximum(d + dd, 0.0), u, v), 0.0, 1.0) * 255.0
                    lo, hi = np.minimum(lo, e), np.maximum(hi, e)
                inside = ((pr >= lo - 2.0) & (pr <= hi + 2.0)).all(-1)
                n = int(fogged.sum())
                fin = inside[fogged].mean() if n else 0.0
                litm = fogged & (hi.max(-1) >= 4.0)
                nl = int(litm.sum())
                lit = nl / n if n else 0.0
                fil = inside[litm].mean() if nl else 0.0
                G.put('P', n > 1000 and fin >= 0.95 and fil >= 0.95 and lit >= 0.01 and nl >= 5000,
                      'probe in the volume\'s range on %.4f of %d fogged fragments, on %.4f of the %d lit ones (expected >= 4/255: %.3f)'
                      % (fin, n, fil, nl, lit))
                if all(os.path.exists(os.path.join(run, f)) for f in ('on.png', 'off.png')):
                    on, off = png(os.path.join(run, 'on.png')), png(os.path.join(run, 'off.png'))
                    nondark = (on >= off - 1).all(-1)[fogged].mean() if n else 0.0
                    gained = ((on - off).max(-1) >= 2)[fogged].mean() if n else 0.0
                    ng = int((((on - off).max(-1) >= 2) & fogged).sum())
                    G.put('A', nondark >= 0.995 and gained >= 0.01 and ng >= 5000,
                          'on >= off on %.4f of the fogged fragments; on - off >= 2/255 on %.3f (%d)' % (nondark, gained, ng))
            else:
                G.put('P', False, 'no probe.png / geo.png')
        elif red in ('', 'off', 'wrongsrc'):
            G.put('R', False, 'no vol.bin dump (the pass did not run: %s)' % run)
        # O: off == before
        if Image and all(os.path.exists(os.path.join(run, f)) for f in ('off.png', 'before.png')):
            a, b = png(os.path.join(run, 'off.png')), png(os.path.join(run, 'before.png'))
            flake = np.zeros(b.shape[:2], bool)
            if os.path.exists(os.path.join(run, 'rung2.png')):
                flake = (png(os.path.join(run, 'rung2.png')) != b).any(-1)

            def same(x):
                """byte for byte, or every difference one level and within 32 px of a pixel where the before exe
                differs from ITSELF (its own flaky patch; lane BAKEBLOCK1 measured 9 px on Sanctuary)"""
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
            if os.path.exists(os.path.join(run, 'on.png')) and not red:
                blind, nb, mb = same(png(os.path.join(run, 'on.png')))
                G.put('O', not blind, 'its red: on.png judged by the same rule must differ (%d px, max %d)' % (nb, mb))
        # S: shadows
        sb, sr = os.path.join(run, 'shad.bin'), os.path.join(run, 'shadref.bin')
        if os.path.exists(sb) and os.path.exists(sr):
            ts = read_txt(sb + '.txt')
            a = read_vol(sb)[1][-1, ..., :3].sum(-1)
            b = read_vol(sr)[1][-1, ..., :3].sum(-1)
            m = b > 1e-6
            drop = (a[m] <= 0.98 * b[m]).mean() if m.any() else 0.0
            rise = (a[m] > 1.001 * b[m]).mean() if m.any() else 1.0
            G.put('S', int(ts['csm'][0]) == 1 and drop >= 0.01 and rise == 0.0,
                  'cascades %d: %.4f of the columns lose >= 2%% to the shadows, %.4f gain' % (int(ts['csm'][0]), drop, rise))
        # G / F: the GI
        gb, gf = os.path.join(run, 'gi.bin'), os.path.join(run, 'giflat.bin')
        if os.path.exists(gb):
            tg = read_txt(gb + '.txt')
            last = read_vol(gb)[1][-1, ..., :3]
            tot = last.sum()
            cols = last.shape[0] * last.shape[1]
            G.put('G', tot >= 1e-3 * cols, 'GI alone: summed %.5g over %d columns (bar %.3g; red %d)' % (tot, cols, 1e-3 * cols, int(tg['red'][0])))
            if os.path.exists(gf):
                lf = read_vol(gf)[1][-1, ..., :3]

                def spread(L):
                    s = L.sum(-1)
                    m = s > 1e-3 * max(s.max(), 1e-12)
                    rg = (L[..., 0] - L[..., 1]) / np.maximum(s, 1e-12)
                    return rg[m].std() if m.any() else 0.0
                sg, sf = spread(last), spread(lf)
                G.put('F', sf >= 0 and sg >= 1.5 * sf and sg > 0, 'chromaticity spread (r - g): GI %.5f, flat %.5f, ratio %.2f'
                      % (sg, sf, sg / sf if sf > 0 else float('inf')))
        tt = os.path.join(run, 'time.bin.txt')
        if os.path.exists(tt):
            t2 = read_txt(tt)
            ms = float(t2['ms'][0])
            G.put('C', ms > 0, 'cost: %.3f ms for %s x %s x %s froxels, all terms (%d emitters, cascades %d), against the GPURELIGHT1 GPU relight, 13.5 ms'
                  % (ms, *(int(x) for x in t2['dims']), int(t2['emitters'][0]), int(t2['csm'][0])))
            t6 = os.path.join(run, 'time6.bin.txt')
            if os.path.exists(t6):   # lane VOLFOG1b: the same camera with the VOLFOG1 six surface reads
                ms6 = float(read_txt(t6)['ms'][0])
                G.put('K', 0 < ms < ms6, 'GI read cost: one room lookup %.3f ms, VOLFOG1 six reads %.3f ms (%.2fx)'
                      % (ms, ms6, ms6 / ms if ms > 0 else float('inf')))
    want = {'off': ['P', 'A'], 'wrongsrc': ['R'], 'noshadow': ['S'], 'gioff': ['G'], 'flat': ['F']}
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
