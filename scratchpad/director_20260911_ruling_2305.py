"""Director 23:05: bungo's ruling 'use vanilla normal map for those tiles, use those details for the diffuse' + the _msn channel answer -> RULING 23:0x paragraph before RULING 22:4x; EROSION1 note updated (vanilla copy is the default, erosion where no sheet). LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
anchor = b"  RULING 22:4x (bungo, over the director's cmp_msn_2024.png -- vanilla's\n"
assert b.count(anchor) == 1
ruling = b"""  RULING 23:0x (bungo, after RULING 22:4x): "For now, I think we can use
  vanilla normal map for those tiles, use those details for the
  diffuse" -> SHIPPED DEFAULT for a chunk with a vanilla sheet:
  `--land-detail-source vanilla` -- vanilla's _msn fine detail over our
  coarse normal (== vanilla's bytes where our coarse terrain agrees,
  guarded), and the colour's fine detail = the shading of that normal
  detail. Sent to TILING3 at 23:05 (its deliverable regardless of A/B/C's
  verdict); chunks with no vanilla sheet keep the rung until EROSION1.
  He asked what the green channel holds; director measured 23:05 on
  chunk -20,24 (scratchpad msn_channels.py): all three channels signed,
  |n| = 1.02 sd 0.07; R = east-west tilt, G = UP (255 flat, mean 242.5
  -> z 0.90), B = north-south tilt (mean 99.7 -> -0.22; ours -0.15, the
  same bias, so convention). Our writer already matches: coarse 8x8
  correlation vanilla vs ours R-R 0.94, G-G 0.76, B-B 0.93.
"""
b = b.replace(anchor, ruling + anchor)
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF: RULING 23:0x inserted")

p = R + "scratchpad/brief_erosion1.md"
t = open(p, "rb").read()
old = b"## The ruling this lane serves\n"
assert t.count(old) == 1
t = t.replace(old, old + b"RULING 23:0x (bungo): \"For now, I think we can use vanilla normal map for those tiles, use those details for the diffuse\" -> TILING3 ships vanilla's copy as the default where a sheet exists; this lane is the source for chunks WITHOUT a vanilla sheet (and the knob everywhere when asked). Do not touch the vanilla-copy path.\n")
open(p, "wb").write(t)
print("brief_erosion1: ruling noted")
