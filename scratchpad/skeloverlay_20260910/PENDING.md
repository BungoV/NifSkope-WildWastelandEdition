# PENDING -- lane SKELOVERLAY (Overlays > Show Skeleton)

**Why pending:** the brief allowed ONE check for `scratchpad/build8_20260910/DONE`
and it was not there (checked once, 2026-09-10 ~15:1x; BUILD8's own directory
was still being written into at 14:52:47 and the exe it produced is
`release/NifSkope.exe` 2026-09-10 14:37:53). `tasklist` showed no `Fallout4.exe`
and no `NifSkope.exe` (`rc=1`), and no other lane's `BUILDING` marker existed --
the build slot was free, the BUILD8 handshake was not. Nothing was built and no
`BUILDING` marker was created.

Code, harness and pictures script are on disk and all three changed translation
units pass `-fsyntax-only`. Nothing is committed.

## Step 0 -- the slot

```bash
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     # must be rc=1
ls scratchpad/build8_20260910/DONE                        # BUILD8 finished
ls scratchpad/filestab_20260910/BUILDING scratchpad/hkx3_20260910/BUILDING
touch scratchpad/skeloverlay_20260910/BUILDING            # and delete it after
```

## Step 1 -- the hook-up (REFUSING script, five edits, unapplied)

```bash
python scratchpad/skeloverlay_20260910/hookup.py            # --check, writes nothing
python scratchpad/skeloverlay_20260910/hookup.py --apply
```

Checked 2026-09-10: **5 of 5 anchors match exactly once**, no text already
present, `NifSkope.pro` CR 0 / LF 726 and `src/nifskope_ui.cpp` CR 0 / LF 31495
(both LF-only, the script asserts dCR 0). Predicted growth: `NifSkope.pro`
+27 bytes, `src/nifskope_ui.cpp` +1901 bytes.

**Lanes FILESTAB and HKX3 are writing into `src/nifskope_ui.cpp` and BUILD8 owns
`NifSkope.pro`.** If either has landed since, re-run `--check` first: the script
refuses on any anchor that no longer matches once and writes NOTHING, and every
inserted block carries the marker `lane SKELOVERLAY`, so "applied or not" is
decided by `grep -c "lane SKELOVERLAY" src/nifskope_ui.cpp` (expect 4 after
apply, 0 before) and not by the anchor still matching -- which it does either
way.

## Step 2 -- qmake BEFORE make

`NifSkope.pro` gains `src/skeloverlaytest.cpp`, and `src/skeloverlaytest.cpp`
includes `glview.h`, `skeletontools.h`, `hkxplayback.h`, `gl/glnode.h`,
`gl/glscene.h`, `model/nifmodel.h` -- a new object with new dependencies, so the
Makefile must be regenerated, not just re-made.

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/skeloverlay_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/skeloverlay_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/skeloverlay_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

Read the dependency back by name:

```bash
grep -n "skeloverlaytest" Makefile.Release | cut -c1-160
```

## Step 3 -- TWO HEADERS CHANGED: sweep the objects, do not trust make

`src/glview.h` and `src/gl/glscene.h` both grew members. Every object that
includes them must be newer than them:

```bash
for H in src/glview.h src/gl/glscene.h; do
  echo "== $H"
  for f in $(grep -rln "#include \"$(basename $H)\"" src/ | grep '\.cpp$'); do
    o=GeneratedFiles/.obj/$(basename $f .cpp).o
    [ -f "$o" ] || { echo "no object yet $o"; continue; }
    [ "$o" -nt "$H" ] && echo "ok   $o" || echo "STALE $o"
  done
done
```

A `.o` older than a header it includes is a stale build whatever make says
(`nifskope-ww-build-verify`, "A successful build is not a consistent one").
`glscene.h` is included very widely, so expect a long rebuild.

Then the whole-working-set sweep:

```bash
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue; [ "$EXE" -nt "$f" ] || echo "STALE vs $f"; done
cmp res/style.qss release/style.qss && echo "sheet in step"
```

## Step 4 -- the gates and the pictures (one command, ~4 runs of the app)

```bash
timeout 900 bash tests/spells/skeleton_overlay.sh 2>&1 | tail -60
```

