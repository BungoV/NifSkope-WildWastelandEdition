"""Lane GROUND1 Part A pictures.

Every panel is assembled from a real .lodt off disk, through the same reader the
harness uses, with the codec picked per tile (maskdec.py -- a cover tile's mask
sheet is BC3, and reading it as BC1 is what MISTAKES_ENTRIES.md is about).

  a_cmp_terrain_ao.png      the AO byte: off | on | (off - on) x 4
  a_cmp_terrain_colour.png  the colour sheet lit by that AO: off | on | the two
                            side by side over one strip of ground
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
sys.path.insert(0, HERE)
import lodgen_vt_check as V           # noqa: E402
import maskdec                        # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), 'images')


def mosaic(path, what):
    """Content-only mosaic of a whole level, north row first.

    `what` is 'ao' (the mask sheet's B byte) or 'rgb' (the colour sheet)."""
    v = V.Lodv(path)
    b, n, c = v.border, v.stored, v.content
    W, H = v.tilesX * c, v.tilesY * c
    img = np.zeros((H, W) if what == 'ao' else (H, W, 3), np.uint8)
    for ty in range(v.tilesY):
        for tx in range(v.tilesX):
            i = ty * v.tilesX + tx
            if what == 'ao':
                rows, _ = maskdec.mask_rows(v, i)
                if rows is None:
                    continue
                a = np.array(rows, np.uint8)[b:n - b, b:n - b, 2]
            else:
                rows = v.colour(i)
                if rows is None:
                    continue
                a = np.array(rows, np.uint8)[b:n - b, b:n - b, :]
            img[ty * c:(ty + 1) * c, tx * c:(tx + 1) * c] = a
    return v, img


def label(im, texts, panel_w, pad, title):
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, im.width, pad], fill=(16, 16, 16))
    d.text((6, 4), title, fill=(235, 235, 235))
    for k, t in enumerate(texts):
        d.text((pad // 2 + k * (panel_w + pad // 2), pad - 14), t,
               fill=(235, 235, 235))
    return im


def three_panel(name, title, panels, captions, scale=1):
    h, w = panels[0].shape[:2]
    if scale != 1:
        panels = [np.array(Image.fromarray(p).resize(
            (w * scale, h * scale), Image.NEAREST)) for p in panels]
        h, w = panels[0].shape[:2]
    pad = 34
    gap = pad // 2
    out = Image.new('RGB', (gap + 3 * (w + gap), pad + h + gap), (16, 16, 16))
    for k, p in enumerate(panels):
        im = Image.fromarray(p)
        if im.mode != 'RGB':
            im = im.convert('RGB')
        out.paste(im, (gap + k * (w + gap), pad))
    label(out, captions, w, pad, title)
    p = os.path.join(OUT, name)
    out.save(p)
    print('%s  %dx%d  %d bytes' % (p, out.width, out.height, os.path.getsize(p)))


def main(off_dir, on_dir, level='2'):
    off = os.path.join(off_dir, 'Terrain', 'Commonwealth.VT.%s.lodt' % level)
    on = os.path.join(on_dir, 'Terrain', 'Commonwealth.VT.%s.lodt' % level)
    v, ao_off = mosaic(off, 'ao')
    _, ao_on = mosaic(on, 'ao')
    _, col_off = mosaic(off, 'rgb')

    d = (ao_off.astype(np.int16) - ao_on.astype(np.int16))
    print('AO off mean %.2f   on mean %.2f   drop mean %.2f max %d'
          % (ao_off.mean(), ao_on.mean(), d[d > 0].mean(), d.max()))
    # x1, not x4: at the shipped strength the mean drop is already 79 of
    # 255, so a x4 panel is a white silhouette and says less than the
    # drop itself does.
    diff = np.clip(d, 0, 255).astype(np.uint8)

    three_panel(
        'a_cmp_terrain_ao.png',
        'mask sheet B (AO), Commonwealth level %s, cells (-24,24)..(-17,31), '
        '--road-detail 1' % level,
        [ao_off, ao_on, diff],
        ['off  (mean %.1f)' % ao_off.mean(),
         'on, strength 0.5  (mean %.1f)' % ao_on.mean(),
         'the drop itself, x1  (mean over moved %.1f, max %d)'
         % (d[d > 0].mean(), d.max())])

    # the colour sheet lit by each AO, which is what the byte is FOR
    def lit(rgb, ao):
        f = (ao.astype(np.float32) / 255.0)[:, :, None]
        return np.clip(rgb.astype(np.float32) * f, 0, 255).astype(np.uint8)

    three_panel(
        'a_cmp_terrain_colour.png',
        'the colour sheet multiplied by that AO byte (what the channel is for)',
        [col_off, lit(col_off, ao_off), lit(col_off, ao_on)],
        ['colour sheet, unlit', 'x AO with the switch off',
         'x AO with the switch on'])


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], *sys.argv[3:])
