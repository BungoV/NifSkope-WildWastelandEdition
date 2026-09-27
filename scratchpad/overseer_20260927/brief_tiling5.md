# Lane TILING5 -- LOD terrain: height-aware blending + large-scale variation

Worktree: E:\Projects\NifskopeWWE-tiling5, branch tiling5-20260927 from night-trial @ 5b338d39 (already created).
FIRST read the shared rules: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\night_rules.md
Report: scratchpad\tiling5_20260927\DONE.md in the worktree (incremental; commit by path as soon as a step compiles).

## His words (2026-09-27, verbatim)
"Can we do the 3 today? / or 2 / so 1 and 2" -- 1 = height-aware blending, 2 = large-scale variation (macro colour /
roughness variation over tens of metres, independent of the patch pattern; NOT fake shadows).
"because right now, terrain blending on lods / lod terrain patterning could use an improvement"

## What exists (read first, do not re-derive)
- src/lodgen.h ~123-227 + lodgen.cpp: TILING3 domain warp (refused: swirls) and TILING4 Heitz-Neyret hex tiling
  (`--land-sample stochastic`, `--land-hex UNITS`, `--land-mip-bias`): 3 offset-only patches blended with
  variance-preserving barycentric weights. SHIPS OFF: repeat gate 6 of 7 on both sheet sets (reds (-4,-20) 0.308,
  (-12,-20) 0.279 vs 0.264 ceiling).
- TILING4's report + instruments: scratchpad/lane_tiling4_report.md and
  E:\Projects\NifskopeWildWastelandEdition\scratchpad\tiling4_20260912\ (selection + validation sheet sets, repeat /
  swirl / grain gates, SPLAT1's offline model). REUSE these gates and frozen sheet sets unchanged.
- The terrain colour sheet also blends several land textures per texel (the LTEX layer splat); today that is a plain
  crossfade -> soft, smeary transitions ("terrain blending on lods").

## The work
1. Height-aware blending, BOTH places:
   a. between the 3 hex patches (Heitz-Neyret joins): raised features win at the joins, with a controlled contrast;
   b. between land texture layers (dirt/grass/rock/road transitions in the splat): height-based blend instead of a
      linear crossfade.
   Height source: FO4 land textures usually carry no height map. Choose by measurement (diffuse luminance, normal-map
   derived height, or a per-texture blend) and say why with numbers. Blend sharpness is one constant, tuned against the
   gates; no INI knobs.
2. Large-scale variation: a smooth world-space field (tens of metres to ~1 km, pure function of world position, so no
   seams and thread-count-invariant) that varies the land colour's brightness/hue/saturation slightly. Its amplitude is
   set by MEASUREMENT against vanilla's own LOD sheets' large-scale variance (never more than vanilla shows).
   Saturation must never drop below the unmodified bake's (hard line). Not correlated with the hex patches.
3. Switches: both go behind `--land-sample stochastic` (or a new mode name if cleaner); off = today's bytes exactly.
   Report whether the 7-of-7 repeat gate now passes. The default stays OFF; turning it on by default is bungo's call,
   and the report gives him the numbers to make it.

## Gates
- Off = night-trial's Boston terrain sheets byte for byte (ww-off-identity-cross-exe skill if exes differ).
- TILING4's repeat / swirl / grain gates on BOTH frozen sheet sets: current hex vs hex+height vs hex+height+macro.
  Target: repeat 7 of 7; swirl no regression; grain no regression.
- Layer transitions: a measurement that fails on a plain crossfade (e.g. transition-zone contrast / edge sharpness vs
  vanilla's own LOD sheets at the same places), shown failing on today's bake first.
- Macro: large-scale variance within vanilla's range; saturation >= unmodified per sheet; no seam at chunk edges
  (bake at 1 and 16 threads, byte-identical).
- Whole-map terrain bake time before/after.

## Pictures (full size, 60 px title bar, never a contact sheet)
maps1 Boston camera + one wide rural camera (hills, where repetition is worst): terrain colour sheet today vs on,
and a 4x crop of one layer transition (dirt->grass) today vs on. Also one lit-view pair if the lit view is in the tree.

## Rules
- NifSkope runs: `bash E:/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire tiling5 21600`, release
  after EACH run; lanes WATER1 and GATES share it. Second monitor (WW_WINDOW_AT=1920,0), unused --port, one instance.
- Build: nifskope-ww-worktree-build skill (objects: sibling NifskopeWWE-night is built at night-trial @ 5b338d39 --
  section 5b). ONE build at a time on the machine: check no make/g++/cc1plus is running first. Game gate first.
- Never kill anything; bungo's NifSkope (main tree) stays untouched. Commit by explicit path; never -a/-A, stash, push.
- Public repo: no game data, no binaries, no PDB names.
- Report sections: 1 skills loaded; 2 height source choice + numbers; 3 gates table (expected/measured/PASS-FAIL);
  4 pictures; 5 cost; 6 DELIVERABLE_TEXT (WW_CHANGES entry); LAST: skills used / wished / written (write missing ones
  under E:\Projects\Claude\.claude\skills\<name>\SKILL.md and copy to E:\Tools\AISkills).
