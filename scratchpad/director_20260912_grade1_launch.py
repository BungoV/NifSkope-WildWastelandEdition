p = 'HANDOFF.md'
s = open(p, 'rb').read().decode('utf-8')
assert s.count(chr(13)) == 0
a = '  LIVE NOW: lane TILING4 (Opus, Agent tool, launched 2026-09-12 00:0x,'
assert s.count(a) == 1
new = '''  LANDED 02:4x: TILING4 DONE 02:35 (resume lane), verified (exe
  02:08:57 21,487,616 B newer than every changed source; rung
  before_tiling4 == launch bytes; no NifSkope/Fallout4 at 02:36),
  spliced 02:37 (WW_CHANGES + MISTAKES +1, the lane's duplicate of the
  director's dead-lane item dropped + HANDOFF block below), skill
  ww-prototype-is-not-the-product written from the lane's draft and
  mirrored, pictures cmp_tiling4.png + sheet_tiling4.png sent 02:4x.
  RESULT: `--land-sample stochastic` is now a hex tiling (Heitz-Neyret),
  swirl 13/14 (warp 7/14), grain vs current build 14/14, repeat 9/14
  (warp 11/14) -> DEFAULT UNCHANGED, still OFF; his call: soft hex
  blotchiness at 256 u vs the swirls. LIVE NOW: lane GRADE1 (Opus,
  Agent tool, launched 02:4x on TILING4's DONE exe 02:08:57; brief
  scratchpad/brief_grade1.md refilled 02:4x with the exe line + the
  parallel-lane rule; markers scratchpad/grade1_20260911/BUILDING /
  DONE) beside UINOTES1 in the copy. His window: none at 02:36.
  (superseded LIVE NOW follows for the record)
'''
s = s.replace(a, new + a)
open(p, 'wb').write(s.encode('utf-8'))
print('ok')
