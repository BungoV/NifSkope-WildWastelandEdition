#!/usr/bin/env python3
"""Lane CHANVIEW1 step 4 -- the SOURCE gloss beside the baked roughness (bungo 06:1x).

bungo asked for roughness/metallic "since these are baked from legacy textures and
materials, not .pbrm", and for one picture in which the inversion can be read.

The law, one place in the tree (`src/lodgen.cpp`):

    layerRough()  ->  1.0f - lodgenLegacyGloss( m.glossScale, specGreen )
    lodgenLegacyGloss( s, g ) = clamp( clamp(s,0,1) * g, 0, 1 )

and for a TERRAIN layer the smoothness argument is the literal `1.0f`
(`lodgen.cpp:10825`, the LTEX texture-set resolve), so on this bake the law is
exactly `roughness = 1 - gloss` and nothing else. The gloss is the `_s` map's
GREEN channel (`out->roughnessChannel = 1`, `lodgen.cpp:1907`).

This picture reads that `_s` map out of the vanilla corpus, mip 0, and puts it
beside the same texels through the law. Panel 3 is the SHIPPED sheet's R so the
reader can see the range the chunk actually carries -- it is a blend of up to 17
layers, which the caption says, so no one reads it as this layer alone.

usage: make_gloss.py
"""
import json
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
sys.path.insert(0, os.path.join(S, '..', '..', 'tests', 'spells'))
import lodl_channels_table as CT                                          # noqa: E402
import lodgen_vt_check as VT                                        # noqa: E402

VANILLA = r'E:/Tools/Fallout 4/DataUnpacked/Data'
LAYER = 'textures/landscape/ground/dirtgravel01_s.dds'
LODT = os.path.join(S, '..', 'slab1_20260918', 'after', 'vt', 'FO4CSLOD',
                    'Commonwealth', 'Commonwealth.VT.1.lodt')
WIN, MAG = 256, 2
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


