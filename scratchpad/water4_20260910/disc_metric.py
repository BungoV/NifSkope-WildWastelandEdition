# -*- coding: utf-8 -*-
"""disc_metric.py -- gate D1 / F5's instrument: the spatial structure of the
flow direction over one body, read OUT OF A FILE through the independent
decoder.

    python disc_metric.py <body-id> <file.lodl> [more.lodl ...]

Per file it prints:
  * wet texels, distinct 8-bit directions;
  * adjacent pairs (4-connected, both wet): the 50/90/99th percentile of the
    angle difference and the SEAM fraction (pairs differing by > 10 deg);
  * PATCHES: 4-connected components of texels sharing ONE direction word, of
    >= 64 texels, whose boundary with other wet texels is mostly seam (> 50 %
    of the boundary pairs differ by > 5 deg).  For each: size, equivalent
    radius sqrt(A/pi) in texels, mean seam jump.
The patch definition is the pre-registered one (lane report section 0).
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from lodl_np import body_region      # noqa: E402


def angdiff(a, b):
    """|a - b| on the 256-step circle, in degrees."""
    d = np.abs(a.astype(np.int32) - b.astype(np.int32))
    d = np.minimum(d, 256 - d)
    return d * (360.0 / 256.0)


def label4(mask):
    """4-connected labelling of a boolean mask (two-pass union-find on runs;
    no scipy on this machine)."""
    h, w = mask.shape
    lab = np.zeros((h, w), dtype=np.int32)
    parent = [0]
    nxt = 1
    for y in range(h):
        row = mask[y]
        x = 0
        while x < w:
            if not row[x]:
                x += 1
                continue
            x1 = x
            while x1 < w and row[x1]:
                x1 += 1
            # a run [x, x1)
            l = 0
            if y:
                above = lab[y - 1, x:x1]
                for v in above[above > 0]:
                    r = int(v)
                    while parent[r] != r:
                        r = parent[r]
                    if l == 0:
                        l = r
                    elif r != l:
                        if r < l:
                            parent[l] = r
                            l = r
                        else:
                            parent[r] = l
            if l == 0:
                parent.append(nxt)
                l = nxt
                nxt += 1
            lab[y, x:x1] = l
            x = x1
    # flatten
    par = np.array(parent, dtype=np.int32)
    for i in range(1, len(par)):
        r = i
        while par[r] != r:
            r = par[r]
        par[i] = r
    lab = par[lab]
    # compact
    u, inv = np.unique(lab, return_inverse=True)
    return inv.reshape(h, w), len(u) - 1


def measure(path, body):
    d, b, ids, flow, (px0, py0) = body_region(path, body)
    wet = ids == body
    dirw = (flow & 0xFF).astype(np.int32)
    n = int(wet.sum())
    distinct = len(np.unique(dirw[wet]))
    # adjacent wet pairs, horizontal and vertical
    hp = wet[:, :-1] & wet[:, 1:]
    vp = wet[:-1, :] & wet[1:, :]
    dh = angdiff(dirw[:, :-1], dirw[:, 1:])[hp]
    dv = angdiff(dirw[:-1, :], dirw[1:, :])[vp]
    dd = np.concatenate([dh, dv])
    pairs = len(dd)
    p50, p90, p99 = (np.percentile(dd, q) for q in (50, 90, 99)) if pairs else (0, 0, 0)
    seam = float((dd > 10.0).sum()) / pairs if pairs else 0.0
    # patches: components of identical direction inside wet
    lab, nlab = label4(wet)
    # split each wet component by direction: label on (wet & same direction)
    # by labelling the pair-equality graph: two adjacent wet texels are joined
    # when their direction words are equal
    key = np.where(wet, dirw, -1)
    # label components of equal-key: do it per distinct direction value
    patches = []
    for v in np.unique(dirw[wet]):
        m = wet & (dirw == v)
        if m.sum() < 64:
            continue
        l, k = label4(m)
        for c in range(1, k + 1):
            comp = l == c
            a = int(comp.sum())
            if a < 64:
                continue
            # boundary pairs: comp texel adjacent to a wet non-comp texel
            jumps = []
            # east neighbour
            sel = comp[:, :-1] & ~comp[:, 1:] & wet[:, 1:]
            jumps.append(angdiff(dirw[:, :-1], dirw[:, 1:])[sel])
            # west neighbour
            sel = comp[:, 1:] & ~comp[:, :-1] & wet[:, :-1]
            jumps.append(angdiff(dirw[:, 1:], dirw[:, :-1])[sel])
            # north neighbour
            sel = comp[:-1, :] & ~comp[1:, :] & wet[1:, :]
            jumps.append(angdiff(dirw[:-1, :], dirw[1:, :])[sel])
            # south neighbour
            sel = comp[1:, :] & ~comp[:-1, :] & wet[:-1, :]
            jumps.append(angdiff(dirw[1:, :], dirw[:-1, :])[sel])
            j = np.concatenate(jumps)
            if len(j) == 0:
                continue
            seamy = float((j > 5.0).sum()) / len(j)
            if seamy > 0.5:
                patches.append((a, math.sqrt(a / math.pi), float(j.mean()), int(v)))
    patches.sort(reverse=True)
    return dict(path=os.path.basename(path), body=body, wet=n, distinct=distinct,
                pairs=pairs, p50=p50, p90=p90, p99=p99, seam=seam, patches=patches)


if __name__ == '__main__':
    body = int(sys.argv[1])
    for p in sys.argv[2:]:
        r = measure(p, body)
        print('%s body %d: %d wet texels, %d distinct directions, %d adjacent pairs'
              % (r['path'], r['body'], r['wet'], r['distinct'], r['pairs']))
        print('  adjacent angle difference p50 %.2f  p90 %.2f  p99 %.2f deg; seam fraction '
              '(> 10 deg) %.3f%%' % (r['p50'], r['p90'], r['p99'], r['seam'] * 100.0))
        print('  seam-bounded constant-direction patches (>= 64 texels): %d' % len(r['patches']))
        for (a, rad, jump, v) in r['patches'][:40]:
            print('    %6d texels  r_eq %5.1f texels  mean seam jump %5.1f deg  dir %3d (%.1f deg)'
                  % (a, rad, jump, v, v * 360.0 / 256.0))
        if len(r['patches']) > 40:
            print('    ... %d more' % (len(r['patches']) - 40))
        if r['patches']:
            rads = [q[1] for q in r['patches']]
            print('  patch r_eq: median %.1f, max %.1f texels (stroke half-width = 16 texels)'
                  % (float(np.median(rads)), max(rads)))
