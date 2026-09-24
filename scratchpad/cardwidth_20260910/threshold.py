#!/usr/bin/env python
"""THE THRESHOLD QUESTION, decided inside the SOURCE render alone.

measure.py compared the sheet's coverage at 16/255 against `framefit`, which the
bake also measured at 16/255 -- fine for the DOWNSAMPLE (does the crop land where
it should) and circular for the THRESHOLD (is 16 the right number). This decides
the threshold with no card and no bake in it at all.

The six unclipped source renders of the 2026-09-10 run (`*_ring_src.png`;
recon.md: every `mid` and every `ortho` render is clipped by the viewport) give a
FINE silhouette at 941 px. Box-filter that binary mask down to the card's own
texel pitch -- which is exactly what the bake's `scaled( iw, ih,
SmoothTransformation )` computes, a coverage fraction per texel -- and read the
silhouette back at each candidate threshold:

  conservative   { fraction >= 16/255 }   the bake's coverage floor
  majority       { fraction >= 128/255 }  the reader's own alpha test, 0.5

The error against the FINE mask's own bbox is then the threshold's error and
nothing else's. The same downsample also answers the 18-121% width excess of lane
CARDORTHO's trunk table: the per-row widths of the coarse mask against the fine
one's, with no card in the comparison, is the inflation RESOLUTION alone buys.

FLOORS. (1) A synthetic disc of known radius through the same downsample: the
metric must return the disc's radius, and must show the same +-1 texel
quantisation. (2) The fine mask downsampled by 1 (pitch 1.0) must return zero
error at every threshold, or the pipeline itself is adding the error.
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


def coarse(mask, pitch_x, pitch_y):
    """The binary mask box-filtered to a grid of `pitch` fine pixels per texel.

    Returns a float array of coverage FRACTIONS -- the same quantity the bake's
    downsample writes into the sheet's alpha.
    """
    H, W = mask.shape
    nx, ny = max(1, int(round(W / pitch_x))), max(1, int(round(H / pitch_y)))
    im = Image.fromarray((mask * 255).astype(np.uint8), "L").resize((nx, ny), Image.BOX)
    return np.array(im, dtype=np.float64) / 255.0, (float(W) / nx, float(H) / ny)


def half_extents(m):
    bb = S.bbox(m)
    if not bb:
        return None
    return 0.5 * (bb[2] - bb[0]), 0.5 * (bb[3] - bb[1])


def main():
    lines = []

    def out(s=""):
        print(s)
        lines.append(s)

    out("# Lane CARDWIDTH -- the threshold, decided inside the source render")
    out()

    out("## J. Conservative (16/255) against majority (0.5) -- the source only")
    out()
    out("The fine mask is the 941-px render's own silhouette. `pitch` is the card's texel")
    out("in fine pixels. The errors are HALF-extents in card texels: positive = the coarse")
    out("silhouette is wider than the source's.")
    out()
    out("| tree | view | pitch px | fine halfW px | ref texels | err x @16 | err y @16 | err x @128 | err y @128 |")
    out("|---|---|---|---|---|---|---|---|---|")
    rows = []
    for ident in TREES:
        c = S.card_of(CARDS, ident)
        tex_x = 2.0 * c["halfW"] / c["tw"]
        tex_y = 2.0 * c["halfH"] / c["th"]
        for view in ("front", "right"):
            png = os.path.join(RUN, "%s_%s_ring_src.png" % (ident, view))
            cam = S.camera(os.path.join(RUN, "%s_%s_ring_src.camera" % (ident, view)))
            upp = float(cam["upp"])
            m = S.mesh_mask(png)
            assert not S.touches_edge(m), png
            fw, fh = half_extents(m)
            px, py = tex_x / upp, tex_y / upp
            cf, got = coarse(m, px, py)
            refx, refy = fw / px, fh / py
            e16 = half_extents(cf >= 16 / 255.0)
            e128 = half_extents(cf >= 128 / 255.0)
            out("| %s | %s | %.2f | %.1f | %.2f | %+.2f | %+.2f | %+.2f | %+.2f |"
                % (ident, view, px, fw, refx,
                   e16[0] - refx, e16[1] - refy, e128[0] - refx, e128[1] - refy))
            rows.append((ident, view, px, refx, refy, e16, e128, m, cf))
    out()
    e16x = [abs(r[5][0] - r[3]) for r in rows] + [abs(r[5][1] - r[4]) for r in rows]
    e128x = [abs(r[6][0] - r[3]) for r in rows] + [abs(r[6][1] - r[4]) for r in rows]
    out("**Worst absolute error over the twelve half-extents: 16/255 = %.2f texels, "
        "0.5 = %.2f texels.**" % (max(e16x), max(e128x)))
    out()

    out("## K. The 18-121%% width excess: what RESOLUTION alone buys, no card in it")
    out()
    out("Lane CARDORTHO B.3's trunk table read the widest row of the bottom fifth and of")
    out("the top fifth of the card against the same of the source, and got 18% to 121%.")
    out("Here the SAME statistic is taken between the fine source mask and the same mask")
    out("box-filtered to the card's texel pitch and thresholded -- the card is not in the")
    out("comparison, so whatever this shows is not the bake's.")
    out()
    out("| tree | view | band | fine widest row, px | coarse @16, px | excess | coarse @128, px | excess |")
    out("|---|---|---|---|---|---|---|---|")
    for (ident, view, px, refx, refy, e16, e128, m, cf) in rows:
        bb = S.bbox(m)
        fine_rows = m[bb[1]:bb[3], :].sum(axis=1)
        for name, sl in (("bottom fifth", slice(int(0.8 * len(fine_rows)), None)),
                         ("top fifth", slice(0, int(0.2 * len(fine_rows))))):
            fine_w = float(fine_rows[sl].max())
            for thr, tag in ((16, "@16"), (128, "@128")):
                pass
            outs = []
            for thr in (16, 128):
                cm = cf >= thr / 255.0
                cb = S.bbox(cm)
                crows = cm[cb[1]:cb[3], :].sum(axis=1) * px
                n = len(crows)
                s2 = slice(int(0.8 * n), None) if name == "bottom fifth" else slice(0, int(0.2 * n))
                cw = float(crows[s2].max())
                outs.append((cw, (cw - fine_w) / max(1.0, fine_w)))
            out("| %s | %s | %s | %.0f | %.0f | %+.0f%% | %.0f | %+.0f%% |"
                % (ident, view, name, fine_w, outs[0][0], 100 * outs[0][1],
                   outs[1][0], 100 * outs[1][1]))
    out()

    out("## L. Floor 1: pitch 1.0 -- the pipeline must add nothing")
    out()
    out("| tree | view | err x @16 | err y @16 | err x @128 | err y @128 |")
    out("|---|---|---|---|---|---|")
    for (ident, view, px, refx, refy, _e16, _e128, m, _cf) in rows:
        cf1, _ = coarse(m, 1.0, 1.0)
        f = half_extents(m)
        a = half_extents(cf1 >= 16 / 255.0)
        b = half_extents(cf1 >= 128 / 255.0)
        out("| %s | %s | %+.2f | %+.2f | %+.2f | %+.2f |"
            % (ident, view, a[0] - f[0], a[1] - f[1], b[0] - f[0], b[1] - f[1]))
    out()

    out("## M. Floor 2: a disc of known radius through the same downsample")
    out()
    out("| radius px | pitch | ref texels | halfW @16 | err | halfW @128 | err |")
    out("|---|---|---|---|---|---|---|")
    N = 900
    yy, xx = np.mgrid[0:N, 0:N]
    for r in (60.0, 137.5, 300.0):
        for pitch in (2.0, 7.4):
            d = np.hypot(xx - (N - 1) / 2.0, yy - (N - 1) / 2.0)
            m = d <= r
            cf, _ = coarse(m, pitch, pitch)
            ref = r / pitch
            a = half_extents(cf >= 16 / 255.0)
            b = half_extents(cf >= 128 / 255.0)
            out("| %.1f | %.1f | %.2f | %.2f | %+.2f | %.2f | %+.2f |"
                % (r, pitch, ref, a[0], a[0] - ref, b[0], b[0] - ref))
    out()

    open(os.path.join(OUT, "threshold.md"), "w").write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
