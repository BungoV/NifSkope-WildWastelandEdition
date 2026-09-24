#!/usr/bin/env python
# The refuter for the leg (h) fix: two synthetic records that differ ONLY on
# lines the mask covers (the `baked` value and the `peak working set:` clause),
# and a third pair that differs on a census line the mask does NOT cover.
#
# Run it BEFORE the patch: pair 1 must go red (that is the bug) and pair 2 red.
# Run it AFTER:            pair 1 must go green and pair 2 must STILL go red,
# or the fix is a sweep rather than a floor.
#
#   python p9_refuter.py <tests/spells dir> <work dir>
import io
import os
import sys

LF = chr(10)
TAB = chr(9)

CENSUS = ("census" + TAB + "bake census: threads 16, chunk jobs 9, "
          "peak working set: %s, 9 chunk(s), 0 dropped")


def rec(baked, peak, merged):
    return LF.join([
        "lodb" + TAB + "2" + TAB + "Commonwealth" + TAB + "1.2.3" + TAB + "22534144",
        "baked" + TAB + baked,
        "chunk" + TAB + "4" + TAB + "-24" + TAB + "16" + TAB + "deadbeef",
        "census" + TAB + "stage times: landscape 0.0 s, meshes %s s" % merged,
        CENSUS % peak,
        "census" + TAB + "merged: 124 shapes -> 121 across 9 chunks",
        "out" + TAB + "Commonwealth.4.-24.16.lodj" + TAB + "abc123",
        "",
    ])


def main():
    spells, work = sys.argv[1], sys.argv[2]
    sys.path.insert(0, spells)
    import lodgen_bakerec_gate as g
    if not os.path.isdir(work):
        os.makedirs(work)
    pairs = [
        ("PAIR 1 -- differs only where the mask reaches (must be GREEN after the fix)",
         rec("2026-09-17T07:00:00", "2.28 GB (2449879040 bytes)", "2.5"),
         rec("2026-09-17T07:40:00", "2.31 GB (2481000448 bytes)", "2.9")),
        ("PAIR 2 -- a census line the mask does NOT cover (must stay RED)",
         rec("2026-09-17T07:00:00", "2.28 GB (2449879040 bytes)", "2.5"),
         rec("2026-09-17T07:40:00", "2.28 GB (2449879040 bytes)", "2.5")
         .replace("merged: 124 shapes -> 121", "merged: 124 shapes -> 118")),
    ]
    bad = 0
    for i, (name, a, b) in enumerate(pairs):
        pa = os.path.join(work, "h_%d_a.lodb" % i)
        pb = os.path.join(work, "h_%d_b.lodb" % i)
        io.open(pa, "wb").write(a.encode("utf-8"))
        io.open(pb, "wb").write(b.encode("utf-8"))
        sys.stdout.write(LF + "== " + name + LF)
        rc = g.cmd_identical([pa, pb])
        sys.stdout.write("   rc=%d%s" % (rc, LF))
        want = 0 if i == 0 else 1
        if rc != want:
            bad += 1
    sys.stdout.write(LF + ("REFUTER: %d pair(s) off expectation" % bad) + LF)
    return 0


if __name__ == "__main__":
    sys.exit(main())
