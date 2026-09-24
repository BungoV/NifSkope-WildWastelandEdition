"""Lane ROADS2's three pictures. Every panel is the SAME 512-texel grid at the
same 32 world units a texel, so nothing is resampled on any side.

  cmp_seam.png       chunk (-20,20): vanilla | ours before this lane | ours at
                     the new defaults, whole sheet on top and a 4x zoom of the
                     worst 48x48 window (texel 53,378 -- the window the
                     before/after diagnostic picked, kept so the crop is not
                     chosen after the numbers) underneath, with the boundary
                     gradient and the local 5x5 SD burned into each panel.
  cmp_highway.png    chunk (-8,8): the same three, with the elevated road
                     family's own footprint outlined in the zoom and its
                     clearance above the displaced-mask floor burned in.
  cmp_sanctuary_road_v2.png
                     ROADS1's picture re-taken on the new exe at ROADS1's own
                     crop (150,120)-(300,270): vanilla | ours --no-roads | ours
                     --roads.

usage: make_pics.py <outdir>
"""

import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'flagscan1_20260911'))
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))

from lodgen_terrain_model import Dds                          # noqa: E402
import seam                                                   # noqa: E402

VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
SEAM_ZOOM = (53, 378, 53 + 48, 378 + 48)
SANC_ZOOM = (150, 120, 300, 270)
HW_ZOOM = (200, 180, 200 + 96, 180 + 96)


