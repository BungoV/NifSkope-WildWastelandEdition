#!/usr/bin/env python3
"""FARVIEW1b twin 2: bulb dots on the skyline. docs/cloud/FARVIEW1b_DESIGN.md section 5.

  python3 tests/spells/farview1b_dots_check.py            # green + every red; exit 0 only when green passes
                                                          # every check AND every red fails the check it targets
  FARVIEW1B_DOTS_RED=<name> python3 tests/spells/farview1b_dots_check.py   # one red alone; exit 1 when it fails

A small rasterizer, numpy only, nothing from the game. A camera looks at one lamp from distance d
(1920 x 1080, 70 degrees across, so f = 1371 px; a parameter). The lamp is drawn two ways:
  * the REAL bulb: a 6-unit disc of even brightness, pixel coverage from 32 x 32 samples a pixel (what the
    game's own mesh gives with its anti-aliasing; only drawn while its cell is loaded)
  * the DOT: a round Gaussian sprite at the light's position, sampled at pixel centers, whose total is
    normalized analytically (never by its own sampled sum, which would hide twinkle). Its size and brightness:
        rho   = 6 f / d                                the bulb's radius on screen, px
        sigma = max(SIGMA_MIN, sqrt(rho^2 / 4 + 1/12))  same second moment as the bulb's pixel footprint
        total = m x I x (D1 / d)^2 x T_fog(d) x fade(d)   the bulb's own energy (inverse square), m = the
                                                        record's intensity estimate / the real bulb (0.6..1.6)
  * the HAND-OFF: the FARVIEW1 band, w = smoothstep(3072, 7168, d): real x (1 - w) + dot x w
  * the FAR FADE: fade = 1 - smoothstep(0.7 d_max, d_max, d), d_max = where the dot's peak pixel falls to half a
    display code value (0.5 / 255) for that light's intensity
Nine lamps: intensity 0.25 / 1 / 4 x mismatch 0.6 / 1.0 / 1.6. Sixteen sub-pixel camera offsets per distance.

Checks:
  T  twinkle    at every distance the drawn energy varies <= 2% across the 16 sub-pixel offsets
  H  hand-off   through the band (2560..7680 u by 32 u) the energy over the physical target changes <= 1% a step,
                and never leaves [min(1, m), max(1, m)] by more than 0.02 (no double draw)
  S  size       the drawn rms radius changes <= 3% a step over the whole sweep (2560..250000 u)
  M  match      at D0 and at D1 the dot alone and the real bulb alone have the same rms radius within 10%, so the
                hand-off is a brightness fade, not a change of shape (added after the first run, see the doc)
  F  far fade   at the step before a dot is dropped its peak pixel is <= 0.5 / 255 (no pop at the cull)
  O  occlusion  an occluder edge swept across the dot by 1/8 px: visibility (4 x 4 depth taps over the sprite)
                changes <= 0.15 a step and never rises while the edge covers more
Reds (each must FAIL its targets):
  nohandoff  real bulb at full until the band edge D1, dot only past it                      -> H
  bothdrawn  the dot fades in but the real bulb is not faded out (drawn until D1)           -> H
  nofloor    physical size only (sigma = rho / 2, no minimum): sub-pixel dots twinkle       -> T
  bigdot     a fixed 4 px glow sprite instead of the bulb-matched size                       -> M
  intsize    the sprite's diameter (4 sigma) rounded to whole pixels, as integer point sizes -> S
  fixedcull  every dot dropped at 20000 u, no fade                                           -> F
  onetap     occlusion from one depth tap at the dot's center                                -> O
"""
import math
import os
import sys
import time

import numpy as np

try:
    import resource  # Unix: peak RSS from getrusage (KB on Linux)

    def peak_mb():
        return peak_mb()
except ImportError:  # Windows (lane FARVIEW1): no 'resource'; the peak working set from the process' own counters
    import ctypes
    import ctypes.wintypes as _wt

    class _PMC(ctypes.Structure):
        _fields_ = [('cb', _wt.DWORD), ('PageFaultCount', _wt.DWORD), ('PeakWorkingSetSize', ctypes.c_size_t),
                    ('WorkingSetSize', ctypes.c_size_t), ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaPagedPoolUsage', ctypes.c_size_t), ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                    ('QuotaNonPagedPoolUsage', ctypes.c_size_t), ('PagefileUsage', ctypes.c_size_t),
                    ('PeakPagefileUsage', ctypes.c_size_t)]

    def peak_mb():
        c = _PMC()
        c.cb = ctypes.sizeof(c)
        k32 = ctypes.windll.kernel32
        k32.GetCurrentProcess.restype = ctypes.c_void_p   # the pseudo-handle -1: an int return would truncate it
        k32.K32GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(_PMC), _wt.DWORD]
        if not k32.K32GetProcessMemoryInfo(k32.GetCurrentProcess(), ctypes.byref(c), c.cb):
            return 0.0
        return c.PeakWorkingSetSize / (1024 * 1024)

