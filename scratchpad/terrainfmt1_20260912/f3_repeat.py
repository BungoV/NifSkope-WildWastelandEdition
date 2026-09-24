"""Gate F3, the repeat the candidate introduces.

The land tiling is 341.333 world units and a dim-4 sheet is 16384 across 512
texels, so a blended land `_n` term repeats EXACTLY 48 times over the sheet and
must show as a spectral line at k=48. Vanilla's `_msn` carries no such line, and
neither does our own bake; a candidate that carries one has added a visible
repeat to a sheet that had none.

Statistic: |F| averaged over the k=48 row and column of the high-passed sheet,
over the median of the same quantity for k in 40..56 excluding 48. 1.0 = no line.
"""
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dds_np import Dds                                            # noqa: E402
from bc5_np import Bc5                                            # noqa: E402
from f3_common import tiled, hp                                   # noqa: E402

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
LAND = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Landscape'


def line48(a):
    F = np.abs(np.fft.fft2(hp(a)))

    def amp(k):
        return 0.5 * (F[k, :].mean() + F[:, k].mean())

    other = np.median([amp(k) for k in range(40, 57) if k != 48])
    return float(amp(48) / other)


out = {}
for ch, bake in (('Commonwealth.4.-20.24', 'ourleg'),
                 ('Commonwealth.4.-16.24', 'ourleg2')):
    van = Dds(os.path.join(VAN, ch + '_msn.DDS')).rgb(0)[:, :, 0].astype(float)
    ours = Dds(os.path.join(HERE, 'bake', bake, 'tex',
                            ch + '_msn.DDS')).rgb(0)[:, :, 0].astype(float)
    b = Bc5(os.path.join(LAND, 'Ground', 'BlastedForestDirt01_n.DDS'))
    d = tiled(b.xy(6).astype(float)[:, :, 0])
    d -= d.mean()
    rec = {'vanilla': line48(van), 'ours': line48(ours),
           'candidate_s1': line48(ours + d),
           'candidate_fitted': line48(ours + 1.73 * d)}
    out[ch] = rec
    print('%s  vanilla %.3f  ours %.3f  candidate s=1 %.3f  fitted %.3f'
          % (ch, rec['vanilla'], rec['ours'], rec['candidate_s1'],
             rec['candidate_fitted']))
json.dump(out, open(os.path.join(HERE, 'f3_repeat.json'), 'w'), indent=1)
