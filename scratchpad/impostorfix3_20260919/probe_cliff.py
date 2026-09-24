"""Which invariant actually separates the card-plane fill from the 8-ring fill?

Candidate A (what fix03 wrote): the first 8 rings outside the silhouette must
lie in the band the frame's whole texels occupy. MEASURED BELOW AND IT DOES NOT
FAIL on the old sheets, because that band is the object's FULL depth range and
for an object centred on its own card the range straddles the plane.

Candidate B: CONTINUITY ACROSS THE SILHOUETTE. A texel that is uncovered but
8-adjacent to a covered one must carry a height within SLACK of the mean of its
covered neighbours. The 8-ring fill carries the object's own depth out to meet
it, so the field is continuous there; the card-plane fill puts a cliff at the
boundary exactly as tall as the object's local depth.

Prints both for every subject on both sheet sets. No repo file is written.
"""
import sys, os, glob, json
import numpy as np

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from impostor_bc_decode import load_dds

FULL, FLOOR, PLANE, SLACK, NEAR = 250, 16, 128, 12, 8
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix3_20260919/fixture'
TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')


def rings(mask, n):
    m = mask
    for _ in range(n):
        p = np.zeros((m.shape[0] + 2, m.shape[1] + 2), bool)
        p[1:-1, 1:-1] = m
        m = (p[:-2, :-2] | p[:-2, 1:-1] | p[:-2, 2:] |
             p[1:-1, :-2] | p[1:-1, 1:-1] | p[1:-1, 2:] |
             p[2:, :-2] | p[2:, 1:-1] | p[2:, 2:])
    return m


def nb_sum(a):
    p = np.zeros((a.shape[0] + 2, a.shape[1] + 2), a.dtype)
    p[1:-1, 1:-1] = a
    return (p[:-2, :-2] + p[:-2, 1:-1] + p[:-2, 2:] +
            p[1:-1, :-2] + p[1:-1, 2:] +
            p[2:, :-2] + p[2:, 1:-1] + p[2:, 2:])


def measure(lodm):
    raw = open(lodm, 'rb').read()
    j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
    N = int(j['oct'])
    d = os.path.dirname(os.path.abspath(lodm))
    stem = os.path.basename(lodm)[:-len('_oct.lodm')]
    alb = load_dds(glob.glob(os.path.join(d, stem + '_oct_d.DDS'))[0])[0]
    nsh = load_dds(glob.glob(os.path.join(d, stem + '_oct_n.DDS'))[0])[0]
    H, W = alb.shape[:2]
    fw, fh = W // N, H // N
    a = alb[..., 3] * 255.0
    h = nsh[..., 2] * 255.0
    fA = fB = 0
    wA = wB = 0.0
    nfr = 0
    for jj in range(N):
        for ii in range(N):
            ys, xs = slice(jj * fh, (jj + 1) * fh), slice(ii * fw, (ii + 1) * fw)
            af, hf = a[ys, xs], h[ys, xs]
            full = af >= FULL
            if full.sum() < 8:
                continue
            nfr += 1
            lo, hi = hf[full].min() - SLACK, hf[full].max() + SLACK
            uncov = af < FLOOR
            inked = af >= FLOOR
            # A: the band test
            near = uncov & rings(full, NEAR)
            offA = near & ((hf < lo) | (hf > hi))
            fracA = float(offA.sum()) / max(1, int(near.sum()))
            # B: continuity across the silhouette (first ring only)
            cnt = nb_sum(inked.astype(np.int32))
            first = uncov & (cnt > 0)
            mean = np.where(cnt > 0, nb_sum(np.where(inked, hf, 0.0)) / np.maximum(cnt, 1), 0.0)
            offB = first & (np.abs(hf - mean) > SLACK)
            fracB = float(offB.sum()) / max(1, int(first.sum()))
            if fracA > 0.10:
                fA += 1
            if fracB > 0.10:
                fB += 1
            wA = max(wA, fracA)
            wB = max(wB, fracB)
    return nfr, fA, wA, fB, wB


print('%-10s %-10s %s' % ('subject', 'sheets', 'frames  A:band fail/worst   B:continuity fail/worst'))
for t in TAGS:
    for which in ('cards_r3', 'cards'):
        l = glob.glob(os.path.join(ROOT, t, which, '*_oct.lodm'))
        if not l:
            print('%-10s %-10s MISSING' % (t, which)); continue
        nfr, fA, wA, fB, wB = measure(l[0])
        print('%-10s %-10s %3d      %3d / %5.1f%%          %3d / %5.1f%%'
              % (t, which, nfr, fA, 100 * wA, fB, 100 * wB))
