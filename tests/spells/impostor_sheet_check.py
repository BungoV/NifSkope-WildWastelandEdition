"""The height channel of a baked `_n` sheet, checked against the sheet itself.

THIS ROW NEEDS NO APPLICATION AND NO SCENE. It reads the two BC3 sheets a
`.lodm` names and asks one question per frame: does every texel's height lie
inside the depth band the frame's OWN fully covered texels occupy?

WHY THAT IS THE RIGHT QUESTION. The drawer's parallax step reads the height at
a texel and then moves the sample that many world units along the view ray. A
height no surface ever had is therefore not a small error in a shaded pixel --
it is a licence to fetch colour from anywhere on the sheet, and on the shipped
blast_n4 fixture the texels UNDER the coverage floor decoded to +264 world
units on average and +743 at the 95th percentile against a card half-width of
135. That is what turned a bare trunk into a spray of detached flakes.

The band comes from the frame's own whole texels and from nothing outside the
file, so the row cannot be satisfied by editing a constant.

    python impostor_sheet_check.py <id>_oct.lodm

Prints one line per frame that fails and a summary; exit 1 on any failure.
"""
import sys, os, json, glob
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from impostor_bc_decode import load_dds

FULL = 250          # "the object covers this texel whole", in encoded alpha
FLOOR = 16          # the spec's coverage floor
PLANE = 128         # the card plane
SLACK = 12          # BC3 palette headroom, measured, not chosen to pass
NEAR = 8            # rings; must equal lodgenRepairOctHeight's kOutRings (8-connected, so a square)


def rings(mask, n):
    """The set within n 8-connected passes of `mask` -- Chebyshev distance <= n,
    which is exactly how the bake's dilation grows."""
    m = mask
    for _ in range(n):
        p = np.zeros((m.shape[0] + 2, m.shape[1] + 2), bool)
        p[1:-1, 1:-1] = m
        m = (p[:-2, :-2] | p[:-2, 1:-1] | p[:-2, 2:] |
             p[1:-1, :-2] | p[1:-1, 1:-1] | p[1:-1, 2:] |
             p[2:, :-2] | p[2:, 1:-1] | p[2:, 2:])
    return m


def nb_sum(a):
    """Sum over the eight neighbours, excluding the texel itself."""
    p = np.zeros((a.shape[0] + 2, a.shape[1] + 2), a.dtype)
    p[1:-1, 1:-1] = a
    return (p[:-2, :-2] + p[:-2, 1:-1] + p[:-2, 2:] +
            p[1:-1, :-2] + p[1:-1, 2:] +
            p[2:, :-2] + p[2:, 1:-1] + p[2:, 2:])


def main(argv):
    if len(argv) < 2:
        print('usage: impostor_sheet_check.py <id>_oct.lodm')
        return 2
    lodm = argv[1]
    raw = open(lodm, 'rb').read()
    j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
    N = int(j['oct'])
    span = float(j.get('depthSpan', 0.0))
    half = j.get('half', [1.0, 1.0])
    d = os.path.dirname(os.path.abspath(lodm))
    stem = os.path.basename(lodm)[:-len('_oct.lodm')]
    col = glob.glob(os.path.join(d, stem + '_oct_d.DDS')) \
        or glob.glob(os.path.join(d, stem + '_oct_bc.DDS'))
    nrm = glob.glob(os.path.join(d, stem + '_oct_n.DDS'))
    if not col or not nrm:
        print('no _d/_bc or _n sheet beside %s' % lodm)
        return 2
    alb = load_dds(col[0])[0]
    nsh = load_dds(nrm[0])[0]
    if alb.shape[:2] != nsh.shape[:2]:
        print('sheets disagree in size')
        return 2
    H, W = alb.shape[:2]
    fw, fh = W // N, H // N
    a = alb[..., 3] * 255.0
    h = nsh[..., 2] * 255.0

    bad = 0
    worstU = 0.0
    checked = 0
    for jj in range(N):
        for ii in range(N):
            ys, xs = slice(jj * fh, (jj + 1) * fh), slice(ii * fw, (ii + 1) * fw)
            af, hf = a[ys, xs], h[ys, xs]
            full = af >= FULL
            if full.sum() < 8:
                continue        # no depth of its own to be measured against
            lo, hi = hf[full].min() - SLACK, hf[full].max() + SLACK
            checked += 1
            # (a) every texel the object covers sits in the band
            inked = af >= FLOOR
            off = inked & ((hf < lo) | (hf > hi))
            # (b) the height field is continuous across the silhouette: an uncovered texel touching an inked
            #     one must carry a height within SLACK of its inked neighbours' mean.
            # (c) beyond 8 rings the card plane, unchanged, so the parallax step is an exact no-op.
            # Both are fractions, not absolutes: BC3 shares the height's palette with normal X/Y, so
            # boundary blocks are a few levels off however the bake wrote them.
            uncov = af < FLOOR
            near = uncov & rings(full, NEAR)
            far = uncov & ~near
            cnt = nb_sum(inked.astype(np.int32))
            first = uncov & (cnt > 0)
            edge = np.where(cnt > 0, nb_sum(np.where(inked, hf, 0.0)) / np.maximum(cnt, 1), 0.0)
            offNear = first & (np.abs(hf - edge) > SLACK)
            fracNear = float(offNear.sum()) / max(1, int(first.sum()))
            out = far & (np.abs(hf - PLANE) > SLACK)
            frac = float(out.sum()) / max(1, int(far.sum()))
            if off.any() or frac > 0.10 or fracNear > 0.10:
                bad += 1
                u = 0.0
                if off.any():
                    u = max(u, float(np.abs(np.where(hf[off] < lo, lo - hf[off],
                                                     hf[off] - hi)).max()))
                if offNear.any():
                    u = max(u, float(np.abs(hf[offNear] - edge[offNear]).max()))
                if out.any():
                    u = max(u, float(np.abs(hf[out] - PLANE).max()))
                worstU = max(worstU, u / 255.0 * span)
                print('  frame %d,%d: %d covered texels outside [%.0f,%.0f], '
                      '%.0f%% of the silhouette edge discontinuous, '
                      '%.0f%% of the texels beyond %d rings off the card plane'
                      % (ii, jj, int(off.sum()), lo, hi,
                         100.0 * fracNear, 100.0 * frac, NEAR))
    print('sheet check: %d frames measured, %d fail, worst excursion %.0f world '
          'units (card half %.1f x %.1f, depthSpan %.0f)'
          % (checked, bad, worstU, half[0], half[1], span))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
