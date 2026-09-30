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

## 2026-09-30 08:10  part 1 (rock) + part 2 (rounded outline) coded
- Rock: rule score counts brightness half (lodgenRuleBuild). Whole-map dump re-run in numpy (tune.py):
  rock share 14-22 deg 0.12 -> 0.27, above 45 deg 0.01 -> 0.63, hue error down in every slope bin.
  Whole-map hybrid bake `rock1` running for the drift + edge gates.
- Seam fade margin 128 -> 1024 (half a quadrant, setter clamps to it); the live preview now cross-fades
  the four quadrants too (buildGrid), same halved quintic ease as the bake.
- Rounded outline: LodgenOutlineField (lodgen.h/.cpp). Exact signed distance to the quadrant staircase on a
  512 u grid (Felzenszwalb EDT), Gaussian blur sigma 2048, capped at band + 2 offset + 3 sigma;
  w = smoothstep(0, band, f - offset), offset = sigma*sqrt(pi/2) + 2 half-diagonals = 3291 u;
  unpainted quadrants held at w = 0 outright. Bake builds it once (lodgenVtFillPainted); the preview builds
  the same field over its window (reach = lodgenOutlineWindowQuads, so values equal the bake's).
  Fast-outs and ruleNeed reach = ceil((band + 2 offset)/2048)+1 = 9 quadrants. Log: vanillaFill ...
  outline=rounded sigma offset maxUnpaintedField.
- Gate: edge/outline_gate.py on WW_BLEND_DUMP (new preview dump). Axis-aligned share of the band's
  gradient vs the coast's own (mask blurred 4096 u) + 0.05. Law 2 (the head's staircase, rebuilt from the
  same quadrants) must read FAIL; the new head must PASS with zero weight on unpainted texels.

## 2026-09-30 08:26 part 3 preview wired (not built yet)
- terrainpreview: spec "loda" (else the .loda beside "rule") read with lodgenAoRead; per view an R8 texture
  over the view's quadrants at 32 u (box-filtered to <= 8192 a side), 255 outside painted ground; the
  terrain shader multiplies OUR splat by it before the vanilla/rule blend (so the fade band carries it,
  outside nothing). Per render "loda": true|false (default on). Log line tags "loda on/off".
- Specs: spec_steps.json (north steps, rule on/off, baked + dynamic), spec_aomap.json (street, Boston:
  hybrid off / on, dynamic on, old FULL + AO 16 u reference). Both read final_on / final_off bakes.

## 2026-09-30 08:37 rock result + stall
- rock1 (luma half): hills drift far 7.62 -> 8.79 HYBRID, 12.50 -> 13.62 DYNAMIC (worse, darker by ~3 levels);
  edge gate PASS (dip 0.02, step 0.29). Rock share up by numbers, not visible in the far picture.
- Temp knob WW_RULE_LUMA_K (lodgen.cpp gRuleLumaK, TUNE-ONLY, remove before commit) for a K sweep with
  DYNAMIC rule-only bakes (make_sweep.py -> spec_sw_<name>.json).
- Build 08:33 OK (outline + AO + preview .loda). New exe staged in run_k (EXE=run_k for chain.sh/pv.sh).
- Stalled: dyn05 started on the stale run_new exe; its NifSkope (pid 18852) still holds the turn; my stop was refused.

## 2026-09-30 09:10 rock = hue + brightness gain; AO map measured
- Offline (sweep_luma.py, sweep_gain.py on rule_dump.bin): above 30 deg the palette is 7-28 levels darker than
  vanilla at ANY luma weight, so the drift is brightness, not choice. Fix: .lodr v2 adds plane G (gain x128,
  vanilla lum / mix lum, clamped 0.5..2); choice stays luma-half (rock). Offline: |lum| -> ~0 every bin,
  chroma down on steep bins (b6 28.2 -> 15.8). Bake applies it in mixAt; preview in ruleGain(); --rule-check
  prints gain mean/range. Temp knob removed. docs 2.6c updated.
- AO map (dyn075, whole map): 48,021,688 bytes, 15,893 quadrants, build 73 s + gather 9 s; --ao-check OK.
  vs baked AO (rock1 VT.8, 64 u) on the street cells: mean 123.3 vs 124.6, corr 0.989, |diff| 7.5.
  The 16 u reference bake was deleted in the cleanup and FULL can no longer be baked: the reference is 64 u.

## 09:17 rock: brightness gain (.lodr v2 plane G), DYNAMIC whole map (dyngain)
- bake rc=0 578 s; outsideRule meanColourError8bit=5.00 meanLumError8bit=0.14 meanGain=1.016 gainClamped=150 of 2.36M
- hills far-half drift 12.50 -> 5.98 DYNAMIC (|d lum| 4.90; near 1.94); mean rgb off [95.1 86.1 72.4] on [95.4 84.9 69.3]
- next: HYBRID whole-map final_on/final_off, then outline gate + steps, AO map pictures

## 12:26 close-out: outline, AO map, all three items done
- **Rounded outline**: sigma 2048 -> 3072 (lodgen.h LODGEN_OUTLINE_SIGMA). Chosen on the dump's own quadrants
  before the C++ (edge/outline_proto.py: s2048 0.310, s3072 0.167, s4096 0.096). outline_gate on the new exe:
  coast 0.194, bar 0.244, law2 0.561 FAIL (the proof), this head 0.166 PASS, unpainted max w 0.0000 -> GREEN.
  Band now starts ~4.6 km inside our paint (was 3.3 km).
- **Re-bakes with run_l** (= release, cmp same): final_on (rule) rc 0 2233 s, final_off rc 0 1821 s.
  gate_law2: final_on --rule PASS (outside drift 3.94, dip 0.00, step 0.16); final_off PASS (1.48, 0.00, 0.14).
- **AO map vs reference**: the 16 u reference had died when C: filled (4 KB lodt); re-baked on E: as a region
  bake (ref16, 64 u, objAoTexels 944951, 758 s). ao_vs_ref over -8,-12..3,-1: 589,824 texels at 64 u,
  .loda mean 123.3 vs ref 124.6, corr 0.989, mean |diff| 7.5. Black under building footprints is in the
  reference too (the objects' sky union).
- GPU ms (median of 30): street hybrid loda off 0.408 / on 0.410, dynamic on 0.491; Boston 0.137 / 0.138,
  dynamic 0.280; old FULL reference 0.044 / 0.068.
