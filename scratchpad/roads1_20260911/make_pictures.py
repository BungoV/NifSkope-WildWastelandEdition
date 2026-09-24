"""ROADS1's pictures.

  cmp_sanctuary_road.png   Bethesda's shipped far-terrain sheet | ours with
                           --no-roads | ours with --roads, the SAME chunk, the
                           same 512 texels, the same 32 world units a texel, no
                           resampling on any side, plus a 4x zoom of the
                           cul-de-sac under each.
  road_mask_and_metric.png the centreline mask -- extracted from VANILLA's sheet
                           by its colour alone -- drawn over the same three, with
                           the gate's numbers burned in.

  python make_pictures.py <vanillaDir> <cx> <cy> <before.DDS> <after.DDS>
                          <masks.npz> <metric.json> <outdir>
"""

import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                    # noqa: E402

CHROMA_T = 17.5
LUM_T = 87.7
ZOOM = (150, 120, 300, 270)      # the cul-de-sac, in texels of the 512 sheet


def font(sz):
    for p in (r'C:\Windows\Fonts\consola.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


F, FS, FT = font(15), font(13), font(21)


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    return np.array(px, dtype=np.float64).reshape(h, w, 4)[:, :, :3] * 255.0


def img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def erode(m):
    out = m.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            out &= np.roll(np.roll(m, dy, axis=0), dx, axis=1)
    return out


def panel(im, dr, x, y, pic, title, lines, w):
    im.paste(pic, (x, y))
    dr.rectangle([x, y, x + w - 1, y + pic.size[1] - 1], outline=(70, 70, 76))
    ty = y + pic.size[1] + 5
    dr.text((x + 2, ty), title, font=F, fill=(120, 190, 255))
    ty += 18
    for ln in lines:
        dr.text((x + 2, ty), ln, font=FS, fill=(150, 152, 158))
        ty += 15


def main(argv):
    vanDir, cx, cy, before, after, npz, metric, outdir = (
        argv[0], int(argv[1]), int(argv[2]), argv[3], argv[4], argv[5],
        argv[6], argv[7])
    os.makedirs(outdir, exist_ok=True)
    V = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx, cy)))
    B = sheet(before)
    A = sheet(after)
    m = json.load(open(metric))
    geo = np.load(npz)['road']

    lum = V[:, :, 0] * .2126 + V[:, :, 1] * .7152 + V[:, :, 2] * .0722
    centre = erode((V[:, :, 0] - V[:, :, 2] <= CHROMA_T) & (lum >= LUM_T))

    N = V.shape[0]
    cell = 340
    zcell = 340
    pics = [img(V), img(B), img(A)]
    pics = [p.resize((cell, cell), Image.NEAREST) for p in pics]
    x0, y0, x1, y1 = ZOOM
    zooms = [img(X[y0:y1, x0:x1]).resize((zcell, zcell), Image.NEAREST)
             for X in (V, B, A)]

    pad, top = 18, 86
    W = pad + 3 * (cell + pad)
    capH, capH2 = 90, 62
    H = top + cell + capH + zcell + capH2 + pad
    im = Image.new('RGB', (W, H), (22, 22, 24))
    dr = ImageDraw.Draw(im)
    dr.text((pad, 12), 'Roads in the far terrain: Bethesda\'s bake, ours without, '
            'ours with', font=FT, fill=(235, 235, 238))
    dr.text((pad, 40),
            'Commonwealth chunk (%d,%d), dim 4 -- cells %d..%d x %d..%d. All three '
            'are 512 x 512 texels at 32 world units a texel, the same grid, no '
            'resampling anywhere.' % (cx, cy, cx, cx + 3, cy, cy + 3),
            font=FS, fill=(150, 152, 158))
    dr.text((pad, 57),
            'Lower row: the same three at 4x over texels x %d..%d, y %d..%d -- the '
            'cul-de-sac and its island.' % (x0, x1, y0, y1),
            font=FS, fill=(150, 152, 158))

    titles = [
        ('VANILLA: Commonwealth.4.%d.%d.DDS' % (cx, cy),
         ['Bethesda\'s own shipped chunk sheet',
          'the road network is plainly in it']),
        ('OURS, --no-roads (the rung)',
         ['byte-identical to the bake before this lane',
          'mean error vs vanilla: whole tile %.2f of 255'
          % m['wholeTile_--no-roads'],
          'on the road centreline %.2f' % m['centreline_--no-roads']]),
        ('OURS, --roads (shipped default)',
         ['the road meshes rasterised into the colour',
          'mean error vs vanilla: whole tile %.2f of 255'
          % m['wholeTile_--roads'],
          'on the road centreline %.2f' % m['centreline_--roads']]),
    ]
    for k in range(3):
        x = pad + k * (cell + pad)
        panel(im, dr, x, top, pics[k], titles[k][0], titles[k][1], cell)
        panel(im, dr, x, top + cell + capH, zooms[k], titles[k][0].split(':')[0]
              + ' -- 4x', [], zcell)
    p1 = os.path.join(outdir, 'cmp_sanctuary_road.png')
    im.save(p1)
    print('wrote %s  %dx%d' % (p1, W, H))

    # ---------------- picture 2: the mask and the metric ----------------
    def overlay(X, mask, colour):
        o = np.clip(X, 0, 255).astype(np.uint8).copy()
        o[mask] = (o[mask] * 0.30 + np.array(colour) * 0.70).astype(np.uint8)
        return Image.fromarray(o)

    outs = [overlay(V, centre, (255, 70, 70)),
            overlay(B, centre, (255, 70, 70)),
            overlay(A, centre, (255, 70, 70))]
    outs = [p.resize((cell, cell), Image.NEAREST) for p in outs]
    maskpic = Image.fromarray(
        np.stack([(centre * 255).astype(np.uint8),
                  (geo * 90).astype(np.uint8),
                  (geo * 200).astype(np.uint8)], axis=2)).resize(
        (cell, cell), Image.NEAREST)

    W2 = pad + 4 * (cell + pad)
    H2 = top + cell + 150 + pad
    im2 = Image.new('RGB', (W2, H2), (22, 22, 24))
    d2 = ImageDraw.Draw(im2)
    d2.text((pad, 12), 'The road-presence gate: the mask, and what each sheet '
            'does on it', font=FT, fill=(235, 235, 238))
    d2.text((pad, 40),
            'The mask is taken from VANILLA\'s sheet alone, by colour: chroma '
            '(R-B) <= %.1f and luminance >= %.1f, then eroded 3x3 so only cores '
            'survive. %d texels.' % (CHROMA_T, LUM_T, m['centrelineTexels']),
            font=FS, fill=(150, 152, 158))
    d2.text((pad, 57),
            'Its control: %.1f%% of it falls inside the INDEPENDENT geometric '
            'projection of the road meshes, against %.1f%%..%.1f%% for the same '
            'projection displaced five ways.'
            % (100 * m['extractorInsideGeometry'],
               100 * min(m['extractorControl']), 100 * max(m['extractorControl'])),
            font=FS, fill=(150, 152, 158))
    caps = [
        ('THE MASK (red) over the geometry (blue)',
         ['red = road-coloured in vanilla\'s sheet',
          'blue = our top-down projection of the meshes',
          'they are the same network; neither was fitted']),
        ('VANILLA under the mask',
         ['ceiling: vanilla against itself = %.3f' % m['ceiling_vanilla'],
          'by construction, and it is what 1.000 means']),
        ('OURS --no-roads under the mask',
         ['floor: %.4f of the mask within 16 of 255' % m['floor_ours'],
          'this is the bake with no road in it']),
        ('OURS --roads under the mask',
         ['after: %.4f  -- %.2fx the floor' % (m['after_ours'],
                                               m['after_ours'] / max(1e-9, m['floor_ours'])),
          'the ground around it reaches %.4f' % m['reference_background_roads'],
          'pre-registered pass: >= 2x floor and >= 0.8x that']),
    ]
    for k, pic in enumerate([maskpic] + outs):
        x = pad + k * (cell + pad)
        panel(im2, d2, x, top, pic, caps[k][0], caps[k][1], cell)
    p2 = os.path.join(outdir, 'road_mask_and_metric.png')
    im2.save(p2)
    print('wrote %s  %dx%d' % (p2, W2, H2))


if __name__ == '__main__':
    main(sys.argv[1:])
