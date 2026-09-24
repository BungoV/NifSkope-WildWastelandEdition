# Lane SHOWCASE1 -- the final Sanctuary chunk bake with EVERY landed feature on, and the pictures bungo asked for

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. NO CODE CHANGES in this lane. NO BUILD. You bake and you photograph.
  YOUR EXE IS A COPY: `E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912/ns_run/NifSkope.exe` (a copy of release/ taken
  15:4x; 2026-09-12 12:58:48, 22,007,808 B, sha1 ba7585cba389c8b38f0e07c6c11063e0cec1124c -- check both before anything; if either differs,
  stop and write PENDING.md). Use the COPY for every bake, card bake and render (its own `ww_*.log` files land beside it), because lane
  PANEL1 is building `release/NifSkope.exe` at the same time and will rename it aside. `tools/bake_impostor_cards.sh` and the spells
  hard-code `release/NifSkope.exe`: run them with their exe variable pointed at the copy (read the script's `NS=`/`EXE=` line) or copy
  the driver into your lane dir and point it there; never edit a tool in the tree. Never commit, never `git stash`.
- ONE GUI NifSkope HARNESS INSTANCE AT A TIME across BOTH lanes: before EVERY GUI launch (renders, the card bake) run
  `powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Select-Object ProcessId, CommandLine | Format-List"`;
  if a NifSkope with `--port` is up that is not yours, WAIT (poll every 10 s) until it is gone; never kill it. A NifSkope with no
  `--port` is bungo's own window: ignore it, never touch it.
- Markers: `scratchpad/showcase1_20260912/BUILDING` at start (touch it FIRST), `DONE` at the end. Report
  `scratchpad/showcase1_20260912/lane_showcase1_report.md`, incremental, PENDING.md past half context.
- Game: `tasklist | grep -i Fallout4` before EVERY exe launch; if it is up, stop and write PENDING.md (CONSTITUTION 6). `NifSkope.exe`
  is counted SEPARATELY: a NifSkope with no `--port` is bungo's own window, never touched; one GUI harness instance at a time, yours
  always with `--port <unused>` and `WW_WINDOW_AT=1960,40` (second monitor). Headless `-no-gui` bakes need no GUI slot.
- Never bungo's installed `Data\Terrain` or any mod folder; never the whole Commonwealth. Own out-dirs under
  `scratchpad/showcase1_20260912/` only, every path ABSOLUTE `E:/...` (the exe resolves relative paths against release/).
- bungo's cache `E:/Tools/Upscale/esrgan-bat/` is READ-ONLY input.
- Read first: `CONSTITUTION.md`; HANDOFF.md top block down to the `BAKED 13:5x` line; `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md`;
  `scratchpad/director_sanctuary_20260912/bake.sh` + `bake2.sh` (the director's bake of the same region this afternoon: three commands,
  because `--lodl` and `--heightmap` RETURN after writing and never reach the region); `docs/LODGEN_TERRAIN_VT.md` §2.5h (object AO),
  §2.5i (erosion), §7a; `docs/LODGEN_LEDGER_FORMAT.md`; the LAND1 / GROUND1 / TERRAINFMT1 blocks in HANDOFF.md and WW_CHANGES.md for the
  winning settings; `docs/LODGEN_IMPOSTOR_SPEC.md` and `docs/LODGEN_CARD_SHEETS.md` for cards.
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen`, `nifskope-ww-render-shot` (READ WHOLE: the camera pin, WW_LODGEN_RESOURCES root shape,
  the frame-size read-back, the save-confirm hang), `nifskope-ww-vanilla-compare`, `ww-render-arm-isolate`, `ww-texel-picture`,
  `ww-module-off-is-identical`, `ww-prototype-is-not-the-product`.

## bungo's words, verbatim (2026-09-12 15:3x)
"Now, can you give me final chunk bakes with all those features you've shown me? Show me the 2D and 3D (various views, with AO, also
include impostors and buildings and other LOD objects, also a render where you can see the baked AO of the buildings and the terrain
together in greyscale)"

## The bake (region: Sanctuary, cells -20 24 -9 35, dim 4; plus the far rings that contain it)
Every landed feature ON, at the setting each lane's own pictures used (read the lane blocks; write the setting you chose beside its
source in the report; if a lane left the value as bungo's call, use the value in that lane's shipped picture and say so):
- `--road-detail 1` (always; bungo's standing rule).
- LAND1: `--land-guide aspecthex` with `--land-guide-scale 256` on `--land-hex 256` (LAND1's winner). Check the exact spelling in
  `src/nifcli.cpp` before typing it.
- GROUND1: `--terrain-object-ao` (strength default 0.5) and `--erosion <GROUND1's picture strength>` with its iterations/seed.
  NOTE: `--terrain-object-ao` REFUSES with `--lodl` (exit 2) -- the .lodl is a separate command anyway.
- TERRAINFMT1: `--msn-cache E:/Tools/Upscale/esrgan-bat/output` (or wherever TERRAINFMT1's block says the cleaned 2K cache is; read-only),
  `--sheet-format legacy` (the default; say so).
- Terrain identity: bake the default (vertex-colour identity ON). Bungo has not ruled; do NOT decide it. For the LIT 3D pictures also bake
  the same chunk with `--no-terrain-identity` and show both, labelled, because the identity vertex colours turn the viewport blue-purple.
- Objects: `--vt --tex-dir --cover --native --native-mesh-report` as the director's bake2.sh, plus `--arrays` (texture arrays; FO4CS
  target) -- buildings and every other LOD object come from the region bake's .BTO files. Also `--atlas` ONLY if it composes with
  `--arrays` in one run (read nifcli.cpp; if they conflict, arrays win and say so).
- Impostors: cards substitute ONLY at far chunks (dim 16/32). Bake a card library for the region's tree candidates per
  BAKE_INSTRUCTION §7: `CANDIDATES=trees OCT=8 TILE=512 bash tools/bake_impostor_cards.sh "<esm>" -20 24 -9 35 "<card dir>"` (check the
  driver's argument order in the script first; it is a GUI bake -- second monitor, opacity 0, your own port; the sidecars must carry a
  `projection ortho` line). Then bake the dim-8, dim-16 and dim-32 chunks that contain Sanctuary (`--dim 8/16/32`, same region, own
  out-dirs) with `--impostors <card dir>` (and `--impostors-from-level` if needed to get a card into the picture; say what you used).
  A far chunk is EMPTY without cards or `--slot-fallback`; read the counters before blaming anything.
- Separate commands: `--lodl <dir>` and `--heightmap <dir>` once each (whole worldspace, ~5 s each), own dirs.
- Record for every command: the exact argv, rc, the stage-times line, the census line, files written with sizes. Run decoders:
  `--native-verify`, `--lodl --verify-only`, `tests/spells/lodgen_vt_check.py header/tiles` on the pyramid (V1 'height sheet' fails
  without `--vt-height`; either add `--vt-height` or name the skip), `cmp` the heightmap against
  `E:/Projects/Fallout 4 Mods/mods/FO4CS/Textures/Terrain/Commonwealth/Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds`.
- Control (cheap, one chunk): the same chunk (-20,24) with every switch OFF; note which files differ and which are byte-identical, so the
  picture's differences are attributable.

## The pictures (`scratchpad/showcase1_20260912/images/`, every one with the label burned in, every frame size read back with PIL)
1. **2D sheets** for the near chunk (-20,24) and one neighbour: colour sheet, `_msn` normal sheet, mask sheet (decode per tile, BC1 vs BC3
   by the cover bit -- `scratchpad/ground1_20260912/work/maskdec.py`), each channel of the mask as greyscale (R material, G wetness,
   B occlusion, A shore -- check §2.5h for the exact channel roles), all-features-on beside all-off, vanilla's own sheets as a third
   column where vanilla has them (`E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/`).
2. **3D lit views** of the near chunk with TERRAIN + OBJECTS together. The render hook opens ONE file; find the route that shows both
   (candidates: the `.lodl` whole-worldspace viewer used by `tests/spells/lodl_open.sh`; opening the .BTO and .BTR as two documents is not
   one scene). If no route exists that puts a BTR and a BTO in one framebuffer, composite the two renders from the SAME pinned camera
   (same `WW_RENDER_CENTER/ORTHO/VIEW`, prove with `ww_camera_pin.log` that `upp` and look-at match) and say that it is a composite.
   Views: top (VIEW=1), front, and two oblique views (rotate via the pin; read the skill). Our own sheets through `WW_LODGEN_RESOURCES`
   with the root shaped `<root>/Textures/Terrain/Commonwealth/...` (NOT `<root>/Data/...`) and `<root>/Textures/Objects/...` for arrays.
   Each view: identity ON (default) and `--no-terrain-identity`, labelled.
3. **Far rings with impostors**: the dim-16 (and dim-32 if it has anything) chunk rendered lit, with at least one tree card visible;
   plus a crop at the card. Frame with the pin; name the card and its source model in the caption from the manifest `C` lines.
4. **Buildings and other LOD objects**: the near .BTO alone, lit, two views; the manifest's object count in the caption.
5. **The AO picture, greyscale**: terrain AO and object AO together. Terrain AO = the mask sheet's occlusion channel (§2.5h) at texel
   resolution AND the BTR vertex-colour B (identity channel, `src/lodgen.cpp:952`); object AO = `WW_LOD_CHANNEL=3` on the .BTO (vertex
   B). Produce (a) a top-down greyscale composite at the sheet's resolution with the objects' AO rendered from the same pinned top camera
   and multiplied in, and (b) an oblique greyscale view. Say in the caption which channel came from where and at what resolution; a
   pixel from the sheet and a pixel from a vertex channel are two different instruments.
6. One **overview contact sheet** of everything above, plus each picture standalone.
Rules for every picture: `WW_RENDER_CLEAN=1`; the camera pinned and its `upp` read back; label + numbers in the caption; a picture
whose two halves are byte-identical is a picture of nothing -- find out why before showing it (the resource-root shape is the usual
cause). Never describe how something SHOULD look; bungo's eye decides.

## Deliverables in `scratchpad/showcase1_20260912/`
- `images/` as above; `bake_*.sh` scripts (re-runnable, every path absolute), `logs/`.
- `lane_showcase1_report.md`: `## 0. Settings table` (feature | switch | value | source lane), `## 1. Commands and results` (argv, rc,
  stage times, files, decoders), `## 2. Pictures` (one line per file: what, camera, size), `## 3. Owed / red / bungo's calls`,
  `## 4. Mistakes` (also appended to root `MISTAKES.md` the moment recognised, newest first, `## ` heading each), `## 5. Skill review`
  (any skill text the run disproved; amend it in the repo `.claude/skills` and list the file).
- `HANDOFF_BLOCK.md` (10-20 lines, plain language, what was baked, what was shown, what is owed).
- No WW_CHANGES entry unless a skill or doc changed (say which).

## Rules
Plain language; numbers beside floors; no "final/true" claims (bungo confirms); incremental writes; two-lane cap (you are one of at
most two); if context runs short: PENDING.md with exact resume steps and a DONE-so-far list.
