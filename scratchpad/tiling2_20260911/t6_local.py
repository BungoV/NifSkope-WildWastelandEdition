"""TILING2 -- is the residual reading on chunk (-20,20) the LAND repeat?

With `--land-sample average` every land layer contributes ONE constant colour
per (quadrant, texture), so no land-texture periodicity can reach the sheet by
construction. The sheet still read 0.562 at the repeat. Either the instrument
is reading broadband structure in that bin, or something else in the bake still
samples a texture at the landscape tiling -- the road and cover planes do.

The decisive test is LOCAL: find the largest road-free window on that chunk and
read the same statistic there, on vanilla and on every variant. 256 texels is
exactly 24 repeats, so the repeat still lands on an integer FFT bin.

    python t6_local.py  ->  logs/t6_local.txt
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
CX, CY = -20, 20
W = 256


def main():
    mask = np.load(os.path.join(HERE, 'road_mask_t2020.npy'))
    # integral image over the road mask, best 256x256 window on a 32-texel grid
    best = None
    for y0 in range(0, 512 - W + 1, 32):
        for x0 in range(0, 512 - W + 1, 32):
            n = int(mask[y0:y0 + W, x0:x0 + W].sum())
            if best is None or n < best[0]:
                best = (n, y0, x0)
    n, y0, x0 = best
    L1 = ['TILING2 -- the repeat inside the largest ROAD-FREE window of (-20,20)',
          '',
          'window %dx%d at (x=%d, y=%d), %d road texels in it (%.3f%% of it);'
          % (W, W, x0, y0, n, 100.0 * n / (W * W)),
          'the whole sheet is 9.58%% road. 256 texels = exactly 24 repeats, so',
          'the repeat still sits on an integer FFT bin and nothing leaks.', '',
          '  %-12s %8s %8s %8s %9s' % ('variant', 'vis', 'floor', 'vis/fl', 'locVar'),
          '  ' + '-' * 50]
    rows = [('VANILLA', S.lum(S.Dds(S.van_sheet(CX, CY)).level(0)))]
    for v in ('rung', 't2048', 'avg', 'avgblend', 'k0.25', 'rungnorod', 'avgnorod'):
        p = os.path.join(HERE, 'out', v, 't2020', 'tex',
                         'Commonwealth.4.%d.%d.DDS' % (CX, CY))
        if os.path.exists(p):
            rows.append((v, S.lum(S.Dds(p).level(0))))
    for name, L in rows:
        w = L[y0:y0 + W, x0:x0 + W]
        vis, fl = T.tiling_visibility(w, P)
        L1.append('  %-12s %8.3f %8.3f %8.3f %9.2f'
                  % (name, vis, fl, vis / fl, S.local_var(w).mean()))
    txt = '\n'.join(L1) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 't6_local.txt'), 'w', newline='\n') as f:
        f.write(txt)


if __name__ == '__main__':
    main()
