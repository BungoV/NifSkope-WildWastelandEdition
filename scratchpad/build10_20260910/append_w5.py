#!/usr/bin/env python3
"""Lane BUILD10 -- append the Build section to lane WATER5's report (append-only)."""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
P = os.path.join(ROOT, "scratchpad", "lane_water5_report.md")

TEXT = """
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
"""


def main():
    b = open(P, "rb").read()
    assert b.count(b"\r") == 0, "the report is not LF-only"
    assert b.count(b"## Build (BUILD10") == 0, "already appended"
    t = TEXT.encode("utf-8")
    assert t.count(b"\r") == 0
    open(P, "wb").write(b + t)
    print("appended %d bytes -> %d, CR 0" % (len(t), len(b) + len(t)))


if __name__ == "__main__":
    main()
