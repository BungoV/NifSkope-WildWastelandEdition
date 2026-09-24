"""LAND1 gate A5 -- the swirl instrument's KNOWN ANSWERS, re-run in THIS lane
before any candidate of this lane is scored.

TILING4 froze the instrument (t4_lib.swirl, h1_sweep.score); this lane imports
it UNCHANGED and only asks it the four questions whose answers were written
down before the run:

  A5.1  a pure sinusoid grating          must read near 1.0 (perfectly coherent)
  A5.2  band-limited isotropic noise     must read near its own phase twin
  A5.3  a synthetic swirl of KNOWN strain on a real vanilla sheet: the reading
        must RISE MONOTONICALLY with the strain, up to about 0.75 texels
  A5.4  the NOTCH: a 16x amplified 10.667-texel repeat injected into a real
        vanilla sheet must move the swirl reading by ~0, or the instrument is
        reading the repeat and calling it swirl

    python a5_controls.py  ->  logs/a5_controls.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T4 = os.path.join(os.path.dirname(HERE), 'tiling4_20260912')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (T4, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t4_lib as W                                            # noqa: E402
import h1_sweep as HS                                         # noqa: E402

VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
L = []


def say(s):
    L.append(s)
    print(s)


def main():
    say('LAND1 gate A5 -- the frozen swirl instrument, four known answers')
    say('instrument: tiling4_20260912/t4_lib.py, imported unchanged')
    say('')
    ok = 0
    n = 0

    # A5.1 -- a pure grating
    g = W.grating(512, 512, period=16.0, amp=8.0, angle=0.37)
    r = W.swirl(g, notch=False)
    n += 1
    good = r >= 0.90
    ok += good
    say('A5.1 pure sinusoid grating                  %.4f   want >= 0.90   %s'
        % (r, 'ok' if good else 'RED'))

    # A5.2 -- isotropic noise against its own phase twin
    iso = W.isotropic(512, 512, seed=5)
    r = W.swirl(iso, notch=False)
    f = W.swirl_floor(iso, seed=1, notch=False)
    n += 1
    good = abs(r - f) <= 0.25 * max(f, 1e-9)
    ok += good
    say('A5.2 isotropic noise %.4f vs its own twin %.4f  within 25%%   %s'
        % (r, f, 'ok' if good else 'RED'))

    # A5.3 -- monotone in injected strain, on a REAL vanilla sheet
    p = os.path.join(VAN, 'Commonwealth.4.-20.20.DDS')
    if not os.path.isfile(p):
        say('A5.3/A5.4 REFUSED: no vanilla sheet at %s' % p)
        return 2
    lum = S.lum(S.Dds(p).level(0))
    prev = None
    mono = True
    row = []
    for amp in (0.0, 0.25, 0.5, 0.75):
        w = W.apply_swirl(lum, amp, 24.0)
        r = W.swirl(w)
        row.append((amp, r))
        if prev is not None and r < prev - 1e-6:
            mono = False
        prev = r
    n += 1
    ok += mono
    say('A5.3 swirl vs injected strain (texels):     %s   monotone   %s'
        % ('  '.join('%.2f->%.4f' % t for t in row), 'ok' if mono else 'RED'))

    # A5.4 -- the notch must not read the repeat
    h, w = lum.shape
    yy, xx = np.mgrid[0:h, 0:w]
    rep = np.sin(2.0 * np.pi * xx / W.REPEAT_TEXELS) * 16.0
    inj = lum + rep
    a = W.swirl(inj, notch=True)
    b = W.swirl(lum, notch=True)
    c = W.swirl(inj, notch=False)
    d = W.swirl(lum, notch=False)
    n += 1
    good = abs(a - b) <= 0.02
    ok += good
    say('A5.4 a 16x 10.667-texel repeat injected:')
    say('     WITH the notch    clean %.4f  injected %.4f  moved %+.4f  %s'
        % (b, a, a - b, 'ok' if good else 'RED'))
    say('     WITHOUT the notch clean %.4f  injected %.4f  moved %+.4f'
        '  (this is what the notch is for)' % (d, c, c - d))
    say('')
    say('A5: %d of %d known answers met' % (ok, n))
    with open(os.path.join(HERE, 'logs', 'a5_controls.txt'), 'w',
              newline='\n') as f:
        f.write('\n'.join(L) + '\n')
    return 0 if ok == n else 1


if __name__ == '__main__':
    sys.exit(main())
