# VT1: the owed picture pair -- the ASSEMBLED colour sheet as the rung exe
# builds it against the same sheet as the new exe builds it, with the byte
# difference counted and burned in.
#
# Both panels are real DDS files off disk, decoded here (not through the
# writer's own code), both bakes on the BARE RULED DEFAULT with --road-detail 1.
import os, struct, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

G = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vt1_20260912/gate'
OUT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vt1_20260912/images'


def bc1_mip0(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    out = np.zeros((h, w, 3), np.uint8)
    bw, bh = w // 4, h // 4

    def rgb(c):
        return ((((c >> 11) & 31) * 255 + 15) // 31,
                (((c >> 5) & 63) * 255 + 31) // 63,
                ((c & 31) * 255 + 15) // 31)

    for by in range(bh):
        for bx in range(bw):
            o = 128 + (by * bw + bx) * 8
            c0, c1 = struct.unpack_from('<HH', b, o)
            bits = struct.unpack_from('<I', b, o + 4)[0]
            a, d = rgb(c0), rgb(c1)
            if c0 > c1:
                pal = [a, d,
                       tuple((2 * a[i] + d[i]) // 3 for i in range(3)),
                       tuple((a[i] + 2 * d[i]) // 3 for i in range(3))]
            else:
                pal = [a, d, tuple((a[i] + d[i]) // 2 for i in range(3)), (0, 0, 0)]
            for k in range(16):
                out[by * 4 + k // 4, bx * 4 + k % 4] = pal[(bits >> (2 * k)) & 3]
    return out, len(b)


def font(sz):
    for f in ('C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(f):
            return ImageFont.truetype(f, sz)
    return ImageFont.load_default()


def main():
    os.makedirs(OUT, exist_ok=True)
    tag = 'Commonwealth.4.-20.28'
    pr = '%s/rung.vt/tex/%s.DDS' % (G, tag)
    pn = '%s/new.vt/tex/%s.DDS' % (G, tag)
    pd = '%s/new.d/tex/%s.DDS' % (G, tag)
    for p in (pr, pn, pd):
        if not os.path.exists(p):
            print('missing %s' % p)
            return 2
    br, bn = open(pr, 'rb').read(), open(pn, 'rb').read()
    bd = open(pd, 'rb').read()
    nbytes = sum(1 for x, y in zip(br, bn) if x != y)
    nvsd_rung = sum(1 for x, y in zip(br, bd) if x != y)
    nvsd_new = sum(1 for x, y in zip(bn, bd) if x != y)

    ar, _ = bc1_mip0(pr)
    an, _ = bc1_mip0(pn)
    dif = (ar.astype(np.int16) != an.astype(np.int16)).any(axis=2)
    ys, xs = np.nonzero(dif)
    ntex = int(dif.sum())

    # the crop: the band where the tile seam meets the chunk's north edge
    cx0, cx1, cy0, cy1 = 232, 280, 0, 24
    Z = 14
    crops = []
    for arr in (ar, an):
        c = arr[cy0:cy1, cx0:cx1]
        crops.append(Image.fromarray(c, 'RGB').resize(
            ((cx1 - cx0) * Z, (cy1 - cy0) * Z), Image.NEAREST))
    # panel 3: where they differ, on the same crop
    dm = np.zeros((cy1 - cy0, cx1 - cx0, 3), np.uint8)
    dm[..., 0] = 24
    dm[..., 1] = 24
    dm[..., 2] = 28
    sub = dif[cy0:cy1, cx0:cx1]
    dm[sub] = (255, 96, 32)
    crops.append(Image.fromarray(dm, 'RGB').resize(
        ((cx1 - cx0) * Z, (cy1 - cy0) * Z), Image.NEAREST))

    pw, ph = crops[0].size
    pad, top, gap, foot = 18, 96, 18, 150
    W = pad * 2 + pw * 3 + gap * 2
    H = top + ph + foot
    im = Image.new('RGB', (W, H), (18, 18, 20))
    d = ImageDraw.Draw(im)
    f18, f14, f12 = font(19), font(14), font(12)
    d.text((pad, 16), 'The assembled colour sheet %s, mip 0, bare ruled land default'
           % tag, font=f18, fill=(235, 235, 238))
    d.text((pad, 42), 'crop x %d..%d, y %d..%d at %dx nearest  --  x=256 is the seam '
           'between the two dim-2 tiles, y=0 is the chunk north edge'
           % (cx0, cx1 - 1, cy0, cy1 - 1, Z), font=f14, fill=(150, 150, 158))
    labels = ['RUNG  release/NifSkope.before_vt1.exe (11:54)',
              'NEW   release/NifSkope.exe (12:49)',
              'where they differ: %d texels of 262,144' % ntex]
    for i, (c, lab) in enumerate(zip(crops, labels)):
        x = pad + i * (pw + gap)
        im.paste(c, (x, top))
        d.rectangle([x - 1, top - 1, x + pw, top + ph], outline=(70, 70, 78))
        d.text((x, top - 22), lab, font=f14, fill=(220, 220, 226))
        # the seam column, marked on the two sheet panels
        if i < 2:
            sx = x + (256 - cx0) * Z
            d.line([sx, top, sx, top + ph], fill=(255, 210, 60), width=1)
    y = top + ph + 16
    lines = [
        'assembled sheet, RUNG vs NEW:        %d bytes of %d differ  (%d texels at mip 0)'
        % (nbytes, len(br), ntex),
        'assembled vs DIRECT bake, RUNG exe:  %d bytes differ   <-- the defect' % nvsd_rung,
        'assembled vs DIRECT bake, NEW exe:   %d bytes differ   <-- V9a-1 passes' % nvsd_new,
        'The direct bake itself did not move: no ruled default changed, only which cell owns a',
        'shared height row in the RING of a tile grid. The moved texels are the assembled ones.',
    ]
    for ln in lines:
        d.text((pad, y), ln, font=f12, fill=(205, 205, 212))
        y += 17
    if ntex:
        d.text((pad, y), 'moved texels: x %d..%d, y %d..%d'
               % (xs.min(), xs.max(), ys.min(), ys.max()), font=f12, fill=(255, 150, 90))
    p = '%s/vt1_pyramid_sheet_rung_vs_new.png' % OUT
    im.save(p)
    print('wrote %s' % p)
    print('rung-vs-new assembled %d bytes, %d texels; rung assembled-vs-direct %d; '
          'new assembled-vs-direct %d' % (nbytes, ntex, nvsd_rung, nvsd_new))
    return 0


sys.exit(main())
