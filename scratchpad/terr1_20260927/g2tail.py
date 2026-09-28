"""TERR1 continuation: texel-level 'normal moved, colour did not' census (decoded BC1), ON vs OFF / ON vs noroads."""
import sys
import numpy as np
import vtmosaic as VM
from nrmgate import ang
on, off, nr = (VM.Sheets(p) for p in sys.argv[1:4])
a = ang(VM.msn_world(on.mosaic(2)), VM.msn_world(off.mosaic(2)))
cd = np.abs(on.mosaic(1)[..., :3].astype(int) - nr.mosaic(1)[..., :3].astype(int)).max(-1)
for thr in (1, 10, 30):
    m = a > thr
    print('normal moved >%d deg: %d texels; colour diff 0: %.4f, <=2: %.4f, <=5: %.4f' % (
        thr, m.sum(), (cd[m] == 0).mean(), (cd[m] <= 2).mean(), (cd[m] <= 5).mean()))
# of colour-diff-0 moved texels, how many have a colour-changed texel within 1 / 2 texels
m = (a > 10) & (cd == 0)
chg = cd > 0
from numpy.lib.stride_tricks import sliding_window_view as sw
p = np.pad(chg, 2)
n1 = sw(p, (3, 3))[1:-1, 1:-1].any((-1, -2)); n2 = sw(p, (5, 5)).any((-1, -2))
print('moved>10 & colour 0:', m.sum(), 'with colour change within 1 texel %.3f, within 2 %.3f' % (n1[m].mean(), n2[m].mean()))
