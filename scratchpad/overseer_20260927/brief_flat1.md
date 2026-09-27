# Lane FLAT1 -- paint flat ground objects into the LOD terrain colour, by a measured rule + a user override file

Worktree: E:\Projects\NifskopeWWE-flat1, branch flat1-20260926 (already created at d4c64ef7 = roads1-20260926 head:
pavements default on, material swaps applied, has-LOD ground pieces painted). Lane AO2 works in
E:\Projects\NifskopeWWE-bake2 (vertex AO) -- do not touch that worktree; stay out of src/lodgenao.h.
Read E:\Projects\ClaudeNifskope CONSTITUTION and the NifSkope HANDOFF top block first (the ww-* skills apply).
Read the ROADS1 report first: E:\Projects\NifskopeWWE-roads1\scratchpad\roads1_20260926\DONE.md and its work\ scripts
(faith_cmp.py, confine.py, pave_faith.py, haslod_census.py, pics.py, bake.sh) -- reuse them, do not re-derive.

## His words, verbatim (2026-09-26, after seeing the roads bake)
- "awesome roads, anything flat or near the terrain that could also be baked into it?"
- (my list: parking lots / concrete pads / slabs and house foundations; railway tracks and their gravel beds; dirt paths
  and trails that are separate models; flat decals -- road cracks, paint lines, oil stains, manholes, drains, grates;
  flat debris -- leaf litter, flattened rubble, gravel patches. Stays out: anything that stands up -- curbs, guard rails,
  fences, low walls, car wrecks -- and anything that already has its own distant model, so it is not drawn twice.)
- "By rails you mean railway tracks?" (yes: rails, sleepers, gravel bed)
- "Good, do the 5 you listed"
- "So, I take there needs to be some kind of file that specifies which ones of these can be baked into the terrain?"
- "I'm asking this, because some mods can add new objects"
- (agreed design) "Sounds good": a RULE decides on every bake from his current load order; a short OVERRIDE FILE holds
  only his exceptions (bake / no-bake per model), kept across bakes; every bake writes a REPORT of what it painted and why.

