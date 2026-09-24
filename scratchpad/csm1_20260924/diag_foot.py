"""where are fp1/fp2's hard-tap misses? one verdict line per frame"""
import sys, os
import numpy as np
from PIL import Image
sys.path.insert(0, r"E:\Projects\NifskopeWildWastelandEdition\tests\spells")
import pbr_csm1_gates as J

OUT = r"E:\Projects\NifskopeWildWastelandEdition\scratchpad\csm1_20260924\g2"
for fr in ("fp1", "fp2"):
    e = J.parse_echo(os.path.join(OUT, fr + "_p1.csm.txt"))
    p1 = np.asarray(Image.open(os.path.join(OUT, fr + "_p1.png")).convert("RGB")).astype(float) / 255
    cam = e["camo"]
    H, W = p1.shape[:2]
    L = -J.sun_to(12.0)
    right, up = J.basis(L)
    fits = J.fit(L, cam, e["Dn"], e["mapn"])
    hull = J.cube_hull(right, up)
    d, G, ground, cube, P, axis = J.scene_pixels(cam, W, H)
    dv = (G - cam.C) @ cam.fwd
    A, Bc, T = J.select(dv)
    ci = np.where(T < 0.5, A, Bc)[ground]
    c = fits[int(np.bincount(ci).argmax())]
    mh = np.ones((H, W))
    mh[ground] = J.hard(G[ground], c, hull, right, up)
    meas = p1[..., 0]
    edge = ground & (mh > 0.02) & (mh < 0.98)
    bad = (np.abs(meas - mh) > 0.2) & edge
    # distance of each bad ground point to the cube's base square (|x|,|y| <= HALF), in texels
    gx, gy = G[..., 0], G[..., 1]
    dx = np.maximum(np.abs(gx) - J.HALF, 0)
    dy = np.maximum(np.abs(gy) - J.HALF, 0)
    db = np.hypot(dx, dy) / c["tex"]
    ov = (np.clip(meas, 0, 1)[..., None] * np.array([1, 1, 1]) * 255).astype(np.uint8)
    ov[bad] = (255, 0, 0)
    ov[cube] = (ov[cube] * 0.5 + np.array([0, 0, 120])).astype(np.uint8)
    ys, xs = np.nonzero(bad)
    if ys.size:
        y0, y1 = max(ys.min() - 150, 0), min(ys.max() + 150, H)
        x0, x1 = max(xs.min() - 150, 0), min(xs.max() + 150, W)
        Image.fromarray(ov[y0:y1, x0:x1]).save(os.path.join(os.path.dirname(OUT), "diag_%s.png" % fr))
    b = db[bad]
    sgn = (meas - mh)[bad]
    print("%s tex %g: %d bad; base-dist texels min %.1f med %.1f max %.1f; meas brighter in %d; bad beyond 8 tex %d; bad side x>0 %d"
          % (fr, c["tex"], bad.sum(), b.min() if b.size else -1, np.median(b) if b.size else -1,
             b.max() if b.size else -1, (sgn > 0).sum(), (b > 8).sum(), (gx[bad] > 0).sum()))
