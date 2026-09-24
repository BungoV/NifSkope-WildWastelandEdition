"""Why 85 road shape-tiles reported `no-diffuse`: every road shape in the bake's
own cell window, with its shader property, its material name and whether that
material resolves.

  python road_shape_probe.py <refs.json> <dataRoot> <cx0> <cy0> <cx1> <cy1>
"""

import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from gltf_nifread import Nif                                  # noqa: E402
from matinfo import find_material, read_material, shader_info  # noqa: E402
from placements import load                                   # noqa: E402
from rasterlib import MeshCache                               # noqa: E402


def main(argv):
    pl = load(argv[0])
    dataRoot = argv[1]
    cx0, cy0, cx1, cy1 = (int(argv[2]), int(argv[3]), int(argv[4]), int(argv[5]))
    mc = MeshCache(dataRoot)
    models = set()
    for p in pl:
        if p['family'] not in ('road', 'landscape/sidewalks'):
            continue
        # the bake gathers over the region GROWN by two cells
        cx = p['pos'][0] / 4096.0
        cy = p['pos'][1] / 4096.0
        if cx0 - 2 <= cx <= cx1 + 3 and cy0 - 2 <= cy <= cy1 + 3:
            models.add(p['modl'])
    print('road models in the gather window: %d' % len(models))
    kinds = collections.Counter()
    for m in sorted(models):
        path = mc.path_for(m)
        if path is None:
            kinds['model file not found'] += 1
            continue
        nif = Nif(path)
        for sh in nif.shapes.values():
            info = shader_info(nif, sh)
            if info is None:
                kinds['no shader property'] += 1
                continue
            name, f1, f2 = info
            tex = nif.diffuse_for(sh)
            if not name:
                kinds['no material name, texset diffuse %s'
                      % ('yes' if tex else 'NO')] += 1
                continue
            ext = name.lower().rsplit('.', 1)[-1]
            mat = find_material(dataRoot, name)
            if mat is None:
                kinds['material %s NOT FOUND (texset %s)'
                      % (ext, 'yes' if tex else 'NO')] += 1
                continue
            r = read_material(mat)
            kinds['material %s ok kind=%s decal=%s (texset %s)'
                  % (ext, r['kind'] if r else '?', r['decal'] if r else '?',
                     'yes' if tex else 'NO')] += 1
    for k, v in kinds.most_common():
        print('%6d  %s' % (v, k))


if __name__ == '__main__':
    main(sys.argv[1:])
