"""DEFAULTS1 item 5: the panel column, rung beside new.

WW_LODGEN_SHOT_FULL grabs the scroll area's INNER widget -- the whole settings
column, with every folding section opened for the grab -- so the moved rows are
in the frame rather than below it. Same exe-side grab on both sides, so the two
pictures are comparable row for row.

What the pair actually proves, and what it cannot: the five land/road rows read
SAVED settings, and the one-shot migration has already moved those, so both
exes draw the same numbers. The two identity boxes are not saved and follow the
code. So the difference between these two columns is exactly the identity rows,
and the caption says so rather than claiming more.
"""
import os
from PIL import Image, ImageChops, ImageDraw

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults1_20260912'
P = ROOT + '/pics'
OUT = ROOT + '/images'
PAD, LINE = 14, 15
BG, FG = (24, 25, 28), (232, 232, 235)

TITLE = [
    'the LOD Generation settings column, WW_LODGEN_SHOT_FULL, the same grab on both exes -- no row added, none removed',
    'the five LAND / ROAD rows read the SAME numbers on both sides, because they come from saved settings and the',
    'one-shot migration has already moved those (landHex 256, landWarp 341, landMipBias -0.22, landGuide 5,',
    'roadGroundPaint 0, defaults1Applied true, read back out of the registry)',
    'the two identity boxes are NOT saved, so they follow the code -- and that is the whole difference here',
]
CAPS = ['the rung exe, reading the same saved settings',
        'the new exe: the identity boxes are unticked']

a = Image.open(P + '/panel_rung_full.png').convert('RGB')
b = Image.open(P + '/panel_new_full.png').convert('RGB')
print('rung %s   new %s' % (a.size, b.size))

head = LINE * len(TITLE) + PAD
bar = LINE + PAD
h = max(a.height, b.height)

tiles = []
for im, text in zip((a, b), CAPS):
    t = Image.new('RGB', (im.width, h + bar), BG)
    t.paste(im, (0, bar))
    ImageDraw.Draw(t).text((8, 8), text, fill=FG)
    tiles.append(t)

w = sum(t.width for t in tiles) + PAD * (len(tiles) + 1)
out = Image.new('RGB', (w, h + bar + head + PAD), BG)
d = ImageDraw.Draw(out)
for i, line in enumerate(TITLE):
    d.text((PAD, 8 + i * LINE), line, fill=FG)
x = PAD
for t in tiles:
    out.paste(t, (x, head))
    x += t.width + PAD
path = OUT + '/iv_panel_rows.png'
out.save(path)
print('iv_panel_rows.png %dx%d %d bytes' % (out.size[0], out.size[1], os.path.getsize(path)))

if a.size == b.size:
    df = ImageChops.difference(a, b).convert('L')
    n = sum(1 for px in df.getdata() if px)
    print('panel pair: %d pixels differ, bbox %s' % (n, df.getbbox()))
else:
    print('panel pair: the columns are different heights (%d vs %d)' % (a.height, b.height))
