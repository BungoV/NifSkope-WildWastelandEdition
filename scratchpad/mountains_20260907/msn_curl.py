"""Is vanilla's terrain `_msn` high-frequency detail INTEGRABLE?

A normal map baked from a height field is curl-free.  Write the slope field the
normal encodes, in texture coordinates (u east, v SOUTH -- row 0 is the north
edge, WW_CHANGES 2026-09-07):

    P = dh/du = -n_east  / n_up
    R = dh/dv = +n_north / n_up

then h exists iff  dP/dv == dR/du.  A field composited from material normal maps
has no reason to satisfy it.  That is the whole question left on the terrain
thread: integrable means the detail came from a finer height field than
Fallout4.esm ships (data we do not have); not integrable means it came from
landscape material normals, which we can reproduce.

TWO MEASUREMENTS of the same thing, because neither is above suspicion alone:

  CURL RATIO   rms(dP/dv - dR/du) / rms(dP/dv + dR/du), one-texel central
               differences.  Local, no transform, but a finite stencil has
               truncation error at these frequencies, so its floor is not zero.
  NON-INT      the Helmholtz split.  Mirror the field so it is periodic (odd in
               u for P, even for R, and the reverse in v -- the standard even
               extension of h), FFT, and split each frequency into the part
               parallel to k (a gradient) and the part perpendicular (not).
               NON-INT is the perpendicular share of the energy: 0 for a height
               field, ~0.5 for an isotropic random field, ~1 for one built to be
               solenoidal.

THE FLOOR CANNOT BE OUR OWN SHIPPED SHEET.  It was tried first and it fails, for
a reason worth keeping: our high-frequency residual is 1.9-2.4 bytes against
vanilla's 13.3-14.5, so almost all of what ours has in this band IS the block
codec, and a codec's error is not integrable.  Our sheet reads 0.93 on the curl
ratio -- indistinguishable from its own swapped copy.  Measuring vanilla against
that floor would prove nothing.

So the controls are built to carry VANILLA'S OWN amplitude and spectrum through
the same encoder:

  INT-CTRL   vanilla's residual Helmholtz-projected onto its curl-free part, in
             float, recombined with vanilla's low-frequency field, re-encoded to
             8-bit normals and run through a 4-colour block codec.  An
             integrable field of vanilla's size, seen through vanilla's optics.
             THE FLOOR.
  SOL-CTRL   the same, with the solenoidal part instead, rescaled to the same
             rms.  Same size, same spectrum, no height behind it.  THE CEILING.
  OURS       our shipped sheet, reported for the record, not used as a gate.
  VAN-SWAP   vanilla's residual with its components exchanged.  NOT a ceiling:
             if vanilla is already non-integrable this reads the same, and that
             agreement is itself a reading.

The block codec is emulated (`bc_roundtrip`), not read from a file, and it is
checked against vanilla's own texels before any control uses it.

Usage:  python msn_curl.py [tile ...]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dds import DDS
from bcnp import decode_rgb

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
OURS = r'C:/Users/bungo/AppData/Local/Temp/claude/laneb/gen/peak/tex'

TILES = ['Commonwealth.4.-12.44', 'Commonwealth.4.-16.44', 'Commonwealth.4.-12.48']
BLUR = 9          # the local blur the earlier high-frequency measurement used
MARGIN = 8        # crop, so no edge-padded texel reaches a printed number
UP_FLOOR = 0.15   # n_up below this is not trusted as a divisor
GATE = 5.0        # required ceiling/floor separation


# ---------------------------------------------------------------- basics

def boxmean(a, k):
    """Mean over a k x k window, edge-padded, same shape out (msn_features.py)."""
    r = k // 2
    p = np.pad(np.asarray(a, dtype=np.float64), [(r, r), (r, r)], mode='edge')
    cs = np.cumsum(p, axis=0)
    cs = np.concatenate([np.zeros((1,) + cs.shape[1:]), cs], axis=0)
    s = cs[k:] - cs[:-k]
    cs = np.cumsum(s, axis=1)
    cs = np.concatenate([np.zeros((cs.shape[0], 1)), cs], axis=1)
    return (cs[:, k:] - cs[:, :-k]) / float(k * k)


def slopes(rgb):
    """(h,w,3) uint8 _msn -> (P, R) = (dh/du, dh/dv).  FO4: R east, G up, B north."""
    n = np.asarray(rgb, dtype=np.float64) / 255.0 * 2.0 - 1.0
    up = np.maximum(n[:, :, 1], UP_FLOOR)
    return -n[:, :, 0] / up, n[:, :, 2] / up


def encode(P, R):
    """(dh/du, dh/dv) -> the 8-bit RGB an _msn stores."""
    n = np.stack([-P, np.ones_like(P), R], axis=2)
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return np.clip(np.rint((n * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)


def highpass(P, R, k=BLUR):
    return P - boxmean(P, k), R - boxmean(R, k)


# ---------------------------------------------------------------- metric 1

def curl_ratio(P, R):
    """rms(dP/dv - dR/du) / rms(dP/dv + dR/du), all texels and in-block only.

    A central difference at column u reads u-1 and u+1, so it stays inside its
    4x4 compression block only when u % 4 is 1 or 2.  The in-block figure keeps
    only texels where BOTH differences do.
    """
    dPdv = (P[2:, :] - P[:-2, :])[:, 1:-1] * 0.5
    dRdu = (R[:, 2:] - R[:, :-2])[1:-1, :] * 0.5
    m = MARGIN
    c = (dPdv - dRdu)[m:-m, m:-m]
    s = (dPdv + dRdu)[m:-m, m:-m]
    h, w = c.shape
    vv, uu = (np.arange(h) + 1 + m) % 4, (np.arange(w) + 1 + m) % 4
    inb = np.outer((vv == 1) | (vv == 2), (uu == 1) | (uu == 2))

    def rms(x, msk=None):
        y = x if msk is None else x[msk]
        return float(np.sqrt(np.mean(y * y)))

    return (rms(c) / rms(s), rms(c, inb) / rms(s, inb))


# ---------------------------------------------------------------- metric 2

def mirror(P, R):
    """Even extension of h: P odd in u / even in v, R even in u / odd in v."""
    h, w = P.shape
    Pe = np.zeros((2 * h, 2 * w))
    Re = np.zeros((2 * h, 2 * w))
    Pe[:h, :w], Re[:h, :w] = P, R
    Pe[:h, w:] = -P[:, ::-1]
    Re[:h, w:] = R[:, ::-1]
    Pe[h:, :w] = Pe[:h, :w][::-1, :]
    Re[h:, :w] = -Re[:h, :w][::-1, :]
    Pe[h:, w:] = Pe[:h, w:][::-1, :]
    Re[h:, w:] = -Re[:h, w:][::-1, :]
    return Pe, Re


def helmholtz(P, R):
    """(curl-free part, solenoidal part, non-integrable energy fraction)."""
    h, w = P.shape
    Pe, Re = mirror(P, R)
    H, W = Pe.shape
    Ph, Rh = np.fft.fft2(Pe), np.fft.fft2(Re)
    ku = np.fft.fftfreq(W)[None, :] * 2 * np.pi
    kv = np.fft.fftfreq(H)[:, None] * 2 * np.pi
    kn = np.sqrt(ku ** 2 + kv ** 2)
    kn[0, 0] = 1.0
    eu, ev = ku / kn, kv / kn
    dot = eu * Ph + ev * Rh
    Pc, Rc = dot * eu, dot * ev
    Ps, Rs = Ph - Pc, Rh - Rc
    Ps[0, 0] = Rs[0, 0] = 0.0
    num = float(np.sum(np.abs(Ps) ** 2 + np.abs(Rs) ** 2))
    den = float(np.sum(np.abs(Ph) ** 2 + np.abs(Rh) ** 2))
    cf = (np.real(np.fft.ifft2(Pc))[:h, :w], np.real(np.fft.ifft2(Rc))[:h, :w])
    so = (np.real(np.fft.ifft2(Ps))[:h, :w], np.real(np.fft.ifft2(Rs))[:h, :w])
    return cf, so, num / den


def nonint(P, R):
    m = MARGIN
    return helmholtz(P[m:-m, m:-m], R[m:-m, m:-m])[2]


BANDS = [(16.0, 1e9), (8.0, 16.0), (4.0, 8.0), (2.7, 4.0), (2.0, 2.7)]


def nonint_bands(P, R):
    """NON-INT restricted to each band of spatial period, in texels.

    A block codec's error lives at the finest periods.  Real content does not
    have to.  If vanilla's non-integrable share is flat across the bands it is
    content; if it is all at period 2-3 it is the encoder.
    """
    m = MARGIN
    Pe, Re = mirror(P[m:-m, m:-m], R[m:-m, m:-m])
    H, W = Pe.shape
    Ph, Rh = np.fft.fft2(Pe), np.fft.fft2(Re)
    ku = np.fft.fftfreq(W)[None, :] * 2 * np.pi
    kv = np.fft.fftfreq(H)[:, None] * 2 * np.pi
    kn = np.sqrt(ku ** 2 + kv ** 2)
    kn[0, 0] = 1.0
    eu, ev = ku / kn, kv / kn
    dot = eu * Ph + ev * Rh
    Ps, Rs = Ph - dot * eu, Rh - dot * ev
    per = 2 * np.pi / np.maximum(kn, 1e-12)          # period in texels
    out = []
    for a, b in BANDS:
        msk = (per >= a) & (per < b)
        msk[0, 0] = False
        den = float(np.sum(np.abs(Ph[msk]) ** 2 + np.abs(Rh[msk]) ** 2))
        num = float(np.sum(np.abs(Ps[msk]) ** 2 + np.abs(Rs[msk]) ** 2))
        out.append((num / den if den > 0 else float('nan'), den))
    return out


# ---------------------------------------------------------------- the codec

def bc_roundtrip(rgb):
    """Emulate a 4-colour 4x4 block codec (BC1 opaque / the BC3 colour block).

    Per block: principal axis of the 16 colours, endpoints at the extremes of
    the projection, quantised to RGB565 with the decoder's own bit replication,
    a 4-entry palette, nearest index.  Returns the decoded texels.
    """
    a = np.asarray(rgb, dtype=np.float64)
    h, w, _ = a.shape
    b = a.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3)
    mu = b.mean(axis=1, keepdims=True)
    d = b - mu
    # principal axis by a few power iterations on the 3x3 covariance
    cov = np.einsum('nij,nik->njk', d, d)
    v = np.tile(np.array([1.0, 1.0, 1.0]), (b.shape[0], 1))
    for _ in range(12):
        v = np.einsum('njk,nk->nj', cov, v)
        nv = np.linalg.norm(v, axis=1, keepdims=True)
        v = np.where(nv > 1e-12, v / np.maximum(nv, 1e-12), np.array([1.0, 0.0, 0.0]))
    t = np.einsum('nij,nj->ni', d, v)
    lo, hi = t.min(axis=1), t.max(axis=1)
    e0 = mu[:, 0, :] + v * hi[:, None]
    e1 = mu[:, 0, :] + v * lo[:, None]

    def q565(c):
        c = np.clip(c, 0, 255)
        r = np.rint(c[:, 0] / 255.0 * 31).astype(np.int64)
        g = np.rint(c[:, 1] / 255.0 * 63).astype(np.int64)
        bl = np.rint(c[:, 2] / 255.0 * 31).astype(np.int64)
        return np.stack([(r * 527 + 23) >> 6, (g * 259 + 33) >> 6,
                         (bl * 527 + 23) >> 6], axis=1).astype(np.float64)

    p0, p1 = q565(e0), q565(e1)
    pal = np.stack([p0, p1, (2 * p0 + p1) // 3, (p0 + 2 * p1) // 3], axis=1)
    dist = ((b[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(axis=3)
    idx = dist.argmin(axis=2)
    out = np.take_along_axis(pal, idx[:, :, None].repeat(3, axis=2), axis=1)
    out = out.reshape(h // 4, w // 4, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3)
    return np.clip(out, 0, 255).astype(np.uint8)


# ------------------------------------------------- the supersampling refuter

def fractal_height(n, beta, seed):
    """A random height field with a k^-beta power spectrum."""
    rng = np.random.default_rng(seed)
    w = rng.normal(size=(n, n))
    F = np.fft.fft2(w)
    ku = np.fft.fftfreq(n)[None, :] * 2 * np.pi
    kv = np.fft.fftfreq(n)[:, None] * 2 * np.pi
    k = np.sqrt(ku ** 2 + kv ** 2)
    k[0, 0] = 1.0
    F *= k ** (-beta / 2.0)
    F[0, 0] = 0.0
    return np.real(np.fft.ifft2(F)), (ku, kv)


def exact_slopes(h, kk):
    ku, kv = kk
    H = np.fft.fft2(h)
    return (np.real(np.fft.ifft2(1j * ku * H)),
            np.real(np.fft.ifft2(1j * kv * H)))


def randphase(P, R, seed=3):
    """A field with the SAME 2D amplitude spectrum as (P,R) and random phase.

    The paired floor is built by projecting vanilla's own residual, so it shares
    vanilla's structure as well as its spectrum, and a sceptic can call it
    self-serving.  This one keeps only the spectrum.  Projected curl-free it is
    an INDEPENDENT integrable field that the codec sees exactly as it sees
    vanilla's.
    """
    rng = np.random.default_rng(seed)
    h, w = P.shape
    Pe, Re = mirror(P, R)
    out = []
    for A in (Pe, Re):
        F = np.fft.fft2(A)
        W = np.fft.fft2(rng.normal(size=A.shape))
        U = W / np.maximum(np.abs(W), 1e-30)
        out.append(np.real(np.fft.ifft2(np.abs(F) * U))[:h, :w])
    return out[0], out[1]


def slope_rms(rgb):
    """rms of the high-passed slope field -- the quantity the metric reads.

    Byte energy is the WRONG thing to match a control on: the encoding saturates,
    so a much steeper field can carry the same byte energy.  Everything is matched
    on this instead.
    """
    P, R = highpass(*slopes(rgb))
    m = MARGIN
    return float(np.sqrt(np.mean(P[m:-m, m:-m] ** 2 + R[m:-m, m:-m] ** 2)))


def supersampled(target_slope, base=None, factor=4, n=2048, beta=3.0, seed=5):
    """A height-derived normal map baked at `factor` x and filtered down.

    THE REFUTER.  Averaging unit normals and renormalising is not a linear
    operation, so a sheet baked fine and downsampled is not exactly the gradient
    of any height field at the coarse resolution -- even though a height field is
    all that ever went into it.  If that alone reads like vanilla, the verdict
    is void.  The field's amplitude is bisected until its high-frequency energy
    at 512 matches the tile being explained.
    """
    if base is not None:
        n = base[0].shape[0] * factor      # the refuter follows the tile's size
    h, kk = fractal_height(n, beta, seed)
    P, R = exact_slopes(h, kk)
    P /= P.std()
    R /= R.std()
    m = n // factor
    # the control must be as steep as the tile it explains and no steeper: it
    # rides on the tile's OWN low-frequency slope, so only the residual differs
    if base is not None:
        bP = boxmean(np.repeat(np.repeat(base[0], factor, 0), factor, 1), 4 * BLUR + 1)
        bR = boxmean(np.repeat(np.repeat(base[1], factor, 0), factor, 1), 4 * BLUR + 1)
        P, R = P - boxmean(P, 4 * BLUR + 1), R - boxmean(R, 4 * BLUR + 1)
    else:
        bP = bR = 0.0

    def bake(a):
        nv = np.stack([-(bP + P * a), np.ones_like(P), bR + R * a], axis=2)
        nv /= np.linalg.norm(nv, axis=2, keepdims=True)
        nv = nv.reshape(m, factor, m, factor, 3).mean(axis=(1, 3))
        nv /= np.linalg.norm(nv, axis=2, keepdims=True)
        return bc_roundtrip(np.clip(np.rint((nv * 0.5 + 0.5) * 255.0), 0, 255)
                            .astype(np.uint8))

    lo, hi = 1e-3, 8.0
    for _ in range(16):
        mid = 0.5 * (lo + hi)
        if slope_rms(bake(mid)) < target_slope:
            lo = mid
        else:
            hi = mid
    a = 0.5 * (lo + hi)
    rgb = bake(a)
    # the same field with NO supersampling, as this control's own paired floor
    P2 = (bP + P * a).reshape(m, factor, m, factor).mean(axis=(1, 3))
    R2 = (bR + R * a).reshape(m, factor, m, factor).mean(axis=(1, 3))
    flat = bc_roundtrip(encode(P2, R2))
    return rgb, flat, slope_rms(rgb), hf_energy(rgb)


# ---------------------------------------------------------------- reporting

def measure(rgb):
    P, R = highpass(*slopes(rgb))
    ca, ci = curl_ratio(P, R)
    return dict(curl=ca, curl_in=ci, ni=nonint(P, R))


def hf_energy(rgb):
    a = np.asarray(rgb, dtype=np.float64)
    return float(np.mean([np.abs(a[:, :, c] - boxmean(a[:, :, c], BLUR)).mean()
                          for c in range(3)]))


HDR = '%-26s %10s %10s %10s' % ('field', 'CURL', 'CURL(in)', 'NON-INT')


def line(name, m):
    return '%-26s %10.4f %10.4f %10.4f' % (name, m['curl'], m['curl_in'], m['ni'])


# ---------------------------------------------------------------- main

def main(argv):
    args = argv[1:]
    mip = 0
    if args and args[0].startswith('--mip='):
        mip = int(args.pop(0).split('=')[1])
    tiles = args or TILES
    print('msn_curl.py -- is the _msn high-frequency residual integrable?')
    print('mip %d, blur %d, margin %d, up floor %.2f, gate ceiling/floor >= %.1fx'
          % (mip, BLUR, MARGIN, UP_FLOOR, GATE))
    print()

    # -- 0. does the metric answer a question we already know the answer to? --
    print('=== 0. the metric on fields whose answer is known ===')
    rng = np.random.default_rng(11)
    v_, u_ = np.mgrid[0:256, 0:256].astype(np.float64)
    hgt = np.zeros((256, 256))
    P0 = np.zeros((256, 256))
    R0 = np.zeros((256, 256))
    for _ in range(60):
        per = rng.uniform(4.0, 40.0)
        th, ph = rng.uniform(0, 2 * np.pi), rng.uniform(0, 2 * np.pi)
        amp = rng.uniform(0.05, 0.4)
        ku, kv = 2 * np.pi / per * np.cos(th), 2 * np.pi / per * np.sin(th)
        hgt += amp * np.cos(ku * u_ + kv * v_ + ph)
        t = -amp * np.sin(ku * u_ + kv * v_ + ph)
        P0 += t * ku
        R0 += t * kv
    hp0 = highpass(P0, R0)
    print(HDR)
    print(line('exact gradient (float)',
               dict(curl=curl_ratio(*hp0)[0], curl_in=curl_ratio(*hp0)[1],
                    ni=nonint(*hp0))))
    sw = (hp0[1], hp0[0])
    print(line('the same, components swapped',
               dict(curl=curl_ratio(*sw)[0], curl_in=curl_ratio(*sw)[1],
                    ni=nonint(*sw))))
    nz = (rng.normal(size=P0.shape), rng.normal(size=P0.shape))
    hpn = highpass(*nz)
    print(line('white noise',
               dict(curl=curl_ratio(*hpn)[0], curl_in=curl_ratio(*hpn)[1],
                    ni=nonint(*hpn))))
    print('    (a gradient must read near 0 on both; noise near 1 on CURL and'
          ' near 0.5 on NON-INT)')
    print()

    ok_all = True
    verdicts = []
    for tile in tiles:
        vd = DDS(os.path.join(VAN, tile + '_msn.DDS'))
        op = os.path.join(OURS, tile + '_msn.DDS')
        od = DDS(op) if os.path.exists(op) else None
        vr = decode_rgb(vd, mip)
        orr = decode_rgb(od, mip) if od is not None else None
        print('=== %s   mip %d   %dx%d   vanilla %s / ours %s ==='
              % (tile, mip, vr.shape[1], vr.shape[0], vd.fmt,
                 od.fmt if od is not None else 'not generated'))
        ev = hf_energy(vr)
        eo = hf_energy(orr) if orr is not None else float('nan')
        print('    high-frequency energy (mean |channel - blur9|, bytes): '
              'vanilla %.2f  ours %.2f  ratio %.2fx' % (ev, eo, ev / eo))
        vup = vr[:, :, 1].astype(np.float64) / 255.0 * 2 - 1
        print('    n_up min %.3f, %d of %d texels below the %.2f divisor floor'
              % (vup.min(), int((vup < UP_FLOOR).sum()), vup.size, UP_FLOOR))

        # the codec model, checked on vanilla's own texels before it is trusted
        rt = bc_roundtrip(vr)
        err = float(np.sqrt(np.mean((rt.astype(np.float64) - vr) ** 2)))
        m_rt = measure(rt)
        m_van = measure(vr)
        print('    codec model: re-encoding vanilla adds %.2f bytes rms and moves'
              ' CURL %.4f -> %.4f' % (err, m_van['curl'], m_rt['curl']))

        # controls, built from vanilla's own field
        Pv, Rv = slopes(vr)
        lo = (boxmean(Pv, BLUR), boxmean(Rv, BLUR))
        hi = (Pv - lo[0], Rv - lo[1])
        cf, so, _ = helmholtz(*hi)
        scale = np.sqrt((hi[0] ** 2 + hi[1] ** 2).mean()
                        / max((cf[0] ** 2 + cf[1] ** 2).mean(), 1e-30))
        s2 = np.sqrt((hi[0] ** 2 + hi[1] ** 2).mean()
                     / max((so[0] ** 2 + so[1] ** 2).mean(), 1e-30))
        rp = helmholtz(*randphase(*hi))[0]
        s3 = np.sqrt((hi[0] ** 2 + hi[1] ** 2).mean()
                     / max((rp[0] ** 2 + rp[1] ** 2).mean(), 1e-30))
        rp_rgb = bc_roundtrip(encode(lo[0] + rp[0] * s3, lo[1] + rp[1] * s3))
        int_rgb = bc_roundtrip(encode(lo[0] + cf[0] * scale, lo[1] + cf[1] * scale))
        sol_rgb = bc_roundtrip(encode(lo[0] + so[0] * s2, lo[1] + so[1] * s2))

        rows = [('INT-CTRL  floor', measure(int_rgb)),
                ('PHASE-CTRL floor (indep)', measure(rp_rgb)),
                ('SOL-CTRL  ceiling', measure(sol_rgb)),
                ('VANILLA', m_van),
                ('VAN-SWAP', None)]
        if orr is not None:
            rows.append(('OURS shipped (fyi)', measure(orr)))
        Ph, Rh = highpass(*slopes(vr))
        rows[4] = ('VAN-SWAP', dict(curl=curl_ratio(Rh, Ph)[0],
                                    curl_in=curl_ratio(Rh, Ph)[1],
                                    ni=nonint(Rh, Ph)))
        print(HDR)
        for nm, m in rows:
            print(line(nm, m))

        fl, ce, ind = rows[0][1], rows[2][1], rows[1][1]
        van = m_van
        tv = {}
        for key, label in (('curl', 'CURL'), ('curl_in', 'CURL(in)'), ('ni', 'NON-INT')):
            sep = ce[key] / fl[key] if fl[key] else float('inf')
            pos = (van[key] - fl[key]) / (ce[key] - fl[key]) if ce[key] != fl[key] else float('nan')
            good = sep >= GATE
            ok_all = ok_all and good
            print('    %-9s floor %.4f  ceiling %.4f  separation %5.2fx %s   '
                  'vanilla %.4f = %+.0f%% of the way up'
                  % (label, fl[key], ce[key], sep, 'PASS' if good else 'FAIL',
                     van[key], 100 * pos))
            tv[label] = (fl[key], ce[key], van[key], sep, pos)
            if label == 'NON-INT':
                print('    %-9s independent floor (same spectrum, random '
                      'structure) %.4f -> vanilla is %.2fx it, and %.2fx the '
                      'paired floor' % ('', ind[key], van[key] / ind[key],
                                        van[key] / fl[key]))
        verdicts.append((tile, tv))

        # where in the spectrum the non-integrable share sits
        print('    NON-INT by period (texels), and each band\'s share of the '
              'residual energy:')
        bv = nonint_bands(*highpass(*slopes(vr)))
        bf = nonint_bands(*highpass(*slopes(int_rgb)))
        bc = nonint_bands(*highpass(*slopes(sol_rgb)))
        tot = sum(x[1] for x in bv)
        print('      %-12s %8s %8s %8s %8s' %
              ('period', 'energy%', 'floor', 'VANILLA', 'ceiling'))
        for (a, b), v, f, c in zip(BANDS, bv, bf, bc):
            nm = '>%g' % a if b > 1e8 else '%g - %g' % (a, b)
            print('      %-12s %7.1f%% %8.4f %8.4f %8.4f'
                  % (nm, 100 * v[1] / tot, f[0], v[0], c[0]))

        # how much of the residual has no height behind it
        #   NON-INT(vanilla) = (1-f) * floor + f * 0.5
        # taking the non-gradient component to be isotropic, which puts half its
        # energy perpendicular to k.  A component that is ITSELF partly
        # integrable -- a tiled material normal map baked from height, say --
        # would need a larger f to reach the same reading, so this is a LOWER
        # bound on the share that did not come from this sheet's own height.
        f = (van['ni'] - fl['ni']) / (0.5 - fl['ni'])
        print('    mixture: %.0f%% of the residual energy has no height field'
              ' behind it (lower bound)' % (100 * f))

        # what the floor is actually made of
        f_float = nonint(*highpass(lo[0] + cf[0] * scale, lo[1] + cf[1] * scale))
        f_8bit = measure(encode(lo[0] + cf[0] * scale, lo[1] + cf[1] * scale))['ni']
        print('    the floor, step by step: float %.4f -> 8-bit %.4f -> block '
              'codec %.4f' % (f_float, f_8bit, fl['ni']))
        print('    (a second generation of block noise on vanilla itself, +%.2f'
              ' bytes rms, moves NON-INT %.4f -> %.4f)'
              % (err, van['ni'], m_rt['ni']))

        # THE REFUTER: a height field baked at 4x and filtered down
        sv = slope_rms(vr)
        print('    slope residual rms: vanilla %.4f  floor %.4f  ceiling %.4f'
              '  ours %.4f' % (sv, slope_rms(int_rgb), slope_rms(sol_rgb),
                               slope_rms(orr) if orr is not None else float('nan')))
        ss, ssflat, sss, sse = supersampled(sv, base=lo)
        m_ss, m_sf = measure(ss), measure(ssflat)
        print('    refuter: an unrelated height field matched on slope residual'
              ' (%.4f vs %.4f; hf %.2f bytes vs %.2f), baked 4x and filtered to'
              ' 512, reads NON-INT %.4f; the SAME field with no supersampling'
              ' reads %.4f' % (sss, sv, sse, ev, m_ss['ni'], m_sf['ni']))

        # the verdict must not rest on the channel convention alone
        alt = vr[:, :, [0, 2, 1]]
        print('    sanity: with UP read as BLUE instead of GREEN, vanilla NON-INT'
              ' = %.4f (green %.4f)' % (measure(alt)['ni'], van['ni']))
        print()

    print('GATE: %s' % ('at least one metric separated the controls by %.1fx' % GATE
                        if ok_all else 'see per-metric PASS/FAIL above'))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
