"""Gate S3, scored with a TIE-HONEST AUC, and the audit of why that was needed.

gate4.py reproduced FLAGSCAN1's masks to the texel (72,264 / 27,988 / 145,001)
and its brightness AUC to the last digit (0.628 against 0.629), but its GREYNESS
AUC came out 0.757 where hw_m8_8.txt records 0.716 -- and hw_tile.py itself,
re-run today on the same vanilla sheet, also says 0.757.  The reason is in the
score, not in either script:

  * saturation on this sheet takes 281 distinct values over 262,144 texels, and
    ONE of them covers 140,305 texels -- 53.5 percent of the tile;
  * `tile2.auc()` ranks with `np.argsort`, which breaks ties by array index, and
    the index runs across the sheet in raster order, so a spatially clustered
    mask gets a rank that depends on WHERE its texels sit inside the tie block;
  * with the ties broken at random instead the same AUC is 0.731, repeatably
    (0.7316 / 0.7308 / 0.7308 on three seeds).

Brightness has 1,499 distinct values and no dominant block, which is why that
half reproduces and the grey half does not.  So this script scores every sheet
twice: the score as hw_tile.py wrote it, and with TIE-AVERAGED ranks, which is
the AUC's own definition when ties exist (a tie contributes half a comparison).
The gate is read off the tie-averaged column.

usage: gate4b.py <masks.npz> <name=texdir> ...
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'flagscan1_20260911'))
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from tile2 import SHIFTS, auc, phase_twin, sheet, shift      # noqa: E402

CX0, CY0 = -8, 8
FAMS = ('road_elevated', 'road_ground', 'nonroad')


def rankdata(s):
    """Average ranks over ties (scipy.stats.rankdata, no scipy here)."""
    order = np.argsort(s, kind='stable')
    sv = s[order]
    r = np.empty(len(s), dtype=np.float64)
    n = len(s)
    i = 0
    while i < n:
        j = i
        while j + 1 < n and sv[j + 1] == sv[i]:
            j += 1
        r[order[i:j + 1]] = 0.5 * (i + 1 + j + 1)
        i = j + 1
    return r


def auc_tie(score, mask):
    s = score.ravel().astype(np.float64)
    y = mask.ravel()
    r = rankdata(s)
    n1 = float(y.sum())
    n0 = float(len(y) - n1)
    return (r[y].sum() - n1 * (n1 + 1) / 2.0) / (n1 * n0)


def main(argv):
    mz = np.load(argv[0])
    masks = {k: mz[k] for k in FAMS}
    sheets = [a.split('=', 1) for a in argv[1:]]

    fields = {}
    for name, d in sheets:
        col = sheet(os.path.join(d, 'Commonwealth.4.%d.%d.DDS' % (CX0, CY0)))
        lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
        sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)
        fields[name] = (lum, -sat)

    for si, lbl, fn in ((0, 'AUC BRIGHTNESS, tie-averaged (brighter = higher)', auc_tie),
                        (1, 'AUC GREYNESS, tie-averaged (greyer = higher)', auc_tie),
                        (1, 'AUC GREYNESS, argsort as hw_tile.py wrote it', auc)):
        print('')
        print(lbl)
        print('%-16s %7s' % ('family', 'texels')
              + ''.join(' %22s' % n[:22] for n, _ in sheets))
        for fam in FAMS:
            m = masks[fam]
            line = '%-16s %7d' % (fam, int(m.sum()))
            for n, _ in sheets:
                s = fields[n][si]
                a = fn(s, m)
                fl = [fn(s, shift(m, dx, dy)) for dx, dy in SHIFTS]
                line += ' %6.3f [%.3f..%.3f]' % (a, min(fl), max(fl))
            print(line)
        line = '%-16s %7s' % ('twins (ground)', '')
        for n, _ in sheets:
            s = fields[n][si]
            tw = [fn(s, phase_twin(masks['road_ground'], k)) for k in (1, 2, 3)]
            line += ' %22s' % ('%.3f..%.3f' % (min(tw), max(tw)))
        print(line)

    print('')
    print('CLEARANCE above the top of the displaced floor (tie-averaged '
          'brightness); positive = the family is visible in the sheet (ranked apart from an unregistered mask of its own shape)')
    print('%-16s' % 'family' + ''.join(' %11s' % n[:11] for n, _ in sheets))
    for fam in FAMS:
        m = masks[fam]
        line = '%-16s' % fam
        for n, _ in sheets:
            s = fields[n][0]
            a = auc_tie(s, m)
            fl = max(auc_tie(s, shift(m, dx, dy)) for dx, dy in SHIFTS)
            line += ' %+11.3f' % (a - fl)
        print(line)


if __name__ == '__main__':
    main(sys.argv[1:])
