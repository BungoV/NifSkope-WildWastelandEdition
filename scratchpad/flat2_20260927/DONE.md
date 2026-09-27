# Lane FLAT2 -- one-colour terrain chunk sheets stored as one value

Worktree E:\Projects\NifskopeWWE-flat2, branch flat2-20260927 from night-trial @ 5b338d39.
Written incrementally; newest state at the bottom of each section.

## 1. Skills loaded
search-lean, nifskope-ww-lodgen, nifskope-ww-worktree-build, nifskope-ww-build-verify,
ww-module-off-is-identical (render/picture skills loaded when the render step starts).
CONSTITUTION + HANDOFF top block read; brief + night_rules read.

## 2. What was wrong (measurement)
Every sheet was stored in full, even where it is one value over every texel of every mip.
Measured on the installed 09-25 whole-map bake (read-only, work/measure.py; full table in
work/measure_installed.txt). a = one value after decode, bit-exact; b = not a but within 1 level
per channel (never collapsed: height has no codec, and BC b-only was 0).

| worldspace | sheet | sheets | a | a bytes | b-only | of bytes |
|---|---|---|---|---|---|---|
| Commonwealth | colour | 12276 | 0 | 0 | 0 | 2,138,970,240 |
| Commonwealth | msn | 12276 | 5230 | 911,275,200 | 0 | 2,138,970,240 |
| Commonwealth | mask | 12276 | 0 | 0 | 0 | 2,389,875,840 |
| Commonwealth | height | 12276 | 5563 | 3,877,188,480 | 3 | 8,555,880,960 |
| Far Harbor | colour | 1164 | 0 | 0 | 0 | 202,815,360 |
| Far Harbor | msn | 1164 | 357 | 62,203,680 | 0 | 202,815,360 |
| Far Harbor | mask | 1164 | 337 | 58,718,880 | 0 | 320,950,080 |
| Far Harbor | height | 1164 | 370 | 257,875,200 | 32 | 811,261,440 |
| Nuka-World | colour/msn/mask/height | 2139 each | 646 each | 112.6 M / 112.6 M / 112.6 M / 450.2 M | 0 | 372.7 M / 372.7 M / 469.8 M / 1,490.8 M |
| Sanctuary | all | 400 each | 0 | 0 | 0 | |

Whole installed bake: 19,984,102,304 B of .lodt; case a = 5,955,174,720 B (29.8 %); a+b 29.9 %.
Far above the 1 % stop line. The one-value tiles are the ones with no LAND record (the ring
outside the worldspace and the open sea east of the coast); in the Commonwealth the colour and mask
sheets there are NOT one value (measured; why was not looked into), only normal + height.
Picture: pics/F1_commonwealth_one_value_tiles.png.

