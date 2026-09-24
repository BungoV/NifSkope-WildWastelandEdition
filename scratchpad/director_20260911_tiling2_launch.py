"""Director: ROADS2 landed -> LIVE NOW = TILING2; fill TILING2's exe line. LF-only files, CR asserted 0."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
p = R + "HANDOFF.md"
b = open(p, "rb").read(); assert b.count(b"\r") == 0
s = b.find(b"  LIVE NOW: lane ROADS2 (Opus, Agent tool, launched 19:4x, markers")
e_marker = b"  exe at the last check.\n"
e = b.find(e_marker, s)
assert s > 0 and e > s
e += len(e_marker)
new_live = b"""  LIVE NOW: lane TILING2 (Opus, Agent tool, launched 21:0x, markers
  scratchpad/tiling2_20260911/BUILDING; brief scratchpad/brief_tiling2.md;
  exe at its launch = ROADS2's DONE exe release/NifSkope.exe 20:46:44
  21,458,944 B). ROADS2 DONE 21:05, spliced 21:07 (WW_CHANGES + MISTAKES
  +5 + HANDOFF block below; skills spec-gate-audit + control-calibration
  already mirrored; three pictures sent 21:0x; roads suite 11/0 again;
  default composite is max-z, way back `--roads-legacy`). RESUME3 landed
  19:34. No other lane alive. Game down. bungo's window: none held the
  exe at the last check.
"""
b = b[:s] + new_live + b[e:]
assert b.count(b"\r") == 0
open(p, "wb").write(b)
print("HANDOFF live paragraph replaced")

p = R + "scratchpad/brief_tiling2.md"
t = open(p, "rb").read()
old = b"Exe at launch: from RESUME3's DONE line (director fills: ______)."
assert t.count(old) == 1
t = t.replace(old, b"Exe at launch: `release/NifSkope.exe` 2026-09-11 20:46:44, 21,458,944 B (ROADS2's DONE exe; ROADS2 changed the road composite -- read its HANDOFF block and `scratchpad/lane_roads2_report.md` too, its roads suite is 11/0 and must stay so).")
open(p, "wb").write(t)
print("brief_tiling2 exe filled")
