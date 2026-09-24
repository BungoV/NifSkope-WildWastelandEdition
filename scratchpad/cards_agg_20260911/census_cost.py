#!/usr/bin/env python3
"""Lane CARDS-AGG -- what one aggregate card set per forested cell COSTS.

Reads the per-cell table census.exe wrote (cx,cy,trees,placements,treeBases,
landMin,landMax,relief,maxTreeExtent) and prices the aggregate under the
EXISTING frame law of docs/LODGEN_CARD_SHEETS.md 3.1-3.4, unchanged:

  * the long side of a frame is the run's tile rung;
  * the short side is the smallest multiple of 16 whose INNER rect is not
    narrower than the measured silhouette;
  * gap(side) = max(2, side/16) rounded UP to even, pad = gap/2;
  * mips = max(1, log2(min(gapX, gapY)));
  * every sheet is BC3 (1 byte a texel), three sheets a set: colour+coverage,
    normal+height+sway, mask.  The aggregate writes NO emissive sheet.

The silhouette a CELL presents at the horizon, from the census's own numbers:

  width  W = 4096*sqrt(2) + 2*maxTreeExtent   the cell seen corner-on, plus the
                                              trees that overhang both edges
  height H = relief + 2*maxTreeExtent         the terrain's own range inside the
                                              cell plus a whole tree

`maxTreeExtent` is the larger of a model's horizontal radius and its half
height (src/nifcli.cpp:3341), so 2*extent is an upper bound on tree height and
on tree width -- the frame is sized generously and never crops, which is the
direction the frame law already errs in.

No file is written.  Every number printed here is derived from the CSV and the
four constants above.
"""

import csv
import math
import sys

BC3_BYTES_PER_TEXEL = 1.0          # 4x4 block = 16 B
SHEETS_PER_SET = 3                 # colour+coverage, normal+height+sway, mask
CELL = 4096.0


def even_up(v):
    v = int(math.ceil(v))
    return v + (v & 1)


def gap_of(side):
    return max(2, even_up(side / 16.0))


def bc3_bytes(w, h):
    return ((w + 3) // 4) * ((h + 3) // 4) * 16


def chain_bytes(w, h, mips):
    total = 0
    for k in range(mips):
        total += bc3_bytes(max(1, w >> k), max(1, h >> k))
    return total


def frame_for(relief, extent, tile):
    w = CELL * math.sqrt(2.0) + 2.0 * extent
    h = relief + 2.0 * extent
    want = tile * (h / w) if w > 0 else tile
    short = 16
    while short < want and short < tile:
        short += 16
    return min(short, tile), w, h


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'logs/census_cw.csv'
    views = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    rows = []
    with open(path, newline='') as f:
        for r in csv.DictReader(f):
            rows.append((int(r['cx']), int(r['cy']), int(r['trees']),
                         float(r['relief']), float(r['maxTreeExtent'])))
    total_trees = sum(r[2] for r in rows)
    print(f'per-cell table   : {path}')
    print(f'cells with trees : {len(rows)}')
    print(f'tree placements  : {total_trees}')
    print(f'horizon views    : {views} azimuths x 1 elevation band')
    print()

    # the silhouette the frame law is fed, over the cells that hold trees
    reliefs = sorted(r[3] for r in rows)
    exts = sorted(r[4] for r in rows)
    def pct(v, p):
        return v[min(len(v) - 1, int(p * len(v)))]
    print('== the cell silhouette, measured (units)')
    print(f'  land relief inside a cell : min {reliefs[0]:.0f}  p50 {pct(reliefs,.5):.0f}'
          f'  p90 {pct(reliefs,.9):.0f}  max {reliefs[-1]:.0f}')
    print(f'  biggest tree extent       : min {exts[0]:.0f}  p50 {pct(exts,.5):.0f}'
          f'  p90 {pct(exts,.9):.0f}  max {exts[-1]:.0f}')
    print()

    for tile in (64, 128, 256):
        gx = gap_of(tile)
        print(f'== tile {tile} px  (the long side of every aggregate frame)')
        print('     N    cells   trees aggregated   ring-3 draws  short sides seen'
              '     sheet bytes       per cell')
        for n in (1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64):
            sel = [r for r in rows if r[2] >= n]
            if not sel:
                continue
            total = 0
            shorts = {}
            for (_cx, _cy, _t, relief, ext) in sel:
                short, _w, _h = frame_for(relief, ext, tile)
                shorts[short] = shorts.get(short, 0) + 1
                gy = gap_of(short)
                mips = max(1, int(math.log2(min(gx, gy))))
                total += SHEETS_PER_SET * chain_bytes(views * tile, short, mips)
            trees = sum(r[2] for r in sel)
            shortstr = ','.join(f'{k}x{v}' for k, v in sorted(shorts.items()))
            print(f'  {n:4d} {len(sel):8d} {trees:18d} {trees - len(sel):14d}'
                  f'  {shortstr:>22s} {total/1048576.0:11.1f} MB'
                  f' {total/len(sel)/1024.0:8.0f} KB')
        print()

    print('"ring-3 draws" is the number of per-tree quads the aggregate removes'
          ' from the far band:')
    print('trees aggregated - cells, i.e. what the runtime stops submitting once'
          ' the cross-fade is done.')


if __name__ == '__main__':
    main()
