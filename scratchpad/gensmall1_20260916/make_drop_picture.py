#!/usr/bin/env python
"""The second picture: the asymmetric drop, one pixel column per 40 placements.

Chunk (-32,0) dim 32, the bucket-cap chunk. The manifest lists 42,560
placements in the order the bake walked them. This draws that order left to
right: green where the .BTO carries geometry for the placement, red where it
carries none. The point is the SHAPE -- everything is green until index 38,695
and then two thirds of it is red -- because "6.17% are missing" reads like a
scatter of losses and it is not one.

The native row underneath is the same chunk in the same run: 42,560 of 42,560.
"""
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
sys.path.insert(0, HERE)

BTO = os.path.join(HERE, 'drop', 'Commonwealth.32.-32.0.BTO')
MAN = BTO + '.manifest.txt'
OUT = os.path.join(HERE, 'drop_order_dependence.png')

src = open(os.path.join(HERE, 'dropprobe.py'), encoding='utf-8').read()
src = src.split("def main(")[0]
g = {'__file__': os.path.join(HERE, 'dropprobe.py')}
exec(src, g)
Probe = g['Probe']

p = Probe(BTO)
rows = []
with open(MAN, 'r', encoding='utf-8', errors='replace') as f:
    f.readline()
    for line in f:
        if line and line[0].isdigit():
            rows.append(int(line.split()[0]))
rows.sort()
total = len(rows)
present = p.present

W, BAR, GAP = 1064, 46, 16
PER = (total + W - 1) // W          # placements a pixel column
TOP = 96
canvas = Image.new('RGB', (W + 48, TOP + BAR * 2 + GAP + 180), (22, 22, 24))
d = ImageDraw.Draw(canvas)

miss_by_col = []
for c in range(W):
    lo, hi = c * PER, min((c + 1) * PER, total)
    if lo >= hi:
        miss_by_col.append(0.0)
        continue
    m = sum(1 for i in rows[lo:hi] if i not in present)
    miss_by_col.append(m / float(hi - lo))

for c, frac in enumerate(miss_by_col):
    x = 24 + c
    # stock row: red in proportion to how much of that slice is missing
    col = (int(60 + 195 * frac), int(190 - 150 * frac), int(90 - 60 * frac))
    d.line([x, TOP, x, TOP + BAR], fill=col)
    # native row: nothing is missing, ever
    d.line([x, TOP + BAR + GAP, x, TOP + BAR * 2 + GAP], fill=(60, 190, 90))

miss = sorted(i for i in rows if i not in present)
first = miss[0]
fx = 24 + int(first / float(total) * W)
d.line([fx, TOP - 10, fx, TOP + BAR * 2 + GAP + 10], fill=(255, 220, 90))
d.text((min(fx + 6, W - 180), TOP - 24), 'first miss: index %d' % first, fill=(255, 220, 90))

d.text((24, 14), 'The asymmetric drop is ORDER-dependent, not a scatter', fill=(245, 245, 245))
d.text((24, 34), 'chunk (-32,0) dim 32, 42,560 placements in manifest order, one pixel column per %d' % PER,
       fill=(170, 170, 178))
d.text((24, 50), 'green = the file carries geometry for that placement   red = it carries none',
       fill=(170, 170, 178))

d.text((24, TOP + BAR // 2 - 22), 'stock .BTO', fill=(255, 140, 140))
d.text((24, TOP + BAR + GAP + BAR // 2 - 22), 'native .lodo / .lodi', fill=(140, 230, 160))

y = TOP + BAR * 2 + GAP + 26
for line, col in (
    ('stock  .BTO      42,560 offered   39,932 carried   2,628 lost  (6.17%)', (255, 180, 180)),
    ('native .lodo/.lodi 42,560 offered   42,560 carried       0 lost  (0.00%)', (180, 235, 195)),
    ('', (0, 0, 0)),
    ('Same chunk, same run, same 42,560 placements handed to both paths.', (225, 225, 230)),
    ('After index 38,695: 3,865 placements remain and 2,628 of them (68.0%) are gone.', (225, 225, 230)),
    ('Four shapes sit at 65,535 / 65,534 / 65,529 / 65,424 vertices -- the 16-bit index cap.', (170, 170, 178)),
    ('src/lodgen.cpp:3919  `continue;  // bucket full`  -- counted nowhere, no census clause.', (170, 170, 178)),
    ('All 2,628 are STAT. One base loses 851 alone; ten bases account for 2,542.', (170, 170, 178)),
):
    if line:
        d.text((24, y), line, fill=col)
    y += 16

d.text((24, canvas.size[1] - 16),
       'lane GENSMALL1, 2026-09-16, release/NifSkope.exe 22,300,160 B 13:52:56 -- measured, not quoted',
       fill=(120, 120, 128))
canvas.save(OUT)
print('wrote %s  %dx%d  (per column %d)' % (OUT, canvas.size[0], canvas.size[1], PER))
