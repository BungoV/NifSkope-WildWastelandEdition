"""Does the 8-ring fill REACH outside the silhouette at all?

The number for gate row 14b's floor: the fraction of the uncovered texels
within 8 rings of a whole texel whose height is more than SLACK off the card
plane. On exe af457755's sheets that fraction is ~0 by construction (its law
was "card plane everywhere outside"); on this lane's it should be most of the
band. Printed per subject for both sheet sets, worst frame and mean frame, so
the floor written into the gate is a measurement and not a guess.
"""
import sys, os, glob, json
import numpy as np

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from impostor_bc_decode import load_dds

FULL, FLOOR, PLANE, SLACK, NEAR = 250, 16, 128, 12, 8
M = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix3_20260919'
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
    fr = []
    for jj in range(N):
        for ii in range(N):
            ys, xs = slice(jj * fh, (jj + 1) * fh), slice(ii * fw, (ii + 1) * fw)
            af, hf = a[ys, xs], h[ys, xs]
            full = af >= FULL
            if full.sum() < 8:
                continue
            band = (af < FLOOR) & rings(full, NEAR)
            if band.sum() < 16:
                continue
            fr.append(float((np.abs(hf[band] - PLANE) > SLACK).sum()) / band.sum())
    return len(fr), (min(fr) if fr else 0.0), (float(np.mean(fr)) if fr else 0.0)


print('%-10s %-10s %6s %10s %10s' % ('subject', 'sheets', 'frames', 'worst', 'mean'))
for t in TAGS:
    for root, which in ((M + '/fixture_r3', 'af457755'), (M + '/fixture', 'this bake')):
        g = glob.glob(os.path.join(root, t, 'cards', '*_oct.lodm'))
        if not g:
            print('%-10s %-10s MISSING' % (t, which)); continue
        n, w, m = measure(g[0])
        print('%-10s %-10s %6d %9.1f%% %9.1f%%' % (t, which, n, 100 * w, 100 * m))
