#!/usr/bin/env python3
"""Extend the transition gate to PER-FRAME positioning.

`scratchpad/cardfit_20260909/transition_bounds.py` gated `card.center` against the
model's own declared bound spheres, an instrument independent of the bake.  Since
2026-09-09 evening every frame also carries its own offset (`card.frameOffset`),
so the quad of frame (i,j) sits at `center + ox*right + oy*up`.  Whatever the
view basis, that centre is within |offset| of `center`, so the MOST DISPLACED
FRAME is the one the gate has to survive -- and it must, with the zeroed control
still failing.
"""
import sys

P = 'scratchpad/cardfinal_20260909/transition_bounds.py'
Q = chr(39)

s = open(P, encoding='utf-8').read()


def rep(old, new):
    global s
    n = s.count(old)
    if n != 1:
        print('anchor matched %d times: %r' % (n, old[:70]))
        sys.exit(1)
    s = s.replace(old, new)


rep('''"""THE TRANSITION GATE, camera-free.''',
    '''"""THE TRANSITION GATE, camera-free -- extended to PER-FRAME positioning.''')

rep('''CONTROL, which must FAIL: `center` treated as zero, i.e. the quad at the pivot.
''',
    '''  frames : since 2026-09-09 evening each frame carries its own offset
           (`card.frameOffset`), so its quad sits at `center + ox*right +
           oy*up`.  Whatever the view basis that centre is within |offset| of
           `center`, so the MOST DISPLACED FRAME is what the centre and extent
           tests have to survive -- both are re-run against it.

  CONTROLS, which must FAIL: `center` treated as zero, i.e. the quad at the
  pivot; and, for the per-frame half, the most displaced frame measured against
  the pivot rather than against `center`.
''')

rep('''        C, HW, HH = m['center'], m['half'][0], m['half'][1]''',
    '''        C, HW, HH = m['center'], m['half'][0], m['half'][1]
        # PER-FRAME POSITIONING: the largest displacement any one frame's quad
        # carries, as a scalar, since the view basis is not known here and the
        # bound is |offset| whatever it is
        fo = m.get('frameOffset') or []
        worst = max((math.hypot(fo[2 * k], fo[2 * k + 1]) for k in range(len(fo) // 2)),
                    default=0.0)''')

rep('''        okC = d0 > 0 and d * RATIO <= d0''',
    '''        okC = d0 > 0 and d * RATIO <= d0
        # the same statement for the frame that moves furthest: even IT must be
        # four times closer to the model's own bound centre than the pivot is
        dF = d + worst
        okF = d0 > 0 and dF * RATIO <= d0
        # and its quad must still lie inside the model's own bound sphere
        okFin = dF + max(HW * (fw - 2 * px) / fw, HH * (fh - 2 * py) / fh) <= br * 1.25''')

rep('''        for f in (okC, okS, okIn, okBig, okCtl):''',
    '''        print('           per frame: largest quad displacement %.1f units;'
              ' the most displaced frame is %.1f units = %.1f%% of r from the bound centre'
              ' (%.1fx closer than the pivot)   %s'
              % (worst, dF, 100.0 * dF / br, d0 / max(dF, 1e-6),
                 'ok' if okF and okFin else 'FAIL'))
        if not fo:
            print('           NOTE: this set carries no frameOffset -- a bake from before'
                  ' per-frame positioning, and the per-frame rows above are the fixed-centre case')
        for f in (okC, okS, okIn, okBig, okCtl, okF, okFin):''')

open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('written; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
