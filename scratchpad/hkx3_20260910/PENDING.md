# Lane HKX3 -- BUILD PENDING. The exact resume.

**Why pending.** Lane BUILD8 was still building at 14:56 (its
`scratchpad/build8_20260910/DONE` was absent; the folder was being written into
that minute) and it owns `NifSkope.pro`, `src/nifskope.cpp`,
`src/nifskope_ui.cpp` and `src/gltfexport.*`. HKX3 may not edit those files
while it is alive, and it may not build over it. Checked ONCE, never polled:

```
scratchpad/build8_20260910/DONE          ABSENT
tasklist | grep -i -E "Fallout4|NifSkope";  rc=1   (nothing running)
```

So: nothing was built, no exe was launched, no gate was run, no picture was
taken. **Every claim about behaviour in `scratchpad/lane_hkx3_report.md` is a
claim of MECHANISM, not a measurement.**

Read first: `CONSTITUTION.md`, then `scratchpad/lane_hkx3_report.md` sections 5
and 6 (the pre-registered gates and the hook-up), then this file.

---

## Step 0 -- the gate before anything

```bash
cd /e/Projects/NifskopeWildWastelandEdition
test -f scratchpad/build8_20260910/DONE && echo BUILD8-DONE || echo BUILD8-RUNNING
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
```

BUILD8 must be DONE and `rc=1`. If either fails, stop and stay PENDING: HKX3's
hook-up writes into two files BUILD8 owns.

## Step 1 -- apply the hook-up (nine edits, two files)

```bash
python scratchpad/hkx3_20260910/hookup.py            # --check, writes nothing
python scratchpad/hkx3_20260910/hookup.py --apply
```

`--check` printed, at 15:0x on 2026-09-10, **nine anchors each matching x1**,
`NifSkope.pro` CR 0 -> 0 (18,982 bytes) and `src/nifskope_ui.cpp` CR 0 -> 0
(1,476,286 bytes). If any anchor now matches 0 or 2 times, BUILD8 has moved the
line: **re-anchor by hand, do not loosen the anchor.**

`--check` never prints "ok" and still matches after `--apply` (an "after" anchor
survives its own insertion). Decide applied-or-not by the marker:

```bash
grep -c "lane HKX3" NifSkope.pro src/nifskope_ui.cpp     # expect 1 and 7
```

Predicted byte delta after `--apply`: `NifSkope.pro` +~330, `src/nifskope_ui.cpp`
+~2,050. CR must stay 0 on both.

## Step 2 -- qmake BEFORE make

The `.pro` gained two SOURCES, one HEADERS and one DEFINES line, and
`src/ui/widgets/timeline.cpp` gained four new `#include`s
(`hkxanimui.h`, `hkxplayback.h`, `glview.h`, `gl/glscene.h`). qmake freezes
dependency lists when the Makefile is generated, so this is not optional.

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/hkx3_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/hkx3_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/hkx3_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

Both RCs must be 0. `make`'s own exit code is the gate, never a grep.

**Dependency read-back, per object, by name** (never `grep -A3`):

```bash
grep -n "hkxanimui\.h" Makefile.Release | cut -c1-160
for n in <those line numbers>; do awk -v s=$n 'NR<=s && /^GeneratedFiles\/\.obj\/[a-z_]*\.o:/ {last=$0} NR==s {print last}' Makefile.Release; done
```

`hkxanimui.h` must be named by `timeline.o`, `nifskope_ui.o`, `hkxanimui.o` and
`hkxanimuitest.o` -- **four**. `moc_hkxanimui.cpp` must exist under
`GeneratedFiles/` (the hub has `Q_OBJECT`); if it does not, qmake did not pick
up the new HEADERS line and the link will fail on the vtable.

Also check `WW_HKXANIM_UI` actually reached the compiler, because everything
this lane added to the dock is behind it and a missing define builds a GREEN
suite with none of the feature in it:

