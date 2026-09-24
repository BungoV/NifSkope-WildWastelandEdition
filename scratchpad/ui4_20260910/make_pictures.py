"""Lane UI4 -- the before/after pictures.

Both halves come from the APPLICATION's own grab (water_ui.sh's SHOT=), one per
exe, same spell, same crop -- never a desktop capture (CONSTITUTION 5). This
script only crops and magnifies what the application already drew, and it uses
the SAME crop rectangle the harness computes for its own 4x zoom, so the before
zoom and the after zoom are the same window pixels.

  crop x 0 .. rTabs.x() + rTabs.width() + 190 = 573
  crop y rTabs.top() - 10 = 25 .. + 35 + 20 = 80
  4x, NEAREST -- a texel claim must stay a texel.
"""
from PIL import Image, ImageDraw

D = "scratchpad/ui4_20260910/images/"
CROP = (0, 25, 573, 80)          # left, top, right, bottom -- the harness's own
SCALE = 4

before = Image.open(D + "strip_before.png").convert("RGB")
after = Image.open(D + "strip_after.png").convert("RGB")

bz = before.crop(CROP).resize(
    ((CROP[2] - CROP[0]) * SCALE, (CROP[3] - CROP[1]) * SCALE), Image.NEAREST)
bz.save(D + "strip_zoom_before.png")
print("strip_zoom_before.png", bz.size)

az = Image.open(D + "strip_zoom_after.png").convert("RGB")
print("strip_zoom_after.png ", az.size)

# the two zooms stacked, a red rule between them, labels burned in
gap = 26
w = max(bz.width, az.width)
sheet = Image.new("RGB", (w, bz.height + az.height + gap * 2 + 6), (24, 25, 28))
d = ImageDraw.Draw(sheet)
d.text((8, 6), "BEFORE  release/NifSkope.exe 18:25:20   top 0  bottom 0  left 0  "
               "right 3  between 0", fill=(230, 230, 230))
sheet.paste(bz, (0, gap))
d.rectangle([0, gap + bz.height + 2, w, gap + bz.height + 4], fill=(220, 60, 60))
d.text((8, gap + bz.height + 8), "AFTER   release/NifSkope.exe 20:45:47   top 4  "
                                 "bottom 4  left 4  right 4  between 4",
       fill=(230, 230, 230))
sheet.paste(az, (0, gap * 2 + bz.height + 6))
sheet.save(D + "cmp_strip_zoom.png")
print("cmp_strip_zoom.png    ", sheet.size)

# and the two whole top strips stacked at 1:1, for the shape of the row
gap2 = 22
sheet2 = Image.new("RGB", (before.width, before.height * 2 + gap2 * 2 + 4), (24, 25, 28))
d2 = ImageDraw.Draw(sheet2)
d2.text((8, 5), "BEFORE  18:25:20", fill=(230, 230, 230))
sheet2.paste(before, (0, gap2))
d2.rectangle([0, gap2 + before.height + 1, before.width, gap2 + before.height + 3],
             fill=(220, 60, 60))
d2.text((8, gap2 + before.height + 7), "AFTER   20:45:47", fill=(230, 230, 230))
sheet2.paste(after, (0, gap2 * 2 + before.height + 4))
sheet2.save(D + "cmp_toprow.png")
print("cmp_toprow.png        ", sheet2.size)
