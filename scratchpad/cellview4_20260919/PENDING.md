# PENDING -- lane CELLVIEW4, the blended ground

Written 2026-09-19 20:3x (clock read with `date`). The lane is CODE-ONLY: it
compiled nothing and ran no exe. **Nothing below has been run.** The C++ in
`src/cellsplat.cpp` and `src/cellsplat.h` has never been through a compiler --
see step 0 for why -- so treat the first build as the first review.

Run the steps in order. Any step that fails stops the rest.

---

## 0. THE SYNTAX PASS, FIRST, BEFORE ANYTHING IS HOOKED UP

`g++` in this lane's shell exits 1 with **zero bytes of stderr** even for
`int main(){return 0;}`, so every "clean" syntax pass the lane could have
reported would have been an artifact of swallowed output. The lane stopped
claiming them. Do this in a shell where the compiler actually speaks:

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
  'cd /e/Projects/NifskopeWildWastelandEdition && \
   g++ -fsyntax-only -std=c++17 -I src -I lib -I lib/libfo76utils/src \
       $(pkg-config --cflags Qt5Core 2>/dev/null) src/cellsplat.cpp; \
   echo RC=$?'
```

If the include flags fight you, do not spend time on them -- the real syntax
gate is step 3, and a failure there points at the same lines. What matters is
that step 0 is attempted with a compiler whose errors you can see.

## 1. THE HOOK-UP, DRY

```bash
cd /e/Projects/NifskopeWildWastelandEdition/scratchpad/cellview4_20260919
python hookup.py --check
```

Expect **13 of 13 anchors match once** and `NOTHING WAS WRITTEN`. The script
refuses outright if any anchor matches other than once, or if a target file
already carries the string `lane CELLVIEW4` (that -- not an anchor -- is the
already-applied marker). No anchor is in `src/lodgen.cpp`; lane IMPOSTORFIX5
owns that file and this lane never touched it.

Optional, and worth the ten seconds: prove the apply without applying it.

```bash
python hookup_dryrun.py
```

It runs the same table through the same `apply_to()` onto **copies** under this
folder, and checks each copy's CR delta and its brace/paren delta against what
the inserted text carries. Last run: every file `OK`, whole-file braces `+0`.
It leaves `copy_*` files behind -- delete them, they are not source.

## 2. THE HOOK-UP, FOR REAL

```bash
python hookup.py --apply
```

Thirteen edits over four files: `NifSkope.pro` (2), `src/esmdata.h` (2),
`src/esmdata.cpp` (1), `src/cellview.cpp` (8). All four are LF-only and the
script asserts the CR count does not move.

## 3. QMAKE, THEN BUILD

`NifSkope.pro` gained two lines, so the Makefiles are stale and **qmake is not
optional**:

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
  'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro'
bash tools/ww_build.sh src/cellsplat.cpp src/cellsplat.h src/cellview.cpp src/esmdata.cpp src/esmdata.h
```

`ww_build.sh` refuses while `Fallout4.exe` is up and renames a running exe
aside rather than killing it.

**Expect to fix compile errors here.** The lane's own hand review already found
one live bug in `cellsplat.cpp` (a quadrant taken from the corner instead of
from the quad, which would have indexed a neighbouring quadrant's layer list
along the cell's centre lines); a file that has never been compiled will have
more.

## 4. THE GATES

### 4a. The blend gate -- the known answer

```bash
python tests/spells/cell_splat_compare.py \
  <shot.png> \
  scratchpad/cellview4_20260919/images/sim_m20_7_mosaic.png \
  scratchpad/cellview4_20260919/images/sim_m20_7_blended.png
```

The script starts nothing; you supply the shot. It reduces all three to 32x32
per-quad block means, fits one gain+offset per channel over the whole cell (so
exposure cannot be mistaken for a result), and asks: of the quads where the two
rules DISAGREE, how many is the shot closer to BLENDED on.

Pre-registered and both ends measured by feeding each target in as the shot:

| shot fed in | disagreeing quads sided with blended | verdict |
|---|---|---|
| the blended target (a correct build) | 249/249, 100.0% | PASS |
| the mosaic target (today's build) | 187/249, 75.1% | FAIL |
| the line | 90% | 15 points above the failing case |

A score near 75% means the blend did not happen at all. A score between 76% and
90% means the layers composite but in the **wrong order** -- check the ground
legend does not say "WITHOUT THE PAINT ORDER".

### 4b. The legend, read from the viewer's own census line

It must print the passes-per-quad multiplier and must **not** say "WITHOUT THE
PAINT ORDER". If it does, `WW_CELLSPLAT_LAYER_INDEX` did not survive step 2.

### 4c. The budget

`cellSplatCountVerts()` runs before any allocation and the mosaic draws the
rectangle when the count is past `CELL_MAX_TOTAL_VERTS` (12,000,000). Ask for a
large rectangle and confirm the census says the blend REFUSED rather than the
viewer stalling or dying.

## 5. THE PICTURES TO SHOOT

Same camera as CELLVIEW3's, so the pairs are comparable.

1. **`ground_m20_7.png`** -- Sanctuary -20,7 alone, top-down, terrain only, the
   framing `splat_sim.py` used. This is the one step 4a eats. Its target is
   `scratchpad/cellview4_20260919/images/sim_m20_7_blended.png`.
2. **`ground_m20_7_before.png`** -- the same frame from the rung built before
   this change, for the pair.
3. **`after_downtown.png`** -- the exact camera CELLVIEW3 used (centre
   22528,-43008; 2.69704 units/px; 1822x925). Two things to read off it:
   the solid black arrow at roughly (1364,547) must be **gone** (it is an
   editor marker, see the report's item 3), and the transparent pass must not
   have got worse -- the root became a `BSOrderedNode`, which sorts **every**
   transparent shape by block number instead of depth. If glass or tree cards
   come back sorting wrong, that is this change and the fix is to give the
   ground its own `BSOrderedNode` child instead of the scene root.
4. **`ground_bare_quads.png`** -- -20,7 with the ground only, to look at the
   six quads that stay bare under the four-corner rule:
   (25,27) (27,18) (28,25) (30,16) (31,16) (31,17). All six are in quadrant 3,
   which carries no BTXT. The lane proposes no invention there -- see item 2.

All pictures land under `scratchpad/cellview4_20260919/`.

## 6. THE DOCS

`WW_CHANGES.md` and `HANDOFF.md` text is in this folder's `report.md`, section
"the doc text". The lane wrote neither file -- the director splices them.

The root `MISTAKES.md` entry **was** written by the lane (byte splice, CRLF
preserved: CR 10517 -> 10550, LF 10517 -> 10550, +33 lines).
