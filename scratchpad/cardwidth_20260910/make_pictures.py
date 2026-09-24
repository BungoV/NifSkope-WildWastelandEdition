#!/usr/bin/env python
"""The texel picture for the coverage contract (ww-texel-picture).

The defect is not visible in a render: it is which texels a consumer's alpha test
keeps. Three columns, per the skill's section 6, because this is a change of LAW
and not of content --

  1. the sheet the bake wrote, read at the bake's own coverage floor 16/255.
     This is the silhouette `half` and `frameOffset` describe, and section 5 of
     the report measured it against the source's own to 0.63 texels;
  2. THE SAME SHEET read at the reader's alpha test, 0.5. This is what a
     consumer draws today, and the red texels are what it loses;
  3. the sheet after the re-encoding, read at the same 0.5. It must equal
     column 1 texel for texel -- the whole claim, in one picture.

The frame drawn is chosen BY THE METRIC on the BEFORE artefact: the frame whose
half-width loses the most between column 1 and column 2, searched over all 64.

Runs with the game up: it reads the sheets already on disk and applies the new
law offline. After the re-bake it can be pointed at the new cards, where column 2
and column 3 will be the same bytes.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image, ImageDraw

import sheetlib as S
from measure import frame_extent
from reencode import encode

REPO = "E:/Projects/NifskopeWildWastelandEdition"
CARDS = os.path.join(REPO, "scratchpad", "cardortho_20260910", "cards")
OUT = os.path.join(REPO, "scratchpad", "cardwidth_20260910")
TREES = ["0003a28b", "0004a074", "00038599"]
NAMES = {"0003a28b": "TreeHero01", "0004a074": "TreeMapleForest2", "00038599": "TreeBlasted01"}
FLOOR, TEST = 16, 128
CELL_W, CELL_H = 460, 470
CHECK = (56, 56, 60), (40, 40, 44)


def draw_frame(sets, w, h, mag):
    """`sets` = (keep, lost): a magnified picture of one frame's coverage.

    Checkerboard under transparency (8 device pixels a square, so it cannot be
    read as content), nearest magnification by an integer factor, and the texel
    grid only once a texel is 4 device pixels or more.
    """
    keep, lost = sets
    img = Image.new("RGB", (w * mag, h * mag))
    d = ImageDraw.Draw(img)
    for y in range(0, h * mag, 8):
        for x in range(0, w * mag, 8):
            d.rectangle([x, y, x + 7, y + 7], fill=CHECK[((x // 8) + (y // 8)) & 1])
    for y in range(h):
        for x in range(w):
            if keep[y, x]:
                d.rectangle([x * mag, y * mag, (x + 1) * mag - 1, (y + 1) * mag - 1], fill=(226, 222, 206))
            elif lost[y, x]:
                d.rectangle([x * mag, y * mag, (x + 1) * mag - 1, (y + 1) * mag - 1], fill=(214, 66, 48))
    if mag >= 4:
        for x in range(w + 1):
            d.line([(x * mag, 0), (x * mag, h * mag)], fill=(20, 20, 22))
        for y in range(h + 1):
            d.line([(0, y * mag), (w * mag, y * mag)], fill=(20, 20, 22))
    return img


def main():
    for ident in TREES:
        c = S.card_of(CARDS, ident)
        a = np.array(Image.open(os.path.join(CARDS, ident + "_oct_albedo.png")).convert("RGBA"))[:, :, 3]
        ap = encode(a)                        # the same sheet under the NEW law
        tw, th = c["tw"], c["th"]
        # pick BY THE METRIC on the BEFORE artefact: the frame that loses the most
        # half-width between the floor and the reader's test
        best = None
        for j in range(c["oct"]):
            for i in range(c["oct"]):
                fa = S.frame_alpha(a, c, i, j)
                e0, e1 = frame_extent(fa, FLOOR), frame_extent(fa, TEST)
                if not e0:
                    continue
                loss = e0[0] - (e1[0] if e1 else 0.0)
                if best is None or loss > best[0]:
                    best = (loss, i, j)
        loss, fi, fj = best
        fa = S.frame_alpha(a, c, fi, fj)
        fp = S.frame_alpha(ap, c, fi, fj)
        s_floor = fa >= FLOOR
        s_test = fa >= TEST
        s_new = fp >= TEST
        cols = [("the bake's coverage, alpha >= %d" % FLOOR, s_floor, np.zeros_like(s_floor)),
                ("read at the consumer's 0.5, TODAY", s_test, s_floor & ~s_test),
                ("re-encoded, read at the same 0.5", s_new, s_floor & ~s_new)]
        mag = max(1, min(CELL_W // tw, (CELL_H - 96) // th))
        # the cell is the same for all three panels (so no caption clips) but it is
        # sized to the CONTENT, not to a constant, or a short frame gets a page of
        # empty ground under it
        cw = max(430, tw * mag + 24)   # wide enough that no caption reaches its neighbour
        ch = th * mag + 60
        page = Image.new("RGB", (3 * cw, ch + 54), (14, 14, 16))
        dr = ImageDraw.Draw(page)
        dr.text((8, 8), "%s %s   frame (%d,%d) of %d x %d, %d x %d texels, one texel = %.3f units"
                % (ident, NAMES[ident], fi, fj, c["oct"], c["oct"], tw, th, 2.0 * c["halfW"] / tw),
                fill=(232, 232, 232))
        dr.text((8, 26), "pale = drawn;  RED = coverage the bake measured that the reading LOSES;"
                         "  checker = transparent.  Frame chosen by the metric: the worst loss of the 64",
                fill=(190, 190, 190))
        for k, (cap, keep, lostm) in enumerate(cols):
            e = frame_extent(keep.astype(np.uint8) * 255, 1)
            hw = e[0] if e else 0.0
            img = draw_frame((keep, lostm), tw, th, mag)
            x0 = k * cw
            page.paste(img, (x0 + (cw - img.size[0]) // 2, 54 + 44))
            dr.text((x0 + 8, 54 + 6), cap, fill=(232, 232, 232))
            ref = frame_extent(fa, FLOOR)[0]
            col = (232, 232, 232) if abs(hw - ref) < 0.51 else (240, 120, 100)
            dr.text((x0 + 8, 54 + 24), "half-width %.1f tex (bake %.1f), %d lost"
                    % (hw, ref, int(lostm.sum())), fill=col)
        out = os.path.join(OUT, "cardwidth_coverage_%s.png" % ident)
        page.save(out)
        print("  %s  frame (%d,%d) loses %.1f texels of half-width; %d bytes"
              % (out, fi, fj, loss, os.path.getsize(out)))


if __name__ == "__main__":
    main()