# ---------------------------------------------------------------- the camera, the lamp, the band (parameters)
F_PX = 960.0 / math.tan(math.radians(35.0))   # 1920 px across 70 degrees
R_BULB = 6.0                                  # bulb radius, units
D0, D1 = 3072.0, 7168.0                       # FARVIEW1's band (design doc FARVIEW1 section 4)
D_START, D_END = 2560.0, 250000.0
STEP_BAND = 32.0                              # camera step in and around the band (as FARVIEW1 check C)
STEP_FAR = 0.01                               # beyond: 1% of the distance a step
THETA = 0.5 / 255                             # half a display code value at the night exposure
FOG = dict(near=0.0, far=120000.0, power=1.0, max=0.8)   # a made-up night weather, PRTP2 section 5 form
INTENSITIES = (0.25, 1.0, 4.0)
MISMATCH = (0.6, 1.0, 1.6)
SS = 32                                       # real-bulb coverage samples per pixel axis (16 in the first run:
                                              # its coverage noise alone moved H by about 0.5% a step)
JIT = [((i + 0.5) / 4, (j + 0.5) / 4) for i in range(4) for j in range(4)]

# ---------------------------------------------------------------- pre-registered bars (set before the first run)
BAR_T = 0.02          # twinkle, (max - min) / mean over 16 offsets
SIGMA_RULE = 0.01     # SIGMA_MIN = the smallest sigma whose twinkle is <= this (half the bar), from the scan
BAR_H_STEP = 0.01     # energy / target, change a 32 u step in the band
BAR_H_OVER = 0.02     # allowed excursion outside [min(1, m), max(1, m)]
BAR_S = 0.03          # rms radius, relative change a step (the natural 1/d change is 1.25% a step at 2560 u)
BAR_M = 0.10          # dot vs bulb rms radius at D0 and D1 (added after the first run)
BAR_O = 0.15          # visibility change a 1/8 px step of the occluder edge
FIXED_CULL = 20000.0
BIGDOT_SIGMA = 4.0


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def fog_t(d):
    f = min(FOG['max'], max(0.0, d / (FOG['far'] - FOG['near']) - FOG['near'] / (FOG['far'] - FOG['near'])) ** FOG['power'])
    return 1.0 - f


def target(I, d):
    """the physical energy of the lamp on screen (pixel-value x px), I = 1 at D1 sums to 1"""
    return I * (D1 / d) ** 2 * fog_t(d)


# ---------------------------------------------------------------- raster
HALF = 16  # window +-HALF px around the center


def real_unit(d, jx, jy):
    """unit-energy image of the bulb disc, coverage from SS x SS samples a pixel; center at (jx, jy) in pixel 0"""
    rho = R_BULB * F_PX / d
    n = int(math.ceil(rho)) + 2
    px = np.arange(-n, n + 1)
    s = (np.arange(SS) + 0.5) / SS
    xs = (px[:, None] + s[None]).ravel() - jx
    ys = (px[:, None] + s[None]).ravel() - jy
    inside = (xs[:, None] ** 2 + ys[None, :] ** 2) <= rho * rho
    cov = inside.reshape(len(px), SS, len(px), SS).mean((1, 3))
    area = math.pi * rho * rho
    img = np.zeros((2 * HALF + 1, 2 * HALF + 1))
    img[HALF - n:HALF + n + 1, HALF - n:HALF + n + 1] = cov / area
    return img


def dot_sigma(d, red):
    rho = R_BULB * F_PX / d
    if red == 'nofloor':
        return rho / 2
    if red == 'bigdot':
        return BIGDOT_SIGMA
    s = max(SIGMA_MIN, math.sqrt(rho * rho / 4 + 1.0 / 12))
    if red == 'intsize':
        s = max(1.0, round(4 * s)) / 4
    return s


def dot_unit(sigma, jx, jy):
    """Gaussian at pixel centers, normalized by its analytic integral (1), not by the sampled sum"""
    px = np.arange(-HALF, HALF + 1) + 0.5
    gx = np.exp(-0.5 * ((px - jx) / sigma) ** 2)
    gy = np.exp(-0.5 * ((px - jy) / sigma) ** 2)
    return np.outer(gx, gy) / (2 * math.pi * sigma * sigma)


