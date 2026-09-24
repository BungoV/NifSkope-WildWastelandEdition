# Lane IMPOSTORFIN1 -- finish the tree cards (octahedral impostors)

Director brief, 2026-09-22 21:5x. Model: Opus 5.5. You own the BUILD + EXE slot (no other lane is live).
Folder: scratchpad/impostorfin1_20260922/ . Write your notes there INCREMENTALLY (a line per finished step
in progress.md) -- lanes die silently and the director checks the disk.

## Read first
1. CONSTITUTION.md, then HANDOFF.md lines 1-10 and the STOP line of 2026-09-20 (grep "STOP 00:55").
2. scratchpad/impostorlook1_20260919/FOR_BUNGO.md + DELIVERABLE_TEXT.md (the draw finding: the shader tests
   coverage then paints alpha 1.0; hookup_cardlight.py; shoot_missing.sh; big-frame maple bakes).
3. scratchpad/impostorfix2_20260919/report.md section 6 (the `_n` height<->sway swap).
4. scratchpad/impostorfix4_20260919/DELIVERABLE_TEXT.md + impostorfix5_20260919 (alpha cut 0.20 measurements).
Skills: nifskope-ww-build-verify, nifskope-ww-render-shot, nifskope-ww-lodgen, ww-reference-card-diagnose,
ww-anchored-hookup, ww-test-harness-add, fo4-bridge not needed.

## bungo's words (2026-09-22)
"Finish up what remains with lod gen, show me the rendered tree cards, I have not decided on transparency
cut off, but what vanilla game had worked pretty well."
=> THE CUT-OFF IS VANILLA'S. Do not pick 0.20 or any simulated optimum. Measure what vanilla uses and ship that.

## Jobs, in order
1. VANILLA CUT-OFF CENSUS (offline, before any build). Vanilla corpus = E:\Tools\Fallout 4\DataUnpacked\Data
   (NOT a mod folder). Over EVERY tree/foliage LOD mesh vanilla ships (meshes\lod\ and whatever the tree LOD
   actually lives in -- find it, say where): the alpha test threshold (NiAlphaProperty threshold + flags, and
   the material's alpha test ref if a BGSM carries one). Print the TABLE (value -> count, with 3 example paths
   per value), not just a mode. Also say HOW vanilla uses it (hard test on texture alpha? blend?). If vanilla
   has more than one value, ship the dominant one for trees and name the others. Whole corpus, not a sample.
2. Take the rung: release/NifSkope.before_impostorfin1.exe (copy of the current exe, hash both).
   Game must be DOWN before any build (check Fallout4.exe). bungo's own NifSkope is open (pid 8728 at 21:54):
   NEVER kill it; rename the exe aside per nifskope-ww-build-verify. One harness NifSkope at a time,
   second monitor only, never focus-steal.
3. Apply: the vanilla cut-off as the draw's default (src/gl/impostordraw.cpp ~:477 and the frag it feeds, per
   the IMPOSTORLOOK1 finding), hookup_cardlight.py (re-run its --check first; the tree moved since 19 Sep),
   the big-frame maple bakes (256 and 512), and shoot_missing.sh so blast_n8 / dead_n4 / rock_n4 get their
   mesh and card cells.
4. The `_n` swap (height to ALPHA, sway to BLUE) is NOT RULED. PREPARE it only: an anchored hook-up script
   with --check (writer in src/lodgen.cpp, reads in res/shaders/impostor_oct.frag, the spec line in
   docs/LODGEN_IMPOSTOR_SPEC.md:45, a .lodm version bump so an old sheet is refused). Do NOT apply it.
   If the director sends you his yes mid-lane, apply it, rebake, and gate it in the same build.
5. Build (gated chain, make's own exit code), rebake the five sets, run impostor_draw.sh, lodgen_octahedral.sh,
   native_lighting as the control. Never lower a bar to pass.
6. PICTURES for bungo (the deliverable he asked for): for all five subjects, a fresh
   "every bake angle" sheet (mesh | card as drawn | raw frame) on the new exe, plus one before/after pair per
   subject (before = rung exe, after = new exe, same views). Put the two best in FOR_BUNGO.md first.

## End
- DONE marker whose first word is DONE, PARTIAL or PENDING. FOR_BUNGO.md in plain words, short.
- DELIVERABLE_TEXT.md: WW_CHANGES entry text, MISTAKES entries, any skill update -- the director splices docs.
  Do not edit HANDOFF.md, WW_CHANGES.md or MISTAKES.md yourself. Do not commit.
- Report back in under 300 words: verdict line, the vanilla cut-off value + table, gate numbers, picture paths,
  exe size/time/sha1.
