"""DEFAULTS1: the pictures that live in the terrain SHEETS, not in geometry.

Why not a render: NifSkope's .BTR view does not use the chunk's terrain diffuse
at all -- proved by rendering one .BTR against two different resource roots
(the paint-1 sheet and the paint-0 sheet) and getting pixel-identical frames,
difference bounding box None. The land look and the verge paint are texels in
`<chunk>.DDS`, so the honest picture is the sheet itself, decoded.

Writes side-by-side panels with the labels burned in, plus a difference map and
a zoomed crop centred on the largest cluster of changed texels -- the crop is
CHOSEN by the difference, not by eye.
"""
import os
from PIL import Image, ImageChops, ImageDraw, ImageStat

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults1_20260912'
G = ROOT + '/gate'
OUT = ROOT + '/images'
os.makedirs(OUT, exist_ok=True)

PAD = 14
BAR = 34
BG = (24, 25, 28)
FG = (232, 232, 235)


def label(img, text):
    w, h = img.size
    out = Image.new('RGB', (w, h + BAR), BG)
    out.paste(img, (0, BAR))
    d = ImageDraw.Draw(out)
    d.text((8, 10), text, fill=FG)
    return out


def row(panels, path, title):
    tiles = [label(im, t) for im, t in panels]
    w = sum(t.width for t in tiles) + PAD * (len(tiles) + 1)
    h = max(t.height for t in tiles) + PAD * 2 + BAR
    out = Image.new('RGB', (w, h), BG)
    d = ImageDraw.Draw(out)
    d.text((PAD, 10), title, fill=FG)
    x = PAD
    for t in tiles:
        out.paste(t, (x, PAD + BAR))
        x += t.width + PAD
    out.save(path)
    return out.size


def load(p):
    return Image.open(p).convert('RGB')


def diffmap(a, b):
    d = ImageChops.difference(a, b).convert('L')
    return d.point(lambda v: min(255, v * 8))


def hotspot(a, b, box=96):
    """The box-sized window holding the most changed texels. Measured, not picked."""
    d = ImageChops.difference(a, b).convert('L')
    w, h = d.size
    best, bx, by = -1, 0, 0
    step = box // 3
    for y in range(0, h - box + 1, step):
        for x in range(0, w - box + 1, step):
            s = ImageStat.Stat(d.crop((x, y, x + box, y + box))).sum[0]
            if s > best:
                best, bx, by = s, x, y
    return bx, by, box, best


def zoom(im, x, y, box, scale=4):
    return im.crop((x, y, x + box, y + box)).resize((box * scale, box * scale), Image.NEAREST)


report = []

# --- (i) the land look: hex 256, warp 341, mip bias -0.22, guide flatwarp ----
A = load(G + '/b_rung/tex/Commonwealth.4.-20.24.DDS')
B = load(G + '/a_new/tex/Commonwealth.4.-20.24.DDS')
size = row([(A, 'rung default'), (B, 'new default (hex 256, warp 341, mip -0.22, flat warp)'),
            (diffmap(A, B).convert('RGB'), 'difference x8')],
           OUT + '/i_land_look_sheet.png',
           'chunk (-20,24) dim 4, terrain diffuse 512x512 -- the ruled land look')
report.append(('i_land_look_sheet.png', size))

x, y, box, s = hotspot(A, B)
size = row([(zoom(A, x, y, box), 'rung default'), (zoom(B, x, y, box), 'new default')],
           OUT + '/i_land_look_crop.png',
           'the same 96x96 texels at 4x, where the two sheets differ most (%d,%d)' % (x, y))
report.append(('i_land_look_crop.png', size))

# --- (ii) the verge: road ground paint 1 vs 0 -------------------------------
A = load(G + '/a2_old/tex/Commonwealth.4.-20.20.DDS')
B = load(G + '/a2_new/tex/Commonwealth.4.-20.20.DDS')
size = row([(A, 'road-ground-paint 1 (the old default)'), (B, 'road-ground-paint 0 (the new default)'),
            (diffmap(A, B).convert('RGB'), 'difference x8')],
           OUT + '/ii_verge_sheet.png',
           'chunk (-20,20) dim 4, terrain diffuse -- the grass planes in the road NIFs')
report.append(('ii_verge_sheet.png', size))

x, y, box, s = hotspot(A, B)
size = row([(zoom(A, x, y, box), 'paint 1'), (zoom(B, x, y, box), 'paint 0')],
           OUT + '/ii_verge_crop.png',
           'the kerb at 4x: the same 96x96 texels, where the paint differs most (%d,%d)' % (x, y))
report.append(('ii_verge_crop.png', size))

for name, size in report:
    p = OUT + '/' + name
    print('%-28s %5dx%-5d %8d bytes' % (name, size[0], size[1], os.path.getsize(p)))
