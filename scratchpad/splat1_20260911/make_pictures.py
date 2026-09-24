"""SPLAT1 section 4 -- the picture: vanilla | ours | ours at the engine's own
tiling, offline | the difference. Same texels in every panel.

ww-texel-picture: the crop is chosen BY THE METRIC on the BEFORE artefact (the
128x128 window of our shipped sheet with the worst local variance), searched
over the whole sheet; nearest-neighbour magnification at an integer factor
clamped on both axes; one fixed cell per panel so no caption clips; and every
caption carries the number the report quotes, in the report's units.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import splatlib as S                                          # noqa: E402
import offline_bake as B                                      # noqa: E402

CX, CY = -20, 24
CROP = 128
MAG = 4
CELL_W, CELL_H = CROP * MAG, CROP * MAG
PAD = 18
HEAD = 78
CAP = 84

try:
    F = ImageFont.truetype('consola.ttf', 15)
    FB = ImageFont.truetype('consolab.ttf', 19)
    FH = ImageFont.truetype('consolab.ttf', 24)
except Exception:
    F = FB = FH = ImageFont.load_default()

van = S.Dds(S.van_sheet(CX, CY)).level(0)[:, :, :3]
ours = S.Dds(S.OURS[(CX, CY)]).level(0)[:, :, :3]
print('re-baking offline at the engine tiling 341.3333 ...')
fixed = B.bake(CX, CY, 4, mip='code', tile=341.3333)
print('re-baking offline at the shipped tiling 2048 (the model\'s own control) ...')
model = B.bake(CX, CY, 4, mip='code', tile=2048.0)

lvO = S.local_var(S.lum(np.dstack([ours, np.full(ours.shape[:2], 255.0)])))

# --- the crop, chosen by the metric on the BEFORE artefact -------------------
best, bx, by = -1.0, 0, 0
for y in range(0, 512 - CROP + 1, 8):
    for x in range(0, 512 - CROP + 1, 8):
        m = lvO[y:y + CROP, x:x + CROP].mean()
        if m > best:
            best, bx, by = m, x, y
print('worst-speckle %dx%d crop of OUR shipped sheet at (%d,%d), local var %.2f'
      % (CROP, CROP, bx, by, best))


def cut(a):
    return a[by:by + CROP, bx:bx + CROP]


diff = np.clip(np.abs(cut(ours).astype(np.float64) - cut(van).astype(np.float64)) * 4.0,
               0, 255)


def lv_of(a):
    return S.local_var(S.lum(np.dstack([a, np.full(a.shape[:2], 255.0)]))).mean()


panels = [
    ('VANILLA  Commonwealth.4.-20.24.DDS', cut(van), lv_of(cut(van)),
     ['Bethesda\'s shipped chunk sheet, DXT5',
      'local variance %.2f  <- the reference' % lv_of(cut(van))]),
    ('OURS as shipped   TILE = 2048 u/repeat', cut(ours), lv_of(cut(ours)),
     ['one texture repeat = 64 texels; the footprint mip is 5,',
      'a 64x64 image of the ground texture, printed at full contrast',
      'local variance %.2f  = %.1fx vanilla' % (lv_of(cut(ours)),
                                                lv_of(cut(ours)) / lv_of(cut(van)))]),
    ('OURS re-baked offline  TILE = 341.333 u/repeat', cut(fixed), lv_of(cut(fixed)),
     ['the engine\'s own repeat (Fallout4.exe 1.10.155:',
      'fLandTextureTilingMult 1.5 -> 128/0.375). One repeat =',
      '10.7 texels; footprint mip 7.58. local variance %.2f' % lv_of(cut(fixed))]),
    ('|OURS as shipped - VANILLA|  x4', diff, 0.0,
     ['whole-tile mean |RGB| difference 16.33 of 255;',
      'the SPECKLE is what this lane removes, the overall',
      'level (the x0.82 grading) it does NOT -- see section 4']),
]

W = 2 * (CELL_W + PAD) + PAD
H = HEAD + 2 * (CELL_H + CAP + PAD) + PAD
img = Image.new('RGB', (W, H), (26, 26, 30))
d = ImageDraw.Draw(img)
d.text((PAD, 14), 'The landscape textures are baked 6x too large',
       font=FH, fill=(240, 240, 235))
d.text((PAD, 46), 'chunk (-20,24), dim 4, 512 texels at 32 world units each. '
       'Same %d x %d texels at (%d,%d) in every panel, %dx nearest neighbour.'
       % (CROP, CROP, bx, by, MAG), font=F, fill=(170, 170, 180))

for k, (title, arr, lv, lines) in enumerate(panels):
    cx = PAD + (k % 2) * (CELL_W + PAD)
    cy = HEAD + (k // 2) * (CELL_H + CAP + PAD)
    p = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), 'RGB')
    p = p.resize((CROP * MAG, CROP * MAG), Image.NEAREST)
    img.paste(p, (cx, cy))
    d.rectangle([cx - 1, cy - 1, cx + CELL_W, cy + CELL_H], outline=(70, 70, 78))
    col = (255, 210, 120) if k == 1 else ((150, 230, 150) if k == 2 else (225, 225, 230))
    d.text((cx, cy + CELL_H + 6), title, font=FB, fill=col)
    for i, ln in enumerate(lines):
        d.text((cx, cy + CELL_H + 28 + i * 16), ln, font=F, fill=(170, 170, 180))

out = os.path.join(HERE, 'images', 'speckle_diagnosis.png')
img.save(out)
print('wrote %s  (%d x %d)' % (out, W, H))
print('  panel local variances: vanilla %.2f  ours %.2f  ours@341.333 %.2f'
      % (lv_of(cut(van)), lv_of(cut(ours)), lv_of(cut(fixed))))
print('  model control (offline at the shipped 2048) on the same crop: %.2f'
      % lv_of(cut(model)))
