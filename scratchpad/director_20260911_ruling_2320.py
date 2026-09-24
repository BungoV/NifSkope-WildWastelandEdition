"""Director 23:1x: bungo -- out-of-bounds cells carry no texture layers; vanilla's colour there was baked outside FO4 and cannot be recovered. -> HANDOFF ruling + EROSION1 brief: colour term for layerless cells = grown material assignment. LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
anchor = b"  IDEA 23:1x (bungo): \"so, if we had erosion simulation to add those\n"
assert b.count(anchor) == 1
add = b"""  RULING 23:1x (bungo): "out of bounds terrain blends are not included
  in the actual cells out of bounds, they never were, so we can't
  recover the color data anymore, because it was baked in a different
  tool outside of fo4" -> layerless cells (heights only) have no colour
  to bake from. TILING3 told 23:1x: for a chunk with no land-texture
  layers, reuse vanilla's COLOUR sheet byte for byte too (same rule as
  the _msn); detail-over-ours only where layers exist; count chunks per
  class. EROSION1's brief: for layerless chunks with no vanilla sheet the
  colour is a material assignment grown from the pass (rock on steep /
  scoured, sediment on deposits, the flat default between), fitted to
  vanilla's own out-of-bounds sheets as the reference.
"""
b = b.replace(anchor, add + anchor)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF: RULING 23:1x (layerless cells) inserted")

p = R + "scratchpad/brief_erosion1.md"
t = open(p, "rb").read()
old = b"## What is known going in\n"
assert t.count(old) == 1
t = t.replace(old, old + b"- RULING 23:1x (bungo): out-of-bounds cells carry heights and NO land-texture layers; vanilla's colour there was baked in an outside tool and cannot be recovered. So for a layerless chunk with no vanilla sheet, this lane's colour is not \"ours plus detail\": it is a MATERIAL ASSIGNMENT grown from the pass -- rock on steep and scoured ground, sediment on deposits, the flat default between -- with its palette and its slope/flow thresholds fitted to vanilla's own out-of-bounds sheets (TILING3's report names which of the 22 sheets are layerless; use those as the reference, with the floors). Layerless chunks that HAVE a vanilla sheet are TILING3's (vanilla's colour copied byte for byte) and stay untouched at the default.\n")
open(p, "wb").write(t)
print("brief_erosion1: layerless colour term added")
