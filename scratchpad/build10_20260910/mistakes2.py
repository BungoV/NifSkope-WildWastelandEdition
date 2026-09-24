#!/usr/bin/env python3
"""Lane BUILD10 -- the three mistakes lane WATER6's own first run found."""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
P = os.path.join(ROOT, "MISTAKES.md")

ANCHOR = """   (`src/lodtfile.cpp` 3096). The name IS in the file -- the harness's own
   window printed it -- and the reader cannot see it. A sentinel value that is
   also a legal value is the whole bug, and it was invisible until a test named
   exactly one body.
"""

TEXT = """
### Lane WATER6's own first run (BUILD10 again)

9. **The water window's self-test dereferenced a DELETED document, and the new
   code turned that into a segmentation fault.** `WaterWindow::openFile` does
   `delete doc; doc = new WaterMarkDoc();`, and `runWindowSelfTest` captured
   `WaterMarkDoc * doc = win->document();` once at the top and reopened the file
   at gate W3. Everything after that -- W4 first -- used a freed object. It had
   never crashed, because `sweep()` began by reading a bool and a pointer that
   happened to survive in the freed block; lane WATER6 made `sweep()` begin by
   comparing two QVector members and the harness died with **exit 139**, taking
   W4, W5, W6 and the whole check count with it. The lesson is not about the
   pointer: **a latent use-after-free is invisible until something reads a
   different field**, and "the gates were green yesterday" is not evidence that
   the memory was ever valid. Repaired in the instrument (one line, re-read the
   pointer after the reopen); no check, assertion or widget touched.
10. **Two of lane WATER6's own gates were wrong on their first run, and both
    were the gate and not the code.** X2 placed its one-point pin at
    `axis.pts[size/2]` without asking whether that point is WET -- the
    centreline is the mean position of each slice's wet texels and on a winding
    reach that mean lands on the bank, so `addStroke` refused it in words, the
    gate never read the refusal, and the solve had no strokes at all: the gate
    reported "a one-point curve is not consumed" about a pin that was never
    stored. X3b counted texels outside the raster layer whose word EQUALS the
    layer's constant, and exactly one of the river's 29,121 outside texels
    solves to that constant on its own -- a collision reported as an authority
    leak (1 of 29,121; X3c 190 of 191). Both instruments were corrected to
    measure the thing the gate was registered to measure -- the pin is placed on
    a texel that is on the river and `addStroke`'s answer is read; the layer's
    effect is measured against the LAYER-FREE SOLVE texel by texel -- and no
    threshold was moved. The first run's numbers are in the lane report.
11. **A registered gate can be body-shaped, and X2b is.** "The water round a
    one-point pin points away from it" is measured as the mean cosine between
    the flow and the outward radial over a disc of four pin widths. That reads
    **0.742 on body 2** and **0.371 on body 3**, against a registered 0.5 -- not
    because the pin behaves differently but because the Charles BENDS inside
    four widths, so the straight-line radial and the channel-following flow
    diverge geometrically. The gate was not moved; it is reported red on body 3
    with its cause, and the instrument a later lane should use instead is named
    (the net flux through a ring around the pin, which curvature cannot bias).
    Rule: a gate whose value depends on the SHAPE of the fixture is a
    measurement of the fixture until it is normalised.
"""


def main():
    b = open(P, "rb").read()
    cr = b.count(b"\r")
    a = ANCHOR.encode("utf-8")
    n = b.count(a)
    print("anchor count=%d CR=%d bytes=%d" % (n, cr, len(b)))
    assert n == 1
    assert b.count(b"Lane WATER6's own first run") == 0, "already spliced"
    t = TEXT.encode("utf-8")
    assert t.count(b"\r") == 0
    out = b.replace(a, a + t)
    assert out.count(b"\r") == cr
    open(P, "wb").write(out)
    print("wrote MISTAKES.md %d -> %d bytes, CR %d unchanged" % (len(b), len(out), cr))


if __name__ == "__main__":
    main()
