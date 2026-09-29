# TERRLIVE1 job 2 (worktree terrlive2, branch terrlive2-20260929 from origin/main 5adb627f)

## RESUME (written 2026-09-29 14:31, stopped by the coordinator: account usage cap)
Done:
- Worktree E:/Projects/NifskopeWWE-terrlive2 on branch terrlive2-20260929 (from origin/main 5adb627f). Build seeded per skill
  nifskope-ww-worktree-build: main's objects (make -n = 0 in main), .qmake.stash, qmake, and the revision objects deleted.
  First build BUILD-RC=0 at 14:31 (release/NifSkope.exe), turn released.
- src/lodgen.cpp: a debug dump only, in lodgenRuleBuild. WW_RULE_DUMP=<file> writes every rule sample: vanilla rgb, slope,
  height, have-vanilla, our true mix (8 form ids + weights), the forms (id, area, diffuse path), the palette (form index,
  colour) and the choice a/b/w. Magic "RDMP" v1. Nothing reads it back and no shipped format changed. Not run yet.
- Scratch scripts re-pointed to terrlive2: build.sh, chain.sh, pv.sh, bake.sh, bake_small.sh, make_spec_rule.py, seed.sh; edge/ tools copied.
Half-done (design only, no code):
- Part 1 rock: planned an affine colour model (vanilla ~ M*mix + b, fitted on painted samples) before matching, a rock
  flag from the diffuse path ("rock"/"cliff"), and rock share per slope bin (painted truth / rule on painted / rule outside /
  vanilla estimate), to be tuned offline on the dump.
- Part 2 rounding: planned a Gaussian-blurred signed distance (512 u grid, sigma about 2048, offset c = E|Y| + 362 so every
  unpainted texel keeps w = 0), one function shared by the bake fill and the preview weight map. OPEN QUESTION found: the hard
  squares in pics3/north_steps_on.png (terrlive1 scratch) look like per-quadrant content, not the fade outline. Measure first:
  compare "ours" unblended against V on those quadrants. step_align.py only tests the class edge at cell grain; the quadrant-grain
  version (realq, the view's own ortho) is still to write, and it must read red on the head.
- Part 3 shading map: not started. The object AO code is located: lodgen.h:1178-1325 (terrainObjectAo, strength, slab, skyObjects,
  census); lodgen.cpp object field built at ~12786 and ~17534, applied at ~13726 and ~15130.
Exact next step:
  1. Run the whole-map DYNAMIC rule bake with the dump:
     WW_RULE_DUMP=$S/rule_dump.bin bash chain.sh dyn_dump --terrain-option dynamic --outside-paint rule
     (about 7 min; check chain.sh's EXE path first: it still points at run_rule/, so copy release/ to run_new/ first).
  2. Write the numpy tuner on rule_dump.bin (rock share per slope bin, affine model, lambda/prior), then port to C++.

Brief: C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/6b94e90a-da02-4884-b365-32e898e917da/scratchpad/terrlive1_next_after_merge2.md
Three parts: (1) more rock on steep ground in the rule paint; (2) round the painted-area outline; (3) 32 u shading map over painted ground + band.

## 2026-09-29 14:17  set-up
- Worktree made, HANDOFF top block and brief read (clock read 14:12).
- Tools copied from terrlive1 scratch (edge/*.py, cells.npz) and scripts re-pointed (build.sh, chain.sh, pv.sh, bake*.sh).
- Baselines (job 1, terrlive1 scratch): hills far-half drift 7.62 HYBRID / 12.50 DYNAMIC; gate_law2 --rule PASS on whole_rule.
