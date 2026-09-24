"""Director 23:3x: bungo cleaned vanilla's DXT5 _msn blocking with a BC1-artifact ESRGAN model (before/after looked right) -> note for TERRAINFMT1: the gain survives only in a format that does not re-block. LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
anchor = b"  RULING 23:1x (bungo): \"out of bounds terrain blends are not included\n"
assert b.count(anchor) == 1
add = b"""  NOTE 23:3x: vanilla's sheets are DXT5 (BC3), 512^2, 10 mips (header
  read 23:1x); the colour block is BC1's 5:6:5, so the north-south slope
  in B (5 bits, far from midpoint) is visibly blocky. bungo ran a BC1
  artifact-removal ESRGAN model (OpenModelDB "1x_BC1" family) on the
  chunk -20,24 _msn and judged the before/after good: blocks gone, rills
  kept. FOR TERRAINFMT1: a decode -> clean -> re-encode path for the
  copied vanilla _msn keeps that gain only in BC7 or uncompressed
  (DXT5 re-encode brings the blocks back); renormalise after cleaning
  (recompute G from R,B). Candidate item, his call; measure the cleaned
  sheet (unit length, channel means, coarse agreement, fine energy at
  the 4-texel scale) before shipping any model output.
"""
b = b.replace(anchor, add + anchor)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF: NOTE 23:3x inserted")
