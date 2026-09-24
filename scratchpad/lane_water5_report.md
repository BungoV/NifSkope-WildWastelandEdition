# Lane WATER5 -- the water window: curves, solve, files, PNG

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main` at `720762a`
(52+ uncommitted files from earlier lanes; nothing committed by this lane,
CONSTITUTION 8). Written incrementally; each section is appended as it
finishes. **BUILD PENDING** -- see section 5 and
`scratchpad/water5_20260910/PENDING.md`.

Read first: `CONSTITUTION.md` (1, 1a, 1b, 2, 4), the `HANDOFF.md` top block
(the WATER5 ruling paragraph), `scratchpad/lane_water3_report.md`,
`scratchpad/lane_water4_report.md`, `scratchpad/water4_20260910/PENDING.md`,
`scratchpad/specs_20260909/spec_water.md` sections 3.7, 5. Skills loaded:
`nifskope-ww-panel-style`, `nifskope-ww-resume-pending`,
`nifskope-ww-render-shot`, `nifskope-ww-build-verify`,
`ww-contract-provenance`.

bungo's words, verbatim, which this lane builds to: *"curves you can draw in
nifskope, that can have as many connection points as you want. Then you solve
the rest with a button to fill in the gaps, something like a simulation"*;
*"just make it open a new popup window that can be set to full screen and you
can drag that shows the flowmap"*; *"Add all the tools needed to mark the
rivers and solve it and export import there, into that new window"*; *"allow
me to save the curves as some type of a file"* / *"so you can load them again
and edit in nifskope"*; and on the dock's map: *"do you draw it on that tiny
map?"*.

---

## 0. The gates, PRE-REGISTERED before any code was written (CONSTITUTION 1)

Sources this lane reads, hashed BEFORE reading (`ww-contract-provenance`
step 1; step 5 re-hashed them at the end: **all six unchanged, end to end**,
section 7):

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/watermark.h` | `5e2c0a628e657c44` | 21,377 | 412 |
| `src/watermark.cpp` | `8228f690bdf25951` | 151,755 | 4,161 |
| `src/watermarkpanel.h` | `d9871b688a278081` | 1,070 | 28 |
| `src/watermarkpanel.cpp` | `7f18f352fab55b6c` | 51,314 | 1,358 |
| `src/lodtfile.h` | `4ffeccdc9b581e5e` | 21,491 | 435 |
| `src/lodtfile.cpp` | `601fb65136c8766d` | 141,680 | 3,658 |

The gates, each with the floor that stops an empty implementation passing.
They run inside the window's own self-test (`WW_WATER_WINDOW_TEST=1`, log
`release/ww_water_window_test.log`), driven by `tests/spells/water_window.sh`:

| gate | what is measured | passes when |
|---|---|---|
| W1 panel style | the `nifskope-ww-panel-style` counts on the WINDOW: scrub fields (plain 0 of >= 4), group boxes 0 against headings >= 5, selectors in matched chrome (unmatched 0 of >= 5), check boxes with no " - " and no missing tooltip (>= 3), settings one to a row by GEOMETRY (>= 8 fields on as many distinct rows), the map / summary / action bar outside the scrolling settings and the settings inside it, the settings band showing >= 6 of its fields without scrolling, the window is a top level (`isWindow()`), the full-screen toggle flips `isFullScreen()` and back | all |
| W2 json round trip | a 5-point curve with distinct per-point weights (1.0, 0.5, 2.0, 0.75, 1.25), a source pin, a dye pin and one body override: save -> load -> save; the two byte strings | identical, and the curve count read back is the count written (floor: the file is > 200 bytes and carries the weighted points) |
| W3 store round trip | the same curves mirrored into the `.lodl` stroke store, the file saved, reopened, read back into a fresh curve document | every point's x, y equal as floats; the weights equal too ONLY once hook-up H2 (section 5) is applied -- before it the check prints "weights not carried by the store" as a named SKIP, never a pass |
| W4 load onto a regenerated file | a second, UNMARKED copy of the fixture opened, the json loaded onto it, Solve; the flow words of both documents swept (`WaterMarkDoc::sweep`, every texel of the body plane) and hashed | the two hashes equal; floor: the marked body's words differ from the automatic word on >= 60 % of its texels, and the hash of the unmarked copy BEFORE the load differs from both |
| W5 export -> import | the flow map exported as PNG at the file's body-plane grid (RG direction, B speed, A confidence) plus the 16-bit body mask, then imported as a raster layer; the raster's words against the document's own words over the painted texels | 0 differ; floor: painted texels >= the marked body's area |
| W6 the flipped green | the same PNG with G mirrored (255 - G) imported | REFUSED, the sentence names the green channel, and the layer count does not move; the unflipped import right after is ACCEPTED (the control) |
| W7 the harness | `tests/spells/water_window.sh`: the window opened headless (invisible, never on the primary monitor -- `release/ww_headless_windows.log` says `onprimary=0`), a 5-point curve laid on the largest river of the fixture, Solve, json saved, reloaded, compared | log carries >= 24 checks, 0 failures, PASS, and the 19 gates the script names each `ok` |
| W8 the pictures | `WW_WATER_WINDOW_SHOT=<dir>`: the window at first open (the whole worldspace fits), and zoomed to the river's mouth with a curve and its arrows | two PNGs in `scratchpad/water5_20260910/images/`, opened and read |
| P0-P8, `water_mark.sh`, `water_flow.sh` | unchanged; the dock's hook-up hides its canvas and adds one button | green |

