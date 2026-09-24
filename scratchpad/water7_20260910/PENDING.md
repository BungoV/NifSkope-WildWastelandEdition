# LANE WATER7 / UI2 -- BUILD PENDING

**Why.** The build slot was held: `scratchpad/build11_20260910/DONE` does not
exist (that folder holds `BUILDING` and a `PENDING.md` of its own). The GAME was
down -- `tasklist | grep -i -E "Fallout4|NifSkope"` printed `rc=1` -- so the game
is not the blocker; the slot is. The check was made ONCE, as the brief requires,
and nothing was polled. No `BUILDING` marker was written by this lane and
`hookup.py` was **not applied**.

**What is on disk and finished.** Every file this lane owns is written and
syntax-checked with the real `Makefile.Release` flags. Nothing is committed.

## 1. Apply the hook-up

```
cd /e/Projects/NifskopeWildWastelandEdition
python scratchpad/water7_20260910/hookup.py            # check, writes nothing
python scratchpad/water7_20260910/hookup.py --apply
```

Seven edits over four files, all LF-only (CR 0 before, CR 0 after, asserted by
the script). `--check` on 2026-09-10 printed **7 of 7 anchors matching exactly
once**. The script prints the anchor COUNT and never the word "ok", so
"applied or not" is read from the MARKER block it prints, whose expected counts
are DERIVED from its own edit table rather than typed:

| file | marker | applied = |
|---|---|---|
| `src/nifskope.h` | `LeftWater = 3` | 2 |
| `src/nifskope_ui.cpp` | `mode > LeftWater` | 1 |
| `src/nifskope_ui.cpp` | `wwBarRowButtonQss` | 2 |
| `src/nifskope_ui.cpp` | `UI/CompactTopBars` | 2 |
| `src/nifskope_ui.cpp` | `wwWaterUiHarness` | 2 |
| `NifSkope.pro` | `src/wateruitest.cpp` | 1 |

**The order cannot go wrong**: the script applies all seven or none, and
`src/wateruitest.cpp` (E7) calls `wwBarRowButtonQss` (E3).

## 2. qmake BEFORE make, and why

`NifSkope.pro`'s `SOURCES` gains `src/wateruitest.cpp` (E7), so **qmake must
run first** or the new object never enters the build. **No `DEFINES` or
`CXXFLAGS` line is touched by any of the seven edits**, so lane BUILD9's
changed-flag staleness trap does NOT apply here and no object needs deleting
for that reason.

`src/wwskin.h` DID change (two new declarations). Every translation unit that
includes it must be newer than it after the build; the object-mtime sweep from
`nifskope-ww-build-verify` ("A successful build is not a consistent one") is
the check:

```
grep -rln '#include "wwskin.h"' src/
```

qmake regenerates the dependency block, so this should be automatic; read it
back by object name anyway (the `awk` walk in `nifskope-ww-resume-pending`
section 3), because `grep -A3` misses a dependency ten continuation lines down.

```
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/water7_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/water7_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/water7_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

The exe bungo's window holds is renamed aside, never killed -- use the `&&`
chain at the top of `nifskope-ww-build-verify`, or `tools/ww_build.sh`, so the
process guard sits immediately before the LINK and not at the top of the lane.

## 3. The syntax pass that already ran, and what it proves

`sx_WATER7.sh` at the repo root (flags read out of `Makefile.Release`, not
typed). **RC=0 on all five owned translation units** --
`src/wateruitest.cpp`, `src/watermarkpanel.cpp`, `src/watercurves.cpp`,
`src/waterwindow.cpp`, `src/watermark.cpp` -- with only the known pre-existing
`qchar.h` / `-Wsfinae-incomplete` noise.

The hook-up's own inserted TEXT was proved to compile too, without touching the
shared tree: `scratchpad/water7_20260910/sx_overlay.py` applies `hookup.py`'s
EDITS table (imported, never retyped) to copies under
`scratchpad/water7_20260910/sx/`, and

```
bash sx_WATER7.sh -Iscratchpad/water7_20260910/sx scratchpad/water7_20260910/sx/nifskope_ui.cpp
```

returned **RC=0**. That run is self-proving about the overlay: `mode > LeftWater`
only compiles against the PATCHED `src/nifskope.h`, so a run that had silently
used the unpatched header would have failed.

It proves the files compile. It proves nothing about LINKING, nothing about
moc, and nothing about behaviour. Delete `sx_WATER7.sh` when done; it is this
lane's throwaway and nobody else's (`sx_BUILD11.sh`, `sx_HKXEDIT2.sh` and
`sx_tmp.sh` at the root are other lanes' and were not touched).

## 4. The gates, in this order, one NifSkope instance at a time

```
bash tests/spells/water_ui.sh        # NEW -- the Water tab (T) + the bar row (R)
bash tests/spells/ui_align.sh        # BUILD9's, unchanged: the neighbour that must still hold
bash tests/spells/water_mark.sh      # the panel, now the tab; red 4 fixed (body 3 pinned)
bash tests/spells/water_flow.sh      # reds 2 and 3 fixed (verdict-line grep, floor 17)
bash tests/spells/water_weights.sh   # red 1: X2b is the ring now; floor 16
bash tests/spells/water_window.sh    # the window; red 5a/5b touch what it saves
bash tests/spells/lodl_water.sh      # the .lodl water reader -- red 5b changes a name offset
bash tests/spells/lodl_open.sh       # the .lodl viewer
```

`loaded_nifs.sh` and `top_bar.sh` are the SIBLINGS most likely to notice the
menu bar's new height or the retired dock; run them and compare with BUILD9's
recorded baselines (166/3 and 43/5, both pre-existing).

Pictures (both are wanted by bungo and both are owed):

```
SHOT="$(pwd)/scratchpad/water7_20260910/images/topbar_after.png" \
TABSHOT="$(pwd)/scratchpad/water7_20260910/images/watertab.png" \
  bash tests/spells/water_ui.sh
