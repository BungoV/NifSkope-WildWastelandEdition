"""Director 23:0x: bungo's ruling over cmp_msn_2024.png -> RULING 22:4x paragraph + EROSION1 in the queue (after TILING3, before GRADE1). LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0

anchor = b"  RULING 21:1x (bungo, over scratchpad/roads2_20260911/images/\n"
assert b.count(anchor) == 1
ruling = b"""  RULING 22:4x (bungo, over the director's cmp_msn_2024.png -- vanilla's
  _msn vs ours for chunk (-20,24), local variance 88.15 vs 5.96, ours
  baked from the LAND heightmap): "We lose all the fluvial, erosion
  features and other topographical features" -> the far sheets must
  carry them again. Director's answer (23:0x): the loaded cells never had
  them either (same 128-u grid); Bethesda's unshipped source terrain
  exists only in the far sheets. Two sources, both wanted: (1) vanilla's
  own copy where a shipped sheet exists and our coarse terrain still
  agrees with it (TILING3's `--land-detail-source vanilla`, guarded);
  (2) GROWN: lane EROSION1 (brief_erosion1.md) = a deterministic
  hydraulic erosion pass on our heightfield at bake resolution, seeded by
  world position, byte-identical 1 vs 16 threads and single-chunk vs
  region, feeding the _msn writer and the colour shading; fitted to
  vanilla's measured erosion statistics (rill spacing, flow coherence
  with the coarse downslope, amplitude vs slope) with floors; `--erosion
  <strength>` 0 == rung bytes; the knob may be pushed past vanilla.
  Queued right after TILING3, before GRADE1, so the tone fit sees the
  finished ground. Bears on TERRAINFMT1's _msn plan: blending land-texture
  normals adds grain, not geology -- amend that brief at its launch.
"""
b = b.replace(anchor, ruling + anchor)

old_q = b"  three hypotheses), GRADE1 (brief_grade1.md), ROADS3\n"
assert b.count(old_q) == 1
new_q = b"""  three hypotheses), EROSION1 (brief_erosion1.md, added 23:0x on his
  judgement of cmp_msn_2024.png: "We lose all the fluvial, erosion
  features" -- grown erosion detail for the _msn and the colour, before
  GRADE1 so the tone fit sees the finished ground), GRADE1
  (brief_grade1.md), ROADS3\n"""
b = b.replace(old_q, new_q)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF: RULING 22:4x inserted, EROSION1 queued")
