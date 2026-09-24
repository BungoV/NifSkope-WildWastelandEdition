#!/usr/bin/env python3
"""Print the side-by-side verdict table from the compare_*.json files."""
import json
import numpy as np
import os

HERE = os.path.dirname(os.path.abspath(__file__))

GROUPS = []
b = json.load(open(os.path.join(HERE, 'compare_bto.json')))['bto']
GROUPS.append(('FO76 .bto RemeshedShape', [r for r in b if r['kind'] == 'remesh']))
GROUPS.append(('FO76 .bto GlobalAtlasShape', [r for r in b if r['kind'] == 'atlas']))
GROUPS.append(('FO76 .bto water/XWA quads (control)', [r for r in b if r['kind'] == 'other']))
GROUPS.append(('FO76 Meshes/LOD *_lod.nif', json.load(open(os.path.join(HERE, 'compare_f76lod.json')))['f76lod']))
GROUPS.append(('FO4  Meshes/LOD *_LOD.nif (kit)', json.load(open(os.path.join(HERE, 'compare_f4lod.json')))['f4lod']))
GROUPS.append(('FO4  Meshes/LOD Bld**LOD.nif', json.load(open(os.path.join(HERE, 'compare_f4bld.json')))['f4bld']))

ROWS = [
    ('shapes', '_n', ''),
    ('triangles total', '_t', ''),
    ('triangles / shape', 'nt', 'med'),
    ('aspect ratio, median (1=equilateral)', 'AR_median', 'med'),
    ('aspect ratio, p90', 'AR_p90', 'med'),
    ('slivers AR>4  (% of tris)', 'sliver_pct_AR4', 'med'),
    ('slivers AR>10 (% of tris)', 'sliver_pct_AR10', 'med'),
    ('degenerate tris (%)', 'degenerate_pct', 'med'),
    ('normals on a world axis (% of tris)', 'axis_normal_pct', 'med'),
    ('normals on a world axis (% of AREA)', 'axis_normal_areapct', 'med'),
    ('YAW-INVARIANT boxiness (% of tris)', 'yawinv_box_pct', 'med'),
    ('YAW-INVARIANT boxiness (% of AREA)', 'yawinv_box_areapct', 'med'),
    ('edges on a world axis (%)', 'axis_edge_pct', 'med'),
    ('edge length p90/p10', 'edge_p90_over_p10', 'med'),
    ('vertex valence, mean', 'valence_mean', 'med'),
    ('vertex valence <=4 (%)', 'valence_pct_le4', 'med'),
    ('vertex valence entropy (bits)', 'valence_entropy', 'med'),
    ('coords on a 1-unit grid (%)', 'snap_pct_1', 'med'),
    ('connected components in 3D', 'components_3d', 'med'),
    ('UV islands', 'uv_islands', 'med'),
    ('texel density p90/p10', 'texdens_p90_over_p10', 'med'),
]

w = 38
print(' ' * w + ''.join('%14s' % ('g%d' % i) for i in range(len(GROUPS))))
for i, (nm, _) in enumerate(GROUPS):
    print('  g%d = %s' % (i, nm))
print()
for label, key, how in ROWS:
    cells = []
    for nm, rows in GROUPS:
        if key == '_n':
            cells.append('%14d' % len(rows))
        elif key == '_t':
            cells.append('%14d' % sum(r['nt'] for r in rows))
        else:
            v = [r[key] for r in rows if key in r]
            cells.append('%14.2f' % np.median(v) if v else '%14s' % '-')
    print('%-38s%s' % (label, ''.join(cells)))