def rms(img, jx, jy):
    px = np.arange(-HALF, HALF + 1) + 0.5
    r2 = (px[:, None] - jx) ** 2 + (px[None, :] - jy) ** 2
    return math.sqrt((img * r2).sum() / max(img.sum(), 1e-300))


def twinkle_of_sigma(sigma):
    e = [dot_unit(sigma, jx, jy).sum() for jx, jy in JIT]
    return (max(e) - min(e)) / np.mean(e)


def choose_sigma_min():
    rows = []
    chosen = None
    for s in np.arange(0.30, 0.801, 0.05):
        tw = twinkle_of_sigma(s)
        rows.append((s, tw))
        if chosen is None and tw <= SIGMA_RULE:
            chosen = round(float(s), 2)
    return chosen, rows


SIGMA_MIN, SIGMA_SCAN = choose_sigma_min()


_DMAX = {}


def d_max_of(I):
    if I not in _DMAX:
        _DMAX[I] = _d_max(I)
    return _DMAX[I]


def _d_max(I):
    """where the dot's peak pixel (SIGMA_MIN sprite, fog, no fade) falls to THETA; bisection on log d"""
    peak = lambda d: target(I, d) / (2 * math.pi * SIGMA_MIN ** 2)
    lo, hi = D1, 1e7
    if peak(lo) <= THETA:
        return lo
    for _ in range(80):
        mid = math.sqrt(lo * hi)
        lo, hi = (mid, hi) if peak(mid) > THETA else (lo, mid)
    return lo


def distances():
    a = list(np.arange(D_START, D1 + 512 + 1, STEP_BAND))
    d = a[-1]
    while d < D_END:
        d *= 1 + STEP_FAR
        a.append(d)
    return np.array(a)


def frame(I, m, d, red, jx, jy, cache):
    """the drawn image of one lamp at camera distance d, sub-pixel offset (jx, jy)"""
    w = float(smoothstep(D0, D1, d))
    if red in ('nohandoff', 'bothdrawn'):
        real_w = 1.0 if d < D1 else 0.0
        dot_w = (1.0 if d >= D1 else 0.0) if red == 'nohandoff' else w
    else:
        real_w, dot_w = 1.0 - w, w
    if red == 'fixedcull':
        fade = 1.0 if d < FIXED_CULL else 0.0
    else:
        dm = d_max_of(m * I)          # the dot knows only its record's intensity
        fade = 1.0 - float(smoothstep(0.7 * dm, dm, d))
    img = np.zeros((2 * HALF + 1, 2 * HALF + 1))
    tgt = target(I, d)
    if real_w > 0:
        k = ('r', d, jx, jy)
        if k not in cache:
            cache[k] = real_unit(d, jx, jy)
        img += real_w * tgt * cache[k]   # the real bulb is not faded by fog-free rules: target already has the fog
    if dot_w > 0 and fade > 0:
        s = dot_sigma(d, red)
        k = ('d', s, jx, jy)
        if k not in cache:
            cache[k] = dot_unit(s, jx, jy)
        img += dot_w * fade * m * tgt * cache[k]
    return img, tgt, fade


def sweep(red, cache):
    """per lamp: arrays over distance of mean energy / target, twinkle, rms radius, peak, fade"""
    ds = distances()
    out = {}
    for I in INTENSITIES:
        for m in MISMATCH:
            ratio, tw, rr, pk, fd = [], [], [], [], []
            for d in ds:
                es, rs, ps = [], [], []
                for jx, jy in JIT:
                    img, tgt, fade = frame(I, m, d, red, jx, jy, cache)
                    es.append(img.sum()); rs.append(rms(img, jx, jy) if img.sum() > 0 else float('nan'))
                    ps.append(img.max())
                es = np.array(es)
                ratio.append(es.mean() / tgt)
                tw.append((es.max() - es.min()) / es.mean() if es.mean() > 0 else (0.0 if fade == 0 else 1.0))
                rr.append(np.nanmean(rs) if np.isfinite(rs).any() else float('nan'))
                pk.append(max(ps))
                fd.append(fade)
            out[(I, m)] = dict(d=ds, ratio=np.array(ratio), tw=np.array(tw), rms=np.array(rr), peak=np.array(pk),
                               fade=np.array(fd))
    return out


