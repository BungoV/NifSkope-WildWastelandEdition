# Lane BUILD9 -- running notes (written as the work happens)

Repo `E:\Projects\NifskopeWildWastelandEdition`, `main`, nothing committed.
BUILD9 builds and gates three lanes that ended BUILD PENDING (FILESTAB, HKX3,
SKELOVERLAY), then owns the tab-strip / viewport-toolbar ALIGNMENT.

BUILD8 never wrote a `DONE` marker; its folder's newest file is 14:56:05 and
`tasklist` showed nothing running at 15:16, so it is finished-or-dead and the
build slot was taken.

## Step 1 -- FILESTAB (DONE 15:29:24)

* before grab: `scratchpad/filestab_20260910/dock_before.png` (from the OLD exe
  through `tests/spells/loaded_nifs.sh`, 15:16:33) -- tab "NIFs", root
  "Available NIFs", "Search loaded NIFs...", "Loaded NIFs / 2".
* `hookup.py --check` 15:17: 71 edits, every anchor matched as declared.
  `--apply`: 5 files written, byte and CR deltas exactly as predicted
  (`src/nifskope.cpp` CR 9379 -> 9520, +141 CRLF lines; the other four LF-only
  and unchanged at CR 0).
* syntax pass both configurations ALL-RC=0.
* qmake RC=0, make RC=0, exe 15:20:34. Dependency read-back: `src/filestab.h` is
  named by `filestab.o`, `filestabtest.o`, `nifskope.o`, `nifskope_ui.o`;
  `src/hkxplayback.h` by `nifskope.o` among nine. Exe newer than every one of
  the changed paths (0 stale), `res/style.qss` and `release/style.qss` equal.
* after grab: `scratchpad/filestab_20260910/dock_after.png` -- tab "Files", root
  "Available files", "Search loaded files...", "Loaded file / 1", and the
  archive's animation folders (behaviors, genericbehaviors, uniquebehaviors)
  now in the tree beside the loose `ww` folder.

### The gate table (second run, exe 15:25:05)

| gate | number | verdict |
|---|---|---|
| (1) | 0 strings say NIF; seeded offender found = 1, then 0 again | pass |
| (2) | .nif 1, .bto 1, .btr 1, .hkx 14939, .gltf 0, .lodl 0, .lodt 0; first .hkx `meshes/actors/_testcharacter/behaviors/_testcharacter.hkx` | pass, floor pass |
| (3) | summary 78 play / 17 named with no node / 4 by case; clip count +1; became the playing sequence | pass |
| (4) | posed: 78 of 139 nodes moved; after unload 1 of 139 differs -- `PipboyBone` | FAIL, cause measured below |
| (5) | 0 group boxes, 6 tool buttons of which 2 untipped (`QLineEditIconButton` x2), 2 placeholders, loaded list below browser (523 / 556) | FAIL, cause measured below |
| (6) | refuses in words with 0 named nodes, loads nothing | pass |
| (shot) | dock_after.png written | pass |

29 checks, 2 failures. Neighbouring harnesses: `loaded_nifs.sh` 166 checks /
3 failures = the SAME three that the before-run had (no regression from the
renames, after the six expectation literals were updated -- see below);
`hkxanim_play.sh` 27 checks / 0 failures, so HKX2's playback is untouched.

### Two instrument defects found and repaired, and two red gates left red

1. **The gate (2) floor was measuring the machine, not the code.** First run:
   `.nif 0 | .bto 0 | .btr 0 | .hkx 14939`. Cause: `tests/spells/files_tab.sh`
   built its loose fixture tree with `mktemp -d`, which returns `/tmp/tmp.XXXX`,
   and `winpath()` in `tests/spells/_harness.sh` only rewrites DRIVE-style
   `/x/...` paths -- so the Windows binary was handed the literal string
   `/tmp/tmp.XXXX/Data`, could not open it, counted it into
   `nifBrowserSkippedResources` and indexed the archive alone. FIXED in the
   driver: the fixture tree is built under `scratchpad/filestab_20260910/` and
   the script now refuses if `winpath` does not return a `X:/` path.