**NOT RUNNABLE BY THIS LANE**: every gate above needs a build, and the brief
allows one check for `water4_20260910/GO` + `DONE` + no exe running. Section 5
records that check. What this lane CAN prove without a build is
`g++ -fsyntax-only` with the real flags on every new file, and it does.

---

## 1. The window, as built (task 1)

Four NEW files, one NEW harness, no existing file touched (the hook-ups are
a script, section 5). New-file provenance at the end of the lane:

| file | sha256 (16) | bytes | lines | what |
|---|---|---|---|---|
| `src/watercurves.h` | `824323afff71037a` | 9,728 | 201 | the model: `WaterCurvePoint {x, y, w}`, `WaterCurve`, `WaterBodyOverride`, `WaterRasterLayer`, `WaterCurveDoc` |
| `src/watercurves.cpp` | `d4ac2b21658156ab` | 34,210 | 968 | the raw-store reader for the trailing bytes (`parseStoreExtras`), the deterministic json writer (`WaterCurveDoc::toJson`), the mirror (`writeTo` / `readFrom`), PNG (`exportFlowPng`, `importFlowPng`, `wordFromRgba`, `flipGreen`) |
| `src/waterwindow.h` | `bf6461b0968871f2` | 1,979 | 47 | `waterWindowOpen( mw, lodl )`, `waterWindowInstall( mw )` |
| `src/waterwindow.cpp` | `e120131a57a07c48` | 95,217 | 2,530 | `WaterMapView` (the map), `WaterWindow` (the rows, the actions, undo), `runWindowSelfTest` (gates W1-W8) |
| `tests/spells/water_window.sh` | `149bac13c6c0bb65` | 5,939 | 137 | the harness |

`g++ -fsyntax-only` with the real `Makefile.Release` flags
(`scratchpad/water3_20260910/syn.sh`): **rc=0 on both translation units, no
warnings** after one narrowing fix. That proves they compile; it proves
nothing about linking, moc (no `Q_OBJECT` was added -- every class uses
plain `connect` on lambdas, as the dock does), or behaviour.

### 1.1 The dock: a button, the canvas hidden

