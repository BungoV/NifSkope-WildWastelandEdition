"""Lane UI5-HOVERPIC -- the HOVERED "Spells" title, before and after lane UI5.

Both halves come from release/ui5_hoverprobe.exe, a standalone Qt program that
renders a real QMenuBar under release/style.qss with the ${...} skin tokens
substituted the way src/nifskope_ui.cpp:30199 does, hovers Spells with a
synthetic QMouseEvent, and QWidget::grab()s the bar. Never a desktop capture,
and never release/NifSkope.exe -- lane UI6 holds the one allowed instance.

  BEFORE = images/hover_bar_before.png -- the row sheet as it stood before lane
           UI5: wwBarRowBoxQss + wwBarRowButtonQss( 26 ) and nothing else on
           QMenuBar::item, so the item is 2 + 16 + 2 = 20 px at the TOP of the
           35 px row and the :selected highlight is 20 px tall.
  AFTER  = images/hover_bar_after.png -- the same, plus wwBarRowMenuItemQss
           ( 35, 16 ) appended last: padding-top 9 / bottom 10, so the item's
           box IS the row and the highlight is 35 px tall.

Same layout as make_pictures.py's cmp_menu_zoom.png: 4x NEAREST, a red rule
between the halves, each half labelled. A pixel claim stays a pixel.

Run: python scratchpad/ui5_20260910/make_hoverpic.py
"""
import os
from PIL import Image, ImageDraw

D = "scratchpad/ui5_20260910/images/"
B = D + "hover_bar_before.png"
A = D + "hover_bar_after.png"
CROP = (0, 0, 270, 35)
SCALE = 4

before = Image.open(B).convert("RGB")
after = Image.open(A).convert("RGB")
print("before", B, before.size)
print("after ", A, after.size)

HILITE = (74, 122, 176)          # res/style.qss:49  rgba(${rgb}, 255)


def hilite_rows(img):
    """The measured height of the highlight, read back out of the SAVED png so
    the number under the picture is the number in the picture."""
    px = img.load()
    top, bot = None, None
    for y in range(img.height):
        for x in range(img.width):
            if px[x, y] == HILITE:
                if top is None:
                    top = y
                bot = y
                break
    return top, bot


bt, bb = hilite_rows(before)
at, ab = hilite_rows(after)
assert bt is not None and at is not None, "no highlight in a grab -- not the hovered state"
bh, ah = bb - bt + 1, ab - at + 1
print("BEFORE highlight y %d..%d -> %d px" % (bt, bb, bh))
print("AFTER  highlight y %d..%d -> %d px" % (at, ab, ah))

bz = before.crop(CROP).resize(
    ((CROP[2] - CROP[0]) * SCALE, (CROP[3] - CROP[1]) * SCALE), Image.NEAREST)
az = after.crop(CROP).resize(
    ((CROP[2] - CROP[0]) * SCALE, (CROP[3] - CROP[1]) * SCALE), Image.NEAREST)
bz.save(D + "hover_zoom_before.png")
az.save(D + "hover_zoom_after.png")

LBL_B = ("BEFORE lane UI5   hover box y %d..%d  ->  %d px tall in a 35 px row"
         "   (titles at the top)" % (bt, bb, bh))
LBL_A = ("AFTER  lane UI5   hover box y %d..%d  ->  %d px tall in a 35 px row"
         "   (titles centred)" % (at, ab, ah))
FOOT = ["both halves: standalone Qt probe (release/ui5_hoverprobe.exe) rendering a real QMenuBar under",
        "res/style.qss with the skin tokens substituted; Spells hovered by a synthetic mouse move;",
        "QWidget::grab(), 4x nearest.  A PROBE RENDER, not an in-app grab."]

gap = 26
foot = 8 + 12 * len(FOOT)
w = max(bz.width, az.width)
sheet = Image.new("RGB", (w, bz.height + az.height + gap * 2 + 6 + foot), (24, 25, 28))
d = ImageDraw.Draw(sheet)
d.text((8, 6), LBL_B, fill=(230, 230, 230))
sheet.paste(bz, (0, gap))
d.rectangle([0, gap + bz.height + 2, w, gap + bz.height + 4], fill=(220, 60, 60))
d.text((8, gap + bz.height + 8), LBL_A, fill=(230, 230, 230))
sheet.paste(az, (0, gap * 2 + bz.height + 6))
for i, line in enumerate(FOOT):
    d.text((8, gap * 2 + bz.height + 6 + az.height + 6 + i * 12), line, fill=(150, 154, 160))
sheet.save(D + "cmp_menu_hover.png")
print("cmp_menu_hover.png", sheet.size)
