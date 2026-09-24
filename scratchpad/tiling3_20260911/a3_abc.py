"""TILING3 hypotheses A, B and C, on what hypothesis D left behind.

D's verdict (logs/d1_geology.txt, logs/d2_deep.txt): vanilla's fine colour detail
SHARES ITS DIRECTIONAL STRUCTURE with vanilla's own `_msn` (orientation agreement
+0.23..+0.40 on 7 of 7 sheets against twin floors of +0.014..+0.107, and our own
`_msn` at +0.015), but only 2.5 % / 3.5 % of its VARIANCE is any per-texel
function of that `_msn` (22-column ceiling basis, twin floor 0.0001, overfit floor
0.00006).  So ~97 % of the residual is still unexplained, and that is what A, B
and C are tested against here.

A  STOCHASTIC TILING -- vanilla's residual is the land textures' own grain with
   the phase broken.  Phase randomisation changes NEITHER the power spectrum NOR
   the histogram, so the two statistics that survive it are exactly the two that
   test A: the radial spectrum and the moments.  The comparator is the composite
   sampled at the mip where the texture's grain lands at the bake texel scale.
   FLOOR: white noise of the same SD (flat spectrum, skew 0, kurtosis 3).  If
   vanilla's residual is white and Gaussian it is noise, not texture.

B  A NOISE TEXTURE -- `Textures/Terrain/Noise.dds` at a searched scale, as a
   fixed-phase candidate with its own phase twin as the floor, exactly as TILING2
   correlated its ten candidates; plus its spectrum against vanilla's residual.

C  A FINER BAKE RESAMPLED -- the composite baked at 4x (2048 texels over the same
   4x4 cells) and box-downsampled to 512.  The claim under test is that this
   leaves the texture's grain but no coherent repeat; the discriminator is
   TILING2's periodicity law (vanilla's ceiling 0.264 absolute / 0.448 over the
   sheet's own null floor).

Known answers run first: the spectrum-distance instrument must read ~0 on a field
against itself and must read large on white noise against a terrain sheet; the
moment instrument must read skew 0 / kurtosis 3 on Gaussian input.

    python a3_abc.py  ->  logs/a3_abc.txt
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
import offline_bake as OB                                     # noqa: E402

TILE = 341.3333
TILES = [('t2024', -20, 24), ('t2020', -20, 20)]
NOISE = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Noise.dds'
CELL = 4096.0


def hp(a, r=2):
    return T.hp_residual(np.asarray(a, np.float64), r=r)


def moments(a):
    x = np.asarray(a, np.float64).ravel()
    x = x - x.mean()
    s = x.std()
    if s <= 0:
        return 0.0, 0.0, 0.0
    z = x / s
    return float(s), float((z ** 3).mean()), float((z ** 4).mean())


def resid_bands(a):
    """The residual's radial power in the four bands finer than 32 texels, as a
    SHARE of the residual's own variance -- so a blurry field and a sharp one can
    be compared on SHAPE regardless of amplitude."""
    ctr, pw = S.radial_power(np.asarray(a, np.float64))
    tab = S.band_table(ctr, pw)
    vals = [v for _n, v in tab]
    tot = sum(vals[2:]) or 1e-12
    return [v / tot for v in vals[2:]], [n for n, _v in tab][2:]


def shape_dist(a, b):
    """Mean |log2| ratio of the four fine-band SHARES: 0 = the same spectral
    shape, whatever the amplitude."""
    sa, _ = resid_bands(a)
    sb, _ = resid_bands(b)
    return float(np.mean([abs(np.log2(max(x, 1e-9) / max(y, 1e-9)))
                          for x, y in zip(sa, sb)]))


def box_down(a, f):
    h, w = a.shape[:2]
    H, W = h // f, w // f
    if a.ndim == 3:
        return a[:H * f, :W * f].reshape(H, f, W, f, a.shape[2]).mean((1, 3))
    return a[:H * f, :W * f].reshape(H, f, W, f).mean((1, 3))


def main():
    rng = np.random.default_rng(77)
    L = ['TILING3 -- hypotheses A, B and C on the ~97 % of vanilla`s residual that',
         'hypothesis D`s ceiling basis does NOT explain.', '']
    out = {}

    # --------------------------------------------------------- known answers first
    van24 = S.lum(S.Dds(S.van_sheet(-20, 24)).level(0))
    v24 = hp(van24)
    L.append('=' * 78)
    L.append('KNOWN-ANSWER CONTROLS')
    L.append('=' * 78)
    L.append('   shape distance, a field against ITSELF          %.4f   (must be 0)'
             % shape_dist(v24, v24))
    wn = rng.normal(0, v24.std(), v24.shape)
    L.append('   shape distance, vanilla residual vs WHITE NOISE %.4f   (must be large)'
             % shape_dist(v24, wn))
    s, sk, ku = moments(wn)
    L.append('   moments of Gaussian noise: skew %+.3f kurtosis %.3f  (must be 0 / 3)'
             % (sk, ku))
    sh = [round(x, 4) for x in resid_bands(wn)[0]]
    L.append('   band shares of white noise: %s  (the flat reference)' % sh)
    L.append('')

    for name, cx, cy in TILES:
        van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        vhp = hp(van)
        vs, vsk, vku = moments(vhp)
        vb, bn = resid_bands(vhp)
        L.append('=' * 78)
        L.append('chunk (%d,%d)' % (cx, cy))
        L.append('=' * 78)
        L.append('   band names (share of the residual`s variance): %s' % bn)
        L.append('   %-28s %7s %7s %7s   %s' % ('field', 'SD', 'skew', 'kurt', 'band shares'))
        L.append('   ' + '-' * 74)
        L.append('   %-28s %7.3f %7.3f %7.3f   %s'
                 % ('VANILLA residual', vs, vsk, vku, [round(x, 3) for x in vb]))
        wn2 = rng.normal(0, vs, vhp.shape)
        ws, wsk, wku = moments(wn2)
        L.append('   %-28s %7.3f %7.3f %7.3f   %s   <- FLOOR'
                 % ('white noise, same SD', ws, wsk, wku,
                    [round(x, 3) for x in resid_bands(wn2)[0]]))

        # -------------------------------------------------------------- A, the mips
        rows = {}
        for lab, kw in (('composite @ code mip', dict(mip='code')),
                        ('composite @ code-1', dict(mip=-1.0)),
                        ('composite @ code-2', dict(mip=-2.0)),
                        ('composite @ code-3', dict(mip=-3.0)),
                        ('composite @ code-4', dict(mip=-4.0)),
                        ('composite @ mip 0', dict(mip=-8.0))):
            sh_ = S.lum(OB.bake(cx, cy, dim=4, tile=TILE, **kw))
            r = hp(sh_)
            ss, ssk, sku = moments(r)
            bb, _ = resid_bands(r)
            vis, floor = T.tiling_visibility(sh_)
            L.append('   %-28s %7.3f %7.3f %7.3f   %s  repeat %.3f/%.3f'
                     % (lab, ss, ssk, sku, [round(x, 3) for x in bb], vis, floor))
            rows[lab] = dict(sd=ss, skew=ssk, kurt=sku, bands=bb,
                             shape=shape_dist(vhp, r), vis=vis, floor=floor)
        L.append('   shape distance of each to vanilla`s residual (0 = same spectral shape):')
        for lab, d in rows.items():
            L.append('      %-28s %.4f' % (lab, d['shape']))
        L.append('      %-28s %.4f   <- FLOOR' % ('white noise', shape_dist(vhp, wn2)))

        # ------------------------------------------------------------- B, the noise
        try:
            nd = S.Dds(NOISE)
            nl = S.lum(nd.level(0))
            L.append('   B  Noise.dds %dx%d, luminance SD %.2f' % (nl.shape[0], nl.shape[1], nl.std()))
            best = None
            for cells in (0.5, 1, 2, 4, 8, 16, 32):
                # tile the noise so one repeat spans `cells` cells of the sheet
                per = 512.0 / (4.0 / cells) if cells else 512.0
                reps = 4.0 / cells                      # repeats across the 4-cell sheet
                yy, xx = np.mgrid[0:512, 0:512].astype(np.float64)
                u = np.mod(xx / 512.0 * reps, 1.0)
                v = np.mod(yy / 512.0 * reps, 1.0)
                samp = S._bilerp_wrap(nl[:, :, None], u, v)[:, :, 0]
                r = T.corr(vhp, hp(samp))
                tw = T.corr(vhp, hp(S.phase_twin(samp, seed=9)))
                if best is None or abs(r) > abs(best[1]):
                    best = (cells, r, tw, samp)
                L.append('      noise repeat = %5.1f cells   r %+.4f   twin %+.4f'
                         % (cells, r, tw))
            # and at the landscape repeat itself
            reps = 4.0 * CELL / TILE
            yy, xx = np.mgrid[0:512, 0:512].astype(np.float64)
            samp = S._bilerp_wrap(nl[:, :, None], np.mod(xx / 512.0 * reps, 1.0),
                                  np.mod(yy / 512.0 * reps, 1.0))[:, :, 0]
            L.append('      noise at the LANDSCAPE repeat 341.3333 u   r %+.4f   twin %+.4f'
                     % (T.corr(vhp, hp(samp)), T.corr(vhp, hp(S.phase_twin(samp, seed=9)))))
            nb, _ = resid_bands(hp(nl))
            ns, nsk, nku = moments(hp(nl))
            L.append('      Noise.dds own residual: SD %.3f skew %+.3f kurt %.3f bands %s'
                     % (ns, nsk, nku, [round(x, 3) for x in nb]))
            L.append('      its shape distance to vanilla`s residual %.4f' % shape_dist(vhp, hp(nl)))
        except Exception as e:
            L.append('   B  Noise.dds unreadable: %s' % e)

        # ------------------------------------------------------------ C, 4x resample
        big = OB.bake(cx, cy, dim=4, tile=TILE, mip='code', res=2048)
        for f, lab in ((4, 'C 4x box-downsampled'),):
            small = S.lum(box_down(big, f))
            r = hp(small)
            ss, ssk, sku = moments(r)
            bb, _ = resid_bands(r)
            vis, fl = T.tiling_visibility(small)
            L.append('   %-28s %7.3f %7.3f %7.3f   %s  repeat %.3f/%.3f  shape %.4f'
                     % (lab, ss, ssk, sku, [round(x, 3) for x in bb], vis, fl,
                        shape_dist(vhp, r)))
            rows[lab] = dict(sd=ss, skew=ssk, kurt=sku, vis=vis, floor=fl,
                             shape=shape_dist(vhp, r))
        L.append('   (vanilla`s periodicity law: visibility <= 0.264 absolute, <= 0.448 over')
        L.append('    the sheet`s own null floor -- TILING2 section 1a)')
        L.append('')
        out[name] = dict(van=dict(sd=vs, skew=vsk, kurt=vku, bands=vb), rows=rows)

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'a3_abc.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'a3_abc.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
