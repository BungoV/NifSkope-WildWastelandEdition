"""PIC-GRASS: what the grass tint puts into the far colour sheet.

One picture, four panels of the SAME 512 x 512 texels of Commonwealth chunk
(-20,20) at dim 4 -- the Sanctuary loop-road chunk, so roads are in all three
bakes:

  1  VANILLA   Bethesda's shipped Commonwealth.4.-20.20.DDS
  2  OURS      --cover, grass tint at the shipped default 0.35
  3  OURS      --cover --grass-tint 0  (cover plane written, albedo untinted)
  4  |2 - 3| x 4, the tint's own footprint

Row two is the same four at 4x over the 128-texel window carrying the most
ground cover, so the tint is legible at texel scale.

  python make_picture.py <vanillaDir> <cx> <cy> <default.DDS> <tint0.DDS>
                         <default_data.DDS> <outdir>
"""

import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                       # noqa: E402

ZOOMW = 128          # texels of the 4x window
CELL = 512


def font(sz):
    for p in (r'C:\Windows\Fonts\consola.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


F, FS, FT = font(16), font(14), font(23)


def rgba(path):
    t = Dds(path)
    px, w, h = t._level(0)
    a = np.array(px, dtype=np.float64).reshape(h, w, 4) * 255.0
    return a, t


def sheet(path):
    a, _ = rgba(path)
    return a[:, :, :3]


def cover_plane(path):
    """The ground-cover byte per texel, by docs/LODGEN_TERRAIN_VT.md 1.4.

    A DXT5 _data.DDS stamped 'WWCV' in dwReserved1 carries cover in its alpha.
    Anything else -- DXT1, or DXT5 whose alpha is constant 255 -- carries NO
    cover and reads as zero everywhere.
    """
    a, t = rgba(path)
    hdr = np.frombuffer(open(path, 'rb').read(128), dtype='<u4')
    stamped = (t.fourcc == b'DXT5') and (int(hdr[8]) == 0x56435757)
    alpha = np.rint(a[:, :, 3]).astype(np.int32)
    if not stamped or (alpha.min() == 255 and alpha.max() == 255):
        return np.zeros(alpha.shape, dtype=np.int32), False, int(hdr[8]), \
            t.fourcc.decode('latin-1')
    return alpha, True, int(hdr[8]), t.fourcc.decode('latin-1')


def err(X, V, m=None):
    """PARITY's own metric: max over channels of the absolute difference."""
    e = np.abs(X - V).max(2)
    return float(e.mean()) if m is None else float(e[m].mean())


def img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def panel(im, dr, x, y, pic, title, lines, w, tcol=(120, 190, 255)):
    im.paste(pic, (x, y))
    dr.rectangle([x, y, x + w - 1, y + pic.size[1] - 1], outline=(70, 70, 76))
    ty = y + pic.size[1] + 6
    dr.text((x + 2, ty), title, font=F, fill=tcol)
    ty += 20
    for ln in lines:
        dr.text((x + 2, ty), ln, font=FS, fill=(150, 152, 158))
        ty += 17


def main(argv):
    vanDir, cx, cy, pDef, pT0, pData, outdir = (
        argv[0], int(argv[1]), int(argv[2]), argv[3], argv[4], argv[5], argv[6])
    os.makedirs(outdir, exist_ok=True)

    V = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx, cy)))
    A = sheet(pDef)                 # ours, tint 0.35
    B = sheet(pT0)                  # ours, tint 0
    cov, stamped, stamp, fcc = cover_plane(pData)
    assert V.shape == A.shape == B.shape == (512, 512, 3), \
        (V.shape, A.shape, B.shape)
    m = cov > 0

    D = np.abs(A - B)
    dmax = D.max(2)

    res = dict(
        chunk=[cx, cy], dim=4,
        coverStamped=bool(stamped), coverStamp='0x%08X' % stamp,
        dataFourCC=fcc,
        coverTexels=int(m.sum()), texels=int(cov.size),
        coverMax=int(cov.max()), coverMeanOverCovered=(
            float(cov[m].mean()) if m.any() else 0.0),
        errWhole_vanilla_vs_default=err(A, V),
        errWhole_vanilla_vs_tint0=err(B, V),
        errCover_vanilla_vs_default=(err(A, V, m) if m.any() else None),
        errCover_vanilla_vs_tint0=(err(B, V, m) if m.any() else None),
        tintWhole_mean=float(dmax.mean()), tintWhole_max=float(dmax.max()),
        tintCover_mean=(float(dmax[m].mean()) if m.any() else None),
        tintCover_p95=(float(np.percentile(dmax[m], 95)) if m.any() else None),
        tintOutsideCover_max=(float(dmax[~m].max()) if (~m).any() else None),
        identicalOutsideCover=bool((~m).all() or dmax[~m].max() == 0),
    )

    # --- the 4x window: the ZOOMW box carrying the most cover -------------
    if m.any():
        ii = np.cumsum(np.cumsum(cov.astype(np.int64), 0), 1)
        ii = np.pad(ii, ((1, 0), (1, 0)))
        best, bxy = -1, (0, 0)
        for yy in range(0, 512 - ZOOMW + 1, 8):
            for xx in range(0, 512 - ZOOMW + 1, 8):
                s = (ii[yy + ZOOMW, xx + ZOOMW] - ii[yy, xx + ZOOMW]
                     - ii[yy + ZOOMW, xx] + ii[yy, xx])
                if s > best:
                    best, bxy = s, (xx, yy)
        zx, zy = bxy
    else:
        zx, zy = 192, 192
    res['zoom'] = [zx, zy, zx + ZOOMW, zy + ZOOMW]
    zm = m[zy:zy + ZOOMW, zx:zx + ZOOMW]
    res['zoomCoverTexels'] = int(zm.sum())
    json.dump(res, open(os.path.join(outdir, 'numbers.json'), 'w'), indent=1)
    for k, v in sorted(res.items()):
        print('  %-32s %s' % (k, v))

    # --- compose ----------------------------------------------------------
    pics = [img(V), img(A), img(B), img(np.clip(D * 4.0, 0, 255))]
    zoom = [p.crop((zx, zy, zx + ZOOMW, zy + ZOOMW)).resize(
        (CELL, CELL), Image.NEAREST) for p in pics]

    pad, top = 20, 104
    W = pad + 4 * (CELL + pad)
    capH, capH2 = 96, 50
    H = top + CELL + capH + CELL + capH2 + pad
    im = Image.new('RGB', (W, H), (22, 22, 24))
    dr = ImageDraw.Draw(im)
    dr.text((pad, 12), 'The grass tint in the far colour sheet: vanilla, ours '
            'with the tint, ours without, and the tint alone',
            font=FT, fill=(235, 235, 238))
    dr.text((pad, 44),
            'Commonwealth chunk (%d,%d), dim 4 -- cells %d..%d x %d..%d. All '
            'four panels are the SAME 512 x 512 texels at 32 world units a '
            'texel, the same grid, no resampling on any side. Roads are baked '
            'in both of our panels (--roads is on by default).'
            % (cx, cy, cx, cx + 3, cy, cy + 3), font=FS, fill=(150, 152, 158))
    dr.text((pad, 63),
            'The cover plane comes from our own bake\'s %s_data.DDS alpha '
            '(DXT5, stamped %s): %d of %d texels carry cover > 0, max %d. '
            '"cover>0" below means exactly those texels.'
            % (os.path.basename(pData).replace('_data.DDS', ''),
               res['coverStamp'], res['coverTexels'], res['texels'],
               res['coverMax']), font=FS, fill=(150, 152, 158))
    dr.text((pad, 82),
            'Lower row: the same four at 4x over texels x %d..%d, y %d..%d -- '
            'the 128-texel window carrying the most ground cover (%d covered '
            'texels in it), chosen from the cover plane, not by eye.'
            % (zx, zx + ZOOMW, zy, zy + ZOOMW, res['zoomCoverTexels']),
            font=FS, fill=(150, 152, 158))

    def e2(a, b):
        return ('mean colour error vs vanilla: whole crop %.2f of 255' % a,
                'on cover>0 texels only: %.2f' % b)

    caps = [
        ('VANILLA: Commonwealth.4.%d.%d.DDS' % (cx, cy),
         ["Bethesda's own shipped chunk sheet",
          'the reference both of ours are scored against',
          'whole crop 0.00 of 255 against itself, by construction']),
        ('OURS: --cover, grass tint 0.35 (shipped default)',
         list(e2(res['errWhole_vanilla_vs_default'],
                 res['errCover_vanilla_vs_default'])) +
         ['the cover plane is written AND mixed into the albedo']),
        ('OURS: --cover --grass-tint 0',
         list(e2(res['errWhole_vanilla_vs_tint0'],
                 res['errCover_vanilla_vs_tint0'])) +
         ['the cover plane is written, the albedo is left alone']),
        ('|default - no-tint| x 4  (the tint alone)',
         ['mean |difference|: whole crop %.2f of 255, max %.0f'
          % (res['tintWhole_mean'], res['tintWhole_max']),
          'on cover>0 texels only: mean %.2f, p95 %.0f'
          % (res['tintCover_mean'], res['tintCover_p95']),
          'outside cover>0 the two sheets differ by at most %.0f'
          % res['tintOutsideCover_max']]),
    ]
    for k in range(4):
        x = pad + k * (CELL + pad)
        panel(im, dr, x, top, pics[k], caps[k][0], caps[k][1], CELL)
        panel(im, dr, x, top + CELL + capH, zoom[k],
              caps[k][0].split(':')[0].split('  ')[0] + ' -- 4x', [], CELL,
              tcol=(150, 152, 158))
    p = os.path.join(outdir, 'cmp_grass_tint.png')
    im.save(p)
    print('wrote %s  %dx%d' % (p, W, H))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
