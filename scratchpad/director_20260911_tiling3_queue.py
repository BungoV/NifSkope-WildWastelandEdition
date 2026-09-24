"""Director 22:2x: bungo's ruling on TILING2's picture -> lane TILING3 queued right after TILING2 (before GRADE1); ruling paragraph. LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
old_q = b"  its tone curve on this lane's colour), GRADE1 (brief_grade1.md), ROADS3"
new_q = b"""  its tone curve on this lane's colour), TILING3 (brief_tiling3.md, added
  22:2x on his judgement of TILING2's picture: the averaged ground is
  "solid color blobs", the footprint ground is the repeat, neither
  ships; vanilla's grain by spectrum + moments, not fixed-phase
  correlation; stochastic tiling / Noise.dds / resampled bake as the
  three hypotheses), GRADE1 (brief_grade1.md), ROADS3"""
assert b.count(old_q) == 1
b = b.replace(old_q, new_q)
anchor = b"  RULING 21:1x (bungo, over scratchpad/roads2_20260911/images/\n"
assert b.count(anchor) == 1
ruling = b"""  RULING 22:1x (bungo, over scratchpad/tiling2_20260911/images/
  cmp_tiling2.png, the green panel "--land-sample average --blend-edges
  quadrant"): "Is green "ours" the final one? because it lost all the
  texture to it, now it's only solid color blobs" -> NOT final. TILING2
  keeps `footprint` as the default (the rung's bytes) and proved with a
  validated instrument that vanilla's fine detail correlates with no
  fixed-phase candidate (land textures at any mip, VCLR, slope, shading
  all r = 0.000 with the alignment control at 0.79). Director's reading:
  that is the signature of the SAME textures with the phase broken
  (stochastic tiling), or a noise term (vanilla ships
  Textures/Terrain/Noise.dds, 1024^2 DXT1, 11 mips -- no engine setting
  name found in a two-minute grep), or a finer bake resampled. Lane
  TILING3 tests the three by spectrum and moments, ships the winner
  behind `--land-sample stochastic` (deterministic, seeded by world
  texel) or `--land-noise`, gates repeat AND grain on the same bake.
"""
b = b.replace(anchor, ruling + anchor)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF ok")
