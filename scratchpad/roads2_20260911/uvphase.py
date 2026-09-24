"""Section 1d: the decisive discriminator for the banding.

Per road texel, keep the winning triangle's interpolated TEXTURE COORDINATE,
taken mod 1 -- i.e. where inside the asphalt texture's own repeat that texel
lands.  Then ask how much of the sheet's luminance variance on the road is
EXPLAINED by that phase alone (a 16x16 table of bin means; R-squared).

  * if our sheet's road luminance is a function of the texture phase, the bake
    is printing the texture's own pattern at the repeat's period -- 256 world
    units, 8 bake texels, which is the band spacing the picture shows;
  * vanilla's sheet is the CEILING for how much phase dependence is correct;
  * our own --no-roads bake is the FLOOR: the bare ground cannot know anything
    about a road mesh's UVs, so whatever R-squared it scores is what this
    estimator returns on noise with the same mask and the same bin count.

usage: uvphase.py <ourColour.DDS> <ourNoRoads.DDS> [tag]
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from roads2lib import Dds, Nif                              # noqa: E402
from rasterlib import MeshCache, Grid, rasterise, local_to_model  # noqa: E402
import placements                                           # noqa: E402
import seam                                                 # noqa: E402

N = seam.N
CELL = seam.CELL
NB = 16


def project_uv():
    wx0, wy1 = seam.CX0 * CELL, (seam.CY0 + 4) * CELL
    grid = Grid(wx0, wy1 - 4 * CELL, wx0 + 4 * CELL, wy1, N)
    pl = [p for p in placements.load(seam.REFS)
          if seam.is_roadish(p['sig'], p['modl'])]
    mc = MeshCache(seam.DATA)
    zbuf = np.full((N, N), -1e30)
    idbuf = np.full((N, N), -1, dtype=np.int32)
    abuf = [np.zeros((N, N)), np.zeros((N, N))]     # u, v
    cache = {}
    for pi, p in enumerate(pl):
        full = mc.path_for(p['modl'])
        if full is None:
            continue
        key = full.lower()
        if key not in cache:
            try:
                cache[key] = Nif(full)
            except Exception:
                cache[key] = None
        nif = cache[key]
        if nif is None:
            continue
        for idx, sh in nif.shapes.items():
            if not sh['verts'] or not sh['tris'] or not sh['uvs']:
                continue
            R, t, s = local_to_model(nif, sh)
            v = (np.array(sh['verts'], dtype=np.float64) * s).dot(R.T) + t
            v = (v * p['scale']).dot(p['R'].T) + p['pos']
            uv = np.array(sh['uvs'], dtype=np.float64)
            tri = np.array(sh['tris'], dtype=np.int32).reshape(-1, 3)
            pts = grid.to_texel(v)[tri]
            zz = v[tri][:, :, 2]
            attrs = uv[tri]                              # (T,3,2)
            rasterise(grid, pts, zz, zbuf, idbuf, pi, attrs=attrs, abuf=abuf)
    return idbuf, abuf[0], abuf[1]


def r2_by_phase(L, mask, u, v):
    """Fraction of the masked field's variance explained by a NB x NB table of
    means indexed by (frac(u), frac(v))."""
    x = L[mask]
    if x.size < NB * NB * 4:
        return float('nan')
    iu = np.minimum((np.mod(u[mask], 1.0) * NB).astype(int), NB - 1)
    iv = np.minimum((np.mod(v[mask], 1.0) * NB).astype(int), NB - 1)
    key = iu * NB + iv
    tot = x.var()
    fit = np.zeros_like(x)
    for k in np.unique(key):
        m = key == k
        fit[m] = x[m].mean()
    return float(1.0 - ((x - fit) ** 2).mean() / max(tot, 1e-12))


def main():
    ours = Dds(sys.argv[1]).lum()
    nor = Dds(sys.argv[2]).lum()
    tag = sys.argv[3] if len(sys.argv) > 3 else 'maxz'
    van = Dds(seam.VAN).lum()
    cache = os.path.join(HERE, 'uv_m20_20.npz')
    if os.path.isfile(cache):
        z = np.load(cache)
        idbuf, uu, vv = z['idbuf'], z['u'], z['v']
        print('UV projection loaded from %s' % cache)
    else:
        idbuf, uu, vv = project_uv()
        np.savez_compressed(cache, idbuf=idbuf, u=uu, v=vv)
        print('UV projection written to %s' % cache)
    road = idbuf >= 0
    print('road texels %d' % int(road.sum()))
    print('')
    print('VARIANCE OF THE ROAD LUMINANCE EXPLAINED BY THE TEXTURE PHASE '
          '(%dx%d bins)' % (NB, NB))
    rows = {}
    for name, L in (('vanilla (the ceiling)', van), ('ours ' + tag, ours),
                    ('ours --no-roads (the floor)', nor)):
        r = r2_by_phase(L, road, uu, vv)
        rows[name] = r
        print('   %-30s R2 = %.4f   (SD on the road %6.3f)'
              % (name, r, float(L[road].std())))
    # a second floor: the phase map rolled, which keeps its distribution and
    # destroys its registration with the sheet
    for dj, di in ((37, 0), (0, 37), (53, -29)):
        r = r2_by_phase(ours, road, np.roll(np.roll(uu, dj, 0), di, 1),
                        np.roll(np.roll(vv, dj, 0), di, 1))
        print('   %-30s R2 = %.4f' % ('ours, phase map rolled %+d%+d' % (dj, di), r))
    print('')
    print('LOCAL 5x5 SD, the number a fix has to land on')
    for name, L in (('vanilla', van), ('ours ' + tag, ours), ('no-roads', nor)):
        loc = np.array([[L[max(0, j - 2):j + 3, max(0, i - 2):i + 3].std()
                         for i in range(N)] for j in range(N)])
        print('   %-16s road %6.3f   off-road %6.3f' % (name, loc[road].mean(),
                                                        loc[~road].mean()))
    json.dump(rows, open(os.path.join(HERE, 'uvphase_%s.json' % tag), 'w'),
              indent=1)


if __name__ == '__main__':
    main()
