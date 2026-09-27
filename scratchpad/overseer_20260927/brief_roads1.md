# Lane ROADS1 -- pavements back into the LOD terrain colour, faithful to the in-game texture; riverside roads

Worktree: E:\Projects\NifskopeWWE-roads1, branch roads1-20260926 (already created at 6382a09a = incr2-20260926 head, the
same base as lane AO2 in E:\Projects\NifskopeWWE-bake2 -- do not touch that worktree or src/lodgenao.h beyond need).
Read E:\Projects\ClaudeNifskope CONSTITUTION and the NifSkope HANDOFF top block first (the ww-* skills apply).

## His words, verbatim
- "I approve of the roads"
- "if that is their in game texture, it is their texture on our terrain too"
- (with a picture of the Boston riverbank west of Diamond City: three grass strips along the river bank, circled, beside
  two bridges) "Is there no roads here?"

## What is already known (lane ROADS0, scratch: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\roads1\ -- its scripts bake_region.sh, shot.sh, street_census*.py, sample.py, compare.py are yours to reuse)
- Terrain-colour road stamping refuses Landscape\Sidewalks models by default (lodgenIsSidewalkModel, src/lodgen.cpp ~8539,
  refusal ~9097). `--road-sidewalks` restores them: 3,994 pavement pieces, 14.7% of the Boston box.
- With them, pavement texels average 134.2 luminance vs 89.9 installed and 89.1 in Bethesda's own terrain LOD, with
  visible 2x2 tile edges.
- His ruling: Bethesda's LOD is not the target. The IN-GAME texture of those pavement models is. If 134 is what their
  in-game diffuse texture really averages, 134 is right.

## The work
1. Measure faithfulness: for the sidewalk models that stamp, read their actual in-game diffuse textures (material/BGSM ->
   _d.dds, the mip that matches the texel footprint), compute the average colour the stamp SHOULD produce, and compare
   with what our stamp writes on the same texels. Say the two numbers. If they match within a few levels, the colour
   stays. If they do not (e.g. we sample the wrong texture, the wrong mip, skip the material tint, skip vertex colour,
   or sRGB/linear is mixed up), fix the stamp so it reproduces the in-game texture -- never tune toward Bethesda's 89.
2. The 2x2 tile edges: decide with a measurement whether they are in the in-game texture (real slab joints -- keep) or a
   bake artifact (gaps/overlap between placement footprints, per-piece edge falloff, texel-snapping seams -- fix).
3. Default ON: pavements become part of the default road stamping (no fix-only toggle; `--road-sidewalks` may stay as a
   no-op alias or be removed -- say which; a flag to turn them OFF is fine if the code already has the pattern).
4. The riverside: locate the three circled strips (Commonwealth, Charles River bank just west/north-west of Diamond City,
   beside two road bridges). List every placement whose footprint crosses those strips: model path, base FormID, plugin,
   and what our road stamp did with it (stamped / refused + reason / not a road model / has no footprint). Report
   whether in game there is a road or path there, and if a road model is refused or missed, fix it the same way
   (in-game texture, default on). If there is genuinely no road in game there, say so plainly with the evidence.
5. Test bake of the Boston box only (bake_region.sh pattern), then pictures: the downtown close-up and the riverside strips,
   each as installed | new, plus the numbers. Put them under scratchpad/roads1_20260926/pics/ in the worktree.
   Do NOT do the full Commonwealth bake and do NOT install anything: the full re-bake is combined with lane AO2 later.

## Gates
- Faithfulness number: our stamped pavement texel average vs the in-game texture's expected average, on >= 200 sampled
  pavement texels, stated both ways. A gate that would pass on the old code is not a gate: show it fails on 6382a09a if
  the stamp changed.
- Nothing outside pavements/roads moved: diff the new Boston-box terrain colour sheets against a same-code bake with
  pavements off; every changed texel must lie under a road/pavement footprint (count any that do not, must be 0 or explained).
- Build: the NifSkope build the constitution names. Avast kills fresh exes run from lane worktrees
  (NifskopeWWE-*): copy your built exe + its DLLs to a run folder under your scratchpad (as lane INCR2 did with
  ...\scratchpad\incr2\ns_run1\) and run it from there. One harness/headless NifSkope on the machine at a time
  (turn.sh acquire/release as in bake_region.sh).
- Game gate before every bake: `if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi`

## Rules
- Commit early and small, by explicit path only (never -a/-A), in your worktree. Never git stash. No push, no merge.
- Never write NifSkope main (E:\Projects\NifskopeWildWastelandEdition), its ledgers, or anything under
  E:\Projects\Fallout 4 Mods\ (no install). Never kill his NifSkope window or MO2; never kill by image name.
- The repo is public: no game data, extracted textures or binaries committed. Pictures and scratch stay out of git
  unless the repo already tracks that scratch path pattern.
- Write deliverable text for the ledgers (HANDOFF / WW_CHANGES / MISTAKES) into scratchpad/roads1_20260926/DELIVERABLE_TEXT.md.

## Report
Incremental report at scratchpad/roads1_20260926/DONE.md in the worktree, written as you go:
1. Skills loaded. 2. Faithfulness numbers and what (if anything) was wrong in the stamp. 3. Tile edges: real or artifact,
with the measurement. 4. Riverside: the placement list and the verdict. 5. Commits. 6. Gates with numbers.
7. Pictures (paths). 8. Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote.
A procedure you re-derived from memory or worked out from first principles is a missing skill -- write it under
.claude/skills/<name>/SKILL.md before you finish. Declining is allowed: name the procedure and say why it will not recur.
Your final message: short, plain words, no invented names.
