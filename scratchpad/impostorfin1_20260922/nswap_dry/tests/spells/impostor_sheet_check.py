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
SLACK = 12          # BC3 interpolates the blue in a shared 4x4 palette; this is
                    # the measured headroom, not a number chosen to pass
NEAR = 8            # rings. THE SAME NUMBER as lodgenRepairOctHeight's
                    # kOutRings, and the same metric: 8-connected passes, so a
                    # square of radius 8 and not a disc. If one moves the other
                    # must, and this row is where a disagreement shows up.


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
    h = nsh[..., 3 if b'"nlayout":2' in raw.replace(b' ', b'') else 2] * 255.0  # _n layout 2 = height in A

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
            # (b) THE HEIGHT FIELD IS CONTINUOUS ACROSS THE SILHOUETTE. A
            #     neighbouring frame's ray lands just outside this frame's
            #     coverage constantly, and out there the only honest answer is
            #     the depth the object had at the edge -- so an uncovered texel
            #     TOUCHING an inked one must carry a height within SLACK of the
            #     mean of its inked neighbours.
            #
            #     THE CLAUSE WRITTEN HERE FIRST WAS "those texels lie in the
            #     band the frame's whole texels occupy", AND IT COULD NOT FAIL:
            #     on exe af457755's sheets, where every one of them is the card
            #     plane, it failed 0 frames of 16 on four of the five fixture
            #     subjects, because that band is the object's FULL depth range
            #     and an object centred on its own card straddles the plane by
            #     construction. It was measured, found unable to convict, and
            #     replaced. Continuity separates them on all five:
            #       frames failing at the 10% fraction, af457755 -> this bake
            #       blast_n4 4/16 -> 0/16, blast_n8 17/64 -> 0/64,
            #       maple_n4 3/16 -> 2/16, dead_n4 6/16 -> 0/16,
            #       rock_n4 16/16 -> 1/16.
            #     The maple and the rock still fail frames after the repair and
            #     that is reported rather than tuned away: the maple has 297
            #     whole texels in 32,768, so there is nearly nothing for the
            #     dilation to carry outward.
            #
            # (c) BEYOND 8 RINGS the card plane, unchanged, so the parallax step
            #     is an exact no-op out where nothing samples.
            #
            #     BOTH HALVES ARE FRACTIONS, NOT ABSOLUTES, and the reason is
            #     BC3: the height shares one 4x4 palette with normal X and
            #     normal Y, so a block straddling either boundary carries its
            #     value a few levels off however the bake wrote it. A repaired
            #     blast_n4 frame has 7..136 such texels of about four thousand
            #     (under 4%); the same frames before the repair had 2360..3790,
            #     which is 60% to 95%. Ten percent separates them by a factor of
            #     six in both directions.
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
