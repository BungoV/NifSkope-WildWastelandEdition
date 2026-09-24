#!/usr/bin/env python3
"""Lane CARDS-AGG -- the third picture, from the .lodi BYTES.

The brief asks for a render-hook far view of the region with per-tree cards
beside aggregate cards. **That picture cannot be taken in this tree and the
refusal is the honest answer**: an aggregate is DATA, not geometry. Nothing in
NifSkope draws a `.lodi` aggregate row -- the reconstruction path is FO4CS's and
does not exist yet (the same state lane NATIVE1b reported for the cluster
ladder: "Nothing in FO4CS reads either file, so the pictures are the decoder's
own geometry"). A render of "the aggregate cards" would therefore be a render of
something this lane invented for the photograph, which is worse than no picture.

What CAN be shown, and is: the region from above, every cell that holds trees,
which of them the aggregate swallowed, how many trees each one removed from the
far band, and the cells it left alone -- all read back from the `.lodi` with the
independent decoder, beside the ESM census taken before the code existed.
"""

import csv
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aggcheck import read_lodi   # noqa: E402

INK = (20, 20, 20)


def main():
    lodi_path, census, out = sys.argv[1], sys.argv[2], sys.argv[3]
    thr = int(sys.argv[4]) if len(sys.argv) > 4 else 8
    reg = [int(v) for v in sys.argv[5].split(',')] if len(sys.argv) > 5 else None
    h = read_lodi(lodi_path)
    agg = {tuple(a['cell']): a for a in h['aggregates']}
    trees = {}
    with open(census, newline='') as f:
        for r in csv.DictReader(f):
            trees[(int(r['cx']), int(r['cy']))] = int(r['trees'])

    x0, x1 = h['west'] * 4, h['east'] * 4 + 3
    y0, y1 = h['south'] * 4, h['north'] * 4 + 3
    cells = [c for c in trees if x0 <= c[0] <= x1 and y0 <= c[1] <= y1]
    S = 46
    W = max((x1 - x0 + 1) * S + 40, 1000)
    H = (y1 - y0 + 1) * S + 150
    im = Image.new('RGB', (W, H), (250, 250, 250))
    d = ImageDraw.Draw(im)
    covered = sum(a['coveredCount'] for a in h['aggregates'])
    d.text((14, 12), 'AGGREGATE COVERAGE OF THE REGION, read back from %s'
           % os.path.basename(lodi_path), fill=INK)
    d.text((14, 28), 'cells %d..%d x %d..%d; %d cells hold trees; %d are forested at >= %d and '
           'carry an aggregate row; %d trees covered'
           % (x0, x1, y0, y1, len(cells), len(agg), thr, covered), fill=INK)
    d.text((14, 44), 'GREEN = an aggregate, its number is the trees it removes from the far band.  '
           'GREY = trees but under the threshold.  WHITE = no tree.', fill=INK)
    d.text((14, 60), 'north is UP, the same row order the file uses. Every number here is the '
           'file\'s own coveredCount, not the bake\'s log.', fill=INK)
    if reg:
        regtxt = '%d..%d x %d..%d' % (reg[0], reg[2], reg[1], reg[3])
        d.text((14, 76), 'BLUE HATCHING = OUTSIDE the baked region ' + regtxt +
               ': the .lodi chunk extent reaches past it because the chunk builder passes the',
               fill=(50, 80, 170))
        d.text((14, 92), '        emitter placements from the neighbouring cells. Those cells were '
               'never offered to the aggregate; their numbers are the ESM census.',
               fill=(50, 80, 170))
    for cy in range(y0, y1 + 1):
        for cx in range(x0, x1 + 1):
            px = 20 + (cx - x0) * S
            py = 118 + (y1 - cy) * S
            n = trees.get((cx, cy), 0)
            a = agg.get((cx, cy))
            if a:
                k = min(255, 60 + a['coveredCount'] * 3)
                fill = (40, k, 40)
                d.rectangle([px, py, px + S - 2, py + S - 2], fill=fill, outline=(20, 90, 20))
                d.text((px + 6, py + S // 2 - 6), str(a['coveredCount']),
                       fill=(255, 255, 255))
            elif n:
                d.rectangle([px, py, px + S - 2, py + S - 2], fill=(205, 205, 205),
                            outline=(150, 150, 150))
                d.text((px + 8, py + S // 2 - 6), str(n), fill=(70, 70, 70))
            else:
                d.rectangle([px, py, px + S - 2, py + S - 2], fill=(252, 252, 252),
                            outline=(225, 225, 225))
            if reg and not (reg[0] <= cx <= reg[2] and reg[1] <= cy <= reg[3]):
                # OUTSIDE the baked region: the .lodi's chunk extent reaches
                # past it because the chunk builder's skirt hands the emitter
                # placements from the neighbouring cells. Hatched, so a cell
                # with trees and no aggregate is not read as a defect.
                for k in range(0, S, 6):
                    d.line([(px + k, py), (px, py + k)], fill=(90, 120, 200))
                    d.line([(px + S - 2, py + k), (px + k, py + S - 2)], fill=(90, 120, 200))
    im.save(out)
    print('wrote', out, im.size, '-- %d aggregates, %d covered' % (len(agg), covered))
    return 0


if __name__ == '__main__':
    sys.exit(main())
