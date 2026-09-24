"""Gate S3, work item 4: on chunk (-8,8), does the elevated road family still
paint into our colour sheet, and does the flat road still clear its floor?

FLAGSCAN1 measured vanilla with hw_tile.py and left the refuter written down:
"bake (-8,8) with the shipped rule and score our own sheet's elevated-road
footprint the same way".  This is that refuter, run on four sheets at once so
the masks, the displaced floors and the phase twins are computed ONCE and every
column is the same instrument:

  vanilla   Bethesda's own Commonwealth.4.-8.8.DDS
  rung      the exe this lane started from (max-z, raised INCLUDED)
  raised    the new exe with --road-raised (the way back, raised INCLUDED)
  blend     the new exe at its defaults (blend + detail 0, raised EXCLUDED)

Masks, floors and twins are hw_tile.py's/tile2.py's, unchanged and imported,
not re-typed.  ww-spec-gate-audit: gate4_audit.py first established that on THIS
tile hw_tile.py's elevated split (folder clause only) and the shipped C++
refusal (hasLod OR folder) cover the same 32 bases and 97 placements, so the
two halves of the gate are over the same domain.

usage: gate4.py <refs.json> <census.json> <dataRoot> <name=texdir> ...
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
FS = os.path.join(HERE, '..', 'flagscan1_20260911')
sys.path.insert(0, FS)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from placements import load                                   # noqa: E402
from rasterlib import Grid, MeshCache                         # noqa: E402
from flagtable import is_road, comps                          # noqa: E402
from tile2 import (CELL, DIM, N, SHIFTS, auc, phase_twin,      # noqa: E402
                   project, sheet, shift)

CX0, CY0 = -8, 8


def main(argv):
    refs, cenp, dataRoot = argv[0], argv[1], argv[2]
    sheets = [a.split('=', 1) for a in argv[3:]]

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

    g = Grid(CX0 * CELL, CY0 * CELL, (CX0 + DIM) * CELL, (CY0 + DIM) * CELL, N)
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

    fields = {}
    for name, d in sheets:
        col = sheet(os.path.join(d, 'Commonwealth.4.%d.%d.DDS' % (CX0, CY0)))
        lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
        sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)
        fields[name] = (lum, sat)

    for score, lbl in ((0, 'AUC brightness (brighter = higher)'),
                       (1, 'AUC greyness (greyer = higher)')):
        print('')
        print(lbl)
        hdr = '%-16s %7s' % ('family', 'texels')
        for n, _ in sheets:
            hdr += ' %22s' % n[:22]
        print(hdr)
        for fam, _ in fams:
            m = masks[fam]
            line = '%-16s %7d' % (fam, int(m.sum()))
            for n, _ in sheets:
                lum, sat = fields[n]
                s = lum if score == 0 else -sat
                a = auc(s, m)
                fl = [auc(s, shift(m, dx, dy)) for dx, dy in SHIFTS]
                line += ' %6.3f [%.3f..%.3f]' % (a, min(fl), max(fl))
            print(line)
        line = '%-16s %7s' % ('(phase twins)', '')
        for n, _ in sheets:
            lum, sat = fields[n]
            s = lum if score == 0 else -sat
            tw = [auc(s, phase_twin(masks['road_ground'], k)) for k in (1, 2, 3)]
            line += ' %14s' % ('gnd %.3f..%.3f' % (min(tw), max(tw)))
        print(line)

    print('')
    print('MEAN LUMINANCE per family (the raw field, no ranking)')
    hdr = '%-16s' % 'family'
    for n, _ in sheets:
        hdr += ' %11s' % n[:11]
    print(hdr)
    for fam, _ in fams:
        m = masks[fam]
        line = '%-16s' % fam
        for n, _ in sheets:
            line += ' %11.2f' % fields[n][0][m].mean()
        print(line)
    line = '%-16s' % 'elev MINUS gnd'
    for n, _ in sheets:
        lum = fields[n][0]
        line += ' %11.2f' % (lum[masks['road_elevated']].mean()
                             - lum[masks['road_ground']].mean())
    print(line)

    print('')
    print('MEAN ABSOLUTE LUMINANCE ERROR AGAINST VANILLA')
    van = fields['vanilla'][0]
    for fam, _ in fams:
        m = masks[fam]
        line = '%-16s %7d' % (fam, int(m.sum()))
        for n, _ in sheets:
            line += ' %11.3f' % np.abs(fields[n][0][m] - van[m]).mean()
        print(line)

    np.savez_compressed(os.path.join(HERE, 'gate4_masks.npz'), **masks)


if __name__ == '__main__':
    main(sys.argv[1:])
