"""Dry run of transition2's non-exe half on the sheets already on disk.

No shot is taken: the six unclipped `*_ring_src.png` of the 2026-09-10 01:1x run
stand in for the renders, so the composite, the contract read, the three arms and
the comparison are exercised with the game up. The cards are PRE-CONTRACT, so
`covTest` must come back 16 -- that is itself the check that absence is read as
absence.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import sheetlib as S
from transition2 import card_of, card_mask, compare, AXIS_FRAMES

RUN = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/cardortho_20260910"
CARDS = os.path.join(RUN, "cards")
print("| tree | view | arm | contract | test | one texel px | centre px | in texels | dx | dy |")
print("|---|---|---|---|---|---|---|---|---|---|")
for ident in ("0003a28b", "0004a074", "00038599"):
    c = card_of(CARDS, ident)
    alpha = S.dds_alpha_mip0(os.path.join(CARDS, ident + "_oct_d.DDS"))
    texw = 2.0 * c["halfW"] / c["tw"]
    for view, (fi, fj, vs) in AXIS_FRAMES.items():
        png = os.path.join(RUN, "%s_%s_ring_src.png" % (ident, view))
        cam = S.camera(png[:-4] + ".camera")
        upp = float(cam["upp"])
        mm = S.mesh_mask(png)
        assert not S.touches_edge(mm)
        for arm, kw in (("card", {}), ("ctl-offset", dict(zero_offset=True)), ("ctl-wide5", dict(wide=1.05))):
            cm = card_mask(alpha, c, fi, fj, upp, (mm.shape[1], mm.shape[0]), **kw)
            r = compare(cm, mm)
            print("| %s | %s | %s | %s | %d | %.2f | %.2f | %.2f | %.2f%% | %.2f%% |"
                  % (ident, view, arm, "yes" if c["contract"] else "absent", c["covTest"],
                     texw / upp, r[0], r[0] / (texw / upp), 100 * r[1], 100 * r[2]))
