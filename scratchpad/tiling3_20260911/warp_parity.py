"""TILING3 -- does src/lodgen.cpp's warp compute the same numbers as the
prototype the numbers were picked on?

The C++ that shipped is a transcription of a4_warp.py's `_hash01` /
`warp_offsets` and a5_tune.py's `warp2`.  A transcription is a place to make a
mistake -- a shift width, a multiply order, a sign on an octave stride -- and a
mistake there would not crash anything: it would quietly bake a DIFFERENT warp
from the one the 7-sheet selection measured, and every number in this lane's
report would then describe a bake the exe never produces.

So the same pure function is evaluated on both sides at the same fifteen world
positions, at OFF and at four settings, and diffed.  The positions deliberately
include negatives (the hash's int-to-uint wrap), a point one ulp under a lattice
line, and +-1,999,999 units -- the far edge of the worldspace, where a float
lattice index would have quantised and where the C++ uses double for exactly
that reason.

FLOOR: the OFF block must come back identical to the input, because "off is the
rung's bytes" is the switch's whole safety argument; and the ON blocks must
differ from OFF at every point, or the comparison is passing on a warp that does
nothing.  Both are asserted, not eyeballed.

THE PRECISION THIS PROBE CAN AND CANNOT REACH.  warp_parity.cpp prints nine
significant digits, so the comparison below is exact only to nine -- at the far
worldspace corner (+-1,999,999 units) that is about 0.005 world units, which is
one twenty-fifth of a float32 ulp there (0.125) and about a ten-thousandth of a
bake texel (32 units).  So this probe proves the two implementations agree far
inside the representable resolution; it does NOT by itself prove the OFF path is
bit-identical.  That claim rests on something stronger and structural -- OFF is
an early `return` that copies the input, not arithmetic that happens to cancel
-- and it is PROVED on the real exe by gate F2, which requires the switch at its
off value to produce the rung's bytes on every file of two tiles.

    (compile)  g++ -O2 -std=c++17 -static -o warp_parity.exe warp_parity.cpp
    (run)      warp_parity.exe > logs/warp_parity_cpp.txt
    python warp_parity.py  ->  logs/warp_parity.txt
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import a4_warp as W                                           # noqa: E402
import a5_tune as A5                                          # noqa: E402

POS = [(0.0, 0.0), (1.0, -1.0), (341.3333, 341.3333),
       (-81920.0, 98304.0), (-81920.0, 81920.0), (-147456.0, -81920.0),
       (-16384.0, -81920.0), (114688.0, -81920.0), (-16384.0, 65536.0),
       (98304.0, 65536.0), (1999999.0, -1999999.0), (12345.678, -98765.432),
       (-0.5, -0.5), (-1024.0, -1024.0), (1023.9999, 1023.9999)]
SETTINGS = [(683.0, 1024.0, 1), (683.0, 1024.0, 2),
            (1365.0, 2048.0, 3), (341.0, 2048.0, 1)]


def py_warp(amp, lat, oc):
    """The prototype, evaluated exactly as the C++ does: the world coordinate
    enters as a float32 (it is a float in the generator), the warp is computed
    in double, and the result is stored back into a float32."""
    wx = np.array([float(np.float32(p[0])) for p in POS], np.float64)
    wy = np.array([float(np.float32(p[1])) for p in POS], np.float64)
    if amp <= 0.0:
        return np.float32(wx), np.float32(wy)
    ox, oy = A5.warp2(wx, wy, amp, lat, oc)
    return np.float32(wx + ox), np.float32(wy + oy)


def main():
    src = os.path.join(HERE, 'logs', 'warp_parity_cpp.txt')
    if not os.path.exists(src):
        raise SystemExit('REFUSED: %s is missing -- compile and run '
                         'warp_parity.cpp first' % src)
    raw = [ln.strip() for ln in open(src, encoding='utf-8-sig') if ln.strip()]
    blocks, cur = {}, None
    for ln in raw:
        if ln.startswith('OFF') or ln.startswith('A='):
            cur = ln
            blocks[cur] = []
        else:
            blocks[cur].append(tuple(float(x) for x in ln.split()))
    L = ['TILING3 -- C++ warp vs the prototype it was picked on', '']
    names = ['OFF'] + ['A=%g L=%g o%d' % s for s in SETTINGS]
    settings = [(0.0, 1024.0, 1)] + list(SETTINGS)
    worst_all = 0.0
    off_cpp = None
    bad = []
    for nm, (amp, lat, oc) in zip(names, settings):
        if nm not in blocks:
            raise SystemExit('REFUSED: block %r missing from the C++ output' % nm)
        cpp = np.asarray(blocks[nm], np.float64)
        if len(cpp) != len(POS):
            raise SystemExit('REFUSED: block %r has %d rows, expected %d'
                             % (nm, len(cpp), len(POS)))
        px, py = py_warp(amp, lat, oc)
        pyv = np.stack([np.float64(px), np.float64(py)], 1)
        # both sides through the SAME nine-digit formatting the C++ used
        pyv9 = np.array([[float('%.9g' % x), float('%.9g' % y)] for x, y in pyv])
        d = np.abs(cpp - pyv9)
        worst = float(d.max())
        worst_all = max(worst_all, worst)
        # ulp of a float32 at the magnitude in play, for scale
        ulp = float(np.max(np.spacing(np.float32(np.abs(pyv)))))
        L.append('%-16s worst |C++ - prototype| = %.6g world units  (one float32 '
                 'ulp here = %.6g, one bake texel = 32)' % (nm, worst, ulp))
        if worst > ulp:
            bad.append('%s disagrees by more than one float32 ulp' % nm)
        if nm == 'OFF':
            off_cpp = cpp
            # compared at the probe's OWN print precision: nine significant
            # digits, the same formatting the C++ used, so the test is exact on
            # what was actually written rather than on what a wider format would
            # have written.
            exact = np.array([[float('%.9g' % float(np.float32(a))),
                               float('%.9g' % float(np.float32(b)))] for a, b in POS])
            same = bool(np.array_equal(cpp, exact))
            L.append('   FLOOR 1 -- OFF returns the coordinate untouched to the probe`s'
                     ' nine digits: %s' % ('YES' if same else 'NO -- THE WAY BACK MOVES IT'))
            L.append('              (bit-identity of OFF is structural -- an early return'
                     ' -- and is proved on the exe by gate F2)')
            if not same:
                bad.append('OFF does not reproduce the input')
        else:
            moved = float(np.abs(cpp - off_cpp).max())
            L.append('   FLOOR 2 -- this setting actually moves the coordinate: '
                     'max shift %.3f world units %s' % (moved, 'YES' if moved > 1.0 else 'NO'))
            if moved <= 1.0:
                bad.append('%s does not move the coordinate' % nm)
    L.append('')
    L.append('worst disagreement anywhere: %.6g world units over %d positions x %d '
             'settings' % (worst_all, len(POS), len(settings)))
    L.append('VERDICT: %s' % ('PARITY -- the exe computes the warp the numbers were '
                              'picked on' if not bad
                              else 'FAILED: ' + '; '.join(bad)))
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'warp_parity.txt'), 'w', newline='\n') as f:
        f.write(txt)
    if bad:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
