#!/usr/bin/env python3
"""CARDFIT3 fix 05 -- two of the new octahedral checks were wrong. Both found by
running them (68 ok, 2 FAIL) rather than by reading them.

 1. "the silhouette FILLS the inner rect on the long axis to within 2%" read
    89.3% and failed. 2% was not a measurement, it was a hope: pass one measures
    the silhouette in VIEWPORT pixels and pass two DOWNSAMPLES that into a
    64-texel frame, so an extremity a fraction of a texel wide falls under the
    coverage floor on the way in. 89.3% at 64 px and 94.6% at 128 px are what
    the law actually produces. The floor becomes 85%, with the reason written
    down -- and, because a long-axis floor alone would not have caught the old
    law at all (its failure was on the SHORT axis), the discriminating check is
    added beside it: the frame must be the SMALLEST multiple of 16 that does not
    crop, i.e. ONE RUNG NARROWER WOULD HAVE CUT the silhouette. That is
    "maximizing the tree's size in each row and column" stated as a test, and
    the old five-rung ladder fails it.

 2. "the transparent texels beside the silhouette carry dilated colour" read
    355 black of 473 and failed -- correctly, because it was reading the BAKE's
    PNG, which is un-dilated by design (the dilation is `lodgenCard`'s, on the
    way into the DDS). The harness already measures it in the right place, on
    the converted sheet's block endpoints ("edge blocks in the colour sheet:
    362, with a near-black endpoint: 0"). The misplaced check is removed rather
    than weakened.
"""
P = 'tests/spells/lodgen_octahedral.sh'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
n = 0

old = """print('  union of the %d silhouette boxes: %d x %d texels; inner rect %d x %d (long axis fill %.1f%%)'
      % (N * N, ux1 - ux0, uy1 - uy0, INNER_S, INNER_L, 100.0 * unionL / INNER_L))
check('the silhouette FILLS the inner rect on the long axis to within 2%%',
      unionL >= 0.98 * INNER_L)
check('and does not exceed it (the padding is not eaten)', unionL <= INNER_L and unionS <= INNER_S)
"""
new = """print('  union of the %d silhouette boxes: %d x %d texels; inner rect %d x %d (long axis fill %.1f%%)'
      % (N * N, ux1 - ux0, uy1 - uy0, INNER_S, INNER_L, 100.0 * unionL / INNER_L))
# THE LONG AXIS IS FILLED. Not to 100%: pass one measures the silhouette in
# VIEWPORT pixels and pass two downsamples that into the frame, so an extremity
# a fraction of a texel wide falls under the coverage floor on the way in.
# Measured 89.3% at a 64 px frame and 94.6% at 128 px, so the floor is 85%.
check('the silhouette fills at least 85% of the inner rect on the long axis',
      unionL >= 0.85 * INNER_L)
check('and does not exceed it (the padding is not eaten)', unionL <= INNER_L and unionS <= INNER_S)
# THE SHORT AXIS IS AS NARROW AS IT CAN BE. A long-axis floor alone cannot see
# the defect this law was written for -- the old five-rung ladder left
# TreeBlasted05's silhouette in 4 texels of a 32-texel frame while its LONG axis
# was fine. The discriminating statement is that the frame is the SMALLEST
# multiple of 16 that does not crop, so ONE RUNG NARROWER WOULD HAVE CUT it.
SHORT = min(TW, TH)
if SHORT > 16:
    nxt = SHORT - 16
    nxtPad = padOf(nxt)
    nxtInner = nxt - 2 * nxtPad
    # the silhouette scales with the LONG axis, which does not move, so the
    # question is only whether it would still fit across the narrower inner rect
    print('  one rung narrower: short side %d -> %d, inner %d; the silhouette needs %d'
          % (SHORT, nxt, nxtInner, unionS))
    check('the frame is the NARROWEST multiple of 16 that does not crop (one rung less would)',
          nxtInner < unionS)
else:
    print('  the short side is already at the floor of 16 texels')
    check('the short side is at the floor, so no narrower frame exists', SHORT == 16)
"""
assert s.count(old) == 1, ('fill', s.count(old))
s = s.replace(old, new); n += 1

old = """# DILATION: colour under the transparent texels, so filtering never pulls black
# into an edge. Measured on the texel just outside the silhouette.
apl = alb.load()
dil = darkc = 0
for j in range(N):
    for i in range(N):
        for k in covered(i, j):
            x, y = i * TW + k % TW, j * TH + k // TW
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                p = apl[x + dx, y + dy]
                if p[3] < 16:
                    dil += 1
                    if p[0] + p[1] + p[2] == 0:
                        darkc += 1
print('  transparent texels beside the silhouette: %d, of which BLACK: %d' % (dil, darkc))
check('the transparent texels beside the silhouette carry dilated colour', dil > 0 and darkc == 0)
"""
new = """# DILATION is NOT checked here. This is the BAKE's PNG and it is un-dilated by
# design -- the dilation is lodgenCard's, on the way into the DDS -- and a check
# here reads 355 black texels of 473 and is right to. It is measured further
# down, on the converted sheet's own block endpoints ("edge blocks in the colour
# sheet ... with a near-black endpoint: 0"), which is where the property exists.
"""
assert s.count(old) == 1, ('dilation', s.count(old))
s = s.replace(old, new); n += 1

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('lodgen_octahedral.sh: %d edits' % n)
