#!/usr/bin/env python3
"""Two checks in tests/spells/lodgen_octahedral.sh that per-frame positioning moved.

1. The short-axis AIR BUDGET was a proxy calibrated on the union of the frames'
   silhouette boxes.  Per-frame positioning collapses that union onto the widest
   SINGLE view, so the same bake reads one texel narrower and the budget tips
   over its boundary with nothing having got worse.  Replaced by the ladder's own
   law, which the new `framefit` line makes directly checkable.

2. My own per-frame transition check capped the quad displacement at a quarter of
   the card's half extent -- a number chosen without measurement, and the maple's
   top view legitimately sits 239 units off.  Replaced by the exact relation
   between the offsets and the picture: |off| + that frame's own half box is the
   union half-extent `framefit` records, so no frame may exceed it and at least
   one must reach it.
"""
import sys

P = 'tests/spells/lodgen_octahedral.sh'
Q = chr(39)          # apostrophe, never typed through a shell heredoc
BS = chr(92)         # backslash


def rep(s, old, new):
    n = s.count(old)
    if n != 1:
        print('anchor matched %d times: %r' % (n, old[:70]))
        sys.exit(1)
    return s.replace(old, new)


s = open(P, encoding='utf-8').read()

OLD1 = """# THE SHORT AXIS IS AS NARROW AS IT CAN BE. A long-axis floor alone cannot see
# the defect this law was written for -- the old five-rung ladder left
# TreeBlasted05's silhouette in 4 texels of a 32-texel frame while its LONG axis
# was fine. The discriminating statement is that the frame is the SMALLEST
# multiple of 16 that does not crop, so ONE RUNG NARROWER WOULD HAVE CUT it.
# Stated as an AIR BUDGET rather than as tightness against the frame-resolution
# silhouette, because the two are not the same thing and the difference is not a
# defect: the frame is sized from pass one's VIEWPORT-resolution measurement, so
# that a faint extremity a fraction of a texel wide is never cropped, and the
# frame-resolution silhouette is therefore always a little smaller. What the law
# does promise is that the quantisation costs AT MOST ONE RUNG: 16 texels.
#
# The old five-rung ladder cannot pass this. TreeBlasted05 sat in a 32x32 frame
# (inner 24) with a 4-texel silhouette -- 20 texels of air, five rungs' worth.
SHORT = min(TW, TH)
print('  air on the short axis: inner %d, silhouette %d -> %d texels (one rung is 16)'
      % (INNER_S, unionS, INNER_S - unionS))
check('the short side carries at most ONE rung of air, which is the bound the quantisation itself sets',
      INNER_S - unionS < 16)"""

NEW1 = """# THE SHORT AXIS IS AS NARROW AS IT CAN BE, stated as the ladder's own law: the
# short side is the SMALLEST multiple of 16 in [16, long] whose inner rect is not
# narrower, in proportion, than the silhouette pass one measured -- so ONE RUNG
# NARROWER WOULD HAVE CROPPED IT.
#
# It was an AIR BUDGET in texels ("at most one rung, 16") until 2026-09-09
# evening, calibrated on the union of the frames' own silhouette boxes. That
# proxy stopped meaning what it says when PER-FRAME POSITIONING landed: the union
# used to be wider than any single view, because the views sat at different
# offsets inside the frame, and now it IS the widest single view -- so the same
# bake reads one texel narrower and the budget tips over its own boundary with
# nothing having got worse (measured: 29 texels of silhouette before, 28 after,
# against a bound of 16 texels of air). The law itself is checkable directly,
# because the `framefit` line records what the ladder was fed, so it is now
# checked directly and the air is printed as information.
#
# The old five-rung ladder cannot pass this either: TreeBlasted05 sat in a 32x32
# frame whose inner rect was 24 texels against a silhouette that needed 4, three
# rungs above the smallest that fits.
SHORT = min(TW, TH)
print('  air on the short axis: inner %d, silhouette %d -> %d texels'
      ' (information; the ladder's law is the check below)'
      % (INNER_S, unionS, INNER_S - unionS))
_ff = [l.split() for l in open(f'{d}/{base}.txt').read().splitlines() if l.startswith('framefit ')]
if not _ff:
    check('the meta carries the framefit line the aspect ladder is checked from', False)
else:
    _sx, _sy = float(_ff[0][1]), float(_ff[0][2])
    LONG = max(TW, TH)
    wantRatio = min(_sx, _sy) / max(_sx, _sy)
    il = LONG - 2 * padOf(LONG)
    rungs = [(_s, (_s - 2 * padOf(_s)) / il) for _s in range(16, LONG + 1, 16)]
    fits = [r for r in rungs if r[1] >= wantRatio]
    smallest = fits[0][0] if fits else LONG
    below = [r for r in rungs if r[0] < smallest]
    print('  aspect ladder: silhouette %.1f x %.1f units wants a short/long inner ratio of %.4f;'
          ' rungs %s; smallest that does not crop %d, shipped %d'
          % (_sx, _sy, wantRatio, ['%d:%.3f' % r for r in rungs], smallest, SHORT))
    check('the short side is the SMALLEST multiple of 16 whose inner rect does not crop the measured silhouette',
          SHORT == smallest)
    # the floor on the other side: one rung narrower must actually crop, or the
    # statement above is one a frame of almost any width could satisfy
    check('and one rung narrower would have cropped it (the ladder sits on its floor, not merely on a rung)',
          not below or below[-1][1] < wantRatio)"""

