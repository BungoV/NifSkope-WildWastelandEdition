# Lane NATIVEVIEW2 -- the viewer lights LOD terrain from its model-space normal sheet (the dark blotches)

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: `release/NifSkope.exe` 2026-09-12 21:52:25,
  22,280,192 B (DEFAULTS1's). Rung ONCE before your first build: `release/NifSkope.before_nativeview2.exe` (copy of the exe on
  disk at that moment; never delete any `release/NifSkope.before_*.exe`). Markers `scratchpad/nativeview2_20260912/BUILDING`
  (touch FIRST) / `DONE` (first word `lighting`). Report `scratchpad/nativeview2_20260912/lane_nativeview2_report.md`,
  incremental; `PENDING.md` past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the exe
  aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did not
  create. One GUI harness instance at a time, yours always `--port <unused>` and `WW_WINDOW_AT=1960,40`. Every path in argv and
  every WW_* path ABSOLUTE `E:/...`.
- LANE VT1 RUNS IN THIS TREE AT THE SAME TIME (it edits `src/lodgen.cpp` / `src/lodtfile.cpp` and `tests/spells/lodgen_terrain_vt.sh`;
  you edit the renderer and `src/btdterrain.cpp`; neither touches the other's files). Builds and GUI harness runs are SERIALISED
  through one mutex: `mkdir scratchpad/ww_build_mutex` (atomic; poll 30 s while it exists, never delete another lane's), write your
  lane name into `scratchpad/ww_build_mutex/owner`, build + run your harnesses, then `rm -r scratchpad/ww_build_mutex`. The rung
  is copied from the exe on disk at YOUR first build (it may already carry VT1's link -- that is fine, your gates compare the arms
  your change reaches, and a bake gate is VT1's, not yours).
- Read first: `CONSTITUTION.md`; HANDOFF.md top block down to `LANDED 21:58` and the `MEASURED 20:57` line (the whole diagnosis,
  with the pictures it names); `res/shaders/fo4_default.frag` ~372-470 (the normal read: `normalMap.rg * 2 - 1`, b reconstructed,
  `btnMatrix_norm`) and `fo4_default.vert` (where `btnMatrix` comes from); `src/gl/renderer.cpp` (how FO4 shapes get their program
  and uniforms; `SLSF1_Model_Space_Normals` = bit 12, `src/gl/glproperty.h:525`, currently read by NOTHING in the FO4 path);
  `res/shaders/sk_msn.frag` (the Skyrim model-space path, the one precedent in this tree); `src/btdterrain.cpp` 320-414 (the
  sheet-lit tile: its tangent frame at 334-344, the flag edits at 395-410); `src/lodgen.cpp` ~930-945 (the .BTR's 12-byte vertex:
  no normal, no tangent, Bitangent X = 1) and where lodgen sets the .BTR shader flags (bit 12 set, specular clear);
  `docs/LODGEN_TERRAIN_VT.md` (what the `.lodt` normal sheet stores and in which channels) and `docs/LODGEN_BTD_FORMAT.md`.
- Skills (repo `.claude/skills`): `nifskope-ww-render-shot` (whole: camera pin, absolute paths, the size floor, "Photographing a
  BUILT document"), `nifskope-ww-build-verify`, `ww-module-off-is-identical`, `ww-render-arm-isolate`, `ww-test-harness-add`,
  `ww-anchored-hookup`, `nifskope-ww-lodgen` (the defaults moved on 2026-09-12; read its top).
- The FO4CS repo `E:\Projects\Fo4CommunityShaders` is READ-ONLY reference: its LOD landscape / terrain shaders decode the game's
  `_msn` sheets. Read how the GAME reads a terrain LOD normal (which channel is up, the swizzle, the range) there and in the
  vanilla sheets themselves before writing a line of GLSL.

## bungo's words, verbatim (2026-09-12 20:3x)
"What are those dark blotches on the terrain?" -- the NATIVE oblique of NATIVEVIEW1's picture i. Then 22:0x, on the two routes
offered (a fixed tangent frame like the .BTR, or a real model-space-normal path): "Do both now" -- the director's recommendation
was the proper route, and that is this lane.

## What was measured (director, 20:57; do not re-derive, verify)
The blotches are the viewer's lighting, not the bake: the colour tiles equal the legacy sheet texel for texel; terrain alone shows
them; FLAT normal tiles give the same blotches in the oblique (dark 20.0 vs 20.1 percent, IoU 0.86) and the top view has none.
`fo4_default.frag` has no model-space path, so the `.lodt` normal sheet (model-space, up in GREEN, mean (130,241,110)) is read as a
tangent-space map and its "up" lands along each vertex's bitangent; `btdterrain.cpp:334-344` builds T = normal x world up, B =
normal x T, which swings with the slope's compass direction and is arbitrary on flat ground. The legacy `.BTR` is read the same
wrong way through ONE fixed frame (Bitangent X = 1, no normal, no tangent), so it lights evenly and shows the 2048 map's grain.
Pictures: `scratchpad/nativeview1_20260912/images/blotch_normals_test.png`, `blotch_edge_zoom.png`, `blotch_top_native_legacy_sheet.png`.
Test inputs: the session sheet cache with FLAT normal tiles is at
`C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/843169d8-671d-4705-ba26-ce53a8012252/scratchpad/flatcache` (copy it
into your scratchpad; it is a copy of `scratchpad/nativeview1_20260912/work/sheetcache` with every `.n.DDS` replaced by BC1 blocks
of (128,255,128)); the render recipe is `scratchpad/nativeview1_20260912/shots.sh` (camera CENO `-73728,106496,8500`, ORTHO 8192,
VIEW 8 oblique / VIEW 1 top, `WW_LODL_REGION="-20,24,-17,27,2"`).

## The work
1. **A model-space-normal path in the FO4 program**, taken when the shape's Shader Flags 1 carries bit 12
   (`SLSF1_Model_Space_Normals`): the normal is the sheet's texel decoded in the GAME's convention (channel order, sign, range --
   measured on the vanilla `_msn` sheets and read off FO4CS's shader, both cited in the report with the numbers), transformed by
   the shape's model matrix only (no tangent frame), then the existing lighting. The tangent-space path is untouched for every
   shape without bit 12 -- that is gate (a). This lights BOTH the `.BTR` (lodgen sets bit 12) and the sheet-lit `.lodl` tile.
   `hasModelSpaceNormals` in `sk_msn.frag` is the precedent for the uniform plumbing; read how the renderer sets it there.
2. **The sheet-lit tile's frame**: with a model-space path the tangent frame is unused for that shape; leave the vertex data as
   it is unless the model-space path needs something the tile does not carry, and say so either way.
3. **Gates** (`tests/spells/native_lighting.sh`, new, with floors; `render_shot.sh`, `lodl_open.sh`, `native_open.sh` keep their
   counts): (a) every shape WITHOUT bit 12 renders byte-identical before/after (`ww-module-off-is-identical`; the existing
   render-shot baselines plus one object `.BTO` of chunk (-20,24)); (b) the oblique of the native terrain ALONE with its OWN normal
   tiles vs the FLAT tiles now DIFFER (the refuter is the rung, where they are the same to IoU 0.86), and the flat-tile render is
   lit uniformly (luma SD over the terrain below a floor you measure and state); (c) the dark fraction (luma < 40) of the native
   oblique terrain drops from 20.0 percent to a number you report, and the same view of the legacy `.BTR` moves too (report both
   directions; the .BTR was lit wrongly as well); (d) a slope test with a known answer: a synthetic tile whose normal sheet says
   "up" everywhere renders the same luma at the top view and at the oblique modulo the light's own angle (state the arithmetic),
   and one whose sheet tilts +X darkens on the side the arithmetic says.
4. **Pictures** (`scratchpad/nativeview2_20260912/images/`, labels burned in, sizes read back with PIL, `WW_RENDER_CLEAN=1`):
   (i) NATIVEVIEW1's picture i re-shot: native terrain + objects, top and oblique, LEGACY control column, rung vs new -- four
   panels per view; (ii) terrain alone: own tiles vs flat tiles, rung vs new. Send nothing yourself; the director sends.
5. **Build**: the chain from `nifskope-ww-build-verify` (game check as its own command, mutex, rename aside, `make -j2` gated on
   make's rc, exe newer than EVERY changed file via `git status --porcelain -- src res tools tests`, stale-object check for every
   header touched, `make -n` zero compile lines). Shaders reach the app as `release/shaders/*` at link time -- check the copy
   step and `cmp` after the link; a stale copy renders the old shader with a new exe. Harnesses the change reaches, named; skipped
   ones named with the reason. Read every count next to the exe timestamp.
6. **Documents in `scratchpad/nativeview2_20260912/`**: `WW_CHANGES_ENTRY.md` (starts `## 2026-09-12 — <title>`), `HANDOFF_BLOCK.md`
   (10-20 lines, plain language, opening with a blank line after its lead line), `MISTAKES_ENTRIES.md` (and root `MISTAKES.md` at
   the TOP the moment recognised), `CHANGED_FILES.txt` (A/M + path, CR/LF byte counts before and after), the report: `## 0. The
   convention` (the measured channel order with the vanilla numbers and the FO4CS citation), `## 1. Gates`, `## 2. Build and
   chain`, `## 3. Pictures`, `## 4. Owed / red / bungo's calls`, `## 5. Mistakes`, `## 6. Skill review` (amend
   `nifskope-ww-render-shot` "Photographing a BUILT document" with the lighting facts; list the file). Docs: a `## Lighting`
   note in `docs/LODGEN_NATIVE_LODO_LODI.md`'s Viewer section.

## Rules
Renderer + `src/btdterrain.cpp` only: no writer change, no bake output change (a fresh (-20,24) bake `cmp`s against the rung's).
No defaults change. No PBR renderer work (bungo's standing order): this is the FO4 program's normal read, nothing else in the
lighting. No tone changes. Plain language; numbers beside floors; no "final/true" claims; incremental writes; if context runs
short, PENDING.md with exact resume steps and a DONE-so-far list. MISTAKES at the top of the root file by you, the moment.
