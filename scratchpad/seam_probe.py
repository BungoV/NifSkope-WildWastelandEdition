import os, sys, numpy as np
HERE = os.path.abspath('.')
for p in (HERE, os.path.join(os.path.dirname(HERE), 'splat1_20260911')): sys.path.insert(0, p)
import splatlib as S
cx, cy = -20, 24
tag = 'r_%d_%d_%d_%d' % (cx, cy, cx + 3, cy + 3)
srcs = {'vanilla': None}
for arm in ('fs_rung', 'fs_stoch', 'fs_warp'):
    srcs[arm] = os.path.join('out', arm, tag, 'tex', 'Commonwealth.4.%d.%d.DDS' % (cx, cy))
# vanilla from the game's own file if the lane cached it
import glob
v = glob.glob('../tiling2_20260911/**/Commonwealth.4.-20.24.DDS', recursive=True) + glob.glob('../tiling3_20260911/**/Commonwealth.4.-20.24.DDS', recursive=True)
if v: srcs['vanilla'] = v[0]
for name, p in srcs.items():
    if not p or not os.path.exists(p): print(name, 'missing'); continue
    L = S.lum(S.Dds(p).level(0)).astype(np.float64)
    rows = L.mean(axis=1); cols = L.mean(axis=0)
    dr = np.abs(np.diff(rows)); dc = np.abs(np.diff(cols))
    ir = int(np.argmax(dr)); ic = int(np.argmax(dc))
    print('%-9s rows: seam between %3d|%3d step %.2f (median row step %.2f)   cols: %3d|%3d step %.2f (median %.2f)'
          % (name, ir, ir + 1, dr[ir], np.median(dr), ic, ic + 1, dc[ic], np.median(dc)))
    top = np.argsort(dr)[-4:][::-1]
    print('          top row steps:', ', '.join('%d:%.2f' % (i, dr[i]) for i in top))