def occlusion(red):
    """edge x > e covers the dot (in front); visibility = share of taps left of the edge; sweep e by 1/8 px"""
    s = SIGMA_MIN
    if red == 'onetap':
        taps = np.array([0.0])
    else:
        # 4 x 4 taps over +-1.5 sigma on a rotated ('n-rooks') grid: all 16 have distinct x and distinct y, so an
        # edge from any side uncovers them one at a time (a plain 4 x 4 grid would step by 4 taps)
        k = np.array([(i * 4 + (j * 3 + i) % 4) for i in range(4) for j in range(4)])
        taps = ((k + 0.5) / 16 - 0.5) * 3.0 * s
    es = np.arange(-3.0, 3.0001, 0.125)
    vis = np.array([(taps < e).mean() for e in es])   # the edge moves right: covers less as e grows
    return es, vis


# ---------------------------------------------------------------- the checks
def run(red, cache):
    res, out = {}, []

    def row(name, ok, text):
        res[name] = bool(ok)
        out.append('%-2s %-4s %s' % (name, 'PASS' if ok else 'FAIL', text))

    S = sweep(red, cache)
    # T
    worst = max((float(v['tw'].max()), k, float(v['d'][v['tw'].argmax()])) for k, v in S.items())
    row('T', worst[0] <= BAR_T, 'twinkle over 16 sub-pixel offsets, all lamps, %d distances: worst %.4f at %.0f u '
        '(I %.2f, m %.1f) (bar %.2f)' % (len(S[(1.0, 1.0)]['d']), worst[0], worst[2], worst[1][0], worst[1][1], BAR_T))
    # H
    okH, wstep, wover, wl = True, 0.0, 0.0, None
    for (I, m), v in S.items():
        band = (v['d'] >= D0 - 512) & (v['d'] <= D1 + 512)
        r = v['ratio'][band]
        st = np.abs(np.diff(r)).max()
        lo, hi = min(1.0, m), max(1.0, m)
        ov = max(0.0, lo - r.min(), r.max() - hi)
        if st > wstep:
            wstep, wl = st, (I, m, float(v['d'][band][1:][np.abs(np.diff(r)).argmax()]))
        wover = max(wover, ov)
        okH &= st <= BAR_H_STEP and ov <= BAR_H_OVER
    row('H', okH, 'band %.0f..%.0f u by %.0f u: energy / physical target, worst change a step %.4f at %.0f u (I %.2f, '
        'm %.1f) (bar %.2f), worst excursion outside [min(1,m), max(1,m)] %.4f (bar %.2f)'
        % (D0 - 512, D1 + 512, STEP_BAND, wstep, wl[2], wl[0], wl[1], BAR_H_STEP, wover, BAR_H_OVER))
    # S
    okS, ws, wsl = True, 0.0, None
    for (I, m), v in S.items():
        r = v['rms']
        ok = np.isfinite(r[1:]) & np.isfinite(r[:-1])
        ch = np.abs(np.diff(r))[ok] / r[:-1][ok]
        if ch.max() > ws:
            ws, wsl = float(ch.max()), (I, m, float(v['d'][1:][ok][ch.argmax()]))
        okS &= ch.max() <= BAR_S
    r1 = S[(1.0, 1.0)]['rms']
    row('S', okS, 'rms radius, change a step over %.0f..%.0f u: worst %.4f at %.0f u (I %.2f, m %.1f) (bar %.2f); '
        'I 1: %.2f px at %.0f, %.2f at D0, %.2f at D1, %.2f far'
        % (D_START, D_END, ws, wsl[2], wsl[0], wsl[1], BAR_S, r1[0], D_START,
           r1[np.argmin(np.abs(S[(1.0, 1.0)]['d'] - D0))], r1[np.argmin(np.abs(S[(1.0, 1.0)]['d'] - D1))], np.nanmin(r1)))
    # M -- the dot alone against the bulb alone, at both ends of the band
    mm = []
    for d in (D0, D1):
        rr, rd = [], []
        for jx, jy in JIT:
            rr.append(rms(real_unit(d, jx, jy), jx, jy))
            rd.append(rms(dot_unit(dot_sigma(d, red), jx, jy), jx, jy))
        mm.append((d, np.mean(rr), np.mean(rd)))
    worstM = max(abs(b / a - 1) for _, a, b in mm)
    row('M', worstM <= BAR_M, 'rms radius dot / bulb: %s; worst %.3f (bar %.2f)'
        % ('; '.join('%.0f u: bulb %.3f px, dot %.3f px' % x for x in mm), worstM, BAR_M))
    # F
    okF, lines = True, []
    for (I, m), v in S.items():
        on = v['peak'] > 0
        last = np.nonzero(on)[0].max()
        cut = last + 1 < len(v['d'])
        pk = float(v['peak'][last])
        okF &= (not cut) or pk <= THETA
        if m != 1.0:
            continue
        lines.append('I %.2f: dropped after %.0f u, peak there %.5f%s' % (I, v['d'][last], pk,
                                                                          '' if cut else ' (never dropped in the sweep)'))
    worstF = max(float(v['peak'][np.nonzero(v['peak'] > 0)[0].max()]) for v in S.values()
                 if np.nonzero(v['peak'] > 0)[0].max() + 1 < len(v['d']))
    row('F', okF, 'far fade, all 9 lamps: worst peak at the step before the drop %.5f (bar %.5f = 0.5/255); m 1: %s'
        % (worstF, THETA, '; '.join(lines)))
    # O
    es, vis = occlusion(red)
    st = float(np.abs(np.diff(vis)).max())
    mono = bool(np.all(np.diff(vis) >= -1e-12))
    row('O', st <= BAR_O and mono, 'occluder edge swept %.1f..%.1f px by 1/8 px: largest visibility change a step %.3f '
        '(bar %.2f), monotone %s' % (es[0], es[-1], st, BAR_O, mono))
    return res, out


