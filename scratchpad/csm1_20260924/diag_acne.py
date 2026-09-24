"""acne_16: which faces carry the darkened lit px; acne_08: where is the lit strip px"""
import sys, os, math
import numpy as np
from PIL import Image
sys.path.insert(0, r"E:\Projects\NifskopeWildWastelandEdition\tests\spells")
import pbr_csm1_gates as J

OUT = r"E:\Projects\NifskopeWildWastelandEdition\scratchpad\csm1_20260924\g4"
for h in (8.0, 16.0):
    tag = "acne_%02d" % int(h)
    e = J.parse_echo(os.path.join(OUT, tag + ".csm.txt"))
    p4 = np.asarray(Image.open(os.path.join(OUT, tag + ".png")).convert("RGB")).astype(float) / 255
    cam = e["camo"]
    H, W = p4.shape[:2]
    L = -J.sun_to(h)
    sun = -L
    d, G, ground, cube, P, axis = J.scene_pixels(cam, W, H)
    meas = p4[..., 0]
    inner = cube & ~J.dilate(~cube, 2)
    for ax in range(3):
        for sg in (-1, 1):
            n = np.zeros(3)
            n[ax] = sg
            on = inner & (axis == ax) & (np.sign(P[..., ax]) == sg)
            oth = [k for k in range(3) if k != ax]
            on &= (np.abs(P[..., oth[0]]) < J.HALF - 8) & (np.abs(P[..., oth[1]]) < J.HALF - 8)
            if on.sum() == 0:
                continue
            dark = on & (meas < 0.9)
            if n @ sun > 0.1 or dark.sum():
                q = P[dark]
                extra = ""
                if dark.sum():
                    extra = " dark coords %s..%s  meas min %.3f" % (np.round(q.min(0)), np.round(q.max(0)), meas[dark].min())
                print("%s face %s%s n.sun %.3f: %d px, %d darkened%s" % (tag, "+-"[sg < 0], "xyz"[ax], n @ sun, on.sum(), dark.sum(), extra))
    # the strip
    strip = np.zeros((H, W), bool)
    for ax in (0, 1):
        for sg in (-1, 1):
            n = np.zeros(3)
            n[ax] = sg
            if n @ sun >= 0:
                continue
            oth = 1 - ax
            strip |= ground & (sg * G[..., ax] > J.HALF) & (sg * G[..., ax] < J.HALF + 6) & (np.abs(G[..., oth]) < J.HALF - 20)
    lit = strip & (meas > 0.1)
    ys, xs = np.nonzero(lit)
    for y, x in zip(ys[:3], xs[:3]):
        print("%s strip lit px (%d,%d) G=%s meas %.3f; cube within %d px: %s" % (
            tag, x, y, np.round(G[y, x], 1), meas[y, x], 4, cube[max(y - 4, 0):y + 5, max(x - 4, 0):x + 5].any()))
