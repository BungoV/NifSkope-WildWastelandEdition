# Lane FILESTAB — BUILD PENDING resume

Paste-able. Repo `E:\Projects\NifskopeWildWastelandEdition`, `main`, nothing
committed. Read `scratchpad/lane_filestab_report.md` first (§1 is the inventory,
§4 the .hkx route, §5 the gates and what each floor refutes).

**Why it is pending.** Lane BUILD8 owned `src/nifskope.cpp`,
`src/nifskope_ui.cpp`, `NifSkope.pro` and `src/gltfexport.*` for the whole of
this lane, and it was still building when this lane finished. Nothing was
applied, nothing was built, no exe was launched, no gate was run and no picture
was taken.

**What is on disk, unapplied:**

| file | state |
|---|---|
| `src/filestab.h` / `src/filestab.cpp` | NEW. `g++ -fsyntax-only` with the real `Makefile.Release` flags: **RC=0**, zero new warnings |
| `src/filestabtest.cpp` | NEW, the `WW_FILESTAB_TEST` harness. **RC=0** |
| `tests/spells/files_tab.sh` | NEW, the driver |
| `scratchpad/filestab_20260910/hookup.py` | the refusing patch: **71 edits, every anchor matched as declared, nothing written** |
| `scratchpad/filestab_20260910/syntax.sh` | the pass above, re-runnable |

`hookup.py --check`, **re-run at 14:59:09 against the tree as BUILD8 left it**
(BUILD8 had touched `NifSkope.pro` at 14:23:49 and relinked
`release/NifSkope.exe` at 14:37:53; `src/nifskope.cpp` was still 02:25:22 and
`src/nifskope_ui.cpp` 05:36:21), verbatim numbers:

```
NifSkope.pro           bytes 18982   -> 19044     CR 0    -> 0     (predicted +0)   ok
src/nifskope.cpp       bytes 434370  -> 440714    CR 9379 -> 9520  (predicted +141) ok
src/nifskope.h         bytes 47774   -> 48509     CR 0    -> 0     (predicted +0)   ok
src/nifskope_ui.cpp    bytes 1476286 -> 1476500   CR 0    -> 0     (predicted +0)   ok
src/ui/nifskope.ui     bytes 59945   -> 59947     CR 0    -> 0     (predicted +0)   ok
--check: every anchor matched as declared. Nothing written.
```

`src/nifskope.cpp` is the mixed file: every inserted line takes the ending of
the line it is anchored to, read out of the file's real bytes, and the run
asserts the CR delta equals the number of CRLF lines it inserted (+141). The
other four are LF-only and stay at CR 0.

---

## THE RESUME, in order

**0. The BEFORE picture, taken BEFORE anything is applied.** The current exe
already grabs this exact dock, inside `WW_LOADEDNIFS_TEST`:

```bash
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     # must be 1
cd /e/Projects/NifskopeWildWastelandEdition
bash tests/spells/loaded_nifs.sh 2>&1 | tail -5
cp release/ww_nifbrowser_test.png scratchpad/filestab_20260910/dock_before.png
```

If this step is skipped there will never be a before picture: the harness that
takes the after one does not exist in the old exe. (The two grabs are the same
dock but not the same fixture — `loaded_nifs.sh` opens its own cube tree — so
the pair shows the RENAMES, not a pixel diff. Say so when reporting it.)

**1. Apply the hook-up.**

```bash
python scratchpad/filestab_20260910/hookup.py            # --check again first
python scratchpad/filestab_20260910/hookup.py --apply
```

If an anchor now matches 0 or 2 times, BUILD8 has moved that line: re-read the
line in the file, widen the anchor, re-run `--check`. Never loosen a count.

**2. The syntax pass, now in BOTH configurations** (the second one becomes a
real configuration only after step 1, and the script detects that itself):

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
 'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/filestab_20260910/syntax.sh'
# expect: ALL-RC=0, four RC=0 lines
```

**3. qmake BEFORE make.** `NifSkope.pro` gained two `SOURCES` lines and one
`HEADERS` line, and `src/nifskope.cpp` gained `#include "filestab.h"` and
`#include "hkxplayback.h"` — a NEW cross include, which is exactly the case
qmake's frozen dependency lists miss (`nifskope-ww-resume-pending` §3):

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/filestab_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/filestab_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/filestab_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

then READ THE DEPENDENCY BACK, by object, not by grep -A:

```bash
grep -n "filestab\.h" Makefile.Release | cut -c1-160
# and for each of those line numbers N:
awk -v s=N 'NR<=s && /^GeneratedFiles\/\.obj\/[a-z_]*\.o:/ {last=$0} NR==s {print last}' Makefile.Release
# nifskope.o and filestabtest.o must both name src/filestab.h
```

**4. The exe is newer than EVERY changed file, not just one:**

```bash
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue; [ "$EXE" -nt "$f" ] || echo "STALE vs $f"; done
cmp res/style.qss release/style.qss && echo "sheet in step"
```

**5. The gates, sequentially — one NifSkope instance at a time.**

```bash
SHOT="$PWD/scratchpad/filestab_20260910/dock_after.png" \
  timeout 400 bash tests/spells/files_tab.sh 2>&1 | tail -40
```

Pre-registered, and what each one refutes, is `lane_filestab_report.md` §5. The
headline numbers to read back: gate (1) **0 strings**, with the seeded-offender
floor finding exactly **1**; gate (2) `.hkx > 0` and `.btr > 0` with `.nif > 0`
beneath them; gate (3) the summary sentence carrying **78 / 17 / 4**; gate (4)
**0 of N nodes differ** after unload with **> 0** while it played; gate (5) `0`
untooltipped tool buttons with the blanked-tooltip floor counting `1`; gate (6)
the refusal sentence.

**6. The harnesses this change also reaches** (run them, they touch the same
two files):

```bash
timeout 400 bash tests/spells/loaded_nifs.sh 2>&1 | tail -6     # the same dock
timeout 400 bash tests/spells/hkxanim_play.sh 2>&1 | tail -6    # the same HkxPlayback
```

`hkxanim_play.sh` needs ABSOLUTE Windows paths for `SRC` and `CLIP`
(`ww-hkx-animation` §12) or it reports 0 bones matched and looks like a
regression in this lane.

**7. The four documents that must stop saying "not built"**
(`nifskope-ww-resume-pending` §7): splice
`scratchpad/filestab_20260910/WW_CHANGES_ENTRY.md` into `WW_CHANGES.md` (mixed
file — assert the CR count is unchanged, never `sed -i`), root `MISTAKES.md`,
this lane's report (append a `## Build (<lane>)` section), and any skill the
build disproves.

## What is NOT covered and stays owed

* **The drop route.** HKX3's ruling ("maybe I could just drag the hkx into
  nifskope") is a different entry point — `NifSkope::openFile()` and the
  external-drop menu — and this lane did not touch it. An .hkx dropped on the
  window still goes down the document path.
* **`.lodl` / `.lodt` in the tree.** They are in the extension predicate but the
  tree keeps only paths under `meshes/`, and nothing ships them there. Reachable
  only if a user puts one under `meshes/`. Stated, not fixed.
* **The archive-staged clip loses the skeleton-beside-it search arm.** A clip
  opened out of a `.ba2` is written to a temporary file, so `resolveNames()`
  falls through to the session's skeletons and then to the game archives, and
  names which arm served. Not a defect, but it is the reason an archive clip can
  report a different skeleton source from the same clip on disk.
