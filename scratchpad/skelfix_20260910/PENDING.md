# Lane SKELFIX -- BUILD PENDING resume (paste-able)

Written 2026-09-10 ~16:2x. The build slot was held: `scratchpad/build10_20260910/`
carried `BUILDING_WATER5` (16:20) and **no `DONE`**, so nothing was built, no
`BUILDING` marker was created and `release/NifSkope.exe` (15:52:46) was not
touched. `tasklist | grep -i -E "Fallout4|NifSkope"` -> `rc=1` at that moment.

Everything below is a PREDICTION. Re-derive each number, do not accept it
(`nifskope-ww-resume-pending` section 9).

## What is ALREADY APPLIED in the tree (do not re-apply)

| file | state | proof |
|---|---|---|
| `src/glview.cpp` | the armature rule + the eager rebuild | `grep -c "lane SKELFIX"` = **5** |
| `src/glview.h` | `skipped`, `skeletonOverlayInArmature()`, `skeletonOverlayRule()`, the three members | `grep -c "lane SKELFIX"` = **3** |
| `src/skeloverlaytest.cpp` | gates (f) (f') (g) (g') (h) (i), and `skipped` added to the (a') floor | `grep -c "SKELFIX"` = **2** |
| `tests/spells/skeleton_overlay.sh` | the 4th render + gate (j) + its floor | `grep -c "SKELFIX"` = **2** |
| `tests/spells/skeleton_overlay_mask.py` | NEW, gate (j) itself | new file |

Line endings, Python byte counts: `src/glview.cpp` CR 23,423 / LF 23,494
(was 23,284 / 23,355 -- dCR +139 = dLF +139, its CRLF run intact);
`src/glview.h` CR 0; `src/skeloverlaytest.cpp` CR 0;
`tests/spells/skeleton_overlay.sh` CR 0; `skeleton_overlay_mask.py` CR 0.

## Step 1 -- the ONE cross-file edit, not applied

`src/nifskope_ui.cpp` belongs to another lane. Its tooltip edit is a refusing
script:

```bash
cd /e/Projects/NifskopeWildWastelandEdition
python scratchpad/skelfix_20260910/hookup.py --check   # predicts: anchor x1, +905 bytes, CR 0 -> 0, marker x2
python scratchpad/skelfix_20260910/hookup.py --apply
grep -c "lane SKELFIX" src/nifskope_ui.cpp             # predicts 2
```

Nothing else needs it: `skeletonOverlayRule()` is a public accessor and gate (i)
reads it directly, so a resume that skips this step still gets a green suite --
it only loses the tooltip.

## Step 2 -- build

No `.pro` change and no new `#include` in an existing file, so qmake is only
needed if another lane in the same build added one. `src/glview.h` DID change,
so the two-header object sweep is required:

```bash
grep -rln '#include "glview.h"' src/ | tee /dev/stderr | while read f; do
  o=GeneratedFiles/.obj/$(basename $f .cpp).o
  [ ! -f "$o" ] && echo "(no object yet) $o" && continue
  [ "$o" -nt src/glview.h ] && echo "ok   $o" || echo "STALE $o"
done
```

Then the chain from `nifskope-ww-build-verify` (make's own `$?` gates; the exe
his window holds is renamed aside, never killed).

## Step 3 -- the gate

```bash
timeout 500 bash tests/spells/skeleton_overlay.sh 2>&1 | tail -40
```

PREDICTED, from the offline model in `scratchpad/skelfix_20260910/measure.py`
whose controls reproduce the built application exactly (dock 130 / 93 / 93 / 0
and the clip's 78 / 17 / 4):

| line | predicted |
|---|---|
| overlay ON census, bind pose | nodes 130, missing 0, **segments 110, stubs 62, skipped 38** |
| (a) (b) (c) (c') (d) (d') (e) | unchanged from BUILD9's 17/0 |
| (f) longest bind bone-to-bone | **22.40**, limit **33.60** |
| (f) longest drawn at frame 46 | **31.94** -> pass |
| (f') old rule longest | **300.5** -> the floor goes red as it must |
| (g) endpoints outside the bone box | **0** (worst overshoot 0.000 for bodies; stubs are capped at 2x the characteristic bone size = 2.20) |
| (g') old rule endpoints outside | **19**, worst **241.3** |
| (h) skipped | 38 with the overlay on, **0** with it off |
| (i) armature / marker-only | **111 / 19** |
| (j) stray pixels in `on_frame46.png` | **0** |
| (j') `before_on_frame46.png` | must FAIL (j) |

Total: 17 old checks + 12 new in-app + 5 in the picture gate.

`scratchpad/skelfix_20260910/before_on_frame46.png` is the BUILD9 picture the
defect was seen in, kept **because gate (j') is the only floor for the picture
gate** -- do not delete it (`nifskope-ww-resume-pending` section 8).

## Step 4 -- the neighbours the change reaches

`src/glview.cpp`'s pose-armature helpers were not touched by this lane, but the
file was: run `tests/spells/skeleton.sh` (`WW_SKELETON_TEST`) and record
`WW_POSEDRAW_TEST`'s number against SKELOVERLAY's baseline (it was already
failing at "clicking a bone did not make it the active object" on both fixtures
BEFORE this lane -- see `scratchpad/lane_skeloverlay_report.md`, section
"The pose-armature factoring"). A red there is not this lane's.

## Step 5 -- the four documents

1. `WW_CHANGES.md` -- splice `scratchpad/skelfix_20260910/WW_CHANGES_ENTRY.md`
   with the MEASURED numbers replacing the predicted ones (the file is mixed;
   the 2026-09 entries at the top are LF-only; assert the CR count is unchanged).
2. `MISTAKES.md` -- already carries this lane's entry; add anything the build finds.
3. `scratchpad/lane_skelfix_report.md` -- append a `## Build (<lane>)` section.
4. Re-render the three pictures on the FINAL exe, and hand bungo
   `scratchpad/skeloverlay_20260910/on_frame46.png` beside
   `scratchpad/skelfix_20260910/before_on_frame46.png`.
