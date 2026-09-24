#!/usr/bin/env python3
"""Lane BUILD7: compose one contact sheet from the six frames of one clip.

  python make_sheet.py <prefix> <title> <out.png> [--crop L,T,R,B] [--side <png> <caption>]

The six tiles are <images>/<prefix>_{bind,f0,fq1,fhalf,fq3,flast}.png, each with
a sibling .txt holding "<frameindex> <time>" written by frames.sh -- the caption
number is READ from the render, never retyped, so the picture's number and the
report's number cannot drift.

Rules taken from `ww-texel-picture`: a FIXED cell, the caption drawn inside its
own band (never over the image, never past the edge), and the caption arithmetic
done from the same file the tile came from.
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, 'images')

TILES = [
    ('bind',  'BIND POSE  (no clip)'),
    ('f0',    'frame %s'),
    ('fq1',   'frame %s'),
    ('fhalf', 'frame %s'),
    ('fq3',   'frame %s'),
    ('flast', 'frame %s  (last)'),
]

FONTS = ['C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/arial.ttf',
         'C:/Windows/Fonts/consola.ttf', 'C:/msys64/ucrt64/share/fonts/TTF/DejaVuSans.ttf']


def font(sz, bold=False):
    cands = (['C:/Windows/Fonts/segoeuib.ttf', 'C:/Windows/Fonts/arialbd.ttf'] if bold else []) + FONTS
    for p in cands:
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def main():
    prefix, title, out = sys.argv[1], sys.argv[2], sys.argv[3]
    crop = None
    side = None
    if '--crop' in sys.argv:
        crop = tuple(int(x) for x in sys.argv[sys.argv.index('--crop') + 1].split(','))
    if '--side' in sys.argv:
        i = sys.argv.index('--side')
        side = (sys.argv[i + 1], sys.argv[i + 2])

    cells = []
    for tag, cap in TILES:
        p = os.path.join(IMG, '%s_%s.png' % (prefix, tag))
        if not os.path.exists(p):
            print('MISSING %s' % p); return 2
        im = Image.open(p).convert('RGB')
        meta = os.path.join(IMG, '%s_%s.txt' % (prefix, tag))
        fi, t = open(meta).read().split()
        text = cap % fi if '%s' in cap else cap
        sub = 'bind pose' if tag == 'bind' else 't = %s s' % t
        cells.append((im, text, sub, os.path.basename(p)))
    if crop:
        cells = [(im.crop(crop), a, b, c) for im, a, b, c in cells]

    if side:
        im = Image.open(os.path.join(IMG, side[0])).convert('RGB')
        if crop:
            # The side tile is a DIFFERENT camera (it says so on its caption), so
            # it gets a box of the SAME SIZE centred on its own content instead of
            # the front tiles' box -- same pixel scale, no distortion.
            from PIL import ImageChops
            bg = Image.new('RGB', im.size, im.getpixel((5, 5)))
            bb = ImageChops.difference(im, bg).convert('L').point(lambda p: 255 if p > 12 else 0).getbbox()
            w, h = crop[2] - crop[0], crop[3] - crop[1]
            cx, cy = (bb[0] + bb[2]) // 2, (bb[1] + bb[3]) // 2
            l = max(0, min(im.size[0] - w, cx - w // 2))
            t = max(0, min(im.size[1] - h, cy - h // 2))
            im = im.crop((l, t, l + w, t + h))
        meta = os.path.join(IMG, side[0].replace('.png', '.txt'))
        sfi, st = open(meta).read().split()
        cells.append((im, side[1] % sfi, 'SIDE VIEW -- t = %s s' % st, side[0]))

    CW = 430                      # cell image width
    src_w, src_h = cells[0][0].size
    CH = int(round(CW * src_h / src_w))
    CAP = 52
    M, G = 26, 14
    COLS = 3
    ROWS = (len(cells) + COLS - 1) // COLS
    HDR = 86
    W = M * 2 + COLS * CW + (COLS - 1) * G
    H = HDR + M + ROWS * (CH + CAP) + (ROWS - 1) * G + M

    sheet = Image.new('RGB', (W, H), (22, 22, 24))
    d = ImageDraw.Draw(sheet)
    f_title = font(26, True)
    f_sub = font(15)
    f_cap = font(20, True)
    f_cap2 = font(15)

    d.text((M, 22), title, font=f_title, fill=(240, 240, 245))
    d.text((M, 56), 'fixtures/human_male_vanilla.nif -- ONE pinned camera for every tile of both sheets: '
                    'front view, orthographic (WW_RENDER_VIEW=5  CENTER=0,0,62  ORTHO=80  upp=0.150235)',
           font=f_sub, fill=(150, 152, 160))

    for i, (im, text, sub, name) in enumerate(cells):
        r, c = divmod(i, COLS)
        x = M + c * (CW + G)
        y = HDR + M + r * (CH + CAP + G)
        sheet.paste(im.resize((CW, CH), Image.LANCZOS), (x, y))
        d.rectangle([x, y, x + CW - 1, y + CH - 1], outline=(70, 72, 80))
        d.rectangle([x, y + CH, x + CW - 1, y + CH + CAP - 1], fill=(34, 34, 38))
        d.text((x + 10, y + CH + 6), text, font=f_cap, fill=(235, 236, 240))
        d.text((x + 10, y + CH + 30), sub, font=f_cap2, fill=(150, 200, 160))
        tw = d.textlength(sub, font=f_cap2)
        assert 10 + tw < CW, 'caption "%s" (%.0f px) does not fit the %d px cell' % (sub, tw, CW)
        tw = d.textlength(text, font=f_cap)
        assert 10 + tw < CW, 'caption "%s" (%.0f px) does not fit the %d px cell' % (text, tw, CW)

    sheet.save(out)
    print('%s  %dx%d  %d tiles  (source tiles %dx%d)' % (out, W, H, len(cells), src_w, src_h))
    return 0


if __name__ == '__main__':
    sys.exit(main())
