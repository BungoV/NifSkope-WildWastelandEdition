"""TILING2 -- the instruments on inputs whose answer is known, BEFORE any sheet.

`ww-control-calibration` part 1: a metric that cannot fail on its input is not a
metric. Every check here has a floor on the other side.

    python t1_selftest.py        ->  logs/t1_selftest.txt, exit 0 on PASS
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

P = 341.3333 / 32.0
OUT = []
nOk = nBad = 0


def chk(name, cond, note):
    global nOk, nBad
    if cond:
        nOk += 1
    else:
        nBad += 1
    OUT.append('%-4s %-52s %s' % ('ok' if cond else 'FAIL', name, note))


def main():
    rng = np.random.default_rng(11)
    smooth = S.smooth_field(512, 512, cells=32, seed=3, amp=20.0, mean=90.0)

    # ---- (a) periodicity -------------------------------------------------
    cal = []
    for A in (1.0, 3.0, 7.0):
        for ax in ('x', 'y', 'xy'):
            g = A * np.cos(2 * np.pi * (np.mgrid[0:512, 0:512][1] if ax == 'x'
                                        else np.mgrid[0:512, 0:512][0] if ax == 'y'
                                        else np.mgrid[0:512, 0:512].sum(0)) / P)
            v, _f = T.tiling_visibility(g + 90.0, P)
            cal.append((A, ax, v, abs(v - A) / A))
    worst = max(c[3] for c in cal)
    chk('A0 a cosine of known amplitude reads its own amplitude',
        worst < 0.06, 'worst error %.1f%% over 9 cases; e.g. ' % (100 * worst)
        + '  '.join('%s A=%.0f->%.2f' % (c[1], c[0], c[2]) for c in cal[:3]))

    v0, f0, d0 = T.tiling_visibility(smooth, P, full=True)
    chk('A1 smooth field has no repeat at 10.667',
        v0 < f0, 'visibility %.5f   null floor %.5f' % (v0, f0))

    prev = -9.0
    mono = True
    rows = []
    first = None
    for amp in (0.5, 1.0, 2.0, 4.0, 8.0):
        v, f = T.tiling_visibility(T.inject_repeat(smooth, P, amp), P)
        rows.append((amp, v, f))
        if v < prev:
            mono = False
        prev = v
        if first is None and v > f:
            first = amp
    chk('A2 injected repeat: visibility rises with amplitude', mono,
        '  '.join('%.1f:%.3f' % (a, v) for a, v, _f in rows))
    chk('A3 the instrument DETECTS an injected repeat', first is not None,
        'first amplitude above its own floor: %s of 255 (field SD %.1f)'
        % (first, smooth.std()))

    inj = T.inject_repeat(smooth, P, 4.0)
    vI, fI = T.tiling_visibility(inj, P)
    vN, fN = T.tiling_visibility(T.notch_repeat(inj, P), P)
    chk('A4 notching the repeat out kills the reading',
        vN < fN and vI > fI, 'with %.4f (floor %.4f) -> notched %.4f (floor %.4f)'
        % (vI, fI, vN, fN))

    vT, fT = T.tiling_visibility(S.phase_twin(inj, seed=5), P)
    chk('A5 THE PHASE TWIN IS NOT A FLOOR FOR THIS STATISTIC',
        vT > 0.5 * vI, 'subject %.3f   twin %.3f   = %.0f%% of it. The twin keeps '
        'the amplitude spectrum, so it keeps the repeat.' % (vI, vT, 100.0 * vT / vI))

    # ---- (b) edge width --------------------------------------------------
    meas = []
    for w in (1.0, 2.0, 4.0, 8.0):
        a = np.zeros((512, 512))
        x = np.arange(512, dtype=np.float64)
        for k in range(8):
            c = 64 * k + 32
            a += 60.0 * np.clip((x - (c - w / 2)) / w, 0, 1)[None, :] * (-1) ** k
        a = a - a.min() + 40.0
        a = a + rng.standard_normal(a.shape) * 0.05
        wid, idx, g = T.edge_widths(a)
        med = float(np.median(wid))
        meas.append((w, med))
        exp = max(0.8 * w, 1.0)
        chk('B%d a %.0f-texel linear ramp reads 0.8x its width' % (1 + int(np.log2(w)), w),
            abs(med - exp) <= 1.0,
            'expected %.2f (0.8w, floored at the instrument`s 1-texel resolution)'
            '   measured p50 %.2f over %d edges' % (exp, med, wid.size))

    a = np.zeros((512, 512))
    a[:, 256:] = 60.0
    a = a + 40.0 + rng.standard_normal(a.shape) * 0.05
    wid, idx, g = T.edge_widths(a)
    chk('B5 a HARD step reads at or under one texel',
        float(np.median(wid)) <= 1.0,
        'p50 %.2f over %d edges' % (float(np.median(wid)), wid.size))

    # the quadrant-hit chance baseline
    idxr = rng.choice(512 * 512, 4000, replace=False)
    on, n, chance = T.quadrant_hits(idxr)
    chk('B6 quadrant-hit chance baseline is right',
        abs(on / float(n) - chance) < 0.02,
        'random texels %d of %d = %.3f   chance %.3f' % (on, n, on / float(n), chance))

    yy, xx = np.mgrid[0:512, 0:512]
    onq = np.flatnonzero(((xx % 64) == 0))
    on, n, chance = T.quadrant_hits(rng.choice(onq, 4000, replace=False))
    chk('B7 and it sees a border-locked set', on == n,
        '%d of %d on a quadrant border, chance %.3f' % (on, n, chance))

    # ---- (c) spectrum ----------------------------------------------------
    chk('C1 a sheet against itself has zero spectrum distance',
        T.spectrum_distance(smooth, smooth) < 1e-9,
        '%.2e' % T.spectrum_distance(smooth, smooth))
    noisy = smooth + rng.standard_normal(smooth.shape) * 6.0
    prevd = -1.0
    ds = []
    okm = True
    for r in (1, 2, 3):
        d = T.spectrum_distance(S._box(noisy, r), noisy)
        ds.append(d)
        if d < prevd:
            okm = False
        prevd = d
    chk('C2 blur raises the spectrum distance, monotonically', okm and ds[0] > 0.1,
        '  '.join('r%d:%.3f' % (r, d) for r, d in zip((1, 2, 3), ds)))
    add = float(S.local_var(noisy).mean() - S.local_var(smooth).mean())
    chk('C3 local variance reads white noise at its ANALYTIC value',
        abs(add - 36.0 * 8.0 / 9.0) < 3.0,
        'SD 6 noise must add sigma^2*8/9 = %.1f; measured %.2f (smooth %.2f -> %.2f)'
        % (36.0 * 8.0 / 9.0, add, S.local_var(smooth).mean(), S.local_var(noisy).mean()))

    OUT.append('')
    OUT.append('%d checks, %d failures -- %s' % (nOk + nBad, nBad,
                                                 'PASS' if nBad == 0 else 'FAIL'))
    txt = '\n'.join(OUT) + '\n'
    print(txt)
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    with open(os.path.join(HERE, 'logs', 't1_selftest.txt'), 'w', newline='\n') as f:
        f.write(txt)
    return 1 if nBad else 0


if __name__ == '__main__':
    sys.exit(main())
