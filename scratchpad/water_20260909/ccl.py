#!/usr/bin/env python3
"""ccl.py -- run-length connected components on a boolean grid, with an
optional per-texel KEY that must match for two texels to join.

There is no scipy on this machine (checked), so this is written out rather than
imported.  Row runs are found vectorised; the union-find runs over RUNS, not
texels, which is what keeps a 6144x6144 grid inside a couple of seconds.

Neighbourhood is 4-connected: horizontal inside a run, vertical between
overlapping runs on adjacent rows.  Two runs join only when their keys are
equal, so `key` = quantised water height gives water SURFACES and `key` = 0
gives water BODIES.

KNOWN-ANSWER CONTROL: see selftest() -- three hand-built grids whose component
count and shapes are known before the code runs (ww-control-calibration step 1).
"""

import numpy as np


class UF(object):
    def __init__(self, n):
        self.p = np.arange(n, dtype=np.int64)

    def find(self, a):
        p = self.p
        r = a
        while p[r] != r:
            r = p[r]
        while p[a] != r:
            p[a], a = r, p[a]
        return r

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            if ra < rb:
                self.p[rb] = ra
            else:
                self.p[ra] = rb


def runs_of_row(mask_row, key_row):
    """Start and end (exclusive) indices of maximal runs of True with equal key."""
    idx = np.flatnonzero(mask_row)
    if idx.size == 0:
        return np.empty(0, np.int64), np.empty(0, np.int64)
    brk = np.empty(idx.size, bool)
    brk[0] = True
    brk[1:] = (idx[1:] != idx[:-1] + 1) | (key_row[idx[1:]] != key_row[idx[:-1]])
    starts = idx[brk]
    ends = np.empty_like(starts)
    b = np.flatnonzero(brk)
    ends[:-1] = idx[b[1:] - 1] + 1
    ends[-1] = idx[-1] + 1
    return starts, ends


def label(mask, key=None):
    """(labels int32 grid, count).  Label 0 = background, components are 1..count."""
    H, W = mask.shape
    if key is None:
        key = np.zeros((H, W), np.int64)
    all_s, all_e, all_row, all_key = [], [], [], []
    row_slice = []
    n = 0
    for y in range(H):
        s, e = runs_of_row(mask[y], key[y])
        row_slice.append((n, n + s.size))
        n += s.size
        all_s.append(s)
        all_e.append(e)
        all_row.append(np.full(s.size, y, np.int64))
        all_key.append(key[y][s] if s.size else np.empty(0, key.dtype))
    S = np.concatenate(all_s) if n else np.empty(0, np.int64)
    E = np.concatenate(all_e) if n else np.empty(0, np.int64)
    K = np.concatenate(all_key) if n else np.empty(0, np.int64)
    uf = UF(max(n, 1))
    for y in range(1, H):
        a0, a1 = row_slice[y - 1]
        b0, b1 = row_slice[y]
        i, j = a0, b0
        while i < a1 and j < b1:
            if E[i] <= S[j]:
                i += 1
            elif E[j] <= S[i]:
                j += 1
            else:
                if K[i] == K[j]:
                    uf.union(i, j)
                if E[i] < E[j]:
                    i += 1
                else:
                    j += 1
    roots = np.array([uf.find(i) for i in range(n)], np.int64) if n else np.empty(0, np.int64)
    uniq, comp = np.unique(roots, return_inverse=True)
    labels = np.zeros((H, W), np.int32)
    for y in range(H):
        a0, a1 = row_slice[y]
        for i in range(a0, a1):
            labels[y, S[i]:E[i]] = comp[i] + 1
    return labels, int(uniq.size)


def selftest():
    """Known answers, computed by hand before the code ran."""
    ok = True
    # 1. two separate 2x2 blobs -> 2 components; a diagonal touch does NOT join
    g = np.zeros((6, 6), bool)
    g[0:2, 0:2] = True
    g[2:4, 2:4] = True          # touches the first only diagonally
    lab, n = label(g)
    ok &= (n == 2)
    print('control 1  two diagonally-touching blobs -> %d (expect 2)' % n)
    # 2. one U shape -> 1 component even though rows 0 and 2 are disjoint spans
    g = np.zeros((3, 3), bool)
    g[0, 0] = g[1, 0] = g[2, 0] = g[2, 1] = g[2, 2] = g[1, 2] = g[0, 2] = True
    lab, n = label(g)
    ok &= (n == 1)
    print('control 2  U shape -> %d (expect 1)' % n)
    # 3. the key splits a solid bar into 3
    g = np.ones((1, 9), bool)
    k = np.array([[0, 0, 0, 1, 1, 1, 2, 2, 2]], np.int64)
    lab, n = label(g, k)
    ok &= (n == 3)
    print('control 3  keyed bar -> %d (expect 3)' % n)
    # 4. and with no key it is 1
    lab, n = label(g)
    ok &= (n == 1)
    print('control 4  same bar unkeyed -> %d (expect 1)' % n)
    # 5. empty grid
    lab, n = label(np.zeros((4, 4), bool))
    ok &= (n == 0)
    print('control 5  empty -> %d (expect 0)' % n)
    print('ccl selftest', 'PASS' if ok else 'FAIL')
    return ok


if __name__ == '__main__':
    raise SystemExit(0 if selftest() else 1)
