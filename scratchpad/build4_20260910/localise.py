#!/usr/bin/env python
"""Where do lane CLAMP's beyond-band texels sit?

Not a fix and not a re-pin: the resuming lane measures a failing gate's cause
and stops (nifskope-ww-resume-pending SS6). For each failing sheet, report which
of the four borders each beyond-band texel is nearest, and the histogram of its
distance from that border.

Usage: python localise.py <beforeTexDir> <afterTexDir> <stem> <role> <band>
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'clamp_20260910'))
from edgeband import decode


def main(dbefore, dafter, stem, role, band):
    fa = os.path.join(dbefore, stem + role + '.DDS')
    fb = os.path.join(dafter, stem + role + '.DDS')
    A, w, h = decode(fa)
    B, _, _ = decode(fb)
    sides = {'W': 0, 'E': 0, 'N': 0, 'S': 0}
    hist = {}
    corner = 0
    for y in range(h):
        for x in range(w):
            if A[y * w + x] == B[y * w + x]:
                continue
            ds = (('W', x), ('E', w - 1 - x), ('N', y), ('S', h - 1 - y))
            d = min(v for _, v in ds)
            if d < band:
                continue
            near = [k for k, v in ds if v == d]
            if len(near) > 1:
                corner += 1
            sides[near[0]] += 1
            hist[d] = hist.get(d, 0) + 1
    tot = sum(sides.values())
    print('%s %s  band %d  beyond=%d  (corner-ambiguous %d)'
          % (stem, role or '(colour)', band, tot, corner))
    print('  nearest border: ' + '  '.join('%s=%d' % (k, sides[k]) for k in 'WENS'))
    print('  distance histogram: ' + '  '.join(
        '%d:%d' % (d, hist[d]) for d in sorted(hist)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2], sys.argv[3],
                  sys.argv[4] if sys.argv[4] != '-' else '', int(sys.argv[5])))
