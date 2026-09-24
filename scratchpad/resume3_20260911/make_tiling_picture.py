"""RESUME3 gate R4's picture: the tiling fix, on the REAL bake.

vanilla | ours at 2048 | ours at 341.3333 | |ours@2048 - vanilla| x4, the SAME
texels in every panel, chosen BY THE METRIC on the BEFORE artefact, numbers
burned in (`ww-texel-picture`).

SPLAT1's `speckle_diagnosis.png` showed the fixed panel re-baked OFFLINE. This
one is four real DDS files off disk: nothing here is modelled.

    python make_tiling_picture.py
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'scratchpad', 'splat1_20260911'))
import splatlib as S                                          # noqa: E402

CX, CY = -20, 24
TILE = 't2024'
CROP, MAG = 128, 4
CELL_W, CELL_H = CROP * MAG, CROP * MAG
PAD, HEAD, CAP = 18, 84, 92

try:
    F = ImageFont.truetype('consola.ttf', 15)
    FB = ImageFont.truetype('consolab.ttf', 19)
    FH = ImageFont.truetype('consolab.ttf', 24)
except Exception:
    F = FB = FH = ImageFont.load_default()


def sheet(variant):
    return os.path.join(HERE, 'tiling', variant, TILE, 'tex',
                        'Commonwealth.4.%d.%d.DDS' % (CX, CY))


van = S.Dds(S.van_sheet(CX, CY)).level(0)[:, :, :3].astype(np.float64)
old = S.Dds(sheet('back')).level(0)[:, :, :3].astype(np.float64)   # --land-tiling 2048
new = S.Dds(sheet('new')).level(0)[:, :, :3].astype(np.float64)    # default 341.3333


def lv_of(a):
    return float(S.local_var(S.lum(np.dstack([a, np.full(a.shape[:2], 255.0)]))).mean())


lvOld = S.local_var(S.lum(np.dstack([old, np.full(old.shape[:2], 255.0)])))
best, bx, by = -1.0, 0, 0
for y in range(0, 512 - CROP + 1, 8):
    for x in range(0, 512 - CROP + 1, 8):
        m = lvOld[y:y + CROP, x:x + CROP].mean()
        if m > best:
            best, bx, by = m, x, y
print('worst-speckle %dx%d crop of the 2048 bake at (%d,%d), local var %.2f'
      % (CROP, CROP, bx, by, best))


def cut(a):
    return a[by:by + CROP, bx:bx + CROP]


diff = np.clip(np.abs(cut(old) - cut(van)) * 4.0, 0, 255)
lvV, lvO, lvN = lv_of(cut(van)), lv_of(cut(old)), lv_of(cut(new))
errO = float(np.abs(old - van).mean())
errN = float(np.abs(new - van).mean())
wholeV, wholeO, wholeN = lv_of(van), lv_of(old), lv_of(new)

panels = [
    ('VANILLA  Commonwealth.4.%d.%d.DDS' % (CX, CY), cut(van),
     ["Bethesda's shipped chunk sheet",
      'local variance on this crop %.2f  <- the reference' % lvV,
      'whole tile %.2f' % wholeV]),
    ('OURS  --land-tiling 2048   (the way back)', cut(old),
     ['one texture repeat = 64 texels, footprint mip 5.00:',
      'a 64x64 image of the ground texture at full contrast',
      'local variance %.2f = %.1fx vanilla   whole tile %.2f'
      % (lvO, lvO / lvV, wholeO)]),
    ('OURS  default 341.3333   (the engine\'s own repeat)', cut(new),
     ['Fallout4.exe 1.10.155: fLandTextureTilingMult 1.5,',
      '128/0.375. One repeat = 10.7 texels, footprint mip 7.58',
      'local variance %.2f = %.2fx vanilla  whole tile %.2f'
      % (lvN, lvN / lvV, wholeN)]),
    ('|OURS at 2048 - VANILLA|  x4', diff,
     ['whole-tile mean |RGB| vs vanilla: 2048 %.2f -> 341.3333 %.2f' % (errO, errN),
      'the SPECKLE goes; the overall LEVEL does not. The x0.82-0.83',
      'grading is still open ("splat calibration vs vanilla grading")']),
]

W = 2 * (CELL_W + PAD) + PAD
H = HEAD + 2 * (CELL_H + CAP + PAD) + PAD
img = Image.new('RGB', (W, H), (26, 26, 30))
d = ImageDraw.Draw(img)
d.text((PAD, 12), 'The landscape textures were baked 6x too large - fixed, real bake',
       font=FH, fill=(240, 240, 235))
d.text((PAD, 44), 'chunk (%d,%d), dim 4, 512 texels at 32 world units each. Same %d x %d '
       'texels at (%d,%d) in every panel, %dx nearest neighbour.'
       % (CX, CY, CROP, CROP, bx, by, MAG), font=F, fill=(170, 170, 180))
d.text((PAD, 62), 'All four panels are DDS files off disk - nothing here is modelled.',
       font=F, fill=(170, 170, 180))

for k, (title, arr, lines) in enumerate(panels):
    cx = PAD + (k % 2) * (CELL_W + PAD)
    cy = HEAD + (k // 2) * (CELL_H + CAP + PAD)
    p = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), 'RGB')
    p = p.resize((CROP * MAG, CROP * MAG), Image.NEAREST)
    img.paste(p, (cx, cy))
    d.rectangle([cx - 1, cy - 1, cx + CELL_W, cy + CELL_H], outline=(70, 70, 78))
    col = (255, 210, 120) if k == 1 else ((150, 230, 150) if k == 2 else (225, 225, 230))
    d.text((cx, cy + CELL_H + 6), title, font=FB, fill=col)
    for i, ln in enumerate(lines):
        d.text((cx, cy + CELL_H + 30 + i * 17), ln, font=F, fill=(170, 170, 180))

os.makedirs(os.path.join(HERE, 'images'), exist_ok=True)
out = os.path.join(HERE, 'images', 'cmp_tiling_fixed.png')
img.save(out)
print('wrote %s  (%d x %d)' % (out, W, H))
print('  crop local variance: vanilla %.2f  2048 %.2f  341.3333 %.2f' % (lvV, lvO, lvN))
print('  whole tile local variance: vanilla %.2f  2048 %.2f  341.3333 %.2f'
      % (wholeV, wholeO, wholeN))
print('  whole tile mean |RGB| vs vanilla: 2048 %.2f  341.3333 %.2f' % (errO, errN))
