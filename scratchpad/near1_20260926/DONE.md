# NEAR1 -- near-library bake (rung N1), lane log

Worktree `E:\Projects\NifskopeWWE-near1`, branch `near1-20260926` from main `0834d4dc`.

## 02:1x setup (clock read 02:18)
- `.qmake.stash`, `GeneratedFiles`, release runtime copied from main (main `make -n` = 0 g++); qmake rc 0
  (123 worktree paths, 0 main paths); REVISION objects deleted; first build rc 0.
- RUNG exe for G4: `release/NifSkope.before_near1.exe`, sha1 `6321a3715d3cf76449ed54c7542152b210b3d592`.

## 02:2x design (decided before code)
- A SEPARATE module `src/nearlib.cpp` behind its own switch (`lodgen ... --near-library <dir>`), never a
  branch inside `lodgenNativeWrite`: the far writer's code path is untouched, so G4 holds by construction
  and is then measured anyway.
- Clusters are the far field's: `LODO_CLUSTER_MAX_TRIS` = **16 triangles / 48 vertices**, NOT ~128. Reusing
  the far code means 16; the brief's number is noted, not built. Ladder OFF (levelMax 0): one level, full detail.
- Self-AO not cast on full-detail meshes (ao filled 255 = "not baked", the v1..2026-09-18 value).
- Format, only where the far format lacks it:
  - `.lodo` **v7, written only by the near bake**: header flag bit 4 `NEAR` (full-detail library) and the
    material row's reserved byte becomes `features` (parallax / env map / greyscale-to-palette / vertex colour /
    model-space normals). A far file never sets either, so it stays v6 byte for byte.
  - `.lodi` **v11, written only when an instance carries bit 8 `INITIALLY_DISABLED`** (v10 layout + that bit,
    needs the v7 header like v10). The far field drops initially-disabled refs, so it never sets the bit.
- Material swap: the far field's rule (`nativeEffectiveSwap`, variant keyed by path + substitution).

## 02:4x build + first test bakes
- Commit 1881a85f: the bake (`src/nearlib.cpp`), lodo v7 / lodi v11, decoder accepts both.
- Crash on the first run (segfault, every model "not found"): the resource stack's index is built on first use
  by a function-local static, and four workers reached it at once. Fixed by `lodgenWarmSharedIndices()` before
  pass A (the far bake already did this). Also: the exe changes to `release/`, so a relative `--near-library`
  landed under `release/scratchpad/...`; the census now prints absolute paths and `bake.sh` passes absolute.
- "model-unloadable" split into `model-missing` (not in the stack) and `no-geometry` (in the stack, no
  BSTriShape with vertices and triangles: `*DummyLOD.nif`, sky shells).
- **BSMeshLODTriShape MEASURED** (Boston): 756 of 756 such shapes store LOD0+LOD1+LOD2 end to end; 712,209
  triangles where LOD0 is 336,926. Keeping all would draw the coarser copies inside the full one. Decision:
  keep the first `LOD0 Size` triangles and only the vertices they use (`nearTrimToLod0`). Boston: 739 shapes
  trimmed, 371,604 triangles not drawn; Sanctuary 273 / 248,019.
- Boston (-4 -9 .. 0 -7): refs 13,498, eligible 10,713; .lodo 44.6 MB, 1.455 M tris, 93,636 clusters.
- Sanctuary (-21 21 .. -16 26): refs 7,928, eligible 5,168; .lodo 19.1 MB; scrappable 280. BNS Trees are placed
  as STATs with Tree_Anim: excluded as tree-anim (132 shapes), as the brief rules.