**Which of the brief's two options**: the dock STAYS, reduced. Hook-up H3
(unapplied) gives it a "Water window" button in its action bar and HIDES its
canvas (`canvas->hide()`). It is not removed, for one measured reason: gates
P5-P8 (`water_mark.sh`'s dock half, 20 checks) pin the dock's rows, its
canvas's ancestry (not its visibility) and its `layStroke` hands; removing
the dock would have turned a green suite red on the same build that adds the
window, and the brief says P0-P8 stay green. The rows it keeps still edit the
dock's OWN `WaterMarkDoc`; the window opens its own on the same path. Two
documents on one file is the honest cost of "reduced, not removed", stated
here so the director can retire the dock's rows in a lane that owns
`watermarkpanel.cpp` once the window is flown.

### 1.2 Layout (`nifskope-ww-panel-style`)

A `QWidget` with `Qt::Window`: draggable and resizable by the OS, F11 or the
"Full screen" button (Esc leaves full screen), geometry and splitter state
in `QSettings`. Three bands, HORIZONTAL: settings in a `QScrollArea` on the
left (min 300 px), the map on the right on the splitter (starting 340 /
940), the summary and the action bar (Full screen | Reload Solve Save)
pinned under both. The dock's splitter is vertical; a full-screen map wants
the height, so the window's is horizontal -- a divergence from the dock, not
from the skill (which says "the live part on a splitter").

`wwHeading` for every section (5: Landscape file, Curves, Selected body,
Dye, Files), one `label | field` `QGridLayout` a section with one `labelW`
(132) for the page, one setting a row, whole-word labels with the
explanation in the tooltip, `wwMakeScrubField` on all four numbers,
`wwMatchFieldStyle` on all five selectors and the two line edits,
`wwSkinColor` for every colour (`viewport` for dry land -- the dock's
literal (24,26,30) is not repeated). Files folds (`FoldSection`, the
`MarkSection` shape, fold persisted).

| section | row | control |
|---|---|---|
| Landscape file | File | line edit + Browse |
| | Show | Body ID / Flow / Shore distance / Dye / Water type / Imported flow |
| Curves | Tool | Draw curve / Select / Source pin / Outlet pin / Dye pin / Erase |
| | Speed | scrub field, world units a second, a NEW curve's speed |
| | Width | scrub field, the influence radius (drawn as a faint band along the curve) |
| | Point weight | scrub field: the selected points' weight; a new point takes it |
| | Curve | Reverse, Finish, Delete |
| Selected body | Class, Water form, Colour (Override + Choose), Flow (Still water), Dye (Dye at mouth), Name | as the dock's |
| Dye | Dye colour, Dye fade | as the dock's |
| Files (folds) | Curves file | Save curves, Load curves |
| | Flow map | Export PNG, Import PNG |
| | Flow samples per cell | 8 / 16 / 32 |

The sentence (`refreshSummary`) says what Save will do -- *"Write 2 curve(s)
with 9 points, 1 pin(s) and 0 layer(s) into Commonwealth.lodl and
Commonwealth.water.json (346 bodies, flow at 32 samples a cell) -- unsolved
edits: Solve fills in the gaps first"* -- or the one reason it cannot, and it
owns every button's enabled state (Reverse only with a selection or an
active curve, Delete only with a selection, Finish only while drawing).

### 1.3 The map

`WaterMapView`. Two pictures: an OVERVIEW sampled at ~1024 texels a side once
per open (the Commonwealth's body plane is 6144 across) drawn scaled while
the view moves; a DETAIL image re-sampled texel for texel under the widget
120 ms after the view settles, only once a texel is >= 1 px and the widget is
under 4096^2 -- one `colourAt` per DISTINCT texel along a scanline, never per
pixel. Wheel zooms about the cursor between "the worldspace at a tenth of the
widget" and 64 px a texel; middle-drag pans; Home fits. A cell grid appears
once a cell is wider than 48 px, and the bottom-left corner prints *"one
cell = N px, M px a texel"* so the zoom is a number in every picture. `fit()`
at open; the self-test asserts both worldspace corners are inside the widget.

### 1.4 The curve tools

Blender's curve edit mode with the Curve Pen, on polylines (section 2 has the
divergence table). `WaterCurve` = kind, body, enabled, speed, width, colour,
points with weights. The map edits `WaterCurveDoc` in place and calls
`WaterWindow::edited()`, which pushes an undo snapshot (post-edit; 64 deep),
marks the document unsolved and refreshes the sentence. Solve is a BUTTON, as
bungo said; edits do not re-solve on their own (the dock re-solved on every
stroke). `Solve` = `WaterCurveDoc::writeTo( doc )` (the kinds this model
owns -- 0, 1, 4, 5, 7 and 10 -- are removed from the stroke store and
re-added through `addStroke`, so the store's own refusal of a dry curve is
the window's refusal; the doc's own marks 6 / 8 / 9 are left alone and the
overrides go through `setBody*`) then `WaterMarkDoc::solve()` **as WATER4
wrote it, untouched**. `Save` solves if needed, writes the `.lodl` AND the
json beside it, and names both in the sentence.