2. **The panel-style floor could not fire.** It blanked `tools.first()`, which
   was already untipped, so the count could not rise by one. FIXED: the victim
   is now the first button that is currently tipped. The floor passes.
3. **Gate (5) stays RED and is NOT amended.** The two untipped buttons are
   `QLineEditIconButton` instances -- the clear buttons Qt creates inside a
   `QLineEdit` with `setClearButtonEnabled(true)`, one per search field. They
   are not controls the lane put on the row. Narrowing the gate's population
   after seeing its numbers is exactly the thing CONSTITUTION 1 forbids, so the
   gate is reported red with its cause named; excluding Qt's internal buttons
   is a director/bungo decision.
4. **Gate (4) stays RED and its cause is measured, not guessed.** The node that
   does not restore is `PipboyBone`, and `NifSkope -no-gui list` on the fixture
   shows `[72] NiNode 'PipboyBone'` followed immediately by
   `[73] NiTransformController`: the fixture's OWN animation drives that node.
   The gate snapshots the bind pose at t=0, scrubs the scene to t=mid, unloads,
   and snapshots again WITHOUT stepping back to t=0 -- so a node driven by the
   NIF's own controller is legitimately still at its t=mid value. Corroborated
   twice: `hkxanim_play.sh`'s own restore gate is 27/0 on `skeleton.nif`, which
   has no such controller, and the clip's own bound-bone list never names
   `PipboyBone`. THE PRODUCT QUESTION THIS RAISES, for bungo: unloading a clip
   leaves the scene at the clip's time, so a model with its own animation stays
   wherever the transport left it.
5. **Six sibling checks went red on the renames and were repaired, not
   silenced.** `WW_LOADEDNIFS_TEST` asserts the expected TEXT of six labels
   FILESTAB renamed ("NIFs" tab, "Loaded NIFs / 2", "Loaded NIFs / 1 of 2", two
   skeleton-mark tooltips, one menu item). Only the expected literals moved
   (`scratchpad/build9_20260910/fix_loadednifs_expect.py`, 6 anchors x1, CR
   0 -> 0); no check name, assertion or widget was touched, and the failure
   count went 9 -> 3, back to the before-run baseline.

`scratchpad/filestab_20260910/PENDING.md` predicted markers for
`grep -c "lane FILESTAB"`; the applied counts are pro 0 / nifskope.cpp 6 /
nifskope.h 2 / nifskope_ui.cpp 1.

## Step 2 -- HKX3

* `hookup.py --check` after FILESTAB landed: nine anchors, each x1, unaffected
  by FILESTAB's 71 edits to the same two files. `--apply`: `NifSkope.pro`
  19044 -> 19350, `src/nifskope_ui.cpp` 1476506 -> 1478429, CR 0 -> 0 on both.
