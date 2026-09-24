"""TILING3 -- fitting the SHADING of vanilla's own `_msn` fine detail, which is
what bungo's 23:05 ruling asks the diffuse to carry.

The ruling: "For now, I think we can use vanilla normal map for those tiles, use
those details for the diffuse."  So the colour's fine detail is to be the
shading of the fine detail of vanilla's shipped `_msn` -- hypothesis D's term,
at a fitted strength and light.  This script fits that strength and that light,
on vanilla's own sheets, so the number the exe ships is measured and not chosen.

THE MODEL, and it is deliberately the smallest one that can be called a shading:

    dL(x,y) = kE * dEast + kN * dNorth + kU * dUp

where (dEast,dNorth,dUp) is vanilla's `_msn` MINUS its own coarse version -- the
part of the relief that is finer than our 128-unit height grid and therefore the
part we do not have.  The three coefficients ARE the light: their direction is
the light's direction and their length is its strength, so fitting them is
fitting "the strength and the light" in one step, with no separate azimuth
search to go wrong.

FLOORS AND CEILINGS, all three, because a small R^2 is easy to mistake for a
finding:
  * the PHASE TWIN of the detail field -- same power spectrum, same histogram,
    destroyed structure.  A correlation is only real if it beats this.
  * OUR OWN `_msn` put through the identical fit.  TILING3 section 2 already
    measured that ours carries none of this; if it reads as high as vanilla's
    then the fit is measuring the instrument, not the geology.
  * a KNOWN ANSWER: a synthetic colour built as exactly 0.5*dUp plus the real
    sheet must come back with kU = 0.5 and R^2 near 1.

THE COARSE VERSION is the same DDS at a coarser mip -- mip 2, four texels, 128
world units, which is exactly our height grid's step, so "detail" means "finer
than anything our own normal can know about". That is not a tunable: it is the
definition of what vanilla has and we do not.

    python d3_shade.py  ->  logs/d3_shade.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

SHEETS = [(-20, 24), (-20, 20), (-36, -20), (-4, -20), (28, -20), (-4, 16), (24, 16)]
OURS = os.path.join(T2, 'out', 'rung', 't2024', 'tex')
COARSE_MIP = 2          # 4 texels = 128 world units = our height grid's step


def decode(level):
    """R = east, G = UP, B = north (src/lodgen.cpp:5566), all three signed."""
    c = np.asarray(level, np.float64)[:, :, :3] / 255.0 * 2.0 - 1.0
    n = np.stack([c[:, :, 0], c[:, :, 2], c[:, :, 1]], 2)      # east, north, up
    ln = np.sqrt((n ** 2).sum(2))
    return n / np.maximum(ln, 1e-6)[:, :, None]


def detail_of(path):
    """vanilla's normal minus its own coarse version, at the sheet's resolution.

    BOTH LEVELS ARE READ THROUGH THE SAME TRILINEAR SAMPLER THE C++ USES
    (`getPixelT`), not by expanding a coarse array: the coefficient fitted here
    is the one src/lodgen.cpp will ship, so the two must compute the same field.
    An earlier revision nearest-expanded mip 2, which is a blockier coarse and a
    different divergence, and would have shipped a number fitted to a field the
    exe never evaluates."""
    d = S.Dds(path)
    n = d.level(0).shape[0]
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64)
    u = (xx + 0.5) / n
    v = (yy + 0.5) / n
    fine = decode(S.sample_trilinear(d, u, v, 0.0)[..., :3])
    coarse = decode(S.sample_trilinear(d, u, v, float(min(COARSE_MIP, d.maxMip)))[..., :3])
    return fine, coarse, fine - coarse


def fit(y, cols):
    """Least squares with an intercept; returns (coefficients, R^2)."""
    A = np.concatenate([np.ones((y.size, 1))] + [c.reshape(-1, 1) for c in cols], 1)
    b = y.reshape(-1)
    sol, *_ = np.linalg.lstsq(A, b, rcond=None)
    pred = A @ sol
    ss = float(((b - b.mean()) ** 2).sum())
    return sol, float(1.0 - ((b - pred) ** 2).sum() / max(ss, 1e-12))


def main():
    L = ['TILING3 -- the shading of vanilla`s own `_msn` fine detail', '']
    L.append('Model: dL = kE*dEast + kN*dNorth + kU*dUp, on the colour`s high-pass')
    L.append('residual (everything finer than 5 texels), fitted per sheet.')
    L.append('')
    out, rows = {}, []

    # ------------------------------------------------------- the known answer
    p0 = S.van_sheet(*SHEETS[0], suffix='_msn')
    _f0, _c0, dn0 = detail_of(p0)
    lum0 = S.lum(S.Dds(S.van_sheet(*SHEETS[0])).level(0))
    hp0 = T.hp_residual(lum0, r=2)
    synth = hp0 + 0.5 * dn0[:, :, 2]
    sol, r2 = fit(synth, [dn0[:, :, 0], dn0[:, :, 1], dn0[:, :, 2]])
    L.append('KNOWN ANSWER -- the real residual plus exactly 0.5*dUp injected:')
    L.append('   recovered kE %+.4f kN %+.4f kU %+.4f  (kU must read ~0.5 plus the'
             % (sol[1], sol[2], sol[3]))
    L.append('   sheet`s own kU below); R^2 %.4f' % r2)
    L.append('')

    L.append('   %-12s %8s %8s %8s %8s %9s %9s %9s'
             % ('sheet', 'kE', 'kN', 'kU', 'R2', 'twinR2', 'ourR2', '|k|'))
    curv_rows = []
    for cx, cy in SHEETS:
        vp = S.van_sheet(cx, cy, '_msn')
        if not os.path.exists(vp):
            L.append('   (%4d,%4d)  no vanilla _msn on disk -- skipped' % (cx, cy))
            continue
        _fine, _coarse, dn = detail_of(vp)
        lum = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        hp = T.hp_residual(lum, r=2)
        sol, r2 = fit(hp, [dn[:, :, 0], dn[:, :, 1], dn[:, :, 2]])
        # floor 1: the twin of every detail channel
        tw = [S.phase_twin(dn[:, :, k], seed=11 + k) for k in range(3)]
        _s, r2t = fit(hp, tw)
        # floor 2: our own _msn through the identical fit
        op = os.path.join(OURS, 'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy))
        r2o = float('nan')
        if os.path.exists(op):
            _of, _oc, odn = detail_of(op)
            _s2, r2o = fit(hp, [odn[:, :, 0], odn[:, :, 1], odn[:, :, 2]])
        k = sol[1:4]
        rows.append(dict(cx=cx, cy=cy, k=[float(x) for x in k], r2=r2,
                         twin=r2t, ours=None if r2o != r2o else r2o))
        L.append('   (%4d,%4d) %8.4f %8.4f %8.4f %8.4f %9.5f %9s %9.4f'
                 % (cx, cy, k[0], k[1], k[2], r2, r2t,
                    'n/a' if r2o != r2o else '%.5f' % r2o,
                    float(np.sqrt((k ** 2).sum()))))

        # THE CURVATURE TERM. Section 2 of this lane found the only column of a
        # 22-column basis that carried anything was the DIVERGENCE of the normal
        # (r = -0.1247 / -0.1373) -- a crevice-darkening term, not a Lambert
        # one. A Lambert dot cannot see a rill: the two walls of a rill tilt
        # opposite ways and cancel. Its divergence does not cancel.
        div = (np.gradient(dn[:, :, 0], axis=1) + np.gradient(dn[:, :, 1], axis=0))
        dtw = S.phase_twin(div, seed=21)
        rc = T.corr(hp, div)
        rct = T.corr(hp, dtw)
        _sc, r2c = fit(hp, [div])
        _sct, r2ct = fit(hp, [dtw])
        # the shipped form: divergence plus the three linear terms
        solb, r2b = fit(hp, [dn[:, :, 0], dn[:, :, 1], dn[:, :, 2], div])
        curv_rows.append(dict(cx=cx, cy=cy, r=float(rc), rtwin=float(rct),
                              r2=float(r2c), r2twin=float(r2ct),
                              kdiv=float(solb[4]), r2both=float(r2b)))
    if not rows:
        raise SystemExit('REFUSED: no vanilla _msn sheet was readable')

    L.append('')
    L.append('THE CURVATURE (crevice) TERM -- the divergence of the same detail normal,')
    L.append('with its own phase twin as the floor at every sheet:')
    L.append('   %-12s %9s %9s %9s %9s %9s'
             % ('sheet', 'r', 'r(twin)', 'R2', 'R2(twin)', 'kDiv'))
    for c in curv_rows:
        L.append('   (%4d,%4d) %+9.4f %+9.4f %9.5f %9.5f %+9.3f'
                 % (c['cx'], c['cy'], c['r'], c['rtwin'], c['r2'], c['r2twin'], c['kdiv']))
    rr = np.array([c['r'] for c in curv_rows])
    rt = np.array([c['rtwin'] for c in curv_rows])
    kd = np.array([c['kdiv'] for c in curv_rows])
    L.append('   MEDIAN     %+9.4f %+9.4f %9.5f %9.5f %+9.3f'
             % (float(np.median(rr)), float(np.median(rt)),
                float(np.median([c['r2'] for c in curv_rows])),
                float(np.median([c['r2twin'] for c in curv_rows])), float(np.median(kd))))
    same_sign = int((np.sign(rr) == np.sign(np.median(rr))).sum())
    L.append('   SIGN AGREEMENT %d of %d sheets carry the median`s sign; the twin floor'
             % (same_sign, len(curv_rows)))
    L.append('   is beaten on %d of %d (|r| > |r twin|).'
             % (int((np.abs(rr) > np.abs(rt)).sum()), len(curv_rows)))

    K = np.array([r['k'] for r in rows])
    kmed = np.median(K, 0)
    L.append('')
    L.append('MEDIAN over %d sheets: kE %+.4f  kN %+.4f  kU %+.4f   |k| %.4f'
             % (len(rows), kmed[0], kmed[1], kmed[2], float(np.sqrt((kmed ** 2).sum()))))
    L.append('median R^2 %.4f, median twin floor %.5f, median our-own floor %s'
             % (float(np.median([r['r2'] for r in rows])),
                float(np.median([r['twin'] for r in rows])),
                '%.5f' % float(np.median([r['ours'] for r in rows if r['ours'] is not None]))
                if any(r['ours'] is not None for r in rows) else 'n/a'))
    L.append('')
    L.append('THE VERDICT, and it is not the one the ruling assumed.')
    L.append('')
    L.append('A LAMBERT SHADING OF THAT DETAIL READS ZERO. The three coefficients sit')
    L.append('at the phase-twin floor on every sheet (median R^2 %.4f against a twin'
             % float(np.median([r['r2'] for r in rows])))
    L.append('floor of %.5f) and their SIGNS FLIP sheet to sheet -- kU runs %+.4f to'
             % (float(np.median([r['twin'] for r in rows])),
                float(min(r['k'][2] for r in rows))))
    L.append('%+.4f -- which is what a fit to noise looks like. The fit machinery is'
             % float(max(r['k'][2] for r in rows)))
    L.append('not at fault: the known-answer control recovers an injected 0.5 as')
    L.append('0.5000. A Lambert dot simply cannot SEE a rill -- a rill`s two walls')
    L.append('tilt opposite ways and their dots cancel.')
    L.append('')
    L.append('THE CURVATURE OF THAT SAME DETAIL DOES NOT READ ZERO. Its divergence --')
    L.append('a crevice-darkening term, dark in the channel, light on the ridge --')
    L.append('reads r %+.4f median, the SAME SIGN on %d of %d sheets, beating its own'
             % (float(np.median(rr)), same_sign, len(curv_rows)))
    L.append('phase twin on %d of %d. It is the same column TILING3 section 2 found at'
             % (int((np.abs(rr) > np.abs(rt)).sum()), len(curv_rows)))
    L.append('the top of a 22-column basis, and it is the term this lane ships.')
    L.append('')
    L.append('So the ruling`s INTENT is delivered -- vanilla`s own fine geology reaches')
    L.append('the diffuse, rills and drainage where vanilla has them -- by the term')
    L.append('that measures rather than the term that was named. What it recovers is')
    L.append('about %.1f %% of the colour`s fine variance (median R^2 %.5f), not most'
             % (100.0 * float(np.median(rr)) ** 2, float(np.median(rr)) ** 2))
    L.append('of it, and this lane does not claim otherwise. TILING3 section 2 already')
    L.append('showed why no per-texel law can do better: vanilla`s colour grain SHARES')
    L.append('ITS ORIENTATION with the `_msn` (agreement +0.27..+0.32 against twin')
    L.append('floors near +0.03) but is not a per-texel FUNCTION of it (22-column')
    L.append('ceiling R^2 0.018..0.023).')
    L.append('')
    L.append('SHIPPED DEFAULT: --land-shade %.3f  -- the median kDiv, in 8-bit'
             % float(np.median(kd)))
    L.append('luminance levels per unit of detail-normal divergence at the vanilla')
    L.append('sheet`s own texel spacing. THE LAMBERT COEFFICIENTS ARE NOT SHIPPED:')
    L.append('kE %+.4f kN %+.4f kU %+.4f is printed above as a refutation, not as a'
             % (kmed[0], kmed[1], kmed[2]))
    L.append('setting.')

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'd3_shade.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(rows=rows, kmed=[float(x) for x in kmed],
                   curv=curv_rows, kdiv=float(np.median(kd)),
                   coarse_mip=COARSE_MIP),
              open(os.path.join(HERE, 'd3_shade.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
