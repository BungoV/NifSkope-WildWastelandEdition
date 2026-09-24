#!/usr/bin/env python
"""Lane NATIVEVIEW2's gates (a)-(d), computed off the two shot directories.

  python gates.py <rungdir> <newdir>

Nothing here is a floor by itself: every number is printed for BOTH arms, so a
gate reads as "the rung says X, the new exe says Y" and the rung is the refuter.

Luma is Rec.601 0.299R + 0.587G + 0.114B on the 8-bit frame.
Background is the WW_RENDER_CLEAN clear colour (43,45,49); a TOP frame of this
chunk has none, and covers the whole frame.

"block SD" is the standard deviation of 32x32 BLOCK MEANS over the covered
mask.  It exists because plain luma SD over terrain is dominated by the colour
sheet's own detail, which no lighting change can remove: averaging 1024 texels
a block flattens the albedo and leaves the large-scale shading, which is what a
blotch is.  A block is used only when every one of its 1024 pixels is covered.
"""
import os
import sys

import numpy as np
from PIL import Image

BG = np.array([43, 45, 49], np.int16)
DARK = 40.0
BLK = 32


def load(d, n):
    a = np.array(Image.open(os.path.join(d, n + ".png")).convert("RGB")).astype(np.int16)
    cov = (np.abs(a - BG).sum(axis=2) > 0)
    Y = 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]
    return a, cov, Y.astype(np.float64)


def blocks(Y, cov):
    h, w = Y.shape
    bh, bw = h // BLK, w // BLK
    ys = Y[:bh * BLK, :bw * BLK].reshape(bh, BLK, bw, BLK)
    cs = cov[:bh * BLK, :bw * BLK].reshape(bh, BLK, bw, BLK)
    full = cs.all(axis=(1, 3))
    means = ys.mean(axis=(1, 3))
    return means[full], int(full.sum())


def stat(d, n):
    a, cov, Y = load(d, n)
    y = Y[cov]
    bm, nb = blocks(Y, cov)
    return dict(shape=(a.shape[1], a.shape[0]), cov=int(cov.sum()),
                mean=y.mean(), sd=y.std(), dark=100.0 * (y < DARK).mean(),
                bsd=(bm.std() if nb else float("nan")), nb=nb)


def line(tag, s):
    return ("%-26s %5dx%-4d cov %7d  mean %6.2f  sd %6.2f  blockSD %5.2f (%d blocks)"
            "  dark<40 %6.2f%%" % (tag, s["shape"][0], s["shape"][1], s["cov"],
                                   s["mean"], s["sd"], s["bsd"], s["nb"], s["dark"]))


def iou(d1, n1, d2, n2):
    _, c1, Y1 = load(d1, n1)
    _, c2, Y2 = load(d2, n2)
    m = c1 & c2
    a = (Y1 < DARK) & m
    b = (Y2 < DARK) & m
    u = int((a | b).sum())
    return (int((a & b).sum()) / u if u else float("nan")), int(m.sum())


