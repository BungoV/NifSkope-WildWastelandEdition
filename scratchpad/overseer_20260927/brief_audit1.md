# Lane AUDIT1 -- every baked LOD map: what it must ultimately be, and what it is right now (READ-ONLY)

No worktree, no build, no bake, no code change. Read-only audit plus pictures in scratch.
Report: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\audit1\DONE.md
(write it incrementally, one map group at a time -- a crash must not lose work).

## His words, verbatim (2026-09-27, after seeing the 79 full-size maps)
"Ground contact on buildings, does that look right to you, it's a texture map that is not usable? 04, is this the
identity map? 05 is unusable, object id needs to be per building, not a texture map it is, 10 does that look right to
you, same one single texture for all the buildings, normal map on the ground has no details baked from the objects
(roads, railway tracks, concrete pieces, decals, etc) 36, cell height range, what does it do? why is it so pixelated.
water doesn't actually show water, ground cover is empty, water meshes are just the terrain meshes? the water should
be flat, you're not actually showing me the water with baked data"
"review everything else for what it ultimately needs to be, and what it actually is right now"

## Inputs
- The 79 labelled pictures: ...\scratchpad\maps1\full\NN_<key>.png (titles burned in). Small copies of 10 in maps1\rv\.
- How each was made, the numbers and the refuters: ...\scratchpad\maps1\DONE.md (read it first), maps1\*.json.
- Code + docs (read-only): E:\Projects\NifskopeWWE-bake2 (branch ao2-20260926). Key docs: docs/LODGEN_NATIVE_LODO_LODI.md,
  LODGEN_TERRAIN_VT.md, LODGEN_BTD_FORMAT.md, LODGEN_VERTEX_PACKING.md, LODGEN_IMPOSTOR_SPEC.md,
  LODGEN_TEXTURE_ARRAYS.md. Viewer channel code: src/lodinative.cpp ~419-434 and ~1211, src/btdterrain.cpp ~743-1500.
- The consumer: FO4CS (read-only, never build/commit/touch): E:\Projects\Fo4CommunityShaders\wt-fixfirst -- its LOD
  file reader (wave 95 READER1) and any shader/plan doc that says what it will do with each field. Also the campaign
  plans in C:\Users\bungo\.claude\projects\E--Projects-Claude\memory\project_hybrid_lod.md and
  project_physical_atmosphere.md (read-only).
- Facts the overseer already checked (confirm or refute, do not just repeat):
  * `ground` (.lodi 0x12) is ONE byte per placement, but LODGEN_VERTEX_PACKING.md:44 defines ground contact as
    per-vertex "1 at the terrain surface, 0 by 256 world units above it". Picture 03 shows whole buildings one flat grey.
  * 04 `identity` = the group table (one colour per group -- is a group one building? prove it on the ballpark and the
    two towers); 05 `identityraw` = the raw id as grey; 10 `placement` = one colour per placed kit piece.
  * Terrain normal (VT msn) is built from the heightmap only; LODGEN_TERRAIN_VT.md:792 says roads are deliberately
    left out because vanilla's _msn leaves them out. Roads/flat objects are painted into colour only.
  * 36 cellrange = per-cell min/max height from the .lodl cell table: one value per cell by nature.
  * No code in the viewer draws a water SURFACE (grep drawWater/water surface found nothing): every water picture is
    the data painted onto the terrain mesh.
  * .lodl ground cover plane: GCVR count 0 on this bake; VT mask A (cover) has data (mean 31.5).

## The work
For EVERY one of the 79 pictures (and for any baked field MAPS1 listed as "no view"), one row:
1. What it is for: who reads it (FO4CS shader X / FO4CS culling / the bake itself / the NifSkope viewer only / nobody),
   with file:line or doc:line.
2. What it must ultimately be (resolution, per-vertex / per-pixel / per-object / per-cell, what it should look like).
3. What it is right now (look at the picture yourself, and read the bytes where the picture could mislead).
4. Verdict, one of: RIGHT | RIGHT BUT THE PICTURE MISLEADS (say how the view should draw it) | WRONG DATA |
   WRONG RESOLUTION/GRANULARITY | EMPTY (why: switch off, not built, format too old) | NOT A MAP (structure only; should
   not be shown as a picture) | DUPLICATE of another row.
5. The fix, one line, and its size (view change / bake switch / bake code / format change / FO4CS side).
Answer each of his named points first, in plain words, as its own section, with the evidence number for each.
Then the full table grouped: buildings (per-object and per-vertex), building textures + tree cards, ground, water.
Last: a ranked list of what is WRONG or EMPTY and matters for the final product, biggest first.

Pictures: where a view misleads (water on the terrain mesh, identity), you may make an OFFLINE picture in
scratchpad\audit1\pics that shows it the right way (e.g. water drawn as flat surfaces at their body heights over the
terrain, from the v3 file ...\scratchpad\maps1\water\; the Boston camera of maps1). Label every picture in plain words
at the top (60 px bar) and at full size, never a contact sheet. No renderer launches needed; if you do launch NifSkope
headless, use E:\Projects\NifskopeWWE-fix1\scratchpad\fix1_20260926\turn.sh acquire audit1 / release audit1, second
monitor only.

## Rules
- Read-only everywhere except scratchpad\audit1\. No builds, no bakes, no commits, no git stash, no installs.
- Never write anything under E:\Projects\Fallout 4 Mods\, NifSkope main, FO4CS, or any CORE file.
- Scope every search to one folder; never grep a project root. Tool output lean: verdict lines, not dumps.
- Scripts through the Write tool, never a heredoc or python -c with backslashes.
- A tool or permission refusal: stop that step, record it, do not route around it.
- Plain words in the report; no invented names; never "fixed/final/true".

## Report sections
1. Skills loaded (use nifskope-ww-lodgen, search-lean, ww-channel-view-refuter, ww-legend-matches-picture, ww-texel-picture).
2. His named points, answered. 3. The full table. 4. Ranked wrong/empty list with fix sizes. 5. Pictures (paths).
6. Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote. A procedure you
re-derived from first principles is a missing skill -- write it under E:\Projects\Claude\.claude\skills\<name>\SKILL.md
and E:\Tools\AISkills, or say why it will not recur.
Final message: short, plain words, the ranked list and the picture paths.
