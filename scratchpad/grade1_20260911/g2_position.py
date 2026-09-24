"""GRADE1 section 1b/2 -- is the tone difference a CURVE at all?

g1_curve.py found the two brief tiles disagree in SIGN (ours is brighter than
vanilla on (-20,24) and darker on (-20,20)), and that the affine/gamma fits win
only by predicting vanilla's MEAN (their slope is ~0.02..0.21, and our texel
luminance barely correlates with vanilla's at all).  So before naming a cause
this script asks the questions that separate a TONE error from a CONTENT error:

  A  multi-scale: box-average both sheets at 1,2,4,8,16,32,64,128 texels and
     report the correlation and the fitted gain at each scale.  A tone error is
     a clean gain at EVERY scale; a content error only agrees at the coarsest.
  B  per-cell: the chunk is 4x4 land cells (128 texels each).  A tone error
     gives the same k in all 16; a content error gives a spread.
  C  cross-tile: the fitted whole-sheet k on six tiles.  One number or six?
  D  the VCLR experiment: our bake multiplies by VCLR/255.  Divide it back out
     and refit.  If the two tiles then agree on one k, the fingerprint is "we
     apply VCLR and vanilla does not, plus a constant exposure".
  E  the residual-vs-field correlations redone on the GAIN model's residual
     (g1 used the affine winner, whose residual is -vanilla by construction,
     so those correlations described vanilla, not us -- stated as a mistake).

Usage: python g2_position.py
"""

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gradelib as G                                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
BRIEF = [(-20, 24), (-20, 20)]
EXTRA = [(-24, 24), (-16, 24), (-20, 28), (-16, 20)]
out = {}


