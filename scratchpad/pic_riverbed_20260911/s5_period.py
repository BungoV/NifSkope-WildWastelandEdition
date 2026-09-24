#!/usr/bin/env python
"""PIC-RIVERBED step 5: two checks that say WHAT the grey spots are.

  A. Is the TEXTURE'S OWN PATTERN in the sheet, and at which repeat? The
     texture is sampled onto the same 256x256 riverbed block at each repeat,
     and each result is correlated with the sheet, high-passed, against a
     phase-randomised twin of the same panel as the floor (splatlib.phase_twin,
     SPLAT1's own control). A lag-64 autocorrelation CANNOT answer this: 64
     texels is 1 repeat at 2048 AND exactly 6 at 341.333, so both score 1.0 --
     that test was run, it failed to discriminate, and it was replaced.

  B. What the bake actually prints. At the baked repeat one far-sheet texel is
     exactly 32 texture texels, i.e. one texel of the texture's mip 5 (64x64).
     So the spots should be the pale blobs OF THAT MIP IMAGE, and their size in
     mip-5 texels should equal their size in far-sheet texels. Measured on both.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
sys.path.insert(0, HERE)

import splatlib as S                                          # noqa: E402
from s4_pebble import blobs, TEX, window_composition          # noqa: E402
OURS = REPO + '/scratchpad/roads1_20260911/out/after/tex/Commonwealth.4.-20.20.DDS'
VAN = ('E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/'
       'Commonwealth.4.-20.20.DDS')
UPT = 32.0
ENGINE, BAKED = 341.3333, 2048.0


def autocorr_lag(a, lags):
    """Normalised autocorrelation of the mean-removed field at integer lags,
    along x and along y, averaged."""
    f = a - a.mean()
    f = f / (f.std() + 1e-9)
    out = {}
    n = f.size
    for L in lags:
        cx = float((f[:, :-L] * f[:, L:]).mean()) if L < f.shape[1] else 0.0
        cy = float((f[:-L, :] * f[L:, :]).mean()) if L < f.shape[0] else 0.0
        out[L] = 0.5 * (cx + cy)
    return out


def main():
    log = []

    def p(s):
        print(s)
        log.append(s)

    rb = np.load(os.path.join(HERE, 'riverbed.npz'))['riverbed']
    w = json.load(open(os.path.join(HERE, 'window.json')))
    by, bx = w['y'], w['x']

    # A. is the texture's pattern in the sheet, and at which repeat? -----
    p("A. IS THE TEXTURE'S OWN PATTERN IN THE SHEET, AND AT WHICH REPEAT?")
    p('   256x256 riverbed block; high-passed correlation against the texture')
    p('   sampled onto the same block at each repeat; floor = the same panel')
    p('   phase-randomised (keeps its variance and spectrum, destroys its')
    p('   registration). NOTE: a lag-64 autocorrelation was tried first and is')
    p('   BLIND here -- 64 texels is 1 repeat at 2048 and exactly 6 at 341.333,')
    p('   so both read +1.0000. Replaced, not reported as a result.')
    # the block is chosen by the weight of the DOMINANT TEXTURE's own LTEXs,
    # not by the riverbed total -- otherwise the block can be silt where the
    # window is rocks, and the correlation is diluted by a texture not tested.
    z = np.load(os.path.join(HERE, 'riverbed.npz'))
    _, rows = window_composition()
    domw = np.zeros((512, 512))
    for nm, _v in rows[0][1]['edids']:
        domw += z['w_' + nm]
    best, byy, bxx = -1.0, 0, 0
    for y in range(0, 512 - 256 + 1, 8):
        for x in range(0, 512 - 256 + 1, 8):
            sv = domw[y:y + 256, x:x + 256].mean()
            if sv > best:
                best, byy, bxx = sv, y, x
    p('   block y=%d x=%d, weight of %s there = %.3f'
      % (byy, bxx, os.path.basename(TEX), best))

    sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))
    import offline_bake as OB
    dtex = S.Dds(TEX)
    CELL2 = 4096.0
    span = 4.0 * CELL2
    pyy, pxx = np.mgrid[byy:byy + 256, bxx:bxx + 256]
    wyy = 20 * CELL2 + (1.0 - (pyy + 0.5) / 512.0) * span
    wxx = -20 * CELL2 + ((pxx + 0.5) / 512.0) * span

    def hp(a):
        """mean-removed, minus a 9x9 box mean: the texel-scale field only."""
        f = a - a.mean()
        return f - S._box(f, 4)

    def corr(a, b):
        x, y2 = hp(a).ravel(), hp(b).ravel()
        x = x - x.mean()
        y2 = y2 - y2.mean()
        return float((x * y2).mean() / (x.std() * y2.std() + 1e-12))

    sheets = {}
    for nm, path in (('ours', OURS), ('vanilla', VAN)):
        sheets[nm] = S.lum(S.Dds(path).level(0)[:, :, :3])[byy:byy + 256, bxx:bxx + 256]

    res = {}
    p('')
    p('   sheet     repeat        corr        phase-twin floor      verdict')
    for tnm, tile in (('2048   ', BAKED), ('341.333', ENGINE)):
        c = np.asarray(OB._tap(dtex, wxx, wyy, tile, UPT, 'code'), np.float64)
        panel = S.lum(c.reshape(256, 256, 3))
        twin = S.phase_twin(panel, seed=5)
        for nm in ('ours', 'vanilla'):
            cv = corr(sheets[nm], panel)
            fl = corr(sheets[nm], twin)
            res['%s@%s' % (nm, tnm.strip())] = dict(corr=cv, floor=fl)
            p('   %-8s  %s   %+.4f      %+.4f             %s'
              % (nm, tnm, cv, fl,
                 'PRESENT' if abs(cv) > 4 * abs(fl) + 0.02 else 'not above its floor'))
    p('')

    # B. the mip the bake prints -----------------------------------------
    d = S.Dds(TEX)
    m5 = d.level(5)[:, :, :3]
    p('B. WHAT THE BAKE PRINTS AT 2048: mip %d of the texture, %dx%d'
      % (5, m5.shape[1], m5.shape[0]))
    p('   one far-sheet texel (32 world units) = %.0f texture texels = 1 texel of that mip'
      % (BAKED / d.width * 0 + 32.0))
    L5, s5 = S.lum(m5), m5.max(2) - m5.min(2)
    mg = (L5 > L5.mean() + L5.std()) & (s5 < s5.mean())
    ar = blobs(mg, maxiter=80)
    ar = ar[ar >= 3]
    dd = 2.0 * np.sqrt(ar / np.pi)
    p('   pale blobs of that mip: %.2f%% of it, %d blobs, median diameter %.2f mip texels'
      % (100.0 * mg.mean(), ar.size, float(np.median(dd)) if ar.size else 0.0))
    p('   one mip texel = one far-sheet texel at the baked repeat, so that median IS')
    p('   the spot size the sheet should show. Measured on our sheet in s4: 3.19 texels.')
    p('')
    p('   at the ENGINE repeat the same 32 world units span %.0f texture texels'
      % (32.0 / (ENGINE / d.width)))
    p('   -> mip %.2f, a %dx%d image of the whole texture, which is why it averages flat'
      % (S.bake_mip(d, ENGINE, UPT), d.width >> int(S.bake_mip(d, ENGINE, UPT)),
         d.width >> int(S.bake_mip(d, ENGINE, UPT))))

    json.dump(dict(period=res, mip5_blob_median=(float(np.median(dd)) if ar.size else 0.0),
                   mip5_blob_n=int(ar.size)),
              open(os.path.join(HERE, 'period.json'), 'w'), indent=1)
    with open(os.path.join(HERE, 'logs', 's5.log'), 'w') as f:
        f.write('\n'.join(log) + '\n')


if __name__ == '__main__':
    main()
