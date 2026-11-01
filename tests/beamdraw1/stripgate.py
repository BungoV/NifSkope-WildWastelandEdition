"""Strip-continuity gate for a rendered beam (BEAMDRAW1).
usage: python -I stripgate.py [--expect pass|fail] <png> [<png> ...]
Beam pixels = red-dominant (R - max(G, B) > THR). The beam axis is the principal axis of those pixels.
Pixels are projected onto it in 1 px bins between the first and last lit bin; a gap is a run of empty bins.
PASS = gap fraction <= MAXFRAC and longest gap <= MAXGAP px. Prints one line per picture; rc 1 if any
picture disagrees with --expect (default pass)."""
import sys

import numpy as np
from PIL import Image

THR, MAXFRAC, MAXGAP = 12, 0.03, 6

args = sys.argv[1:]
expect = 'pass'
if args[:1] == ['--expect']:
    expect, args = args[1], args[2:]
bad = 0
for p in args:
    a = np.asarray(Image.open(p).convert('RGB')).astype(np.int32)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    m = (r - np.maximum(g, b)) > THR
    ys, xs = np.nonzero(m)
    if len(xs) < 20:
        print('NOBEAM %s lit px %d' % (p, len(xs)))
        bad += 1
        continue
    pts = np.stack([xs, ys], 1).astype(np.float64)
    c = pts.mean(0)
    w, v = np.linalg.eigh(np.cov((pts - c).T))
    ax = v[:, np.argmax(w)]
    t = (pts - c) @ ax
    t0 = int(np.floor(t.min()))
    bins = np.zeros(int(np.ceil(t.max())) - t0 + 1, bool)
    bins[(np.floor(t) - t0).astype(int)] = True
    span = len(bins)
    gaps, run = [], 0
    for on in bins:
        if on:
            if run:
                gaps.append(run)
            run = 0
        else:
            run += 1
    frac = sum(gaps) / span
    longest = max(gaps) if gaps else 0
    ok = frac <= MAXFRAC and longest <= MAXGAP
    verdict = 'PASS' if ok else 'FAIL'
    if verdict.lower() != expect:
        bad += 1
    print('%s %s span %d px, gaps %d, gap px %d (%.1f%%), longest %d, lit px %d (expect %s)' % (
        verdict, p, span, len(gaps), sum(gaps), 100 * frac, longest, len(xs), expect))
sys.exit(1 if bad else 0)
