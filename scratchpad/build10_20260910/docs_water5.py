#!/usr/bin/env python3
"""Lane BUILD10 -- WATER5's documents stop saying BUILD PENDING.

Anchors match exactly once; CR counts asserted unchanged.
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

EDITS = [
    ("WW_CHANGES.md",
     "## 2026-09-10 -- the water window: curves, Solve, a curves file, PNG export / "
     "import (lane WATER5, **BUILD PENDING**)\n"
     "\n"
     "**Build state, 2026-09-10 (lane DOCS2 checked it):** `release/NifSkope.exe` is\n"
     "03:57:46 and the four new sources are 04:50:17 - 05:04:17, so none of them is in\n"
     "the built exe; `NifSkope.pro` does NOT yet name `src/watercurves.*` or\n"
     "`src/waterwindow.*` (hook-up H1 unapplied), so the resume is\n"
     "`scratchpad/water5_20260910/PENDING.md` in full, after WATER4's build.\n"
     "\n"
     "**Status: written and syntax-checked (`g++ -fsyntax-only` with the real\n"
     "`Makefile.Release` flags, rc=0, no warnings), NOT compiled, NOT run, no\n"
     "hook-up applied.** The one gate check (2026-09-10 05:07:16) found neither\n"
     "`water4_20260910/GO` nor `DONE`, so the four new files sit outside the build\n"
     "until `scratchpad/water5_20260910/PENDING.md` is run after WATER4's build.\n",

     "## 2026-09-10 -- the water window: curves, Solve, a curves file, PNG export / "
     "import (lane WATER5, **BUILT AND RUN 2026-09-10 by lane BUILD10**)\n"
     "\n"
     "**Status: hooked up, built and gated.** All twelve of hook-up\n"
     "`scratchpad/water5_20260910/hookup.py`'s anchors matched exactly once (CR 0 on\n"
     "all four files, no marker present) and were applied; `qmake` ran BEFORE `make`\n"
     "because the `.pro` gained two translation units; `release/NifSkope.exe`\n"
     "**16:23:22** (19,970,048 bytes) links `watercurves.o` and `waterwindow.o`, and\n"
     "`Makefile.Release`'s regenerated blocks name `watermark.h` for all five objects\n"
     "that include it -- each of them newer than the header. `style.qss` in step.\n"
     "\n"
     "**The window's own gates (`water_window.sh`): 46 checks, 2 failures**, floor 24,\n"
     "both pictures written from inside the app, `release/ww_headless_windows.log` 2\n"
     "records with `onprimary=0` and 0 opaque.\n"
     "\n"
     "* **W1 the house style: all green.** 4 scrub fields and 0 plain spin boxes,\n"
     "  5 headings and 0 group boxes, 5 selectors all in matched chrome, 3 check\n"
     "  boxes with no \" - \" and none without a tooltip, 9 settings on 9 distinct\n"
     "  rows by geometry, the map / summary / Save outside the scroll area and the\n"
     "  settings inside it, 8 of 8 settings visible without scrolling, the Files\n"
     "  fold, the full-screen toggle 0 -> 1 -> 0, and the window is a top level.\n"
     "* **W2 the json: byte-identical** save -> load -> save at 927 bytes, and the\n"
     "  five weighted points are in the file.\n"
     "* **W3 the store round trip: point for point green, weight for weight RED.**\n"
     "  Cause, measured out of the saved store with the independent decoder: a\n"
     "  DYE PIN's per-point weight is never written. `WaterCurveDoc::writeTo`\n"
     "  (`src/watercurves.cpp` ~684) fills `s.extra` only for `Stroke` and `Pin`;\n"
     "  the harness's dye pin was placed on the curve's third point, which carries\n"
     "  weight 2, so it reads back as the default 1 (\"curve 3 point 1: weight 2\n"
     "  against 1\"). The store's own bytes confirm it: record 0 (the 5-point curve)\n"
     "  carries the floats 1.0 0.5 2.0 0.75 1.25 and record 2 (the dye pin) carries\n"
     "  no weight at all. One-line candidate for the owning lane: write the weights\n"
     "  for every point-carrying kind, not two of them.\n"
     "* **the body override: RED, and it is a name-offset bug in `watermark.cpp`.**\n"
     "  `\"harness river\"` IS in the saved file (once) and the window's own summary\n"
     "  line prints it, so the write worked; the READ gives `''`.\n"
     "  `WaterMarkDoc::encodeNames` packs only the NON-empty names and\n"
     "  `encodeTable` starts its running offset at **0** (`src/watermark.cpp` 503),\n"
     "  while `LodtFile::bodyName` treats offset 0 as \"this body has no name\"\n"
     "  (`src/lodtfile.cpp` 3096). So the FIRST named body in any file this document\n"
     "  writes is unreadable, and the harness named exactly one. Candidates:\n"
     "  reserve offset 0 with a leading NUL in the blob (the reader untouched), or\n"
     "  change the reader's sentinel (which would change how generated files read).\n"
     "  Neither was landed here.\n"
     "* **W4 green**: the json loaded onto an UNMARKED regenerated copy and Solved\n"
     "  re-derives the SAME flow words, hash `ab5e8fd453afcac3` both ways, 29,312 of\n"
     "  29,312 texels moved, 0 curves refused; the floor (the copy differs before the\n"
     "  load) fired.\n"
     "* **W5 green**: export -> import reproduces the flow plane exactly, **0 differ\n"
     "  of 21,754,958 painted texels**.\n"
     "* **W6 green**: the green-mirrored PNG is REFUSED naming the channel (agreement\n"
     "  0.151 as-is against 1.000 mirrored over 173,568 texels) and stored nowhere,\n"
     "  and the unflipped one right after is accepted.\n"
     "* **W7/W8**: 46 checks against a floor of 24; `images/water_window_whole.png`\n"
     "  (the whole worldspace at 0.121 px a texel) and `images/water_window_mouth.png`\n"
     "  (2.00 px a texel at the river, the five-point curve with its width band, its\n"
     "  points, the source pin and the dye pin).\n"
     "\n"
     "**The neighbours did not move**: `water_mark.sh` 47/8 model + 20/0 dock and\n"
     "`water_flow.sh` 47/2 are the SAME numbers as before the hook-up, so hiding the\n"
     "dock's canvas and adding its button changed no count; `lodl_water.sh` PASS,\n"
     "`lodl_open.sh` 23/0.\n"
     "\n"
     "**NOT this step** (they are lane WATER6's, and its entry is below):\n"
     "`CHANGE_NEEDED.md`'s C1-C3 -- the solver consuming per-point weights, a\n"
     "one-point pin, and an imported raster as authority where painted -- and the\n"
     "DirectX green convention for the flow PNG.\n"),
]

SPEC55 = """
### 5.5 The water window (lane WATER5, BUILT 2026-09-10)

