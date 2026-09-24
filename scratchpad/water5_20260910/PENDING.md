# Lane WATER5 -- BUILD PENDING (paste-able resume)

The ONE gate check the brief allows ran at **2026-09-10 05:07:16**, after all
code was written: `scratchpad/water4_20260910/GO` did NOT exist,
`scratchpad/water4_20260910/DONE` did NOT exist, `tasklist | grep -i -E
"Fallout4|NifSkope"` printed nothing (rc=1).  Per the brief: no hook-up
applied, no `NifSkope.pro` edit, no build, no run, no poll.  Everything below
is written, `g++ -fsyntax-only` clean with the real `Makefile.Release` flags
(rc=0, no warnings, on `src/watercurves.cpp` and `src/waterwindow.cpp`), LF-only
(CR = 0 by Python byte count on every new file), and UNRUN.

Read first: `CONSTITUTION.md`, `scratchpad/lane_water5_report.md` (section 0
is the gates), `scratchpad/water5_20260910/CHANGE_NEEDED.md` (what the
solver does not yet consume: NOT a build blocker), `scratchpad/water4_20260910/PENDING.md`
(WATER4's own build comes FIRST: this window calls its `solve()`), skill
`nifskope-ww-resume-pending`.

## What is on disk (NEW files only; not one existing file was touched)

| file | bytes | lines | what |
|---|---|---|---|
| `src/watercurves.h` | 9,728 | 201 | the model: `WaterCurve` (points + weights), `WaterBodyOverride`, `WaterRasterLayer`, `WaterCurveDoc` (json, mirror, PNG) |
| `src/watercurves.cpp` | 34,210 | 968 | the codec, the deterministic json writer, the mirror to/from `WaterMarkDoc`, PNG export/import, the flipped-green refusal |
| `src/waterwindow.h` | 1,979 | 47 | `waterWindowOpen()`, `waterWindowInstall()` |
| `src/waterwindow.cpp` | 95,217 | 2,530 | the window, the map (overview + texel-level detail), the curve tools, the rows, the self-test (gates W1-W8) |
| `tests/spells/water_window.sh` | 5,939 | 137 | the harness: two fixture copies, the log read back BY NAME, the placement log checked |
| `scratchpad/water5_20260910/hookup.py` | | | the 12 hook-up edits, `--check` (counts only, ran clean: 12 of 12 anchors match once, CR 0) / `--apply` |

## Step 0 -- ORDER: WATER4's build first

This window's Solve is `WaterMarkDoc::solve()` as WATER4 rewrote it.  Run
`scratchpad/water4_20260910/PENDING.md` to its `DONE` first; do not fold the
two into one build, because a red F gate would then have two suspects.

## Step 1 -- the gate, the hook-ups, qmake, the build

```
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?          # must print rc=1
ls scratchpad/water4_20260910/GO scratchpad/water4_20260910/DONE  # both must exist
cd /e/Projects/NifskopeWildWastelandEdition
python scratchpad/water5_20260910/hookup.py --check              # 12 anchors, each count=1, CR=0
python scratchpad/water5_20260910/hookup.py --apply              # writes NifSkope.pro, watermark.h/.cpp, watermarkpanel.cpp
bash scratchpad/water3_20260910/syn.sh src/watermark.cpp src/watermarkpanel.cpp src/watercurves.cpp src/waterwindow.cpp   # rc=0
```

The `.pro` gained two translation units, so **qmake BEFORE make** (the
resume skill's section 3), then read the dependency back by object name:

```
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/water5_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/water5_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/water5_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
grep -n "watercurves\.h\|waterwindow\.h\|watermark\.h" Makefile.Release | cut -c1-160
```

`watermark.h` changed (H2: `extra`, the `#define`): every object that
includes it -- `watermark.o`, `watermarkpanel.o`, `nifcli.o`, `watercurves.o`,
`waterwindow.o` -- must be newer than it (build-verify's "a successful build
is not a consistent one").  Then the exe-newer sweep over EVERY changed file
(`git status --porcelain -- src tests NifSkope.pro`), not one.  His open
window's exe is renamed aside, never killed (build-verify).

The hook-up H2's `#define WATERMARK_STROKE_EXTRA` switches ON the weight and
raster mirroring in `watercurves.cpp` and the "weight for weight" check in
the self-test; without H2 they compile out and the check prints SKIP.

## Step 2 -- the harnesses, in this order (one instance at a time)

```
bash tests/spells/water_mark.sh    > scratchpad/water5_20260910/gate_water_mark.txt 2>&1;   tail -3 scratchpad/water5_20260910/gate_water_mark.txt
bash tests/spells/water_flow.sh    > scratchpad/water5_20260910/gate_water_flow.txt 2>&1;   tail -3 scratchpad/water5_20260910/gate_water_flow.txt
SHOT=E:/Projects/NifskopeWildWastelandEdition/scratchpad/water5_20260910/images bash tests/spells/water_window.sh > scratchpad/water5_20260910/gate_water_window.txt 2>&1; tail -8 scratchpad/water5_20260910/gate_water_window.txt
bash tests/spells/lodl_water.sh    > scratchpad/water5_20260910/gate_lodl_water.txt 2>&1;   tail -2 scratchpad/water5_20260910/gate_lodl_water.txt
bash tests/spells/lodl_open.sh     > scratchpad/water5_20260910/gate_lodl_open.txt 2>&1;    tail -2 scratchpad/water5_20260910/gate_lodl_open.txt
```

`mkdir -p scratchpad/water5_20260910/images` first; the SHOT path is
ABSOLUTE (a relative one writes nothing, render-shot skill).

What each must print:

* `water_mark.sh`: model >= 14 checks PASS, dock >= 16 checks PASS -- the
  dock's canvas is now HIDDEN (H3) but its `layStroke` still reaches the
  model and the "map sits outside the scrolling settings" check reads
  ancestry, not visibility, so the counts should not move.  **If the dock's
  "settings band opens showing its settings" count moves**, the hidden canvas
  gave the splitter different sizes: report the number, do not fix here.
* `water_flow.sh`: WATER4's gates, unchanged by this lane; two predicted red
  as registered (F2 island bank, F5 p99) -- WATER4's numbers, not this lane's.
* `water_window.sh`: >= 24 checks, 0 failures, PASS, the 19 gates named in the
  script each `ok`, the placement log with 0 `onprimary=1` and 0 opaque, the
  two pictures.  **Predicted**: W4's hash equality depends on `writeTo()`
  producing the same stroke store the window's own save produced and on
  `solve()` being deterministic (WATER4's CG has a fixed iteration order; the
  numpy twin was); if W4 is red with W2/W3 green, print the two `moved`
  counts -- a difference there is `addStroke` refusing a curve on the copy
  (`refused` is printed).  W5's "0 differ" rests on `rgbaFromWord` /
  `wordFromRgba` being exact inverses over the 256 directions x 16 speeds x
  16 confidences: `python -c` it first if red (the mapping is 12 lines).
  The "Files section folds" check needs the fold OPEN at start: QSettings key
  `WaterWindow/expanded/Files` defaults true.  The full-screen check needs a
  screen; headless at opacity 0 it still toggles the window state.
* `lodl_water.sh` 56/0 and `lodl_open.sh` 23/0: unmoved (the writer did not change).

## Step 3 -- the pictures (W8)

`water_window.sh` with `SHOT=` writes them from inside the app
(`win->grab()`, never a desktop capture): `images/water_window_whole.png`
(the worldspace fitted at first open) and `images/water_window_mouth.png`
(2 px a texel at the river's mouth, the five-point curve with its arrows and
the source pin + dye pin).  Open both.  If the mouth picture shows the
overview instead of texels, the detail render did not run before the grab:
the 120 ms detail timer needs a `processEvents` loop of ~200 ms before the
grab -- add a `QTimer`-driven wait in `shot()` and say so.

## Step 4 -- the documents stop saying BUILD PENDING

`scratchpad/water5_20260910/WW_CHANGES_ENTRY.md` goes to the TOP of
`WW_CHANGES.md` (LF-only entry; assert CR count unchanged, 19,020 as of
2026-09-10), `MISTAKES.md` (what the build found), the lane report's section
5 (gates) and 6 (pictures), `scratchpad/specs_20260909/spec_water.md` section
5 (the window as built, the json schema -- the report's section 3 is the
text), the skill amendments in the report's section 8.  Line endings by
Python byte count.  Then `touch scratchpad/water5_20260910/DONE`.