## The work
1. The rule (in lodgen, next to the road stamping -- LodgenRoadSet, lodgenIsSidewalkModel ~src/lodgen.cpp 8539):
   a placed base qualifies when, MEASURED from its mesh and placement (scale + rotation applied):
   - its height above the terrain under its footprint is low (start at ~70 units = 1 m, justify the final number with
     the census: show the height histogram and where standing objects begin),
   - it sits on the ground (its underside within a tolerance of the heightfield; not over water, not on a bridge or
     overpass -- reuse ROADS1's raised rule),
   - it is not already drawn as a distant object at the same level (no double draw; say how roads1's has-LOD rule
     applies here and keep them consistent).
   The rule is kind-agnostic and path-agnostic: it must catch a mod's flat object with no list entry. The five kinds
   above are LABELS for the report and pictures, not the gate.
2. Paint faithfully, as ROADS1 did: the in-game diffuse texture through the material (BGSM/BGEM, MSWP swaps, tint,
   vertex colour, correct mip, sRGB right). Decals and anything alpha-blended/alpha-tested composite with their alpha
   OVER what is already painted (roads, pavements) in a stated order; opaque pads/tracks replace. Never tune toward
   Bethesda's LOD colours.
3. The override file: plain text, one line per model path (and optionally a folder prefix), `bake` or `nobake`,
   `#` comments. Pick its location and name (user-editable, never overwritten by a bake, read on every bake; a flag may
   point elsewhere) and say it. Lines win over the rule. Ship it EMPTY except a header comment (no pre-filled list).
4. The per-bake report: every base model the rule looked at in the baked region -- model path, plugin, placements,
   measured height, decision (painted / refused + reason / overridden), kind label, texels covered. Written beside the
   bake output (not into the .lodt).
5. Default ON (his "do the 5"), with an off flag if the code has the pattern. No fix-only toggles.
6. Census first, then code: run the rule read-only over the Boston box (-8 -12 3 -1) and list the top models per kind
   by texels, plus the top REFUSED ones and why -- this is the evidence for the thresholds.
7. Test bake of the Boston box only (ROADS1 bake.sh pattern), then pictures (under scratchpad/flat1_20260926/pics/):
   - top-down close-ups installed | roads1 new_default | flat1, for one spot per kind (a rail line, a parking lot,
     a decal-heavy street, a path, a debris patch),
   - the oblique Boston view, roads1 new_default over flat1, using
     C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\persp\shot.sh
     (copy it; env SHEETS=<your bake's mod\FO4CSLOD\Commonwealth> points the terrain sheets at your bake; command:
     `SHEETS=... WW_LODL_AO=1 LV=2 SLOT=0 SDIM=2 bash shot.sh <out>.png "E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth" Commonwealth -5 -10 2 -3 8 16384 1600 1600 <unused port>`).
   Do NOT bake the whole Commonwealth and do NOT install anything: the full re-bake is combined with ROADS1 + AO2 later.

## Gates (numbers in the report; a gate that passes on the old code is not a gate)
- Confinement: diff your Boston sheets against ROADS1's new_default bake
  (C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\roads1b\bake\new_default,
  or rebake it with your exe and the feature off): every changed texel lies under a painted footprint (4x4 block
  allowance as ROADS1); count the outliers, 0 or explained.
- Faithfulness per kind: painted texel average vs the in-game texture's expected average on >= 200 texels per kind.
- Standing objects stay out: name >= 5 standing models in the box (fence, car wreck, curb, guard rail, low wall) and
  show each refused with its reason.
- Mods: show at least one qualifying placement from a non-Bethesda plugin in his load order painted by the rule with
  no file entry (or say plainly none in the box qualifies, with the count of mod placements examined).
- Override file: one `nobake` line reverts that model's texels to the rule-off bake exactly; one `bake` line on a
  refused model paints it. Show both.
- Build: the NifSkope build the constitution names. Avast kills fresh exes run from lane worktrees (NifskopeWWE-*):
  copy the built exe + DLLs to a run folder under YOUR scratch (as ROADS1 did) and run from there. One headless
  NifSkope on the machine at a time: turn.sh acquire/release as in bake.sh (AO2's bake may hold it; wait, never kill).
- Game gate before every bake and every build:
  `if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi`

## Rules
- Commit early and small, by explicit path only (never -a/-A), in your worktree. Never git stash. No push, no merge.
- Never write NifSkope main (E:\Projects\NifskopeWildWastelandEdition), its ledgers, or anything under
  E:\Projects\Fallout 4 Mods\ (no install). Never kill his NifSkope window or MO2; never kill by image name.
- Never touch any CORE file anywhere (E:\Projects\CORE or CORE material in any scratch folder).
- The repo is public: no game data, extracted textures, binaries, PDB names or model lists from game data committed.
  Pictures and scratch stay out of git unless the repo already tracks that scratch path pattern.
- Disk is tight (E: ~100 GB free): keep bakes to the Boston box, delete your own intermediate sheet caches when done.
- Write deliverable text for the ledgers (HANDOFF / WW_CHANGES / MISTAKES) into scratchpad/flat1_20260926/DELIVERABLE_TEXT.md.
- A tool or permission refusal: stop that step, record it in the report, do not route around it.

## Report
Incremental report at scratchpad/flat1_20260926/DONE.md in the worktree, written as you go:
1. Skills loaded. 2. The census (thresholds and the histogram numbers). 3. The rule as built, and the override file
(path, format). 4. Faithfulness numbers per kind, and decal compositing order. 5. Commits. 6. Gates with numbers.
7. Pictures (paths). 8. Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote.
A procedure you re-derived from memory or worked out from first principles is a missing skill -- write it under
.claude/skills/<name>/SKILL.md before you finish. Declining is allowed: name the procedure and say why it will not recur.
Your final message: short, plain words, no invented names.