The Water form row lists the forms the FILE interned plus the worldspace
default (`WaterMarkDoc::waterForms()`), as hex. The brief says "from the
plugin's WATR list"; `EsmWorld` has no WATR accessor (WATER2's
`ESMDATA_CHANGE_NEEDED.md`, still open), so the file's own list is what
exists. Stated, not hidden.

---

## 2. Divergences from Blender, each stated (task 2)

| Blender (curve edit mode, Curve Pen, Image editor) | here | why |
|---|---|---|
| Bezier control points with handles | polyline vertices | the solve interpolates a segment; a handle would be a second thing to place for no second constraint |
| Curve Pen: click on nothing extrudes from the active end | the same (Draw tool) | none |
| click a point selects; Shift extends; drag moves | the same | none |
| Ctrl+click on a segment inserts | the same, the weight interpolated | none |
| X / Delete removes the selection | the same; a curve emptied goes with its points | none |
| box select = left-drag on nothing | the same, in the Select tool only | in the Draw tool a left-click on nothing extrudes, as in Blender's Curve Pen |
| A selects all, Alt+A none | the same | none |
| direction shown only with the Normals overlay | arrows always drawn, one at every segment's middle and one at the end | the direction IS the mark |
| Segment > Switch Direction (menu) | the Reverse button in the Curve row, on the selected or active curve | no menu bar in this window |
| per-point Radius in the N panel | the Point weight row: a SPEED weight, 1 = the curve's speed | Blender's radius scales a bevel; here the scalar scales the speed the solve writes (its consumption is `CHANGE_NEEDED.md` C1) |
| Esc / Enter end an operation | Esc / Enter / right-click / double-click finish the curve | none; four ways because a person will try all four |
| Ctrl+Z undo (global) | Ctrl+Z / Ctrl+Shift+Z over the curve document only | the land file's undo is Reload |
| Image editor: middle-drag pans, wheel zooms about the cursor, Home = View All | the same | none |
| Ctrl+Space maximises an area; full screen is a Window menu entry | F11 and a button; Esc leaves | the OS rule; the window has no menu |
| a curve lives in a Curve object | in `<Worldspace>.water.json` and, baked, in the `.lodl` store | the file is the document |
| the pen draws in the 3-D viewport too | a top-down map only | as WATER3: the 3-D viewer is another lane's file, and a river's direction is a fact about the map |

---

## 3. The curves file: `<Worldspace>.water.json` (task 3)

`WaterCurveDoc::jsonPathFor()` = `<dir>/<completeBaseName>.water.json` beside
the `.lodl` (`Commonwealth.lodl` -> `Commonwealth.water.json`). Version 1.
World coordinates throughout (never texels), so it loads onto any land file
of the worldspace at any sample rate. Written by a HAND serialiser (fixed key
order, numbers at 9 significant digits -- a float survives exactly -- LF,
`QSaveFile`) so that save -> load -> save is a byte comparison (gate W2);
read with `QJsonDocument`, refusing BY NAME a wrong `format`, a version
above 1, a curve of unknown kind or with no points, an override with no id.

```
{
  "format": "ww-water-curves",
  "version": 1,
  "worldspace": "Commonwealth",           the .lodl's base name
  "landFile": "Commonwealth.lodl",
  "cells": [minX, minY, maxX, maxY],      the land file's cell bounds when saved
  "bodySamples": 32,                       its body-ID plane rate when saved
  "units": "world",
  "dye": { "halfDistance": 8192 },
  "curves": [
    { "kind": "curve" | "pin" | "sourcePin" | "outletPin" | "dyePin",
      "body": 3, "enabled": true, "speed": 0.5, "width": 4096,
      "colour": [r, g, b, a],              dyePin only
      "points": [[x, y, w], ...] }         w = the speed weight, 1 = the curve's speed
  ],
  "bodies": [                              only bodies a person changed
    { "id": 3, "name": "Charles", "class": "auto"|"sea"|"river"|"lake",
      "form": "0x0001d3a8" | null, "colour": [r, g, b] | null,
      "still": false, "dyeMouth": false, "dyeStrength": 1 }
  ],
  "rasters": [                             imported flow maps, by reference
    { "file": "Commonwealth.flow.png", "sha256": "...", "origin": [0, 0],
      "size": [6144, 6144], "painted": 21600000 }
  ]
}
```

