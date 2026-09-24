#!/usr/bin/env python3
"""IDENTPROX -- 640x360 contact sheet of the two candidate highway cameras,
so the full-size matrix is not spent on a framing that does not show the deck.
"""
import numpy as np
import sys

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
for q in (LANE, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919',
          'E:/Projects/NifskopeWildWastelandEdition/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import render as RD                                         # noqa: E402
import shade as SH                                          # noqa: E402
from scene import Terrain, Objects                          # noqa: E402
import hwycams                                              # noqa: E402


def main():
    ter = Terrain()
    ob = Objects(verbose=False)
    cands = {}
    g = lambda x, y: float(ter.atf(np.array([x]), np.array([y]))[0])
    cands.update(hwycams.build(ter, 640, 360))
    # two alternates for the under-deck view, east side instead of west
    e = (25200.0, -40200.0)
    t = (20357.0, -39400.0)
    cands['under_east'] = RD.Camera((e[0], e[1], g(*e) + 180.0), (t[0], t[1], 1900.0),
                                    55.0, 640, 360, name='under_east')
    e = (17000.0, -39300.0)
    t = (20357.0, -39000.0)
    cands['under_west'] = RD.Camera((e[0], e[1], g(*e) + 180.0), (t[0], t[1], 1750.0),
                                    55.0, 640, 360, name='under_west')
    e = (22900.0, -33600.0)
    t = (20357.0, -43000.0)
    cands['deck_north'] = RD.Camera((e[0], e[1], g(*e) + 4200.0), (t[0], t[1], 2200.0),
                                    52.0, 640, 360, name='deck_north')
    out = []
    for nm, cam in cands.items():
        gb = RD.GBuffer(cam, ter, ob, verbose=False)
        ss = RD.SunShadow(ter, ob, 120.0, 25.0)
        lit = np.ones(len(gb.kind), dtype=bool)
        m = gb.kind != 0
        lit[m] = ss.lit(gb.pos[m])
        img = SH.to8(SH.shade(gb, lit.astype(float), 25.0, 120.0), cam.w, cam.h)
        out.append((nm, img, int((gb.kind == 2).sum())))
        print('%-12s object px %s' % (nm, '{:,}'.format(int((gb.kind == 2).sum()))))
    from PIL import Image
    W, H = 640, 360
    cols = 3
    rows = (len(out) + cols - 1) // cols
    sheet = Image.new('RGB', (W * cols, H * rows), (16, 16, 18))
    for k, (nm, img, _) in enumerate(out):
        sheet.paste(Image.fromarray(img), ((k % cols) * W, (k // cols) * H))
    from PIL import ImageDraw
    d = ImageDraw.Draw(sheet)
    for k, (nm, img, npx) in enumerate(out):
        d.text(((k % cols) * W + 8, (k // cols) * H + 6), '%s  %s obj px' % (nm, '{:,}'.format(npx)),
               fill=(255, 240, 200))
    sheet.save(LANE + '/images/_hwy_cand_sheet.png')
    print('wrote images/_hwy_cand_sheet.png')


if __name__ == '__main__':
    main()
