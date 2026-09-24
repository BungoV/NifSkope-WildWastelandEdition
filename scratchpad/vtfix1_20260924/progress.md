# VTFIX1 progress (lane LOD-B), worktree E:\Projects\NifskopeWWE-vtfix1, branch vtfix1-20260924 from 47b2cad

## Skills loaded (21:0x)
nifskope-ww-lodgen, nifskope-ww-build-verify, search-lean, ww-test-harness-add. Read: repo skill
ww-out-of-tree-hotfix-build (no nifskope-ww-worktree-build skill existed at 21:10).

## Bootstrap
- main tree HEAD 71f96c1 differs from 47b2cad only in tests/spells/pbr_csm1_gates.sh; main exe 20:36 newer than
  every src/res/lib file -> main's GeneratedFiles objects == 47b2cad sources. Copied them (read-only source).
- qmake in a fresh worktree fails (QMAKE_CXX.COMPILER_MACROS) until main's .qmake.stash is copied. First make
  regenerated moc (22 moc objects) and relinked: rung exe bb60a7ea (21:14), kept as release/NifSkope.before_vtfix1.exe.
  Skill written: E:\Projects\Claude\.claude\skills\nifskope-ww-worktree-build\SKILL.md.

## Item 2 measured on the rung (21:3x) -- the axis swap is REAL
G2 known-answer: fixture _msn with an EAST-only ripple (+-0.6, period 4) under --vanilla-lod-root, chunk 4.-20.24,
--land-detail-source vanilla-blend vs none. Rung: mean|delta| east(R) 4.21, up(G) 36.69, north(B) 77.15 -> FAIL
(the east detail landed in NORTH). Code: lodgenBlendVanillaDetail read e from bits 0-7 (= north per
lodgenTerrainMsnPixel) and n from 16-23 (= east).
