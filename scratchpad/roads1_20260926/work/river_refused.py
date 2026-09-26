"""ROADS1: the river bank east of Diamond City with every road / pavement placement's footprint drawn by decision.
red = stamped road, yellow = stamped pavement, magenta = refused because the base carries LOD, cyan = refused by folder
(overpass / bridge / raised). Background = the installed VT.2 colour sheet (read only).
  python river_refused.py [X0 Y0 X1 Y1]"""
import os
import pickle
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread  # noqa: E402
import roadgeo as rg  # noqa: E402

INST = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
PICS = os.path.join(os.path.dirname(HERE), 'pics')
COL = {'stamped': None, 'refused raised-haslod': (255, 0, 255), 'refused raised-folder': (0, 255, 255)}


def main():
    X0, Y0, X1, Y1 = [int(a) for a in sys.argv[1:5]] if len(sys.argv) > 4 else (-6, -6, 1, -2)
    v = vtread.Vt(INST)
    m, wW, nN = v.mosaic(X0, Y0, X1, Y1, 1)
    per = v.content // v.levelDim
    c0 = (X0 - wW) * per
    r0 = (nN - (Y1 + 1)) * per
    img = m[r0:r0 + (Y1 - Y0 + 1) * per, c0:c0 + (X1 - X0 + 1) * per, :3].copy()
    base = Image.fromarray(img)
    lay = Image.new('RGBA', base.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    R = rg.Reader()
    pl = pickle.load(open(os.path.join(HERE, 'out', 'road_placements.pkl'), 'rb'))
    rows = []
    for p in pl:
        x, y = p['pos'][0], p['pos'][1]
        if not (X0 * 4096 - 2048 <= x < (X1 + 1) * 4096 + 2048 and Y0 * 4096 - 2048 <= y < (Y1 + 1) * 4096 + 2048):
            continue
        model = R.model(p['modl'])
        if not model:
            continue
        if p['decision'] == 'stamped':
            c = (255, 220, 0) if rg.is_sidewalk(p['modl']) else (255, 0, 0)
        else:
            c = COL[p['decision']]
        rot = np.asarray(p['rot'])
        for s in model:
            wp = np.asarray(p['pos']) + (s['pos'] * p['scale']) @ rot.T
            px = (wp[:, 0] - X0 * 4096) / 16
            py = ((Y1 + 1) * 4096 - wp[:, 1]) / 16
            for t in s['tris']:
                d.polygon([(px[i], py[i]) for i in t], fill=c + (110,))
        if p['decision'] != 'stamped':
            rows.append(p)
    out = Image.alpha_composite(base.convert('RGBA'), lay).convert('RGB')
    dd = ImageDraw.Draw(out)
    H, W = img.shape[:2]
    for cx in range(X0, X1 + 1):
        dd.line([((cx - X0) * 256, 0), ((cx - X0) * 256, H)], fill=(255, 255, 255))
        for cy in range(Y0, Y1 + 1):
            dd.text(((cx - X0) * 256 + 4, (Y1 - cy) * 256 + 4), '%d,%d' % (cx, cy), fill=(255, 255, 255))
    for cy in range(Y0, Y1 + 1):
        dd.line([(0, (Y1 - cy) * 256), (W, (Y1 - cy) * 256)], fill=(255, 255, 255))
    tag = '%d_%d_%d_%d' % (X0, Y0, X1, Y1)
    out.save(os.path.join(PICS, 'river_decisions_%s.png' % tag))
    out.rotate(180).resize((W // 2, H // 2)).save(os.path.join(PICS, '_river_decisions_rot180_half.png'))
    for p in sorted(rows, key=lambda p: (p['decision'], p['modl'])):
        print('%08X %-9s %-44s base %08X %s' % (p['ref'], p['decision'][8:], p['modl'][:44], p['base'], p['plugin']))


if __name__ == '__main__':
    main()
