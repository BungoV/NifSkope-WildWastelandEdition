"""DEFAULTS1 picture (iii): compose the blend-edges pair.

The quadrant cross-fade lives in the terrain DIFFUSE sheet (only that file
differs; _data and _msn are byte-identical), so the honest picture is the sheet
decoded, exactly as for pictures (i) and (ii). The crop is chosen by the
difference, not by eye.
"""
import os
from PIL import Image, ImageChops, ImageDraw, ImageStat

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults1_20260912'
B = ROOT + '/blend'
OUT = ROOT + '/images'
PAD, BAR = 14, 34
BG, FG = (24, 25, 28), (232, 232, 235)


def label(img, text):
    out = Image.new('RGB', (img.width, img.height + BAR), BG)
    out.paste(img, (0, BAR))
    ImageDraw.Draw(out).text((8, 10), text, fill=FG)
    return out


def row(panels, path, title):
    tiles = [label(im, t) for im, t in panels]
    w = sum(t.width for t in tiles) + PAD * (len(tiles) + 1)
    h = max(t.height for t in tiles) + PAD * 2 + BAR
    out = Image.new('RGB', (w, h), BG)
    ImageDraw.Draw(out).text((PAD, 10), title, fill=FG)
    x = PAD
    for t in tiles:
        out.paste(t, (x, PAD + BAR))
        x += t.width + PAD
    out.save(path)
    return out.size


def load(p):
    return Image.open(p).convert('RGB')


N = 'Commonwealth.4.-20.24.DDS'
A = load(B + '/off/tex/' + N)
C = load(B + '/quadrant/tex/' + N)
d = ImageChops.difference(A, C).convert('L')
changed = sum(1 for px in d.getdata() if px)
print('%d of %d texels differ (%.2f%%), bbox %s' % (changed, A.width * A.height,
      100.0 * changed / (A.width * A.height), d.getbbox()))

size = row([(A, '--blend-edges off (the default)'),
            (C, '--blend-edges quadrant --blend-margin 128'),
            (d.point(lambda v: min(255, v * 8)).convert('RGB'), 'difference x8')],
           OUT + '/iii_blend_edges_sheet.png',
           'chunk (-20,24) dim 4, terrain diffuse 512x512, FOOTPRINT sampling, new defaults otherwise')
print('iii_blend_edges_sheet.png %dx%d %d bytes' % (size[0], size[1], os.path.getsize(OUT + '/iii_blend_edges_sheet.png')))

box = 96
best, bx, by = -1, 0, 0
for y in range(0, A.height - box + 1, box // 3):
    for x in range(0, A.width - box + 1, box // 3):
        s = ImageStat.Stat(d.crop((x, y, x + box, y + box))).sum[0]
        if s > best:
            best, bx, by = s, x, y


def zoom(im):
    return im.crop((bx, by, bx + box, by + box)).resize((box * 4, box * 4), Image.NEAREST)


size = row([(zoom(A), 'off'), (zoom(C), 'quadrant')],
           OUT + '/iii_blend_edges_crop.png',
           'the same 96x96 texels at 4x, where the cross-fade changes most (%d,%d)' % (bx, by))
print('iii_blend_edges_crop.png %dx%d %d bytes' % (size[0], size[1], os.path.getsize(OUT + '/iii_blend_edges_crop.png')))