Provenance (anchors in `src/watercurves.cpp` `d4ac2b21658156ab`, 968 lines;
line numbers re-derived from the anchors at the end of the lane):

| claim | line | anchor |
|---|---|---|
| the writer, key order as above | 335 | `QByteArray WaterCurveDoc::toJson() const` |
| the reader and its refusals | after 335 | `"the file's \"format\" is \"%1\", not \"ww-water-curves\""` |
| the raw store's trailing bytes (weights, raster payload) | 74 | `QVector<StoreExtra> parseStoreExtras( const QByteArray & raw )` |
| the mirror into the store, the kinds it owns | 643 | `bool WaterCurveDoc::writeTo( WaterMarkDoc & doc, int * refused, QString * error ) const` |
| the PNG channel law | 771 | `quint16 WaterCurveDoc::wordFromRgba( quint8 r, quint8 g, quint8 b, quint8 a )` |
| the flipped-green refusal and its two numbers | 930 | `"the green channel of %1 looks flipped: over %2 painted texels the "` |

**What the json does NOT carry**: a plane (Solve re-derives; loading the
json onto a regenerated `.lodl` is gate W4), and the raster words (the PNG
is their source; Load curves re-imports each PNG named beside the json and
says how many it could not find). **What the `.lodl` store carries**: the same
curves as kinds 0 / 1 / 4 / 5 / 7 with the weights as one float a point AFTER
the points (a record of `20 + 8n + 4n` bytes; an old reader skips them by
stride, spec 3.7) and a raster layer as kind 10 (`x0 y0 w h` as int32, two
`qCompress` stores) -- BOTH ONLY AFTER HOOK-UP H2, because the store's codec
as built drops a record's trailing bytes on decode and writes none. Before
H2 the json carries the weights, the store carries the points, and gate W3
says SKIP on the weights by name. `WaterCurveDoc::weightsFromStore()` reads
the trailing floats out of the raw store itself, so the READ side works from
the day the file has them.

---

## 4. Export / import (task 4)

`exportFlowPng( doc, flow.png, bodies.png )` walks `WaterMarkDoc::sweep()`
once -- every texel of the body plane at the file's own grid (6144 x 6144 on
the Commonwealth) -- and writes RGBA8888: R = (cos + 1) / 2 x 255, G = (sin +
1) / 2 x 255 with **+G = north** (image row 0 is the northernmost texel row),
B = the speed step x 17, A = the confidence x 17, dry = 0,0,0,0, and a wet
texel with confidence 0 gets **A = 1** so water and land differ by alpha
(`wordFromRgba` rounds 1 / 17 back to 0, so W5 stays exact). The body mask
is `Format_Grayscale16`, the id. Every field is exactly invertible: the 8-bit
direction lattice is 1.41 degrees a step and the worst quantisation of a
component is 1 / 510, which is 0.11 degrees -- so `atan2` rounds back to the
lattice point it came from.

`importFlowPng` refuses by name a file that is not a PNG or not the file's
grid, reads painted = alpha > 0, and then runs THE FLIPPED-GREEN TEST: over
the painted wet texels with a non-zero document word, the mean cosine
between the map's direction and the document's own, as-is and with the
direction mirrored (theta -> -theta, which is what a mirrored G does); when
the mirrored reading agrees better AND the as-is reading is below 0.9 the
import is refused with both numbers in the sentence. Against a body nobody
marked the document's word is the automatic constant, the body's mean
direction, so the test still discriminates. `flipGreen()` is the W6 control
(255 - G). The layer lands in `WaterCurveDoc::rasters` (last painted wins),
paints under Show = Imported flow, saves as kind 10 after H2, and is
referenced from the json by name + sha256. **Its authority inside the
solve is `CHANGE_NEEDED.md` C3** (twelve lines in `flowWordOf`), not this
lane's file.

---

## 5. Gates: what has run, what has not, and the ONE check (task 5, task 6)

