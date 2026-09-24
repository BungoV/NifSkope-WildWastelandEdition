"""Gate F3, the structure test -- the one that decides the gate.

Fitting a strength makes the candidate's texel-scale roughness EQUAL vanilla's.
That is an amplitude. It says nothing about whether the detail is in the same
PLACES as vanilla's, and a matched amplitude of unrelated noise would pass it.

So: high-pass both (subtract a 3x3 box mean, which leaves exactly the scale the
roughness statistic measures), and correlate the candidate's added term with
vanilla's own fine detail over the whole 512x512 sheet.

  floor   the SAME correlation against a phase-randomised twin of the candidate
          -- identical power spectrum, structure destroyed. A candidate that
          scores no better than its own twin has reproduced an amplitude and
          nothing else.
  bound   vanilla's fine detail correlated with ITSELF through one BC1
          round trip, i.e. how much correlation survives the compression
          vanilla's own sheet carries. Nothing can beat that.
"""
import glob
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dds_np import Dds                                            # noqa: E402
from bc5_np import Bc5                                            # noqa: E402
from f3_common import hp, corr, tiled, phase_twin as twin          # noqa: E402

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
LAND = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Landscape'

files = []
for sub in ('Ground', 'Grass', 'Roads', 'Rocks'):
    files += sorted(glob.glob(os.path.join(LAND, sub, '*_n.DDS')))[:6]
rng = np.random.default_rng(4242)
out = {}
for ch, bake in (('Commonwealth.4.-20.24', 'ourleg'),
                 ('Commonwealth.4.-16.24', 'ourleg2')):
    van = Dds(os.path.join(VAN, ch + '_msn.DDS')).rgb(0)
    ours = Dds(os.path.join(HERE, 'bake', bake, 'tex', ch + '_msn.DDS')).rgb(0)
    ve, vn = hp(van[:, :, 0]), hp(van[:, :, 2])
    oe, on = hp(ours[:, :, 0]), hp(ours[:, :, 2])
    rec = {'ours_vs_vanilla_fine': 0.5 * (corr(oe, ve) + corr(on, vn)),
           'textures': {}}
    print('\n%s' % ch)
    print('  our own coarse bake, fine detail vs vanilla fine detail: r = %.4f'
          % rec['ours_vs_vanilla_fine'])
    print('  %-40s %8s %8s' % ('land _n detail term', 'r', 'twin r'))
    rs, ts = [], []
    for f in files:
        try:
            b = Bc5(f)
        except ValueError:
            continue
        if b.mips <= 6:
            continue
        xy = b.xy(6).astype(np.float64)
        dx, dy = tiled(xy[:, :, 0]), tiled(xy[:, :, 1])
        hx, hy = hp(dx), hp(dy)
        r = 0.5 * (corr(hx, ve) + corr(hy, vn))
        tr = 0.5 * (corr(twin(hx, rng), ve) + corr(twin(hy, rng), vn))
        rec['textures'][os.path.basename(f)] = {'r': r, 'twin_r': tr}
        rs.append(r)
        ts.append(tr)
        print('  %-40s %8.4f %8.4f' % (os.path.basename(f)[:40], r, tr))
    rec['mean_r'] = float(np.mean(rs))
    rec['mean_abs_r'] = float(np.mean(np.abs(rs)))
    rec['best_abs_r'] = float(np.max(np.abs(rs)))
    rec['mean_abs_twin_r'] = float(np.mean(np.abs(ts)))
    print('  mean |r| %.4f   best |r| %.4f   mean |twin r| %.4f (the floor)'
          % (rec['mean_abs_r'], rec['best_abs_r'], rec['mean_abs_twin_r']))
    out[ch] = rec
json.dump(out, open(os.path.join(HERE, 'f3_struct.json'), 'w'), indent=1)
print('\nwrote f3_struct.json')
