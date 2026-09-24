#!/usr/bin/env python3
"""Freeze the third witness into a TRACKED gate fixture.

The ten receivers and their skylines are DATA, computed from the raw BTD
heightmap and the .lodi/.lodo placement boxes by this lane's own chain
(boxes.py -> pick10.py -> wit.py -> table.py). Nothing in the fixture came from
`src/lodghorizon.h`, which is the whole point: a gate scored against a witness
that shares the code cannot fail when the code is wrong, and that is exactly how
the 51.69% shipped.
"""
import json
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
import fields
from wit import true_skyline

A = 16
AZB = np.array([b * (360.0 / A) for b in range(A)])
land, ground, sky, ter0 = fields.build(verbose=False)
B = np.load(L + '/boxes.npy')
rows = json.load(open(L + '/table.json'))

out = {
    'what': 'the third witness for the .lodt role-7 terrain horizon sheet: ten receivers'
            ' on chunk 4.4.-12 of Commonwealth, and the skyline a 1-degree pencil ray finds'
            ' over the RAW inputs at each of the 16 stored bin azimuths.',
    'how': 'BTD heightmap read bilinear at the ray\'s own position + every .lodi/.lodo placement'
           ' as an exact world box (2,449 of them). No lattice, no mip, no maxAlong, no 2x2 tap,'
           ' no near-skip, no growth ladder. Written by scratchpad/horizon2_20260918/make_witness.py'
           ' from boxes.py / pick10.py / wit.py / table.py, lane HORIZON2, 2026-09-18.',
    'why': 'the in-bake reference (--horizon-refute) calls LodgenHorizonField::maxAlong, the same'
           ' function the march calls, so it moves when the march moves and cannot be the only'
           ' witness a floor is scored against.',
    'azimuths': A,
    'binAzimuthDeg': [float(a) for a in AZB],
    'worldspace': '3C', 'chunk': '4.4.-12', 'region': '4,-12,7,-9',
    'positionsAre': 'the .lodt texel CENTRE the bake cast from (upt 32 u), not the nominal pick',
    'receivers': [],
}
for r in rows:
    d = true_skyline(land, r['x'], r['y'], r['gzLattice'] + 4.0, AZB, boxes=B,
                     skip_containing=bool(r['inBoxes']))
    out['receivers'].append({
        'id': r['id'], 'class': r['class'],
        'x': round(float(r['x']), 3), 'y': round(float(r['y']), 3),
        'groundZ': round(float(r['gzLattice']), 3), 'insideBoxes': int(r['inBoxes']),
        'trueDirDeg': [round(float(v), 3) for v in d],
        'trueSectorMaxDeg': [round(float(v), 3) for v in (r['TRUEX'] if r['inBoxes'] else r['TRUE'])],
        'terrainOnlyDeg': [round(float(v), 3) for v in r['TER']],
    })
p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_horizon_witness.json'
s = json.dumps(out, indent=1) + '\n'
open(p, 'wb').write(s.encode('utf-8'))
print('wrote %s, %d bytes, CRLF %d of %d LF' % (p, len(s), s.count('\r\n'), s.count('\n')))
