"""TILING2 step 0 -- pick the 22 shipped dim-4 sheets, from the MASTER alone.

The rule, written before any number was looked at (`nifskope-ww-vanilla-compare`
section "pick the tile defensibly"):

  1. The two tiles the lane inherits from RESUME3 are in by construction:
     (-20,24) and (-20,20).
  2. Of the 2,304 shipped `Commonwealth.4.*.DDS` colour sheets, a sheet
     QUALIFIES when it is in the LAND population rather than the filler one.
     The discriminator is not invented: the luminance standard deviation of
     mip 3 (64x64, 256 world units a texel) over all 2,304 shipped sheets is
     BIMODAL -- 1,177 sheets under 5.0 (the ocean/void filler: one flat colour
     with a gradient), a trough at 4.5..5.5 (40 and 31 sheets, the two emptiest
     bins of the whole range), and the land population above it. The floor is
     the trough midpoint, 5.25, and `logs/t0_hist.txt` is the histogram it was
     read off. A filler tile has no blend edge and no texture repeat to
     measure, so a periodicity or edge statistic on it measures the codec.
  3. The 20 are then a deterministic 5 x 4 LATTICE over the bounding box of the
     qualifying sheets, each lattice point snapped to the nearest qualifying
     sheet by chunk distance (ties by (cx,cy)), duplicates and the two
     inherited tiles skipped by taking the next-nearest.

Output: `logs/t0_pick.txt` and `sheets.json`.

    python t0_pick.py
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'scratchpad', 'splat1_20260911'))
import splatlib as S                                          # noqa: E402

INHERITED = [(-20, 24), (-20, 20)]
FLOOR_SD = 5.25
NX, NY = 5, 4


def all_coords():
    out = []
    for name in os.listdir(S.VAN):
        if not name.startswith('Commonwealth.4.') or not name.endswith('.DDS'):
            continue
        stem = name[len('Commonwealth.4.'):-len('.DDS')]
        if '_' in stem:
            continue
        a, b = stem.split('.')
        out.append((int(a), int(b)))
    return sorted(out)


def coarse_sd(cx, cy):
    try:
        d = S.Dds(S.van_sheet(cx, cy))
    except Exception as exc:
        return None, str(exc)
    m = min(3, d.maxMip)
    L = S.lum(d.level(m))
    return float(L.std()), 'mip%d %dx%d' % (m, d.levels[m][1], d.levels[m][2])


def main():
    coords = all_coords()
    rows = []
    for (cx, cy) in coords:
        sd, note = coarse_sd(cx, cy)
        rows.append((cx, cy, sd, note))
    qual = [(cx, cy, sd) for (cx, cy, sd, _n) in rows if sd is not None and sd >= FLOOR_SD]
    qset = set((cx, cy) for cx, cy, _s in qual)
    xs = [c[0] for c in qual]
    ys = [c[1] for c in qual]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)

    picked = list(INHERITED)
    lattice = []
    for j in range(NY):
        for i in range(NX):
            lx = x0 + (x1 - x0) * (i + 0.5) / NX
            ly = y0 + (y1 - y0) * (j + 0.5) / NY
            lattice.append((lx, ly))
    for (lx, ly) in lattice:
        cand = sorted(qset - set(picked),
                      key=lambda c: ((c[0] - lx) ** 2 + (c[1] - ly) ** 2, c[0], c[1]))
        if cand:
            picked.append(cand[0])
    # 2 inherited + 20 lattice
    picked = picked[:22]

    lines = []
    lines.append('TILING2 step 0 -- the 22 shipped dim-4 sheets')
    lines.append('shipped dim-4 colour sheets            %d' % len(coords))
    lines.append('qualifying (mip3 luminance SD >= %.2f)  %d' % (FLOOR_SD, len(qual)))
    lines.append('bounding box of the qualifying set     cx %d..%d  cy %d..%d'
                 % (x0, x1, y0, y1))
    lines.append('lattice                                %d x %d over that box' % (NX, NY))
    lines.append('')
    lines.append('%-4s %-12s %8s  %s' % ('#', 'chunk', 'mip3 SD', 'why'))
    sdmap = dict(((cx, cy), sd) for cx, cy, sd, _n in rows)
    for k, (cx, cy) in enumerate(picked):
        why = 'inherited from RESUME3' if (cx, cy) in INHERITED else 'lattice'
        lines.append('%-4d (%4d,%4d) %8.2f  %s' % (k, cx, cy, sdmap[(cx, cy)], why))
    txt = '\n'.join(lines) + '\n'
    print(txt)
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    with open(os.path.join(HERE, 'logs', 't0_pick.txt'), 'w') as f:
        f.write(txt)
    with open(os.path.join(HERE, 'sheets.json'), 'w') as f:
        json.dump(dict(sheets=[list(p) for p in picked],
                       inherited=[list(p) for p in INHERITED],
                       floorSd=FLOOR_SD, nQual=len(qual), nAll=len(coords),
                       bbox=[x0, x1, y0, y1]), f, indent=1)


if __name__ == '__main__':
    main()
