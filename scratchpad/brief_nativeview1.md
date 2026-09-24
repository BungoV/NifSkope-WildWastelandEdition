# Lane NATIVEVIEW1 -- the viewer opens and renders the NATIVE bake (.lodl + .lodt sheets, .lodo + .lodi objects); pictures of the .lod outputs

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Rung ONCE before your first build: `release/NifSkope.before_nativeview1.exe`
  (never delete any `release/NifSkope.before_*.exe`). Markers `scratchpad/nativeview1_20260912/BUILDING` (touch FIRST) / `DONE`. Report
  `scratchpad/nativeview1_20260912/lane_nativeview1_report.md`, incremental; PENDING.md past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i Fallout4` before every build and every exe launch; up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with
  no `--port` is bungo's own window: rename the exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it. One GUI harness
  instance at a time, yours always `--port <unused>` and `WW_WINDOW_AT=1960,40`. Every path in argv and every WW_* path ABSOLUTE `E:/...`.
- Read first: `CONSTITUTION.md`; HANDOFF.md top block down to `BAKED 13:5x`; `docs/LODGEN_NATIVE_LODO_LODI.md` (whole); `docs/LODGEN_BTD_FORMAT.md`
  (the .lodl); `docs/LODGEN_TERRAIN_VT.md` (the .lodt sheet pyramid: tiles, formats, how a chunk maps to sheet texels); `src/btdterrain.cpp`
  (`nifCreateLodtTerrainScene`, `lodtRegionFromEnv`: the .lodl built-document route, and the comment at ~300 "land-texture blending from
  real sheets is a later round" -- THIS lane is that round); `src/nifskope.cpp` ~9855-9880 (how .lodl is routed to a built document);
  `src/lodofile.h/.cpp`, `src/lodtfile.h/.cpp` (the readers; they exist, use them, never a second parser); `tests/spells/lodl_open.sh`
  (the render recipe: `WW_LODL_REGION="x0,y0,x1,y1,lod" WW_LODL_PLANE=...`); `scratchpad/showcase1_20260912/lane_showcase1_report.md`
  (what was baked where; §3 reds).
- Skills (repo `.claude/skills`): `nifskope-ww-render-shot` (READ WHOLE: camera pin, WW_RENDER_ORTHO, the frame-size read-back, absolute
  paths, the save-confirm hang, and the new section "A file that draws its own payload is not a look picture"), `nifskope-ww-lodgen`,
  `nifskope-ww-build-verify`, `ww-anchored-hookup`, `ww-test-harness-add`, `ww-render-arm-isolate`, `ww-module-off-is-identical`.

- LANE PANEL1 IS RELINKING IN THIS TREE RIGHT NOW (one relink, a full-panel picture). Read, plan and edit source at once, but BEFORE YOUR FIRST BUILD wait (poll 30 s) until `scratchpad/panel1_20260912/DONE` exists AND contains the word `panel_full` (or `scratchpad/panel1_20260912/PENDING.md` exists); only then copy the rung `release/NifSkope.before_nativeview1.exe` from the exe on disk and build. Never rename or delete any `NifSkope_inuse_*.exe` you did not create. bungo 18:2x: "add a native view for these, yeah".

## bungo's words, verbatim (2026-09-12 17:3x)
"why are you showing me .btr still? btr is a legacy thing" -- on the SHOWCASE1 pictures, which were all renders of .BTR/.BTO. He wants
the pictures of the .lod outputs: the terrain from `.lodl` with its `.lodt` sheets, the objects from `.lodo`/`.lodi`. Today the viewer
has NO open or render route for `.lodo`/`.lodi`/`.lodt`, and `.lodl` opens only as vertex-coloured planes (a data view, no sheets).

## Inputs (SHOWCASE1's look-only bake, identity OFF in both channels; read its report §0/§1 for the argv of each)
- `scratchpad/showcase1_20260912/out/lodl/Terrain/Commonwealth.lodl` (whole worldspace, v2, 192x192 cells).
- `scratchpad/showcase1_20260912/out/look/native/Commonwealth.lodo` + `Commonwealth.lodi` (region -20 24 -9 35, the `--native` arm with
  `--no-identity --no-terrain-identity`). The `.lodt` sheet pyramid and the texture arrays/`.lodm` for the same region are under
  `scratchpad/showcase1_20260912/out/look/` (find them; the `.lodm` sidecars name the array sheets). If the look arm lacks a file you
  need, re-bake THAT command from the lane's `bake_look.sh` into your own out-dir with the exe COPY at `scratchpad/showcase1_20260912/ns_run/`
  (never bungo's Data/Terrain, never the whole Commonwealth; -no-gui bakes need no GUI slot).

## The work
1. **`.lodi` opens as a built document** (route beside the `.lodl` one in `src/nifskope.cpp`): the `.lodo` is found beside it by the
   worldspace stem; every placement inside the region becomes an instance of its base mesh in the scene, in a NIF shape per (base,
   material) with the `.lodo` vertex layout carried over (positions, normals, UVs, UV2 layer), textured through the `.lodm`/array sheets
   the bake wrote (the same material the .BTO shape carries -- read how the .BTO shapes are built in `src/lodgen.cpp` and reuse the
   material plumbing, do not invent one). Region filter `WW_LODI_REGION="x0,y0,x1,y1"` in cells, default = the file's whole extent;
   ring/cluster level `WW_LODI_LEVEL=n` (default the finest). Occluder boxes drawn ONLY with `WW_LODI_BOXES=1`, as wire boxes.
2. **`.lodl` opens LIT WITH ITS SHEETS**: when the `.lodt` pyramid for the worldspace sits beside the `.lodl` (same folder convention as
   the bake writes; also `WW_LODL_SHEETS=<dir>` to point at one), the terrain planes get the colour sheet as diffuse and `_msn` as the
   normal map through a BSLightingShaderProperty with a real texture set, per chunk of the region at the region's lod. The inline-colour
   data view stays the default when no sheets are found, and `WW_LODL_PLANE` keeps working exactly as before (byte-identical renders --
   that is gate (a)). If the .lodt tiles cannot be handed to the renderer as texture paths without a decode step, decode to loose DDS in
   a temp dir under the scratchpad and say so.
3. **Gates** (`tests/spells/native_open.sh`, new, with floors on the other side; and `lodl_open.sh` must keep its count):
   (a) every existing `lodl_open.sh` render byte-identical before/after when no sheets are present (`ww-module-off-is-identical`);
   (b) the `.lodi` scene of chunk (-20,24) dim 4: instance count == the count of `.lodi` records in that chunk (read with the reader) ==
   the `.bto.manifest.txt` row count for the same chunk from the SAME bake, and every instance's world position within 1 unit of the
   manifest row with the same (ref, part) key; the floor: a region with no placements yields an EMPTY scene and the harness sees 0;
   (c) a render of the `.lodi` scene and of the `.BTO` of the same chunk from the SAME pinned camera (`WW_RENDER_CENTER/ORTHO/VIEW`,
   `upp` read back from `release/ww_camera_pin.log`): coverage masks (non-background pixels) agree to IoU >= 0.95, reported as a number,
   and the refuter is the same comparison against a DIFFERENT chunk's .BTO, which must fail;
   (d) the lit `.lodl` render of the same chunk vs the `.BTR` render with the same sheets via `WW_LODGEN_RESOURCES`: mean absolute
   colour difference over the chunk footprint, reported; a render with `WW_LODL_SHEETS` pointed at an EMPTY dir must equal the data view.
4. **Pictures** (`scratchpad/nativeview1_20260912/images/`, label burned in, sizes read back with PIL, `WW_RENDER_CLEAN=1`, textured roads):
   the same set SHOWCASE1 owed, from the NATIVE files only: (i) terrain + objects together, top (VIEW=1) and one oblique (the pin);
   (ii) objects alone, two views, count in the caption; (iii) far region at the coarsest `.lodi` level with the impostor cards if the
   look bake carries them (else say so); (iv) the AO greyscale IS NOT OWED here (identity is off in the look arm; say so). Beside each,
   the .BTR/.BTO render of the same camera as a control column labelled LEGACY. Send nothing yourself; the director sends.
5. **Build**: the chain from `nifskope-ww-build-verify` (game check, rename aside, `make -j2` gated on make's rc, exe newer than EVERY
   changed file via the `git status --porcelain -- src res tools tests` sweep, stale-object check for every header touched, `make -n`
   zero compile lines). Harnesses the change reaches: `lodl_open.sh`, `native_open.sh` (new), `render_shot.sh`; name the skipped ones.
   Read every count next to the exe timestamp. Counted relinks.
6. **Documents in `scratchpad/nativeview1_20260912/`**: `WW_CHANGES_ENTRY.md` (starts `## 2026-09-12 — <title>`), `HANDOFF_BLOCK.md`
   (10-20 lines, plain language), `MISTAKES_ENTRIES.md` (also root `MISTAKES.md` the moment recognised, newest first), `CHANGED_FILES.txt`
   (A/M + path, CR/LF byte counts before and after), the report: `## 0. Routes added`, `## 1. Gates` (table, numbers, refuters),
   `## 2. Build and chain`, `## 3. Pictures`, `## 4. Owed / red / bungo's calls`, `## 5. Mistakes`, `## 6. Skill review` (amend
   `nifskope-ww-render-shot` with the native open recipe; list the file). Docs: a `## Viewer` section appended to
   `docs/LODGEN_NATIVE_LODO_LODI.md` and the `.lodl` sheets note in `docs/LODGEN_BTD_FORMAT.md`.

## Rules
Readers only: no change to any writer or to any bake output (the byte-identity of every bake file is a gate: `cmp` a fresh (-20,24)
bake against `release/NifSkope.before_nativeview1.exe`'s). No defaults change. No tone or renderer-pass changes beyond what a textured
built document needs. Plain language; numbers beside floors; no "final/true" claims; incremental writes; if context runs short,
PENDING.md with exact resume steps and a DONE-so-far list.