**The one check the brief allows, 2026-09-10 05:07:16**:
`scratchpad/water4_20260910/GO` did not exist, `DONE` did not exist,
`tasklist | grep -i -E "Fallout4|NifSkope"` printed nothing (rc=1). The exe
was free; the markers were not there. **So: BUILD PENDING. No hook-up
applied, `NifSkope.pro` untouched, nothing built, nothing run, never
polled.** `scratchpad/water5_20260910/PENDING.md` is the paste-able resume
(`nifskope-ww-resume-pending`), with WATER4's build ordered FIRST because
this window's Solve is its `solve()`.

| gate | state |
|---|---|
| syntax, real flags | **PASS**: rc=0, no warnings, `watercurves.cpp` + `waterwindow.cpp` |
| line endings | **PASS**: CR = 0 on all five new files, by Python byte count |
| hook-up anchors | **PASS (dry)**: `hookup.py --check` -- 12 of 12 anchors match exactly once, CR 0 on all four target files, marker "lane WATER5" absent from all four (the applied-already guard) |
| W1-W8 | WRITTEN, UNRUN |
| P0-P8, `water_mark.sh`, `water_flow.sh`, `lodl_water.sh`, `lodl_open.sh` | untouched by this lane's files; re-run after the hook-ups (H3 hides the dock's canvas) |
| pictures (W8) | NOT MADE: they come from inside the built app (`win->grab()`); step 3 of the resume |

The hook-ups, written and NOT applied (`scratchpad/water5_20260910/hookup.py`):

| id | file | edit |
|---|---|---|
| H1 | `NifSkope.pro` | the four new paths after `src/watermarkpanel.h \` and `src/watermarkpanel.cpp \` |
| H2 | `src/watermark.h` | `#define WATERMARK_STROKE_EXTRA 1`; `QByteArray extra;` on `WaterStroke` after `pts` |
| H2 | `src/watermark.cpp` | `decodeStrokes` keeps a record's trailing bytes in `extra`; `encodeStrokes` counts and writes them; `addStroke` accepts kind 10 without a body |
| H3 | `src/watermarkpanel.cpp` | `#include "waterwindow.h"`; `canvas->hide()`; the "Water window" button before the bar's stretch; `waterWindowInstall( mw )` after `dock->hide()` |

`WATERMARK_STROKE_EXTRA` is the switch: with it, `writeTo` mirrors the
weights and the raster layers and the self-test checks "weight for weight";
without it those compile out and the check prints a named SKIP. The new
files compile either way (proved: the syntax pass ran without it).

**What the solver still does not consume** -- `scratchpad/water5_20260910/CHANGE_NEEDED.md`:
C1 the per-point weight (speed nibble x weight along the curve; gate: a 1 ->
2 weighted channel doubles its end nibble), C2 a one-point pin (`solveBody`
skips kind 0 / 1 with < 2 points; recommended: a speed pin), C3 the raster
layer's authority (`flowWordOf` consults `rasterWordAt` first). None changes
`solve()`'s signature.

---

## 6. Pictures

None. W8's two pictures are grabbed by the window itself
(`WW_WATER_WINDOW_SHOT=<abs dir>`, `win->grab()`) at the harness's two
framings -- the worldspace fitted, and 2 px a texel at the river's mouth
with the five-point curve, its six arrows, the source pin and the dye pin --
and the app does not exist yet. `PENDING.md` step 3 names the files and the
one thing that could make the mouth picture show the overview instead of
texels (the 120 ms detail timer needs the event loop to turn before the grab).

---

## 7. Mistakes (also in `MISTAKES.md`, spliced at the top, LF-only, CR 0 -> 0)

1. **Two undo conventions in one stack.** `edited()` pushed the post-edit
   state; the first draft of `loadCurves()` pushed the PRE-load state. Found
   on the re-read before the syntax pass; fixed to one convention, commented
   at the stack. Rule: a stack has one convention, written at the stack, and
   every pusher is read against it.
2. **The anchored hook-up script, typed for the fourth time.** WATER2, WATER3,
   WATER4 (`splice.py`) and BUILD5b each wrote it; WATER4 asked the director
   to lift it into `tools/`; this lane wrote `hookup.py` before noticing.
   Recorded against myself under CONSTITUTION 1a (a skill BEFORE the second
   use), and the skill now exists (section 8).

