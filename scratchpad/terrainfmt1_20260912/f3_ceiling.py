"""Gate F3, the CEILING: how much texel-scale roughness could the land `_n`
maps possibly add to a dim-4 `_msn`?

The geometry, all of it fixed by the sheet and the tiling and none of it by
choice:

  a dim-4 chunk is 16384 world units over 512 sheet texels -> 32 world units a
  texel; the land tiling is 341.333 world units a repeat; so one sheet texel is
  32/341.333 = 0.09375 of a repeat, and on a 1024x1024 `_n` that is 96 source
  texels. The footprint mip is log2(96) = 6.585, i.e. between mip 6 (16x16, one
  repeat = 16 texels) and mip 7 (8x8).

At that mip, two ADJACENT sheet texels sample points 0.09375 repeats apart:
1.5 texels of mip 6, 0.75 of mip 7. So the most a single land `_n` at full
strength could contribute to the 1-texel roughness of the `_msn` is the
1-texel roughness of its own mip 6/7 -- which is what this measures, over 24
Commonwealth landscape normals, with no blend weights and no strength applied.
Any real bake blends 1..4 layers per quadrant, which can only average the
number DOWN.

The BC5 decode is cross-checked against dds_np's BC3 alpha decode (bc5_np.py).
"""
import glob
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from bc5_np import Bc5                                            # noqa: E402

D = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Landscape'


def rough(a, lag=1):
    a = a.astype(np.float64)
    return float(0.5 * (np.abs(a[:, lag:] - a[:, :-lag]).mean()
                        + np.abs(a[lag:, :] - a[:-lag, :]).mean()))


files = []
for sub in ('Ground', 'Grass', 'Roads', 'Rocks'):
    files += sorted(glob.glob(os.path.join(D, sub, '*_n.DDS')))[:6]
rows, res = [], {}
for f in files:
    try:
        b = Bc5(f)
    except ValueError as e:
        rows.append((os.path.basename(f), str(e)))
        continue
    r = {'size': b.width, 'mips': b.mips}
    for m in (0, 6, 7):
        if m < b.mips:
            xy = b.xy(m)
            r['mip%d' % m] = {'px': xy.shape[0],
                              'rough1_x': rough(xy[:, :, 0]),
                              'rough1_y': rough(xy[:, :, 1])}
    res[os.path.basename(f)] = r
    rows.append(r)

ok = [r for r in res.values() if 'mip6' in r and 'mip7' in r]


def agg(key, ch):
    v = [r[key]['rough1_' + ch] for r in ok]
    return float(np.mean(v)), float(np.max(v))


print('%d landscape _n maps read (%d were %dx%d)'
      % (len(ok), sum(1 for r in ok if r['size'] == 1024), 1024, 1024))
for key, note in (('mip0', 'full resolution, NOT what a footprint sample reads'),
                  ('mip6', 'one repeat = 16 texels; a sheet texel steps 1.5 of them'),
                  ('mip7', 'one repeat = 8 texels; a sheet texel steps 0.75 of them')):
    mx, hx = agg(key, 'x')
    my, hy = agg(key, 'y')
    print('  %-5s  rough1 X mean %6.3f max %6.3f   Y mean %6.3f max %6.3f   (%s)'
          % (key, mx, hx, my, hy, note))
out = {'files': res,
       'ceiling_mip6_mean': 0.5 * (agg('mip6', 'x')[0] + agg('mip6', 'y')[0]),
       'ceiling_mip6_max': 0.5 * (agg('mip6', 'x')[1] + agg('mip6', 'y')[1]),
       'ceiling_mip7_mean': 0.5 * (agg('mip7', 'x')[0] + agg('mip7', 'y')[0]),
       'ceiling_mip7_max': 0.5 * (agg('mip7', 'x')[1] + agg('mip7', 'y')[1])}
print('\nCEILING (single texture, strength 1, no blend):')
print('  mip 6: mean %.3f levels, best texture %.3f'
      % (out['ceiling_mip6_mean'], out['ceiling_mip6_max']))
print('  mip 7: mean %.3f levels, best texture %.3f'
      % (out['ceiling_mip7_mean'], out['ceiling_mip7_max']))
json.dump(out, open(os.path.join(HERE, 'f3_ceiling.json'), 'w'), indent=1)
