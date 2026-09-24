> **RESOLVED 2026-09-10 by lane BUILD5b. Nothing below is a live instruction.**
> Every step here has been carried out: the hook-ups are applied, the exe is
> built (03:38:56), all six harnesses pass, gate P4 is written and measured
> (moved 0.21 degrees, tolerance 5), and the picture pair is at
> `images/charles_flow_pair.png`. `DONE` sits beside this file. What actually
> happened, including the five gates that were RED on the first build and
> what each one was, is the "Build (BUILD5/BUILD5b)" section of
> `scratchpad/lane_water3_report.md`.
>
> Two statements below were already false when lane BUILD5 died: "nothing was
> built" and "no existing `src/` file was touched". BUILD5 did both and never
> updated this file. See `MISTAKES.md`.

# Lane WATER3 — BUILD PENDING

`Fallout4.exe` was UP (pid 12500) at the one gate check, 2026-09-10 ~01:5x, so
nothing was built and no existing `src/` file was touched. `scratchpad/cardortho_20260910/DONE`
DOES exist, so lane BUILD4's side of the gate is clear; only the game blocks it.

Everything below is written and unrun. Read `scratchpad/lane_water3_report.md`
first for what each step is supposed to produce.

## What is already on disk and needs nothing

| file | state |
|---|---|
| `src/watermark.h` / `src/watermark.cpp` | NEW. `g++ -fsyntax-only` with the real `Makefile.Release` flags: **rc=0, no warnings of ours** |
| `src/watermarkpanel.h` / `src/watermarkpanel.cpp` | NEW. Same, rc=0 |
| `NifSkope.pro` | the four new paths added (`HEADERS` 264-265, `SOURCES` 416-417), LF-only, +90 bytes |
| `tests/spells/water_mark.sh` | NEW, the two-half harness |
| `scratchpad/specs_20260909/spec_water.md` | brought current: 346 bodies, the merge clause, the stride refusal reversed, G5/G6 restated, §5 as built, the P gates, a rebuilt provenance footer whose 38 line numbers were re-derived from their anchors |

## Step 1 — the three hook-ups

```
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?      # must be rc=1
python scratchpad/water3_20260910/hookup.py --check         # five anchors, no writes
python scratchpad/water3_20260910/hookup.py                 # apply
```

It refuses unless every anchor matches EXACTLY ONCE and the CR count moves by
exactly the number of CRLF lines added. Dry run, 2026-09-10 01:5x:

```
src/nifskope.cpp   4db68d35a867f2dd 433872 bytes 10517 lines CR=9370
  ok: 433872 -> 433901 bytes, 10517 -> 10518 lines, CR 9370 -> 9371   (the include)
  ok: 433872 -> 434341 bytes, 10517 -> 10525 lines, CR 9370 -> 9378   (the call)
src/nifcli.cpp     67667c8fd974d154 260816 bytes  5971 lines CR=0
  ok: 260816 -> 261671 bytes, 5971 -> 5990 lines, CR 0 -> 0
```

What they are, in words:

* `src/nifskope.cpp` — `#include "watermarkpanel.h"` after `qt5compat.hpp`, and
  `waterMarkInstall( this );` right after `initConnections();` in the
  constructor. That is the WHOLE registration: the dock, its entry in the
  Workspaces dropdown, the exclusivity rule the other manager docks obey, and
  the self-test's timer are all inside `watermarkpanel.cpp`.
* `src/nifcli.cpp` — `#include "watermark.h"`, one bool, one `--water-mark-selftest`
  argument, three help lines, and the `lodl` dispatch wrapped in braces so the
  new verb is answered before the scene builder runs.

**If an anchor has moved** (another lane edited those lines while this waited),
the script says which and writes nothing. Re-anchor by hand; do not loosen the
match.

## Step 2 — qmake BEFORE make

Two new translation units and a new header included by two existing files, so
`Makefile.Release` must be regenerated or the link mixes stale objects
(`nifskope-ww-resume-pending` step 3):

