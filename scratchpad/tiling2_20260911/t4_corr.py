"""TILING2 work item 1c -- where does vanilla's fine detail COME FROM?

The question the brief asks: our sheet is blurrier than vanilla's. Vanilla's
high-pass residual (everything finer than 5 texels = 160 world units) is a real
field with real structure. Is that structure something the bake has a SOURCE
for, or is it a noise/lighting term we cannot reproduce?

Every candidate is correlated against vanilla's high-pass residual, on both
tiles, and every correlation has THE PHASE TWIN beside it as its floor. This is
the one place the twin IS a valid floor: it is a structure statistic (one field
against another), it keeps the candidate's entire power spectrum, and it
destroys only the alignment. A candidate only counts if it beats its own twin.

Candidates, all on the fixed tiling (341.3333):
  land@code      the land textures sampled the way the generator does now
  land@code-1/-2 one and two mips finer -- MORE texture detail
  land@code+1    one mip coarser
  land@box       the exact box mean of mip 0 over the bake texel's footprint
  land@mean      the texture's global mean: NO texture detail at all
  vclr           the 33x33 vertex-colour field alone
  slope          the shipped _msn normal: 1 - nz, i.e. how steep the ground is
  shading        dot(n, l) for the best of 8 light azimuths at 45 deg elevation
  ours(rung)     our own shipped-path bake, as a reference point

    python t4_corr.py  ->  logs/t4_corr.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
import offline_bake as OB                                     # noqa: E402

TILE = 341.3333
TILES = [('t2024', -20, 24), ('t2020', -20, 20)]


def hp(a):
    return T.hp_residual(a, r=2)


def normals(cx, cy):
    d = S.Dds(S.van_sheet(cx, cy, '_msn'))
    n = d.level(0).astype(np.float64)[:, :, :3] / 255.0 * 2.0 - 1.0
    ln = np.sqrt((n ** 2).sum(2))
    return n / np.maximum(ln, 1e-6)[:, :, None]


def main():
    L1 = ['TILING2 work item 1c -- the source of vanilla`s fine detail',
          '',
          'r    = correlation of the candidate`s high-pass residual (finer than 5',
          '       texels) with VANILLA`s, over the whole 512x512 sheet.',
          'twin = the same correlation against the candidate`s phase-randomised',
          '       twin: same power spectrum, alignment destroyed. THE FLOOR.',
          'A candidate is only a source if |r| is meaningfully above |twin|.', '']
    res = {}
    for name, cx, cy in TILES:
        van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        vhp = hp(van)
        cands = []
        for lab, kw in (('land@code', dict(mip='code')),
                        ('land@code-1', dict(mip=-1.0)),
                        ('land@code-2', dict(mip=-2.0)),
                        ('land@code+1', dict(mip=1.0)),
                        ('land@box', dict(mip='box')),
                        ('land@mean', dict(mip='mean'))):
            sh = OB.bake(cx, cy, dim=4, tile=TILE, **kw)
            cands.append((lab, S.lum(sh)))
        base = OB.bake(cx, cy, dim=4, tile=TILE, mip='mean', layers=False, vclr=False)
        withv = OB.bake(cx, cy, dim=4, tile=TILE, mip='mean', layers=False, vclr=True)
        cands.append(('vclr', S.lum(withv) - S.lum(base) + 128.0))
        n = normals(cx, cy)
        cands.append(('slope', (1.0 - n[:, :, 2]) * 255.0))
        best = None
        for k in range(8):
            az = k * np.pi / 4.0
            l = np.array([np.cos(az) * 0.7071, np.sin(az) * 0.7071, 0.7071])
            sh = np.clip((n * l).sum(2), 0, 1) * 255.0
            c = abs(T.corr(vhp, hp(sh)))
            if best is None or c > best[0]:
                best = (c, k, sh)
        cands.append(('shading az%d' % (best[1] * 45), best[2]))
        ours = S.lum(S.Dds(os.path.join(HERE, 'out', 'rung', name, 'tex',
                                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))).level(0))
        cands.append(('ours (rung)', ours))

        L1.append('chunk (%d,%d)   vanilla high-pass SD = %.3f' % (cx, cy, vhp.std()))
        L1.append('  %-16s %8s %8s %9s %9s' % ('candidate', 'r', 'twin', 'hpSD', 'r/twin'))
        L1.append('  ' + '-' * 56)
        for lab, f in cands:
            fh = hp(f)
            r = T.corr(vhp, fh)
            tw = T.corr(vhp, hp(S.phase_twin(f.astype(np.float64), seed=5)))
            L1.append('  %-16s %8.4f %8.4f %9.3f %9.2f'
                      % (lab, r, tw, fh.std(),
                         abs(r) / max(abs(tw), 1e-6)))
            res.setdefault(lab, []).append(r)
        L1.append('')

    # the spectrum deficit, and vanilla's own sheet-to-sheet ceiling
    L1.append('spectrum distance (mean |log2| band-power ratio, bands finer than 32 texels)')
    pairs = [(-20, 24), (-20, 20), (-19, 24), (-21, 24), (-20, 25), (-20, 23)]
    v0 = S.lum(S.Dds(S.van_sheet(-20, 24)).level(0))
    L1.append('  VANILLA vs its own NEIGHBOURS  -- the ceiling for "how different is normal":')
    for (cx, cy) in pairs[1:]:
        try:
            vn = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        except Exception:
            continue
        L1.append('    (-20,24) vs (%d,%d)   %6.3f' % (cx, cy, T.spectrum_distance(v0, vn)))
    L1.append('')
    for name, cx, cy in TILES:
        van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        for var in ('rung', 't2048'):
            o = S.lum(S.Dds(os.path.join(HERE, 'out', var, name, 'tex',
                                         'Commonwealth.4.%d.%d.DDS' % (cx, cy))).level(0))
            L1.append('    vanilla (%d,%d) vs OURS %-6s  %6.3f   local variance %6.2f vs %6.2f'
                      ' = %+5.0f%%'
                      % (cx, cy, var, T.spectrum_distance(van, o),
                         S.local_var(o).mean(), S.local_var(van).mean(),
                         100.0 * (S.local_var(o).mean() / S.local_var(van).mean() - 1.0)))
    txt = '\n'.join(L1) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 't4_corr.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(res, open(os.path.join(HERE, 't4_corr.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
