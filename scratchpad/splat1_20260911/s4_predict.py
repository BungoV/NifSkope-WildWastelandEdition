"""SPLAT1 -- what the one change would do to the numbers the other two lanes
quoted, predicted OFFLINE before any build.

TERRAIN-R and ROADS1 both quoted a whole-tile mean colour error against
vanilla. This re-bakes each tile offline at the shipped tiling and at the
engine's own, and prints that error both ways, plus the local variance, plus
the codec/reproduction gap so the prediction is not read as a promise.
"""
import os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import splatlib as S
import offline_bake as B

def err(a, b):
    return float(np.abs(a[:, :, :3].astype(np.float64) - b[:, :, :3].astype(np.float64)).mean())

for (cx0, cy0) in ((-20, 24), (-20, 20)):
    van = S.Dds(S.van_sheet(cx0, cy0)).level(0)
    real = S.Dds(S.OURS[(cx0, cy0)]).level(0)
    print('=== chunk (%d,%d) ===' % (cx0, cy0))
    print('  ours as shipped vs vanilla: mean |RGB| %6.2f   lv %6.2f'
          % (err(real, van), S.local_var(S.lum(real)).mean()))
    for t in (2048.0, 341.3333):
        sh = B.bake(cx0, cy0, 4, mip='code', tile=t)
        img = np.dstack([sh, np.full(sh.shape[:2], 255.0)])
        print('  offline TILE=%8.3f       vs vanilla: mean |RGB| %6.2f   lv %6.2f'
              % (t, err(img, van), S.local_var(S.lum(img)).mean()))
    print('  vanilla lv %.2f' % S.local_var(S.lum(van)).mean())
    print('')