OLD2 = """# THE TRANSITION RULE, EXTENDED TO EVERY FRAME. The quad of frame (i,j) sits at
# `center + ox*right + oy*up` for that view's own basis, so whatever the basis,
# its centre is within |offset| of `center` and the whole set of quads lies
# inside a sphere of `max|offset|` about it. The card must therefore still sit
# where the model does: that displacement has to be SMALL against the card's own
# half extents, or the shift has moved the tree instead of the picture.
# (`scratchpad/cardfinal_20260909/transition_bounds.py` is the same statement
# against the model's OWN declared bound spheres, an independent reader.)
worstOff = max((max(abs(fo[2 * k]), abs(fo[2 * k + 1])) for k in range(len(fo) // 2)), default=0.0) if isinstance(fo, list) else 0.0
hw, hh = card.get('half', [0, 0])
print('  transition, per frame: largest quad displacement %.1f units against half extents %.1f x %.1f (%.1f%%)'
      % (worstOff, hw, hh, 100.0 * worstOff / max(hw, hh, 1e-6)))
check('no frame\\'s quad is displaced by as much as a quarter of the card\\'s own half extent',
      hw > 0 and hh > 0 and worstOff < 0.25 * max(hw, hh))"""

NEW2 = """# THE OFFSETS AGREE WITH THE PICTURE THEY SHIFTED, exactly.
#
# For view v the silhouette spans [off - halfV, off + halfV] about the camera
# centre, so max(|x0|, |x1|) is |off| + halfV -- and the UNION half-extent the
# `framefit` line records is the largest of those over all N^2 views. So for
# EVERY frame, |off| plus that frame's own half box (read off the sheet) must not
# exceed the union, and for at least ONE frame it must reach it. An offset with
# the wrong sign, the wrong scale, or from a stale bake breaks the first; a set of
# zeros breaks the second.
#
# This ties the written field to the picture. The transition rule against an
# INDEPENDENT reader -- the model's own declared bound spheres, extended to the
# most displaced frame, with the zeroed control that must fail -- is
# `scratchpad/cardfinal_20260909/transition_bounds.py`, which needs the model and
# is run by the lane rather than here.
#
# A quarter of the card's half extent stood here for one run of this harness and
# was wrong: it was a fraction chosen without measurement, and the maple's TOP
# view legitimately sits 239 units off centre (28.8%), because a canopy's plan
# view is not centred on the trunk.
ffL = [l.split() for l in open(f'{d}/{ident}.txt').read().splitlines() if l.startswith('framefit ')]
albS = Image.open(f'{d}/{ident}_oct_albedo.png').convert('RGBA')
uX, uY = (float(ffL[0][3]), float(ffL[0][4])) if ffL else (0.0, 0.0)
th_ = int(oct[3])
uptX_, uptY_ = 2.0 * card['half'][0] / tw, 2.0 * card['half'][1] / th_
reach, over = 0.0, 0
for j in range(octN_):
    for i in range(octN_):
        cell = albS.crop((i * tw, j * th_, (i + 1) * tw, (j + 1) * th_))
        ks = [k for k, p in enumerate(cell.getdata()) if p[3] >= 16]
        if not ks:
            continue
        xs = [k % tw for k in ks]
        ys = [k // tw for k in ks]
        hX = 0.5 * (max(xs) - min(xs)) * uptX_
        hY = 0.5 * (max(ys) - min(ys)) * uptY_
        ox, oy = fo[2 * (j * octN_ + i)], fo[2 * (j * octN_ + i) + 1]
        rx, ry = (abs(ox) + hX) / max(uX, 1e-6), (abs(oy) + hY) / max(uY, 1e-6)
        reach = max(reach, rx, ry)
        if rx > 1.02 or ry > 1.02:
            over += 1
print('  offsets against the picture: union half-extent %.1f x %.1f units;'
      ' worst frame reaches %.3f of it; frames past it: %d' % (uX, uY, reach, over))
check('every frame's offset plus its own silhouette stays inside the extent a fixed-centre card spanned',
      uX > 0 and uY > 0 and over == 0)
check('and at least one frame REACHES that extent (a sheet of zero offsets could not)', reach >= 0.90)"""

# the apostrophes inside the two Python string literals have to be escaped for
# the harness's own Python, which is why they are spliced rather than typed
NEW1 = NEW1.replace("the ladder's law is the check below", "the ladder" + BS + Q + "s law is the check below")
NEW2 = NEW2.replace("check('every frame's offset", "check(" + Q + "every frame" + BS + Q + "s offset")

s = rep(s, OLD1, NEW1)
s = rep(s, OLD2, NEW2)
open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('written; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
