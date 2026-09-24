"""IMPOSTORFIX3 fix 03 -- row 14's law follows the repair.

`impostor_sheet_check.py` clause (b) asserted that EVERY texel the object does
not cover sits at the card plane. That was the law exe af457755 bakes. The
8-ring fill makes it false on purpose: just outside the silhouette the texel now
carries the OBJECT'S OWN depth, because that is where a neighbouring frame's ray
lands.

Clause (b) is SPLIT, not relaxed:
  (b) uncovered and within 8 rings of a whole texel -> the height must lie in
      the band that frame's own whole texels occupy. NEW, and stricter than
      nothing: before this, "outside coverage" was unexamined except for the
      plane test, and the plane is the one value that band never contains for a
      subject whose surface is not at z = 0.
  (c) uncovered and further than 8 rings -> the card plane, as before.

The far half keeps the old 10 per cent fraction and the old SLACK; nothing is
loosened.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_sheet_check.py'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')


def once(a):
    if s.count(a) != 1:
        print('ANCHOR COUNT %d (want 1):\n%s' % (s.count(a), a[:120])); sys.exit(2)


A1 = """FULL = 250          # "the object covers this texel whole", in encoded alpha
FLOOR = 16          # the spec's coverage floor
PLANE = 128         # the card plane
SLACK = 12          # BC3 interpolates the blue in a shared 4x4 palette; this is
                    # the measured headroom, not a number chosen to pass
"""
B1 = """FULL = 250          # "the object covers this texel whole", in encoded alpha
FLOOR = 16          # the spec's coverage floor
PLANE = 128         # the card plane
SLACK = 12          # BC3 interpolates the blue in a shared 4x4 palette; this is
                    # the measured headroom, not a number chosen to pass
NEAR = 8            # rings. THE SAME NUMBER as lodgenRepairOctHeight's
                    # kOutRings, and the same metric: 8-connected passes, so a
                    # square of radius 8 and not a disc. If one moves the other
                    # must, and this row is where a disagreement shows up.


def rings(mask, n):
    \"\"\"The set within n 8-connected passes of `mask` -- Chebyshev distance <= n,
    which is exactly how the bake's dilation grows.\"\"\"
    m = mask
    for _ in range(n):
        p = np.zeros((m.shape[0] + 2, m.shape[1] + 2), bool)
        p[1:-1, 1:-1] = m
        m = (p[:-2, :-2] | p[:-2, 1:-1] | p[:-2, 2:] |
             p[1:-1, :-2] | p[1:-1, 1:-1] | p[1:-1, 2:] |
             p[2:, :-2] | p[2:, 1:-1] | p[2:, 2:])
    return m
"""
once(A1); s = s.replace(A1, B1)

A2 = """            # (b) and the texels it does NOT cover sit at the card plane, so the
            #     parallax step there is a no-op and cannot drag ink into sky.
            #
            #     THIS HALF IS A FRACTION, NOT AN ABSOLUTE, and the reason is
            #     BC3: the height shares one 4x4 palette with normal X and
            #     normal Y, so a block straddling the silhouette carries the
            #     plane value a few levels off however the bake wrote it. A
            #     repaired blast_n4 frame has 7..136 such texels of about four
            #     thousand (under 4%); the same frames before the repair had
            #     2360..3790, which is 60% to 95%. Ten percent separates them
            #     by a factor of six in both directions.
            uncov = af < FLOOR
            out = uncov & (np.abs(hf - PLANE) > SLACK)
            frac = float(out.sum()) / max(1, int(uncov.sum()))
            if off.any() or frac > 0.10:
                bad += 1
                u = 0.0
                if off.any():
                    u = max(u, float(np.abs(np.where(hf[off] < lo, lo - hf[off],
                                                     hf[off] - hi)).max()))
                if out.any():
                    u = max(u, float(np.abs(hf[out] - PLANE).max()))
                worstU = max(worstU, u / 255.0 * span)
                print('  frame %d,%d: %d covered texels outside [%.0f,%.0f], '
                      '%.0f%% of uncovered texels off the card plane'
                      % (ii, jj, int(off.sum()), lo, hi, 100.0 * frac))
"""
B2 = """            # (b) THE FIRST 8 RINGS OUTSIDE THE SILHOUETTE CARRY THE OBJECT'S
            #     OWN DEPTH. A neighbouring frame's ray lands just outside this
            #     frame's coverage constantly, and out there the only honest
            #     answer is the depth the object had at the nearest whole texel
            #     -- so those texels must lie in the SAME band the covered ones
            #     do. The card plane is the one value that band does not contain
            #     for any subject whose surface is not at z = 0, which is why
            #     this clause fails on a sheet baked by exe af457755 (whose law
            #     was "card plane everywhere outside") and passes on this one.
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
            offNear = near & ((hf < lo) | (hf > hi))
            fracNear = float(offNear.sum()) / max(1, int(near.sum()))
            out = far & (np.abs(hf - PLANE) > SLACK)
            frac = float(out.sum()) / max(1, int(far.sum()))
            if off.any() or frac > 0.10 or fracNear > 0.10:
                bad += 1
                u = 0.0
                if off.any():
                    u = max(u, float(np.abs(np.where(hf[off] < lo, lo - hf[off],
                                                     hf[off] - hi)).max()))
                if offNear.any():
                    u = max(u, float(np.abs(np.where(hf[offNear] < lo, lo - hf[offNear],
                                                     hf[offNear] - hi)).max()))
                if out.any():
                    u = max(u, float(np.abs(hf[out] - PLANE).max()))
                worstU = max(worstU, u / 255.0 * span)
                print('  frame %d,%d: %d covered texels outside [%.0f,%.0f], '
                      '%.0f%% of the first %d rings outside the band, '
                      '%.0f%% of the far texels off the card plane'
                      % (ii, jj, int(off.sum()), lo, hi,
                         100.0 * fracNear, NEAR, 100.0 * frac))
"""
once(A2); s = s.replace(A2, B2)

out = s.encode('utf-8')
if out.count(b'\r') != cr0:
    print('CR COUNT MOVED %d -> %d' % (cr0, out.count(b'\r'))); sys.exit(2)
open(P, 'wb').write(out)
print('fix03 applied: %d -> %d bytes, CR %d -> %d' % (len(b), len(out), cr0, out.count(b'\r')))
