"""GRADE1 section 1 -- the tone transfer curve ours -> vanilla, measured.

Runs only after g0_controls.py is green.  For each tile:

  * the ROAD MASK is derived, not assumed: the shipped default bake (which
    HAS roads -- `roads 1` in its own census; `--roads` is not opt-in, see
    MISTAKES) differenced against a `--no-roads` bake.  Texels that move are
    the road footprint (plus BC1 block bleed, hence the stated threshold).
  * the COVER plane comes from our `_data` alpha when the sheet is DXT5+WWCV.
  * the first pass fits on GROUND texels only: not road, cover == 0.
  * three models are fitted (gain, affine, gamma) plus the two fixed sRGB
    slips which have no free parameter, per channel and on luminance.
  * the winner's residual map is correlated against height, slope, AO and
    VCLR, each beside a phase twin.
  * saturation is measured the same way: a scalar gain leaves HSV S exactly
    alone, a gamma raises it -- so S is a discriminator, not decoration.

Usage: python g1_curve.py
"""

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gradelib as G                                        # noqa: E402
from splatlib import bc1_roundtrip                          # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TILES = [(-20, 24), (-20, 20)]
ROAD_T = 2.0                    # levels; below this is codec noise

print('reading Fallout4.esm for VCLR and VHGT ...')
esm, heights = G.read_lands(G.ESM)
print('  cells with VCLR %d, with VHGT %d' % (
    sum(1 for v in esm.lands.values() if v.get('vclr')), len(heights)))