```bash
grep -c "WW_HKXANIM_UI" Makefile.Release        # expect >= 1
grep -c "TimelineSeqBox" release/NifSkope.exe   # a string only the new code has; expect >= 1
```

## Step 3 -- staleness sweep over the WHOLE working set

```bash
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue
  [ "$EXE" -nt "$f" ] || echo "STALE vs $f"
done
cmp res/style.qss release/style.qss && echo "sheet equal"
```

## Step 4 -- the gates (one instance at a time, sequential)

```bash
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?      # rc=1 before EVERY launch
bash tests/spells/hkxanim_ui.sh                            # lane HKX3
bash tests/spells/hkxanim_play.sh                          # lane HKX2, unchanged behaviour
```

`hkxanim_ui.sh` defaults are already the fixtures; nothing needs overriding. It
prints every `SKIP` line after the log -- **a SKIP is never a pass.**

Expected, pre-registered:

| gate | expected |
|---|---|
| (a) | one new row, 93 frames, 60 fps, and the ROW SAYS both |
| (b) | worst translation <= 1e-4, rotation <= 0.01 deg over the bound nodes at frame 46; the floor (frame 0) goes red |
| (c) | readout `frame 46 / 92`; speed 1 -> 2 -> 1; the Loop action flips; cycle = CycleLoop |
| (d) | 0 nodes differ after unload, > 0 while posed |
| (e) | drag-enter and drop accepted, same row, same 93/60; the junk floor adds no row |
| (f) | `skeleton.hkx` is a row marked refused with a sentence; the good clip is not |
| (g) | 0 unstamped number fields of >= 3; 0 group boxes; 0 unstyled selectors of >= 1; 6 of 6 controls with tooltips; the summary line and the list outside the splitter; the wheel guard both ways |
| picture | `scratchpad/hkx3_20260910/dock_clip_midclip.png`, > 400x80 |

`hkxanim_play.sh` must still be **27 checks / 0 failures, 78 / 17 / 4** on
`fixtures/human_male_vanilla.nif`. That is the regression gate: HKX3 changed the
route the render toolbar's Load button takes, and HKX2's own gate is what proves
the playback underneath it is untouched.

## Step 5 -- the four documents

1. `WW_CHANGES.md` -- splice `scratchpad/hkx3_20260910/WW_CHANGES_ENTRY.md`,
   status block replaced with the measured one. The file is MIXED; the 2026-09
   entries at the top are LF-only. Assert the CR count is unchanged before and
   after (`b.count(b'\r')`, Python, never grep). **Never `sed -i` it.**
2. `MISTAKES.md` at the root -- section 7 of the lane report, plus anything the
   build finds.
3. `scratchpad/lane_hkx3_report.md` -- APPEND a `## Build (<lane>)` section with
   the gate table, the mtimes in one table, and what was skipped.
4. `HANDOFF.md` top block.

Then: **EXE FREE FOR BUNGO**, and tell him his open window needs a restart.

## What is likely to go wrong, and what it means

* **Link error, undefined `WwHkxAnimHub::staticMetaObject`** -- the HEADERS line
  did not reach qmake; re-run qmake and check `GeneratedFiles/moc_hkxanimui.cpp`.
* **The dock looks unchanged and `hkxanim_ui.sh` fails at "the dock's animations
  list is there"** -- `WW_HKXANIM_UI` is not defined (edit 3 of the hook-up), so
  every clip-shaped line in `timeline.cpp` compiled out. Grep the Makefile.
* **`hkxanim_ui.sh` fails only at gate (e)** -- the drop edit (edit 9) landed in
  the wrong branch of the application event filter, or BUILD8 moved the
  `belongsHere` block. Read `src/nifskope_ui.cpp` around the marker.
* **(c) says `frame 23 / 92`** -- the clip's rate was not adopted; the dock is
  ticking at its stored 30 fps preference. `TimelineWidget::hkxAdoptRate`.
* **(b) fails with a huge rotation** -- not a UI defect. Re-run
  `hkxanim_play.sh`; if that fails too the playback moved under this lane.
