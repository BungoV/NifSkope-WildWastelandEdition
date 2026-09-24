#!/usr/bin/env python
"""Choose the silhouette metric BY MEASUREMENT, offline, on pictures already taken.

The gate's step 5/6/7 numbers collapsed on TreeMapleForest2 (IoU 0.16, red
control margin 0.019). The pictures say why: the mesh is a LEAFLESS tree whose
silhouette is a few thousand one-pixel twigs, and the card is the filled canopy
those twigs average to. A pixel-exact IoU between the two is small however
correct the card is, and -- the part that actually matters -- it is small for the
WRONG card as well, so it cannot discriminate.

This sweeps candidate metrics over the azimuth pictures (card / mesh from the
same direction / mesh from the opposite direction) and reports, for each:

    same     mean IoU card-vs-mesh from the direction the card was chosen for
    opposite mean IoU card-vs-mesh from the opposite direction
    margin   same - opposite, which is what a red control has to survive

A metric is only worth a floor if its margin is large. No metric is chosen here;
the numbers are printed and the choice is made against them.

    python metric_sweep.py <dir-of-d_az*.png>
"""
import sys, os, glob
from PIL import Image
import numpy as np

CLEAR_TOL = 12


def mask_of(path, clear):
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.int16)
    d = np.abs(a - np.array(clear, dtype=np.int16))
    return ~np.all(d <= CLEAR_TOL, axis=2)


def iou(a, b):
    u = np.count_nonzero(a | b)
    return float(np.count_nonzero(a & b)) / u if u else -1.0


def block_cov(m, k):
    """Mean coverage over k x k blocks -- what an area-averaged distant render
    of the same silhouette would have in each block."""
    h, w = m.shape
    h2, w2 = (h // k) * k, (w // k) * k
    return m[:h2, :w2].astype(np.float32).reshape(h2 // k, k, w2 // k, k).mean(axis=(1, 3))


def main(d):
    cards = sorted(glob.glob(os.path.join(d, "*_card.png")))
    if not cards:
        print("no pictures in", d)
        return 2
    # the clear colour is whatever the corner of the mesh picture is
    clear = tuple(np.asarray(Image.open(cards[0]).convert("RGB"))[2, 2].tolist())
    print("clear colour read from the picture corner:", clear)
    print("pictures:", len(cards))

    rows = []
    for k in (1, 2, 4, 8, 16):
        for t in (0.02, 0.05, 0.10, 0.20, 0.35):
            if k == 1 and t != 0.02:
                continue                      # k=1 has no threshold to vary
            same, opp = [], []
            for c in cards:
                stem = c[: -len("_card.png")]
                mc = mask_of(c, clear)
                ms = mask_of(stem + "_mesh.png", clear)
                mo = mask_of(stem + "_mesh_opposite.png", clear)
                if k == 1:
                    a, b, o = mc, ms, mo
                else:
                    a = block_cov(mc, k) >= t
                    b = block_cov(ms, k) >= t
                    o = block_cov(mo, k) >= t
                same.append(iou(a, b))
                opp.append(iou(a, o))
            rows.append((k, t, float(np.mean(same)), float(np.mean(opp)),
                         float(np.min(same))))

    print("%6s %6s %8s %8s %8s %8s" % ("block", "thr", "same", "opposite", "margin", "worst"))
    for k, t, s, o, w in rows:
        print("%6d %6.2f %8.4f %8.4f %8.4f %8.4f" % (k, t, s, o, s - o, w))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
