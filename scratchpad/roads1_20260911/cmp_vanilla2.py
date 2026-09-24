"""ROADS1 section 1, part 3: the three discriminators.

  D1  Is the road content in the LAND PAINT?  Every LTEX painted in the chunk's
      sixteen cells is listed with its diffuse path, so "the paint has no road
      texture here" is a list, not an impression.
  D2  Does vanilla's colour sheet single out the road footprint?  A grey score
      (1 - saturation) is ranked and the road mask's AUC against it reported,
      with a phase-randomised control (the same mask shifted) and the ceiling
      (the mask against itself).
  D3  Is it a MESH edge?  The mean colour gradient on the mask's own boundary
      texels against the same statistic on the shifted mask's boundary, and
      against the sheet's overall mean gradient.
  D4  Does the `_msn` carry it?  The tilt distribution inside the mask against
      the background and against the shifted control.

  python cmp_vanilla2.py <masks.npz> <esm> <vanillaDir> <dataRoot> <cx> <cy>
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                          # noqa: E402
from lodgen_cover_model import Esm                            # noqa: E402

FAMS = ('road', 'trees', 'rocks', 'buildings', 'setdressing')
DIM = 4


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    return np.array(px, dtype=np.float32).reshape(h, w, 4)


def auc(score, mask):
    """Area under the ROC of `score` as a detector of `mask`."""
    s = score.ravel()
    y = mask.ravel()
    order = np.argsort(s)
    ranks = np.empty(len(s), dtype=np.float64)
    ranks[order] = np.arange(1, len(s) + 1)
    n1 = y.sum()
    n0 = len(y) - n1
    if n1 == 0 or n0 == 0:
        return float('nan')
    return (ranks[y].sum() - n1 * (n1 + 1) / 2.0) / (n1 * n0)


def boundary(m):
    b = np.zeros_like(m)
    b[:-1, :] |= m[:-1, :] != m[1:, :]
    b[1:, :] |= m[:-1, :] != m[1:, :]
    b[:, :-1] |= m[:, :-1] != m[:, 1:]
    b[:, 1:] |= m[:, :-1] != m[:, 1:]
    return b


def shift(m, dx, dy):
    return np.roll(np.roll(m, dy, axis=0), dx, axis=1)


def main(argv):
    npz, esmPath, vanDir, dataRoot, cx, cy = (argv[0], argv[1], argv[2], argv[3],
                                              int(argv[4]), int(argv[5]))
    d = np.load(npz)
    road = d['road']
    col = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx, cy)))
    msn = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy)))

    print('=== D1  the LAND paint in cells %d..%d x %d..%d ==='
          % (cx, cx + DIM - 1, cy, cy + DIM - 1))
    e = Esm(esmPath)
    e.walk(0x3C)
    used = {}
    for gx in range(cx, cx + DIM):
        for gy in range(cy, cy + DIM):
            land = e.lands.get((gx, gy))
            if land is None:
                print('   cell (%d,%d): NO LAND record' % (gx, gy))
                continue
            for q in range(4):
                if land['base'][q]:
                    used.setdefault(land['base'][q], set()).add('base')
                for l in land['layers'][q]:
                    used.setdefault(l['ltex'], set()).add('layer')
    for form, how in sorted(used.items()):
        rec = e.ltex.get(form)
        tx = e.txst.get(rec['tnam'], {}) if rec else {}
        print('   %08X %-28s %-6s %s'
              % (form, (rec or {}).get('edid', '?'), ','.join(sorted(how)),
                 tx.get('tx00', '(no TXST)')))

    print('=== D2  grey score AUC (1 = the mask is exactly the grey texels) ===')
    sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)
    grey = -sat
    lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
    for f in FAMS:
        print('   %-12s AUC(grey) %.3f   AUC(bright) %.3f'
              % (f, auc(grey, d[f]), auc(lum, d[f])))
    for dx, dy in ((64, 64), (-96, 48), (128, -128), (0, 200)):
        s = shift(road, dx, dy)
        print('   control road shifted %4d,%4d  AUC(grey) %.3f  overlap %.3f'
              % (dx, dy, auc(grey, s),
                 float((s & road).sum()) / max(1, road.sum())))
    print('   ceiling  AUC(the mask itself as the score) %.3f'
          % auc(road.astype(np.float64), road))

    print('=== D3  gradient on the mask boundary (mesh edges are SHARP) ===')
    gy_, gx_ = np.gradient(lum)
    gmag = np.hypot(gx_, gy_) * 255.0
    b = boundary(road)
    print('   road boundary   n=%6d  mean |grad| %.3f' % (b.sum(), gmag[b].mean()))
    print('   whole sheet              mean |grad| %.3f' % gmag.mean())
    for dx, dy in ((64, 64), (-96, 48), (128, -128)):
        bb = boundary(shift(road, dx, dy))
        print('   shifted %4d,%4d boundary n=%6d  mean |grad| %.3f'
              % (dx, dy, bb.sum(), gmag[bb].mean()))
    for f in FAMS[1:]:
        bb = boundary(d[f])
        if bb.sum():
            print('   %-12s boundary n=%6d  mean |grad| %.3f'
                  % (f, bb.sum(), gmag[bb].mean()))

    print('=== D4  the _msn sheet.  Channel means R %.1f G %.1f B %.1f A %.1f ==='
          % tuple(msn[:, :, k].mean() * 255 for k in range(4)))
    # G is the one near 1: the sheet stores (x, up, y) -- report both readings
    for upIdx, nm in ((1, 'G as up'), (2, 'B as up')):
        idx = [0, 1, 2]
        idx.remove(upIdx)
        up = msn[:, :, upIdx] * 2 - 1
        tilt = np.degrees(np.arccos(np.clip(up, -1, 1)))
        line = '   %-10s' % nm
        for f in ('road', 'trees', 'rocks'):
            m = d[f]
            line += '  %s %.2f+-%.2f' % (f, tilt[m].mean(), tilt[m].std())
        bg = ~(road | d['trees'] | d['rocks'] | d['buildings'] | d['setdressing'])
        line += '  bg %.2f+-%.2f' % (tilt[bg].mean(), tilt[bg].std())
        s = shift(road, 64, 64)
        line += '  shifted %.2f+-%.2f' % (tilt[s].mean(), tilt[s].std())
        print(line)
    nxy = np.hypot(msn[:, :, 0] * 2 - 1, msn[:, :, 2] * 2 - 1)
    b = boundary(road)
    gy2, gx2 = np.gradient(msn[:, :, 1])
    gm2 = np.hypot(gx2, gy2) * 255.0
    print('   _msn G gradient: road boundary %.3f   whole sheet %.3f   shifted %.3f'
          % (gm2[b].mean(), gm2.mean(), gm2[boundary(shift(road, 64, 64))].mean()))
    print('   |n.xy| road %.4f  bg %.4f' % (nxy[road].mean(), nxy[~road].mean()))


if __name__ == '__main__':
    main(sys.argv[1:])
