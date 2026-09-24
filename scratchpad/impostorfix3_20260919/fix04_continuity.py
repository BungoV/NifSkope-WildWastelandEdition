"""IMPOSTORFIX3 fix 04 -- row 14's new clause, replaced because the first one
COULD NOT FAIL.

fix03 wrote "the first 8 rings outside the silhouette must lie in the band the
frame's own whole texels occupy". Measured on exe af457755's own sheets, where
every one of those texels is the card plane, that clause failed 0 frames of 16
on four of the five subjects -- because the band is the object's FULL depth
range and an object centred on its own card straddles the plane by
construction. A check that cannot fail on the input it convicts is not a check
(CONSTITUTION 4).

The clause that DOES separate them is CONTINUITY ACROSS THE SILHOUETTE: an
uncovered texel touching an inked one must carry a height within SLACK of the
mean of its inked neighbours. The 8-ring fill carries the object's own depth
out to meet the boundary, so the field is continuous there; the card-plane fill
puts a cliff at the boundary exactly as tall as the object's local depth.

Measured, frames failing this clause at the 10 per cent fraction:

    subject    af457755's sheets   this lane's   worst frame, before -> after
    blast_n4       4 of 16           0 of 16      20.6% -> 0.0%
    blast_n8      17 of 64           0 of 64      33.1% -> 5.9%
    maple_n4       3 of 16           2 of 16      39.9% -> 29.3%
    dead_n4        6 of 16           0 of 16      27.9% -> 5.9%
    rock_n4       16 of 16           1 of 16      57.2% -> 13.9%

It fails on all five before and on two after, and those two are REPORTED, not
tuned away: the maple has 297 whole texels in 32,768, so there is almost
nothing for the dilation to carry outward, and that is the same
photography-resolution defect IMPOSTORFIX1 named.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_sheet_check.py'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')


def once(a):
    if s.count(a) != 1:
        print('ANCHOR COUNT %d (want 1):\n%s' % (s.count(a), a[:120])); sys.exit(2)


A0 = """    return m
"""
B0 = """    return m


def nb_sum(a):
    \"\"\"Sum over the eight neighbours, excluding the texel itself.\"\"\"
    p = np.zeros((a.shape[0] + 2, a.shape[1] + 2), a.dtype)
    p[1:-1, 1:-1] = a
    return (p[:-2, :-2] + p[:-2, 1:-1] + p[:-2, 2:] +
            p[1:-1, :-2] + p[1:-1, 2:] +
            p[2:, :-2] + p[2:, 1:-1] + p[2:, 2:])
"""
once(A0); s = s.replace(A0, B0)

A1 = """            # (b) THE FIRST 8 RINGS OUTSIDE THE SILHOUETTE CARRY THE OBJECT'S
            #     OWN DEPTH. A neighbouring frame's ray lands just outside this
            #     frame's coverage constantly, and out there the only honest
            #     answer is the depth the object had at the nearest whole texel
            #     -- so those texels must lie in the SAME band the covered ones
            #     do. The card plane is the one value that band does not contain
            #     for any subject whose surface is not at z = 0, which is why
            #     this clause fails on a sheet baked by exe af457755 (whose law
            #     was "card plane everywhere outside") and passes on this one.
"""
B1 = """            # (b) THE HEIGHT FIELD IS CONTINUOUS ACROSS THE SILHOUETTE. A
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
"""
once(A1); s = s.replace(A1, B1)

A2 = """            uncov = af < FLOOR
            near = uncov & rings(full, NEAR)
            far = uncov & ~near
            offNear = near & ((hf < lo) | (hf > hi))
            fracNear = float(offNear.sum()) / max(1, int(near.sum()))
"""
B2 = """            uncov = af < FLOOR
            near = uncov & rings(full, NEAR)
            far = uncov & ~near
            cnt = nb_sum(inked.astype(np.int32))
            first = uncov & (cnt > 0)
            edge = np.where(cnt > 0, nb_sum(np.where(inked, hf, 0.0)) / np.maximum(cnt, 1), 0.0)
            offNear = first & (np.abs(hf - edge) > SLACK)
            fracNear = float(offNear.sum()) / max(1, int(first.sum()))
"""
once(A2); s = s.replace(A2, B2)

A3 = """                if offNear.any():
                    u = max(u, float(np.abs(np.where(hf[offNear] < lo, lo - hf[offNear],
                                                     hf[offNear] - hi)).max()))
"""
B3 = """                if offNear.any():
                    u = max(u, float(np.abs(hf[offNear] - edge[offNear]).max()))
"""
once(A3); s = s.replace(A3, B3)

A4 = """                      '%.0f%% of the first %d rings outside the band, '
                      '%.0f%% of the far texels off the card plane'
                      % (ii, jj, int(off.sum()), lo, hi,
                         100.0 * fracNear, NEAR, 100.0 * frac))
"""
B4 = """                      '%.0f%% of the silhouette edge discontinuous, '
                      '%.0f%% of the texels beyond %d rings off the card plane'
                      % (ii, jj, int(off.sum()), lo, hi,
                         100.0 * fracNear, 100.0 * frac, NEAR))
"""
once(A4); s = s.replace(A4, B4)

out = s.encode('utf-8')
if out.count(b'\r') != cr0:
    print('CR COUNT MOVED %d -> %d' % (cr0, out.count(b'\r'))); sys.exit(2)
open(P, 'wb').write(out)
print('fix04 applied: %d -> %d bytes, CR %d -> %d' % (len(b), len(out), cr0, out.count(b'\r')))
