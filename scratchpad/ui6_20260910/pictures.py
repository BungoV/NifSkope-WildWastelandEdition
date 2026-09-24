#!/usr/bin/env python3
"""Lane UI6 -- the before/after comparison pictures.

Every input is an IN-APPLICATION GRAB written by a harness inside the real
window (CONSTITUTION 5); nothing here screen-captures the desktop. This script
only crops, scales (NEAREST, so a pixel stays a pixel) and labels.

The BEFORE halves are the 05:58:21 exe (lane UI5's), photographed by the SAME
spell with the same arguments, before this lane linked; the AFTER halves are
this lane's exe.

The two windows are not the same width to the pixel -- the harness sizes itself
-- so each half is cropped from ITS OWN measured origin (the viewport header's
x, read out of that run's own log) rather than from one shared rectangle.
"""
import sys
from PIL import Image, ImageDraw

D = 'scratchpad/ui6_20260910/images/'
RED = (200, 60, 60)
TEXT = (245, 245, 245)
BG = (24, 25, 28)

# the viewport header's x in each run, read out of that run's own water_ui log
HEADER_X = {'before': 177, 'after': 167}


def label(img, text, pad=20):
    out = Image.new('RGB', (img.width, img.height + pad), BG)
    out.paste(img.convert('RGB'), (0, 0))
    ImageDraw.Draw(out).text((4, img.height + 5), text, fill=TEXT)
    return out


def stack(top, bottom, out_path, top_text, bottom_text):
    a = label(top, top_text)
    b = label(bottom, bottom_text)
    w = max(a.width, b.width)
    out = Image.new('RGB', (w, a.height + 3 + b.height), BG)
    out.paste(a, (0, 0))
    ImageDraw.Draw(out).rectangle([0, a.height, w - 1, a.height + 2], fill=RED)
    out.paste(b, (0, a.height + 3))
    out.save(out_path)
    print('%s  %dx%d' % (out_path, out.width, out.height))


def cropN(path, box, n=4):
    im = Image.open(path).convert('RGB').crop(box)
    return im.resize((im.width * n, im.height * n), Image.NEAREST)


def crop4x(path, box):
    return cropN(path, box, 4)


def main():
    # ---- the LEFT strip: the harness already wrote it at 4x
    stack(Image.open(D + 'strip4x_before.png'),
          Image.open(D + 'strip4x_after.png'),
          D + 'cmp_strip_left.png',
          'BEFORE 05:58:21   Header | Blocks | Files -- 4 px BETWEEN the segments',
          'AFTER  07:06:04   the same strip, segments TOUCHING, outer air still 4')

    # ---- the RIGHT strip: the LOD panel's own two segments, out of the dock grab
    box = (0, 20, 497, 78)
    stack(cropN(D + 'lodtab_before.png', box, 3),
          cropN(D + 'lodtab_after.png', box, 3),
          D + 'cmp_strip_right.png',
          'BEFORE 05:58:21   LOD | Water in the right-hand panel -- 4 px between them',
          'AFTER  07:06:04   the same two segments, TOUCHING')

    # ---- the viewport header's menu arrows, at 4x, from each run's own header x
    for half in ('before', 'after'):
        x = HEADER_X[half]
        globals()['hdr_' + half] = cropN(D + 'toprow_%s.png' % half,
                                         (x, 35, x + 640, 70), 3)
    stack(globals()['hdr_before'], globals()['hdr_after'],
          D + 'cmp_arrows.png',
          'BEFORE 05:58:21   the dropdown arrow sits ON the pivot and grid glyphs',
          'AFTER  07:06:04   every menu arrow has its own column (worst 2 px clear, 13 of 13)')


if __name__ == '__main__':
    sys.exit(main())
