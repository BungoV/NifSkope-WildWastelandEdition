"""GRADE1 gate G1 -- the known-answer controls, run BEFORE any verdict.

Three things have to be true before a fitted number means anything:

  C1  the registration control: our sheet and vanilla's are the SAME texels.
      Identity must beat all six flips/rotations on mean absolute difference,
      and the cross-correlation peak must be within one texel of (0,0).
  C2  a SYNTHETIC GAMMA is recovered to 3 decimals.  A tile is built from
      vanilla's own sheet by v -> 255*(v/255)^g0 with g0 known, and the fitter
      is asked for g.  The fit runs on the inverse direction the real fit
      uses, so the number reported is the same quantity.
  C3  a SYNTHETIC GAIN is recovered to 3 decimals, the same way.
  C4  the codec floor: our own sheet re-encoded to BC1 and decoded gives the
      residual RMS that block quantisation alone costs.  No model can beat it.
  C5  the fitter must FAIL when it should: a synthetic gamma fed to the gain
      fitter must leave a residual far above C4's floor, and a synthetic gain
      fed to the gamma fitter must return g = 1.000.

Usage: python g0_controls.py
"""

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gradelib as G                                       # noqa: E402
from splatlib import bc1_roundtrip                         # noqa: E402

TILES = [(-20, 24), (-20, 20)]
out = {'checks': 0, 'fails': 0, 'rows': []}


def chk(name, cond, detail):
    out['checks'] += 1
    if not cond:
        out['fails'] += 1
    print('%-4s %-46s %s' % ('ok' if cond else 'FAIL', name, detail))
    out['rows'].append({'name': name, 'pass': bool(cond), 'detail': detail})


def flips(a):
    return {
        'identity': a,
        'flipud': a[::-1],
        'fliplr': a[:, ::-1],
        'rot180': a[::-1, ::-1],
        'transpose': a.T,
        'rot90': np.rot90(a),
        'rot270': np.rot90(a, 3),
    }


def xcorr_peak(a, b, r=8):
    """argmax of the normalised cross-correlation over +-r texels."""
    a = a - a.mean()
    b = b - b.mean()
    best, bd = -2.0, (99, 99)
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            sa = a[max(0, dy):a.shape[0] + min(0, dy),
                   max(0, dx):a.shape[1] + min(0, dx)]
            sb = b[max(0, -dy):b.shape[0] + min(0, -dy),
                   max(0, -dx):b.shape[1] + min(0, -dx)]
            d = np.sqrt((sa * sa).sum() * (sb * sb).sum())
            c = float((sa * sb).sum() / d) if d > 0 else 0.0
            if c > best:
                best, bd = c, (dx, dy)
    return bd, best


for cx, cy in TILES:
    tag = '(%d,%d)' % (cx, cy)
    o = G.rgb(G.ours('def', cx, cy))
    v = G.rgb(G.van(cx, cy))
    chk('C0 shapes %s' % tag, o.shape == v.shape == (512, 512, 3),
        'ours %s vanilla %s' % (o.shape, v.shape))

    lo, lv = G.lum(o), G.lum(v)
    # --- C1 registration
    maes = {k: float(np.mean(np.abs(f - lv))) for k, f in flips(lo).items()}
    best = min(maes, key=maes.get)
    chk('C1a identity beats every flip %s' % tag, best == 'identity',
        'identity %.3f, next %s %.3f' % (
            maes['identity'], sorted(maes, key=maes.get)[1],
            sorted(maes.values())[1]))
    (dx, dy), pk = xcorr_peak(lo, lv)
    chk('C1b cross-correlation peak at (0,0) %s' % tag,
        abs(dx) <= 1 and abs(dy) <= 1,
        'peak (%d,%d) r=%.4f' % (dx, dy, pk))

    # --- C4 the codec floor, measured before any model is graded
    rt = bc1_roundtrip(o)
    floor = G.resid(G.lum(rt), lo)
    chk('C4 codec floor measured %s' % tag, floor['rms'] > 0,
        'BC1 re-encode of our own sheet: RMS %.3f MAE %.3f'
        % (floor['rms'], floor['mae']))

    # --- C2 synthetic gamma, recovered from the SAME fitter the lane uses
    for g0 in (1.250, 0.800, 1.350):
        syn = 255.0 * (lo / 255.0) ** g0
        p = G.fit_gamma(lo, syn)
        chk('C2 gamma %.3f recovered %s' % (g0, tag),
            abs(p['g'] - g0) < 5e-4 and abs(p['a'] - 1.0) < 5e-4,
            'g=%.6f a=%.6f dropped=%d' % (p['g'], p['a'], p['dropped']))

    # --- C3 synthetic gain
    for k0 in (0.820, 1.200, 0.950):
        syn = lo * k0
        p = G.fit_gain(lo, syn)
        pa = G.fit_affine(lo, syn)
        chk('C3 gain %.3f recovered %s' % (k0, tag),
            abs(p['k'] - k0) < 5e-4,
            'k=%.6f  (affine k=%.6f c=%.6f)' % (p['k'], pa['k'], pa['c']))

    # --- C5 the fitters must fail on the wrong model
    syn = 255.0 * (lo / 255.0) ** 1.35
    pg = G.fit_gain(lo, syn)
    rg = G.resid(G.apply_gain(lo, pg), syn)
    pgam = G.fit_gamma(lo, syn)
    rgam = G.resid(G.apply_gamma(lo, pgam), syn)
    chk('C5a gain fitter cannot absorb a gamma %s' % tag,
        rg['rms'] > 10.0 * rgam['rms'] and rg['rms'] > floor['rms'],
        'gain residual RMS %.3f vs gamma %.4f (codec floor %.3f)'
        % (rg['rms'], rgam['rms'], floor['rms']))
    syn2 = lo * 0.82
    p2 = G.fit_gamma(lo, syn2)
    chk('C5b gamma fitter returns g=1.000 on a pure gain %s' % tag,
        abs(p2['g'] - 1.0) < 5e-4 and abs(p2['a'] - 0.82) < 5e-4,
        'g=%.6f a=%.6f' % (p2['g'], p2['a']))

    # --- the sRGB slip candidates have no free parameter; show what they do
    se = G.resid(G.apply_slip_encode(lo), lo)
    sd = G.resid(G.apply_slip_decode(lo), lo)
    print('     slip sizes on %s: encode moves the sheet by MAE %.2f, '
          'decode by %.2f' % (tag, se['mae'], sd['mae']))
    out.setdefault('slipsize', {})[tag] = {'encode': se, 'decode': sd}
    out.setdefault('floor', {})[tag] = floor

print('\nG1 CONTROLS: %d checks, %d failures -> %s'
      % (out['checks'], out['fails'], 'PASS' if out['fails'] == 0 else 'FAIL'))
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       'g0_controls.json'), 'w') as f:
    json.dump(out, f, indent=1)
sys.exit(1 if out['fails'] else 0)
