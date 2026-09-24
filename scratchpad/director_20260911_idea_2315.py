"""Director 23:1x: bungo's idea -- erosion for the out-of-bounds areas -> parked as candidate lane HORIZON1 after EROSION1. LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
anchor = b"  RULING 22:4x (bungo, over the director's cmp_msn_2024.png -- vanilla's\n"
assert b.count(anchor) == 1
add = b"""  IDEA 23:1x (bungo): "so, if we had erosion simulation to add those
  details in, we could also use that to do something with the out of
  bounds areas..." -> two jobs. (a) Out-of-bounds cells that HAVE LAND
  records: EROSION1's pass covers them as-is (no vanilla sheet there, so
  they are its chunks). (b) Beyond the last LAND record: grow terrain
  (continue the border slopes, ridges at hill scale, erode, bake meshes
  + sheets for chunks the game never shipped) = candidate lane HORIZON1,
  parked after EROSION1, NOT chartered. Its first step is an engine
  test, not code: place one chunk one step past the worldspace extent
  and see whether the game draws it (LOD loads by the chunk filename
  grid around the player; the worldspace's stored extent may clip it).
  Nothing is built on it before that test says yes.
"""
b = b.replace(anchor, add + anchor)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF: IDEA 23:1x parked")
