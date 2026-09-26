"""FLAT1 pictures: per kind, a top-down close-up of the VT.2 colour sheet -- installed | ROADS1 new_default | FLAT1 --
same texels, the crop centred where the independent raster has the most texels of that kind.

  python flat_pics.py <flat1 bake tag>     -> pics/<kind>_installed_roads1_flat1.png
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flat_faith as ff  # noqa: E402

INST = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
ROADS1 = r'C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/roads1b/bake/'
PICS = os.path.join(os.path.dirname(HERE), 'pics')
N = 160   # crop, texels (16 world units each)
Z = 3


def installed():
    import vtread
    v = vtread.Vt(INST)
    m, wW, nN = v.mosaic(ff.CX0, ff.CY0, ff.CX1, ff.CY1, 1)
    per = v.content // v.levelDim
    c0 = (ff.CX0 - wW) * per
    r0 = (nN - (ff.CY1 + 1)) * per
    return m[r0:r0 + (ff.CY1 - ff.CY0 + 1) * per, c0:c0 + (ff.CX1 - ff.CX0 + 1) * per, :3].astype(float)


def main():
    tag = sys.argv[1]
    b = ff.sheet(tag)
    a = installed()
    old = ff.BAKES
    ff.BAKES = ROADS1
    r = ff.sheet('new_default')
    ff.BAKES = old
    D = np.load(os.path.join(HERE, 'out', 'flat_raster.npz'))
    win, meta = D['win'], D['meta']
    kind = np.full(win.shape, -1)
    kind[win >= 0] = meta[win[win >= 0], 0]
    os.makedirs(PICS, exist_ok=True)
    for i, k in enumerate(ff.KINDS):
        m = (kind == i).astype(np.float64)
        # the N x N window with the most texels of the kind (summed-area table, stride 16)
        S = np.pad(m.cumsum(0).cumsum(1), ((1, 0), (1, 0)))
        best, bx, by = -1, 0, 0
        for y in range(0, m.shape[0] - N, 16):
            for x in range(0, m.shape[1] - N, 16):
                s = S[y + N, x + N] - S[y, x + N] - S[y + N, x] + S[y, x]
                if s > best:
                    best, bx, by = s, x, y
        crop = (slice(by, by + N), slice(bx, bx + N))
        ch = np.abs(r[crop] - b[crop]).max(2) > 2
        cx = ff.CX0 + (bx + N / 2) / 256.0
        cy = ff.CY1 + 1 - (by + N / 2) / 256.0
        txt = ('%s: %d texels of it in view; texels changed > 2 levels roads1 -> flat1: %.1f %%; centre cell %.2f, %.2f' % (
            k, int(best), 100.0 * ch.mean(), cx, cy))
        print(txt)
        ims = [Image.fromarray(x[crop].clip(0, 255).astype(np.uint8)).resize((N * Z, N * Z), Image.NEAREST)
               for x in (a, r, b)]
        out = Image.new('RGB', (N * Z * 3 + 32, N * Z + 40), (255, 255, 255))
        d = ImageDraw.Draw(out)
        for j, (im, t) in enumerate(zip(ims, ('INSTALLED', 'ROADS1 new_default', 'FLAT1 %s' % tag))):
            out.paste(im, (j * (N * Z + 16), 40))
            d.text((j * (N * Z + 16) + 6, 4), t, fill=(0, 0, 0))
        d.text((6, 20), txt, fill=(0, 0, 0))
        out.save(os.path.join(PICS, '%s_installed_roads1_flat1.png' % k))


if __name__ == '__main__':
    main()
