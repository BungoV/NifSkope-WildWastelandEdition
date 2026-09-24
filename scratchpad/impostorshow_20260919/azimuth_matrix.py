#!/usr/bin/env python
"""Does the card TRACK the direction? A rank test, not a threshold.

Absolute silhouette IoU cannot answer that question on a Commonwealth tree and
the numbers say why: the trunk is in every frame at the same place, so a card
built from the WRONG frames still overlaps the mesh nearly as well as the right
one (measured 2026-09-19: honest 0.157 against shuffled 0.132). A floor placed
anywhere between those two would be measuring the trunk.

The rank test throws the common part away by construction. For every azimuth th
it compares the card photographed at th against the MESH photographed at all
eight azimuths, and asks one question: is the best match the mesh at th?

    hit@0   the best match is the same azimuth
    hit@1   the best match is the same azimuth or a neighbour (+-45 deg),
            which is the honest tolerance for a 4x4 grid whose frames sit
            between the cardinal directions
    mean rank  where the true azimuth lands in the sorted list, 0 = first

Chance for eight azimuths is hit@0 = 0.125, hit@1 = 0.375, mean rank 3.5, and a
run prints those beside its own numbers so nobody has to remember them.

    python azimuth_matrix.py <dir-of-d_az*.png> [block] [thr]
"""
import sys, os, glob, re
from PIL import Image
import numpy as np

CLEAR_TOL = 12


def mask_of(path, clear, k, t):
    a = np.asarray(Image.open(path).convert("RGB")).astype(np.int16)
    d = np.abs(a - np.array(clear, dtype=np.int16))
    m = ~np.all(d <= CLEAR_TOL, axis=2)
    if k <= 1:
        return m
    h, w = m.shape
    h2, w2 = (h // k) * k, (w // k) * k
    cov = m[:h2, :w2].astype(np.float32).reshape(h2 // k, k, w2 // k, k).mean(axis=(1, 3))
    return cov >= t


def iou(a, b):
    u = np.count_nonzero(a | b)
    return float(np.count_nonzero(a & b)) / u if u else -1.0


def main(d, k=1, t=0.05):
    cards = sorted(glob.glob(os.path.join(d, "*_card.png")))
    if not cards:
        print("no pictures in", d)
        return 2
    az = [int(re.search(r"_az(\d+)_card\.png$", c).group(1)) for c in cards]
    clear = tuple(np.asarray(Image.open(cards[0]).convert("RGB"))[2, 2].tolist())
    meshes = [c[: -len("_card.png")] + "_mesh.png" for c in cards]
    for m in meshes:
        if not os.path.exists(m):
            print("missing", m)
            return 2

    C = [mask_of(c, clear, k, t) for c in cards]
    M = [mask_of(m, clear, k, t) for m in meshes]
    n = len(C)

    print("azimuths: %s   block %d thr %.2f" % (az, k, t))
    print("rows = card azimuth, columns = mesh azimuth, * = best match in the row")
    hit0 = hit1 = 0
    ranks = []
    print("      " + "".join("%8d" % a for a in az))
    for i in range(n):
        row = [iou(C[i], M[j]) for j in range(n)]
        best = int(np.argmax(row))
        order = list(np.argsort(row)[::-1])
        rank = order.index(i)
        ranks.append(rank)
        if best == i:
            hit0 += 1
        if best in ((i - 1) % n, i, (i + 1) % n):
            hit1 += 1
        cells = "".join(("%7.3f%s" % (row[j], "*" if j == best else " ")) for j in range(n))
        print("%5d " % az[i] + cells + ("   rank %d" % rank))

    print("hit@0 %d/%d = %.3f   (chance %.3f)" % (hit0, n, hit0 / n, 1.0 / n))
    print("hit@1 %d/%d = %.3f   (chance %.3f)" % (hit1, n, hit1 / n, 3.0 / n))
    print("mean rank %.2f   (chance %.2f, 0 is best)" % (float(np.mean(ranks)), (n - 1) / 2.0))
    return 0


if __name__ == "__main__":
    a = sys.argv[1:]
    sys.exit(main(a[0] if a else ".", int(a[1]) if len(a) > 1 else 1,
                  float(a[2]) if len(a) > 2 else 0.05))
