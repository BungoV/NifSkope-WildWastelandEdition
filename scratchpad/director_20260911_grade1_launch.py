"""Director 00:0x: TILING3 landed -> LIVE NOW = GRADE1; queue reordered (EROSION1 after TERRAINFMT1: every Commonwealth chunk has a vanilla sheet); fill GRADE1's exe line. LF-only, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
s = b.find(b"  LIVE NOW: lane TILING3 (Opus, Agent tool, launched 22:2x, markers")
e_marker = b"  none held the exe at the last check.\n"
e = b.find(e_marker, s); assert s > 0 and e > s; e += len(e_marker)
new_live = b"""  LIVE NOW: lane GRADE1 (Opus, Agent tool, launched 2026-09-12 00:0x,
  markers scratchpad/grade1_20260911/BUILDING; brief
  scratchpad/brief_grade1.md; exe at its launch = TILING3's DONE exe
  release/NifSkope.exe 2026-09-11 23:26:29 21,484,032 B). TILING3 DONE
  23:57, spliced 00:0x (WW_CHANGES + MISTAKES +4 + HANDOFF block below;
  skills module-off-is-identical / vanilla-compare /
  explained-variance-ceiling mirrored, cmp clean; DEFAULT CHANGED:
  `--land-detail-source vanilla` -- vanilla _msn byte for byte where it
  exists, vanilla colour byte for byte on layerless chunks, crevice term
  `--land-shade -3.242` on painted ground; `none` == old bytes; repeat
  fix `--land-sample stochastic` present, OFF, 6 of 7). Still red:
  the repeat on painted ground (shipped 2.055 vs vanilla 0.456 on the
  crop). DIRECTOR DECISION 00:0x: EROSION1 moved AFTER TERRAINFMT1 --
  TILING3's census found 0 chunks anywhere without a vanilla sheet
  (180-sheet region 121 layered / 59 layerless / 0 without), so grown
  detail matters only outside the Commonwealth; bungo may pull it
  forward. bungo is denoising + 4x-upscaling all 2304 vanilla _msn
  sheets himself (Downloads/vanillachunks, normal = R e-w / G n-s / B
  up, slope = G alone); ingesting that cache = TERRAINFMT1's item.
  bungo's window: none held the exe at the last check. Game down.
"""
b = b[:s] + new_live + b[e:]
old_q = b"""  three hypotheses), EROSION1 (brief_erosion1.md, added 23:0x on his
  judgement of cmp_msn_2024.png: "We lose all the fluvial, erosion
  features" -- grown erosion detail for the _msn and the colour, before
  GRADE1 so the tone fit sees the finished ground), GRADE1
  (brief_grade1.md), ROADS3\n"""
assert b.count(old_q) == 1
new_q = b"""  three hypotheses; DONE 23:57), GRADE1 (brief_grade1.md), ROADS3\n"""
b = b.replace(old_q, new_q)
old_t = b"  (brief_terrainfmt1.md), then the director's full-module Sanctuary bake\n"
assert b.count(old_t) == 1
b = b.replace(old_t, b"""  (brief_terrainfmt1.md; + ingest bungo's cleaned 2K _msn cache, BC7 or
  uncompressed), EROSION1 (brief_erosion1.md, added 23:0x on "We lose
  all the fluvial, erosion features"; moved here 00:0x -- no Commonwealth
  chunk lacks a vanilla sheet), then the director's full-module Sanctuary bake
""")
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF live paragraph + queue updated")

p = R + "scratchpad/brief_grade1.md"
t = open(p, "rb").read()
old = b"Exe at launch: from ROADS2's DONE line (director fills: ______)."
assert t.count(old) == 1
t = t.replace(old, b"Exe at launch: `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 B (TILING3's DONE exe; chain baseline = TILING2's: lodl_open 23/0, terrain 26/0, terrain_vt 41/1, roads 11/0, ground_cover 29/5, pbrm 14/0, native 18/0, panel_run 125/0, lod_generation 116/0, ui_align 11/0, water_ui 82/0). NOTE: since ROADS2 this brief was written, TILING2 (four opt-in flags, `--blend-edges quadrant`) and TILING3 (DEFAULT `--land-detail-source vanilla`: vanilla _msn byte for byte, vanilla colour on layerless chunks, crevice term `--land-shade -3.242` on painted ground) landed -- read both HANDOFF blocks and reports first; the tone fit is on TILING3's default colour, and the copied vanilla sheets must stay byte-identical through any tone change (gate it).")
open(p, "wb").write(t)
print("brief_grade1 exe filled")