bungo: *"just make it open a new popup window that can be set to full screen
and you can drag that shows the flowmap"*, *"Add all the tools needed to mark
the rivers and solve it and export import there, into that new window"*,
*"allow me to save the curves as some type of a file"*, and on the dock's map,
*"do you draw it on that tiny map?"*

`src/waterwindow.{h,cpp}` is a top-level window (not a dock) opened from the
dock's **Water window** button and from Workspaces; the dock's own canvas is
HIDDEN and stays only as the model's hands for `water_mark.sh`. The map draws
the whole worldspace at first open and zooms to texel level (measured: 0.121
px a texel fitted, 2.00 px a texel at the river's mouth). Rows, one to a row,
in three bands with the map, the summary and the action bar outside the
scrolling settings: Landscape file (File, Show), Curves (Tool, Speed, Width,
Point weight, Reverse / Finish / Delete), Selected body (Class, Water form,
Colour, Still water, Dye at mouth, Name), Dye (Dye colour, Dye fade), and a
folding Files section (Save curves / Load curves, Export PNG / Import PNG,
Flow samples per cell). **Solve** calls `WaterMarkDoc::solve()` as WATER4
wrote it, after the curves are mirrored into the stroke store.

**Two homes, one source.** The curves are saved as `<Worldspace>.water.json`
beside the land file and MIRRORED into the `.lodl` stroke store when the land
file is saved. The json is versioned, in WORLD units, and carries no plane, so
loading it onto a regenerated land file and pressing Solve re-derives the same
flow words (gate W4: hash equal, 29,312 of 29,312 texels moved).

```
{ "format": "ww-water-curves", "version": 1,
  "worldspace": "Commonwealth", "landFile": "Commonwealth.lodl",
  "cells": [minX, minY, maxX, maxY], "bodySamples": 32, "units": "world",
  "dye": { "halfDistance": 8192 },
  "curves": [ { "kind": "curve"|"pin"|"sourcePin"|"outletPin"|"dyePin",
                "body": 2, "enabled": true, "speed": 0.5, "width": 4096,
                "colour": [r, g, b, a],            // dyePin only
                "points": [[x, y, weight], ...] } ],
  "bodies": [ { "id": 2, "name": "...", "class": "auto"|"sea"|"river"|"lake",
                "form": null, "colour": [r, g, b], "still": false,
                "dyeMouth": false, "dyeStrength": 1 } ],
  "rasters": [ ... ] }
```

**The flow map as a PNG.** Export writes the flow plane at the file's own
body-plane grid plus a 16-bit body mask; a wet texel's alpha is floored at 1
so water can be told from land by alpha alone, and the round trip is exact
(gate W5: 0 of 21,754,958 painted texels differ). Import stores the image as a
RASTER SOURCE LAYER (a kind-10 stroke record). The green channel's meaning is
lane WATER6's section below; the refusal that catches a map written the other
way round is gate W6 and it fires with both agreement numbers in the sentence.

**What was RED on the first build** (lane BUILD10, both reported and not
cured): a dye pin's per-point weight is not written by `writeTo`, and the
FIRST named body in a file this document writes reads back nameless because
`encodeTable` gives it name offset 0 while the reader spells 0 "no name".

"""

EDITS.append(("scratchpad/specs_20260909/spec_water.md",
              "in the state the test leaves — a river selected and marked.\n"
              "\n"
              "---\n"
              "\n"
              "## 6. The FO4CS reader's checklist\n",
              "in the state the test leaves — a river selected and marked.\n"
              + SPEC55 +
              "---\n"
              "\n"
              "## 6. The FO4CS reader's checklist\n"))


def main():
    state = {}
    for path, old, new in EDITS:
        full = os.path.join(ROOT, path)
        b = state.get(full) or open(full, "rb").read()
        cr0 = b.count(b"\r")
        a = old.encode("utf-8")
        c = b.count(a)
        print("%-42s count=%d CR=%d" % (path, c, cr0))
        if c != 1:
            print("REFUSED: %s anchor matches %d times" % (path, c))
            return 1
        b2 = b.replace(a, new.encode("utf-8"))
        assert b2.count(b"\r") == cr0, path
        state[full] = b2
    for full, b in state.items():
        old = open(full, "rb").read()
        open(full, "wb").write(b)
        print("wrote %-42s %d -> %d bytes, CR %d -> %d"
              % (os.path.relpath(full, ROOT), len(old), len(b),
                 old.count(b"\r"), b.count(b"\r")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
