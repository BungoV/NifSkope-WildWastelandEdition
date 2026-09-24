"""ROADS1 section 1, part 5: the DECAL footprint, per SHAPE.

`bDecal` is a material property, so it belongs to a SHAPE, not to a model: a
rock's ground-blending skirt is a decal shape inside a rock.  This projects
only the decal shapes, road-family models excluded, and measures them against
vanilla's sheet the same way the road footprint was measured.

  python measure2.py <refs.json> <dataRoot> <vanillaDir> <cx> <cy> <out.npz>
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from gltf_nifread import Nif                                    # noqa: E402
from lodgen_terrain_model import Dds                            # noqa: E402
from matinfo import (F4SF1_DECAL, F4SF1_DYNAMIC_DECAL,          # noqa: E402
                     find_material, read_material, shader_info)
from placements import load                                     # noqa: E402
from rasterlib import Grid, MeshCache, rasterise                # noqa: E402

CELL = 4096.0
DIM = 4
N = 512


class DecalCache(object):
    def __init__(self, dataRoot, mc):
        self.dataRoot, self.mc, self.cache = dataRoot, mc, {}

    def decal_shapes(self, modl):
        key = (modl or '').lower()
        if key in self.cache:
            return self.cache[key]
        path = self.mc.path_for(modl)
        out = set()
        if path:
            try:
                nif = Nif(path)
                for idx, sh in nif.shapes.items():
                    info = shader_info(nif, sh)
                    if info is None:
                        continue
                    name, f1, f2 = info
                    mat = find_material(self.dataRoot, name) if name else None
                    if mat:
                        m = read_material(mat)
                        if m and m['decal']:
                            out.add(sh['name'])
                    elif f1 & (F4SF1_DECAL | F4SF1_DYNAMIC_DECAL):
                        out.add(sh['name'])
            except Exception:
                pass
        self.cache[key] = out
        return out


def main(argv):
    refs, dataRoot, vanDir, cx0, cy0, out = (argv[0], argv[1], argv[2],
                                             int(argv[3]), int(argv[4]), argv[5])
    g = Grid(cx0 * CELL, cy0 * CELL, (cx0 + DIM) * CELL, (cy0 + DIM) * CELL, N)
    pl = load(refs)
    mc = MeshCache(dataRoot)
    dc = DecalCache(dataRoot, mc)
    zbuf = np.full((N, N), -1e30)
    idbuf = np.zeros((N, N), dtype=np.int32)
    nm = nt = 0
    for p in pl:
        if p['family'] in ('road', 'landscape/sidewalks'):
            continue
        names = dc.decal_shapes(p['modl'])
        if not names:
            continue
        hit = False
        for sh in mc.get(p['modl']):
            if sh['name'] not in names:
                continue
            v = (sh['v'] * p['scale']).dot(p['R'].T) + p['pos']
            if v[:, 0].max() < g.wx0 or v[:, 0].min() > g.wx1:
                continue
            if v[:, 1].max() < g.wy0 or v[:, 1].min() > g.wy1:
                continue
            q = g.to_texel(v)[sh['t']]
            z = v[sh['t']][:, :, 2]
            keep = ((q[:, :, 0].max(1) >= 0) & (q[:, :, 0].min(1) < N) &
                    (q[:, :, 1].max(1) >= 0) & (q[:, :, 1].min(1) < N))
            if not keep.any():
                continue
            rasterise(g, q[keep], z[keep], zbuf, idbuf, 1)
            nt += int(keep.sum())
            hit = True
        if hit:
            nm += 1
    m = idbuf > 0
    print('decal shapes: placements %d  triangles %d  texels %d (%.2f%%)'
          % (nm, nt, int(m.sum()), 100.0 * m.sum() / (N * N)))
    np.savez_compressed(out, decal=m)
    json.dump({'placements': nm, 'triangles': nt, 'texels': int(m.sum())},
              open(out.replace('.npz', '.json'), 'w'), indent=1)


if __name__ == '__main__':
    main(sys.argv[1:])