## 03:0x gates G1-G4 (clock read 03:00-03:05)
- tests/spells/near_library_check.py (commit f4b338fb): own ESM stack walk (46 plugins, merge = last
  version wins, REFR keeps first file's cell), own NIF + BGSM decode off the MO2 stack, nif.xml inheritance.
- Boston: G1 PASS (13,498 read, 10,713 eligible, 11,324 placements = .lodi 11,324, each once, disabled bit ok);
  G2 PASS (2,197 models, 5,029 shapes; worst box diff 0.000248 u); G3 PASS.
- Sanctuary: G1 PASS (7,928 / 5,168 / 6,711); G2 PASS (593 models, 1,267 shapes, 0.000127 u); G3 PASS.
- Refuters, each red on its gate: drop-instance, dup-instance (G1), no-marker-rule (G1 on Boston: 26 marker
  REFRs would turn eligible), no-lod0-trim (G2, box off by 1,290 u), tris-off (G2), census-off (G3).
  no-dest-rule stays green: no STAT/SCOL in either region carries DEST -- stated, not hidden.
- G4 (g4_bakes.sh + g4_cmp.py): rung release/NifSkope.before_near1.exe vs the NEAR1 exe, vanilla ESM +
  unpacked data, fixture + SanctuaryHillsWorld -28,-12..2,25 + Commonwealth -4,-9..0,-7: 130 of 130 files
  byte-identical (the far files keep lodo v6 / lodi v7, not even the version word moves; .lodb provenance
  excluded, it carries exe size, clock, path). Refuter: one flipped byte -> FAIL.
- 03:05 whole-Commonwealth near bake started (game down, 42 GB free on E:).

## 03:0x-03:1x whole Commonwealth + format self-tests
- Whole Commonwealth (all cells -96..95), bake 03:05-03:08 (177.5 s internal, 2m58 wall), rc 0:
  refs read 736,214, eligible 523,755, excluded 212,459 (no-drawable-shape 107,389, type:MSTT 14,954,
  type:TXST 13,384, type:MISC 11,869, type:LIGH 8,642, marker 8,360, ... model-missing 203, no-geometry 2,477,
  animated 4,439, deleted 516, scol-no-eligible-part 12); placements 677,390 (SCOL parts 187,401, initially
  disabled 50, scrappable 5,133); SCOL part placements excluded no-geometry 28,028 (29,586 of the P rows are
  StaticCollectionPivotDummy 00035812, a marker NIF), no-drawable-shape 6,594, animated 224.
  Shapes kept 39,528; excluded alpha-blend 1,102, decal 4,822, effect 254, tree-anim 213; 4,351 LOD-trimmed
  (3,119,591 coarser triangles not drawn). Meshes 16,873 (7,519 swap variants), bases 18,423, materials 2,989
  (all legacy; alpha-test 634, alpha-test-two-sided 226, opaque 2,084, two-sided 45). Pairs 38,622.
  Tris 12,160,169, clusters 777,824, verts 16,855,095. Textures 5,166 (17 missing), 80 buckets, 44 sets.
  .lodo 382,058,580 B (v7), .lodi 26,549,276 B (v11), sidecars 71.8 MB. Disk after: 42 GB free.
- near_library_check.py on it: G1 PASS (736,214 / 523,755 / 677,390 placements each once), G2 PASS
  (10,034 models, 24,967 shapes, worst box 0.000282 u), G3 PASS; 86 s. Fix on the way: one MODL is not
  cp1252 text (latin-1 now).
- tests/spells/near_format_selftest.py: 15 checks PASS (v7 NEAR flag both ways, features bits 5..7, v6
  features byte, lodi v11 relabelled 10 -> bit 8 refused, v12 refused; controls accepted).
- lodgen_native_fields.py j0c: far pair has no NEAR flag / features / bit 8. Green on the SHW far bake
  (the 6 other FAILs are identical on the rung's files: no --mesh-report, no ladder); red on the near pair.

## 03:1x pictures (shots.sh; exe 02:49:25; installed far .lodl/.lodt read only for terrain)
- shots/near_top08.png -- Boston near library, 08 camera (view 8, ortho, upp 20.48 read back), 11,322 of
  11,324 placements drawn (2 persistent refs outside the region).
- shots/far_top08.png -- the installed far field, same camera (3,537 placements in the region).
- shots/near_oblique.png / far_oblique.png -- close perspective (FOV 50, dist 5,200 at -7400,-31600,300), same framing.
- Caveats: purple brick = greyscale-to-palette textures drawn raw; magenta = missing texture. The near .lodi's
  placement identity column is 0 everywhere (the `placement` channel read "constant 0"; render dropped).
- All 4 PNGs 1600x1024, untracked (scratchpad PNGs are ignored).

## Skills
- Loaded: ww-lodo-version-bump, nifskope-ww-lodgen, nifskope-ww-worktree-build, nifskope-ww-render-shot.
- Wished: one that states the ESM merge semantics (which cell an overriding REFR stays in) for Python
  checkers -- now written into the new skill's section 3.
- Written: .claude/skills/nifskope-ww-near-library/SKILL.md (bake, reasons, G1-G4 + refuters, pictures, traps);
  copied to E:\Tools\AISkills\fo4-near-library-bake\SKILL.md (not committed there: that repo has others' edits).