## 3. What changed
- .lodt tile flag bit 2+k = sheet k is one value; its place in the payload holds a 16-byte record (the
  sheet's unit: BC1 8 B block, BC3 16 B block, R16 2 B texel, RGBA8 4 B texel, then zeros). Repeating the
  unit gives the uncollapsed bytes exactly. No version bump (mipSkip precedent; old readers refuse the bit
  by rule 16). Collapse only when every unit of every mip is identical AND the block is one value under any
  decoder (index test). Validator rule 16 + new 16c. Doc LODGEN_TERRAIN_VT.md 3.2a.
- Writer on by default; `--no-collapse-uniform` (CLI only) = today's bytes. Census words on the `vt:` line.
- Readers: lodtsheets.cpp (viewer) reads through lodvReadSheetMip (expands); tests/spells/lodgen_vt_check.py
  expands. FO4CS reader change listed in DELIVERABLE_TEXT.md.
- Builds: rung 12:53:56 (release/NifSkope.before_flat2.exe sha1 88ef4c6b), new 12:59:31 (sha1 223fc434), RC 0.

## 4. Gates
Boston (-8 -12 3 -1), LEAN bakes, rung / off / on (bakes/rung, bakes/off, bakes/on):
| gate | expected | measured | verdict |
|---|---|---|---|
| G1 controls: flipped data byte, flipped chunk-key bto byte, flipped lodb plugin name, removed file | red | all 4 red | PASS |
| G1 OFF == RUNG, data files | byte-identical | 136 files, 0 data differ; 19 provenance files differ only in named words (17 chunk .key inputs/switches hashes, the .lodb record, flat_objects_report paths) | PASS |
| G1 every .lodt OFF == RUNG | byte-identical | 2 of 2 | PASS |
| G2 non-.lodt ON == OFF | identical except provenance | 0 data differ, 18 provenance | PASS |
| G2 per-tile checks | 0 failures | 0 failures (Boston collapses nothing: 0 of 180 sheets) | PASS (vacuous here) |
| R1 non-uniform sheet forced to collapse | red | red ("OFF sheet spans 148 levels") | PASS |
| R2/R3 doctored record | red | NOT RUN: Boston has no collapsed tile | owed (sea bake) |
| bake time | within noise | wall 443 / 435 / 469 s (rung/off/on), other lanes' bakes running; no VT stage timer | noted |
The Boston box is all land: the installed-bake map shows no one-value tile there, which the ON bake confirms.
The sea region (32 -12 43 -1) is where the ON arm is really tested; its bakes had not started at the pause.

## 5. Commits
- 7e160882 FLAT2: one-value .lodt sheets (src, checker, doc, bake.sh, measure.py, vtread.py)
- (pause commit) gates.py, map.py, shot.sh, render_sea.sh, sea_bakes.sh, whydiff.py, lodbdiff.py,
  lodbresid.py, pick_sea.py, DONE.md, DELIVERABLE_TEXT.md

## 6. Pictures
- scratchpad/flat2_20260927/pics/F1_commonwealth_one_value_tiles.png (legend check PASS, not in git)
- sea-edge before/after: owed (render_sea.sh)

## 7. What is still not right

## 8. Skills loaded / wished / written

## RESUME (paused 2026-09-27 ~14:20 on bungo's word)
Measured: whole installed bake 5,955,174,720 of 19,984,102,304 B (29.8 %) are one-value sheets (section 2).
Done: code + doc + checker (7e160882); both builds; Boston rung/off/on bakes + gates (section 4); map picture.
Open at pause: sea_bakes.sh (background, started 13:25) was still WAITING on the machine turn lock when the
pause came (no NifSkope of it had started). TaskStop reported it stopped, but its bash + bake.sh were still
listed afterwards; a kill of my own waiter was DENIED by the classifier, so it was left alone. If it got the
lock after the pause it runs the sea_off, sea_on, sea_rung bakes into bakes/sea_* (each ~7.5 min, releasing
the lock after each). Check sea_off.out / sea_on.out / sea_rung.out first.
Next steps, in order:
1. If bakes/sea_{off,on,rung} are not all rc 0: from scratchpad/flat2_20260927
   `REGION="32 -12 43 -1" LEAN=1 bash bake.sh "$PWD/run_new/NifSkope.exe" "$PWD/bakes/sea_off" --no-collapse-uniform`
   then the same for sea_on (no switch) and sea_rung (run_rung exe).
2. `python gates.py bakes/sea_rung bakes/sea_off bakes/sea_on --make-doctored doctored` (expect R2/R3 to run red).
3. `run_new/NifSkope.exe -no-gui lodgen --lodt-check doctored/control.lodt` (ok) and R2_block / R3_pad (refused),
   each under turn.sh acquire/release flat2.
4. `bash render_sea.sh` (before = rung exe + sea_rung sheets, after = new exe + sea_on sheets); pixel diff; label.
5. Fill sections 4/6/7/8, DELIVERABLE_TEXT counts; skill write; delete bakes/, cache/, run_* copies.
