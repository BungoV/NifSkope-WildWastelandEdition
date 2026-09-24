# Compose images/20_distance_strip.png -- the same tree at five apparent sizes.
#
#   python compose_distance.py <out.png>
#
# ROWS      the full mesh, Bethesda's three authored LOD models for it, and the
#           octahedral card. Nothing here is decimated: the LOD rows are the
#           shipped LOD NIFs opened as they are.
# COLUMNS   how many pixels tall the object lands on screen: 256, 128, 64, 32,
#           16. Each column is a separate render whose orthographic box was
#           widened until the subject measured that height, so the card's mip
#           selection is the real one for that size and not a staged one.
#
# WHY PIXELS AND NOT METRES. The brief asks for the LOD4/8/16/32 switch
# heights. Those distances are `ringEdges` in the game's configuration, not in
# this repository, and docs/LODGEN_CENSUS.md 1.1 says plainly that a per-ring
# number printed without them beside it is unreadable. What actually decides
# whether a card can stand in for a mesh is the pixel height, so that is the
# axis; the distance that produces each one follows from the reader's own
# screen and field of view, and the caption prints the arithmetic instead of
# asserting four numbers nobody measured.
import os
import re
import sys
from math import tan, radians

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.abspath(__file__))
DIST = os.path.join(ROOT, "dist")
BG = (24, 24, 26)
FG = (232, 232, 232)
DIM = (150, 150, 155)
ACC = (150, 190, 240)

# (folder tag, row label, which grab of that run to use)
ROWS = [
    ("mesh", "full mesh", "scene"),
    ("lod0", "vanilla LOD_0", "scene"),
    ("lod1", "vanilla LOD_1", "scene"),
    ("lod2", "vanilla LOD_2", "scene"),
    ("mesh", "octahedral card  N=4", "card"),
]
SCREEN_H = 1080.0
FOV_Y = 50.0


def font(size, bold=False):
    for name in (("arialbd.ttf" if bold else "arial.ttf"),
                 "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"):
        for d in ("C:/Windows/Fonts", "/usr/share/fonts/truetype/dejavu"):
            p = os.path.join(d, name)
            if os.path.exists(p):
                try:
                    return ImageFont.truetype(p, size)
                except OSError:
                    pass
    return ImageFont.load_default()


def read_log(tag):
    rows, half_h, vp = {}, None, None
    with open(os.path.join(DIST, tag + ".log"), encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.match(r"^distance strip: .* viewport height (\d+) px,"
                         r" set half ([\d.]+) x ([\d.]+)", line)
            if m:
                vp = int(m.group(1))
                half_h = float(m.group(3))
            m = re.match(r"^distance px (\d+) widen [\d.]+ scene ([\d.]+) card ([\d.]+) iou ([-\d.]+)", line)
            if m:
                rows[int(m.group(1))] = (float(m.group(2)), float(m.group(3)), float(m.group(4)))
    return rows, half_h, vp


def tile(tag, px, which, box, out_side):
    """The centre of the grab, cropped to `box` px and fitted to the cell.

    THE RESAMPLE IS CHOSEN BY DIRECTION, and the first draft of this picture
    got it wrong in a way worth recording: NEAREST for everything. Blowing a
    16-pixel subject up with NEAREST is right -- the interesting thing there is
    what each texel did, and a smooth filter would draw a nicer picture of
    something that is not there. But SHRINKING the 256-pixel column with
    NEAREST throws away two pixels in three, and on a bare tree that turns a
    trunk into confetti and libels the card. So: BOX (area average) when the
    tile must come down, NEAREST only when it goes up.
    """
    p = os.path.join(DIST, tag, "d_px%04d_%s.png" % (px, which))
    im = Image.open(p).convert("RGB")
    cx, cy = im.width // 2, im.height // 2
    h = box // 2
    im = im.crop((cx - h, cy - h, cx - h + box, cy - h + box))
    how = Image.NEAREST if out_side >= box else Image.BOX
    return im.resize((out_side, out_side), how)


def main(out):
    rows_mesh, half_h, vp = read_log("mesh")
    pxs = sorted(rows_mesh.keys(), reverse=True)

    cell = 190
    pad = 8
    rowlbl_w = 170
    head_h = 34
    lbl_h = 40
    num_h = 18

    W = rowlbl_w + len(pxs) * (cell + pad) + pad
    H = head_h + lbl_h + len(ROWS) * (cell + num_h + pad) + 78
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    f_head = font(19, True)
    f_lbl = font(13)
    f_row = font(14, True)
    f_num = font(12)

    d.text((pad, 8), "TreeMapleblasted05 at five apparent sizes  "
                     "- full mesh, the three authored vanilla LOD models, and the card",
           font=f_head, fill=FG)

    y = head_h
    for i, px in enumerate(pxs):
        x = rowlbl_w + i * (cell + pad)
        dist = half_h * SCREEN_H / (px * tan(radians(FOV_Y / 2.0)))
        d.text((x + 2, y + 2), "%d px tall" % px, font=f_row, fill=ACC)
        d.text((x + 2, y + 20), "= %.0f units away" % dist, font=f_lbl, fill=DIM)
    y += lbl_h

    for tag, label, which in ROWS:
        rows, _, _ = read_log(tag)
        d.text((4, y + cell // 2 - 8), label, font=f_row, fill=FG)
        for i, px in enumerate(pxs):
            x = rowlbl_w + i * (cell + pad)
            box = max(24, int(px * 1.7))
            im.paste(tile(tag, px, which, box, cell), (x, y))
            cov = rows.get(px, (0, 0, 0))[1 if which == "card" else 0]
            d.text((x + 2, y + cell + 2),
                   "covers %.4f%% of frame" % (cov * 100.0), font=f_num, fill=DIM)
        y += cell + num_h + pad

    d.text((pad, H - 50),
           "each column is its own render: the orthographic box was widened until the subject"
           " measured that many pixels, so the card reads the mip a real draw would read",
           font=f_lbl, fill=DIM)
    d.text((pad, H - 34),
           "distance = halfHeight x screenHeight / (pixels x tan(fovY/2)), with halfHeight %.1f units,"
           " screen %d px and fovY %.0f deg -- change the two you own and the column moves"
           % (half_h, int(SCREEN_H), FOV_Y), font=f_lbl, fill=DIM)
    d.text((pad, H - 18),
           "no mesh here is decimated: the LOD rows are Bethesda's own authored LOD NIFs;"
           " the card is unshaded baked colour (IMPOSTOR_MATERIAL_SEAM)", font=f_lbl, fill=DIM)
    # Azimuth 45 is a DIAGONAL, where the weight is spread over three frames and
    # the blend is at its worst. It is kept rather than swapped for a cardinal
    # so this strip is not the flattering angle; the orbit's 24-view mean is
    # printed beside it so one hard view is not read as the typical one.
    d.text((pad, H - 66),
           "azimuth 45 deg is a DIAGONAL -- the three-frame blend's worst case, kept on purpose."
           "  IoU here 0.18-0.23; the 24-view orbit mean for this set is 0.3546",
           font=f_lbl, fill=ACC)
    im.save(out)
    print("distance strip -> %s  %dx%d" % (out, im.width, im.height))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else
         os.path.join(ROOT, "..", "..", "images", "20_distance_strip.png"))
