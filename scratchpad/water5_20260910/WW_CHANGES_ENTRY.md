## 2026-09-10 -- the water window: curves, Solve, a curves file, PNG export / import (lane WATER5, **BUILD PENDING**)

**Status: written and syntax-checked (`g++ -fsyntax-only` with the real
`Makefile.Release` flags, rc=0, no warnings), NOT compiled, NOT run, no
hook-up applied.** The one gate check (2026-09-10 05:07:16) found neither
`water4_20260910/GO` nor `DONE`, so the four new files sit outside the build
until `scratchpad/water5_20260910/PENDING.md` is run after WATER4's build.

bungo, verbatim: *"curves you can draw in nifskope, that can have as many
connection points as you want. Then you solve the rest with a button to fill
in the gaps, something like a simulation"*; *"just make it open a new popup
window that can be set to full screen and you can drag that shows the
flowmap"*; *"Add all the tools needed to mark the rivers and solve it and
export import there, into that new window"*; *"allow me to save the curves as
some type of a file"*; and on the dock's map, *"do you draw it on that tiny
map?"*.

* **The water window** (`src/waterwindow.{h,cpp}`, NEW): a top-level window
  of its own, draggable and resizable, F11 or its button for full screen,
  geometry remembered.  Settings on the left in a scroll area (Landscape
  file, Curves, Selected body, Dye, a folding Files section), the map on the
  right on a splitter, the summary sentence and Reload / Solve / Save pinned
  under both.  Every control through the shared helpers; the self-test counts
  them with floors.  The map fits the whole worldspace at first open and
  zooms to 64 px a texel: an overview at ~1024 texels a side while the view
  moves, a texel-for-texel detail image once it settles.  Planes: Body ID,
  Flow, Shore distance, Dye, Water type, Imported flow.
* **Curve tools, Blender curve-edit style** (Curve Pen): click adds a point
  to the active curve (as many as wanted), click a point selects it (Shift
  extends), drag moves, Ctrl+click a segment inserts, Delete / X removes,
  Enter / Esc / right-click / double-click finishes, box select in the
  Select tool, A selects all, Home fits.  An arrow at every segment's middle
  and at the end shows the direction; Reverse switches it.  A per-point speed
  weight (the "Point weight" row: Blender's per-point Radius, as an idea).  A
  one-point curve is a pin.  Source / Outlet / Dye pins are one click each.
  Ctrl+Z / Ctrl+Shift+Z over the curve document.
* **Solve** runs `WaterMarkDoc::solve()` as WATER4 wrote it, after the curves
  are mirrored into the stroke store; nothing is reimplemented.  What the
  solver does not yet consume (the per-point weight, a one-point pin, the
  raster layer's authority) is `scratchpad/water5_20260910/CHANGE_NEEDED.md`.
* **The curves file** `<Worldspace>.water.json` beside the land file
  (`src/watercurves.{h,cpp}`, NEW): version 1, world coordinates, every curve
  with its points and weights, the pins, the dye pins, the per-body overrides
  (name, class, water form, colour, still, dye at mouth), the dye half
  distance, and the flow-map layers by file name + sha256.  Written by a
  deterministic hand serialiser (fixed key order, 9 significant digits, LF),
  so save -> load -> save is a byte comparison.  Save writes the `.lodl` AND
  the json; Save curves / Load curves take a path.  Loading onto a
  regenerated `.lodl` re-creates the curves editable and Solve re-derives.
* **Export / Import PNG** at the file's body-plane grid: R, G = the direction
  as (cos + 1) / 2, (sin + 1) / 2 with +G = north, B = the speed step x 17,
  A = the confidence x 17 (a wet unmarked texel gets A = 1 so water and land
  differ by alpha), every field exactly invertible; the body mask beside it
  as 16-bit grey.  Import = a raster layer, and a FLIPPED GREEN CHANNEL is
  refused: the map's mean cosine against the file's own flow is taken as-is
  and with G mirrored, and when the mirrored reading agrees better and the
  as-is one is below 0.9 the import says so with both numbers.
* **The dock** keeps its rows and gains a "Water window" button; its tiny
  canvas is HIDDEN (hook-up H3, unapplied).
* **Gates, pre-registered** (`scratchpad/lane_water5_report.md` section 0)
  and run by `tests/spells/water_window.sh` (NEW): W1 panel style with
  floors, W2 json round trip byte-identical, W3 store round trip (weights
  only after hook-up H2, else a named SKIP), W4 load-onto-regenerated =
  same flow words (hash), W5 export -> import 0 words differ, W6 flipped green
  refused + unflipped accepted, W7 the harness (headless, `onprimary=0`), W8
  the two pictures.  **None has run.**
* **Hook-ups, written and NOT applied** (`scratchpad/water5_20260910/hookup.py`,
  12 anchors, each counted once): H1 the four `.pro` paths; H2
  `WaterStroke::extra` + the codec keeping a record's trailing bytes +
  `addStroke` accepting kind 10; H3 the dock's include, hidden canvas, button,
  `waterWindowInstall()`.
* Files NEW: `src/watercurves.h` (9,728 B), `src/watercurves.cpp` (34,210 B),
  `src/waterwindow.h` (1,979 B), `src/waterwindow.cpp` (95,217 B),
  `tests/spells/water_window.sh`, `scratchpad/water5_20260910/`.  All LF-only
  by Python byte count.  No existing file touched.
