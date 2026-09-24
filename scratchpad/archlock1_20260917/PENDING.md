> **SUPERSEDED 2026-09-17 04:0x.** The game went away at 03:13:31, the build
> ran (03:19:59, and three more while the harness's ending was fixed), the
> rung was built at 03:56:37 and `tests/spells/gamemanager_archlock.sh`
> passed 20/0 at 04:00:30. Steps 0-3 below are DONE; the numbers they asked
> for are in sections 0, 3d, 3e, 6 and 7 of `lane_archlock1_report.md`, which
> is the file to read. Kept only as the record of where the lane stopped.

# Lane ARCHLOCK1 -- resume file (written 2026-09-17 03:1x from `date`, before the build)

## Where the lane is

The FIX was already in the tree when the lane started (the director's two-line
hotfix at 01:02, carried by the 01:17 exe). This lane's work was the AUDIT, the
rule at the lock's declaration, the REFUTER, the `--bake-record` refusal clause,
and BAKEREC1's pending gates. All of that is done and on disk. **The build has
not run.**

**Why it stopped:** `tests/spells/lodgen_btofree.sh` refused itself at 03:10 with
`REFUSED: Fallout4.exe is running`, and a direct `tasklist` at 03:11:26 confirmed
it (`Fallout4.exe 38248, 5,411,800 K`). The game was down at launch (02:19) and
through every gate before that one. CONSTITUTION rule 6: no build, and no exe
launch, while the game is up.

Exe at launch, untouched: `release/NifSkope.exe` 2026-09-17 01:17:01,
22,459,904 B, sha1 `5ddc0a7a78b28539cc5bb6013319a691b07d6f5e`. It was copied to
`release/NifSkope.at_0117.exe` before anything, so the gate numbers in the
report can be reproduced after the relink.

## What changed on disk (nothing committed; `WW_CHANGES.md` and `HANDOFF.md` untouched)

| file | what |
|---|---|
| `src/gamemanager.h:104-125` | THE RULE at `archiveLock()`'s declaration: never recurse to `parent` under the read lock, with the mechanism and bungo's words. Comment only. |
| `src/gamemanager.cpp` | **not edited.** The two `archiveReadLock.unlock()` sites (`:296`, `:323`) are the director's hotfix, kept and audited. |
| `src/lodbfile.cpp:417-428` | the `--bake-record` refusal now names the directory: `no bake record at %1 (looked in %2)`. |
| `src/nifskope.h:566-571` | declares `wwFilesTabOpenConfiguredRow( int game, const QString & virtualPath )`. |
| `src/nifskope.cpp:3172-3200` | that seam: a CONFIGURED-RESOURCE row opened through the view's own `doubleClicked`. **CRLF file, spliced in binary**; CR 9639 / LF 10787 after. |
| `src/archlocktest.cpp` | NEW, 319 lines. `WW_ARCHLOCK_TEST`, the in-application refuter. |
| `src/nifskope_ui.cpp:8245-8252` | the one dispatch line that calls it. |
| `src/nifcli.cpp` | NEW verb `archlock-probe <file.nif> --data-root <a;b;c> [--probe <tex>]`, plus usage. |
| `NifSkope.pro:363` | `src/archlocktest.cpp` added. LF-only, CR 0 verified. |
| `tests/spells/gamemanager_archlock.sh` | NEW, 365 lines. Legs (a)-(e). |
| `tools/archlock1_build_rung.sh` | NEW. Builds the refuter exe (see step 2). |
| `docs/MISTAKES.md`, `MISTAKES.md` | one entry and three items, already written. |

Every changed translation unit passes `-fsyntax-only` with the real flags out of
`Makefile.Release` (lane report section 3c). One error was caught that way and
fixed: `GLView` is a `QOpenGLWindow` and has no `repaint()`.

## The steps this lane could not run, in order, with the numbers each must produce

0. `tasklist | grep -i -E "Fallout4|NifSkope"` -- must be EMPTY. Then
   `touch scratchpad/archlock1_20260917/BUILDING`.

1. **`qmake` BEFORE `make`.** `src/archlocktest.cpp` is a NEW translation unit
   and `src/nifskope.h` grew a declaration; the frozen dependency list names
   neither.
   ```bash
   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake -o Makefile NifSkope.pro > /tmp/qmake.log 2>&1; echo QMAKE-RC=$?'
   bash tools/ww_build.sh src/archlocktest.cpp src/nifcli.cpp src/nifskope.cpp src/nifskope_ui.cpp src/gamemanager.h src/lodbfile.cpp
   ```
   Gate on `BUILD-RC=0`, not on the exe's timestamp. Then confirm
   `GeneratedFiles/.obj/archlocktest.o` exists and `release/NifSkope.exe` is
   newer than every file in the table above.

2. **Build the rung.** `bash tools/archlock1_build_rung.sh`. It removes the two
   `archiveReadLock.unlock();` lines (regex, asserts exactly 2), builds, copies
   `release/NifSkope.exe` to `release/NifSkope.archlock1_rung.exe`, restores the
   file under a `trap` and verifies the restore by SHA-1, then builds again.
   **It must print `restored: <sha1> (byte-identical)`.** If it does not, the
   backup path it prints is the recovery.
   The rung has to be built rather than taken off the shelf: the harness, the
   seam and the verb are all new in this lane, so no older exe can run either
   refuter leg. Report section 3b says why at length.

3. **The refuter.**
   ```bash
   SHOT=scratchpad/archlock1_20260917/images/archlock_viewport.png \
   WSHOT=scratchpad/archlock1_20260917/images/archlock_window.png \
   OUT=scratchpad/archlock1_20260917/gates_new/archlock_work \
     bash tests/spells/gamemanager_archlock.sh 2>&1 | tee scratchpad/archlock1_20260917/gates_new/gamemanager_archlock.log
   ```
   Expected: leg (a) the RUNG is killed at the watchdog limit (90 s) and its
   `release/ww_archlock_test.log` ends at the line beginning `opening `; the new
   exe writes `PASS` and exits 0. Leg (b) the same with no window. Legs (c)/(e)
   are counts. Leg (d) bakes twice and takes minutes -- `LEGS=abce` skips it if
   time is short, but then say so. RECORD the seconds the rung hung.

4. **The gates on the new exe**, the same four as section 3a plus the two that
   never ran on the 01:17 exe:
   `lodgen_bakerec.sh`, `lodgen_layout.sh`, `lodgen_defaults.sh`,
   `lodgen_native.sh`, `lodgen_btofree.sh`, `lod_generation.sh` (floor 121).
   Table them beside the 01:17 numbers, which are already in the report.

5. **Pictures** into `scratchpad/archlock1_20260917/images/`: the two grabs from
   step 3, plus the terminal capture of the rung hanging and being killed --
   render it from the gate log with the same script that made BAKEREC1's picture
   (`scratchpad/make_picture.py` in the session scratchpad; PIL 10.0.1).

6. Fill in report sections 0, 3 and 6, delete `BUILDING`, write `DONE` whose
   first word is `archlock`.

## What is RED and whose it is

Four gates on the 01:17 exe fail, and every failure is the same file:
BAKEREC1's `Commonwealth.lodb` bake record, which carries fields that move
between two runs of the same tree.

* `lodgen_bakerec.sh` 21/3 -- leg (e) the `--native-verify` refusal does not
  name the plugin or a direction word; leg (h) a SECOND nondeterministic census
  field, `peak working set`, beside the masked `stage times:`.
* `lodgen_layout.sh` 22/5 -- leg (c) x4, the only differing file in the stock
  tree is that record; leg (f) `census 79, on disk 80`, the extra file is it.
* `lodgen_native.sh` 2 FAIL rows -- section 5 wants the two records to differ
  only in the command-line digest; they differ in more.
* `lodgen_btofree.sh` and `lod_generation.sh` -- never ran (the game came up).

None of it is reachable from anything this lane touched, and none of it is
fixed here: BAKEREC1 owns the record.
