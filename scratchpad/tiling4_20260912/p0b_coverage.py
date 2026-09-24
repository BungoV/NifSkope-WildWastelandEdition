"""TILING4 step 0b -- how much of each of the fourteen sheets the offline model
actually paints, and how much of it is the 127.5 stand-in.

Step 0 found 139 fully painted dim-4 chunks, and exactly ONE of TILING3's seven
selection sheets is among them.  That is not a contradiction -- TILING3's own
floor was only "the bake is not flat" (SD >= 1.0) -- but it means six of the
seven sheets the shipped proposal was picked on carry some fraction of flat
127.5 stand-in, and a flat patch is a real term in a periodicity or a variance
statistic.  This prints the fraction, per sheet, for both sevens, so every
number later in this lane can be read next to how much of its sheet is real.

    python p0b_coverage.py  ->  logs/p0b_coverage.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import offline_bake as OB                                     # noqa: E402

TILE = 341.3333


def coverage(cx, cy):
    a = OB.bake(cx, cy, dim=4, tile=TILE, mip='code')
    flat = np.all(np.abs(a - 127.5) < 1e-9, axis=2)
    return float(flat.mean()), float(S.lum(a).std())


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    L = ['TILING4 -- the offline model`s coverage of the fourteen sheets', '',
         'stand-in%% = texels the model left at the flat 127.5 fill (no LAND record,',
         'or a quadrant with no base texture).  A flat patch is a real term in a',
         'periodicity and in a variance, so every later number is read beside this.', '']
    out = {}
    for label, coords in (('SELECTION (TILING3 frozen)', cfg['selection']),
                          ('VALIDATION (TILING4 frozen)', cfg['validation'])):
        L.append(label)
        L.append('   %-12s %10s %10s' % ('sheet', 'stand-in%', 'bake SD'))
        for cx, cy in coords:
            f, sd = coverage(cx, cy)
            out['%d,%d' % (cx, cy)] = dict(standin=f, sd=sd)
            L.append('   (%4d,%4d) %9.1f%% %10.2f' % (cx, cy, 100 * f, sd))
        L.append('')
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'p0b_coverage.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'coverage.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
