#!/usr/bin/env python3
"""Lane BUILD8: the per-tile difference between two frame sets.

  python diff_tiles.py <prefixA> <prefixB> [--out diff.png]

For each of the six tags frames.sh writes, the two PNGs are compared pixel for
pixel: identical or not, the count of differing pixels, the worst per-channel
difference, and the bounding box of everything that differs.  A DIFFERENCE
PICTURE is written per tag when there is one (16x amplified, so a single-level
difference is visible), and the amplification is stated in the caption -- never
a raw subtract that shows black and gets reported as "no difference".

The FLOOR: the bind tile is rendered from NO clip on both sides, so it must
come out identical; a run where even that tile differs is measuring the
renderer, not the round trip, and says so.
"""
import os
import sys

from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, "images")
TAGS = ["bind", "f0", "fq1", "fhalf", "fq3", "flast"]


def main():
    pa, pb = sys.argv[1], sys.argv[2]
    rows = []
    worst_any = 0
    for tag in TAGS:
        a_p = os.path.join(IMG, "%s_%s.png" % (pa, tag))
        b_p = os.path.join(IMG, "%s_%s.png" % (pb, tag))
        if not (os.path.exists(a_p) and os.path.exists(b_p)):
            print("MISSING %s / %s" % (a_p, b_p))
            return 2
        a = Image.open(a_p).convert("RGB")
        b = Image.open(b_p).convert("RGB")
        if a.size != b.size:
            print("%-6s SIZE MISMATCH %s vs %s" % (tag, a.size, b.size))
            return 2
        d = ImageChops.difference(a, b)
        bbox = d.getbbox()
        # PER CHANNEL, never through convert("L"): a difference of (0,0,1)
        # weights to 0.114 and ROUNDS TO ZERO in luminance, so an L-based count
        # reports "identical" on a tile whose own bounding box is not empty --
        # measured on this very run, lane BUILD8, 2026-09-10.
        bands = d.split()
        worst = max(bd.getextrema()[1] for bd in bands)
        data = list(zip(*[bd.getdata() for bd in bands]))
        px = sum(1 for p in data if p[0] or p[1] or p[2])
        total = a.size[0] * a.size[1]
        worst_any = max(worst_any, worst)
        rows.append((tag, px, total, worst, bbox))
        verdict = "IDENTICAL" if px == 0 else "differs"
        print("%-6s %-9s  %d of %d pixels differ (%.5f%%), worst channel step %d, bbox %s"
              % (tag, verdict, px, total, 100.0 * px / total, worst, bbox))
        if px:
            amp = d.point(lambda v: min(255, v * 16))
            out = os.path.join(IMG, "diff_%s_%s_%s.png" % (pa, pb, tag))
            amp.save(out)
            print("       amplified x16 -> %s" % out)
    ident = all(r[1] == 0 for r in rows)
    print("\nVERDICT: %s (worst channel step over all six tiles: %d)"
          % ("the two sheets are PIXEL-IDENTICAL" if ident
             else "the sheets differ; every difference is quantified above", worst_any))
    bind = [r for r in rows if r[0] == "bind"][0]
    print("FLOOR: the bind tile (no clip on either side) %s"
          % ("is identical, as it must be" if bind[1] == 0
             else "DIFFERS -- this run is measuring the renderer, not the round trip"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
