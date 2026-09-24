# BUILT, GATED GREEN -- nothing is pending

Lane CELLVIEW1, whole-cell viewer (bungo 2026-09-19: "I think a prerequisite
would be, to be able to view a whole cell like the CK editor").

Phase A parked CODE-ONLY at 12:08. Phase B took the BUILD slot and the EXE slot
at director clock 15:07:17 2026-09-19 and finished both. This file used to say
"nothing has been built and nothing has been run". That is no longer true.

## The exe

| | |
|---|---|
| `release/NifSkope.exe` | 23,504,384 B, 2026-09-19 15:14:27, sha1 `af4577556f2b80ee71a048c637cbe218643ee8d7` |
| `release/NifSkope.before_cellview1.exe` | 23,367,168 B, 2026-09-19 14:51:48, sha1 `68ffb42ff00754b09d0b9de3f2a802dde05d5d12` (the rung, taken ONCE) |

`hookup_cellview1.py --check` was re-run first and all six anchors were still
present exactly once despite three other lanes having edited the same files
since 12:36. `--apply` clean, `qmake` RC=0 (`grep -c cellview Makefile.Release`
= 9), `make` RC=0 by its OWN exit code, the exe newer than every changed file,
and `cellview.o` + `cellpick.o` both read back out of the link line.

## The gate

`bash tests/spells/cell_open.sh` -- **PASS, 8 rows, 0 failures**, 15:36:27.

| row | result |
|---|---|
| exe newer than every source the gate covers | PASS |
| wilderness -30,-30, 74 placements | PASS -- 74 refs, 74 boxes recomputed and matched |
| Sanctuary -20,7, 240 placements | PASS -- 141 refs, 240 boxes matched |
| downtown 5,-11, 1578 placements | PASS -- 1429 refs, 1578 boxes matched |
| the five named references | PASS -- 5 ok, 0 wrong (STAT, SCOL, MSTT, FURN, CONT) |
| overlay `type` / `has-lod` / `layer` | PASS -- 4 / 4 / 2 legend buckets |

`bash tests/spells/cell_open.sh --red` -- **PASS**, 15:38:20. The wrong euler
convention (`+x,+y,+z`) moves **62 of 74**, **220 of 240** and **853 of 1578**
boxes and the log names the references and the offsets. The 12 / 20 / 725 that
do not move are the placements whose stored rotation is zero, where the two
conventions are the same matrix.

## What the gate found, and what it was

Every disagreement the gate produced was in the CHECKER, not in the viewer. Five
reader defects, all of which presented as "the rotation convention is wrong",
plus three places where the checker's expectation was narrower than the code's.
They are written up in root `MISTAKES.md` under two `CELLVIEW1, 2026-09-19`
headings. The structural one worth repeating here: **a box cannot be rotated** --
rotating the eight corners of a model-space AABB gives the AABB of the rotated
AABB, which is exact at rotation zero and too large everywhere else, which is
precisely the "73 of 74, and only the rotated ones" pattern.

## The pictures

`scratchpad/cellview1_20260919/images/`, 1822x960 (the main window floors at
1822 px wide on this machine; a narrower request is REFUSED in
`release/ww_harness_window.log`), top-down orthographic, `WW_RENDER_CLEAN=1`.

| shot | built in | scene |
|---|---|---|
| `wild.png` -30,-30 | 846 ms | 74 refs, 23 models, 0 failed, 77,072 source tris -> 25 welded shapes / 82,970 verts |
| `sanctuary.png` -20,7 | 1787 ms | 150 refs read, 255 placements, 240 drawn, 123 models, 1 failed, 223,177 tris -> 89 shapes / 224,288 verts |
| `downtown.png` 5,-11 | (see notes) | 395 models, 3 failed, 1578 placements |
| `overlay_type.png`, `overlay_has-lod.png`, `overlay_layer.png` | -20,7 | one bucket colour per key, multiplied over the diffuse |

## What is missing, said plainly

* **PICKING IS NOT WIRED TO THE GUI.** `src/cellpick.{h,cpp}` is complete and
  filled -- every placement is in the table with its world AABB, `pick()` does
  the CPU ray test and reports how many boxes the ray entered so a pick never
  implies it was unambiguous, and `rowsFor()` returns the flat Name|Value rows
  in panel order. But `cellPickTableMutable()` has exactly ONE caller in the
  tree (`src/cellview.cpp:702`, the builder). Nothing reads it back: there is no
  click handler and no panel, because the mouse lives in `src/glview.cpp` and
  the docks in `src/nifskope_ui.cpp`, both owned by other lanes. **So the
  brief's "click a reference, get a flat Name|Value panel" is written and
  untested.** It needs a hook-up lane of its own, the same shape as this one's.
* **The LOD-group overlay bungo asked for does not exist.** `has-lod` colours a
  reference by which MNAM slots its base fills -- it does NOT colour by `.lodi`
  identity group. Identity groups are not implemented and the census line refuses
  by name rather than drawing silent grey.
* **Precombined meshes are not implemented.** XCRI is not read at all. A downtown
  cell in the CK shows its precombined geometry; this shows the individual
  references instead. The census line says so.
* **The terrain is a flat grey sheet.** LAND is built and placed, but it carries
  no material and no splat sampling, so in a top-down orthographic shot with no
  relief shading it reads as a featureless quad. It is geometry, not a picture of
  the ground.
* **The cell grid is on and invisible in these shots.** At `n = 1` it draws one
  cell's outline, which lies exactly on the terrain's own edge, so there is
  nothing to see. It is worth a 3x3 shot before anyone calls it broken.
* **Magenta on some meshes in downtown.** Two of the cars carry magenta patches.
  532 material warnings in that run, 98 distinct, all of the form
  `materials/c:/projects/fallout4/build/pc/data/materials/...` -- vanilla NIFs
  whose material name is an ABSOLUTE Bethesda build path, which the shared loader
  prepends `materials/` to and then cannot find. That is
  `lodgenLoadModel`/`lodgenReadAsset` behaviour, NOT this lane's file, and it
  affects the LOD bake the same way. Reported, not touched.
* **Some foliage draws a white rectangle outline** around the leaf cards in the
  Sanctuary shot -- an alpha property that is not reaching the draw.
* The overlays TINT the diffuse rather than replacing it, so the colour on screen
  is not the colour in the legend.

## The neighbours (run after the build, on the 15:14 exe)

| harness | result |
|---|---|
| `render_shot.sh` | 82 checks, 0 failures, **PASS** |
| `native_open.sh` | 17 checks, **1 failure**, 2 skipped, FAIL -- the failure is the KNOWN one the brief named: `the .lodi scene draws everything the .BTO draws (covered 0.8978 >= 0.90)`. The two skips are a missing `NifSkope.before_nativeview1.exe` rung and the manifest leg with `GBAKE` unset. Not this lane's code; re-run whole and captured to be sure of which check it was. |
| `harness_window.sh` (`RUN_NATIVE_OPEN=0`) | 11 checks, 0 failures, 1 skip, **PASS** -- and it measured the layout floor this lane had to work around: asked 640x480, window came out 1024x480, logged `FLOORED` with both numbers. |

## A quirk in another lane's file, for whoever owns it

`WW_RENDER_VIEW` cannot select ViewTop. `src/glview.h:455` has
`ViewTop = 0`, and `src/glview.cpp:6450` reads
`if ( v == 0 || v > int(ViewUser) ) v = int(ViewFront);` -- so `WW_RENDER_VIEW=0`
silently means front. This gate asks for `1` (ViewBottom) and gets the top-down
picture it wants, so nothing here is blocked. Not edited: not this lane's file.

## Still not done, for a later lane

* `.lodi` identity groups and XCRI precombined, above.
* A dedicated **File > Open Cell...** dialog. Today a `.wwcell` opens through
  ordinary File > Open, and the file-type row is registered.
* **Welded, not instanced.** The brief asked for instanced draws; this builds one
  welded document, because a built document needs no renderer change at all and
  the renderer files belong to other lanes. Measured cost: 365.3 MB welded vs
  96.6 MB unique-resident for a 5x5 downtown. Every placement is already in the
  pick table with its own transform, so instancing is a renderer change later,
  not a rewrite.

## Shared files touched

The hook-up's four (`NifSkope.pro`, `src/esmdata.h`, `src/esmdata.cpp`,
`src/nifskope.cpp`), root `MISTAKES.md` (appended by byte splice, CRLF
preserved: 9820 -> 9902 CRLF, 0 bare LF, +5418 bytes), and
`tests/spells/cell_census.py` (one defaulted `types=` parameter on
`index_bases`; the census's own numbers are unchanged). Nothing committed,
nothing stashed.
