"""Work item 6: decide sidewalks with a number, on a tile that has enough of
them to decide anything.

`lodgenIsRoadModel()` accepts second component `roads` OR `sidewalks`, so today
every kerb and pavement slab under `Landscape\\Sidewalks\\` is painted into the
ground with the roads.  The question is whether vanilla does that, and the only
honest way to ask it is the way the raised-highway question was asked: project
the sidewalk family's own footprint, score it against vanilla's shipped sheet,
and put a displaced floor and a phase twin beside it.

Same instruments as gate4b.py (hw_tile.py's masks, tile2.py's floors, the
tie-averaged AUC), and the SAME TILE, chunk (-8,8) downtown, which is where the
sidewalks are.  The brief requires at least 5,000 sidewalk texels for the tile
to count; the run prints the number first and refuses below it.

usage: sidewalk.py <refs.json> <census.json> <dataRoot> <name=texdir> ...
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'flagscan1_20260911'))
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from placements import load                                   # noqa: E402
from rasterlib import Grid, MeshCache                         # noqa: E402
from flagtable import comps                                   # noqa: E402
from tile2 import (CELL, DIM, N, SHIFTS, phase_twin, project,  # noqa: E402
                   sheet, shift)
from gate4b import auc_tie                                    # noqa: E402

CX0, CY0 = -8, 8
MIN_TEXELS = 5000


def main(argv):
    refs, cenp, dataRoot = argv[0], argv[1], argv[2]
    sheets = [a.split('=', 1) for a in argv[3:]]

    cen = json.load(open(cenp))
    sw, flat, other = set(), set(), set()
    for r in cen['rows']:
        if r['sig'] != 'STAT':
            continue
        f = int(r['formid'], 16)
        c = comps(r['modl'])
        if len(c) >= 3 and c[0] == 'landscape' and c[1] == 'sidewalks':
            sw.add(f)
        elif (len(c) >= 3 and c[0] == 'landscape' and c[1] == 'roads'
              and c[2] not in ('highwayoverpass', 'bridge')
              and not (r['flags'] & 0x8000)):
            flat.add(f)
        else:
            other.add(f)

    g = Grid(CX0 * CELL, CY0 * CELL, (CX0 + DIM) * CELL, (CY0 + DIM) * CELL, N)
    pl = load(refs)
    mc = MeshCache(dataRoot)
    fams = (('sidewalk', lambda p: p['base'] in sw),
            ('road_flat', lambda p: p['base'] in flat),
            ('nonroad', lambda p: p['base'] in other))
    masks = {}
    for name, test in fams:
        m, nm, nt = project(pl, mc, g, test)
        masks[name] = m
        print('%-12s meshes %5d  triangles %8d  texels %6d (%5.2f%%)'
              % (name, nm, nt, int(m.sum()), 100.0 * m.sum() / (N * N)))
    n = int(masks['sidewalk'].sum())
    print('')
    print('sidewalk texels on chunk (%d,%d): %d  (the brief asks for >= %d): %s'
          % (CX0, CY0, n, MIN_TEXELS, 'OK' if n >= MIN_TEXELS else 'TOO FEW'))
    if n < MIN_TEXELS:
        return 1

    fields = {}
    for name, d in sheets:
        col = sheet(os.path.join(d, 'Commonwealth.4.%d.%d.DDS' % (CX0, CY0)))
        lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
        sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)
        fields[name] = (lum, -sat)

    for si, lbl in ((0, 'AUC BRIGHTNESS, tie-averaged (brighter = higher)'),
                    (1, 'AUC GREYNESS, tie-averaged (greyer = higher)')):
        print('')
        print(lbl)
        print('%-12s %7s' % ('family', 'texels')
              + ''.join(' %22s' % nm[:22] for nm, _ in sheets))
        for fam, _ in fams:
            m = masks[fam]
            line = '%-12s %7d' % (fam, int(m.sum()))
            for nm, _ in sheets:
                s = fields[nm][si]
                a = auc_tie(s, m)
                fl = [auc_tie(s, shift(m, dx, dy)) for dx, dy in SHIFTS]
                line += ' %6.3f [%.3f..%.3f]' % (a, min(fl), max(fl))
            print(line)
        line = '%-12s %7s' % ('twins (sw)', '')
        for nm, _ in sheets:
            s = fields[nm][si]
            tw = [auc_tie(s, phase_twin(masks['sidewalk'], k)) for k in (1, 2, 3)]
            line += ' %22s' % ('%.3f..%.3f' % (min(tw), max(tw)))
        print(line)

    print('')
    print('CLEARANCE above the displaced floor top (tie-averaged brightness)')
    print('%-12s' % 'family' + ''.join(' %11s' % nm[:11] for nm, _ in sheets))
    for fam, _ in fams:
        m = masks[fam]
        line = '%-12s' % fam
        for nm, _ in sheets:
            s = fields[nm][0]
            fl = max(auc_tie(s, shift(m, dx, dy)) for dx, dy in SHIFTS)
            line += ' %+11.3f' % (auc_tie(s, m) - fl)
        print(line)

    print('')
    print('MEAN LUMINANCE and MEAN ABSOLUTE ERROR AGAINST VANILLA')
    van = fields['vanilla'][0]
    for fam, _ in fams:
        m = masks[fam]
        line = '%-12s %7d  lum' % (fam, int(m.sum()))
        for nm, _ in sheets:
            line += ' %7.2f' % (fields[nm][0][m].mean() * 255.0)
        line += '   |err|'
        for nm, _ in sheets:
            line += ' %7.2f' % (np.abs(fields[nm][0][m] - van[m]).mean() * 255.0)
        print(line)

    np.savez_compressed(os.path.join(HERE, 'sidewalk_masks.npz'), **masks)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
