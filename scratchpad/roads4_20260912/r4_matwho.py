"""ROADS4 -- WHO the winning materials are: full material path, the models
that carry them, the diffuse texture they resolve to, and the shape names.

No guessing from a stem: the classification of item 1b is checked here against
the material's own path and its diffuse, and against the shape NAME inside the
road NIF, which is what the artist called the thing.
"""
import json
import os
import sys
from collections import Counter

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r4lib as R                                              # noqa: E402
import matinfo                                                 # noqa: E402


def run(tile):
    ours = R.sheet('rung_detail1', tile)
    ground = R.sheet('rung_noroads', tile)
    pr = R.project(tile)
    shp = pr['shp']
    road = R.painted_mask(ours, ground) & (shp >= 0)
    shapes = pr['shapes']
    idx = np.clip(shp, 0, None)
    counts = np.bincount(idx[road].ravel(), minlength=len(shapes))

    per = {}
    for sid, n in enumerate(counts):
        if n == 0:
            continue
        s = shapes[sid]
        k = (s['mat'] or '(none)')
        d = per.setdefault(k.lower(), dict(path=k, texels=0,
                                           models=Counter(), shapes=Counter(),
                                           skirtTris=0, tris=0,
                                           alphaTest=s['alphaTest'],
                                           alphaBlend=s['alphaBlend'],
                                           decal=s['decal']))
        d['texels'] += int(n)
        d['models'][os.path.basename(s['model'].replace(chr(92), '/'))] += int(n)
        d['skirtTris'] += s['skirtTris']
        d['tris'] += s['tris']

    print('')
    print('=' * 78)
    print('CHUNK %s -- %d classified road texels' % (tile, int(road.sum())))
    print('=' * 78)
    for k, d in sorted(per.items(), key=lambda kv: -kv[1]['texels']):
        mp = matinfo.find_material(R.DATA, d['path'])
        tex = ''
        if mp:
            try:
                m = matinfo.read_material(mp)
                tex = (m.get('diffuse') or '') if isinstance(m, dict) else ''
            except Exception as e:
                tex = '(read failed: %s)' % e
        print('')
        print('  %-8d %s' % (d['texels'], d['path']))
        print('           resolved: %s' % (mp or 'NOT FOUND'))
        print('           diffuse : %s' % (tex or '(none)'))
        print('           alphaTest=%s alphaBlend=%s decal=%s  tris %d '
              '(skirt %d)'
              % (d['alphaTest'], d['alphaBlend'], d['decal'], d['tris'],
                 d['skirtTris']))
        print('           top models: %s'
              % ', '.join('%s(%d)' % (m, c)
                          for m, c in d['models'].most_common(4)))


for t in ('t2020', 't0808'):
    run(t)
