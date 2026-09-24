"""TILING2 -- THE CONTROL the 1c table needs before it can be believed.

Every correlation in `t4_corr.txt` came out near zero. A correlation of zero is
also what a MISALIGNED pair of sheets reads, so the table is worth nothing until
the offline composite is shown to correlate with the generator's OWN output at
the same alignment. That is this file: the offline `land@code` bake against our
rung sheet, over a +-2 texel shift search.

If this reads high, the offline model and its alignment are right and the zeros
against vanilla are a real finding. If it reads zero too, the 1c table is void.

    python t4b_align.py  ->  logs/t4b_align.txt
"""
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


def main():
    L1 = ['TILING2 -- 1c`s alignment control: the offline composite vs OUR OWN bake',
          '',
          'r(shift) = correlation of the offline land@code high-pass residual with',
          'the high-pass residual of the sheet named, at a whole-texel shift.', '']
    for tile, cx, cy in (('t2024', -20, 24), ('t2020', -20, 20)):
        off = S.lum(OB.bake(cx, cy, dim=4, tile=TILE, mip='code'))
        ours = S.lum(S.Dds(os.path.join(HERE, 'out', 'rung', tile, 'tex',
                                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))).level(0))
        van = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        oh = T.hp_residual(off)
        L1.append('chunk (%d,%d)' % (cx, cy))
        for lab, ref in (('OURS rung', ours), ('vanilla', van)):
            rh = T.hp_residual(ref)
            best = None
            for dy in (-2, -1, 0, 1, 2):
                for dx in (-2, -1, 0, 1, 2):
                    a = np.roll(np.roll(oh, dy, 0), dx, 1)[8:-8, 8:-8]
                    b = rh[8:-8, 8:-8]
                    r = T.corr(a, b)
                    if best is None or abs(r) > abs(best[0]):
                        best = (r, dx, dy)
            r0 = T.corr(oh[8:-8, 8:-8], rh[8:-8, 8:-8])
            L1.append('  %-12s r(0,0) = %+7.4f   best %+7.4f at shift (%+d,%+d)'
                      % (lab, r0, best[0], best[1], best[2]))
        # a flipped-row check: if the offline model had V upside down
        rh = T.hp_residual(ours)
        L1.append('  %-12s r = %+7.4f  (row-flipped offline sheet; a model that '
                  'had V upside down would read HERE instead)'
                  % ('flip check', T.corr(oh[::-1][8:-8, 8:-8], rh[8:-8, 8:-8])))
        L1.append('')
    txt = '\n'.join(L1) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 't4b_align.txt'), 'w', newline='\n') as f:
        f.write(txt)


if __name__ == '__main__':
    main()
