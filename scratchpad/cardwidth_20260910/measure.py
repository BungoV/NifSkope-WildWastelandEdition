#!/usr/bin/env python
"""LANE CARDWIDTH -- where the card's extra width comes from, measured offline.

Nothing here launches the exe: every number comes off files the 2026-09-10
01:1x bake already wrote. Fallout4.exe was up for the whole lane.

THE REFERENCE, and why it is not a render. The bake's PASS ONE reads each view's
silhouette box off the VIEWPORT matte -- 941 px tall against the frame's 128
texels, so ~7.4x finer -- and the sidecar keeps two of those numbers outright:

    framefit <maxDx> <maxDy> <unionDx> <unionDy>

`maxDx` is the half-width, in world units, of the WIDEST single view's
silhouette, and `maxDy` the half-height of the tallest. Those are the source
silhouette at the card's own scale, measured by the bake itself at 7.4x the
card's resolution. Comparing the SHEET's own coverage against them needs no
render at all, and therefore carries none of a render's confounds -- which
matters, because every orthographic and every `mid` render of the 2026-09-10 run
is CLIPPED by the viewport (recon.py), so the `height diff 0.00%` in that run's
report is the viewport's height twice over, not an agreement.

THE ARMS, so the four candidates separate:

  png     the alpha the bake wrote                 -- the bake's own coverage
  dil     that alpha after `lodgenDilateFrames`    -- the dilation candidate
  dds     the alpha the shipped BC3 file carries   -- the block-compression one

and over each arm a THRESHOLD SWEEP, which is the coverage-floor candidate
(16/255) against the reader's own alpha test (0.5 = 128/255, docs/
LODGEN_CARD_SHEETS.md section 4 and LODGEN_IMPOSTOR_SPEC.md: "A consumer
alpha-tests at 0.5").

THE CONTROLS (ww-control-calibration):

  known-answer  a synthetic silhouette of KNOWN half-width put through the same
                downsample and the same reader rule: the metric must return its
                own error, not zero and not the tree's.
  floor         the same card scaled 5% wide -- the extent check must FAIL on it,
                or the extent bars in the transition gate have no floor under
                them (lane CARDORTHO B.5: dx extent read the SAME value on 12 of
                12 rows for the card arm and its control).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image

import sheetlib as S

REPO = "E:/Projects/NifskopeWildWastelandEdition"
RUN = os.path.join(REPO, "scratchpad", "cardortho_20260910")
CARDS = os.path.join(RUN, "cards")
OUT = os.path.join(REPO, "scratchpad", "cardwidth_20260910")
TREES = ["0003a28b", "0004a074", "00038599"]
NAMES = {"0003a28b": "TreeHero01", "0004a074": "TreeMapleForest2", "00038599": "TreeBlasted01"}
READER_TEST = 128           # the consumer's alpha test, 0.5, as the two specs state it
BAKE_FLOOR = 16             # the coverage floor the bake's extents are measured at
SWEEP = [1, 8, 16, 24, 32, 48, 64, 96, 128, 160, 192, 224]


def frame_extent(fa, t):
    """Half-width and half-height, in TEXELS, of the frame's coverage at alpha >= t."""
    m = fa >= t
    bb = S.bbox(m)
    if not bb:
        return None
    return (0.5 * (bb[2] - bb[0]), 0.5 * (bb[3] - bb[1]), bb)


def widest(alpha, c, t):
    """(i, j, halfW_texels) of the widest frame, and (i, j, halfH_texels) of the tallest."""
    bw = bh = None
    for j in range(c["oct"]):
        for i in range(c["oct"]):
            e = frame_extent(S.frame_alpha(alpha, c, i, j), t)
            if not e:
                continue
            if bw is None or e[0] > bw[2]:
                bw = (i, j, e[0])
            if bh is None or e[1] > bh[2]:
                bh = (i, j, e[1])
    return bw, bh


