#!/usr/bin/env python
"""THE FIX, tried on the sheet bytes before it is written into the bake.

threshold.py decided the threshold inside the source render alone: at the card's
own texel pitch the bake's coverage floor (16/255) reproduces the source
silhouette's bbox to 0.63 texels worst of twelve half-extents, and the reader's
own alpha test (0.5) to 2.07. So the coverage definition is right and the
DISAGREEMENT is the defect -- the `.lodm` declares a quad sized to the 16/255
silhouette and the consumer draws the 0.5 one, up to 5.41 texels of half-width
short (measure.py section C).

THE RE-ENCODING. Write the coverage so that the reader's own test selects
exactly the coverage the bake measured:

    a' = 0                                              a <  floor
    a' = test + round( (a - floor) * (255 - test) / (255 - floor) )   otherwise

with floor = 16 (the spec's coverage floor) and test = 128 (the alpha test both
specs tell a consumer to use). Then { a' >= test } == { a >= floor } EXACTLY, by
construction; 255 stays 255, so a solid interior is untouched; and the fraction
survives monotone and invertible in [test, 255], so a consumer that blends
instead of testing recovers it as
    a = floor + (a' - test) * (255 - floor) / (255 - test).

This checks the construction on the sheets already on disk, and -- because the
sheet SHIPS as BC3, whose alpha is an eight-step ramp per 4x4 block -- checks it
again after a BC3 round trip, which is where a re-encoding that only holds in
PNG would fall over.

FLOOR: the same check against a re-encoding with the floor deliberately at 64,
which must NOT reproduce the 16/255 set.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image

import sheetlib as S
from measure import frame_extent, widest

REPO = "E:/Projects/NifskopeWildWastelandEdition"
RUN = os.path.join(REPO, "scratchpad", "cardortho_20260910")
CARDS = os.path.join(RUN, "cards")
OUT = os.path.join(REPO, "scratchpad", "cardwidth_20260910")
TREES = ["0003a28b", "0004a074", "00038599"]
FLOOR, TEST = 16, 128


def encode(a, floor=FLOOR, test=TEST):
    a = a.astype(np.int32)
    out = np.zeros_like(a)
    m = a >= floor
    out[m] = test + ((a[m] - floor) * (255 - test) + (255 - floor) // 2) // (255 - floor)
    return np.clip(out, 0, 255).astype(np.uint8)


def decode(ap, floor=FLOOR, test=TEST):
    ap = ap.astype(np.int32)
    out = np.zeros_like(ap)
    m = ap >= test
    out[m] = floor + ((ap[m] - test) * (255 - floor) + (255 - test) // 2) // (255 - test)
    return np.clip(out, 0, 255).astype(np.uint8)


def bc3_alpha_roundtrip(a):
    """A BC3 alpha block encoder+decoder: endpoints max/min, eight-step ramp,
    nearest index. The alpha half of DXT5 is exactly this, so a re-encoding that
    survives here survives the shipped file."""
    H, W = a.shape
    out = np.zeros_like(a)
    for by in range(0, H, 4):
        for bx in range(0, W, 4):
            blk = a[by:by + 4, bx:bx + 4].astype(np.int32)
            a0, a1 = int(blk.max()), int(blk.min())
            if a0 == a1:
                out[by:by + 4, bx:bx + 4] = a0
                continue
            ramp = np.array([a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)])
            idx = np.abs(blk[:, :, None] - ramp[None, None, :]).argmin(axis=2)
            out[by:by + 4, bx:bx + 4] = ramp[idx]
    return out


def main():
    lines = []

    def out(s=""):
        print(s)
        lines.append(s)

    out("# Lane CARDWIDTH -- the re-encoding, on the sheet bytes")
    out()
    out("## N. { a' >= 128 } == { a >= 16 }, in PNG and after a BC3 round trip")
    out()
    out("| tree | texels a>=16 | a'>=128 | disagreeing | after BC3: a'>=128 | disagreeing | worst halfW move, texels |")
    out("|---|---|---|---|---|---|---|")
    for ident in TREES:
        c = S.card_of(CARDS, ident)
        a = np.array(Image.open(os.path.join(CARDS, ident + "_oct_albedo.png")).convert("RGBA"))[:, :, 3]
        ap = encode(a)
        bc = bc3_alpha_roundtrip(ap)
        s0 = a >= FLOOR
        s1 = ap >= TEST
        s2 = bc >= TEST
        # the silhouette each set draws, on the widest frame of the old sheet
        bw, _ = widest(a, c, FLOOR)
        f0 = frame_extent(S.frame_alpha(a, c, bw[0], bw[1]), FLOOR)
        f1 = frame_extent(S.frame_alpha(ap, c, bw[0], bw[1]), TEST)
        f2 = frame_extent(S.frame_alpha(bc, c, bw[0], bw[1]), TEST)
        out("| %s | %d | %d | %d | %d | %d | %.2f |"
            % (ident, int(s0.sum()), int(s1.sum()), int((s0 != s1).sum()),
               int(s2.sum()), int((s0 != s2).sum()),
               max(abs(f1[0] - f0[0]), abs(f2[0] - f0[0]))))
    out()

    out("## O. The fraction is recoverable: decode(encode(a)) against a")
    out()
    out("| tree | texels a>=16 | max |decode-a| | mean |decode-a| |")
    out("|---|---|---|---|")
    for ident in TREES:
        a = np.array(Image.open(os.path.join(CARDS, ident + "_oct_albedo.png")).convert("RGBA"))[:, :, 3]
        d = decode(encode(a)).astype(np.int32)
        m = a >= FLOOR
        err = np.abs(d[m] - a[m].astype(np.int32))
        out("| %s | %d | %d | %.3f |" % (ident, int(m.sum()), int(err.max()), float(err.mean())))
    out()

    out("## P. Floor: a re-encoding with the floor at 64 must NOT reproduce the 16/255 set")
    out()
    out("| tree | floor 16: disagreeing texels | floor 64: disagreeing | floor is discriminating? |")
    out("|---|---|---|---|")
    for ident in TREES:
        a = np.array(Image.open(os.path.join(CARDS, ident + "_oct_albedo.png")).convert("RGBA"))[:, :, 3]
        s0 = a >= FLOOR
        d16 = int((s0 != (encode(a, 16) >= TEST)).sum())
        d64 = int((s0 != (encode(a, 64) >= TEST)).sum())
        out("| %s | %d | %d | %s |" % (ident, d16, d64, "YES" if d64 > 100 and d16 == 0 else "NO -- refused"))
    out()

    open(os.path.join(OUT, "reencode.md"), "w").write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