Provenance step 5: the six cited sources re-hashed after the last edit --
`watermark.h 5e2c0a628e657c44`, `watermark.cpp 8228f690bdf25951`,
`watermarkpanel.h d9871b688a278081`, `watermarkpanel.cpp 7f18f352fab55b6c`,
`lodtfile.h 4ffeccdc9b581e5e`, `lodtfile.cpp 601fb65136c8766d`, and
`NifSkope.pro b22b4596fc17ac9a` -- **identical to the start**: no concurrent
lane moved a file this lane's anchors or claims point at.

---

## 8. Skill review (CONSTITUTION 1a)

**Loaded and used.**

* `nifskope-ww-panel-style` -- every control through the helpers, the
  one-row grid with one `labelW`, the three bands with the summary pinned,
  the sentence owning the buttons, the fold, and the self-test counts with
  floors (W1: 4 / 0, 0 / 5, 5 / 0, 3 / 0 / 0, 8 rows, 6 visible). Its
  "Blender is the reference; state divergences" is section 2.
* `nifskope-ww-resume-pending` -- the shape of `PENDING.md`: the read order,
  the order of the two pending builds, qmake before make for a `.pro`
  change, the dependency read-back by object, the exe-newer sweep over every
  changed file, the sequential harness chain, the four documents.
* `nifskope-ww-render-shot` -- ABSOLUTE output paths (the harness refuses a
  relative `SHOT`), one instance at a time, `--port`, the placement log
  read for `onprimary` and opacity, and "opacity 0 for every top level"
  which the window inherits from the application's event filter rather than
  re-implementing.
* `nifskope-ww-build-verify` -- "When you CANNOT build": the syntax pass with
  the real flags on every new file, gated on its own rc, and the Qt keyword
  check (no `emit` / `slots` / `signals` identifiers) before it.
* `ww-contract-provenance` -- steps 1 and 5 on the six sources (hashed before
  reading, re-hashed after the last edit: unchanged), step 2's anchors beside
  every line number in section 3.

**Wished for, and WRITTEN**: `ww-anchored-hookup`
(`.claude/skills/ww-anchored-hookup/SKILL.md`, repo tree, 4,598 bytes; the
director applies it to the live tree per rule 1a) -- the hook-up as a
refusing script: exact-once anchors carrying the file's line ending,
`--check` that writes nothing, the CR assert, the applied-already MARKER (the
BUILD5b trap: an "after" anchor still matches once after the edit), and the
`#define` switch that lets new files compile with and without the hook-up
while their self-test prints a named SKIP. Five lanes re-typed it; the sixth
copies `scratchpad/water5_20260910/hookup.py`.

**Declined, with the reason.**

* *"a deterministic serialiser makes a round trip a byte gate"* -- one
  sentence, and it is CONSTITUTION 4's "a writer works when it regenerates
  its file byte-identically" applied to a new format; not a procedure.
* *"an exactly invertible channel law for a PNG export"* -- the arithmetic in
  section 4 is six lines and specific to an 8-bit direction lattice; it will
  not recur outside this format.
* *"a harness that reads its gates back by name"* -- `water_flow.sh` already
  does it and `water_window.sh` copied the loop; the pattern lives in the
  scripts, and a skill would only point at them.

---

## Build (BUILD10, 2026-09-10)

### The hook-up and the build

`hookup.py --check`: **12 of 12 anchors matched exactly once, CR 0 on all four
files, marker `lane WATER5` absent from every one** -- so "not applied" was
read off the marker, never off an anchor (the trap the hook-up skill names).
`--apply` then wrote `NifSkope.pro` 19,488, `src/watermark.h` 21,966,
`src/watermark.cpp` 152,515, `src/watermarkpanel.cpp` 52,324 bytes, CR 0.

`qmake` BEFORE `make`, because the `.pro` gained two translation units
(QMAKE-RC=0, BUILD-RC=0). `release/NifSkope.exe` **16:23:22**, 19,970,048
bytes; `res/style.qss` and `release/style.qss` identical.

