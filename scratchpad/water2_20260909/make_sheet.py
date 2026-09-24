#!/usr/bin/env python3
"""make_sheet.py -- the four Charles-region planes in one picture, labelled.

Every render is the SAME framing: `WW_LODL_REGION=-16,-21,-6,-4,0`, top view,
flat (vertex colours only), 1500x1000, from the same version-3 file. Only
`WW_LODL_PLANE` differs, so a difference in the picture is a difference in the
plane and nothing else. The captions carry the numbers the builder itself
printed for that plane, not numbers typed from somewhere else.
"""

import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, 'images')
CROP = (600, 175, 945, 715)          # the meshed region inside the 1500x1000 frame
SCALE = 2

PANELS = [
    ('charles_watertype.png',
     'watertype - what version 2 could say',
     'per CELL: 198 of 198 cells have water, 15 WATR records. Cyan is the '
     'worldspace default. It paints dry land too, and every lake sharing a form '
     'shares a colour.'),
    ('charles_bodyid.png',
     'bodyid - what version 3 adds',
     'per TEXEL: 346 bodies in the table, 28,078 of 203,681 samples name one, '
     '21 distinct here, highest id 314. The Charles is ONE body across the '
     'whole reach; the lake to its north-east is another.'),
    ('charles_flow.png',
     'flow - direction, speed and confidence',
     '27,532 of 28,078 wet samples carry a direction. Hue is the direction, '
     'brightness the speed; a body with no flow rule that answered draws dark, '
     'which is the same word as dry on purpose.'),
    ('charles_shore.png',
     'shore - distance to the nearest bank',
     '28,078 wet samples, stored steps 4..88 at 32 world units a step. The '
     'gradient is per BODY: a neighbour belonging to another body counts as a '
     'bank, so two bodies that touch each keep their own shore.'),
]


def font(sz, bold=False):
    for p in (r'C:\Windows\Fonts\segoeuib.ttf' if bold else r'C:\Windows\Fonts\segoeui.ttf',
              r'C:\Windows\Fonts\arialbd.ttf' if bold else r'C:\Windows\Fonts\arial.ttf'):
        try:
            return ImageFont.truetype(p, sz)
        except OSError:
            pass
    return ImageFont.load_default()


def wrap(draw, text, f, width):
    out, line = [], ''
    for w in text.split():
        t = (line + ' ' + w).strip()
        if draw.textlength(t, font=f) <= width:
            line = t
        else:
            out.append(line)
            line = w
    if line:
        out.append(line)
    return out


def main():
    tiles = []
    for name, title, note in PANELS:
        im = Image.open(os.path.join(IMG, name)).convert('RGB').crop(CROP)
        im = im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST)
        tiles.append((im, title, note))

    tw, th = tiles[0][0].size
    fT, fN = font(30, True), font(21)
    pad, gap, capH = 26, 22, 132
    W = pad * 2 + tw * 2 + gap
    H = pad * 2 + (th + capH) * 2 + gap + 74
    sheet = Image.new('RGB', (W, H), (24, 24, 28))
    d = ImageDraw.Draw(sheet)
    d.text((pad, 20), 'Commonwealth.lodl v3 - the Charles, one framing, four planes',
           font=font(34, True), fill=(238, 238, 240))
    d.text((pad, 62), 'top view, flat (vertex colours only), LOD 0 = the file\'s own '
           '32 samples a cell, 1500x1000 cropped and doubled',
           font=fN, fill=(150, 152, 158))

    for i, (im, title, note) in enumerate(tiles):
        x = pad + (i % 2) * (tw + gap)
        y = 74 + pad + (i // 2) * (th + capH + gap)
        sheet.paste(im, (x, y))
        d.rectangle([x - 1, y - 1, x + tw, y + th], outline=(70, 72, 80))
        d.text((x, y + th + 10), title, font=fT, fill=(238, 238, 240))
        yy = y + th + 48
        for line in wrap(d, note, fN, tw):
            d.text((x, yy), line, font=fN, fill=(168, 170, 178))
            yy += 26

    out = os.path.join(IMG, 'charles_four_planes.png')
    sheet.save(out)
    print('%s  %dx%d  %d bytes' % (out, sheet.width, sheet.height, os.path.getsize(out)))


if __name__ == '__main__':
    main()