```
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/water3_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/water3_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/water3_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

Then read the dependency back by name, per object:

```
grep -n "watermark\.h\|watermarkpanel\.h" Makefile.Release | cut -c1-160
```

`nifskope.o` must list `src/watermarkpanel.h` and `nifcli.o` must list
`src/watermark.h`. Both new headers are under `src/`, so qmake's scan reaches
them (the `lib/` exception in the skill does not apply).

## Step 3 — the exe is newer than every changed file

```
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue; [ "$EXE" -nt "$f" ] || echo "STALE vs $f"; done
```

## Step 4 — the gates, in this order

1. **`bash tests/spells/water_mark.sh`** — the lane's own, both halves. Floors:
   14 model checks, 16 dock checks, and both halves must print PASS. It copies
   the fixture, because the model half REWRITES the file it is given.
   Add `SHOT=E:/Projects/NifskopeWildWastelandEdition/scratchpad/water3_20260910/dock.png`
   for the dock grab (gate P6, first half).
2. **`bash tests/spells/lodl_water.sh`** — WATER2's 56 checks must still be
   56/0. Nothing in this lane touches `src/lodtfile.cpp`, so a change here would
   be a surprise and that is exactly why it is run.
3. **`bash tests/spells/lodl_open.sh`** — 23/0. The viewer's plane route is
   untouched but shares the reader.
4. **`bash tests/spells/lod_generation.sh`** — the LOD panel's own house-style
   counts. Run because a second dock now joins the same Workspaces menu and the
   exclusivity rule is shared; its floor is 74 checks.
5. Skipped, and why: every harness that reads no `.lodl` and builds no panel —
   collision, block list, impostor, atlas, arrays, merge, VT, render_shot.

## Step 5 — the picture pair (gate P6, second half)

The "before" already exists at the exact framing:
`scratchpad/water2_20260909/images/charles_flow.png`
(`WW_LODL_REGION=-16,-21,-6,-4,0`, `WW_LODL_PLANE=flow`, top view, flat,
1500x1000). The "after" is the same render of the file the harness marked:

```
WORK=scratchpad/water3_20260910/work
WW_LODL_REGION=-16,-21,-6,-4,0 WW_LODL_PLANE=flow \
WW_RENDER_SHOT=scratchpad/water3_20260910/charles_flow_after.png \
  release/NifSkope.exe "$WORK/panel.lodl"
```

(`nifskope-ww-render-shot` for the switches and the one-instance rule; the dock
harness leaves `work/panel.lodl` marked and saved.) Then put the two halves in
one sheet with `scratchpad/water2_20260909/make_sheet.py` as the pattern —
same framing, captions carrying the numbers the builder itself printed.

## Step 6 — gate P4, which is NOT in the harness yet

`WaterMarkDoc::setFlowRate` re-derives the flow plane at 8 or 16 samples a cell
from the same world-coordinate strokes; the Bake section's row drives it.
The gate the spec asks for — mark at 32, write at 8, the same body still points
the same way within 5 degrees — needs three lines in
`lodtWaterMarkSelfTest`: after the isolation case, call `setFlowRate( 8 )`,
`save()`, re-open, and compare the body's mean direction. It was left out
because it needs a run to calibrate the tolerance honestly, and this lane could
not run anything.

## Step 7 — the four documents

`WW_CHANGES.md` (text in `scratchpad/water3_20260910/WW_CHANGES_ENTRY.md`, and
the file is MIXED — assert the CR count is unchanged), `MISTAKES.md`,
`HANDOFF.md`'s top block, and `scratchpad/lane_water3_report.md`'s Build
section.

## What would make this lane WRONG, and how you would see it

* the flow re-derivation is NOT byte-identical on an unmarked file — the first
  two checks of the model half. It was pre-measured in Python and came out
  exact (`repro_flow.py`: 0 mismatches on 37,748,736 texels, floor 25,114), so a
  red here is the C++ twin drifting from `lodtPackPlane`, not the idea;
* the isolation sweep is slow rather than wrong: it walks all 36,864 tiles three
  times in the model half. If it takes minutes, that is the cost, not a hang;
* `waterMarkInstall` finds no `ViewWorkspacesButton` — then the dock exists but
  has no menu entry, and the self-test still passes because it shows the dock
  itself. Check the dropdown by eye once.