| read-back | result |
|---|---|
| `Makefile.Release` blocks naming `watermark.h` / `watercurves.h` / `waterwindow.h` | `nifcli.o`, `watercurves.o`, `watermark.o`, `watermarkpanel.o`, `waterwindow.o` -- by the `awk` walk, not `grep -A` |
| every object that includes `watermark.h` newer than it | all five `ok` |
| exe newer than every changed file (`.pro`, both water headers, four sources, `lodtfile.*`) | no `STALE` line |

`tasklist` printed no `Fallout4.exe` and no `NifSkope.exe` before the build and
before every launch.

### The gates

| harness | result |
|---|---|
| `water_window.sh` | **46 checks, 2 failures**, floor 24; 2 headless window records, `onprimary=0`, 0 opaque; both pictures written |
| `water_mark.sh` | model 47 / 8, dock **20 / 0 PASS** -- the SAME numbers as before the hook-up, so the hidden canvas moved no count |
| `water_flow.sh` | 47 / 2 -- WATER4's two pre-registered reds, unmoved by this lane |
| `lodl_water.sh` | RESULT PASS (33 `ok`) |
| `lodl_open.sh` | 23 / 0 PASS |

W1 (house style) all green with every floor firing: 4 scrub fields / 0 plain,
5 headings / 0 group boxes, 5 selectors all matched, 3 check boxes with no
" - " and none without a tooltip, 9 settings on 9 distinct rows by GEOMETRY,
the three bands, 8 of 8 settings visible unscrolled, the Files fold, full
screen 0 -> 1 -> 0, `isWindow()`. W2 byte-identical at 927 bytes. W4 hash
`ab5e8fd453afcac3` both ways with the floor firing first. W5 **0 differ of
21,754,958**. W6 refused at 0.151 as-is against 1.000 mirrored over 173,568
texels, layer count unmoved, the control accepted right after.

### The two reds, with their causes measured

**1. W3's weight half: a DYE PIN's per-point weight is never written.**
`WaterCurveDoc::writeTo` (`src/watercurves.cpp` ~684) fills `s.extra` only when
`s.kind` is `Stroke` or `Pin`. The harness places its dye pin on the curve's
THIRD point, whose weight is 2, so the readback defaults it to 1 -- "curve 3
point 1: weight 2 against 1". Read straight out of the saved store with lane
WATER2's independent decoder: record 0 (the 5-point curve) carries the floats
`1.0 0.5 2.0 0.75 1.25`; record 1 (the source pin) carries none; record 2 (the
dye pin) carries none. So the codec is fine and the mirror is not. One-line
candidate: write the weights for every point-carrying kind. NOT LANDED.

**2. The body override's name: the FIRST named body is unreadable.**
`"harness river"` is in the saved `.lodl` (once) and the window's own summary
line prints it, so the write worked. `WaterMarkDoc::encodeNames`
(`src/watermark.cpp` 537) packs only NON-EMPTY names, and `encodeTable` starts
its running name offset at **0** (line 503) -- while `LodtFile::bodyName`
(`src/lodtfile.cpp` 3096) spells offset 0 as "this body has no name". The
harness named exactly one body, so its name landed at offset 0 and vanished.
This is not the window's bug and not this lane's: it is in the document, and it
affects any file `WaterMarkDoc` writes with names. Candidates: reserve offset 0
with a leading NUL in the blob (the reader untouched), or move the reader's
sentinel (which changes how generated files read). NOT LANDED -- a resuming
lane's product is a verdict, not a cure.

### The pictures

`scratchpad/water5_20260910/images/water_window_whole.png` -- the window at
first open, the whole worldspace fitted (0.121 px a texel), every row one to a
line, the three bands and the action bar.
`scratchpad/water5_20260910/images/water_window_mouth.png` -- 2.00 px a texel
at the river, the five-point curve with its width band and its points, the
source pin and the dye pin, and the summary naming body 2 "harness river"
(which is how the name bug above was known to be a READ bug).

Both grabbed from inside the application (`win->grab()`), never a desktop
capture; `SHOT` was an ABSOLUTE path.

### What is NOT in this step

`CHANGE_NEEDED.md` C1-C3 and the DirectX green convention: lane WATER6, built
and gated in the same session -- see `scratchpad/lane_water6_report.md`.
