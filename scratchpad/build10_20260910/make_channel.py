# -*- coding: utf-8 -*-
"""make_channel.py -- gate F1's synthetic channel, drawn with the SPEED as
brightness (lane BUILD10, WATER4's PENDING step 3 picture 3).

The grid is `flow_proto.f1_continuity`'s own -- the numpy twin of the C++
solver -- so the picture and the harness's F1 numbers come from the same
method; the caption's ratio is re-derived here, never typed.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

W4 = r'E:\Projects\NifskopeWildWastelandEdition\scratchpad\water4_20260910'
sys.path.insert(0, W4)
import flow_proto as fp                          # noqa: E402

fails, g, phi = fp.f1_continuity()
ux, uy = g.velocity(phi)
sp = g.field(np.hypot(ux, uy))
ang = fp.angle_deg(g.field(ux), g.field(uy))
wet = g.wet
s64 = sp[wet[:, 64], 64].mean()
s192 = sp[wet[:, 192], 192].mean()

h, w = wet.shape
SCALE = 4
im = Image.new('RGB', (w, h), (18, 20, 24))
px = im.load()
mx = sp[wet].max()
for y in range(h):
    for x in range(w):
        if not wet[y, x]:
            continue
        t = sp[y, x] / mx
        a = np.radians(ang[y, x])
        px[x, y] = (int(40 + 215 * t * abs(np.cos(a))), int(90 + 165 * t), int(60 + 40 * t))
im = im.transpose(Image.FLIP_TOP_BOTTOM).resize((w * SCALE, h * SCALE), Image.NEAREST)
dr = ImageDraw.Draw(im)
for x, lab in ((64, 'x=64'), (192, 'x=192')):
    dr.line([x * SCALE, 0, x * SCALE, h * SCALE], fill=(255, 210, 90))


def font(sz, bold=False):
    try:
        return ImageFont.truetype(r'C:\Windows\Fonts\segoeuib.ttf' if bold
                                  else r'C:\Windows\Fonts\segoeui.ttf', sz)
    except OSError:
        return ImageFont.load_default()


fT, fN, fB = font(24, True), font(18, True), font(15)
PAD, TOP, CAP = 24, 62, 210
sheet = Image.new('RGB', (im.width + PAD * 2, TOP + im.height + CAP), (24, 26, 30))
s = ImageDraw.Draw(sheet)
title = 'F1 - a channel that narrows to half its width, speed as brightness'
assert s.textlength(title, font=fT) <= sheet.width - 2 * PAD, title
s.text((PAD, 18), title, font=fT, fill=(230, 232, 235))
sheet.paste(im, (PAD, TOP))
s.rectangle([PAD - 1, TOP - 1, PAD + im.width, TOP + im.height], outline=(70, 74, 80))
y = TOP + im.height + 12
for line, good in (('mean speed at x = 64  %.5f   at x = 192  %.5f' % (s64, s192), True),
                   ('ratio %.4f   (gate 1.90 .. 2.10)' % (s192 / s64), 1.90 <= s192 / s64 <= 2.10),
                   ('%d gate failure(s) in F1' % fails, fails == 0)):
    s.text((PAD, y), line, font=fN, fill=(150, 210, 150) if good else (230, 120, 120))
    y += 24
for ln in ('256 x 32 texels, wet everywhere for x < 128 and 16 wide beyond it; inflow across the',
           'west end, outflow across the east. The yellow lines are the two cross-sections the gate',
           'measures. Halving the width doubles the speed because k grad phi is flux, not velocity.'):
    s.text((PAD, y + 4), ln, font=fB, fill=(174, 179, 186))
    y += 21
assert y + 4 <= sheet.height
out = os.path.join(W4, 'images', 'flow_channel_f1.png')
sheet.save(out)
print('%s  %dx%d' % (out, sheet.width, sheet.height))
