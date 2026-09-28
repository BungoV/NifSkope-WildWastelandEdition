"""TERR1 continuation: is the normal stamp offset from the colour on the texel grid? IoU of the moved-normal mask
(ON vs OFF > 5 deg) against the colour-changed mask (on vs noroads > 3 levels) shifted by (dj, di)."""
import sys
import numpy as np
import vtmosaic as VM
from nrmgate import ang
on, off, nr = (VM.Sheets(p) for p in sys.argv[1:4])
a = ang(VM.msn_world(on.mosaic(2)), VM.msn_world(off.mosaic(2))) > 5
c = np.abs(on.mosaic(1)[..., :3].astype(int) - nr.mosaic(1)[..., :3].astype(int)).max(-1) > 3
N = a.shape[0]
res = []
for dj in range(-2, 3):
    for di in range(-2, 3):
        A = a[2:N-2, 2:N-2]; C = c[2+dj:N-2+dj, 2+di:N-2+di]
        res.append((dj, di, (A & C).sum() / (A | C).sum(), (A & ~C).sum(), (C & ~A).sum()))
for r in sorted(res, key=lambda r: -r[2])[:6]:
    print('shift dj %+d di %+d  IoU %.4f  normal-only %d  colour-only %d' % r)
