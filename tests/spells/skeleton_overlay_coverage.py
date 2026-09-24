#!/usr/bin/env python
"""Gate S2, lane SKEL2: THE WAY BACK IS EXACT IN GEOMETRY, AND ONLY THE COLOUR MOVED.

  skeleton_overlay_coverage.py RUNG_OFF RUNG_ON NEW_OFF NEW_ON [--bar 64] [--out cmp.png]

The brief asked for `Stick` to reproduce the shipped framebuffer BYTE FOR BYTE.
That is impossible by construction and the reason is bungo's own later ruling:
he asked for the bones to be blue, and the overlay that shipped drew them in the
palette's `text`, `accent` and `textMuted`. Every overlay pixel therefore changes
colour, whatever the shape does.

So the way back is proved on the thing the colour ruling did NOT license to
move: WHICH PIXELS THE OVERLAY COVERS. For each pair, the coverage is the set of
pixels where the ON render differs from the OFF render at the same camera. If
`Wire` draws what the old overlay drew, the two coverage sets are the same set
-- the bones are in the same places, the same thickness, the same collar, the
same joint dots -- and only their colour differs.

  * `--bar` is the jitter tolerance measured on the rung (five identical runs:
    (c) 16 / 13 / 14 / 9 / 1, (e) 12 / 22 / 19 / 15 / 21; worst on record 37).
    It is the same 64 the in-app gate uses.
  * FLOORS, both printed: the coverage must not be empty, and the two ON renders
    must actually DIFFER (they are different colours, so a zero there would mean
    the new picture is the old one and nothing was rebuilt).

`--out` paints the evidence: grey where both cover, orange where only the rung
covered, magenta where only the new build covers.

Exit 0 = pass, 1 = a check failed, 2 = the inputs are unusable.
"""
import sys

import numpy as np
from PIL import Image


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int16)


def main(argv):
    if len(argv) < 5:
        print(__doc__)
        return 2
    rung_off, rung_on, new_off, new_on = argv[1:5]
    bar = int(argv[argv.index("--bar") + 1]) if "--bar" in argv else 64
    out_path = argv[argv.index("--out") + 1] if "--out" in argv else None

    imgs = [load(p) for p in (rung_off, rung_on, new_off, new_on)]
    if len({i.shape for i in imgs}) != 1:
        print("FAIL: the four renders are not the same size: %s"
              % [i.shape for i in imgs])
        return 2
    ro, rn, no, nn = imgs
    h, w = ro.shape[:2]
    print("%dx%d  rung=%s/%s  new=%s/%s" % (w, h, rung_off, rung_on, new_off, new_on))

    checks = fails = 0

    def check(what, ok):
        nonlocal checks, fails
        checks += 1
        if not ok:
            fails += 1
        print("  %s %s" % ("ok  " if ok else "FAIL", what))

    cov_rung = np.abs(rn - ro).sum(axis=2) > 0
    cov_new = np.abs(nn - no).sum(axis=2) > 0
    n_rung, n_new = int(cov_rung.sum()), int(cov_new.sum())
    only_rung = int((cov_rung & ~cov_new).sum())
    only_new = int((cov_new & ~cov_rung).sum())
    both = int((cov_rung & cov_new).sum())
    print("coverage: rung %d px, new %d px, shared %d, only-rung %d, only-new %d"
          % (n_rung, n_new, both, only_rung, only_new))

    check("FLOOR: the rung's overlay covered something (%d px)" % n_rung, n_rung > 1000)
    check("FLOOR: the new overlay covers something (%d px)" % n_new, n_new > 1000)

    on_diff = int((np.abs(nn - rn).sum(axis=2) > 0).sum())
    print("the two ON renders differ in %d px (the colour ruling)" % on_diff)
    check("FLOOR: the two ON renders are different pictures (%d px)" % on_diff,
          on_diff > bar)

    check("S2 the way back covers the same pixels (only-rung %d, only-new %d, bar %d)"
          % (only_rung, only_new, bar),
          only_rung <= bar and only_new <= bar)

    if out_path:
        img = np.zeros((h, w, 3), dtype=np.uint8)
        img[cov_rung & cov_new] = (120, 120, 120)
        img[cov_rung & ~cov_new] = (255, 157, 0)
        img[cov_new & ~cov_rung] = (255, 0, 255)
        Image.fromarray(img).save(out_path)
        print("wrote %s" % out_path)

    print("%d checks, %d failures" % (checks, fails))
    print("PASS" if fails == 0 else "FAIL")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