* Markers: `NifSkope.pro` 1 and `src/nifskope_ui.cpp` **6** (the PENDING
  predicted 7; six edits reach that file and each carries one marker, so 6 is
  the right number and the resume's figure was wrong).

### MISTAKE (mine): a DEFINES change does not invalidate an object

`qmake` RC=0, `make` then died at the LINK with one undefined reference,
`TimelineWidget::setGLView(GLView*)`, and DELETED `release/NifSkope.exe` on the
way out -- so for four minutes there was no exe on disk at all.

Cause: edit 3 of the hook-up adds `DEFINES += WW_HKXANIM_UI`, and every
clip-shaped line of `src/ui/widgets/timeline.cpp` is behind `#ifdef
WW_HKXANIM_UI`. `qmake` regenerated the Makefile with the new flag, but `make`
compares MTIMES, not flags: `timeline.o` was 15:19:10, newer than its source,
so it was kept -- compiled WITHOUT the define, with `setGLView` compiled out,
while the freshly built `nifskope_ui.o` called it.

What is true instead: **the object-staleness rule in
`nifskope-ww-build-verify` covers a changed HEADER but not a changed FLAG.**
A new `DEFINES` line makes every object that reads that macro stale even
though every mtime says otherwise.

How it was found: the link error named one symbol, and `ls -l` on
`GeneratedFiles/.obj/timeline.o` showed the previous build's timestamp.

The rule that prevents it: after any change to `DEFINES` / `CXXFLAGS` in the
`.pro`, delete the objects of every translation unit that reads the macro --
`grep -rln <MACRO> src/` for the sources, then every `.cpp` that includes any
header on that list -- before running make. Applied here: six objects deleted
(`timeline`, `timelineedit`, `timelineviews`, `main`, `nifskope`, `meshtools`).

## Steps 2-5: see the ledgers, not this file

From the HKX3 build onward the record lives where it belongs and is not
duplicated here:

* `WW_CHANGES.md`, one entry: "2026-09-10 -- three pending lanes built and gated,
  and the bars given one row (lane BUILD9)", carrying all three lanes' delivered
  text with a measured status block each, plus the alignment.
* `MISTAKES.md`, five new entries at the top.
* `scratchpad/lane_{filestab,hkx3,skeloverlay}_report.md`, a `## Build (BUILD9)`
  section each with the gate table and one mtime table.
* `scratchpad/build9_20260910/HANDOFF_BLOCK.md`, the text for the HANDOFF top
  block (the director splices it).

Scripts this lane wrote, all re-runnable: `fix_loadednifs_expect.py`,
`splice_wwchanges.py`, `splice_mistakes.py`, `splice_mistakes2.py`,
`append_reports.py`. Logs: `qmake.log`, `build_dump.log`, `build_fix.log`,
`build_fix2.log`.

## Finished-work skill review (CONSTITUTION 1a)

**Loaded and used in earnest:** `nifskope-ww-resume-pending` (the read order,
qmake-before-make, the per-object dependency read-back by awk, the
whole-working-set staleness sweep, the four documents, and rule 6 -- measure the
cause and STOP, which is what kept two red gates red instead of cured);
`nifskope-ww-build-verify` (the gated chain, the process guard immediately
before the link, `sx_$LANE.sh` rather than the colliding `sx_tmp.sh`, and the
"a successful build is not a consistent one" section -- which did NOT cover the
flag case and has now been amended); `ww-anchored-hookup` (three refusing
scripts run as `--check` then `--apply`, counts printed, markers rather than
anchors as the applied test; and I wrote a fourth in the same shape for the
sibling-harness literals); `nifskope-ww-panel-style` (the alignment fix is its
subject: shared helpers, never per-widget, counted with a floor).

**Written, because it was owed for the third time:** `ww-test-harness-add`, in
BOTH trees. Three lanes had reconstructed the WW_*_TEST shape by reading
`src/hkxplaybacktest.cpp` and a spell end to end. It now carries the arming and
timing shape and WHY the 1.5 s exists, the own-translation-unit rule, reading
WIDGETS instead of private members (both `NifSkope::ogl` and
`setLeftColumnMode` were hit, in two different lanes), the dock that defers work
while hidden, the two floors that could not fire, the `winpath`/`mktemp` trap
that made a census measure the machine, the same-scene-time rule for snapshot
comparisons, the UTF-16 exe grep, and what a harness can never test.

**Amended, in both trees:** `nifskope-ww-build-verify` (a changed FLAG makes an
object stale and no mtime says so), `nifskope-ww-resume-pending` (sections 9 and
10: a PENDING's numbers are predictions; the siblings assert the old strings),
`nifskope-ww-panel-style` (bars that meet along one line share one row -- the
helper, grow-never-shrink, restyle the tabs to fill, call it after restoreState,
and the geometry gate with its floors).

**Declined, with the reason:** `nifskope-ww-render-shot` -- I ran
`skeleton_overlay.sh`, which uses the hook, but I wrote none of it; the pictures
I authored are dock and seam grabs, which the skill itself says are a different
instrument. `nifskope-ww-commit` -- nothing is committed and his "Not yet"
stands. `ww-hkx-animation` -- read for the 78/17/4 figures, not exercised: this
lane built and measured animation code, it did not derive anything about the
format.
