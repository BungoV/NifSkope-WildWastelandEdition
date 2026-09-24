#!/usr/bin/env python
"""The one picture this lane owes: WHY V9c was red, with the labels burned in.

Left  -- the two `_msn` sheets meeting at their chunk seam, decoded with the
         DXT1 reader the check used until today.
Right -- the same two sheets, same crop, decoded as the DXT5 (BC3) they are.

The numbers under each half are the check's own readings, not new ones: the
interior control and the E/W seam ratio the harness prints.
"""
import struct
import sys

from PIL import Image, ImageDraw

D = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/gensmall1_20260916/v9c/tex'
OUT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/gensmall1_20260916/v9c_dxt1_vs_bc3.png'


def c565(c):
    return ((((c >> 11) & 31) * 255 + 15) // 31,
            (((c >> 5) & 63) * 255 + 31) // 63,
            ((c & 31) * 255 + 15) // 31)


def decode(path, block):
    """block = 8 for the old DXT1 read, 16 for the correct BC3 read."""
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    px = [(0, 0, 0)] * (w * h)
    p = 128
    cofs = 0 if block == 8 else 8
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            c0, c1 = struct.unpack_from('<HH', b, p + cofs)
            idx = struct.unpack_from('<I', b, p + cofs + 4)[0]
            p += block
            p0, p1 = c565(c0), c565(c1)
            if block == 8 and c0 <= c1:
                pal = [p0, p1, tuple((p0[k] + p1[k]) // 2 for k in range(3)), (0, 0, 0)]
            else:
                pal = [p0, p1,
                       tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x < w and y < h:
                        px[y * w + x] = pal[(idx >> (2 * (j * 4 + i))) & 3]
    im = Image.new('RGB', (w, h))
    im.putdata(px)
    return im


CROP = 160          # texels either side of the seam
SC = 2              # on-screen scale


def pair(block):
    """The west sheet's last CROP columns beside the east sheet's first CROP."""
    W = decode(D + '/Commonwealth.4.-24.24_msn.DDS', block)
    E = decode(D + '/Commonwealth.4.-20.24_msn.DDS', block)
    w, h = W.size
    out = Image.new('RGB', (CROP * 2, CROP))
    out.paste(W.crop((w - CROP, 0, w, CROP)), (0, 0))
    out.paste(E.crop((0, 0, CROP, CROP)), (CROP, 0))
    return out.resize((CROP * 2 * SC, CROP * SC), Image.NEAREST)


left = pair(8)
right = pair(16)
pw, ph = left.size
PAD, TOP, BOT = 24, 84, 118
canvas = Image.new('RGB', (PAD * 3 + pw * 2, TOP + ph + BOT), (22, 22, 24))
d = ImageDraw.Draw(canvas)

d.text((PAD, 14), 'lodgen_terrain_vt.sh check V9c -- why it was red', fill=(245, 245, 245))
d.text((PAD, 34), 'Commonwealth.4.-24.24_msn.DDS | Commonwealth.4.-20.24_msn.DDS, 160 texels either side of the chunk seam',
       fill=(170, 170, 178))
d.text((PAD, 50), 'the file is DXT5 (BC3), 512x512, 10 mips, 349,680 bytes -- byte-identical to vanilla',
       fill=(170, 170, 178))

canvas.paste(left, (PAD, TOP))
canvas.paste(right, (PAD * 2 + pw, TOP))
for x0 in (PAD, PAD * 2 + pw):
    d.rectangle([x0 - 1, TOP - 1, x0 + pw, TOP + ph], outline=(90, 90, 96))
    # the seam itself
    d.line([x0 + pw // 2, TOP, x0 + pw // 2, TOP + ph], fill=(255, 90, 90), width=1)

y = TOP + ph + 10
d.text((PAD, y), 'DECODED AS DXT1 -- what the check did until 2026-09-16', fill=(255, 140, 140))
d.text((PAD, y + 18), 'interior control 13.243 / 12.182  (band 20.0 .. 36.0)', fill=(225, 225, 230))
d.text((PAD, y + 34), 'E/W seam ratio 14.20  against a bar of 3.20  -> RED', fill=(225, 225, 230))
d.text((PAD, y + 52), '8-byte blocks walked through a 16-byte-block payload:', fill=(160, 160, 168))
d.text((PAD, y + 66), 'from block 2 on it is reading alpha bytes as colour endpoints', fill=(160, 160, 168))

x = PAD * 2 + pw
d.text((x, y), 'DECODED AS BC3 -- what it does now', fill=(140, 230, 160))
d.text((x, y + 18), 'interior control 28.804 / 25.488  (band 20.0 .. 36.0)', fill=(225, 225, 230))
d.text((x, y + 34), 'E/W seam ratio 0.990  against a bar of 1.15  -> green', fill=(225, 225, 230))
d.text((x, y + 52), 'refuter in the same check: neighbours shifted 16 texels', fill=(160, 160, 168))
d.text((x, y + 66), 'read 1.330 and 1.602 and break 2 of 2 bars', fill=(160, 160, 168))

d.text((PAD, canvas.size[1] - 16), 'lane GENSMALL1, 2026-09-16, release/NifSkope.exe 22,300,160 B 13:52:56',
       fill=(120, 120, 128))
canvas.save(OUT)
print('wrote %s  %dx%d' % (OUT, canvas.size[0], canvas.size[1]))
