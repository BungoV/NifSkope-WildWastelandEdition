# Lane CELLVIEW1 -- open a whole exterior cell (or an N x N block) in NifSkope the way the Creation Kit shows it

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` before every timestamp. Never commit, never
  `git stash`, never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/cellview1_20260919/` (`BUILDING` first,
  `DONE` last, first word `cellview1`; `report.md` incremental, section 0 within ten tool calls; `PENDING.md` past
  half your context).
- YOU ARE NOT THE ONLY BUILD LANE. Lanes HORIZONOUT (finishing) and then IMPOSTORSHOW own the build slot and the one
  NifSkope instance. PHASE A below is read + design + code with `g++ -fsyntax-only` only: NO make, NO exe run. At the
  end of phase A write `PENDING.md` headed `BUILD PENDING` and stop; the director resumes you for phase B when the
  slot is free. Files IMPOSTORSHOW is likely to touch (the native LOD viewer draw path, `lodinative.*`, the shader
  it adds): keep your edits in NEW files wherever you can and list every shared file you touch with the hunk.
- Phase B rules: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` as its own command before every build and
  exe run; Fallout4 up = no build, no exe run, never wait-loop. One NifSkope at a time, `--port <unused>` +
  `WW_WINDOW_AT=1960,40`, absolute `E:/...` paths. A NifSkope with no `--port` is bungo's: never kill, rename the
  exe aside at link time. Wedged harness instance: `NifSkope::open <scene>` UTF-16LE UDP to its port. Build MSYS2
  UCRT64 per skill `nifskope-ww-build-verify`; rung ONCE `release/NifSkope.before_cellview1.exe`; never delete rungs.
- Read first: `CONSTITUTION.md`; HANDOFF top block (2026-09-19 lines: LODLEVELS, DIRECTION 04:28 = this lane's
  origin, IDENTPROX, the RULED join); `MISTAKES.md` top 20; `src/esmdata.*` (what the ESM reader already gives:
  worldspaces, cells, REFR placement, STAT/SCOL/MODL, XLYR layers, LAND), `src/lodinative.*` (the instanced scene),
  the terrain path (`btdterrain`, LAND decode), the normal NIF loader + material path. Skills: `nifskope-ww-lodgen`,
  `nifskope-ww-render-shot`, `nifskope-ww-panel-style`, `ww-test-harness-add`, `nifskope-ww-build-verify`.

## bungo's words
"I think a prerequisite would be, to be able to view a whole cell like the CK editor" (2026-09-19, while discussing
generated whole-building LODs: he wants to SEE what a building is made of, in place, before any LOD generation work).

## What it is
Read-only viewer. File > Open Cell... (and CLI/harness `WW_CELL_OPEN=<plugin list>|<worldspace edid>|<x>,<y>|<n>`):
worldspace + cell XY + block size N (1, 3, 5). Data root + load order from the existing lodgen settings (loose files
+ BA2s through the existing resource layer; masters + plugins in order, last override wins -- reuse what lodgen does,
do not write a second resolver).
- Every persistent + temporary REFR of the cells whose base has a model (STAT, SCOL expanded to its parts, MSTT,
  FURN, CONT, DOOR, ACTI, TREE, FLOR, LIGH with a model; state the list you support and what you skip) drawn with its
  FULL MODL model, REFR position/rotation/scale, materials through the normal loader. ONE load per distinct model,
  instanced draws. Initially-disabled refs and refs with an enable parent in the opposite state: hidden by default,
  a toggle shows them tinted. Markers hidden by default.
- LAND terrain under it (heights + the base texture layers if the existing terrain path gives them cheaply; else
  flat-shaded heights with vertex colour, and say so). Water plane at the cell's water height.
- Navigation as the rest of the viewer (Blender-style per the house rule); frame-all on open; a cell grid overlay with
  cell coordinates.
- PICKING: click an object -> a flat Name | Value panel: REFR formID, base formID + editor ID + record type, model
  path, position/rotation/scale, XLYR layer (name + formID), our `.lodi` GROUP id + group size when a bake of this
  worldspace is loaded beside it (match through the cold record's `refFormId`), has-LOD (MNAM slots 0-3 with the mesh
  names), precombined yes/no (XCRI / the cell's combined refs list) and previs owner cell if cheap.
- COLOUR OVERLAYS (one at a time): by XLYR layer; by our identity group; by "has authored LOD at level 4/8/16/32";
  by record type; by precombined. Deterministic colours, a legend.
- Read-only: no save path. The file stays open as a scene like the native LOD scene does.

## Phase A (now)
1. Inventory, file:line: what `esmdata` already parses vs what is missing for this (temporary-children groups,
   XESP, XLYR names, SCOL parts, LAND, XCLW, XCRI). Cost estimate for a 5x5 downtown block (refs, distinct models,
   triangles, VRAM) from a Python count on Fallout4.esm -- so the design knows its budget.
2. Design note (one page in the report): scene representation, instancing, load order handling, picking (ID buffer vs
   ray), where the panel and the overlays live, what is reused from lodinative, what is new. Divergences from the CK
   or Blender stated.
3. Write the code in new files where possible, `-fsyntax-only` clean, the harness `WW_CELL_OPEN` and the gate
   `tests/spells/cell_open.sh` (written, not run): ref count drawn == Python census for three named cells (one
   wilderness, one Sanctuary, one downtown), five named refs each with their expected world position picked back by
   formID, a red control (rotate one ref 90 degrees in a fixture copy -> the gate names it), overlays produce the
   expected number of distinct colours.
4. `PENDING.md` = `BUILD PENDING` with the exact build + gate + picture commands.

## Phase B (on resume)
Build, run the gate, neighbours before/after (`native_open.sh` 17/0/2, `render_shot.sh`, `lodl_open.sh` 23/0), pictures
to `images/`: Sanctuary 1x1 and 3x3, a downtown 3x3 (DN135 Gwinnett area, cell of chunk 4.4.-12) plain + by layer + by
identity group + by has-LOD, one picking screenshot with the panel. Load time and memory for each.

## Rules
Masters ship OFF does not apply to a File > Open entry (it is a viewer, not a behaviour change) -- but nothing about
existing file opening may change. No "fixed/final/true": mechanism + refuter. Every number from a named log. UI per
`nifskope-ww-panel-style`: flat Name | Value, palette from skinVars only, label + control, no description text.

## Report
0 state at launch; 1 inventory + budget; 2 design; 3 code (files, lines, shared files touched); 4 gate; 5 PENDING /
build (mtime, size, sha1); 6 pictures + timings; 7 WW_CHANGES + HANDOFF text for the director; 8 MISTAKES; 9 skill
text (a new skill `nifskope-ww-cell-view` if the procedure is repeatable). END phase A with `BUILD PENDING` and four
plain sentences for bungo. Final message under 300 words.
