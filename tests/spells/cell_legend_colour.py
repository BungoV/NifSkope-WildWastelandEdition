#!/usr/bin/env python
"""DOES THE LEGEND PRINT THE COLOUR THE PICTURE ACTUALLY DRAWS?

  cell_legend_colour.py PLAIN.png OVERLAY.png OVERLAY.notes [--tol 0.08]

Lane CELLVIEW3, 2026-09-19. The cell view's overlay legend used to be written by
a SECOND code path from the one that drew the pixels: the draw site wrote a flat
grey 0.35 for the "unknown" bucket while the legend called overlayColour() on the
same key and printed mauve 0.60,0.21,0.37. Both lines were green in every gate,
because no gate had ever looked at the picture and the legend together.

THE MEASUREMENT. The overlay is a VERTEX COLOUR: the shape keeps its own texture
and the overlay multiplies it. So the overlay colour is not a pixel value to look
for -- it is a RATIO between two renders of the same cell from the same camera,
one with the overlay off and one with it on:

    ratio = overlay_pixel / plain_pixel        per channel, per pixel

over the pixels the overlay changed. Lighting, texture and tone mapping are the
same in both frames and cancel. The per-channel MEDIAN of that ratio is the
colour the overlay actually drew over the majority of the drawn placements, and
it is compared against the rgb the notes file PRINTED for the legend key with the
most placements.

It is a real red/green pair without needing a second exe: if the legend prints a
colour the draw site does not use, the distance is large and the row fails --
which is exactly what it does on the pre-2026-09-19 code, where the printed mauve
is 0.29 away from the drawn grey.

Exit 0 = the printed colour of the busiest key matches what was drawn.
Exit 1 = it does not. Exit 2 = bad inputs (never counted as a pass).
"""
import re
import sys

import numpy as np
from PIL import Image


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.float64)


def legend(notes_path):
    """[(key, placements, (r,g,b), label)] in the order the notes printed them."""
    rows = []
    pat = re.compile(
        r"key (0x[0-9a-f]+)\s+(\d+) placements\s+rgb "
        r"([0-9.]+),([0-9.]+),([0-9.]+)\s*(.*)$")
    with open(notes_path, "r", errors="replace") as f:
        for line in f:
            m = pat.search(line.strip())
            if m:
                rows.append((m.group(1), int(m.group(2)),
                             (float(m.group(3)), float(m.group(4)), float(m.group(5))),
                             m.group(6).strip()))
    return rows


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    tol = 0.08
    if "--tol" in argv:
        tol = float(argv[argv.index("--tol") + 1])
    plain, over, notes = argv[1], argv[2], argv[3]

    rows = legend(notes)
    if not rows:
        print("REFUSED: %s carries no legend lines" % notes)
        return 2
    a, b = load(plain), load(over)
    if a.shape != b.shape:
        print("REFUSED: %s is %s and %s is %s" % (plain, a.shape, over, b.shape))
        return 2

    # the pixels the overlay changed, with enough signal in the plain frame for a
    # ratio to mean anything (a near-black pixel divides into noise)
    diff = np.abs(a - b).max(axis=2)
    lit = a.min(axis=2) > 24.0
    mask = (diff > 6.0) & lit
    n = int(mask.sum())
    print("changed, lit pixels: %d of %d" % (n, a.shape[0] * a.shape[1]))
    if n < 500:
        print("REFUSED: too few changed pixels to measure a ratio")
        return 2

    ratio = b[mask] / np.maximum(a[mask], 1.0)
    med = np.median(ratio, axis=0)
    print("measured drawn colour (median ratio): %.3f,%.3f,%.3f" % tuple(med))

    rows.sort(key=lambda r: -r[1])
    top = rows[0]
    print("busiest legend key %s: %d placements, printed rgb %.2f,%.2f,%.2f  %s"
          % (top[0], top[1], top[2][0], top[2][1], top[2][2], top[3]))
    for k, p, rgb, lab in rows[:8]:
        d = float(np.linalg.norm(med - np.array(rgb)))
        print("  key %-14s %5d placements  printed %.2f,%.2f,%.2f  distance %.3f  %s"
              % (k, p, rgb[0], rgb[1], rgb[2], d, lab))

    d = float(np.linalg.norm(med - np.array(top[2])))
    # the colour the pre-repair legend printed for the grey sentinel, kept here by
    # value so the control does not need that build to exist
    old = np.array([0.60, 0.21, 0.37])
    print("distance to the busiest key's printed rgb: %.3f (tolerance %.2f)" % (d, tol))
    print("distance to the PRE-REPAIR printed mauve 0.60,0.21,0.37: %.3f"
          % float(np.linalg.norm(med - old)))
    if d <= tol:
        print("PASS the legend prints the colour the picture draws")
        return 0
    print("FAIL the legend prints a colour the picture does not draw")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
