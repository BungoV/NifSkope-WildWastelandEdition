#!/usr/bin/env python
"""The cube gate against the FROZEN pre-registration, not the run-time one.

`prereg_cube.md` allows the harness to recompute its predicted table from the
sidecar's own `oct` line at run time, and the harness does. That is legitimate
-- but it means the number the gate compared against was computed after the
build. This re-runs the same comparison against the table written BEFORE it, so
the pre-registration is used as a pre-registration.

Usage: python frozen_cube.py <prereg_cube.md> <lodgen_octahedral.log>
"""
import re, sys


def prereg(path):
    t = {}
    for ln in open(path, encoding='utf-8'):
        m = re.match(r'\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*[\d.]+\s*\|\s*[\d.]+\s*\|'
                     r'\s*([\d.]+)\s*\|\s*([\d.]+)\s*\|', ln)
        if m:
            t[(int(m.group(1)), int(m.group(2)))] = (float(m.group(3)),
                                                     float(m.group(4)))
    return t


def measured(path):
    t = {}
    for ln in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'\s*(\d+) (\d+) \|\s*([\d.]+)\s+([\d.]+) \|\s*(\d+)\s+(\d+) \|'
                     r'\s*(\d+)\s+(\d+)', ln)
        if m:
            g = m.groups()
            t[(int(g[0]), int(g[1]))] = (float(g[2]), float(g[3]),
                                         int(g[4]), int(g[5]),
                                         int(g[6]), int(g[7]))
    return t


def main(pp, lp):
    P, M = prereg(pp), measured(lp)
    print('frozen pre-registration rows: %d   measured frames: %d' % (len(P), len(M)))
    common = sorted(set(P) & set(M))
    print('frames in both: %d' % len(common))
    wo = wp = wr = 0.0
    wof = wpf = None
    for k in common:
        px, py = P[k]
        rx, ry, ox, oy, sx, sy = M[k]
        for pred, run, o, s in ((px, rx, ox, sx), (py, ry, oy, sy)):
            if abs(o - pred) > wo:
                wo, wof = abs(o - pred), k
            if abs(s - pred) > wp:
                wp, wpf = abs(s - pred), k
            wr = max(wr, abs(run - pred))
    print()
    print('worst |ORTHO measured - FROZEN predicted|      : %.2f texels  (frame %s)'
          % (wo, wof))
    print('worst |PERSPECTIVE CONTROL - FROZEN predicted| : %.2f texels  (frame %s)'
          % (wp, wpf))
    print('worst |harness run-time predicted - FROZEN|    : %.2f texels'
          % wr)
    print()
    print('the bar is 2 texels, and the control must exceed it')
    print('ORTHO %s   CONTROL %s'
          % ('PASSES' if wo <= 2.0 else 'FAILS',
             'exceeds (good)' if wp > 2.0 else 'DOES NOT EXCEED (bad)'))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))