def bc5_green(path):
    """Mip 0's GREEN channel of a BC5U .dds -- the second BC4 block of each pair,
    decoded by the same interpolated-endpoint routine the lane's BC3 alpha uses."""
    b = open(path, 'rb').read()
    hdr = struct.unpack('<7I', b[4:32])
    h, w = hdr[2], hdr[3]
    assert b[84:88] == b'BC5U', b[84:88]
    o = 128
    blocks = w // 4
    plane = bytearray(w * h)
    for by in range(h // 4):
        for bx in range(blocks):
            base = o + (by * blocks + bx) * 16
            texels = CT.bc3_alpha(b[base + 8:base + 16])     # block 2 = GREEN
            for i in range(16):
                plane[(by * 4 + (i >> 2)) * w + bx * 4 + (i & 3)] = texels[i]
    return plane, w, h


def stats(buf, W, x0, y0, n=WIN):
    lo, hi, s = 255, 0, 0
    for y in range(y0, y0 + n):
        row = buf[y * W + x0:y * W + x0 + n]
        lo = min(lo, min(row))
        hi = max(hi, max(row))
        s += sum(row)
    return lo, hi, s / float(n * n)


def sheet_r_crop(bx, by):
    v = VT.Lodv(LODT)
    si = [i for i in range(v.sheetCount) if v.sheets[i]['role'] == 5][-1]
    dim, b = v.stored, v.border
    c = dim - 2 * b
    W = v.tilesX * c
    buf = bytearray(W * v.tilesY * c)
    for ty in range(v.tilesY):
        for tx in range(v.tilesX):
            i = ty * v.tilesX + tx
            if not (v.table[i]['flags'] & 1):
                continue
            plane, _ = CT.sheet_plane(v, i, si, 0)
            for y in range(c):
                src = (y + b) * dim + b
                dst = (ty * c + y) * W + tx * c
                buf[dst:dst + c] = plane[src:src + c]
    return buf, W, v.tilesY * c


def main():
    src = os.path.join(VANILLA, LAYER)
    gloss, W, H = bc5_green(src)
    # rule 1: the crop where the thing IS -- the window whose gloss varies most
    best = None
    for y in range(0, H - WIN + 1, 128):
        for x in range(0, W - WIN + 1, 128):
            lo, hi, m = stats(gloss, W, x, y)
            if best is None or (hi - lo) > best[0]:
                best = (hi - lo, x, y, lo, hi, m)
    _, gx, gy, glo, ghi, gmean = best
    print('gloss window %d,%d of %dx%d: min %d max %d mean %.3f'
          % (gx, gy, W, H, glo, ghi, gmean))

    g = Image.frombytes('L', (W, H), bytes(gloss)).crop((gx, gy, gx + WIN, gy + WIN))
    r = Image.eval(g, lambda p: 255 - p)                # THE LAW, smoothness = 1
    sbuf, SW, SH = sheet_r_crop(0, 0)
    sx, sy = 384, 128                                   # the same window as the mask picture
    slo, shi, smean = stats(sbuf, SW, sx, sy)
    s_img = Image.frombytes('L', (SW, SH), bytes(sbuf)).crop(
        (sx, sy, sx + WIN, sy + WIN))

    table = json.load(open(os.path.join(S, 'table_after.json')))
    panelW = WIN * MAG
    pad, headH = 18, 96
    pageW = 3 * (panelW + 2 * pad)
    scratch = ImageDraw.Draw(Image.new('RGB', (8, 8)))
    cap = ('The inversion, read on one layer. LEFT: the SOURCE gloss -- the GREEN '
           'channel of %s (BC5U, mip 0, 1024x1024), the map the bake reads, at '
           'nearest-neighbour x%d. MIDDLE: the same texels through the bake\'s law, '
           'roughness = 1 - lodgenLegacyGloss(smoothness, glossGreen) with smoothness = '
           '1.0 for every terrain layer (src/lodgen.cpp:10825 passes the literal 1.0f), '
           'so on this bake the law is exactly roughness = 255 - gloss and the two '
           'panels are complements texel for texel: bright gloss is dark roughness. '
           'RIGHT: the SHIPPED mask sheet\'s R for scale -- it is the blend of up to 17 '
           'layers over that texel, NOT this layer alone, which is why its range is '
           'narrower than the source\'s. The crop was chosen by the metric (the 256x256 '
           'window of the source with the widest gloss range), not by eye. metallic is '
           'constant 0 across the chunk because a legacy material has no metallic to '
           'read and bungo\'s ruling is that it is derived from a PBRM or not at all '
           '(bake census: maskPbrm 0, maskLegacyInverted 17, maskMetalMaps 0).'
           % (LAYER, MAG))
    caplines = wrap(scratch, cap, F_SRC, pageW - 32)
    capBlock = 10 + len(caplines) * 19 + 12
    page = Image.new('RGB', (pageW, headH + capBlock + panelW + 108), BG)
    d = ImageDraw.Draw(page)
    d.text((16, 22), 'WW native LOD -- source gloss vs baked roughness, layer '
                     'dirtgravel01 (bungo 06:1x)', font=F_HEAD, fill=INK)
    d.text((16, 58), 'vanilla corpus %s; baked sheet = SLAB1 after-bake '
                     'Commonwealth.VT.1.lodt' % VANILLA, font=F_SRC, fill=DIM)
    y = headH
    for ln in caplines:
        d.text((16, y), ln, font=F_SRC, fill=DIM)
        y += 19
    top = headH + capBlock

    panels = [
        (g, 'SOURCE gloss  (_s green)', glo, ghi, gmean,
         'vanilla dirtgravel01_s.dds, mip 0, 65,536 texels of this crop', GREEN),
        (r, 'BAKED roughness  (1 - gloss)', 255 - ghi, 255 - glo, 255.0 - gmean,
         'the law applied to the panel on the left, texel for texel', GREEN),
        (s_img, 'SHIPPED sheet R  (all layers)', slo, shi, smean,
         'Commonwealth.VT.1.lodt role 5 R; whole sheet min %g max %g mean %.3f'
         % (table['mask-r']['min'], table['mask-r']['max'], table['mask-r']['mean']),
         INK),
    ]
    for k, (im, name, lo, hi, mean, note, col) in enumerate(panels):
        px = k * (panelW + 2 * pad) + pad
        page.paste(im.resize((panelW, panelW), Image.NEAREST).convert('RGB'), (px, top))
        d.rectangle([px - 1, top - 1, px + panelW, top + panelW], outline=(70, 70, 74))
        ty = top + panelW + 10
        d.text((px, ty), name, font=F_NAME, fill=INK)
        d.text((px, ty + 30), 'min %g   max %g   mean %.3f' % (lo, hi, mean),
               font=F_NUM, fill=col)
        for j, ln in enumerate(wrap(d, note, F_SRC, panelW)[:2]):
            d.text((px, ty + 52 + j * 17), ln, font=F_SRC, fill=DIM)

    out = os.path.join(S, 'images', 'gloss_vs_roughness.png')
    page.save(out)
    print('%s  %dx%d  %d B' % (out, page.width, page.height, os.path.getsize(out)))
    print('crop gloss mean %.3f, crop roughness mean %.3f, sum %.3f (must be 255)'
          % (gmean, 255.0 - gmean, gmean + (255.0 - gmean)))


if __name__ == '__main__':
    main()