def main():
    rung, new = sys.argv[1], sys.argv[2]
    arms = ["t_own_top", "t_own_obl", "t_flat_top", "t_flat_obl",
            "t_tilt_top", "t_tilt_obl", "t_tiltw_top", "t_tiltw_obl",
            "i_native_both_top", "i_native_both_obl",
            "legacy_btr_top", "legacy_btr_obl", "legacy_bto_top", "legacy_bto_obl"]

    print("=" * 100)
    print("FRAME STATISTICS        (R = rung / before,  N = new exe / after)")
    print("=" * 100)
    S = {}
    for a in arms:
        for tag, d in (("R", rung), ("N", new)):
            p = os.path.join(d, a + ".png")
            if not os.path.exists(p):
                continue
            S[(tag, a)] = stat(d, a)
            print(" %s %s" % (tag, line(a, S[(tag, a)])))
        print()

    # ---------------------------------------------------------------- gate (a)
    print("=" * 100)
    print("GATE (a)  every shape WITHOUT Shader Flags 1 bit 12 renders byte-identical")
    print("=" * 100)
    same = diff = 0
    for a in arms:
        pr = os.path.join(rung, a + ".png")
        pn = os.path.join(new, a + ".png")
        if not (os.path.exists(pr) and os.path.exists(pn)):
            continue
        eq = open(pr, "rb").read() == open(pn, "rb").read()
        same += eq
        diff += (not eq)
        print("  %-24s %s" % (a, "IDENTICAL" if eq else "differs"))
    print("  -> %d identical, %d differ" % (same, diff))

    # ---------------------------------------------------------------- gate (b)
    print("=" * 100)
    print("GATE (b)  own tiles vs FLAT tiles must now DIFFER; flat must be lit evenly")
    print("=" * 100)
    for tag, d in (("R", rung), ("N", new)):
        v, n = iou(d, "t_own_obl", d, "t_flat_obl")
        _, c1, Y1 = load(d, "t_own_obl")
        _, c2, Y2 = load(d, "t_flat_obl")
        m = c1 & c2
        dY = np.abs(Y1 - Y2)[m]
        # threshold-free companion to the dark-mask IoU, which becomes
        # undefined once neither arm has any pixel under luma 40: the darkest
        # FIFTH of each arm, which always exists and is always the same size.
        q1 = np.quantile(Y1[m], 0.20)
        q2 = np.quantile(Y2[m], 0.20)
        a = (Y1 <= q1) & m
        b = (Y2 <= q2) & m
        q = int((a & b).sum()) / max(1, int((a | b).sum()))
        print("  %s oblique own vs flat: dark-mask IoU %.3f over %d px, "
              "mean|dY| %.2f, |dY|>8 on %.2f%%, darkest-fifth IoU %.3f"
              % (tag, v, n, dY.mean(), 100.0 * (dY > 8).mean(), q))
        # is the own-vs-flat difference STRUCTURE or noise?  A real slope
        # signal survives averaging over 32x32 blocks; uncorrelated noise does
        # not.  Printed as the block SD of the signed difference image.
        D = (Y1 - Y2)
        dbm, dnb = blocks(D, m)
        print("     own-minus-flat difference: pixel SD %.2f, blockSD %.2f over %d blocks"
              % (D[m].std(), dbm.std(), dnb))
        print("     flat-tile oblique blockSD %.2f   (own %.2f)   FLOOR: the same"
              " flat tiles at the TOP view, where N.L is provably the same at every"
              " texel, give blockSD %.2f"
              % (S[(tag, "t_flat_obl")]["bsd"], S[(tag, "t_own_obl")]["bsd"],
                 S[(tag, "t_flat_top")]["bsd"]))

    # ---------------------------------------------------------------- gate (c)
    print("=" * 100)
    print("GATE (c)  dark fraction (luma < 40) of the native oblique terrain")
    print("=" * 100)
    for a in ("t_own_obl", "i_native_both_obl", "legacy_btr_obl", "legacy_btr_top",
              "legacy_bto_obl"):
        print("  %-22s  before %6.2f%%   after %6.2f%%   (%+.2f)"
              % (a, S[("R", a)]["dark"], S[("N", a)]["dark"],
                 S[("N", a)]["dark"] - S[("R", a)]["dark"]))

    # ---------------------------------------------------------------- gate (d)
    print("=" * 100)
    print("GATE (d)  the slope test with a known answer")
    print("=" * 100)
    # the light, from the shader's own inputs
    import math
    rx, ry, rz = math.radians(-63.5593), 0.0, math.radians(133.3081)
    sx, cx, sy, cy, sz, cz = (math.sin(rx), math.cos(rx), math.sin(ry),
                              math.cos(ry), math.sin(rz), math.cos(rz))
    Lw_obl = (sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy)
    Lw_top = (0.0, 0.0, 1.0)
    N = {"flat": (0.0353, 0.0353, 0.9988),
         "tilt": (0.4827, 0.0353, 0.8751),
         "tiltw": (-0.4827, 0.0353, 0.8751)}
    print("  headlight, view space (0,0,1); the census read it back as 0 0 1")
    print("  top view    rotation (0,0,0)             -> light in world axes "
          "(%+.4f, %+.4f, %+.4f)" % Lw_top)
    print("  oblique     rotation (-63.5593,0,133.3081) -> light in world axes "
          "(%+.4f, %+.4f, %+.4f)" % Lw_obl)
    for k, n in N.items():
        dt = sum(a * b for a, b in zip(n, Lw_top))
        do = sum(a * b for a, b in zip(n, Lw_obl))
        print("    %-6s normal (E %+.4f, N %+.4f, U %.4f)  N.L top %.4f  oblique %.4f"
              % (k, n[0], n[1], n[2], dt, do))

    print("  d1 TOP CONTROL: east tilt and west tilt have the SAME N.L, so the two")
    print("     frames must be the same picture.")
    for tag, d in (("R", rung), ("N", new)):
        p1 = os.path.join(d, "t_tilt_top.png")
        p2 = os.path.join(d, "t_tiltw_top.png")
        if not (os.path.exists(p1) and os.path.exists(p2)):
            continue
        eq = open(p1, "rb").read() == open(p2, "rb").read()
        _, c1, Y1 = load(d, "t_tilt_top")
        _, c2, Y2 = load(d, "t_tiltw_top")
        m = c1 & c2
        dY = np.abs(Y1 - Y2)[m]
        print("     %s byte-identical: %-5s   max|dY| %.2f  mean|dY| %.4f"
              % (tag, str(eq), dY.max(), dY.mean()))

    print("  d2 OBLIQUE ORDER: N.L is west %.4f > flat %.4f > east %.4f, and"
          % tuple(sum(a * b for a, b in zip(N[k], Lw_obl))
                  for k in ("tiltw", "flat", "tilt")))
    print("     nothing else in the pixel changes, so per-pixel luma must order the")
    print("     same way under any monotone tone map.")
    for tag, d in (("R", rung), ("N", new)):
        need = ["t_tiltw_obl", "t_flat_obl", "t_tilt_obl"]
        if not all(os.path.exists(os.path.join(d, x + ".png")) for x in need):
            continue
        _, cw, Yw = load(d, "t_tiltw_obl")
        _, cf, Yf = load(d, "t_flat_obl")
        _, ce, Ye = load(d, "t_tilt_obl")
        m = cw & cf & ce
        ok = ((Yw > Yf) & (Yf > Ye))[m]
        print("     %s west>flat>east on %6.2f%% of %d covered px   "
              "(means west %.2f flat %.2f east %.2f)"
              % (tag, 100.0 * ok.mean(), int(m.sum()),
                 Yw[m].mean(), Yf[m].mean(), Ye[m].mean()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
