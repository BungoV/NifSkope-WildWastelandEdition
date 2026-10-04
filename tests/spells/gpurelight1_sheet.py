#!/usr/bin/env python3
"""Lane GPURELIGHT1: the before|after sheet of `NifSkope -no-gui gpurelight --out <dir>` (numpy + PIL; Windows-safe).

Room B seen from room A through the wall's three doorways (the surfels of room B, x > 528, splatted onto the y-z plane
from the side, nearest wall first), and room B's floor seen from above. Columns: doors open (baked) | doors closed
(this lane: real door triangles, the alpha-test hole, the glass pane's tint) | doors closed as a blanket cut (the
rejected way: every link through a closed door's box cut). Row 1 direct light (B1), row 2 with the bounce (B; the blanket
column has no B dump and repeats B1). Grey background so black reads as black.

usage: python gpurelight1_sheet.py <out dir> <sheet.png>
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

BG = (96, 96, 104)


def load(d, n, cols):
    return np.fromfile(os.path.join(d, n), dtype=np.float32).reshape(-1, cols).astype(np.float64)


def panel(P, N, B, sel, ax, ranges, scale, expo, size):
    W = int((ranges[0][1] - ranges[0][0]) * scale)
    H = int((ranges[1][1] - ranges[1][0]) * scale)
    img = Image.new('RGB', (W, H), BG)
    dr = ImageDraw.Draw(img)
    idx = np.nonzero(sel)[0]
    # far first: the splat nearest the viewer wins
    depth = P[idx, 3 - ax[0] - ax[1]]
    idx = idx[np.argsort(-depth)] if ax == (1, 2) else idx[np.argsort(P[idx, 2])]
    for i in idx:
        c = B[i] * expo
        c = c / (1.0 + c)
        c = np.clip(c, 0, 1) ** (1 / 2.2)
        x = (P[i, ax[0]] - ranges[0][0]) * scale
        y = H - (P[i, ax[1]] - ranges[1][0]) * scale
        r = size * scale / 2
        dr.rectangle([x - r, y - r, x + r, y + r], fill=tuple(int(255 * v) for v in c))
    return img


def main():
    root, outp = sys.argv[1], sys.argv[2]
    tw = os.path.join(root, 'twin')
    surf = load(tw, 'surf.f32', 9)
    P, N = surf[:, 0:3], surf[:, 3:6]
    cols = [('doors open (baked)', 'b1_cpu_baked.f32', 'b_cpu_baked.f32'),
            ('doors closed: real geometry + mask + glass', 'b1_cpu_doorsclosed.f32', 'b_cpu_doorsclosed.f32'),
            ('doors closed as a blanket cut (rejected)', 'b1_cpu_doorsclosed_doorblanket.f32',
             'b1_cpu_doorsclosed_doorblanket.f32')]
    roomB = P[:, 0] > 528
    floor = roomB & (N[:, 2] > 0.7) & (P[:, 2] < 40)
    tiles = []
    for row in (1, 2):
        mx = max(load(tw, c[row], 3)[roomB].max() for c in cols)
        expo = 4.0 / max(mx, 1e-9)
        for c in cols:
            B = load(tw, c[row], 3)
            b = panel(P, N, B, floor, (0, 1), ((528, 1056), (0, 784)), 0.8, expo, 44)
            t = Image.new('RGB', (b.width + 96, b.height + 30), BG)
            d = ImageDraw.Draw(t)
            lab = ('direct (B1): ' if row == 1 else 'with bounce (B): ') + c[0]
            if row == 2 and c[2] == c[1]:
                lab = 'direct only (no B dump): ' + c[0]
            d.text((8, 4), lab, fill=(255, 255, 255))
            t.paste(b, (88, 24))
            for y, lab in ((128, 'solid door'), (384, 'hole door'), (640, 'glass door')):
                yy = 24 + b.height - int(y * 0.8)
                d.line([(74, yy), (86, yy)], fill=(255, 220, 0), width=3)
                d.text((6, yy - 6), lab + ' >', fill=(255, 220, 0))
            tiles.append(t)
    w, h = tiles[0].width, tiles[0].height
    sheet = Image.new('RGB', (3 * w, 2 * h + 30), BG)
    ImageDraw.Draw(sheet).text((8, 8), 'GPURELIGHT1: room B floor from above, lit from room A through three doorways (left edge): '
                               'a solid door, a door with an alpha-tested hole, a door with a glass pane (tint 60/200/110)',
                               fill=(255, 255, 255))
    for k, t in enumerate(tiles):
        sheet.paste(t, ((k % 3) * w, 30 + (k // 3) * h))
    sheet.save(outp)
    print('sheet %s %dx%d' % (outp, sheet.width, sheet.height))


if __name__ == '__main__':
    main()
