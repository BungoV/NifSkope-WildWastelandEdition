#!/usr/bin/env python
"""PIC-RIVERBED step 3: pick the 128x128 window, by metric, over the
riverbed-dominant part of chunk (-20,20).

Metric (pre-registered here, before any picture):
  * ADMISSIBLE  = mean riverbed LTEX weight >= 0.85 over the window
                  (step 1's weight map, from the LAND records only)
                  AND ZERO texels covered by ROADS1's road mask -- the road
                  is a rasterised OBJECT, not ground paint, and a road stripe
                  in the crop is a second thing to explain.
  * RANKED BY   = mean 3x3 local variance of luminance on OUR shipped sheet
                  (splatlib.local_var -- SPLAT1's own speckle instrument),
                  which is the quantity the grey spots ARE.
The whole ranked table is printed, plus the rejected best-by-variance window
that is not riverbed, so the choice is visible.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))

import splatlib as S                                          # noqa: E402

OURS = REPO + '/scratchpad/roads1_20260911/out/after/tex/Commonwealth.4.-20.20.DDS'
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Commonwealth.4.-20.20.DDS'
WIN = 128
RES = 512


def boxsum(a, n):
    ii = np.zeros((a.shape[0] + 1, a.shape[1] + 1))
    ii[1:, 1:] = a.cumsum(0).cumsum(1)
    return (ii[n:, n:] - ii[:-n, n:] - ii[n:, :-n] + ii[:-n, :-n])


def main():
    log = []

    def p(s):
        print(s)
        log.append(s)

    ours = S.Dds(OURS).level(0)[:, :, :3]
    van = S.Dds(VAN).level(0)[:, :, :3]
    rb = np.load(os.path.join(HERE, 'riverbed.npz'))['riverbed']

    lv_o = S.local_var(S.lum(ours))
    sat = ours.max(2) - ours.min(2)

    n = WIN
    k = float(n * n)
    rb_w = boxsum(rb, n) / k
    lv_w = boxsum(lv_o, n) / k
    sat_w = boxsum(sat, n) / k

    p('windows: %d  (128x128, stride 1, over 512x512)' % rb_w.size)
    p('riverbed weight over all windows: min %.3f max %.3f' % (rb_w.min(), rb_w.max()))
    p('')

    road = np.load(REPO + '/scratchpad/roads1_20260911/masks_m20_20.npz')['road']
    road_w = boxsum(road.astype(np.float64), n) / k
    rb_only = rb_w >= 0.85
    ry, rx = np.unravel_index(np.argmax(np.where(rb_only, lv_w, -1.0)), lv_w.shape)
    p('riverbed-only best window (BEFORE the road rule): y %d x %d  localVar %.2f '
      'road %.1f%% of the window -- REJECTED, the road is an object, not paint'
      % (ry, rx, lv_w[ry, rx], 100.0 * road_w[ry, rx]))
    p('')
    adm = rb_only & (road_w == 0.0)
    p('admissible (riverbed weight >= 0.85, road texels == 0): %d windows' % int(adm.sum()))
    sc = np.where(adm, lv_w, -1.0)
    order = np.dstack(np.unravel_index(np.argsort(-sc, axis=None), sc.shape))[0]
    p('')
    p('top 10 admissible windows, ranked by our sheet\'s local variance:')
    p('   y    x   riverbedW   localVar(ours)   meanSat(ours)   road%')
    for y, x in order[:10]:
        p('  %3d  %3d   %.3f       %7.2f         %5.2f        %.2f'
          % (y, x, rb_w[y, x], lv_w[y, x], sat_w[y, x], 100.0 * road_w[y, x]))
    by, bx = int(order[0][0]), int(order[0][1])

    # the rejected control: the best window by variance ANYWHERE
    ay, ax = np.unravel_index(np.argmax(lv_w), lv_w.shape)
    p('')
    p('for contrast, the best window by variance ANYWHERE (rejected, not riverbed):')
    p('  y %d x %d  riverbedW %.3f  localVar %.2f' % (ay, ax, rb_w[ay, ax], lv_w[ay, ax]))
    p('  (the riverbed rule is what makes this lane\'s window the riverbed\'s.)')

    # numbers on the chosen window
    o = ours[by:by + WIN, bx:bx + WIN]
    v = van[by:by + WIN, bx:bx + WIN]
    p('')
    p('CHOSEN WINDOW  y=%d x=%d  (%dx%d texels of the 512x512 sheet)' % (by, bx, WIN, WIN))
    p('  riverbed LTEX weight      %.3f' % rb_w[by, bx])
    p('  road texels in the window %d of %d (%.2f%%)'
      % (int(road[by:by + WIN, bx:bx + WIN].sum()), WIN * WIN, 100.0 * road_w[by, bx]))
    p('  ours    local var %.2f   mean RGB %.1f,%.1f,%.1f  mean sat %.2f'
      % (S.local_var(S.lum(o)).mean(), o[:, :, 0].mean(), o[:, :, 1].mean(),
         o[:, :, 2].mean(), (o.max(2) - o.min(2)).mean()))
    p('  vanilla local var %.2f   mean RGB %.1f,%.1f,%.1f  mean sat %.2f'
      % (S.local_var(S.lum(v)).mean(), v[:, :, 0].mean(), v[:, :, 1].mean(),
         v[:, :, 2].mean(), (v.max(2) - v.min(2)).mean()))

    # world box
    CELL, DIM, CX0, CY0 = 4096.0, 4, -20, 20
    span = DIM * CELL
    wx0 = CX0 * CELL + (bx / RES) * span
    wx1 = CX0 * CELL + ((bx + WIN) / RES) * span
    wy1 = CY0 * CELL + (1.0 - (by / RES)) * span
    wy0 = CY0 * CELL + (1.0 - ((by + WIN) / RES)) * span
    p('  world box  x %.0f .. %.0f   y %.0f .. %.0f   (%.0f x %.0f units, 32 u/texel)'
      % (wx0, wx1, wy0, wy1, wx1 - wx0, wy1 - wy0))

    # an overview PNG with the window marked, for the lane's own eye only
    ov = Image.new('RGB', (RES * 2 + 12, RES), (20, 20, 22))
    ov.paste(Image.fromarray(ours.astype(np.uint8), 'RGB'), (0, 0))
    ov.paste(Image.fromarray(van.astype(np.uint8), 'RGB'), (RES + 12, 0))
    from PIL import ImageDraw
    d = ImageDraw.Draw(ov)
    for x0 in (0, RES + 12):
        d.rectangle([x0 + bx, by, x0 + bx + WIN - 1, by + WIN - 1], outline=(255, 60, 60))
    ov.save(os.path.join(HERE, 'images', 'overview_window.png'))

    json.dump(dict(y=by, x=bx, win=WIN, riverbedW=float(rb_w[by, bx]),
                   world=[wx0, wx1, wy0, wy1],
                   lv_ours=float(S.local_var(S.lum(o)).mean()),
                   lv_van=float(S.local_var(S.lum(v)).mean())),
              open(os.path.join(HERE, 'window.json'), 'w'), indent=1)
    with open(os.path.join(HERE, 'logs', 's3.log'), 'w') as f:
        f.write('\n'.join(log) + '\n')


if __name__ == '__main__':
    main()
