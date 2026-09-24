"""Lane UI5 -- the before/after pictures of the menu row.

Both halves are the APPLICATION's own grab (water_ui.sh's SHOT=), one per exe,
same spell, same framing, never a desktop capture (CONSTITUTION 5). This script
only crops and magnifies what the application already drew.

  BEFORE = scratchpad/ui4_20260910/images/strip_after.png -- lane UI4's grab of
           the 20:45:47 exe. It IS the shipped menu row: the grab starts at the
           window's own (0,0), so its first 35 rows are the row bungo
           photographed, and measure_before.py read the titles' ink out of them.
  AFTER  = scratchpad/ui5_20260910/images/toprow_after.png -- the same grab from
           this lane's exe.

The zoom crop is the File..Help region and nothing else: the titles span
x 14..241 in both, so 0..270 takes all five with a margin and stops well before
the first toolbar button. 4x NEAREST -- a pixel claim must stay a pixel.

Run: python scratchpad/ui5_20260910/make_pictures.py
"""
import os
from PIL import Image, ImageDraw

B = "scratchpad/ui4_20260910/images/strip_after.png"
D = "scratchpad/ui5_20260910/images/"
A = D + "toprow_after.png"
CROP = (0, 0, 270, 35)
SCALE = 4

os.makedirs(D, exist_ok=True)
before = Image.open(B).convert("RGB")
after = Image.open(A).convert("RGB")
print("before", B, before.size)
print("after ", A, after.size)

before.crop(CROP).resize(
    ((CROP[2] - CROP[0]) * SCALE, (CROP[3] - CROP[1]) * SCALE),
    Image.NEAREST).save(D + "menu_zoom_before.png")
after.crop(CROP).resize(
    ((CROP[2] - CROP[0]) * SCALE, (CROP[3] - CROP[1]) * SCALE),
    Image.NEAREST).save(D + "menu_zoom_after.png")

bz = Image.open(D + "menu_zoom_before.png")
az = Image.open(D + "menu_zoom_after.png")

LBL_B = os.environ.get("LBL_B", "BEFORE  20:45:47   titles at y 6..16, centre 11.0, row centre 17.0  -> 6 px HIGH")
LBL_A = os.environ.get("LBL_A", "AFTER   this build  titles centred, row still 35")

gap = 26
w = max(bz.width, az.width)
sheet = Image.new("RGB", (w, bz.height + az.height + gap * 2 + 6), (24, 25, 28))
d = ImageDraw.Draw(sheet)
d.text((8, 6), LBL_B, fill=(230, 230, 230))
sheet.paste(bz, (0, gap))
d.rectangle([0, gap + bz.height + 2, w, gap + bz.height + 4], fill=(220, 60, 60))
d.text((8, gap + bz.height + 8), LBL_A, fill=(230, 230, 230))
sheet.paste(az, (0, gap * 2 + bz.height + 6))
sheet.save(D + "cmp_menu_zoom.png")
print("cmp_menu_zoom.png ", sheet.size)

# and the two whole top strips stacked at 1:1, for the shape of the whole row
gap2 = 22
h = before.height + after.height + gap2 * 2 + 4
sheet2 = Image.new("RGB", (max(before.width, after.width), h), (24, 25, 28))
d2 = ImageDraw.Draw(sheet2)
d2.text((8, 5), LBL_B, fill=(230, 230, 230))
sheet2.paste(before, (0, gap2))
d2.rectangle([0, gap2 + before.height + 1, sheet2.width, gap2 + before.height + 3],
             fill=(220, 60, 60))
d2.text((8, gap2 + before.height + 7), LBL_A, fill=(230, 230, 230))
sheet2.paste(after, (0, gap2 * 2 + before.height + 4))
sheet2.save(D + "cmp_toprow.png")
print("cmp_toprow.png    ", sheet2.size)
