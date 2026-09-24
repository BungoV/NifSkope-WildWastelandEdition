p = 'HANDOFF.md'
b = open(p, 'rb').read()
assert b.count(b'\r') == 0
lines = b.decode('utf-8').splitlines(keepends=True)
anchor = '  LIVE NOW: lane TILING4 (Opus, Agent tool, launched 2026-09-12 00:0x,'
hits = [i for i, l in enumerate(lines) if l.rstrip(chr(10)) == anchor]
assert len(hits) == 1, hits
i = hits[0]
new = '''  DIRECTOR NOTE 01:4x 2026-09-12 (read before the LIVE NOW below): the
  TILING4 agent of 00:0x DIED SILENTLY -- last disk write 00:40
  (logs/g0_grain.txt), no subagent listed at 01:40, marker still
  BUILDING; found an hour late (director's mistake: no liveness check;
  the resume lane records it in MISTAKES_ENTRIES.md). Its work is on
  disk: report complete through section 1 (Gate F1 passed: instrument
  acquits the rung 7/7, convicts the isolated warp 7/7, TILING3's
  proposal 5/7; split frozen 00:11, law 00:27), h1_sweep: H1 hex tiling
  reads NO swirl 7 of 7, repeat 5 of 7, but the brief's per-sheet grain
  gate vs the same chunk's vanilla is unattainable by ANY sampler
  (vanilla grain spans a factor 24 by painted texture; rung 1/7, TILING3
  proposal 4/7). DIRECTOR DECISION 01:4x: grain + band gates become G1
  (median over the seven within 20 % of vanilla's median, TILING3's own
  criterion) AND G2 (per sheet within 20 % of the rung's, no regression);
  the literal per-sheet number still reported beside them. RELAUNCHED
  01:4x as lane TILING4 RESUME (Opus, Agent tool, background): brief
  scratchpad/brief_tiling4b.md, same lane dir and report (appended),
  same DONE marker, same exe baseline 23:26:29 21,484,032 B. Landing
  sequence unchanged (verify exe/DONE/sweep, mirror skills, splice with
  `python scratchpad/splice_lane_docs.py scratchpad/tiling4_20260912
  TILING4`, pictures cmp_tiling4.png + triptych, refill
  brief_grade1.md's exe line, launch GRADE1).
  bungo's UI NOTES 01:3x-01:4x (scratchpad/brief_uinotes_20260912.md,
  three rulings collected BEFORE any animation-workspace work, in his
  words there): 1 remove the bottom status bar entirely (its Saved /
  Loading messages need a home, two harnesses read it); 2 selected
  keyframes vanish -- they must stay visible and turn orange, Blender
  style; 3 a track context-menu entry under Remove track that strips
  chosen translation X/Y/Z and rotation X/Y/Z components from every key
  of the track (his case: COM of Running_To_Slide keeps Z, loses X/Y).
  Plus six unconfirmed timeline guesses put to him (in the file). These
  become lane UINOTES1 (brief to write) and go BEFORE any further
  timeline/animation lane; not before the lodgen queue unless he says.
  His NifSkope window: OPEN since 01:29 (pid 60820, no --port) on the
  23:26:29 exe; rename aside at link time, never kill.
'''
lines[i:i] = [new]
out = ''.join(lines).encode('utf-8')
assert out.count(b'\r') == 0
open(p, 'wb').write(out)
print('inserted before line', i + 1, 'bytes', len(out))
