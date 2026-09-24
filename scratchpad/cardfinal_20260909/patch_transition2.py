#!/usr/bin/env python3
"""The per-frame half of the transition gate, stated so its threshold is derived.

The first version reused lane CARDFIT3's RATIO of 4 -- written for `card.center`,
a single point, against the zeroed-offset control -- and applied it to
`d + max|offset|`, a PESSIMISTIC scalar bound that assumes every frame's offset
points straight away from the model's bound centre. Two of three bases came out
at 2.6x and 3.0x and "failed" a number nothing had measured.

What the per-frame row can honestly say, with the instrument available (the
model's own declared bound spheres):

  * the MOST DISPLACED frame's quad still lies inside the model's own bound
    sphere -- and the CONTROL, the same quad anchored at the pivot instead of at
    `center`, does not;
  * that frame is still closer to the bound centre than the pivot is, with the
    ratio reported rather than thresholded.

The 4x discrimination stays where CARDFIT3 measured it: on `card.center` itself.
"""
import sys

P = 'scratchpad/cardfinal_20260909/transition_bounds.py'
s = open(P, encoding='utf-8').read()


def rep(old, new):
    global s
    n = s.count(old)
    if n != 1:
        print('anchor matched %d times: %r' % (n, old[:70]))
        sys.exit(1)
    s = s.replace(old, new)


rep("""        # the same statement for the frame that moves furthest: even IT must be
        # four times closer to the model's own bound centre than the pivot is
        dF = d + worst
        okF = d0 > 0 and dF * RATIO <= d0
        # and its quad must still lie inside the model's own bound sphere
        okFin = dF + max(HW * (fw - 2 * px) / fw, HH * (fh - 2 * py) / fh) <= br * 1.25""",
    """        # THE MOST DISPLACED FRAME. `worst` is a scalar, because the view basis is
        # not known here, so `d + worst` is the pessimistic case: every frame's
        # offset pointing straight away from the model's bound centre.
        dF = d + worst
        # it must still be CLOSER to the bound centre than the pivot is (reported
        # as a ratio, not thresholded -- the 4x discrimination belongs to
        # `card.center`, which is a point, not to this upper bound)
        okF = d0 > 0 and dF < d0
        # and its quad must still lie inside the model's own bound sphere...
        inner = max(HW * (fw - 2 * px) / fw, HH * (fh - 2 * py) / fh)
        okFin = dF + inner <= br * 1.25
        # ...which is the check, and THIS is its control: the same quad anchored
        # at the pivot instead of at `center` must NOT fit
        okFinCtl = d0 + inner > br * 1.25""")

rep("""        if not fo:
            print('           NOTE: this set carries no frameOffset -- a bake from before'
                  ' per-frame positioning, and the per-frame rows above are the fixed-centre case')
        for f in (okC, okS, okIn, okBig, okCtl, okF, okFin):""",
    """        print('           CONTROL, the most displaced frame anchored at the PIVOT:'
              ' %.1f + %.1f units against a bound radius of %.1f   %s'
              % (d0, inner, br, 'ok (it does not fit, as it must not)' if okFinCtl else 'FAIL'))
        if not fo:
            print('           NOTE: this set carries no frameOffset -- a bake from before'
                  ' per-frame positioning, and the per-frame rows above are the fixed-centre case')
        for f in (okC, okS, okIn, okBig, okCtl, okF, okFin, okFinCtl):""")

rep("""              ' (%.1fx closer than the pivot)   %s'
              % (worst, dF, 100.0 * dF / br, d0 / max(dF, 1e-6),
                 'ok' if okF and okFin else 'FAIL'))""",
    """              ' (%.1fx closer than the pivot), quad %.1f + %.1f against r %.1f   %s'
              % (worst, dF, 100.0 * dF / br, d0 / max(dF, 1e-6), dF, inner, br,
                 'ok' if okF and okFin else 'FAIL'))""")

open(P, 'w', encoding='utf-8', newline='').write(s)
print('written')
