"""Director 23:1x: bungo's correction -- reuse vanilla's _msn bytes outright when toggled, our normal bake skipped for those chunks. Appended to RULING 23:0x. LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
anchor = b"  correlation vanilla vs ours R-R 0.94, G-G 0.76, B-B 0.93.\n"
assert b.count(anchor) == 1
add = b"""  + 23:1x bungo: "so now we do not use our own normal map if that is
  toggled, but reuse these ones for terrain chunks" -> the default is
  NOT a blend: a chunk with a vanilla _msn gets vanilla's sheet byte for
  byte, our normal bake skipped for it; chunks without one get ours; the
  guarded composite survives only as `--land-detail-source vanilla-blend`
  (not default). Sent to TILING3 23:1x; gate = cmp == vanilla per chunk.
"""
b = b.replace(anchor, anchor + add)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF: 23:1x correction appended")
