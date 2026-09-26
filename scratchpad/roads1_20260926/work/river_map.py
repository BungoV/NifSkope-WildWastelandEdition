"""ROADS1: a top-down map of the Charles River bank west of Diamond City, to find his three circled strips.
The installed VT.2 colour sheet over cells X0..X1, Y0..Y1, with the road/pavement footprint of the independent
raster tinted (red = road, yellow = pavement), bridge pieces in cyan crosses, cell grid in white."""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread  # noqa: E402
import roadgeo as rg  # noqa: E402

INST = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
X0, Y0, X1, Y1 = -9, -7, -1, -2
PICS = os.path.join(os.path.dirname(HERE), 'pics')


def main():
    os.makedirs(PICS, exist_ok=True)
    v = vtread.Vt(INST)
    m, wW, nN = v.mosaic(X0, Y0, X1, Y1, 1)
    per = v.content // v.levelDim
    c0 = (X0 - wW) * per
    r0 = (nN - (Y1 + 1)) * per
    img = m[r0:r0 + (Y1 - Y0 + 1) * per, c0:c0 + (X1 - X0 + 1) * per, :3].copy()
    np.save(os.path.join(HERE, 'out', 'river_installed.npy'), img)
    ing = np.load(os.path.join(HERE, 'out', 'raster_INGAME.npz'))
    foot, win, meta = ing['foot'], ing['win'], ing['meta']
    # raster grid: cells -8..3 x -12..-1, row 0 north at y=0
    RX0, RYT = -8, -1 + 1
    ov = img.astype(float)
    for j in range(ov.shape[0]):
        pass
    oy = (RYT - (Y1 + 1)) * 256
    ox = (X0 - RX0) * 256
    H, W = img.shape[:2]
    ys0, xs0 = max(0, -oy), max(0, -ox)
    sub = np.zeros((H, W), np.int8)
    fy0, fx0 = oy + ys0, ox + xs0
    fh = min(H - ys0, foot.shape[0] - fy0)
    fw = min(W - xs0, foot.shape[1] - fx0)
    f = foot[fy0:fy0 + fh, fx0:fx0 + fw]
    w = win[fy0:fy0 + fh, fx0:fx0 + fw]
    swm = np.zeros_like(f)
    swm[w >= 0] = meta[w[w >= 0], 0] == 1
    sub[ys0:ys0 + fh, xs0:xs0 + fw] = np.where(swm, 2, np.where(f, 1, 0))
    ov[sub == 1] = ov[sub == 1] * 0.5 + np.array([255, 0, 0]) * 0.5
    ov[sub == 2] = ov[sub == 2] * 0.5 + np.array([255, 220, 0]) * 0.5
    out = Image.fromarray(ov.clip(0, 255).astype(np.uint8))
    d = ImageDraw.Draw(out)
    for cx in range(X0, X1 + 2):
        d.line([((cx - X0) * 256, 0), ((cx - X0) * 256, H)], fill=(255, 255, 255))
    for cy in range(Y0, Y1 + 2):
        d.line([(0, (Y1 + 1 - cy) * 256), (W, (Y1 + 1 - cy) * 256)], fill=(255, 255, 255))
    for cx in range(X0, X1 + 1):
        for cy in range(Y0, Y1 + 1):
            d.text(((cx - X0) * 256 + 4, (Y1 + 1 - cy) * 256 + 4), '%d,%d' % (cx, cy), fill=(255, 255, 255))
    R = rg.Reader()
    for form, r, cx, cy in R.refs_in(X0, Y0, X1, Y1):
        bi = R.base_info(r['base'])
        if not bi:
            continue
        mo = bi['modl'].lower()
        px = (r['pos'][0] - X0 * 4096) / 16
        py = ((Y1 + 1) * 4096 - r['pos'][1]) / 16
        if 'bridge' in mo:
            d.line([(px - 6, py), (px + 6, py)], fill=(0, 255, 255), width=2)
            d.line([(px, py - 6), (px, py + 6)], fill=(0, 255, 255), width=2)
        elif mo.startswith('landscape' + rg.BS + 'roads' + rg.BS + 'river'):
            d.ellipse([px - 5, py - 5, px + 5, py + 5], outline=(255, 0, 255), width=2)
    out.save(os.path.join(PICS, 'river_map_installed_with_footprints.png'))
    Image.fromarray(img).save(os.path.join(PICS, 'river_map_installed_plain.png'))
    print('saved', out.size)


if __name__ == '__main__':
    main()
