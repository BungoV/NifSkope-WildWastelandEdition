#!/usr/bin/env python3
"""Lane CHANVIEW1 step 4 -- the contact sheet.

Every channel at the CLOSE framing in one labelled grid, each cell captioned with
the channel's own min / max / mean -- and the number in the caption is the number
in the report's table (section 1.3), read from `table_after.json`, which is the
independent Python reader's output, not this script's arithmetic
(`ww-texel-picture` rule 4).

Rules honoured here: fixed cell so no caption clips (rule 3), the page caption
wrapped to the page width with the canvas grown by it (rule 8), the camera named
with the numbers the renders were actually taken at, and a channel the bake does
not carry captioned ABSENT in red rather than given a value it does not have.

usage: make_contact.py
"""
import json
import os

from PIL import Image, ImageDraw, ImageFont

S = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(S, 'images')

# name -> (the table row to quote, the population sentence, the units)
ROWS = [
    ('identity',    'identity',    '2,446 placements, .lodi identity (hashed to colour)'),
    ('identityraw', 'identitylow', '2,446 placements, .lodi identity low byte'),
    ('sky',         'sky',         '2,446 placements, .lodi 0x11 sky visibility'),
    ('ground',      'ground',      '2,446 placements, .lodi 0x12 ground contact'),
    ('seed',        'seed',        '2,446 placements, .lodi 0x13 seed (0 = not a tree)'),
    ('sway',        'sway',        '53,349 vertices, .lodo 0x0E sway'),
    ('selfao',      'selfao',      '53,349 vertices, .lodo 0x0F self-AO'),
    ('ao',          'ao',          '53,349 vertices, .lodi v6 scene AO x placement AO'),
    ('mask-r',      'mask-r',      '4,194,304 texels, mask sheet R = roughness'),
    ('mask-g',      'mask-g',      '4,194,304 texels, mask sheet G = metallic'),
    ('mask-b',      'mask-b',      '4,194,304 texels, mask sheet B = sky AO'),
    ('mask-a',      'mask-a',      'mask sheet A = ground cover'),
    ('emissive',    'emissive-r',  'role-6 emissive sheet'),
    ('normal',      'normal-r',    '4,194,304 texels, role-2 MSN sheet (R shown; G/B in the report)'),
]

COLS, THUMB_W = 4, 348
INK = (232, 232, 232)
DIM = (150, 150, 150)
RED = (236, 106, 106)
GREEN = (126, 200, 126)
BG = (22, 22, 24)


def font(sz, bold=False):
    for p in (r'C:\Windows\Fonts\%s' % ('arialbd.ttf' if bold else 'arial.ttf'),
              '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'):
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


F_NAME, F_NUM, F_SRC, F_HEAD, F_CAP = (font(21, True), font(17), font(14),
                                       font(26, True), font(15))


def wrap(draw, text, fnt, width):
    out, line = [], ''
    for w in text.split(' '):
        t = (line + ' ' + w).strip()
        if draw.textlength(t, font=fnt) <= width or not line:
            line = t
        else:
            out.append(line)
            line = w
    if line:
        out.append(line)
    return out


def main():
    table = json.load(open(os.path.join(S, 'table_after.json')))
    thumbs = {}
    for name, _, _ in ROWS:
        im = Image.open(os.path.join(IMG, 'chunk_%s_close.png' % name)).convert('RGB')
        h = round(im.height * THUMB_W / im.width)
        thumbs[name] = im.resize((THUMB_W, h), Image.LANCZOS)
    th = max(t.height for t in thumbs.values())

    pad, capH = 14, 74
    cellW, cellH = THUMB_W + 2 * pad, th + capH + 2 * pad
    rows = (len(ROWS) + COLS - 1) // COLS
    pageW = COLS * cellW
    headH = 96

    scratch = ImageDraw.Draw(Image.new('RGB', (8, 8)))
    cap = ('Every baked channel of chunk 4.4.-12, close framing, SLAB1 after-bake '
           '(scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth) with the v6 '
           '.lodi/.lodo pair; camera ortho half-width 2600, centre 24900,-41300,450, '
           'view 8, frame 1400x1091, WW_RENDER_CLEAN=1, texturing off except normal and '
           'emissive. Each min/max/mean is the independent Python reader\'s number from '
           'the report table (section 1.3), NOT this script\'s arithmetic; the viewer\'s '
           'own note line agrees with it to three decimals for every channel (section '
           '3.1). GREEN = the channel carries values and its render differs from the '
           'default render; RED = the bake does not carry it, the render is identical to '
           'the default and the note line says so by name.')
    caplines = wrap(scratch, cap, F_CAP, pageW - 32)
    capH_page = 10 + len(caplines) * 20 + 12
    pageH = headH + capH_page + rows * cellH + 12

    page = Image.new('RGB', (pageW, pageH), BG)
    d = ImageDraw.Draw(page)
    d.text((16, 22), 'WW native LOD viewer -- WW_LODL_CHANNEL, all 14 names, '
                     'chunk 4.4.-12', font=F_HEAD, fill=INK)
    d.text((16, 58), 'release/NifSkope.exe 2026-09-18 08:40:07  sha1 62efc25c3871',
           font=F_CAP, fill=DIM)
    y = headH
    for ln in caplines:
        d.text((16, y), ln, font=F_CAP, fill=DIM)
        y += 20

    top0 = headH + capH_page
    for i, (name, key, src) in enumerate(ROWS):
        cx = (i % COLS) * cellW
        cy = top0 + (i // COLS) * cellH
        t = thumbs[name]
        page.paste(t, (cx + pad, cy + pad + (th - t.height) // 2))
        d.rectangle([cx + pad - 1, cy + pad - 1,
                     cx + pad + THUMB_W, cy + pad + th], outline=(70, 70, 74))
        ty = cy + pad + th + 8
        row = table.get(key, {})
        absent = 'absent' in row
        d.text((cx + pad, ty), name, font=F_NAME, fill=RED if absent else INK)
        if absent:
            d.text((cx + pad, ty + 25),
                   'ABSENT -- panel is the UNCHANGED default render', font=F_SRC,
                   fill=RED)
            wy = ty + 43
            for ln in wrap(d, row['absent'], F_SRC, THUMB_W)[:2]:
                d.text((cx + pad, wy), ln, font=F_SRC, fill=RED)
                wy += 16
        else:
            const = row.get('constant')
            num = ('constant %g   (min %g  max %g  mean %.3f)'
                   % (const, row['min'], row['max'], row['mean'])) if const is not None \
                else ('min %g   max %g   mean %.3f'
                      % (row['min'], row['max'], row['mean']))
            d.text((cx + pad, ty + 25), num, font=F_NUM,
                   fill=RED if const is not None else GREEN)
            d.text((cx + pad, ty + 47), src, font=F_SRC, fill=DIM)

    out = os.path.join(IMG, 'channels_contact.png')
    page.save(out)
    print('%s  %dx%d  %d B' % (out, page.width, page.height, os.path.getsize(out)))


if __name__ == '__main__':
    main()
