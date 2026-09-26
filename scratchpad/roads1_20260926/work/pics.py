"""ROADS1 pictures: the installed VT.2 colour sheet | a new bake's, top down, same texels, with numbers.
  python pics.py <new bake tag>
Writes pics/downtown_installed_vs_new.png and pics/riverside_installed_vs_new.png (+ _rot180 = his camera's side)."""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'tests', 'spells'))  # lodgen_vt_check, which vtread imports
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/seam1_20260925')
import vtread  # noqa: E402
import faith_cmp as fc  # noqa: E402

INST = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
PICS = os.path.join(os.path.dirname(HERE), 'pics')
LW = np.array([0.2126, 0.7152, 0.0722])
# crops in the fc box's texel grid (cells -8..3 x -12..-1, 256 texels a cell, row 0 = north edge of cell -1)
CROPS = {
    'downtown': (8 * 256, int(2.5625 * 256), 512, 512, 2),        # cells x 0..2, y -5.56..-3.56 (ROADS0's close crop)
    'riverside': (2 * 256, 1 * 256, 7 * 256, 3 * 256, 1),        # cells x -6..1, y -5..-2
}


def installed():
    v = vtread.Vt(INST)
    m, wW, nN = v.mosaic(fc.CX0, fc.CY0, fc.CX1, fc.CY1, 1)
    per = v.content // v.levelDim
    c0 = (fc.CX0 - wW) * per
    r0 = (nN - (fc.CY1 + 1)) * per
    return m[r0:r0 + (fc.CY1 - fc.CY0 + 1) * per, c0:c0 + (fc.CX1 - fc.CX0 + 1) * per, :3].astype(float)


def main():
    tag = sys.argv[1]
    a = installed()
    b = fc.sheet(tag)
    ing = np.load(os.path.join(HERE, 'out', 'raster_INGAME_NEWRULE.npz'))
    win, meta = ing['win'], ing['meta']
    ok = win >= 0
    pav = np.zeros(win.shape, bool)
    pav[ok] = meta[win[ok], 0] == 1
    road = ok & ~pav
    for name, (x, y, w, h, z) in CROPS.items():
        A, B = a[y:y + h, x:x + w], b[y:y + h, x:x + w]
        P, Rd = pav[y:y + h, x:x + w], road[y:y + h, x:x + w]
        la, lb = A @ LW, B @ LW
        ch = (np.abs(A - B).max(2) > 2)
        txt = ('%s: pavement texels %d, luma installed %.1f -> new %.1f | road texels %d, installed %.1f -> new %.1f | '
               'texels changed >2 levels %.1f %%' % (name, P.sum(), la[P].mean(), lb[P].mean(), Rd.sum(), la[Rd].mean(),
                                                    lb[Rd].mean(), 100.0 * ch.mean()))
        print(txt)
        ia = Image.fromarray(A.clip(0, 255).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST)
        ib = Image.fromarray(B.clip(0, 255).astype(np.uint8)).resize((w * z, h * z), Image.NEAREST)
        for suffix, rot in (('', 0), ('_rot180', 180)):
            if suffix and name != 'riverside':
                continue
            pa, pb = ia.rotate(rot), ib.rotate(rot)
            out = Image.new('RGB', (w * z * 2 + 16, h * z + 40), (255, 255, 255))
            out.paste(pa, (0, 40))
            out.paste(pb, (w * z + 16, 40))
            d = ImageDraw.Draw(out)
            d.text((6, 4), 'INSTALLED', fill=(0, 0, 0))
            d.text((w * z + 22, 4), 'NEW (%s)' % tag, fill=(0, 0, 0))
            d.text((6, 20), txt[len(name) + 2:], fill=(0, 0, 0))
            out.save(os.path.join(PICS, '%s_installed_vs_new%s.png' % (name, suffix)))


if __name__ == '__main__':
    main()
