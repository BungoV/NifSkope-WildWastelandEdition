"""Director: queue TILING2 ahead of GRADE1 and record bungo's 19:2x ruling in the HANDOFF top block (LF-only, CR asserted 0)."""
P = "E:/Projects/NifskopeWildWastelandEdition/HANDOFF.md"
b = open(P, "rb").read()
assert b.count(b"\r") == 0

old_q = b"""  is done. QUEUE AFTER RESUME3 (briefs written): ROADS2
  (brief_roads2.md), GRADE1 (brief_grade1.md), INCR1 (brief_incr1.md),"""
new_q = b"""  is done. QUEUE AFTER RESUME3 (briefs written): ROADS2
  (brief_roads2.md), TILING2 (brief_tiling2.md, added 19:2x on his
  judgement of cmp_tiling_fixed.png -- BEFORE GRADE1 because GRADE1 fits
  its tone curve on this lane's colour), GRADE1 (brief_grade1.md), INCR1 (brief_incr1.md),"""
assert b.count(old_q) == 1
b = b.replace(old_q, new_q)

anchor = b"  STANDING INSTRUCTION 16:38: run the queue to the end, send every\n"
assert b.count(anchor) == 1
ruling = b"""  RULING 19:2x (bungo, over the "OURS default 341.3333" panel of
  scratchpad/resume3_20260911/images/cmp_tiling_fixed.png, the newest
  bake, exe 19:08:42): "Yes, you can see the tiling pattern of each
  texture, which is not good, hard blend edges also appear in some
  places, and yeah, it's muddy or blurry looking" -> lane TILING2
  (brief_tiling2.md): measure vanilla's periodicity / edge width /
  detail spectrum first, then the repeat-averaged land sample, the edge
  blend (quadrant border suspected), the detail term from wherever it
  correlates -- all behind switches, off == rung bytes; tone stays
  GRADE1's, the constant stays RESUME3's.
"""
b = b.replace(anchor, ruling + anchor)
assert b.count(b"\r") == 0
open(P, "wb").write(b)
print("ok", len(b))
