# Lane SHADOW1 -- cast sun shadows in the lit LOD view

Worktree: E:\Projects\NifskopeWWE-lit1, branch lit1-20260927 (continue on it; head b5ca755e = lane LIT1's lit view).
Read E:\Projects\ClaudeNifskope CONSTITUTION, the NifSkope HANDOFF top block, and LIT1's report first:
E:\Projects\NifskopeWWE-lit1\scratchpad\lit1_20260927\DONE.md (how the lit view is built, how it was rendered, the
run-folder pattern, the port collision mistake, the antivirus launch outage).
Skills: ww-lod-lit-view (repo tree .claude\skills), nifskope-ww-build-verify, nifskope-ww-render-shot,
ww-channel-view-refuter, ww-module-off-is-identical, search-lean.

## His words, verbatim
- 2026-09-27 (earlier): "render the whole terrain with real lighting, so make normals, specular and gloss and ao work
  and render" / "terrain, including the lod meshes and trees"
- 2026-09-27 (after seeing it): "there's no shadows on your real render"

## The work
1. Cast shadows from the lit view's one sun (WW_LODL_LIT=1): a sun shadow map over everything drawn -- terrain
   (hills shadow the ground), LOD building meshes, bridges, rocks, tree meshes (a tree's cut-out leaves cast only
   where their texture is opaque, the same alpha test the view already draws them with). Receivers: terrain and every
   mesh. The sun direction is the one the lit view already uses.
2. It must hold at BOTH scales rendered so far: the Boston chunk camera and the whole-Commonwealth oblique. Fit the
   shadow map to what the camera sees; if one map is too coarse for the whole map, use cascades or tiles -- pick by
   measurement (shadow edge width in pixels on screen) and say which and why.
3. Filtering: soft enough to not stair-step, no acne on flat roofs or flat ground, no shadow detached from its caster
   at the foot of a wall. Measure each (below), do not eyeball.
4. `WW_LODL_LIT_TERM=shadow` shows the shadow term alone (white lit, black shadowed). With WW_LODL_LIT unset the view
   stays byte-identical to today's (LIT1's off gate), and WW_LODL_LIT=1 with shadows forced off must equal LIT1's lit
   picture byte for byte -- give that force-off an env switch for the gate only (WW_LODL_LIT_SHADOW=0), not a menu row.
5. Commit early and small by explicit path.

## Gates (numbers in the report)
- Off gate: unset = byte-identical to LIT1's off_base.png; shadow forced off = byte-identical to LIT1's lit_all.png.
- Repeatable: two shadow runs byte-identical.
- Direction refuter: rotate the sun azimuth by 180 deg -> shadows move to the other side of the two towers (measure the
  dark area's centroid offset from each tower base: sign flips).
- Length refuter: one tower's shadow length on flat ground vs height / tan(sun elevation), within 10%.
- Acne: fraction of up-facing lit-side texels (flat roofs, open flat ground) marked shadowed < 1%.
- Contact: at three wall feet, the gap between wall and shadow <= 2 screen pixels.
- Whole map: every placement still drawn (LIT1's count 184,431), 0 dropped.

## Pictures (scratchpad\shadow1_20260927\pics\, not in git)
Each ONE picture, FULL SIZE, a plain-words title burned into a 60 px bar at the top. NO side-by-side sheets, no
contact sheets (bungo: "I can't judge anything if you cram those small previews into that sheet"):
- Boston, 08_roads_AO_decal camera: lit with shadows; the shadow term alone.
- Whole Commonwealth oblique (LIT1's whole camera, both halves stitched as LIT1 did): lit with shadows.
- A close crop at 2x of the two towers with shadows.
- Refuter pictures as needed, each its own file.

## Rules
- Game gate before every build and render:
  `if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi`
- One build on the machine at a time: check for running make/g++/cc1plus first; wait, never kill.
- One headless NifSkope at a time: E:\Projects\NifskopeWWE-fix1\scratchpad\fix1_20260926\turn.sh acquire shadow1 /
  release shadow1 (read it first: every verb but `release` acquires). Unused ports per run, never a fixed base.
- Run built exes from a copy under your scratch run folder (the antivirus kills fresh exes in worktrees).
- Any GUI on the second monitor (x=1920,0); never take focus; never kill his NifSkope or MO2; never kill by image name.
- Commit by explicit path only, never -a/-A, never git stash. No push, no merge into main, no install.
- Never write NifSkope main, its ledgers, anything under E:\Projects\Fallout 4 Mods\, or any CORE file.
- Public repo: no game data, textures, binaries, PDB names, pictures in git.
- Patch source and scripts through the Write/Edit tools only, never a heredoc or python -c.
- A tool or permission refusal: stop that step, record it, do not route around it.
- Deliverable text for the ledgers (HANDOFF / WW_CHANGES / MISTAKES) into scratchpad\shadow1_20260927\DELIVERABLE_TEXT.md.

## Report
Incremental, scratchpad\shadow1_20260927\DONE.md in the worktree:
1. Skills loaded. 2. Design (map / cascades / tiles, resolution, filter, bias) and why, with the measurements.
3. Gates with numbers. 4. Commits. 5. Pictures (paths). 6. What still looks wrong.
7. Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote (amend ww-lod-lit-view
for the shadow switch). A procedure worked out from first principles is a missing skill -- write it under
.claude\skills\<name>\SKILL.md, or say why it will not recur.
Final message: short, plain words, the picture paths.
