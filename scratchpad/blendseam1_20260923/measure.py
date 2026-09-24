"""BLENDSEAM1: where the two colour writers disagree, and what each does to the CHUNK seam.

usage: python measure.py <root> <A> <B> [<A> <B> ...]
Each variant dir holds tex/Commonwealth.4.<x>.<y>.DDS for the four V9a chunks.
Prints per pair: texels that differ, max level, distance of every differing texel to
the chunk edge and to the nearest quadrant line; then per variant the chunk-seam
statistic across the two INTERNAL chunk boundaries (the 14-line interior seam of
pics.seam does not look at a chunk edge at all)."""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'blendedges1_20260923'))
import pics                                                         # noqa: E402

ROOT = sys.argv[1]
CH = [(-24, 24), (-20, 24), (-24, 28), (-20, 28)]


def load(v):
    return {c: pics.dds_rgb(os.path.join(ROOT, v, 'tex', 'Commonwealth.4.%d.%d.DDS' % c)).astype(np.int32)
            for c in CH}


def pair(a, b):
    A, B = load(a), load(b)
    tot = 0
    far = 0
    for c in CH:
        d = np.abs(A[c] - B[c]).max(axis=2)
        n = d.shape[0]
        ys, xs = np.nonzero(d)
        q = n // 8
        edge = np.minimum(np.minimum(xs, n - 1 - xs), np.minimum(ys, n - 1 - ys))
        qx = np.minimum(xs % q, q - 1 - (xs % q))
        qy = np.minimum(ys % q, q - 1 - (ys % q))
        ql = np.minimum(qx, qy)
        tot += len(xs)
        far += int((edge > 8).sum())
        print('  %s vs %s chunk %d.%d: %d of %d texels differ, max %d, edge-dist max %s, quad-line-dist max %s'
              % (a, b, c[0], c[1], len(xs), n * n, int(d.max()),
                 int(edge.max()) if len(xs) else '-', int(ql.max()) if len(xs) else '-'))
    print('PAIR %s vs %s: %d texels differ in all, %d farther than 8 px from a chunk edge' % (a, b, tot, far))


def mosaic(M, north_up):
    top = (-24, 28), (-20, 28)
    bot = (-24, 24), (-20, 24)
    if not north_up:
        top, bot = bot, top
    return np.vstack([np.hstack([M[top[0]], M[top[1]]]), np.hstack([M[bot[0]], M[bot[1]]])])


def chunk_seam(v):
    M = load(v)
    out = {}
    for nu in (True, False):
        L = pics.lum(mosaic(M, nu).astype(np.uint8))
        n = L.shape[0] // 2
        gx = np.abs(np.diff(L, axis=1))
        gy = np.abs(np.diff(L, axis=0))
        mx = np.delete(gx, n - 1, axis=1).mean()
        my = np.delete(gy, n - 1, axis=0).mean()
        out[nu] = (gx[:, n - 1].mean() / mx, gy[n - 1, :].mean() / my)
    print('SEAM %s chunk boundary step / mean step: north-up x %.3f y %.3f | south-up x %.3f y %.3f'
          % (v, out[True][0], out[True][1], out[False][0], out[False][1]))
    for c in CH:
        print('  interior seam %s %d.%d: %.3f' % (v, c[0], c[1], pics.seam(M[c].astype(np.uint8))[1]))


args = sys.argv[2:]
for i in range(0, len(args), 2):
    if args[i] == 'seam':
        chunk_seam(args[i + 1])
    else:
        pair(args[i], args[i + 1])