def font(sz):
    for p in (r'C:\Windows\Fonts\consola.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


def rgb(path):
    t = Dds(path)
    px, w, h = t._level(0)
    a = np.array(px, dtype=np.float32).reshape(h, w, 4)[:, :, :3]
    return np.clip(a * 255.0, 0, 255).astype(np.uint8)


def lum(a):
    a = a.astype(np.float32)
    return a[:, :, 0] * .2126 + a[:, :, 1] * .7152 + a[:, :, 2] * .0722


def locsd(L, m):
    n = L.shape[0]
    s = np.array([[L[max(0, j - 2):j + 3, max(0, i - 2):i + 3].std()
                   for i in range(n)] for j in range(n)])
    return s[m].mean()


def panel(a, zoom, zf, label, lines, box=None):
    """One column: the whole sheet, the zoom under it, a caption under that."""
    n = a.shape[0]
    zx0, zy0, zx1, zy1 = zoom
    z = a[zy0:zy1, zx0:zx1]
    zi = Image.fromarray(z).resize(((zx1 - zx0) * zf, (zy1 - zy0) * zf),
                                   Image.NEAREST)
    top = Image.fromarray(a)
    d = ImageDraw.Draw(top)
    d.rectangle([zx0, zy0, zx1 - 1, zy1 - 1], outline=(255, 64, 64), width=2)
    capH = 18 * (len(lines) + 1) + 10
    W = max(n, zi.width)
    out = Image.new('RGB', (W, n + zi.height + capH + 12), (16, 16, 16))
    out.paste(top, ((W - n) // 2, 0))
    out.paste(zi, ((W - zi.width) // 2, n + 6))
    if box is not None:
        # Only the OUTLINE of the family footprint, so the picture still shows
        # the terrain it is pointing at. A texel is on the outline when it is
        # in the mask and at least one 4-neighbour is not.
        d2 = ImageDraw.Draw(out)
        bm = box[zy0:zy1, zx0:zx1]
        pad = np.zeros((bm.shape[0] + 2, bm.shape[1] + 2), dtype=bool)
        pad[1:-1, 1:-1] = bm
        inner = (pad[:-2, 1:-1] & pad[2:, 1:-1]
                 & pad[1:-1, :-2] & pad[1:-1, 2:])
        edge = bm & ~inner
        ys, xs = np.nonzero(edge)
        ox, oy = (W - zi.width) // 2, n + 6
        for y, x in zip(ys, xs):
            d2.rectangle([ox + x * zf, oy + y * zf,
                          ox + x * zf + zf - 1, oy + y * zf + zf - 1],
                         fill=(80, 200, 255))
    d3 = ImageDraw.Draw(out)
    y = n + zi.height + 10
    d3.text((6, y), label, font=font(15), fill=(255, 255, 255))
    for i, t in enumerate(lines):
        d3.text((6, y + 18 * (i + 1)), t, font=font(13), fill=(190, 190, 190))
    return out


def join(cols, title, out):
    W = sum(c.width for c in cols) + 8 * (len(cols) + 1)
    H = max(c.height for c in cols) + 34
    im = Image.new('RGB', (W, H), (16, 16, 16))
    ImageDraw.Draw(im).text((8, 8), title, font=font(16), fill=(255, 255, 255))
    x = 8
    for c in cols:
        im.paste(c, (x, 28))
        x += c.width + 8
    im.save(out)
    print('wrote %s  %dx%d' % (out, im.width, im.height))


def cmp_seam(outdir):
    pz = np.load(os.path.join(HERE, 'proj_m20_20.npz'))
    diff, feath, solid = seam.boundaries(pz['idbuf'], pz['ramped'])
    road = pz['idbuf'] >= 0
    srcs = [('vanilla (Bethesda)', os.path.join(VAN, 'Commonwealth.4.-20.20.DDS')),
            ('ours BEFORE (--roads-legacy)', 'out/rung_on/tex/Commonwealth.4.-20.20.DDS'),
            ('ours AFTER (the new defaults)', 'out/v2_def/tex/Commonwealth.4.-20.20.DDS')]
    cols = []
    for label, p in srcs:
        a = rgb(os.path.join(HERE, p) if not os.path.isabs(p) and not p.startswith('E:') else p)
        L = lum(a)
        g = seam.grad_mag(L)
        cols.append(panel(a, SEAM_ZOOM, 6, label, [
            'piece-boundary gradient  %.3f' % g[diff].mean(),
            'local 5x5 SD on the road %.3f' % locsd(L, road),
            'mean road luminance      %.2f' % L[road].mean()]))
    join(cols, 'chunk (-20,20), 512 texels, 32 world units a texel -- '
               'the seam bungo pointed at, zoom 6x on texels (53,378)-(101,426)',
         os.path.join(outdir, 'cmp_seam.png'))


def cmp_highway(outdir):
    mz = np.load(os.path.join(HERE, 'gate4_masks.npz'))
    elev = mz['road_elevated']
    srcs = [('vanilla (Bethesda)', os.path.join(VAN, 'Commonwealth.4.-8.8.DDS')),
            ('ours BEFORE (raised painted)', os.path.join(HERE, 'out/hw2_legacy/tex/Commonwealth.4.-8.8.DDS')),
            ('ours AFTER (raised refused)', os.path.join(HERE, 'out/hw2_def/tex/Commonwealth.4.-8.8.DDS'))]
    clr = {'vanilla (Bethesda)': -0.009,
           'ours BEFORE (raised painted)': +0.314,
           'ours AFTER (raised refused)': +0.001}
    cols = []
    for label, p in srcs:
        a = rgb(p)
        L = lum(a)
        c = clr.get(label)
        cols.append(panel(a, HW_ZOOM, 4, label, [
            'elevated-family mean luminance %.2f' % L[elev].mean(),
            ('clearance above its own floor %+.3f' % c) if c is not None
            else 'clearance above its own floor  see report 4b',
        ], box=elev))
    join(cols, 'chunk (-8,8) downtown -- the elevated road family (highway '
               'decks and bridges) outlined in the zoom',
         os.path.join(outdir, 'cmp_highway.png'))


def cmp_sanc(outdir):
    srcs = [('vanilla (Bethesda)', os.path.join(VAN, 'Commonwealth.4.-20.20.DDS')),
            ('ours --no-roads (the floor)', os.path.join(HERE, 'out/v2_off/tex/Commonwealth.4.-20.20.DDS')),
            ('ours --roads (the new defaults)', os.path.join(HERE, 'out/v2_def/tex/Commonwealth.4.-20.20.DDS'))]
    cols = []
    for label, p in srcs:
        a = rgb(p)
        L = lum(a)
        cols.append(panel(a, SANC_ZOOM, 3, label, [
            'mean luminance, whole sheet %.2f' % L.mean()]))
    join(cols, 'Sanctuary chunk (-20,20), ROADS1 picture re-taken at ROADS1 crop '
               '(150,120)-(300,270)',
         os.path.join(outdir, 'cmp_sanctuary_road_v2.png'))


if __name__ == '__main__':
    od = sys.argv[1]
    os.makedirs(od, exist_ok=True)
    cmp_seam(od)
    cmp_highway(od)
    cmp_sanc(od)
