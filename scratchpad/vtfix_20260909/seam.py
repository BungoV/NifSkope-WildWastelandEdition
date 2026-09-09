#!/usr/bin/env python
"""Which of the two boundary normals is RIGHT?

The ground is continuous, so the terrain normal is too. Two chunk sheets that
meet at a chunk seam hold texels 32 world units apart -- the same spacing as any
two adjacent columns inside a sheet. So the invariant is:

    the step ACROSS the seam should be the same size as the step one texel IN.

CONTROL (the floor): the interior step, columns 510|511 of the west sheet and
0|1 of the east sheet, on the SAME bake. It carries the terrain's own amplitude
through the same encoder, so it is what "no discontinuity" reads as here.

Reported for the assembled (ringed) set and the direct (clamped) set, on the
colour sheet too, since the colour is what V9a compares.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ddsdiff import load_mip0

HERE = os.path.dirname(os.path.abspath(__file__))
B = os.path.join(HERE, 'bake')

def sheet(run, stem, sfx=''):
    return os.path.join(B, run, 'tex', stem + sfx + '.DDS')

def col(rgb, w, x):
    return [rgb[y * w + x] for y in range(w)]

def row(rgb, w, y):
    return [rgb[y * w + x] for x in range(w)]

def mad(a, b):
    return sum(max(abs(p[k] - q[k]) for k in range(3)) for p, q in zip(a, b)) / float(len(a))

WEST, EAST = 'Commonwealth.4.-24.24', 'Commonwealth.4.-20.24'
SOUTH, NORTH = 'Commonwealth.4.-24.24', 'Commonwealth.4.-24.28'

for sfx, name in (('_msn', 'model-space normal'), ('', 'colour')):
    print('== %s ==' % name)
    for run in ('vt_cover', 'dir_cover'):
        rw, _, w, _, _, _ = load_mip0(sheet(run, WEST, sfx))
        re_, _, _, _, _, _ = load_mip0(sheet(run, EAST, sfx))
        rs, _, _, _, _, _ = load_mip0(sheet(run, SOUTH, sfx))
        rn, _, _, _, _, _ = load_mip0(sheet(run, NORTH, sfx))
        # east-west seam: west sheet's last column | east sheet's first column
        seam_x = mad(col(rw, w, w - 1), col(re_, w, 0))
        # the control is the TYPICAL adjacent-column step well inside the sheet,
        # not the step one texel in: the clamped bake's own edge column is
        # inside the defect, so using it would put the defect in the control
        ctl_x = sum(mad(col(r, w, x), col(r, w, x + 1))
                    for r in (rw, re_) for x in (100, 200, 300, 400)) / 8.0
        edge_x = (mad(col(rw, w, w - 2), col(rw, w, w - 1))
                  + mad(col(re_, w, 0), col(re_, w, 1))) / 2.0
        # north-south seam: row 0 is NORTH, so the south chunk's row 0 meets the
        # north chunk's last row
        seam_y = mad(row(rs, w, 0), row(rn, w, w - 1))
        ctl_y = sum(mad(row(r, w, y), row(r, w, y + 1))
                    for r in (rs, rn) for y in (100, 200, 300, 400)) / 8.0
        edge_y = (mad(row(rs, w, 0), row(rs, w, 1))
                  + mad(row(rn, w, w - 2), row(rn, w, w - 1))) / 2.0
        print('  %-10s E/W seam %6.3f interior %6.3f ratio %5.2f (edge step %6.3f)  |  '
              'N/S seam %6.3f interior %6.3f ratio %5.2f (edge step %6.3f)'
              % (run, seam_x, ctl_x, seam_x / ctl_x if ctl_x else 0, edge_x,
                 seam_y, ctl_y, seam_y / ctl_y if ctl_y else 0, edge_y))
