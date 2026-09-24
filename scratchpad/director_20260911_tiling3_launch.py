"""Director: TILING2 landed -> LIVE NOW = TILING3; fill TILING3's exe line. LF-only files, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
s = b.find(b"  LIVE NOW: lane TILING2 (Opus, Agent tool, launched 21:0x, markers")
e_marker = b"  exe at the last check.\n"
e = b.find(e_marker, s)
assert s > 0 and e > s
e += len(e_marker)
new_live = b"""  LIVE NOW: lane TILING3 (Opus, Agent tool, launched 22:2x, markers
  scratchpad/tiling3_20260911/BUILDING; brief scratchpad/brief_tiling3.md;
  exe at its launch = TILING2's DONE exe release/NifSkope.exe 21:52:22
  21,466,624 B). TILING2 DONE 22:22, spliced 22:24 (WW_CHANGES + MISTAKES
  +6 + HANDOFF block below; skills lodgen / control-calibration /
  texel-picture already mirrored; NO default changed -- four opt-in
  flags; bungo judged its picture 22:1x -> TILING3). ROADS2 landed
  21:05, RESUME3 19:34. No other lane alive. Game down. bungo's window:
  none held the exe at the last check.
"""
b = b[:s] + new_live + b[e:]
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF live paragraph replaced")

p = R + "scratchpad/brief_tiling3.md"
t = open(p, "rb").read()
old = b"Exe at launch: from TILING2's DONE line (director fills: ______)."
assert t.count(old) == 1
t = t.replace(old, b"Exe at launch: `release/NifSkope.exe` 2026-09-11 21:52:22, 21,466,624 B (TILING2's DONE exe; its chain baseline = ROADS2's: lodl_open 23/0, terrain 26/0, terrain_vt 41/1, roads 11/0, ground_cover 29/5, pbrm 14/0, native 18/0, panel_run 125/0, lod_generation 116/0, ui_align 11/0, water_ui 82/0).")
open(p, "wb").write(t)
print("brief_tiling3 exe filled")
