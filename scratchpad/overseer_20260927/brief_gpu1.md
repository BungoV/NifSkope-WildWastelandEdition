# Lane GPU1 -- measure the LOD bake, then move its heaviest stages onto the GPU

Worktree: E:\Projects\NifskopeWWE-gpu1, branch gpu1-20260926 (created from main 422881d4 = ROADS1 + FLAT1 + AO2 merged).
Read E:\Projects\ClaudeNifskope CONSTITUTION and the NifSkope HANDOFF top block first (E:\Projects\NifskopeWildWastelandEdition\HANDOFF.md).
Skills to load and use: nifskope-ww-lodgen, nifskope-ww-build-verify, ww-measure-before-you-parallelise,
ww-parallelise-a-stage, ww-module-off-is-identical (repo tree: <worktree>\.claude\skills).

## His words, verbatim (2026-09-26 22:0x)
- "The next question after that is gonna be, how can we reduce the bake time?"
- "Is GPU used for this?" (answer: no -- the whole bake is CPU; his GPU is an RTX 5070 Ti 16 GB, idle during bakes)
- "Yes, GPU would reduce bake time by a lot"

## Known numbers (whole Commonwealth, object bake, lane AO2 log
E:\Projects\NifskopeWWE-bake2\scratchpad\ao2_20260926\whole\after\bake.log)
- whole object bake 5386 s: meshes 3341 s (of which library instances 1810 s, the AO face cast 194 s timed alone),
  textures 1128 s, impostors 136 s; model workers 7, ladder workers 17, 16 hardware threads.
- Region Boston: 17 chunks on 16 threads, the heavy Boston chunks serialise.
- The terrain VT bake (the .lodt sheets: road/pavement/flat-object painting, fill, cover, height) is a separate run of
  ~75 min whole-map (lane SEAM1, 2026-09-25, E:\Projects\NifskopeWildWastelandEdition\scratchpad\seam1_20260925\progress.md).
- Texture compression is CPU (src/lodgenbc7.h and the DXT paths); AO is a CPU ray cast (src/lodgenao.h, src/nativeemit.cpp).

## The work
1. MEASURE FIRST (report section 2). A per-stage and per-sub-stage profile of BOTH bakes (object and terrain VT),
   with thread occupancy (how much of the wall clock each stage has all 16 threads busy vs waiting on one chunk).
   Use the Boston box (-8 -12 3 -1) for fast runs, and scale by the whole-map logs you already have; run a whole map
   only if a number can come from nowhere else. Output: a ranked table -- stage, wall seconds whole-map, what bounds
   it (CPU compute / one-thread tail / disk / decode), and the estimated saving of (a) a GPU port, (b) a CPU fix
   (finer chunk split, more workers, cache reuse). Commit this table before writing any GPU code.
2. BUILD the top GPU candidates in ranked order, expected: the AO/sky ray cast, BC7/DXT texture compression, the
   terrain sheet painting. Use what NifSkope already links (Qt OpenGL: an offscreen context + OpenGL 4.3 compute
   shaders), no new vendor SDK unless you show OpenGL cannot do it. The headless `-no-gui lodgen` path must get a
   GPU context without a window on screen.
   - AO code: lane AO2 is fixing two AO defects in src/lodgenao.h / src/nativeemit.cpp right now, in
     E:\Projects\NifskopeWWE-bake2. Do the GPU AO LAST, and before you start on it, merge main again (the AO fix will be
     there) -- ask the overseer by writing "AO2 merged?" in your report if it is not.
3. DETERMINISM IS THE GATE. Every bake is gated by file hashes. A GPU stage must:
   - give byte-identical output across two runs on this machine (show both hashes);
   - match the CPU path exactly where exact arithmetic allows it (integer/fixed-point results, sorted reductions --
     no float atomics in any reduction), and where it cannot (a different BC7 search), be no worse than the CPU path
     on the error measure the CPU stage already uses, with the numbers per texture class;
   - fall back to the CPU path by itself when no GPU context can be made, and say so in the log. A command-line
     switch to force the CPU path is fine; the GPU is the default when it passes the gates (no fix-only toggles).
4. Report the new stage times against the old, per stage, and the whole-map estimate.

## Gates
- CPU vs GPU output of the Boston box: the file hash table, identical or explained file by file with numbers.
- Two GPU runs: identical hashes.
- The AO spot gates from lane AO2's DONE.md section 6.3 (ballpark roof, tower face, lone box) pass on the GPU path.
- A picture: the Boston oblique (persp shot.sh, below) CPU path over GPU path, which must look the same.
  C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\persp\shot.sh
  (copy it; env SHEETS=<your bake's terrain sheets dir> and LODI_DIR=<dir with .lodi/.lodo>; command:
  `SHEETS=... LODI_DIR=... WW_LODL_AO=1 LV=2 SLOT=0 SDIM=2 bash shot.sh <out>.png "E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth" Commonwealth -5 -10 2 -3 8 16384 1600 1600 <unused port>`).
- Game gate before every build and bake:
  `if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi`
- One build on the machine at a time: before `make`, check no other make/g++/cc1plus runs
  (Get-CimInstance Win32_Process); if one does (lane AO2 builds in NifskopeWWE-bake2), wait for it, never kill it.
- One headless NifSkope bake at a time: E:\Projects\NifskopeWWE-fix1\scratchpad\fix1_20260926\turn.sh acquire gpu1 /
  release gpu1 around every NifSkope run (read the script first: every verb but `release` acquires). The overseer will
  run a whole-map bake tonight that holds the turn for ~2 hours; do measurement and code while you wait.
- Avast kills fresh exes run from lane worktrees: copy the built exe + DLLs to a run folder under YOUR scratch and run
  from there.

## Rules
- Commit early and small, by explicit path only (never -a/-A), in your worktree. Never git stash. No push, no merge
  into main, no install.
- Never write NifSkope main (E:\Projects\NifskopeWildWastelandEdition), its ledgers, or anything under
  E:\Projects\Fallout 4 Mods\. Never kill his NifSkope window or MO2; never kill anything by image name.
- Never touch any CORE file anywhere (E:\Projects\CORE or CORE material in any scratch folder).
- The repo is public: no game data, extracted textures, binaries, PDB names committed. Pictures and bake output stay
  out of git.
- Disk: E: has ~134 GB free. Keep bakes to the Boston box; delete your own sheet caches and bake output when done.
- Patch scripts through the Write tool, never a bash heredoc or python -c (backslashes).
- A tool or permission refusal: stop that step, record it, do not route around it.
- Write deliverable text for the ledgers (HANDOFF / WW_CHANGES / MISTAKES) into scratchpad/gpu1_20260926/DELIVERABLE_TEXT.md.

## Report
Incremental report at scratchpad/gpu1_20260926/DONE.md in the worktree, written as you go (a crash must not lose work):
1. Skills loaded. 2. The profile and the ranked table. 3. What was built, per stage, and how it stays deterministic.
4. Gates with numbers. 5. Old vs new stage times and the whole-map estimate. 6. Commits. 7. Pictures (paths).
8. Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote. A procedure you
re-derived from memory or worked out from first principles is a missing skill -- write it under
.claude/skills/<name>/SKILL.md before you finish. Declining is allowed: name the procedure and say why it will not recur.
Your final message: short, plain words, no invented names -- the old and new bake times, and what is left.

## Addendum (22:31, sent by message)
bungo: "Make GPU work enabled by default, but make it so you can disable if needed in nifskope options."
-> GPU default ON; one Settings row to turn it off (CPU path); headless lodgen honours it; CPU-force switch overrides;
log says which path ran and why; GUI harness picture of the row.
