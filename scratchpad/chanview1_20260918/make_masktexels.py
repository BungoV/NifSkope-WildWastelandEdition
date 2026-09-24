#!/usr/bin/env python3
"""Lane CHANVIEW1 step 4 -- the mask sheet's four channels as TEXEL crops.

The brief asks for the mask sheet's R, G, B and A over the SAME 256x256 texel
window, side by side, one picture. A rendered view cannot show this: the mask
sheet reaches the screen only as a bilinear tap at a terrain vertex, so the
render is a resample of the texels and not the texels. This reads the container
itself, mip 0, content texels only, through the one reader the lane already uses
(`lodl_channels_table.sheet_plane`, on `lodgen_vt_check.Lodv`) -- one reader per format.

Choosing the window (`ww-texel-picture` rule 1): the window is picked BY THE
METRIC, not by eye -- the 256x256 window of the assembled content mosaic whose
sky-AO (B) mean is the lowest, i.e. the most occluded patch of ground on the
chunk, which is the ground under the deck. The same window, same indices, is used
for all four panels.

Rule 7: each panel prints its own CROP's min/max/mean AND the whole sheet's
number beside it, and says which one the report gates on (the sheet's).

usage: make_masktexels.py
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
sys.path.insert(0, os.path.join(S, '..', '..', 'tests', 'spells'))
import lodl_channels_table as CT                                          # noqa: E402
import lodgen_vt_check as VT                                        # noqa: E402

LODT = os.path.join(S, '..', 'slab1_20260918', 'after', 'vt', 'FO4CSLOD',
                    'Commonwealth', 'Commonwealth.VT.1.lodt')
ROLE_MASK = 5
WIN, MAG = 256, 2
CH = [(0, 'R', 'roughness', 'mask-r'),
      (1, 'G', 'metallic', 'mask-g'),
      (2, 'B', 'sky AO (this is the AO WW_LODL_AO has always sampled)', 'mask-b'),
      (3, 'A', 'ground cover', 'mask-a')]
INK, DIM, RED, GREEN, BG = ((232, 232, 232), (150, 150, 150), (236, 106, 106),
                            (126, 200, 126), (22, 22, 24))


def font(sz, bold=False):
    p = r'C:\Windows\Fonts\%s' % ('arialbd.ttf' if bold else 'arial.ttf')
    return ImageFont.truetype(p, sz) if os.path.exists(p) else ImageFont.load_default()


F_NAME, F_NUM, F_SRC, F_HEAD = font(24, True), font(17), font(14), font(26, True)


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


def mosaic(v, si, ch):
    """The content texels of every present tile, laid out tile-grid order."""
    dim, b = v.stored, v.border
    c = dim - 2 * b
    W, H = v.tilesX * c, v.tilesY * c
    buf = bytearray(W * H)
    why = None
    for ty in range(v.tilesY):
        for tx in range(v.tilesX):
            i = ty * v.tilesX + tx
            if not (v.table[i]['flags'] & 1):
                continue
            plane, w = CT.sheet_plane(v, i, si, ch)
            if plane is None:
                return None, w, W, H
            why = w
            for y in range(c):
                src = (y + b) * dim + b
                dst = (ty * c + y) * W + tx * c
                buf[dst:dst + c] = plane[src:src + c]
    return buf, why, W, H


def stats(buf, W, x0, y0):
    lo, hi, s, n = 255, 0, 0, 0
    for y in range(y0, y0 + WIN):
        row = buf[y * W + x0:y * W + x0 + WIN]
        lo = min(lo, min(row))
        hi = max(hi, max(row))
        s += sum(row)
        n += len(row)
    return lo, hi, s / float(n)


def main():
    table = json.load(open(os.path.join(S, 'table_after.json')))
    v = VT.Lodv(LODT)
    si = None
    for i in range(v.sheetCount):
        if v.sheets[i]['role'] == ROLE_MASK:
            si = i
    assert si is not None, 'no mask sheet'

    planes, whys = {}, {}
    for ch, _, _, _ in CH:
        buf, why, W, H = mosaic(v, si, ch)
        planes[ch] = buf
        whys[ch] = why
    W, H = v.tilesX * (v.stored - 2 * v.border), v.tilesY * (v.stored - 2 * v.border)

    # pick the window BY THE METRIC: lowest sky-AO mean = the most occluded ground
    b = planes[2]
    best, bx, by = None, 0, 0
    for y in range(0, H - WIN + 1, 64):
        for x in range(0, W - WIN + 1, 64):
            m = stats(b, W, x, y)[2]
            if best is None or m < best:
                best, bx, by = m, x, y
    print('window %d,%d..%d,%d of the %dx%d content mosaic, B mean %.3f'
          % (bx, by, bx + WIN, by + WIN, W, H, best))

    panelW = WIN * MAG
    pad, capH, headH = 18, 96, 96
    scratch = ImageDraw.Draw(Image.new('RGB', (8, 8)))
    pageW = 4 * (panelW + 2 * pad)
    cap = ('The mask sheet (role 5, RMAOS) of %s, mip 0, CONTENT texels only, at '
           'nearest-neighbour x%d -- one texel is %d device pixels, no resample. All '
           'four panels are the SAME window: texels %d,%d..%d,%d of the %dx%d content '
           'mosaic (%d tiles of %dx%d). The window was chosen by the metric, not by '
           'eye: it is the 256x256 window with the LOWEST sky-AO mean on the chunk, '
           'i.e. the most sky-occluded ground -- the ground under the deck. Each panel '
           'prints its own crop\'s numbers AND, beside them, the whole sheet\'s numbers '
           'from the report table (section 1.3); the WHOLE-SHEET number is the one the '
           'report and the gate quote, and the crop\'s differs because it is 65,536 '
           'texels of 4,194,304 chosen for being extreme. GREEN = the channel carries '
           'values; RED = the bake does not carry it.'
           % (os.path.basename(LODT), MAG, MAG, bx, by, bx + WIN, by + WIN, W, H,
              v.tilesX * v.tilesY, v.stored - 2 * v.border, v.stored - 2 * v.border))
    caplines = wrap(scratch, cap, F_SRC, pageW - 32)
    capBlock = 10 + len(caplines) * 19 + 12
    page = Image.new('RGB', (pageW, headH + capBlock + panelW + capH + 2 * pad), BG)
    d = ImageDraw.Draw(page)
    d.text((16, 22), 'WW native LOD -- the mask sheet\'s four channels, one 256x256 '
                     'texel window, chunk 4.4.-12', font=F_HEAD, fill=INK)
    d.text((16, 58), 'SLAB1 after-bake; release/NifSkope.exe 2026-09-18 08:40:07 reads '
                     'these same texels through src/lodtsheets.cpp sheetChannel()',
           font=F_SRC, fill=DIM)
    y = headH
    for ln in caplines:
        d.text((16, y), ln, font=F_SRC, fill=DIM)
        y += 19
    top = headH + capBlock

    for k, (ch, letter, what, key) in enumerate(CH):
        px = k * (panelW + 2 * pad) + pad
        row = table.get(key, {})
        absent = 'absent' in row
        if planes[ch] is None or absent:
            d.rectangle([px, top, px + panelW, top + panelW], outline=(90, 60, 60),
                        fill=(36, 26, 26))
            for j, ln in enumerate(wrap(d, 'no texels exist to photograph: '
                                        + (row.get('absent') or whys[ch] or ''),
                                        F_SRC, panelW - 24)):
                d.text((px + 12, top + panelW // 2 - 20 + j * 18), ln, font=F_SRC,
                       fill=RED)
        else:
            crop = Image.frombytes('L', (W, H), bytes(planes[ch])).crop(
                (bx, by, bx + WIN, by + WIN)).resize((panelW, panelW), Image.NEAREST)
            page.paste(crop.convert('RGB'), (px, top))
            d.rectangle([px - 1, top - 1, px + panelW, top + panelW],
                        outline=(70, 70, 74))
        ty = top + panelW + 10
        d.text((px, ty), 'mask-%s  (%s)' % (letter.lower(), what.split('(')[0].strip()),
               font=F_NAME, fill=RED if absent else INK)
        if absent:
            for j, ln in enumerate(wrap(d, 'ABSENT on this bake -- ' + row['absent'],
                                        F_SRC, panelW)[:3]):
                d.text((px, ty + 30 + j * 17), ln, font=F_SRC, fill=RED)
        else:
            lo, hi, mean = stats(planes[ch], W, bx, by)
            d.text((px, ty + 30), 'this crop:   min %d  max %d  mean %.3f'
                   % (lo, hi, mean), font=F_NUM, fill=INK)
            const = row.get('constant')
            d.text((px, ty + 52), 'WHOLE SHEET (the report\'s number): min %g  max %g  '
                                  'mean %.3f' % (row['min'], row['max'], row['mean']),
                   font=F_SRC, fill=RED if const is not None else GREEN)
            d.text((px, ty + 70), '4,194,304 content texels, %s' % (whys[ch] or ''),
                   font=F_SRC, fill=DIM)

    out = os.path.join(S, 'images', 'mask_sheet_texels.png')
    page.save(out)
    print('%s  %dx%d  %d B' % (out, page.width, page.height, os.path.getsize(out)))


if __name__ == '__main__':
    main()
