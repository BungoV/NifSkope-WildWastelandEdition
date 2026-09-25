# CARDFIX1: DONE.md step 5 sections (IMPOSTORRING1). LF-only file; anchors count==1; CR count unchanged.
# The kept-green line is read from done_step5_kept.txt (written once the chain's verdicts are in).
import io
D = 'E:/Projects/NifskopeWWE-cardfix1/scratchpad/cardfix1_20260924/'
P = D + 'DONE.md'
b = open(P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')
kept = io.open(D + 'done_step5_kept.txt', encoding='utf-8').read().rstrip('\n')


def rep(old, new):
    global s
    assert s.count(old) == 1, (old[:60], s.count(old))
    s = s.replace(old, new)


built = '''## Step 5 -- IMPOSTORRING1: a horizon ring card set (16 views x 1 row)
- `WW_IMPOSTOR_RING=V` bakes V views evenly around the horizon at elevation 0 into a V x 1 sheet (frame v
  at x = v*tw; eye (cos p, sin p, 0), right (-sin p, cos p, 0), up z). The sidecar says `ring V tw th` and
  echoes one `ringview v az el` per frame, which the gate reads back against the law (src/nifskope_ui.cpp).
- lodgen carries a ring set as `views` V / `grid` [V,1] with NO `oct` key and frameOffset 2V
  (src/lodgen.cpp card region; docs/LODGEN_LODM_FORMAT.md 3.2). The drawer picks the nearest azimuth frame
  (src/impostorcard.*, src/gl/impostordraw.cpp). An old exe refuses a ring set by name ("oct is 0, outside").
- Why 16 x 1: a tree seen from LOD distance is seen from within a few degrees of the horizon; a ring spends
  every texel there, and 16 x 1 at tile 256 is a quarter of the N8 sheet's pixels.
- The preview harness gained WW_IMPOSTOR_ORBIT_SELECT (src/impostorpreviewtest.cpp); the bake driver
  passes WW_IMPOSTOR_RING through and defaults the tree run to 16 (tools/bake_impostor_cards.sh); the
  FO4CS reader is owed (spec text only).
- FINDING for bungo: N8 is BETTER than the 16-view ring at every elevation measured, including the horizon.
  N8 already has 28 frames near the horizon (largest step 16.2 degrees), so the ring buys pixels, not shape.
  The ring sits at its own ceiling (the mesh against itself rotated by half a step). The bake driver's
  TREE run defaults to RING=16 as ruled (bungo 2026-09-23 04:4x, "22.5 degrees per take"; RING=0 = the
  N8 grid, the empty-slot run keeps the grid); the numbers above argue for his second look.
- Owed: lodgenaggregate learning the ring (refused by name today); the panel's Card frames row and
  cardsOnDisk ignore ring sets; the FO4CS reader.

'''
rep('# 3. Gates (numbers; red runs)\n', built + '# 3. Gates (numbers; red runs)\n')

gates = '''## Step 5 (exe eaa4b0b6)
- tests/spells/impostor_ring.sh 13 / 0 (gates/impostor_ring.s5.out). R1 16 view echoes, error 0.0000.
  R2 views 16, grid [16,1], albedo 1280 x 256. R3 this exe loads it ("grid: RING of 16 views"); the rung
  refuses it by name ("oct is 0, outside"); floor: the rung loads this exe's N8 set. R4 in-between azimuths
  el 0: IoU 0.4886 >= 0.4513 (0.90 x the mesh's own ceiling 0.5015); RED shuffled frames 0.0776.
  R4a at the bake directions 0.9131. R6 16 / 16 nearest-frame picks. R5 pixels ring16 62,795 vs N8 47,453
  bytes compressed (1.32); ring8 at 1.66 bites the size bar.
- RUN 1 FAILED 13 / 2 (gates/impostor_ring.run1.out): R4's bar was an absolute 0.60 pre-registered without
  measuring the subject; the mesh rotated by half a step against itself only reaches 0.5015. Re-pinned to
  0.90 x that measured ceiling (fix12), and the red filter now drops only 'EXCLUDED: mesh' lines (it was
  also dropping colour EXCLUDED lines). MISTAKES text in DELIVERABLE_TEXT.md.
- Mean IoU at the in-between azimuths, ring16 vs N8 (M, not gated):
  el 0: 0.4886 vs 0.8249 | el 5: 0.4863 vs 0.8044 | el 15: 0.4266 vs 0.6511 | el 30: 0.2849 vs 0.7947 |
  el 60: 0.2086 vs 0.3712. Full turn at el 0: ring16 0.6904 (ceiling 0.7012), N8 0.7842, ring8 0.5255.
''' + kept + '\n\n'
rep('# 4. Exe sha1 + commits\n', gates + '# 4. Exe sha1 + commits\n')

rep('- commits: step 1 19c0347; step 2 91ddd41 (evidence only); step 3 d8302c9; step 4 see git log.\n',
    '- step 5 build: eaa4b0b60e9ff6796df846f54ebf292a94cc0aca (23:00:20), 24,729,600 B; kept as\n'
    '  release/NifSkope.s5_eaa4b0b6.exe (the step-6 gates\' previous exe).\n'
    '- commits: step 1 19c0347; step 2 91ddd41 (evidence only); step 3 d8302c9; step 4 7896ad1;\n'
    '  step 5 1303334 (code) + this DONE commit.\n')

out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(P, 'wb').write(out)
print('patched DONE.md')
