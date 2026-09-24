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

## Code landed 25a36ed (exe 0518d243, 21:23): items 1, 2, 4 in src/lodgen.cpp
- G2 GREEN on 0518d243: east(R) 78.74, up(G) 32.99, north(B) 4.89 (rung: 4.21 / 36.69 / 77.15 = red).
- Byte identity on that chunk: base arm (--land-detail-source none) all 3 sheets + BTO + manifest + BTR identical
  to the rung; blend arm: only _msn moves (the fix), colour and _data identical.

## Gates, 21:25-21:35
- G1 RED on rung bb60a7ea (whole Commonwealth --vt, 421 s): pbrm 0 + legacyInverted 99 + noneDefault 1 = 100, distinctLtex 101.
- G3 RED on rung: flipped copy (4 UTF-16 sites of the default vanilla LOD root -> .../DatX) --incremental:
  "0 of 2 chunks dirty", 4 of 12 files stale vs a full flipped bake. GREEN on 0518d243: "2 of 2 chunks dirty
  (2 inputs moved)", incremental == flipped full bake. Floor: the flip moves 4 of 12 files in a full bake.
- G2 via the spell: rung FAIL (4.21/36.69/77.15), new PASS (78.74/32.99/4.89).
- Spell tests/spells/lodgen_vtfix.sh (G1 leg takes G1_LODM=, else a named SKIP).
- Docs: LODGEN_PARITY terrain-identity line matches nifcli (fix02_docs.py); VT doc anchors + form-0 note.
  docs/LODGEN_LEDGER_FORMAT.md is NOT this lane's file: its row 8a is parked in fix03_ledgerdoc.py.
- Running: G1 green bake on 0518d243; kept-green sweep (run_kept.sh -> kept/kept.log).

## Results, 21:33-21:40
- G1 GREEN on 0518d243 (445 s): pbrm 0 + legacyInverted 99 + noneDefault 2 = 101 = distinctLtex 101.
  Whole-map byte identity vs the rung: 6 files, 5 identical (all .lodt levels + heightmap), the .VT.lodm differs by
  exactly one byte: "noneDefault":1 -> 2.
- Kept green on 0518d243: lodgen_terrain_vt 45/0, lodgen_terrain 26/0, lodgen_slab 16/0, lodgen_terrain_pbrm 14/0,
  lodgen_incremental 0 failures, lodgen_bakerec PASS, lod_generation PASS 128 checks (floor 121, port 42391),
  .lodl (the retired --lodt's successor) byte-identical to the rung (35,953,294 B), native heightmap byte-identical
  to the shipped FO4CS reference.
- lodgen_defaults 31 checks, 1 failure: "(d) no C lines with identity off (floor 1)" -- 0 card lines in BOTH
  arms. SAME RED ON THE RUNG (PHASES=d, bb60a7ea): pre-existing, card region (not this lane's).