```

**The BEFORE picture was NOT taken, and the reason is a refusal, not an
oversight.** At 17:23 `scratchpad/build11_20260910/` was still being written to
(its `logs/` at 17:19:32, `gates_summary.txt` at 17:19:40) -- BUILD11 had
already linked `release/NifSkope.exe` at **17:08:39** and was running its
harnesses. Launching a NifSkope instance to grab a before picture would have
broken "one NifSkope instance ever" and landed in the middle of another lane's
gate runs. So there is no before picture on this lane's exe, and none was
faked.

The honest before/after pair is therefore: `release/NifSkope.exe` **17:08:39**
is the BEFORE state (it has BUILD11 but none of this lane), so a resume that
starts before its own build can still take one; after that it is lost. The
nearest existing stand-in is `scratchpad/build9_20260910/seam_before.png`, and
it is a different crop of a different exe -- say so if it is used.

## 5. What every number in this file is

**Predictions.** Nothing in section 0 of `scratchpad/lane_water7_report.md`
has been executed. Re-derive, do not accept -- including the gate counts, the
floors, and the claim that the row height stays 35.

**The one prediction that must be checked before the change ships**, registered
before the code was written: the menu bar joins the bar row through
`wwAlignBarRow`, which takes the TALLEST natural height among the bars. The
prediction is that the tallest is still a main toolbar (35 px) and the menu bar
GROWS from ~23-25 to 35. **If the log's R block shows the row height ABOVE 35,
the menu bar was the tallest and every other bar has just been made taller --
the opposite of the word bungo used ("compact these vertically"). In that case
report the number and set `UI/CompactTopBars` false rather than shipping it.**

## 6. The way back, exact at its off value

`UI/CompactTopBars` (QSettings, default true). False: the menu bar is not in
the row and no bar's children are restyled -- BUILD9's behaviour, byte for
byte. One reader (`wwCompactTopBars()`), so the sheet and the call site cannot
disagree. The Water tab is NOT behind that key; it is bungo's ruling, not a
preference.

## 7. Files this lane changed

Owned, edited directly (all LF-only, CR 0 before and after):
`src/watermarkpanel.h`, `src/watermarkpanel.cpp`, `src/waterwindow.cpp`,
`src/watercurves.cpp`, `src/watermark.cpp`, `src/wwskin.h`,
`tests/spells/water_flow.sh`, `tests/spells/water_mark.sh`,
`tests/spells/water_weights.sh`. NEW: `src/wateruitest.cpp`,
`tests/spells/water_ui.sh`, `sx_WATER7.sh` (throwaway).

Not owned, and NOT touched -- they are `hookup.py`'s alone:
`src/nifskope.h`, `src/nifskope_ui.cpp`, `NifSkope.pro`. `src/nifskope.cpp`
needed no edit at all: `waterMarkInstall( this )` is already called there and
its signature did not change.

`res/style.qss` was **not** changed. The row's padding is emitted at run time
from the measured row height, which a static sheet cannot know; the sheet's own
`QMenuBar::item { padding: 4px 8px }` stays as the value the off state falls
back to.

`WW_CHANGES.md` was not edited by this lane (the director splices):
`scratchpad/water7_20260910/WW_CHANGES_ENTRY.md`. `MISTAKES.md` WAS appended to
by this lane, append-only, 204,423 -> 208,304 bytes, CR 0 -> 0.