def box(a, n):
    if n == 1:
        return a
    h, w = a.shape
    return a[:h // n * n, :w // n * n].reshape(h // n, n, w // n, n).mean((1, 3))


print('reading Fallout4.esm ...')
esm, heights = G.read_lands(G.ESM)

# ------------------------------------------------------------------ C + D
print('\n--- C cross-tile gain, whole sheet, all texels (no road/cover'
      ' exclusion: neither tile carries either)')
print('%-12s %8s %8s %8s %8s %8s %8s %8s' % (
    'tile', 'ours', 'vanilla', 'k', 'rms', 'VCLRmean', 'k_noVCLR', 'rms_nV'))
rows = {}
for cx, cy in BRIEF + EXTRA:
    tag = '(%d,%d)' % (cx, cy)
    o = G.rgb(G.ours('def', cx, cy))
    v = G.rgb(G.van(cx, cy))
    lo, lv = G.lum(o), G.lum(v)
    p = G.fit_gain(lo, lv)
    r = G.resid(G.apply_gain(lo, p), lv)
    vc, vhave = G.vclr_field(cx, cy, esm)
    # undo the multiply our bake applies: colour *= VCLR/255
    opre = o / np.maximum(vc / 255.0, 1e-3)
    lp = G.lum(opre)
    p2 = G.fit_gain(lp, lv)
    r2 = G.resid(G.apply_gain(lp, p2), lv)
    print('%-12s %8.3f %8.3f %8.4f %8.3f %8.2f %8.4f %8.3f' % (
        tag, lo.mean(), lv.mean(), p['k'], r['rms'], G.lum(vc).mean(),
        p2['k'], r2['rms']))
    rows[tag] = {'ours': float(lo.mean()), 'van': float(lv.mean()),
                 'k': p['k'], 'rms': r['rms'],
                 'vclr_mean': float(G.lum(vc).mean()),
                 'vclr_cov': float(vhave.mean()),
                 'k_noVCLR': p2['k'], 'rms_noVCLR': r2['rms'],
                 'ours_pre': float(lp.mean()),
                 'sat_ours': float(G.saturation(o).mean()),
                 'sat_pre': float(G.saturation(opre).mean()),
                 'sat_van': float(G.saturation(v).mean())}
ks = np.array([rows[t]['k'] for t in rows])
kp = np.array([rows[t]['k_noVCLR'] for t in rows])
print('  k          mean %.4f  sd %.4f  min %.4f  max %.4f'
      % (ks.mean(), ks.std(), ks.min(), ks.max()))
print('  k_noVCLR   mean %.4f  sd %.4f  min %.4f  max %.4f'
      % (kp.mean(), kp.std(), kp.min(), kp.max()))
print('  saturation: ours %.4f  ours-without-VCLR %.4f  vanilla %.4f'
      % (np.mean([rows[t]['sat_ours'] for t in rows]),
         np.mean([rows[t]['sat_pre'] for t in rows]),
         np.mean([rows[t]['sat_van'] for t in rows])))
out['cross_tile'] = rows

# ------------------------------------------------------------------ A + B + E
for cx, cy in BRIEF:
    tag = '(%d,%d)' % (cx, cy)
    o = G.rgb(G.ours('def', cx, cy))
    v = G.rgb(G.van(cx, cy))
    lo, lv = G.lum(o), G.lum(v)
    row = {}
    print('\n--- A multi-scale %s   (box average n x n texels)' % tag)
    print('    %6s %8s %8s %8s %8s' % ('n', 'r', 'k', 'rms', 'sd(van)'))
    for n in (1, 2, 4, 8, 16, 32, 64, 128):
        a, b = box(lo, n), box(lv, n)
        p = G.fit_gain(a, b)
        r = G.resid(G.apply_gain(a, p), b)
        print('    %6d %+8.4f %8.4f %8.3f %8.3f'
              % (n, G.pearson(a, b), p['k'], r['rms'], b.std()))
        row.setdefault('scales', {})[n] = {'r': G.pearson(a, b), 'k': p['k'],
                                           'rms': r['rms'],
                                           'sd_van': float(b.std())}

    print('--- B per-cell %s   (16 land cells, 128x128 texels each)' % tag)
    kk = []
    for gy in range(4):
        line = []
        for gx in range(4):
            # sheet row 0 is NORTH: cell (cx+gx, cy+3-gy) occupies block gy,gx
            sl = (slice(gy * 128, gy * 128 + 128), slice(gx * 128, gx * 128 + 128))
            p = G.fit_gain(lo[sl], lv[sl])
            kk.append(p['k'])
            line.append('%5.3f(%5.1f/%5.1f)' % (p['k'], lo[sl].mean(),
                                                lv[sl].mean()))
        print('      ' + '  '.join(line))
    kk = np.array(kk)
    print('      per-cell k: mean %.4f sd %.4f min %.4f max %.4f'
          % (kk.mean(), kk.std(), kk.min(), kk.max()))
    row['cell_k'] = {'mean': float(kk.mean()), 'sd': float(kk.std()),
                     'min': float(kk.min()), 'max': float(kk.max()),
                     'all': [float(x) for x in kk]}

    print('--- E residual of the GAIN model vs the fields %s' % tag)
    p = G.fit_gain(lo, lv)
    rmap = G.apply_gain(lo, p) - lv
    dmap = lo - lv
    hf, hh = G.height_field(cx, cy, heights)
    sl = G.slope_from_msn(G.ours('def', cx, cy, '_msn'))
    ao = G.rgb(G.ours('def', cx, cy, '_data'))[:, :, 0]
    vc, vhave = G.vclr_field(cx, cy, esm)
    m = np.ones(lo.shape, bool)
    fields = {'height': hf, 'slope': sl, 'AO(_data R)': ao,
              'VCLR lum': G.lum(vc), 'ours lum': lo, 'vanilla lum': lv}
    print('    %-12s %18s %18s' % ('field', 'r(gain resid)', 'r(ours-vanilla)'))
    for i, (nm, f) in enumerate(fields.items()):
        c1 = G.corr_with_floor(rmap, f, m, seed=31 + i)
        c2 = G.corr_with_floor(dmap, f, m, seed=31 + i)
        print('    %-12s %+8.4f (fl %+0.4f) %+8.4f (fl %+0.4f)'
              % (nm, c1['r'], c1['floor'], c2['r'], c2['floor']))
        row.setdefault('corr', {})[nm] = {'gain_resid': c1, 'diff': c2}
    out[tag] = row

with open(os.path.join(HERE, 'g2_position.json'), 'w') as f:
    json.dump(out, f, indent=1, default=float)
print('\nwrote g2_position.json')
