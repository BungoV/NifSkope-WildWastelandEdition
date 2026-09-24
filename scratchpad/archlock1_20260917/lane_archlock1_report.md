# Lane ARCHLOCK1 -- the archive-index lock that deadlocked the GUI thread on itself

Written incrementally. Launched 02:19 2026-09-17. Tree
`E:/Projects/NifskopeWildWastelandEdition`, branch main, sole lane.

## 0. The exes: at launch, the build, the rung

| what | bytes | mtime | sha1 |
|---|---|---|---|
| `release/NifSkope.exe` at launch (BAKEREC1's 01:17 build, carries the two-line hotfix) | 22,459,904 | 2026-09-17 01:17:01 | `5ddc0a7a78b28539cc5bb6013319a691b07d6f5e` |
| `release/NifSkope.at_0117.exe` (that same exe, kept so section 3a can be reproduced) | 22,459,904 | 2026-09-17 01:17:01 | `5ddc0a7a78b28539cc5bb6013319a691b07d6f5e` |
| **`release/NifSkope.exe` -- THIS LANE'S BUILD** | **22,477,824** | **2026-09-17 03:56:42** | `304d6bdc6d6070a508bdfc3782c96a22011a4a0a` |
| `release/NifSkope.archlock1_rung.exe` (THE REFUTER: this tree minus the two unlock lines) | 22,477,824 | 2026-09-17 03:56:37 | `b9b5e8d36d0308b7412fb673985663d051f78827` |
| `release/NifSkope.before_archlock1.exe` (01:00, no hotfix; leg (d)'s `$PREV`) | 22,459,904 | 2026-09-17 01:00:05 | `7460bf768d5d6b9df009d47d3024f431de5f6f4a` |

The 01:17 exe and the 01:00 exe differ only by the hotfix the director applied at
01:02. Neither is overwritten. The rung is built by
`tools/archlock1_build_rung.sh` from THIS tree (section 3b says why), and that
script prints `restored: <sha1> (byte-identical)` before it rebuilds the fixed
exe -- it did, both times it ran (03:39 and 03:56).

Game check at 02:21 and before every launch since: `tasklist | grep -i -E
"Fallout4|NifSkope"` -> rc=1, nothing running.

## 1. The audit: every site that recurses to `parent` or reaches a write-taking
   function, with the verdict

The rule is now written at the lock's declaration,
`src/gamemanager.h:104-125`: **never recurse to `parent` under the read lock;
the parent's lazy init takes the write lock.** `QReadWriteLock::Recursive`
grants read-after-read and write-after-write to the same thread and NEVER a
read -> write upgrade.

| # | site | what it reaches | read lock held? | verdict |
|---|---|---|---|---|
| 1 | `src/gamemanager.cpp:287-298` `find_file` | `parent->find_file()` | **was held, now released** (`archiveReadLock.unlock()` at :296) | FIXED (director's hotfix, kept) |
| 2 | `src/gamemanager.cpp:316-326` `get_file` | `parent->get_file()` | **was held, now released** (`archiveReadLock.unlock()` at :323) | FIXED (director's hotfix, kept) |
| 3 | `src/gamemanager.cpp:284-285` `find_file` prologue | own `init_archives()` (WRITE) | not held -- the `QReadLocker` is constructed on the NEXT line | safe |
| 4 | `src/gamemanager.cpp:311-312` `get_file` prologue | own `init_archives()` (WRITE) | not held -- same shape | safe |
| 5 | `src/gamemanager.cpp:334-337` `get_file` retry (loose file resized) | `close_archives()` (WRITE), then recurses into itself | not held -- `archiveReadLock.unlock()` first, and it was already there before this lane | safe |
| 6 | `src/gamemanager.cpp:176-185` `init_archives` | `close_materials()` (WRITE), `parent->init_archives()` (WRITE) | WRITE held | safe: write-after-write by one thread is what Recursive grants |
| 7 | `src/gamemanager.cpp:257-259` `close_archives` | `close_materials()` (WRITE) | WRITE held | safe, same reason |
| 8 | `src/gamemanager.cpp:268-274` `close_materials` | every child's `close_materials()` (WRITE) | WRITE held | safe, same reason |
| 9 | `src/gamemanager.cpp:219-247` `init_materials` | `parent->init_materials()`, own `init_archives()` (WRITE), `close_materials()` (WRITE) | **no lock of its own at all** | not held -- cannot upgrade. Starfield-only (`game != STARFIELD` returns immediately). |
| 10 | `src/gamemanager.cpp:365-370` `list_files` | `parent->list_files()`, own `init_archives()` (WRITE) | **no lock of its own at all** | not held -- cannot upgrade. See the finding below. |
| 11 | `src/gl/glproperty.cpp:1127-1135` the material retry | `r.close_archives()` and `r.parent->close_archives()` (WRITE) | not held -- this is outside `get_file`, in the catch block | safe from THIS bug. See the finding below. |
| 12 | `src/gamemanager.cpp:684-694` `close_resources` | `close_materials()` / `close_archives()` (WRITE) | not held | safe |

### Lock ORDER, checked as well (a second way to deadlock the same pair)

`close_materials` takes `archiveLock()` (write) and then `nifResourceMutex()`
(`src/gamemanager.cpp:268-269`). For that to deadlock, some path would have to
take them the other way round. There is none: `addNIFResourcePath` and
`removeNIFResourcePath` are the only other holders of `nifResourceMutex`, and
neither reaches anything that takes `archiveLock` -- `removeNIFResourcePath`'s
`delete r` runs `~GameResources()`, which takes no lock at all. One order only,
so no ABBA pair. Recorded because it is the next thing that would bite here.

### Two findings NOT fixed by this lane (they are not deadlocks, and they are
    not mine to change without a ruling)

- `list_files` (#10) takes **no lock at all** while it reads `ba2File` and calls
  `ba2File->scanFileList()`. Under NIFPARSE1's own reasoning -- the index may be
  torn down by `close_archives()` from another thread at any moment -- that read
  is unguarded. It cannot deadlock, and it is pre-existing (NIFPARSE1,
  2026-09-11). Making it take the read lock is a one-line change with a real
  risk attached: it would then be a lock held across a `parent->list_files()`
  recursion, i.e. exactly the shape that caused this bug, so it needs the same
  unlock-first treatment if it is ever added. Left alone, named here.
- `glproperty.cpp:1127-1132` (#11) reads `r.ba2File` and `r.parent->ba2File` and
  calls `findFile` on them with no lock, in the catch block, before
  `close_archives()`. Same class: a race, not a deadlock, pre-existing.

## 2. What changed (file:line)

| file:line | change |
|---|---|
| `src/gamemanager.h:104-125` | the rule, at the lock's declaration: never recurse to `parent` under the read lock, with the mechanism and bungo's words |
| `src/gamemanager.cpp:291-297`, `:322-324` | the director's two `archiveReadLock.unlock()` sites, KEPT and audited (not re-written) |
| `src/lodbfile.cpp:417-428` | the `--bake-record` refusal now names the directory it looked in: `no bake record at %1 (looked in %2)` |
| `src/nifskope.h:566-571` | declaration of the new seam |
| `src/nifskope.cpp:3172-3200` | `NifSkope::wwFilesTabOpenConfiguredRow()` -- a CONFIGURED-RESOURCE row, opened through the view's own `doubleClicked`, which is bungo's route (CRLF, spliced in binary) |
| `src/archlocktest.cpp` (new, 280 lines) | `WW_ARCHLOCK_TEST`, the in-application refuter |
| `src/nifskope_ui.cpp:8245-8252` | the one line that calls it |
| `src/nifcli.cpp` (+116 lines) | `archlock-probe <file.nif> --data-root <..> [--probe <tex>]`, the same refuter with no window; usage lines |
| `NifSkope.pro:363` | the new translation unit |
| `tests/spells/gamemanager_archlock.sh` (new) | the gate, legs (a)-(e) |
| `src/archlocktest.cpp:128-165` | **how the run ENDS.** `qApp->quit()` does not end this one (section 3d): the window is closed and the loop is ended 200 ms later, through the loop. |
| `src/archlocktest.cpp:236-241` | `ogl->center()` before the grab -- otherwise the picture is of an empty grid with a two-pixel sliver of arm in it |
| `tests/spells/gamemanager_archlock.sh:99-105` | SHOT/WSHOT are made ABSOLUTE; the exe's own working directory is `release/`, and a relative one made the grab report `REFUSED` |
| `tests/spells/gamemanager_archlock.sh:154-186` | the watchdog is `timeout`, not a background job plus `wait` (a reaping race answered 127 for a run that exited 0), and a 127 -- Windows refusing to start the image right after the previous one exited -- is retried once |
| `tests/spells/gamemanager_archlock.sh:88-96, 300-311` | leg (d) bakes with `--worldspace 3C --terrain-region $REGION --dim 4 --data-root ...`; the first draft said `--region`, which is not a flag, and both bakes exited 2 having written nothing |

## 3. Gates (both runs)

The lane wrote this file as it went, so the subsections are in the order they
were measured rather than in reading order:

- **3a** the neighbouring gates on BAKEREC1's 01:17 exe, before any build of mine
- **3b** why the rung had to be BUILT from this tree
- **3c** the syntax gate before the build, and what it caught
- **3d** `gamemanager_archlock.sh` on MY build: the refuter, and what the rung did
- **3e** the three defects in my own gate that would have reported something untrue
- **3f** every neighbouring gate, both runs, side by side
### 3a. Gates on BAKEREC1's 01:17 exe, BEFORE any build of mine

Run from `E:/Projects/NifskopeWildWastelandEdition`, logs under
`scratchpad/archlock1_20260917/gates_0117/`. Game check before the run: nothing
running.

| gate | checks | failures | wall | verdict |
|---|---|---|---|---|
| `lodgen_bakerec.sh` (legs a-h, BAKEREC1's PENDING gates) | 21 | 3 | 02:24-02:33, 9 min | **FAIL -- BAKEREC1's, not mine** |

## 5. Changelog text for the director to splice

### 5a. `WW_CHANGES.md` -- new entry at the top

```markdown
## The Files tab stopped freezing the window (2026-09-17, lane ARCHLOCK1)

Double-clicking a file in the **Files** tab could hang NifSkope solid: the window
stayed painted, the CPU sat at zero, and nothing short of ending the process got
it back. bungo: "When I click on anything from 'files', it freezes nifskope".

**What it was.** The archive index is guarded by one lock shared by every
resource set. Looking a file up took the SHARED (read) side of it, and when the
file was not in that set the lookup handed the question to the parent set --
*without letting go of the lock first*. If the parent had not built its index
yet, building it needs the EXCLUSIVE (write) side, and the exclusive side waits
for every reader to leave. The only reader was the thread doing the waiting, so
it waited for itself, forever.

It took a file whose bytes come from an archive rather than from disk, opened in
a window that had not touched that game's archives yet -- exactly what the Files
tab does on the first double-click of a session, and nothing else in the program
does.

**The fix.** The lookup releases the read lock before it asks the parent. Two
lines, in `GameResources::get_file` and `::find_file`. Nothing else changed, and
no lookup answers differently.

**The rule is now written where the lock is declared** (`src/gamemanager.h`),
because this lock is recursive and recursive locks look safer than they are: they
allow read-after-read and write-after-write on one thread, and never a read that
becomes a write. Every other path that recurses to the parent or reaches the
index builder was audited against it; twelve sites, listed in the lane report.

**New gate.** `tests/spells/gamemanager_archlock.sh` opens a file through the
Files tab's configured-resource row in a fresh process with the shared index
deliberately torn down, and checks both halves of that state before it clicks.
The same test is run against the previous build, where the passing result is the
watchdog killing it. There is also `NifSkope -no-gui archlock-probe <file.nif>
--data-root <Data>` -- the same case with no window, one fact a line.

**One refusal now says where it looked.** `lodgen --bake-record` on a missing
file prints the directory it searched, because in batch mode NifSkope resolves a
relative path against its own folder and the old message left you guessing.
```

### 5b. `HANDOFF.md` -- LANDED block

```markdown
### LANDED 2026-09-17 04:47 -- ARCHLOCK1: the archive lock no longer deadlocks the GUI thread

The Files tab freeze bungo reported is fixed and gated. `GameResources::get_file`
and `::find_file` release the archive read lock before recursing to `parent`;
the parent's lazy `init_archives()` takes the write lock, and
`QReadWriteLock::Recursive` never upgrades a read to a write, so the GUI thread
was waiting for its own reader. The two-line hotfix applied at 01:02 is KEPT as
it stands -- this lane audited it rather than rewriting it.

- audit: twelve sites that recurse to `parent` or reach `init_archives()` /
  `close_archives()`, each with a held/not-held verdict; two unguarded READS
  (`list_files`, `glproperty.cpp:1127`) named but NOT changed -- they are races,
  not deadlocks, and pre-existing. Lock order checked: single direction, no ABBA.
- the rule is written at the lock's declaration, `src/gamemanager.h:104-125`.
- gate: `tests/spells/gamemanager_archlock.sh` (GUI refuter through the Files
  tab's configured-resource row, CLI refuter `-no-gui archlock-probe`, a
  `find_file` leg, and the neighbours). The refuter's PASS on the previous exe is
  a watchdog kill.
- `lodgen --bake-record`'s refusal now names the directory it looked in.
- exe: `release/NifSkope.exe` 2026-09-17 03:56:42, 22,477,824 B, sha1
  `304d6bdc6d6070a508bdfc3782c96a22011a4a0a`. The refuter exe is kept beside it
  as `release/NifSkope.archlock1_rung.exe`.
- numbers: `gamemanager_archlock.sh` 20/0 -- the rung HANGS and is killed at 90 s
  (GUI) and 91 s (no window), the fixed exe opens the same row and exits 0 in 5 s.
  `lod_generation.sh` 128/0 (floor 121), `lodgen_defaults.sh` 28/0.
- RED, and not this lane's, all four the same file (`Commonwealth.lodb`, which
  records a wall clock and a peak working set): `lodgen_bakerec.sh` 21/3 (legs e
  and h), `lodgen_layout.sh` 22/5, `lodgen_native.sh` section 5, and
  `lodgen_btofree.sh` 21/1. Identical counts before and after this build.
- KNOWN, not fixed: about one run in ten, the new GUI harness exits 139 AFTER
  writing its PASS -- the Qt teardown of the configured-resource route (it leaves
  a second, invisible window) is not reliable. Every measurement is complete
  before it happens. It is a shutdown defect of that route, not of the lock.
```
| `lodgen_layout.sh` | 22 | 5 | 905 s | **FAIL -- BAKEREC1's, not mine** |

#### Why two of the layout failures are the same thing

All five `lodgen_layout.sh` failures name one file. Leg (c) compares the whole
stock-mode tree against the pinned rung's, four times (dim 4/8/16/32), and each
time the ONLY file that differs is `Commonwealth.lodb` -- BAKEREC1's new bake
record, which carries a wall clock (`baked`, `census stage times:`) and the peak
working set. Leg (f) then counts the tree: `census 79, on disk 80`. The extra
file on disk is that same record, which the census does not count itself in.
Both are consequences of one landing, both are BAKEREC1's, and neither is
reachable from anything this lane touched. `lodgen_bakerec.sh`'s own legs (e) and
(h) are the third and fourth faces of it (leg (h) is literally "the record leaks
a second nondeterministic field").

### 3b. The rung had to be BUILT, not taken off the shelf

The brief names `release/NifSkope.before_archlock1.exe` (the 01:00 build) as the
refuter's rung. It cannot be: the harness (`src/archlocktest.cpp`), the seam it
drives (`NifSkope::wwFilesTabOpenConfiguredRow`) and the batch verb
(`-no-gui archlock-probe`) are all new in this lane, so that exe has no way to
run either leg. "The old exe did not hang" would then mean only that it never
reached the lock -- the exact shape of a gate that passes on a broken build.

So the rung is `release/NifSkope.archlock1_rung.exe`: THIS tree with the two
`archiveReadLock.unlock();` lines removed from `src/gamemanager.cpp` and nothing
else changed. `tools/archlock1_build_rung.sh` (new) does the surgery, builds,
copies the exe aside, puts the two lines back under a `trap` so a failed build
cannot leave them out, verifies the restored file is byte-identical to the
original by SHA-1, and builds again. One variable, one difference.

The 01:00 exe is still used, as `$PREV`, for leg (d): that leg compares BYTES
ACROSS BUILDS, and comparing the new exe against a rung that differs from it in
one file would prove much less.

### 3c. A syntax gate before the build (and what it caught)

The gates hold the exe for half an hour at a time, so every changed translation
unit was compiled `-fsyntax-only` first, with the flags read straight out of
`Makefile.Release`:

```bash
INC=$(grep -m1 "^INCPATH" Makefile.Release | sed "s/^INCPATH *= *//")
g++ -fsyntax-only -std=gnu++2a -fexceptions -mthreads -Wa,-mbig-obj \
    -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external \
    -Ilib/libfo76utils/src <DEFINES> $INC src/<file>.cpp
```

It caught one real error, and a wrong assumption behind it:
`src/archlocktest.cpp` called `ogl->repaint()`. `GLView` is a `QOpenGLWindow`
(`src/glview.h:59`), not a widget -- no `repaint()` -- and `grabFramebuffer()`
reads the CURRENT buffer without painting a new one, so the obvious fallback
would have photographed the frame from BEFORE the open and called it a pass.
The harness now uses the fork's own pattern (`ogl->update()` then two bounded
`processEvents`), which still paints on the calling thread, which is the only
property the refuter needs. Recorded in root `MISTAKES.md` as item 3.

Clean afterwards: `archlocktest.cpp`, `nifcli.cpp`, `nifskope.cpp`,
`nifskope_ui.cpp`, `lodbfile.cpp` -- no errors.

### 3d. The gate on MY build: `tests/spells/gamemanager_archlock.sh`

Run at **03:56:52-04:00:30**, on `release/NifSkope.exe`
(03:56:42) against `release/NifSkope.archlock1_rung.exe` (03:56:37), both built
minutes apart from the same tree with ONE difference: the two
`archiveReadLock.unlock()` lines. Log:
`scratchpad/archlock1_20260917/gates_new/gamemanager_archlock.log`.

**20 check(s), 0 failure(s) -- RESULT PASS.**

| leg | the rung (no fix) | this build |
|---|---|---|
| (a) GUI, the Files tab's configured-resource row | **HUNG. Killed by the watchdog after 90 s**, its log stopped dead on the line `opening meshes/armor/combatarmor/m_arm_heavy_l.nif as a configured resource` | opened it, painted one frame, **exited 0 in 5 s**; 12 checks, 0 failures inside the harness |
| (b) CLI, `-no-gui archlock-probe` | **HUNG. Killed after 91 s**, having printed `blocks 21`, `documentDataPaths 0`, `fallsThroughToShared 1`, `material ...CombatArmor_Arm.BGSM` and then nothing -- it died INSIDE `get_file` | answered and exited 0 in under 1 s |
| (c) `find_file` on a texture | -- | resolves `Armor/CombatArmor/CombatArmor_Arm_d.dds` to `textures/armor/combatarmor/combatarmor_arm_d.dds` in both the CLI and the open document; a "fix" that merely missed would fail here |
| (d) the lock is KEPT (NIFPARSE1's case) | -- | the fixture region (`--worldspace 3C --terrain-region -20 24 -17 27 --dim 4`) baked by `$PREV` and by this build: **3 identical, 0 differ**, 4 files each, the `.lodb` record excluded by name |
| (e) `--bake-record` names the directory | -- | refused, says `looked in`, names the NifSkope folder for a relative path and the file's OWN folder for an absolute one |

The floors matter as much as the rows: inside the harness the document's own data
paths read back **empty**, it reads back as falling through to the shared index,
and the shared index reads back **unbuilt** before the open and **built** after
it. Without those four the leg could pass on a run that never met the condition.

### 3e. What leg (a) had to survive before it could mean anything

Three defects in my own gate, each of which would have reported something untrue.
All three are in the root `MISTAKES.md` as item 4 and in section 2's table.

1. **The harness did not exit.** It ended the way about forty other WW harnesses
   do -- `QTimer::singleShot( 0, qApp, &QApplication::quit )` -- and on this route
   that is not an exit. Qt 6.11's quit is a request to close every top-level
   window first, and the configured-resource open leaves a SECOND, invisible
   NifSkope window beside the loaded one; with it standing the process stayed in
   `exec()`. Log `done` at 3 s, still alive 45 s later, killed by the watchdog:
   **the first run of the new harness passed all twelve of its own checks and
   returned rc 124, which is the REFUTER's ending.** `gdb -p` showed thread 1
   idle in the main event loop rather than blocked on anything, and a `WM_CLOSE`
   posted from outside exited it at once. No in-tree gate had ever noticed,
   because they all wait for `^done$` in the log and then `kill` the process
   (`tests/spells/files_tab.sh:107-119`). Fixed by ending the run: close the
   window, let the loop run the deletes, end the loop 200 ms later. rc 0 in 5 s.
   An intermediate attempt -- `closeAllWindows()` and `exit(0)` in one slot --
   SEGFAULTED (rc 139) after PASS had been written, which is still a red leg.
2. **A reaping race in my own watchdog.** The first version backgrounded the exe,
   polled `kill -0` and asked `wait` for the status; when bash had already reaped
   the child, `wait` answered **127** for a run that exited 0. Measured twice on
   the same passing exe, seconds apart: `exits 0 in 7s`, then `did not finish:
   rc=127 after 6s`, with a complete PASS log on disk both times. Now `timeout`
   runs it in the foreground and answers the real code (or 124 for the hang).
3. **Windows refusing to start the image.** A 127 also happens for real, twice in
   about fifteen launches, always immediately after the previous exe exited:
   `CreateProcess` fails and nothing runs. That is not a verdict about the code,
   so the gate retries such a launch once, after a pause, and counts only the
   second answer.

There is one thing left that I have NOT fixed and that the director should know:
on one run in about ten, the harness exits **139 (segfault) after writing PASS**
-- the ordinary Qt teardown of this two-window route is not reliable. Every
measurement is complete before it happens (the log, both PNGs), it does not occur
in the rung's ending (the rung never gets there), and it is not the deadlock. It
is a shutdown defect of the configured-resource route and belongs to whoever owns
that route, not to the lock.

### 3f. Every neighbouring gate, BOTH runs, side by side

The 01:17 column is section 3a (logs in `gates_0117/`); the 03:56 column is this
lane's exe (logs in `gates_new/`, driver + wall times in
`gates_new/summary.txt`). Game check before the run and inside every script:
clean, 04:00:42-04:47:35.

| gate | 01:17 exe (BAKEREC1's) | THIS BUILD 03:56:42 | verdict |
|---|---|---|---|
| `lodgen_bakerec.sh` | 21 checks, **3 failures**, 9 min | 21 checks, **3 failures**, 478 s | **unchanged.** The same three rows: leg (e) x2 (the `--native-verify` refusal names neither the plugin nor a direction word) and leg (h) (a second nondeterministic census field). BAKEREC1's. |
| `lodgen_layout.sh` | 22 checks, **5 failures**, 905 s | 22 checks, **5 failures**, 966 s | **unchanged.** All five name `Commonwealth.lodb`. BAKEREC1's. |
| `lodgen_defaults.sh` | 28 checks, 0 failures, 1009 s | 28 checks, 0 failures, 937 s | **PASS both.** |
| `lodgen_native.sh` | sections 69 / 44 / 87 / 52 / 17 / 15 / 25, **2 FAIL rows** in section 5, 324 s | the same section counts, **the same 2 FAIL rows**, 222 s | **unchanged.** Section 5 wants two ledgers to differ only in the command-line digest; they differ in more. BAKEREC1's. |
| `lodgen_btofree.sh` | **NOT RUN** -- refused itself, `Fallout4.exe is running` | 21 checks, **1 failure**, 195 s | the one failure is `DIFFERS: Commonwealth.lodb -> Commonwealth.lodb (716 vs 2161)` against the pinned rung -- the same record as the layout and bakerec reds. BAKEREC1's. |
| `lod_generation.sh` (panel self-test) | **NOT RUN** -- the game came up first | **128 checks, 0 failures**, 14 s, floor 121 | **PASS.** |
| `gamemanager_archlock.sh` (new, this lane) | n/a -- the 01:17 exe predates the harness and the verb, so it cannot run either leg | **20 checks, 0 failures**, 03:56:52-04:00:30 | **PASS**, and its rung HANGS (section 3d). |

Nothing moved between the two columns except the two gates that had never run.
Every red is one file, `Commonwealth.lodb`, in four different gates, and it is
BAKEREC1's bake record: it carries a wall clock and a peak working set, so two
runs of the same tree cannot produce the same bytes. Nothing this lane touched is
reachable from it.

## 4. Docs

| file | what was added |
|---|---|
| `docs/MISTAKES.md` (top, newest first) | "Every resource gate opened a file BY PATH, so the deadlock lived in the one route no gate took" -- what let it through: the condition needs BOTH an empty document data path (only the QBuffer route makes one) and an unbuilt parent index (only a fresh process has one), and every gate we had supplied at most one of the two |
| `MISTAKES.md` (root, this lane's own) | three items: (1) broke BAKEREC1's escaping rule in a different wrapper (`python -c` from bash), caught by the script's own anchor assertion; (2) designed the refuter around the loose-file route, which cannot reproduce the bug, and caught it by tracing the two Files tab routes before writing the harness; (3) wrote `ogl->repaint()` when `GLView` is a `QOpenGLWindow`, caught by a syntax-only compile before the build |

Neither `WW_CHANGES.md` nor `HANDOFF.md` was touched: section 5 has their text for
the director to splice.

### The exes, by hash (as they were at launch -- section 0 has the final table)

| exe | sha1 | bytes | mtime |
|---|---|---|---|
| `release/NifSkope.exe` at launch (BAKEREC1's 01:17, carries the hotfix) | `5ddc0a7a78b28539cc5bb6013319a691b07d6f5e` | 22,459,904 | 2026-09-17 01:17:01 |
| `release/NifSkope.before_archlock1.exe` (01:00, no hotfix; used as `$PREV`) | `7460bf768d5d6b9df009d47d3024f431de5f6f4a` | 22,459,904 | 2026-09-17 01:00:05 |

The 01:17 exe was also copied to `release/NifSkope.at_0117.exe` before any build
of mine, so the numbers in 3a can be reproduced after this lane relinks.

### 1a. Why an EMPTY data path is not a detail but the whole condition

Both lookups open with the same guard (`src/gamemanager.cpp:283-284` and
`:311-312`):

```cpp
if ( !ba2File && !dataPaths.isEmpty() )
    init_archives();
```

`init_archives()` builds the PARENT as well as itself
(`src/gamemanager.cpp:183-184`, `if ( parent && !parent->ba2File ) parent->init_archives();`),
and it does all of that under the WRITE lock with nothing else held. So for any
ordinary document -- one opened by path, which has a data path -- the prologue
warms the parent before the read lock is ever taken, and the recursion below can
never meet an unbuilt parent.

A document whose `dataPaths` is empty skips that prologue entirely. It reaches
the read lock with no index of its own and an unbuilt parent, misses, and
recurses. That is the only shape that gets there, and it is produced by exactly
one route in the program: bytes loaded from a `QBuffer`, i.e. the Files tab's
configured-resource row. The parent must ALSO have a non-empty `dataPaths`, or
its own prologue would skip `init_archives()` too and the lookup would simply
miss instead of hanging -- which is why the harness forces roots for `FALLOUT_4`
and not only for the game the empty document reports.

(table 3a, continued)

| gate | checks | failures | wall | verdict |
|---|---|---|---|---|
| `lodgen_defaults.sh` | 28 | 0 | 1009 s | PASS |
| `lodgen_native.sh` | 69 / 44 / 87 / 52 / 17 / 15 / 25 per section, 2 FAIL rows | 2 | 324 s | **FAIL -- BAKEREC1's, not mine** |
| `lodgen_btofree.sh` | -- | -- | 1 s | **NOT RUN: refused itself, "Fallout4.exe is running"** |
| `lod_generation.sh` (the panel self-test) | -- | -- | -- | **NOT RUN: the game came up first** |

`lodgen_native.sh`'s two failures are the same file as `lodgen_layout.sh`'s five:
section 5 compares the stock bake's `Commonwealth.lodb` with the native bake's
and requires them to differ ONLY in the command-line digest. They differ in more
than that, because the record carries fields that move between two runs of the
same tree -- which is exactly what `lodgen_bakerec.sh` leg (h) says in its own
words. One defect, four gates, all BAKEREC1's.

## 6. The game came up at 03:10, and went away again at 03:13

`tests/spells/lodgen_btofree.sh` refused itself after 1 second at 03:10 with
`REFUSED: Fallout4.exe is running`, and a direct check at 03:11:26 confirmed it
(`Fallout4.exe 38248, 5,411,800 K`). The game was down when this lane was
launched (02:19) and when every gate in section 3a started; it came up between
`lodgen_native.sh` finishing and `lodgen_btofree.sh` starting. No build was run
while it was up, and nothing of his was touched: the lane wrote
`scratchpad/archlock1_20260917/PENDING.md` and stopped at the line before its
build.

At 03:13:31 `tasklist` was clean again. Every launch and every build since has
been preceded by `tasklist | grep -i -E "Fallout4|NifSkope"` in the same command,
and `tools/ww_build.sh` refuses on its own account as well. PENDING.md is
superseded by this section and by 3d.

| moment | what |
|---|---|
| 03:13:31 | game gone, `tasklist` clean |
| 03:15:12 | `GeneratedFiles/.obj/archlocktest.o` |
| 03:19:59 | first build of the fixed exe, `BUILD-RC=0` |
| 03:33-03:38 | three more builds while the harness's ENDING was being fixed (section 3e) |
| 03:56:37 | the rung, from this tree minus the two unlock lines |
| 03:56:42 | **`release/NifSkope.exe` as delivered**, 22,477,824 B, sha1 `304d6bdc6d6070a508bdfc3782c96a22011a4a0a` |
| 03:56:52-04:00:30 | `gamemanager_archlock.sh`: 20 checks, 0 failures |

## 7. The pictures

All three under `scratchpad/archlock1_20260917/images/`, all written by the new
exe or drawn from the gate's own bytes -- none is a desktop capture.

| file | what it shows |
|---|---|
| `archlock_viewport.png` (867x730) | **the nif rendered on the new exe**: `m_arm_heavy_l.nif`, opened as a CONFIGURED RESOURCE (the route that used to freeze), textured -- which is the texture lookup in leg (c) going through the parent index on the paint path. `GLView::grabFramebuffer()` after `center()` and a real frame. |
| `archlock_window.png` (1280x800) | the whole harness window: the Files tab, `Loaded file - 1  m_arm_heavy_l.nif`. `QWidget::grab()` renders the widget tree, so the viewport area is empty in it -- the GL window is not part of that tree. The two pictures together are the window and what it was drawing. |
| `rung_hangs_and_is_killed.png` (1120x774) | the terminal capture: legs (a) and (b), the rung killed after 90 s and 91 s with its log stopped at the open, and the fixed exe exiting 0 in 5 s. |

BAKEREC1's owed leg (e) picture was delivered separately:
`scratchpad/bakerec1_20260916/images/native_verify_names_the_plugin.png`
(54,577 B, 1120x378).