res = {}
for cx, cy in TILES:
    tag = '(%d,%d)' % (cx, cy)
    o = G.rgb(G.ours('def', cx, cy))
    v = G.rgb(G.van(cx, cy))
    orr = G.rgb(G.ours('noroads', cx, cy))

    # ---- road mask from the differenced bake
    dmax = np.abs(orr - o).max(2)
    road = dmax > ROAD_T
    cover, have_cover = G.cover_plane(G.ours('def', cx, cy, '_data'))
    ground = (~road) & (cover <= 0.0)

    print('\n=== %s  road texels %d (%.2f%%), cover>0 %d (%.2f%%)%s, '
          'ground %d (%.2f%%)' % (
              tag, int(road.sum()), 100.0 * road.mean(),
              int((cover > 0).sum()), 100.0 * (cover > 0).mean(),
              '' if have_cover else '  [NO cover plane in this sheet]',
              int(ground.sum()), 100.0 * ground.mean()))

    floor = G.resid(G.lum(bc1_roundtrip(o)), G.lum(o))
    print('    BC1 codec floor on this sheet: RMS %.3f MAE %.3f'
          % (floor['rms'], floor['mae']))

    row = {'road_n': int(road.sum()), 'cover_n': int((cover > 0).sum()),
           'have_cover': bool(have_cover), 'ground_n': int(ground.sum()),
           'floor': floor, 'fits': {}}

    # ---- the fits, on luminance and per channel
    for label, (oo, vv) in [('lum', (G.lum(o), G.lum(v)))] + [
            (c, (o[:, :, i], v[:, :, i])) for i, c in enumerate('RGB')]:
        a, b = oo[ground], vv[ground]
        fits = {}
        pk = G.fit_gain(a, b)
        fits['gain'] = (pk, G.resid(G.apply_gain(a, pk), b))
        pa = G.fit_affine(a, b)
        fits['affine'] = (pa, G.resid(G.apply_affine(a, pa), b))
        pg = G.fit_gamma(a, b)
        fits['gamma'] = (pg, G.resid(G.apply_gamma(a, pg), b))
        fits['slip_encode'] = ({}, G.resid(G.apply_slip_encode(a), b))
        fits['slip_decode'] = ({}, G.resid(G.apply_slip_decode(a), b))
        fits['identity'] = ({}, G.resid(a, b))
        print('    %-4s ours mean %7.3f  vanilla mean %7.3f'
              % (label, a.mean(), b.mean()))
        for k in ('identity', 'gain', 'affine', 'gamma',
                  'slip_encode', 'slip_decode'):
            p, r = fits[k]
            ps = ' '.join('%s=%.4f' % (kk, vvv) for kk, vvv in sorted(p.items())
                          if kk in ('k', 'c', 'a', 'g'))
            print('        %-12s rms %7.3f  mae %7.3f  bias %+8.3f  %s'
                  % (k, r['rms'], r['mae'], r['bias'], ps))
        row['fits'][label] = {k: {'p': fits[k][0], 'r': fits[k][1]}
                              for k in fits}

    # ---- saturation, the gamma/gain discriminator
    so, sv = G.saturation(o), G.saturation(v)
    kl = row['fits']['lum']['gain']['p']['k']
    s_gain = G.saturation(o * kl)
    gp = row['fits']['lum']['gamma']['p']
    s_gam = G.saturation(G.apply_gamma(o, gp))
    print('    saturation on ground:  ours %.4f  vanilla %.4f'
          '   gain-predicted %.4f   gamma-predicted %.4f'
          % (so[ground].mean(), sv[ground].mean(),
             s_gain[ground].mean(), s_gam[ground].mean()))
    row['sat'] = {'ours': float(so[ground].mean()),
                  'van': float(sv[ground].mean()),
                  'gain_pred': float(s_gain[ground].mean()),
                  'gamma_pred': float(s_gam[ground].mean())}

    # ---- the winner's residual map, correlated against the fields
    best = min(('gain', 'affine', 'gamma'),
               key=lambda k: row['fits']['lum'][k]['r']['rms'])
    pw = row['fits']['lum'][best]['p']
    pred = {'gain': G.apply_gain, 'affine': G.apply_affine,
            'gamma': G.apply_gamma}[best](G.lum(o), pw)
    rmap = (pred - G.lum(v)).astype(np.float64)
    hf, hh = G.height_field(cx, cy, heights)
    sl = G.slope_from_msn(G.ours('def', cx, cy, '_msn'))
    ao = G.rgb(G.ours('def', cx, cy, '_data'))[:, :, 0]
    vc, vhave = G.vclr_field(cx, cy, esm)
    vcl = G.lum(vc)
    fields = {'height': hf, 'slope': sl, 'AO(_data R)': ao,
              'VCLR lum': vcl, 'VCLR R': vc[:, :, 0], 'VCLR G': vc[:, :, 1],
              'VCLR B': vc[:, :, 2], 'ours lum': G.lum(o)}
    print('    winning global model: %s; residual map correlations on ground'
          ' (r beside its phase-twin floor)' % best)
    row['best'] = best
    row['corr'] = {}
    for i, (name, f) in enumerate(fields.items()):
        c = G.corr_with_floor(rmap, f, ground, seed=11 + i)
        print('        %-12s r %+0.4f   twin floor %+0.4f'
              % (name, c['r'], c['floor']))
        row['corr'][name] = c
    print('    VCLR coverage: %.1f%% of texels sit in a cell WITH a VCLR record'
          % (100.0 * vhave.mean()))
    row['vclr_cov'] = float(vhave.mean())
    # the natural experiment: cells with vs without a VCLR record
    for nm, m in (('with VCLR', ground & vhave), ('no VCLR', ground & ~vhave)):
        if m.sum() < 100:
            print('        %-10s n=%d -- too few to fit' % (nm, int(m.sum())))
            row.setdefault('vclr_split', {})[nm] = {'n': int(m.sum())}
            continue
        p = G.fit_gain(G.lum(o)[m], G.lum(v)[m])
        r = G.resid(G.apply_gain(G.lum(o)[m], p), G.lum(v)[m])
        print('        %-10s n=%-7d k=%.4f  rms %.3f'
              % (nm, int(m.sum()), p['k'], r['rms']))
        row.setdefault('vclr_split', {})[nm] = {'n': int(m.sum()),
                                                'k': p['k'], 'r': r}
    res[tag] = row

with open(os.path.join(HERE, 'g1_curve.json'), 'w') as f:
    json.dump(res, f, indent=1, default=float)
print('\nwrote g1_curve.json')
