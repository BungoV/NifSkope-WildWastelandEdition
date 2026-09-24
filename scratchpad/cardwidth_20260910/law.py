#!/usr/bin/env python
"""THE CANDIDATE LAW, tried on the sheet bytes BEFORE any code is written.

measure.py established, on the bake already on disk:

  * the sheet's coverage at the bake's own floor (16/255) agrees with pass one's
    viewport measurement to under one texel on every tree and both axes -- so the
    sheet is not wide;
  * at the reader's own alpha test (0.5, both specs) the card's silhouette is
    SHORT by up to 5.41 texels of half-width -- the tree shrinks at the
    transition;
  * dilation moves coverage by exactly 0 texels, and BC3 by at most 0.5.

THE LAW UNDER TEST -- coverage-preserving downsample. A box filter conserves the
alpha INTEGRAL, so for any frame

    sum( alpha ) / 255   ==   the source's covered area, in texel units

exactly, with no extra data needed from the bake. Pick per frame the scale `k`
that makes the area the reader actually draws equal it:

    | { k * alpha >= readerTest } |  ==  sum( alpha ) / 255

That is Castano's alpha-coverage-preserving mip rule with the target area taken
from the frame's own integral. On a SOLID silhouette (the cube fixture) the two
sides are already equal and k == 1, so the law cannot move a fixture it must not
move; on a lacy crown it promotes the sub-texel twigs a texel at a time until the
drawn area is the source's, which is what pulls the silhouette back out to the
source's extent.

THE FLOOR for the law: the SAME rescale asked to hit a target 5% too small must
come out 5% too small. A law that returns the right answer for every target is
not measuring the target.
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
NAMES = {"0003a28b": "TreeHero01", "0004a074": "TreeMapleForest2", "00038599": "TreeBlasted01"}
TEST = 128


def coverage_scale(fa, test=TEST, target_mul=1.0):
    """`k` such that |{ k*a >= test }| == sum(a)/255 * target_mul, by bisection.

    Returns (k, drawn_area_after, target_area). k is clamped to [1, 16]: below 1
    the law would CUT coverage the bake measured, which is the defect it exists
    to remove, and 16 is where every texel over alpha 8 is already promoted.
    """
    a = fa.astype(np.float64)
    target = a.sum() / 255.0 * target_mul
    if target <= 0:
        return 1.0, 0.0, 0.0
    lo, hi = 1.0, 16.0
    if float((a >= test).sum()) >= target:
        return 1.0, float((a >= test).sum()), target
    for _ in range(40):
        mid = 0.5 * (lo + hi)
        if float((a * mid >= test).sum()) < target:
            lo = mid
        else:
            hi = mid
    k = 0.5 * (lo + hi)
    return k, float((a * k >= test).sum()), target


def apply_scale(fa, k):
    return np.clip(np.rint(fa.astype(np.float64) * k), 0, 255).astype(np.uint8)


def main():
    lines = []

    def out(s=""):
        print(s)
        lines.append(s)

    out("# Lane CARDWIDTH -- the coverage-preserving law, tried on the sheet bytes")
    out()

    out("## F. The law, per frame, on the widest and tallest frame of each tree")
    out()
    out("`ref` is `framefit`'s pass-one half-extent in texels (the source silhouette,")
    out("measured at 7.4x the card's resolution). `err` is the card's half-extent at the")
    out("READER's test minus it: negative = the tree shrinks at the transition.")
    out()
    out("| tree | axis | frame | ref texels | err before | k | err after | drawn area before | after | target |")
    out("|---|---|---|---|---|---|---|---|---|---|")
    per_tree = {}
    for ident in TREES:
        c = S.card_of(CARDS, ident)
        a_png = np.array(Image.open(os.path.join(CARDS, ident + "_oct_albedo.png")).convert("RGBA"))[:, :, 3]
        tex_x = 2.0 * c["halfW"] / c["tw"]
        tex_y = 2.0 * c["halfH"] / c["th"]
        refx, refy = c["framefit"][0] / tex_x, c["framefit"][1] / tex_y
        bw, bh = widest(a_png, c, TEST)
        per_tree[ident] = (c, a_png, refx, refy)
        for ax, fr, ref, idx in (("x", bw, refx, 0), ("y", bh, refy, 1)):
            fa = S.frame_alpha(a_png, c, fr[0], fr[1])
            e0 = frame_extent(fa, TEST)
            k, drawn, target = coverage_scale(fa)
            e1 = frame_extent(apply_scale(fa, k), TEST)
            out("| %s | %s | (%d,%d) | %.2f | %+.2f | %.3f | %+.2f | %.0f | %.0f | %.0f |"
                % (ident, ax, fr[0], fr[1], ref, e0[idx] - ref, k, e1[idx] - ref,
                   float((fa >= TEST).sum()), drawn, target))
    out()

    out("## G. The law over ALL 64 frames of each tree: the widest frame at the reader's test")
    out()
    out("The number bungo's rule turns on is the SILHOUETTE the reader draws against the")
    out("source's own. Before and after, over every frame of the sheet.")
    out()
    out("| tree | half-extent, texels | ref | before | after | before, %% of ref | after |")
    out("|---|---|---|---|---|---|---|")
    scaled_sheets = {}
    for ident in TREES:
        c, a_png, refx, refy = per_tree[ident]
        a_new = a_png.copy()
        ks = []
        for j in range(c["oct"]):
            for i in range(c["oct"]):
                fa = S.frame_alpha(a_png, c, i, j)
                k, _, _ = coverage_scale(fa)
                ks.append(k)
                a_new[j * c["th"]:(j + 1) * c["th"], i * c["tw"]:(i + 1) * c["tw"]] = apply_scale(fa, k)
        scaled_sheets[ident] = (a_new, ks)
        bw0, bh0 = widest(a_png, c, TEST)
        bw1, bh1 = widest(a_new, c, TEST)
        out("| %s | widest halfW | %.2f | %.2f | %.2f | %+.1f%% | %+.1f%% |"
            % (ident, refx, bw0[2], bw1[2], 100 * (bw0[2] - refx) / refx, 100 * (bw1[2] - refx) / refx))
        out("| %s | tallest halfH | %.2f | %.2f | %.2f | %+.1f%% | %+.1f%% |"
            % (ident, refy, bh0[2], bh1[2], 100 * (bh0[2] - refy) / refy, 100 * (bh1[2] - refy) / refy))
        out("| %s | k over 64 frames | | | | min %.2f median %.2f max %.2f | |"
            % (ident, min(ks), float(np.median(ks)), max(ks)))
    out()

    out("## H. Floor 1: the law asked for a target 5% too small must MISS by 5%")
    out()
    out("| tree | frame | target x1.00 drawn/target | target x0.95 drawn/target x1.00 | law is target-sensitive? |")
    out("|---|---|---|---|---|")
    for ident in TREES:
        c, a_png, refx, refy = per_tree[ident]
        bw, _ = widest(a_png, c, TEST)
        fa = S.frame_alpha(a_png, c, bw[0], bw[1])
        k1, d1, t1 = coverage_scale(fa, target_mul=1.0)
        k2, d2, _ = coverage_scale(fa, target_mul=0.95)
        r1 = d1 / max(1e-9, t1)
        r2 = d2 / max(1e-9, t1)
        out("| %s | (%d,%d) | %.4f | %.4f | %s |"
            % (ident, bw[0], bw[1], r1, r2,
               "YES" if abs(r2 - 0.95) < 0.02 and abs(r1 - 1.0) < 0.02 else "NO -- refused"))
    out()

    out("## I. Floor 2: a SOLID silhouette must not move (the cube fixture's condition)")
    out()
    out("A filled rectangle box-filtered into a 120-texel inner rect: the integral and")
    out("the drawn area already agree, so k must come back 1.000 and the extent must not")
    out("move by a texel. A law that fattens a cube is refused.")
    out()
    out("| source half-width, px | ref texels | k | halfW before | after | moved? |")
    out("|---|---|---|---|---|---|")
    IW, VP = 120, 888
    for hw in (44.0, 88.3, 200.7, 300.0, 440.0):
        src = np.zeros((VP, VP), dtype=np.float64)
        x0, x1 = VP / 2 - hw, VP / 2 + hw
        for x in range(VP):
            cov = max(0.0, min(x + 1.0, x1) - max(float(x), x0))
            src[:, x] = 255.0 * min(1.0, cov)
        fa = np.array(Image.fromarray(src.astype(np.uint8), "L").resize((IW, IW), Image.BOX))
        ref = hw * IW / VP
        k, _, _ = coverage_scale(fa)
        e0 = frame_extent(fa, TEST)
        e1 = frame_extent(apply_scale(fa, k), TEST)
        out("| %.1f | %.3f | %.3f | %.2f | %.2f | %s |"
            % (hw, ref, k, e0[0], e1[0],
               "no" if abs(e1[0] - e0[0]) < 0.51 else "YES -- %+.2f texels" % (e1[0] - e0[0])))
    out()

    np.save(os.path.join(OUT, "scaled_alpha_0003a28b.npy"), scaled_sheets["0003a28b"][0])
    open(os.path.join(OUT, "law.md"), "w").write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
