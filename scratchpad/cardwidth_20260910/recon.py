#!/usr/bin/env python
"""Reconnaissance: what the on-disk renders actually contain, before any verdict.

CONSTITUTION 4: a mask that touches the viewport edge is CLIPPED, and a width or
a height read off it is the viewport's number, not the object's. This prints the
clipping state of every arm of the 2026-09-10 01:1x run before a single width is
compared, because the run's own report quotes `height diff 0.00%` on all three
trees off masks that may both simply fill the frame.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import sheetlib as S

REPO = "E:/Projects/NifskopeWildWastelandEdition"
RUN = os.path.join(REPO, "scratchpad", "cardortho_20260910")
CARDS = os.path.join(RUN, "cards")
TREES = ["0003a28b", "0004a074", "00038599"]

print("| render | vp | upp | src bbox | w x h | touches edge? |")
print("|---|---|---|---|---|---|")
for ident in TREES:
    for tag in ("front_mid", "front_ring", "right_mid", "right_ring", "ortho"):
        png = os.path.join(RUN, "%s_%s_src.png" % (ident, tag))
        cam = os.path.join(RUN, "%s_%s_src.camera" % (ident, tag))
        if not os.path.isfile(png):
            print("| %s %s | MISSING |" % (ident, tag))
            continue
        m = S.mesh_mask(png)
        bb = S.bbox(m)
        c = S.camera(cam)
        print("| %s %s | %s | %s | %s | %dx%d | %s |"
              % (ident, tag, c.get("vp"), c.get("upp"), bb,
                 bb[2] - bb[0], bb[3] - bb[1], "YES" if S.touches_edge(m) else "no"))

print()
print("| tree | frame | halfW | halfH | texel units | card px w (ortho upp) | card px h | fits 1507x941? |")
print("|---|---|---|---|---|---|---|---|")
for ident in TREES:
    c = S.card_of(CARDS, ident)
    cam = S.camera(os.path.join(RUN, "%s_ortho_src.camera" % ident))
    upp = float(cam["upp"])
    pw, ph = 2 * c["halfW"] / upp, 2 * c["halfH"] / upp
    print("| %s | %dx%d | %.2f | %.2f | %.3f | %.1f | %.1f | %s |"
          % (ident, c["tw"], c["th"], c["halfW"], c["halfH"],
             2 * c["halfW"] / c["tw"], pw, ph,
             "yes" if (pw <= 1507 and ph <= 941) else "NO -- CLIPPED"))
