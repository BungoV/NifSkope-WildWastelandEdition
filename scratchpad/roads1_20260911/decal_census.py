"""Which placements in the Sanctuary window are DECALS, by the material's own
`bDecal` (or, where a shape names no material, the shader property's
Fallout4ShaderPropertyFlags1 Decal / Dynamic_Decal bit) -- never by the word
"decal" in a path.

  python decal_census.py <refs.json> <dataRoot>
"""

import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from matinfo import (F4SF1_DECAL, F4SF1_DYNAMIC_DECAL, find_material,   # noqa: E402
                     read_material, shader_info)
from placements import load                                            # noqa: E402
from rasterlib import MeshCache                                        # noqa: E402
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from gltf_nifread import Nif                                           # noqa: E402


def classify(nifPath, dataRoot, cache):
    """(any decal shape, shapes, decal shapes, [reason]) for one model."""
    if nifPath in cache:
        return cache[nifPath]
    try:
        nif = Nif(nifPath)
    except Exception as e:
        cache[nifPath] = (False, 0, 0, ['unreadable: %s' % e])
        return cache[nifPath]
    n = d = 0
    why = []
    for sh in nif.shapes.values():
        n += 1
        info = shader_info(nif, sh)
        if info is None:
            why.append('no shader property')
            continue
        name, f1, f2 = info
        mat = find_material(dataRoot, name) if name else None
        if mat:
            m = read_material(mat)
            if m is None:
                why.append('material unreadable: %s' % name)
                continue
            if m['decal']:
                d += 1
                why.append('bDecal in %s' % os.path.basename(mat))
            continue
        if f1 & (F4SF1_DECAL | F4SF1_DYNAMIC_DECAL):
            d += 1
            why.append('shader flag Decal, no material file')
        elif name:
            why.append('material not found: %s' % name)
    cache[nifPath] = (d > 0, n, d, why[:3])
    return cache[nifPath]


def main(argv):
    pl = load(argv[0])
    dataRoot = argv[1]
    mc = MeshCache(dataRoot)
    cache = {}
    hits = collections.Counter()
    unresolved = collections.Counter()
    models = {}
    for p in pl:
        path = mc.path_for(p['modl'])
        if path is None:
            unresolved['model file not found'] += 1
            continue
        isDecal, n, d, why = classify(path, dataRoot, cache)
        models[p['modl'].lower()] = (isDecal, n, d, why)
        if isDecal:
            hits[p['modl'].lower()] += 1
    print('distinct models examined: %d' % len(models))
    print('placements whose model has at least one DECAL shape: %d over %d models'
          % (sum(hits.values()), len(hits)))
    for k, v in hits.most_common(30):
        print('%6d  %-58s %s' % (v, k, models[k][3][0] if models[k][3] else ''))
    bad = [(k, v[3]) for k, v in models.items() if v[1] == 0 or
           any('unreadable' in w or 'not found' in w for w in v[3])]
    print('models with a refusal to report: %d' % len(bad))
    for k, w in bad[:10]:
        print('   %-58s %s' % (k, w))


if __name__ == '__main__':
    main(sys.argv[1:])
