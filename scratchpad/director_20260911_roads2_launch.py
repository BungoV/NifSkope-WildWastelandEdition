"""Director: RESUME3 landed -> LIVE NOW = ROADS2; fill ROADS2's exe line. LF-only files, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
import re
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
s = b.find(b"  LIVE NOW: lane RESUME3 (Opus, Agent tool, launched 16:44, markers")
e = b.find(b"  alive. Game down. bungo's window: none held the exe at the last check.\n", s)
assert s > 0 and e > s
e += len(b"  alive. Game down. bungo's window: none held the exe at the last check.\n")
new_live = b"""  LIVE NOW: lane ROADS2 (Opus, Agent tool, launched 19:4x, markers
  scratchpad/roads2_20260911/BUILDING; brief scratchpad/brief_roads2.md;
  exe at its launch = RESUME3's DONE exe release/NifSkope.exe 19:08:42
  21,435,904 B). RESUME3 DONE 19:34 and spliced 19:3x (its WW_CHANGES
  entry REPLACED the two BUILD PENDING entries; MISTAKES +7; HANDOFF
  block below; skills crash-diagnose + anchored-hookup mirrored; grass
  picture sent 19:3x; cmp_tiling_fixed.png judged by bungo 19:2x -> lane
  TILING2). No other lane alive. Game down. bungo's window: none held the
  exe at the last check.
"""
b = b[:s] + new_live + b[e:]
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF live paragraph replaced")

p = R + "scratchpad/brief_roads2.md"
t = open(p, "rb").read()
old = b"Exe at launch: from RESUME3's DONE line (director fills: ______)."
assert t.count(old) == 1
t = t.replace(old, b"Exe at launch: `release/NifSkope.exe` 2026-09-11 19:08:42, 21,435,904 B (RESUME3's DONE exe).")
open(p, "wb").write(t)
print("brief_roads2 exe filled")
