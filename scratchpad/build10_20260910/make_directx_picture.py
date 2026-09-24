# -*- coding: utf-8 -*-
"""make_directx_picture.py -- the checked-in DirectX test image, at texel level.

Lane WATER6 (BUILD10).  Sixteen texels are not a render: this is the
`ww-texel-picture` case.  Each texel is drawn 96 px square with its own R and G
printed on it, an arrow for the direction the convention says it means, and the
direction number the shipped decoder reads out of the FILE (through the same
arithmetic `WaterCurveDoc::wordFromRgba` uses, written here independently).

The point of the picture is the GREEN column: north is DARK green (G = 1) and
south is BRIGHT (G = 255), because green grows toward the image BOTTOM.
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'
SRC = os.path.join(ROOT, 'tests', 'fixtures', 'flowmap_directx_4x4.png')
OUT = os.path.join(ROOT, 'scratchpad', 'build10_20260910', 'images', 'directx_convention.png')
TWO_PI = 2.0 * math.pi
CELL, PAD, TOP, CAP = 170, 24, 96, 182


def font(sz, bold=False):
    try:
        return ImageFont.truetype(r'C:\Windows\Fonts\segoeuib.ttf' if bold
                                  else r'C:\Windows\Fonts\segoeui.ttf', sz)
    except OSError:
        return ImageFont.load_default()


fT, fN, fS, fB = font(26, True), font(19, True), font(14), font(15)
im = Image.open(SRC).convert('RGBA')
assert im.size == (4, 4), im.size

W = PAD * 2 + CELL * 4
H = TOP + CELL * 4 + CAP
sheet = Image.new('RGB', (W, H), (24, 26, 30))
d = ImageDraw.Draw(sheet)
title = 'tests/fixtures/flowmap_directx_4x4.png - 4 x 4 texels'
assert d.textlength(title, font=fT) <= W - 2 * PAD, title
d.text((PAD, 18), title, font=fT, fill=(230, 232, 235))
d.text((PAD, 52), 'R = +X (east) - G = +Y TOWARD THE IMAGE BOTTOM - both centred on 128',
       font=fN, fill=(240, 165, 74))

wrong = 0
for row in range(4):
    for col in range(4):
        r, g, b, a = im.getpixel((col, row))
        x0, y0 = PAD + col * CELL, TOP + row * CELL
        d.rectangle([x0, y0, x0 + CELL - 2, y0 + CELL - 2], fill=(r, g, b))
        # the direction the shipped decoder reads back out of these two bytes
        cx = (r - 128) / 127.0
        cy = -((g - 128) / 127.0)
        ang = math.atan2(cy, cx)
        if ang < 0:
            ang += TWO_PI
        got = round(ang / TWO_PI * 256.0) & 0xFF
        want = (row * 4 + col) * 16
        if got != want:
            wrong += 1
        # the arrow: world +Y is NORTH, which is UP in this picture
        mx, my = x0 + CELL // 2, y0 + CELL // 2
        ux, uy = math.cos(ang) * 30, -math.sin(ang) * 30
        d.line([mx - ux, my - uy, mx + ux, my + uy], fill=(20, 20, 24), width=5)
        d.ellipse([mx + ux - 6, my + uy - 6, mx + ux + 6, my + uy + 6], fill=(250, 250, 250))
        ink = (16, 16, 20) if (r + g + b) > 330 else (240, 240, 240)
        d.text((x0 + 6, y0 + 4), 'R%3d' % r, font=fS, fill=ink)
        d.text((x0 + 6, y0 + 20), 'G%3d' % g, font=fS, fill=ink)
        d.text((x0 + 6, y0 + CELL - 24), 'dir %d' % got, font=fS, fill=ink)
        name = {0: 'EAST', 64: 'NORTH', 128: 'WEST', 192: 'SOUTH'}.get(want)
        if name:
            d.text((x0 + CELL - 8 - d.textlength(name, font=fS), y0 + CELL - 24),
                   name, font=fS, fill=ink)

y = TOP + CELL * 4 + 14
for line, good in ((('the shipped decoder reads all 16 texels as the documented direction'
                     if not wrong else '%d of 16 texels decode to the WRONG direction' % wrong),
                    wrong == 0),
                   ('north  R128 G  1 (dark green)      south  R128 G255 (bright green)', True),
                   ('east   R255 G128                   west   R  1 G128', True)):
    assert d.textlength(line, font=fN) <= W - 2 * PAD, line
    d.text((PAD, y), line, font=fN, fill=(150, 210, 150) if good else (230, 120, 120))
    y += 26
d.text((PAD, y + 2), 'The white dot is the head of the arrow. The image was written from the rule by a '
                     'script that', font=fB, fill=(174, 179, 186))
d.text((PAD, y + 22), 'shares no code with NifSkope\'s codec, which is what makes it gate X5c. The blue '
                      'cast is B = A = 255,',
       font=fB, fill=(174, 179, 186))
d.text((PAD, y + 42), 'i.e. speed 15 and confidence 15 everywhere, so only R and G carry anything under test.',
       font=fB, fill=(174, 179, 186))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
sheet.save(OUT)
print('%s  %dx%d  (%d of 16 wrong)' % (OUT, sheet.width, sheet.height, wrong))