TARGET = {'nohandoff': ['H'], 'bothdrawn': ['H'], 'nofloor': ['T'], 'bigdot': ['M'], 'intsize': ['S'],
          'fixedcull': ['F'], 'onetap': ['O']}


def main():
    t_start = time.time()
    print('camera f %.1f px; bulb radius %.0f u = %.2f px at D0 %.0f, %.2f px at D1 %.0f; fog %s'
          % (F_PX, R_BULB, R_BULB * F_PX / D0, D0, R_BULB * F_PX / D1, D1, FOG))
    print('sigma scan (twinkle of a Gaussian dot sampled at pixel centers, 16 offsets): ' +
          ', '.join('%.2f: %.4f' % (s, t) for s, t in SIGMA_SCAN))
    print('SIGMA_MIN = %s px (the smallest with twinkle <= %.2f); dot rms radius floor %.2f px'
          % (SIGMA_MIN, SIGMA_RULE, SIGMA_MIN * math.sqrt(2)))
    print('d_max (peak <= 0.5/255, fog included): ' + ', '.join('I %.2f: %.0f u' % (I, d_max_of(I)) for I in INTENSITIES)
          + '; the sprite takes over from the bulb\'s own size at %.0f u' % (
              R_BULB * F_PX / (2 * math.sqrt(max(SIGMA_MIN ** 2 - 1.0 / 12, 1e-9)))))
    cache = {}
    only = os.environ.get('FARVIEW1B_DOTS_RED', '')
    if only:
        if only not in TARGET:
            raise SystemExit('unknown FARVIEW1B_DOTS_RED %r (one of %s)' % (only, ', '.join(TARGET)))
        res, out = run(only, cache)
        print('\nred %s:' % only)
        print('\n'.join('  ' + o for o in out))
        failed = [c for c in TARGET[only] if not res[c]]
        print('red %s: %s (target checks failing: %s)' % (only, 'FAILS as it must' if failed else 'DID NOT FAIL',
                                                          ', '.join(failed) or 'none'))
        sys.exit(1 if failed else 0)
    print('\ngreen:')
    res, out = run('', cache)
    print('\n'.join('  ' + o for o in out))
    green_ok = all(res.values())
    reds_ok = True
    for red, targets in TARGET.items():
        r, o = run(red, cache)
        failed = [c for c in targets if not r[c]]
        print('\nred %s (must FAIL %s):' % (red, ', '.join(targets)))
        print('\n'.join('  ' + x for x in o if x.split()[0] in targets))
        print('  -> %s' % ('FAILS as it must (%s)' % ', '.join(failed) if len(failed) == len(targets)
                           else 'DID NOT FAIL: the gate is blind'))
        reds_ok &= len(failed) == len(targets)
    verdict = green_ok and reds_ok
    print('\nFARVIEW1b dots %s: green %s, reds %s; %.0f s, peak memory %.0f MB'
          % ('PASS' if verdict else 'FAIL', 'PASS' if green_ok else 'FAIL', 'all fail' if reds_ok else 'NOT all fail',
             time.time() - t_start, peak_mb()))
    sys.exit(0 if verdict else 1)


if __name__ == '__main__':
    main()