def main():
    lines = []

    def out(s=""):
        print(s)
        lines.append(s)

    out("# Lane CARDWIDTH -- the width excess, separated")
    out()

    # ------------------------------------------------------------------ 0. arms
    out("## A. The three arms, and whether they differ AT ALL")
    out()
    out("`dil` is `lodgenDilateFrames( alb, alb, ... )` -- img == coverage, so its")
    out("`put` writes the existing alpha back. `dds` is the shipped BC3 file's own top mip.")
    out()
    out("The FLOOR under the `dil` column is the SAME function called the other way:")
    out("`lodgenDilateFrames` writes the dilated alpha whenever `img != coverage` (the")
    out("normal, mask and emissive sheets), so running the model with `is_coverage=False`")
    out("moves thousands of alphas. A 0 in the coverage column is therefore a result and")
    out("not a function that never writes.")
    out()
    out("| tree | frame | texels a>=16 png | dil | dds | png vs dil (coverage) | png vs dil (FLOOR, aux path) | png vs dds |")
    out("|---|---|---|---|---|---|---|---|")
    arms = {}
    for ident in TREES:
        c = S.card_of(CARDS, ident)
        rgba = np.array(Image.open(os.path.join(CARDS, ident + "_oct_albedo.png")).convert("RGBA"))
        a_png = rgba[:, :, 3].copy()
        deep = max(8, max(c["tw"], c["th"]) // 8)
        a_dil, _ = S.dilate_frames_alpha(rgba, c["tw"], c["th"], deep, is_coverage=True)
        a_aux, _ = S.dilate_frames_alpha(rgba, c["tw"], c["th"], deep, is_coverage=False)
        a_dds = S.dds_alpha_mip0(os.path.join(CARDS, ident + "_oct_d.DDS"))
        assert a_dds.shape == a_png.shape, (a_dds.shape, a_png.shape)
        arms[ident] = (c, a_png, a_dil, a_dds)
        out("| %s | %dx%d | %d | %d | %d | %d | %d | %d |"
            % (ident, c["tw"], c["th"],
               int((a_png >= 16).sum()), int((a_dil >= 16).sum()), int((a_dds >= 16).sum()),
               int((a_png != a_dil).sum()), int((a_png != a_aux).sum()),
               int((a_png != a_dds).sum())))
    out()

    # ------------------------------------------------- 1. the threshold sweep
    out("## B. The threshold sweep against the bake's OWN pass-one measurement")
    out()
    out("`ref` = `framefit`'s `maxDx` / `maxDy` converted to texels: the widest (tallest)")
    out("view's silhouette, measured by the bake at viewport resolution. `err` is the")
    out("card's half-extent minus it, in card texels -- positive = the card is WIDER.")
    out()
    for ident in TREES:
        c, a_png, a_dil, a_dds = arms[ident]
        tex_x = 2.0 * c["halfW"] / c["tw"]        # world units per texel, x
        tex_y = 2.0 * c["halfH"] / c["th"]
        maxDx, maxDy = c["framefit"][0], c["framefit"][1]
        refx, refy = maxDx / tex_x, maxDy / tex_y
        out("### %s %s -- frame %dx%d, texel %.3f x %.3f units, ref halfW %.2f texels, halfH %.2f texels"
            % (ident, NAMES[ident], c["tw"], c["th"], tex_x, tex_y, refx, refy))
        out()
        out("| threshold | arm | widest frame | card halfW, texels | err x | tallest frame | card halfH | err y |")
        out("|---|---|---|---|---|---|---|---|")
        for t in SWEEP:
            for nm, a in (("png", a_png), ("dds", a_dds)):
                bw, bh = widest(a, c, t)
                if not bw or not bh:
                    out("| %d | %s | (empty) | | | | | |" % (t, nm))
                    continue
                out("| %d | %s | (%d,%d) | %.2f | %+.2f | (%d,%d) | %.2f | %+.2f |"
                    % (t, nm, bw[0], bw[1], bw[2], bw[2] - refx,
                       bh[0], bh[1], bh[2], bh[2] - refy))
        out()

    # ------------------------------------------- 2. the crossing, interpolated
    out("## C. The threshold at which the card's silhouette equals the source's")
    out()
    out("Linear interpolation of `err` through zero over the sweep above, per axis.")
    out()
    out("| tree | arm | axis | crossing threshold | err at 16 (the bake's floor) | err at 128 (the reader's test) |")
    out("|---|---|---|---|---|---|")
    crossings = {}
    for ident in TREES:
        c, a_png, a_dil, a_dds = arms[ident]
        tex_x = 2.0 * c["halfW"] / c["tw"]
        tex_y = 2.0 * c["halfH"] / c["th"]
        refx, refy = c["framefit"][0] / tex_x, c["framefit"][1] / tex_y
        for nm, a in (("png", a_png), ("dds", a_dds)):
            errs = {}
            for t in SWEEP:
                bw, bh = widest(a, c, t)
                errs[t] = (bw[2] - refx if bw else None, bh[2] - refy if bh else None)
            for ax, k, ref in (("x", 0, refx), ("y", 1, refy)):
                cross = None
                for p, q in zip(SWEEP, SWEEP[1:]):
                    e0, e1 = errs[p][k], errs[q][k]
                    if e0 is None or e1 is None:
                        continue
                    if (e0 >= 0) != (e1 >= 0):
                        cross = p + (q - p) * (e0 / (e0 - e1))
                        break
                crossings[(ident, nm, ax)] = cross
                out("| %s | %s | %s | %s | %+.2f | %+.2f |"
                    % (ident, nm, ax,
                       ("%.0f" % cross) if cross is not None else "none in 1..224",
                       errs[16][k], errs[128][k]))
    out()

    # -------------------------------------------------- 3. known-answer control
    out("## D. Control 1, known answer: a synthetic silhouette of known width")
    out()
    out("A rectangle of half-width `hw` viewport pixels put through the bake's own")
    out("downsample (a box average into the inner rect) and read back by the same")
    out("`frame_extent` at each threshold. The metric must report the rectangle's own")
    out("half-width, and must report the SAME excess at 16 that it reports on a tree's")
    out("solid trunk -- if it does not, the sweep is measuring the reader, not the bake.")
    out()
    out("| source half-width, viewport px | texels | err at 16 | err at 128 | crossing |")
    out("|---|---|---|---|---|")
    IW, VP = 120, 888          # the 0003a28b inner rect and the crop it is fed from
    for hw in (44.0, 44.5, 88.3, 200.7, 300.0):
        src = np.zeros((VP, VP), dtype=np.float64)
        x0, x1 = VP / 2 - hw, VP / 2 + hw
        for x in range(VP):
            cov = max(0.0, min(x + 1.0, x1) - max(float(x), x0))
            src[:, x] = 255.0 * min(1.0, cov)
        im = Image.fromarray(src.astype(np.uint8), "L").resize((IW, IW), Image.BOX)
        fa = np.array(im)
        ref = hw * IW / VP
        errs = {}
        for t in SWEEP:
            e = frame_extent(fa, t)
            errs[t] = (e[0] - ref) if e else None
        cross = None
        for p, q in zip(SWEEP, SWEEP[1:]):
            if errs[p] is None or errs[q] is None:
                continue
            if (errs[p] >= 0) != (errs[q] >= 0):
                cross = p + (q - p) * (errs[p] / (errs[p] - errs[q]))
                break
        out("| %.1f | %.3f | %+.2f | %+.2f | %s |"
            % (hw, ref, errs[16], errs[128], ("%.0f" % cross) if cross else "none"))
    out()

    # ------------------------------------------------------- 4. the floor control
    out("## E. Control 2, the EXTENT FLOOR: a card scaled 5% wide")
    out()
    out("Lane CARDORTHO B.5: the dx-extent column read the same value on the card arm")
    out("and its zeroed-offset control on 12 of 12 rows, so the 2% extent bar had")
    out("nothing under it. This is the floor: the same frame stretched to 105% of its")
    out("width. An extent check worth quoting must FAIL on it.")
    out()
    out("| tree | frame | true halfW texels (a>=128) | 5%-wide halfW | extent error | fails a 2% bar? |")
    out("|---|---|---|---|---|---|")
    for ident in TREES:
        c, a_png, _, _ = arms[ident]
        bw, _ = widest(a_png, c, READER_TEST)
        fa = S.frame_alpha(a_png, c, bw[0], bw[1])
        wide = np.array(Image.fromarray(fa, "L").resize(
            (int(round(fa.shape[1] * 1.05)), fa.shape[0]), Image.BILINEAR))
        e = frame_extent(wide, READER_TEST)
        err = abs(e[0] - bw[2]) / max(1e-9, bw[2])
        out("| %s | (%d,%d) | %.2f | %.2f | %.2f%% | %s |"
            % (ident, bw[0], bw[1], bw[2], e[0], 100 * err, "YES" if err > 0.02 else "NO -- floor does not fire"))
    out()

    open(os.path.join(OUT, "measure.md"), "w").write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
