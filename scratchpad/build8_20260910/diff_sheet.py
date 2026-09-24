#!/usr/bin/env python3
"""Lane BUILD8: THE DIFFERENCE PICTURE, made so it can be seen.

  python diff_sheet.py <prefixA> <prefixB> <title> <out.png>

Skill `ww-texel-picture`: the thing under test is a handful of pixels on a
silhouette, so a full-frame subtract amplified by a fixed factor is a black
rectangle and reads as "no difference" (x16 on a step of 1 gives 16 of 255 --
made exactly that mistake first). Instead, per tile:

  * crop to the bounding box of the difference, with a margin, so the pixels
    under test fill the cell;
  * amplify to FULL RANGE (255 / the tile's own worst step) and SAY the factor
    in the caption -- the number in the picture is the number in the report;
  * a tile with no difference gets a flat cell that says so, not an empty one.

The caption carries the count, the worst step and the crop, all read from the
same comparison the report quotes.
"""
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, "images")
TAGS = [("bind", "BIND POSE (no clip)"), ("f0", "frame 0"), ("fq1", "1/4"),
        ("fhalf", "1/2"), ("fq3", "3/4"), ("flast", "last")]
FONTS = ["C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"]
BOLD = ["C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/arialbd.ttf"]


def font(sz, bold=False):
    for p in (BOLD if bold else []) + FONTS:
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def main():
    pa, pb, title, out = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
    cells = []
    for tag, label in TAGS:
        a = Image.open(os.path.join(IMG, "%s_%s.png" % (pa, tag))).convert("RGB")
        b = Image.open(os.path.join(IMG, "%s_%s.png" % (pb, tag))).convert("RGB")
        d = ImageChops.difference(a, b)
        bands = d.split()
        worst = max(bd.getextrema()[1] for bd in bands)
        data = list(zip(*[bd.getdata() for bd in bands]))
        px = sum(1 for p in data if p[0] or p[1] or p[2])
        if px == 0:
            cells.append((None, label, "IDENTICAL -- 0 of %d pixels" % (a.size[0] * a.size[1]), ""))
            continue
        bbox = d.getbbox()
        # square the crop around the bbox, with a margin, clamped to the image
        m = 24
        l, t, r, bt = bbox[0] - m, bbox[1] - m, bbox[2] + m, bbox[3] + m
        w, h = r - l, bt - t
        s = max(w, h, 96)
        cx, cy = (l + r) // 2, (t + bt) // 2
        l = max(0, min(a.size[0] - s, cx - s // 2))
        t = max(0, min(a.size[1] - s, cy - s // 2))
        crop = (l, t, l + s, t + s)
        amp = max(1, 255 // max(1, worst))
        pic = d.crop(crop).point(lambda v: min(255, v * amp))
        cells.append((pic, label,
                      "%d px differ, worst step %d of 255" % (px, worst),
                      "crop %dx%d at (%d,%d), amplified x%d" % (s, s, l, t, amp)))

    CW, CAP, M, G, COLS, HDR = 300, 66, 26, 14, 3, 92
    ROWS = (len(cells) + COLS - 1) // COLS
    W = M * 2 + COLS * CW + (COLS - 1) * G
    H = HDR + M + ROWS * (CW + CAP) + (ROWS - 1) * G + M
    sheet = Image.new("RGB", (W, H), (22, 22, 24))
    dr = ImageDraw.Draw(sheet)
    dr.text((M, 22), title, font=font(24, True), fill=(240, 240, 245))
    dr.text((M, 54), "difference between the two renders, per tile. Same pinned camera; the same clip "
                     "rendered twice differs by 0 pixels (the noise floor).",
            font=font(14), fill=(150, 152, 160))
    for i, (pic, label, sub, sub2) in enumerate(cells):
        r, c = divmod(i, COLS)
        x = M + c * (CW + G)
        y = HDR + M + r * (CW + CAP + G)
        if pic is None:
            dr.rectangle([x, y, x + CW - 1, y + CW - 1], fill=(30, 44, 32), outline=(70, 100, 74))
            dr.text((x + 12, y + CW // 2 - 10), "no differing pixel", font=font(17, True),
                    fill=(140, 210, 150))
        else:
            sheet.paste(pic.resize((CW, CW), Image.NEAREST), (x, y))
            dr.rectangle([x, y, x + CW - 1, y + CW - 1], outline=(70, 72, 80))
        dr.rectangle([x, y + CW, x + CW - 1, y + CW + CAP - 1], fill=(34, 34, 38))
        dr.text((x + 9, y + CW + 5), label, font=font(17, True), fill=(235, 236, 240))
        dr.text((x + 9, y + CW + 26), sub, font=font(13), fill=(200, 170, 130))
        dr.text((x + 9, y + CW + 44), sub2, font=font(12), fill=(140, 142, 150))
        for txt, f in ((label, font(17, True)), (sub, font(13)), (sub2, font(12))):
            assert 9 + dr.textlength(txt, font=f) < CW, "caption %r does not fit %d px" % (txt, CW)
    sheet.save(out)
    print("%s  %dx%d  %d tiles" % (out, W, H, len(cells)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
