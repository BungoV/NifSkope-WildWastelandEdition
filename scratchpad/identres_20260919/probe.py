#!/usr/bin/env python3
"""IDENTRES probe -- what can a 4 u map afford?  Sizes only, nothing rendered."""
import numpy as np
import sys
import time

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/identres_20260919'
IP = ROOT + '/scratchpad/identprox_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
for q in (IP, SUNSIM, ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

from scene import Terrain, Objects, CHUNK                     # noqa: E402

log = open(LANE + '/probe.log', 'w')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    log.write(s + '\n')
    log.flush()


ter = Terrain()
ob = Objects(verbose=False)
p('objects: %d verts %d tris, world z %.0f .. %.0f'
  % (len(ob.v), len(ob.tri), ob.v[:, 2].min(), ob.v[:, 2].max()))

margin = 2048.0
x0, y0, x1, y1 = CHUNK
x0 -= margin; y0 -= margin; x1 += margin; y1 += margin
gx = np.linspace(x0, x1, 65)
gy = np.linspace(y0, y1, 65)
GX, GY = np.meshgrid(gx, gy)
GZ = ter.atf(GX, GY)
zlo = min(float(ob.v[:, 2].min()), float(np.nanmin(GZ[GZ > -1e5])))
zhi = max(float(ob.v[:, 2].max()), float(np.nanmax(GZ)))
p('z range used for the map s extent: %.0f .. %.0f' % (zlo, zhi))

for az, el in ((180.0, 10.0), (120.0, 15.0)):
    a = np.radians(az)
    sx, sy = np.sin(a), np.cos(a)
    tn = np.tan(np.radians(el))
    cor = np.array([[x0, y0], [x1, y0], [x0, y1], [x1, y1]])
    V = -cor[:, 0] * sy + cor[:, 1] * sx
    U = cor[:, 0] * sx + cor[:, 1] * sy
    v = ob.v.astype(np.float64)
    OU = v[:, 0] * sx + v[:, 1] * sy
    OV = -v[:, 0] * sy + v[:, 1] * sx
    OS = v[:, 2] - OU * tn
    t = ob.tri
    for texel in (64.0, 16.0, 8.0, 4.0):
        s0 = zlo - U.max() * tn - texel * 4
        s1 = zhi - U.min() * tn + texel * 4
        nv = int((V.max() + texel - (V.min() - texel)) / texel) + 1
        ns = int((s1 - s0) / texel) + 1
        gv = (OV - (V.min() - texel)) / texel
        gs = (OS - s0) / texel
        bx0 = np.floor(np.minimum.reduce([gv[t[:, 0]], gv[t[:, 1]], gv[t[:, 2]]]))
        bx1 = np.ceil(np.maximum.reduce([gv[t[:, 0]], gv[t[:, 1]], gv[t[:, 2]]]))
        by0 = np.floor(np.minimum.reduce([gs[t[:, 0]], gs[t[:, 1]], gs[t[:, 2]]]))
        by1 = np.ceil(np.maximum.reduce([gs[t[:, 0]], gs[t[:, 1]], gs[t[:, 2]]]))
        span = np.maximum(bx1 - bx0, by1 - by0)
        K = np.power(2.0, np.clip(np.ceil(np.log2(np.maximum(span, 1))), 0, 14))
        area = (bx1 - bx0) * (by1 - by0)
        nstep = texel * 0.5
        npts = (int((x1 - x0) / nstep) + 1) * (int((y1 - y0) / nstep) + 1)
        p('az%3.0f el%2.0f texel %5.1f: map %5d x %5d = %13s texels (%7.1f MB int64) | '
          'tri bbox cells %10.3f M, K*K allocations %10.3f M | terrain near splat %8.1f M pts'
          % (az, el, texel, nv, ns, '{:,}'.format(nv * ns), nv * ns * 8 / 1e6,
             area.sum() / 1e6, (K * K).sum() / 1e6, npts / 1e6))
log.close()
