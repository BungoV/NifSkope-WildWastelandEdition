"""TILING2 -- every variant against vanilla's laws, on both tiles.

    python t5_variants.py [variant ...]   ->  logs/t5_variants.txt
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
from t3b_seam import seam, binom_p                            # noqa: E402

P = 341.3333 / 32.0
TILES = [('t2024', -20, 24), ('t2020', -20, 20)]
DEF = ['rung', 't2048', 'avg', 'blend', 'avgblend']


def sheet(variant, tile, cx, cy):
    return S.lum(S.Dds(os.path.join(HERE, 'out', variant, tile, 'tex',
                                    'Commonwealth.4.%d.%d.DDS' % (cx, cy))).level(0))


def row(L, van):
    vis, fl = T.tiling_visibility(L, P)
    w, idx, g = T.edge_widths(L)
    hard = w <= 1.0
    on, n, ch = T.quadrant_hits(idx[hard]) if hard.any() else (0, 0, 0.092)
    smx, sav, _ = seam(L)
    _c, _p, bt = T.bands(L)
    tot = sum(v for _n, v in bt)
    return dict(vis=vis, floor=fl, ratio=vis / fl,
                w50=float(np.median(w)), w90=float(np.percentile(w, 90)),
                hard=int(hard.sum()), on=on, n=n,
                p=binom_p(on, n, ch) if n else 1.0,
                seamMax=smx, seamAvg=sav,
                lv=float(S.local_var(L).mean()),
                sd=float(L.std()), mean=float(L.mean()),
                spec=T.spectrum_distance(van, L),
                fine=100.0 * sum(v for _n, v in bt[3:]) / tot)


def main():
    vars_ = sys.argv[1:] or DEF
    hd = ('%-9s %-6s %7s %7s %7s %6s %6s %6s %7s %7s %7s %7s %7s %7s'
          % ('tile', 'variant', 'vis', 'floor', 'vis/fl', 'w50', 'w90', 'hard',
             'quadP', 'seamAvg', 'seamMax', 'locVar', 'specD', 'fine%'))
    L1 = ['TILING2 -- the variants against vanilla`s law', '',
          'vis    tiling visibility, amplitude of 255 at the 10.667-texel repeat',
          '       VANILLA: median 0.118, WORST OF 22 = 0.264  <- the ceiling',
          'vis/fl the same over the sheet`s own null floor',
          '       VANILLA: median 0.164, WORST OF 22 = 0.448  <- the ceiling',
          'seamAvg mean gradient across the 14 quadrant lines / the sheet`s own',
          '       VANILLA: median 1.041, WORST OF 22 = 1.100  <- the ceiling',
          'quadP  binomial p that this many hard edges sit on a quadrant line',
          'specD  spectrum distance to THIS tile`s vanilla sheet (0 = identical)',
          'fine%  share of the sheet`s variance finer than 8 texels', '',
          hd, '-' * len(hd)]
    out = {}
    for tile, cx, cy in TILES:
        van = sheet.__wrapped__ if False else None
        vanL = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        r = row(vanL, vanL)
        L1.append('%-9s %-6s %7.3f %7.3f %7.3f %6.2f %6.2f %6d %7.3f %7.3f %7.3f %7.2f %7.3f %7.1f'
                  % (tile, 'VANILLA', r['vis'], r['floor'], r['ratio'], r['w50'],
                     r['w90'], r['hard'], r['p'], r['seamAvg'], r['seamMax'],
                     r['lv'], r['spec'], r['fine']))
        for v in vars_:
            try:
                L = sheet(v, tile, cx, cy)
            except Exception as exc:
                L1.append('%-9s %-6s  MISSING (%s)' % (tile, v, exc.__class__.__name__))
                continue
            r = row(L, vanL)
            out['%s/%s' % (tile, v)] = r
            L1.append('%-9s %-6s %7.3f %7.3f %7.3f %6.2f %6.2f %6d %7.3f %7.3f %7.3f %7.2f %7.3f %7.1f'
                      % (tile, v, r['vis'], r['floor'], r['ratio'], r['w50'],
                         r['w90'], r['hard'], r['p'], r['seamAvg'], r['seamMax'],
                         r['lv'], r['spec'], r['fine']))
        L1.append('-' * len(hd))
    txt = '\n'.join(L1) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 't5_variants.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 't5_variants.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
