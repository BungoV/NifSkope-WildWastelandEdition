# Lane HKXEDIT2 -- BUILD PENDING resume (nifskope-ww-resume-pending)

Written 2026-09-10 by a lane that could NOT build or run `release/NifSkope.exe`
(lane BUILD10 held the slot; lanes SKELFIX and HKXEDIT1 pending). Every number
below that the exe would produce is a PREDICTION (skill rule 9): re-derive,
never accept. Read `scratchpad/lane_hkxedit2_report.md` first, then this.

## 0. Process check

```bash
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     # rc=1 or stop: BUILD PENDING again
```

## 1. What is on disk (all LF, CR 0 by Python byte count)

New: `src/hkxclipedit.{h,cpp}`, `src/animdopesheet.{h,cpp}`,
`src/animworkspace.{h,cpp}`, `src/animworkspacetest.cpp`,
`tests/hkxclipedit_gate.cpp`, `tests/spells/animws.sh`,
`res/hkx_annotation_vocabulary.txt`, `sx_HKXEDIT2.sh`,
`scratchpad/hkxedit2_20260910/{hookup.py,build_gate.sh,annot_vocab.py,
CHANGE_NEEDED.md,WW_CHANGES_ENTRY.md,PENDING.md,out/}`.
Changed: `src/hkxplayback.h` (+replaceClip, setHeldNode, activeEntry, heldNode),
`src/hkxplayback.cpp` (+replaceClip body, the held-node test in applyLocal).
Every new/changed source passes `bash sx_HKXEDIT2.sh <file>` (the real
Makefile.Release flags) with `EXTRA_DEFS=""` AND with
`EXTRA_DEFS="-DWW_ANIMWS_HKXMODEL -DWW_HKXCLIP_CANON"`: ALL-RC=0.

## 2. Order

1. **Lane HKXEDIT1's hook-up first**: `python scratchpad/hkxedit1_20260910/hookup.py --check`
   then `--apply` (10 edits; it puts `src/hkxfile.cpp` and `src/hkxmodel.cpp`
   in the .pro and `HkxModel * hkx` in the window). This lane's Save goes
   through `Hkx::File` (`WW_HKXCLIP_CANON`) and its script REFUSES until
   `src/hkxfile.cpp` is in the .pro.
2. `python scratchpad/hkxedit2_20260910/hookup.py` (check): expect
   **15 anchors matching once over 4 files** (NifSkope.pro 4, src/nifskope.h 3,
   src/nifskope_ui.cpp 7, src/nifskope.cpp 1 -- the last is CRLF, the script
   picks the ending), and `tier 2 (WW_ANIMWS_HKXMODEL): yes` once step 1 is
   applied (then 16 anchors). Then `--apply`. It asserts the CR count moves by
   exactly the CRs of the inserted text (2 on nifskope.cpp, 0 elsewhere).
   A marker `lane HKXEDIT2` in a file = applied before.
3. **qmake BEFORE make** (three HEADERS, four SOURCES, two DEFINES, one copy line):
   ```bash
   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/hkxedit2_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/hkxedit2_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/hkxedit2_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
   ```
   The DEFINES trap (MISTAKES 2026-09-10, BUILD9): `make` keeps objects newer
   than their source even when a define changed. After qmake, delete
   `GeneratedFiles/.obj/hkxclipedit.o`, `hkxplayback.o`, `glnode.o`,
   `nifskope_ui.o`, `nifskope.o` if they exist from an earlier build, or the
   link mixes objects compiled without `WW_HKXCLIP_CANON` / `WW_ANIMWS_HKXMODEL`.
4. Dependency read-back: `grep -n "animworkspace\.h\|hkxclipedit\.h" Makefile.Release`
   and the awk walk from the skill; expect `animworkspace.o`, `animworkspacetest.o`,
   `nifskope_ui.o`, `nifskope.o` naming `animworkspace.h`, and `hkxclipedit.o`,
   `animdopesheet.o`, `animworkspace.o`, `animworkspacetest.o` naming
   `hkxclipedit.h`. `grep -c "WW_HKXCLIP_CANON\|WW_ANIMWS_HKXMODEL" Makefile.Release`
   >= 1 each (the way to prove a define reached the compiler; never grep the exe).
5. Exe-newer sweep over `git status --porcelain -- src res tools tests` (skill s4).
6. `release/style.qss` compares equal to `res/style.qss` (no sheet change here).

## 3. The gates, in order (one NifSkope instance at a time)

```bash
# the standalone document gate (no exe needed; already run: 72/0 + HKXPACK 2/1)
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkxedit2_20260910/build_gate.sh'
# the in-app gate, first run ever
bash tests/spells/animws.sh
# the neighbours the change reaches (the playback file changed; the old dock untouched)
bash tests/spells/hkxanim_play.sh     # HKX2's 27 checks, 0 failures on fixtures/human_male_vanilla.nif (absolute SRC!)
bash tests/spells/hkxanim_ui.sh       # HKX3's 48 checks, 1 known red (the focus floor)
bash tests/spells/hkxmodel_test.sh    # HKXEDIT1's, first run ever (its PENDING)
```

Predictions for `animws.sh` (`release/ww_animws_test.log`): (a) 78 bone rows,
17 unbound, 93 keys per row, ruler 60, readout `frame 46 / 92`; (b) 0 deg off
in the document, <= 0.01 deg on the NODE at frame 46, floor > 0.5 deg at
frame 0; (c) 92 keys after the delete, Undo x2 exact, Redo x2 = 92; (d) the
vocabulary >= 50 names with FootLeft, the annotation at 30, marker row count
+1, undo/redo; (g) the saved file bit-identical; (e) 41 / 47 frames, end
40/60 s; (f) 487.6 -> 0, unbake exact; (i) on `10mmPistol.nif` (if it has a
NiControllerSequence -- if not, SEQNIF=<a NIF with one>, e.g. a door or the
`fixtures/` set: pass one that `nifskope-cli list F -t NiControllerSequence`
shows); (j) >= 6 stamped fields, >= 3 matched selectors, >= 20 tipped buttons,
the note and bar outside the splitter, the wheel floor. Pictures:
`scratchpad/hkxedit2_20260910/dock_frame46.png` (> 400x80) and
`viewport_gizmo.png`.

Likely first-run defects, named (rule 9): the `(i)` branch opens a second file
through `NifSkope::openFile` and waits 2.5 s -- if the load is slower the
checks read the old model; the `AnimWsRate` label text is `"60 fps"`; the
`Play` check assumes `ui->aAnimPlay` toggles synchronously; the viewport grab
is `grabFramebuffer()` on a QOpenGLWindow and may be black headless (then the
check still passes on size -- look at the picture).

## 4. When a gate fails

Measure the cause with the harness's own log line (every FAIL names its
number), and STOP -- the director decides (skill s6). Do not land a fix in
another lane's file.

## 5. The four documents

1. `WW_CHANGES.md`: splice `scratchpad/hkxedit2_20260910/WW_CHANGES_ENTRY.md`
   at the top (LF block), its "NOT measured" paragraph replaced by the gate
   table with the exe timestamp; assert the CR count unchanged.
2. `MISTAKES.md`: the lane's entries (report section 9) + anything the build found.
3. `scratchpad/lane_hkxedit2_report.md`: a `## Build (<lane>)` section, appended.
4. `HANDOFF.md` top block: the text in the report's final section; the
   follow-up (retire `src/ui/widgets/timeline.*`, CHANGE_NEEDED 3) stays owed.
