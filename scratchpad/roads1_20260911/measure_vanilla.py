"""ROADS1 section 1: WHAT DOES VANILLA BAKE INTO THE FAR TERRAIN SHEETS?

Measured on Bethesda's own shipped chunk sheets, with our own placement walk
projected top-down onto the same texel grid.  No generator code is involved and
no rasteriser has been written yet.

  python measure_vanilla.py <refs.json> <dataRoot> <vanillaDir> <cx> <cy> <out.npz>
"""

import collections
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                # noqa: E402
from placements import load                          # noqa: E402
from rasterlib import Grid, MeshCache, rasterise     # noqa: E402

CELL = 4096.0
DIM = 4
N = 512

GROUPS = collections.OrderedDict((
    ('road', lambda f: f in ('road', 'landscape/sidewalks')),
    ('trees', lambda f: f == 'landscape/trees'),
    ('rocks', lambda f: f in ('landscape/rocks', 'landscape/dirtcliffs',
                              'landscape/erodedcliffs')),
    ('buildings', lambda f: f in ('architecture',)),
    ('setdressing', lambda f: f == 'setdressing'),
))


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    a = np.array(px, dtype=np.float32).reshape(h, w, 4)
    return a, t.fourcc.decode('latin-1'), w, h


def project(pl, mc, g, test):
    zbuf = np.full((g.n, g.n), -1e30, dtype=np.float64)
    idbuf = np.zeros((g.n, g.n), dtype=np.int32)
    zval = np.zeros((g.n, g.n), dtype=np.float64)
    nmesh = ntri = 0
    for p in pl:
        if not test(p['family']):
            continue
        shapes = mc.get(p['modl'])
        hit = False
        for sh in shapes:
            v = (sh['v'] * p['scale']).dot(p['R'].T) + p['pos']
            if v[:, 0].max() < g.wx0 or v[:, 0].min() > g.wx1:
                continue
            if v[:, 1].max() < g.wy0 or v[:, 1].min() > g.wy1:
                continue
            tx = g.to_texel(v)
            tri = sh['t']
            q = tx[tri]
            z = v[tri][:, :, 2]
            keep = ((q[:, :, 0].max(1) >= 0) & (q[:, :, 0].min(1) < g.n) &
                    (q[:, :, 1].max(1) >= 0) & (q[:, :, 1].min(1) < g.n))
            if not keep.any():
                continue
            rasterise(g, q[keep], z[keep], zbuf, idbuf, 1)
            ntri += int(keep.sum())
            hit = True
        if hit:
            nmesh += 1
    zval = np.where(zbuf > -1e29, zbuf, np.nan)
    return (idbuf > 0), zval, nmesh, ntri


def main(argv):
    refs, dataRoot, vanDir, cx0, cy0, out = (argv[0], argv[1], argv[2],
                                             int(argv[3]), int(argv[4]), argv[5])
    g = Grid(cx0 * CELL, cy0 * CELL, (cx0 + DIM) * CELL, (cy0 + DIM) * CELL, N)
    pl = load(refs)
    mc = MeshCache(dataRoot)
    masks, zs, stats = {}, {}, {}
    for name, test in GROUPS.items():
        m, z, nm, nt = project(pl, mc, g, test)
        masks[name] = m
        zs[name + '_z'] = z
        stats[name] = dict(meshes=nm, triangles=nt, texels=int(m.sum()))
        print('%-12s meshes %5d  triangles %7d  texels %6d (%5.2f%%)'
              % (name, nm, nt, int(m.sum()), 100.0 * m.sum() / (N * N)))
    if mc.missing:
        print('models that would not load: %d' % len(mc.missing))
        for k, v in list(mc.missing.items())[:8]:
            print('   %s -- %s' % (k, v))
    np.savez_compressed(out, **dict(list(masks.items()) + list(zs.items())))
    json.dump({'stats': stats, 'chunk': [cx0, cy0]},
              open(out.replace('.npz', '.json'), 'w'), indent=1)
    print('wrote', out)


if __name__ == '__main__':
    main(sys.argv[1:])
