"""cmp_msn_detail.png -- gate F3's refusal, as a picture.

A 192x192 crop of the east channel at mip 0, shown twice for each arm: the raw
levels on a common window, and the high-pass (the scale the roughness statistic
actually measures) on a common window. A common window on both rows is the
whole point -- an arm cannot look rougher because its own tile was stretched
harder.
"""
import glob
import os
import sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dds_np import Dds                                            # noqa: E402
from bc5_np import Bc5                                            # noqa: E402
from f3_common import tiled, hp, phase_twin                       # noqa: E402
from pairpage import build                                        # noqa: E402

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
LAND = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Landscape'
CH = 'Commonwealth.4.-20.24'
X, Y, S = 160, 160, 192

van = Dds(os.path.join(VAN, CH + '_msn.DDS')).rgb(0)[:, :, 0].astype(np.float64)
ours = Dds(os.path.join(HERE, 'bake', 'ourleg', 'tex',
                        CH + '_msn.DDS')).rgb(0)[:, :, 0].astype(np.float64)
b = Bc5(os.path.join(LAND, 'Ground', 'BlastedForestDirt01_n.DDS'))
d = tiled(b.xy(6).astype(np.float64)[:, :, 0])
d -= d.mean()
cand1 = ours + 1.0 * d
cand5 = ours + 1.73 * d
rng = np.random.default_rng(7)
dtwin = phase_twin(d, rng)
twin5 = ours + 1.73 * dtwin

arms = [('vanilla _msn   rough1 19.83  repeat 0.99', van),
        ('our bake   2.22  repeat 0.99', ours),
        ('candidate s=1   11.77  repeat 6.09', cand1),
        ('candidate fitted s=1.73   19.83  repeat 10.09', cand5),
        ('phase twin of the candidate, s=1.73', twin5)]


def tile(a, lo, hi, name):
    c = a[Y:Y + S, X:X + S]
    v = np.clip((c - lo) / (hi - lo), 0, 1)
    p = os.path.join(HERE, 'images', 'd_%s.png' % name)
    Image.fromarray((v * 255).astype(np.uint8)).resize((S * 2, S * 2),
                                                       Image.NEAREST).save(p)
    return p


raws = [a[Y:Y + S, X:X + S] for _, a in arms]
lo, hi = min(r.min() for r in raws), max(r.max() for r in raws)
hps = [hp(a)[Y:Y + S, X:X + S] for _, a in arms]
hlo, hhi = min(h.min() for h in hps), max(h.max() for h in hps)
items = []
for i, (lab, a) in enumerate(arms):
    items.append((lab, tile(a, lo, hi, 'raw%d' % i)))
for i, (lab, a) in enumerate(arms):
    p = os.path.join(HERE, 'images', 'd_hp%d.png' % i)
    v = np.clip((hps[i] - hlo) / (hhi - hlo), 0, 1)
    Image.fromarray((v * 255).astype(np.uint8)).resize((S * 2, S * 2),
                                                       Image.NEAREST).save(p)
    items.append(('high-pass: ' + lab.split('   ')[0], p))

build(os.path.join(HERE, 'images', 'cmp_msn_detail.png'), 5, 300, items,
      caption=('Commonwealth.4.-20.24, east channel (R) of the _msn at mip 0, '
               'a 192x192 texel crop at (160,160), nearest-neighbour x2. Both rows '
               'share one window across all five tiles. rough1 = mean 1-texel '
               'neighbour difference in levels. The candidate is BlastedForestDirt01_n '
               'at footprint mip 6, tiled 341.333, covering the whole chunk at full '
               'weight -- the best case, no layer blend. Correlation of the candidate'
               "'s fine detail with vanilla's: mean |r| 0.0010 over 22 textures, "
               "against 0.0011 for its own phase twin. 'repeat' = the spectral "
               'line at k=48, the 341.333 tiling frequency, over its neighbours: '
               'vanilla and our bake carry none, the candidate carries one. '
               'Row 1 window %.0f..%.0f levels, row 2 %.0f..%.0f.'
               % (lo, hi, hlo, hhi)))
