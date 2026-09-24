"""DEFAULTS1: compose the two RENDER pairs into labelled panels.

These are NifSkope's own top-down views of the chunk files, one camera per
pair (a .BTR is in the file's own space, a .BTO in world units), taken by
pics_render.sh. What they show is the VERTEX data, which is what the identity
flag moves: the land's colour channel and the object chunk's.
"""
import os
from PIL import Image, ImageChops, ImageDraw

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults1_20260912'
P = ROOT + '/pics'
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


def half(p):
    im = Image.open(p).convert('RGB')
    return im.resize((im.width // 2, im.height // 2), Image.LANCZOS)


rows = []
a, b = half(P + '/i_rung_btr.png'), half(P + '/i_new_btr.png')
rows.append(row([(a, 'rung default: terrain identity ON'),
                 (b, 'new default: terrain identity OFF')],
                OUT + '/i_btr_identity.png',
                'chunk (-20,24) dim 4, the .BTR top-down, one pinned camera (8192,8192 / half-width 8192)'))
a, b = half(P + '/i_rung_bto.png'), half(P + '/i_new_bto.png')
rows.append(row([(a, 'rung default: object identity ON'),
                 (b, 'new default: object identity OFF')],
                OUT + '/i_bto_identity.png',
                'chunk (-20,24) dim 4, the .BTO top-down, one pinned camera (world -73728,106496 / half-width 8192)'))

a, b = half(P + '/ii_paint1_full.png'), half(P + '/ii_paint0_full.png')
rows.append(row([(a, 'road-ground-paint 1 (the old default)'),
                 (b, 'road-ground-paint 0 (the new default)')],
                OUT + '/ii_verge_chunk.png',
                'chunk (-20,20) dim 4, the .BTR top-down, one pinned camera -- the verge beside the kerb'))
a, b = Image.open(P + '/ii_paint1_crop.png').convert('RGB'), Image.open(P + '/ii_paint0_crop.png').convert('RGB')
rows.append(row([(a, 'paint 1'), (b, 'paint 0')],
                OUT + '/ii_verge_kerb.png',
                'the same kerb at the camera the difference chose (6085,8490 / half-width 1521)'))

names = ['i_btr_identity.png', 'i_bto_identity.png', 'ii_verge_chunk.png', 'ii_verge_kerb.png']
for n, s in zip(names, rows):
    print('%-28s %5dx%-5d %8d bytes' % (n, s[0], s[1], os.path.getsize(OUT + '/' + n)))

# the pairs are not identical, and the number says so
for tag, x, y in [('BTR', P + '/i_rung_btr.png', P + '/i_new_btr.png'),
                  ('BTO', P + '/i_rung_bto.png', P + '/i_new_bto.png'),
                  ('verge chunk', P + '/ii_paint1_full.png', P + '/ii_paint0_full.png'),
                  ('verge kerb', P + '/ii_paint1_crop.png', P + '/ii_paint0_crop.png')]:
    d = ImageChops.difference(Image.open(x).convert('RGB'), Image.open(y).convert('RGB'))
    bbox = d.getbbox()
    n = sum(1 for px in d.convert('L').getdata() if px)
    print('%s pair: %d pixels differ, bbox %s' % (tag, n, bbox))