Expected on `fixtures/human_male_vanilla.nif`, per bungo's own screenshot
footer: **130 node(s) shown, 93 bone(s)**. Those two numbers are read from the
DOCK at run time, not typed into the gate -- the harness compares the overlay's
census against the dock's own tree and filter counts, so if the fixture's real
numbers are not 130/93 the gate still holds and the log says what they are.

The four pre-registered floors must all be seen to be capable of failing:
`(a')` empty census while off, `(c')` some pixels differ AND the mask is under
80% of the frame, `(d')` the clip actually moved the joints.

Artefacts the run produces:

* `release/ww_skeloverlay_test.log` -- `N checks, M failures` then `PASS`/`FAIL`
* `scratchpad/skeloverlay_20260910/off.png`, `on.png`, `on_frame46.png`
  (the render hook, one pinned orthographic camera)
* `scratchpad/skeloverlay_20260910/gates/gate_{off,on,on_frame46,mask}.png`
  (the harness's own evidence; `gate_mask.png` paints the mask dark grey and
  every changed pixel orange)

## Step 5 -- only the harnesses this change reaches

* `tests/spells/skeleton_overlay.sh` -- the lane's own, above.
* **`WW_SKELETON_TEST` and `WW_POSEDRAW_TEST` must be run too**, and neither has
  a shell wrapper in `tests/spells/` -- they are driven straight off the
  environment (checked 2026-09-10: `grep -rn` finds them only in `src/` and in
  `WW_CHANGES.md`). They matter here because `characteristicBoneSize()` and
  `boneTailIn()` were factored OUT of `refreshPoseBoneSize()` and
  `poseBoneTail()`, so the Pose Mode / Skeleton Manager armature now goes
  through shared code. **The factoring is intended to be behaviour-identical and
  that claim is untested.**

  ```bash
  . tests/spells/_harness.sh
  SK="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/character/characterassets/skeleton.nif"
  rm -f release/ww_skeleton_test.log
  WW_SKELETON_TEST=1 WW_SKELETON_SHOT="$(winpath "$PWD/scratchpad/skeloverlay_20260910/skelmgr.png")" \
    timeout 300 release/NifSkope.exe --port 42305 "$SK" >/dev/null 2>&1
  tail -20 release/ww_skeleton_test.log
  rm -f release/ww_posedraw_test.log
  WW_POSEDRAW_TEST=1 timeout 300 release/NifSkope.exe --port 42306 \
    "$(winpath "$PWD/fixtures/human_male_vanilla.nif")" >/dev/null 2>&1
  tail -20 release/ww_posedraw_test.log
  ```

  Both must print the same counts they printed BEFORE this lane. If either
  moves, the factoring is the first suspect and the revert is small: give
  `refreshPoseBoneSize()` and `poseBoneTail()` their old bodies back (they are
  reproduced verbatim inside `scratchpad/skeloverlay_20260910/patch_glview.py`,
  edits `refreshPoseBoneSize` and `poseBoneTail`) and leave the two shared
  helpers for the overlay's use only.
* Nothing else: no reader, writer, LOD or terrain path was touched.

## Step 6 -- the four places that must stop saying "not built"

1. `WW_CHANGES.md` -- splice
   `scratchpad/skeloverlay_20260910/WW_CHANGES_ENTRY.md` in, with the measured
   numbers filled in. **Never `sed -i` that file**: it is mixed and stays so;
   assert its CR count is unchanged.
2. `MISTAKES.md` -- anything found while building.
3. `scratchpad/lane_skeloverlay_report.md` -- APPEND a `## Build` section with
   the gate table and one mtime table. Do not rewrite the lane's own text.
4. Tell bungo his open NifSkope window needs a restart, and that the tick is
   **Overlays > Show Skeleton** (default off; tick **Show Nodes** as well for
   bone names).

## What is NOT proven by any of the above

* That the overlay is legible on a dense facial rig. Nothing here measures
  readability; the picture is the only instrument and bungo is the judge.
* That the pose-armature factoring is behaviour-identical -- step 5's second
  and third harnesses are what would say so, and they have not run.
* Linking. `-fsyntax-only` proves the three translation units compile and
  nothing about the link or about moc.
