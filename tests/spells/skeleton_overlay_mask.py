#!/usr/bin/env python
"""Gate (j), lane SKELFIX: the OVERLAY'S OWN PIXELS ARE ON THE CHARACTER.

Gates (f)-(i) live inside the application and read the overlay's own numbers.
This one reads the PICTURE bungo is handed, and it is the gate the defect was
found in: the director saw long segments fanning out of the character in
`scratchpad/skeloverlay_20260910/on_frame46.png` and no count anywhere said so.

  skeleton_overlay_mask.py OFF.png ON.png [--margin 0.05] [--out mask.png]

OFF and ON are the same camera, the same clip, the same frame, differing only
by the overlay. Then:

  * the CHARACTER's box comes from OFF -- every pixel that is not the flat
    viewport background, which with WW_RENDER_CLEAN=1 is the only other thing
    in the frame. It is measured from the picture WITHOUT the overlay, so the
    overlay cannot enlarge the box it is then judged against;
  * every pixel the overlay changed must lie inside that box grown by
    `margin` x its diagonal (5% by default -- room for a leaf bone's stub,
    which the viewport caps at twice the characteristic bone size);
  * `--out` paints the evidence: the box in dark grey, changed pixels inside
    it orange, changed pixels OUTSIDE it magenta.

Exit 0 = pass, 1 = a pixel outside the box, 2 = the inputs are unusable.
Prints `N checks, M failures` and PASS/FAIL like the in-app harnesses do.
"""
import sys

import numpy as np
from PIL import Image


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int16)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    off_path, on_path = argv[1], argv[2]
    margin_frac = 0.05
    if "--margin" in argv:
        margin_frac = float(argv[argv.index("--margin") + 1])
    out_path = argv[argv.index("--out") + 1] if "--out" in argv else None

    off, on = load(off_path), load(on_path)
    checks = fails = 0

    def check(what, ok):
        nonlocal checks, fails
        checks += 1
        if not ok:
            fails += 1
        print("  %s %s" % ("ok  " if ok else "FAIL", what))

    if off.shape != on.shape:
        print("the two renders are different sizes: %s vs %s" % (off.shape, on.shape))
        return 2
    h, w = off.shape[:2]
    print("%dx%d  off=%s  on=%s" % (w, h, off_path, on_path))

    # The background is the single most common colour of the OFF render; with
    # WW_RENDER_CLEAN=1 the frame holds the model and nothing else.
    flat = off.reshape(-1, 3)
    colours, counts = np.unique(flat, axis=0, return_counts=True)
    bg = colours[counts.argmax()]
    bgfrac = counts.max() / float(flat.shape[0])
    print("background %s covers %.1f%% of the off render" % (tuple(int(v) for v in bg), 100 * bgfrac))
    check("the off render has a flat background to measure the character against",
          bgfrac > 0.25)

    body = (np.abs(off - bg).max(axis=2) > 8)
    ys, xs = np.nonzero(body)
    check("the off render actually contains a character", body.sum() > 500)
    if body.sum() <= 500:
        print("%d checks, %d failures\nFAIL" % (checks, fails))
        return 1
    x0, x1, y0, y1 = int(xs.min()), int(xs.max()), int(ys.min()), int(ys.max())
    diag = float(np.hypot(x1 - x0, y1 - y0))
    m = int(round(margin_frac * diag))
    print("character box x %d..%d, y %d..%d (diagonal %.0f px), margin %d px"
          % (x0, x1, y0, y1, diag, m))

    changed = (np.abs(on - off).max(axis=2) > 0)
    n = int(changed.sum())
    print("pixels changed by the overlay: %d (%.3f%%)" % (n, 100.0 * n / (w * h)))
    check("FLOOR: the overlay changed some pixels at all", n > 0)

    inside = np.zeros_like(changed)
    inside[max(0, y0 - m):min(h, y1 + m + 1), max(0, x0 - m):min(w, x1 + m + 1)] = True
    stray = changed & ~inside
    ns = int(stray.sum())
    worst = 0
    if ns:
        sy, sx = np.nonzero(stray)
        worst = int(max(np.max(np.maximum(x0 - m - sx, sx - (x1 + m))),
                        np.max(np.maximum(y0 - m - sy, sy - (y1 + m)))))
        print("stray pixels: %d, furthest %d px outside the box+margin, first at (%d,%d)"
              % (ns, worst, sx[0], sy[0]))
    check("(j) every pixel the overlay drew is on the character (%d stray)" % ns, ns == 0)
    # The box must not be the whole frame, or (j) passes for free.
    boxfrac = float(inside.sum()) / (w * h)
    check("FLOOR: the box is not the whole frame (%.1f%%)" % (100 * boxfrac), boxfrac < 0.90)

    if out_path:
        img = np.zeros((h, w, 3), dtype=np.uint8)
        img[inside] = (30, 30, 34)
        img[changed & inside] = (255, 157, 0)
        img[stray] = (255, 0, 200)
        Image.fromarray(img).save(out_path)
        print("wrote %s" % out_path)

    print("%d checks, %d failures" % (checks, fails))
    print("PASS" if fails == 0 else "FAIL")
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
