#!/usr/bin/env python
"""Section 3b: does vanilla bake the ELEVATED road family -- HighwayOverpass
and Bridge, the road pieces that carry a distant-LOD mesh -- into the colour
sheet the way it bakes the flat road surface?

Chunk (-20,-12) is the discriminator: 26 elevated road placements and ZERO
ground road placements, so the two families cannot be confused there.

Same instruments as tile2.py (ROADS1's rasterlib + placements + the displaced
floor + the phase-randomised twin).

  python hw_tile.py <refs.json> <census.json> <dataRoot> <vanDir> <cx> <cy> <out.npz>
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from placements import load                          # noqa: E402
from rasterlib import Grid, MeshCache                # noqa: E402
from flagtable import is_road, comps                 # noqa: E402
from tile2 import (CELL, DIM, N, SHIFTS, auc, phase_twin, project,  # noqa: E402
                   sheet, shift)


def main(argv):
    refs, cenp, dataRoot, vanDir, cx0, cy0, out = (
        argv[0], argv[1], argv[2], argv[3], int(argv[4]), int(argv[5]), argv[6])
    cen = json.load(open(cenp))
    elev, ground, other = set(), set(), set()
    for r in cen['rows']:
        if r['sig'] != 'STAT':
            continue
        f = int(r['formid'], 16)
        if is_road(r):
            c = comps(r['modl'])
            sub = c[2] if len(c) > 2 else ''
            (elev if sub in ('highwayoverpass', 'bridge') else ground).add(f)
        else:
            other.add(f)

    g = Grid(cx0 * CELL, cy0 * CELL, (cx0 + DIM) * CELL, (cy0 + DIM) * CELL, N)
    pl = load(refs)
    mc = MeshCache(dataRoot)
    fams = (('road_elevated', lambda p: p['base'] in elev),
            ('road_ground', lambda p: p['base'] in ground),
            ('nonroad', lambda p: p['base'] in other))
    masks = {}
    for name, test in fams:
        m, nm, nt = project(pl, mc, g, test)
        masks[name] = m
        print('%-16s meshes %5d  triangles %8d  texels %6d (%5.2f%%)'
              % (name, nm, nt, int(m.sum()), 100.0 * m.sum() / (N * N)))

    col = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx0, cy0)))
    lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
    sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)
    print()
    for name, _ in fams:
        m = masks[name]
        if m.sum() == 0:
            print('%-16s absent from this chunk' % name)
            continue
        a, ag = auc(lum, m), auc(-sat, m)
        fl = [auc(lum, shift(m, dx, dy)) for dx, dy in SHIFTS]
        flg = [auc(-sat, shift(m, dx, dy)) for dx, dy in SHIFTS]
        tw = [auc(lum, phase_twin(m, s)) for s in (1, 2, 3)]
        twg = [auc(-sat, phase_twin(m, s)) for s in (1, 2, 3)]
        print('%-16s texels %6d  AUCbr %.3f  floor %.3f..%.3f  twin %.3f..%.3f'
              '   AUCgrey %.3f  floor %.3f..%.3f  twin %.3f..%.3f'
              % (name, int(m.sum()), a, min(fl), max(fl), min(tw), max(tw),
                 ag, min(flg), max(flg), min(twg), max(twg)))
    np.savez_compressed(out, **masks)
    print('wrote', out)


if __name__ == '__main__':
    main(sys.argv[1:])
