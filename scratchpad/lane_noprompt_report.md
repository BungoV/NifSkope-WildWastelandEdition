# Lane NOPROMPT — headless runs must never raise the Save Confirmation dialog

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **no commits**.
2026-09-09.

bungo, verbatim: *"agents keep always hanging on save confirmation"*, with the
"Save Confirmation - NifSkope" box: *"You have unsaved changes to
TreeMapleForest3. Would you like to save them now? Yes / No / Cancel"*, raised
while a headless render or bake was closing.

---

## 1. Cause

**Two separate facts, and both are needed for the hang.**

### 1a. The exit path reaches `closeEvent`, and always did

`qApp->quit()` — which every WW_* hook ends in
(`src/nifskope_ui.cpp:21422` render shot, `:22076` impostor bake, and ~60 more
for the `WW_*_TEST` harnesses) — **is not `exit(0)` on this Qt.**
`QCoreApplicationPrivate::quit()` is virtual
(`C:/msys64/ucrt64/include/qt6/QtCore/6.11.1/QtCore/private/qcoreapplication_p.h:105`)
and `QGuiApplicationPrivate` overrides it
(`.../QtGui/6.11.1/QtGui/private/qguiapplication_p.h:83`) to close every
top-level window before the loop exits. The build is Qt **6.11.1**
(`qconfig.h:67`).

So the quit delivers a `QCloseEvent` and runs:

* `src/nifskope.cpp:7421` — `NifSkope::closeEvent`, which at
* `src/nifskope.cpp:7516` (was `:7402` before this lane) asks
* `src/nifskope_ui.cpp:28489` — `NifSkope::saveConfirm()`, whose first test is
  `!cfg.suppressSaveConfirm && ( isWindowModified() || !nif->undoStack->isClean() )`,
  and which then raises a **modal** `QMessageBox::question`.

Nobody is there to click it, so the process sits until the caller's `timeout`
kills it (`rc=124`) and the picture is never returned.

This is not a new inference: the tree already carried the evidence.
`src/nifskope_ui.cpp:4266` says, in the poselib harness's own words, *"the
save-on-quit prompt must be DECLINED — clicking Yes there opens a native save
dialog we can't drive (that was the earlier hang)"*, and two harnesses
(`:4272`, `:4543`) keep a 50 ms timer running purely to click "No" on it. About
forty more call `undoStack->setClean()` by hand before quitting. Every one of
those is a per-route patch for this one defect.

### 1b. Which headless route leaves the document modified

Measured by reading the writers, not by inference from the dialog:

| route | what it writes into the loaded model | verdict |
|---|---|---|
| **impostor card bake**, `WW_IMPOSTOR_BAKE` | `src/nifskope_ui.cpp:21489-21490` sets `LOD1 Size` and `LOD2 Size` to 0 on every `BSMeshLODTriShape`; `:21497` sets `Flags \| 1` on every shape whose name ends `_L<digit>` | **dirty.** This is bungo's case exactly — the code's own comment names *"the maple's near mesh: 28 and 16 vertices of branch cards beside the 78"*, and his box named `TreeMapleForest3` |
| **generated terrain documents**, `.btd` / `.lodt` | the document is BUILT, not parsed (`src/nifskope.cpp:10058-10101`, `nifCreateBtdTerrainScene` / `nifCreateLodtTerrainScene`) | **dirty, from birth.** Building fires `NifModel::dataChanged`, wired to `setWindowModified( true )` at `src/nifskope.cpp:1593-1596` |
| `WW_RENDER_SHOT` itself | nothing — it resizes, hides docks, sets camera/time/scene options, calls `indexAt()` and grabs | clean |
| `WW_LOD_CHANNEL` | `wwLodChannelView` only, a view flag (`src/nifskope_ui.cpp:21364`) | clean |

So "the render hook's exit still reaches the dialog" has two halves: the exit
path reaches `closeEvent` for **every** WW_* route (1a), and the document is
dirty on the bake and on any terrain document (1b). A plain `WW_RENDER_SHOT` of
a `.nif` is clean and never hung — which is why this looked intermittent.

**Not measured:** I did not run the old binary to watch the dialog appear. The
mechanism above is read from the sources and from Qt's installed headers, plus
the tree's own contemporaneous notes at `nifskope_ui.cpp:4266`. The harness in
section 3 is what turns it into a number, and it has not been run.

---

## 2. Fix

Three edits, all in files this lane owns. `src/nifskope_ui.cpp` was **not**
touched (lanes TERRAINFIX and IMAGES3 are live).

**(a) One predicate — `NifSkope::wwHeadlessRun()`**, `src/nifskope.cpp:7407`,
declared `src/nifskope.h:131`. It is deliberately **the same test `saveUi()` has
made since 2026-07-27** (`src/nifskope_ui.cpp:28547`): any environment variable
whose name starts `WW_`. Every headless entry this application has is selected
by exactly such a variable, so a new switch is covered the day it is written and
there is no second list to maintain. `-no-gui` is answered too, though that path
selects a `QCoreApplication` in `main.cpp:76` and never builds a window. Cached
in a function-local static.

**(b) One guard in the close path**, `src/nifskope.cpp:7457`, immediately after
the existing `closingWorkspaceGroup` early-out and before anything asks. It sets
**state, not a branch**: `cfg.suppressSaveConfirm = true` and a clean undo stack
on this window, on every window in `sessionDocumentWindows`, and on every
`BackgroundNifDocument` (whose `isModified()` returns false once the stack is
clean and `unsavedInMemory` is cleared). The rest of `closeEvent` then runs
unchanged and takes the discard answer by itself — this window's
`saveConfirm()`, every group member's, and the *"has unsaved changes and is not
on disk anywhere"* background-document question at `:7565`. The group walk, the
member closes and the deletions are untouched, so the harnesses that close a
second window mid-test still see what they expect.

With no `WW_` variable set the block does not execute, so interactive behaviour
is unchanged — including the Reload prompt that `src/nifskope_ui.cpp:11772`
deliberately tests for, which goes through `saveConfirm()` and not through
`closeEvent`.

**(c) The generated document is no longer born dirty**, `src/nifskope.cpp:10102`.
A `.btd`/`.lodt` scene clears and cleans its undo stack and its window-modified
flag before `completeLoading`, exactly as the starter document already does at
`:10039-10042` (whose comment already states the intent: *"including not asking
about unsaved changes on close"*). That is the honest state — nobody edited
anything, and `NifSkope::save()` refuses to write those two suffixes back
anyway (`:10152`, it redirects to Save As so a game file is never overwritten
with foreign bytes). A later real edit dirties it again through the same signal.

The bake's in-memory edits are equally scratch and should arguably be bracketed
the same way, but that code is in `src/nifskope_ui.cpp`, which this lane does
not own. The guard covers it. **Owed:** a one-line bracket there when that file
is free.

**(d) A readback, because a discard leaves no trace.** The dialog was visible; a
silent discard is not, and *"the harness did not hang"* passes just as well on a
document that was never dirty. So the guard names every document it actually
decided for, on `qInfo` and in `release/ww_headless_close.log` (append, same
home and discipline as `WW_GRID_PROBE`), and writes **no file at all** when
there was nothing to discard. The file is written because NifSkope links as a
Windows GUI-subsystem binary and a piped `qInfo` cannot be relied on.

**Line endings.** `src/nifskope.cpp` is mixed; every region touched is CRLF
(verified per line before patching), so every inserted line is CRLF and the
patch scripts assert the CR count grew by exactly the number of lines added:
9234 → 9370, +136, zero lone CRs. `src/nifskope.h` stays LF-only (CR 0).
`tests/spells/render_shot.sh` is LF-only like its neighbours.
`WW_CHANGES.md` is mixed but its newest-first head is LF; the entry is LF and
the file's CR count is unchanged at 19020. `MISTAKES.md` LF-only, CR 0.

**Diff size, against copies taken before patching** (not against HEAD — the tree
already carried other lanes' uncommitted work, see Mistakes):
`src/nifskope.cpp` +136/-0, `src/nifskope.h` +3/-0.

**Syntax check** (CONSTITUTION 6, the cannot-build path): flags read from
`Makefile.Release`, `g++ -fsyntax-only src/nifskope.cpp` → **RC=0**, twice
(after each of the two code patches). That proves it compiles. It proves nothing
about linking or behaviour.

---

## 3. Harness — BUILD PENDING

`tests/spells/render_shot.sh`, new, 177 lines, `bash -n` clean. **Not run**: the
build gate in the brief (`scratchpad/images_20260909/DONE`) did not exist at the
single check, so another lane holds the build slot. Nothing running
(`tasklist | grep -i -E "Fallout4|NifSkope"` → `rc=1`).

Resume instructions, gates and traps: `scratchpad/noprompt_20260909/PENDING.md`.

**Design.** Fixtures are derived from the application itself — `-no-gui new
--cube` for the scene, one `-no-gui set -f Name` to rename the shape `*_L1` —
so the gate needs no game corpus and nothing hand-authored (zero-authoring,
CONSTITUTION 10). Three runs, each measured on exit code, wall clock and its own
output file:

| section | run | must be |
|---|---|---|
| 1 | `WW_RENDER_SHOT` on the plain cube | rc 0, < 45 s, PNG written, **no** discard line |
| 2 | `WW_IMPOSTOR_BAKE` on the plain cube | rc 0, < 45 s, sidecar written, **no** discard line |
| 3 | `WW_IMPOSTOR_BAKE` on the `*_L1` cube | rc 0, < 45 s, ≥ 1 `hidden` line in the sidecar, and `discarded cube_lod` in the log |
| 4 | after all three | no NifSkope process left running |

14 checks, and the floor is on both sides: **the pair is the point.** A guard
that never fires fails section 3's discard-line check; a guard that discards
indiscriminately fails sections 1 and 2's "discarded nothing"; the old code
fails all three on `rc=124`. Section 3's `hidden`-line check is the anti-vacuity
half — without it, a dirty case that was quietly clean would pass by looking
exactly like section 2.

There is no log line for the dialog itself to test the absence of; the dialog
never wrote one. That is why the guard was given a positive readback instead —
absence of a hang is weak evidence, presence of a named discard is not.

**Nothing in this section is a measured result.** The numbers the table asks for
do not exist yet.

---

## 4. Mistakes

Both written into `MISTAKES.md` at the repo root (CONSTITUTION 2), newest first.

1. **`git diff --numstat` read as this lane's diff in a shared tree.** It said
   152 added / 2 deleted against a patch that reported 103 lines. The extra were
   another lane's uncommitted edits to the same file: numstat measures the TREE
   against HEAD, not the lane against its own start. Found by the two numbers
   disagreeing, then `git status --porcelain` (forty-odd modified files). Real
   figure +136/-0, from `diff` against a pre-patch copy. Rule: in a shared
   worktree a lane measures against its own backup, and takes that backup before
   the first patch script.
2. **A build gate read before the step the brief put it at.** The brief said to
   check `scratchpad/images_20260909/DONE` once, after the code and harness were
   written, and never to poll; it was read ~40 minutes early while listing
   `scratchpad/`. Same answer both times and no build was started, but it is the
   first half of polling. Rule: a gate is read at the step it belongs to, and a
   directory listing is not an excuse to read one that is not due.

A third, caught before it did damage rather than after: a heredoc-driven
`python - <<EOF` edit to a patch script silently failed to replace a
backslash-escape, exactly the trap `nifskope-ww-build-verify` documents
("heredocs halve backslashes"). Re-done with the Write/Edit tools. Not a
separate ledger entry — the skill already carries the rule, and this is the rule
working.

---

## 5. Finished-work skill review

**Loaded and used:** `nifskope-ww-render-shot` (the switch table, the
one-instance / second-monitor rules, and `WW_IMPOSTOR_BAKE`'s shape, which is
what identified the bake as the dirtying route), `nifskope-ww-build-verify` (the
cannot-build syntax pass with flags read from `Makefile.Release` — it is the
only reason this lane can say "RC=0" at all; and the patch-with-a-script-file
rule, which the heredoc failure above then proved again).

**Amended, this session:** `nifskope-ww-render-shot`, new section *"The run
hangs and writes nothing: the Save Confirmation dialog"* — the Qt 6.5+ `quit()`
mechanism, the two routes that dirty a document, `rc=124`-with-output-on-disk as
the signature, and how to read the log file to tell "the guard ran" from "the
run was never recognised as headless". Written into the LIVE tree,
`E:\Projects\Claude\.claude\skills\nifskope-ww-render-shot\SKILL.md`. The repo
tree `<repo>/.claude/skills` holds only `ww-control-calibration`, so there is no
second copy to keep in step (CONSTITUTION 1a, the two-tree drift) — **the
director should confirm that**, since the drift rule says diff per file rather
than assume.

**Wished had existed, and why it is not written:** a "measure a diff in a shared
worktree" procedure. It is two lines (back the file up first, diff against the
backup) and it belongs inside the two skills that already own patching and
committing rather than as a skill of its own; it is now in `MISTAKES.md` as a
rule. Declining deliberately, not silently.

**Not written as a skill either:** the fixture trick used by the harness —
deriving a Fallout 4 scene from `-no-gui new --cube` plus one `set -f Name`
instead of reaching for the game corpus. It is one line of shell, it is already
demonstrated in two harnesses, and a third instance would not have been cheaper
with a document to read. If a fourth harness needs it, it becomes one.

---

## Build (BUILD1) -- 2026-09-09

**The clocks, in one table** (CONSTITUTION 4). The exe is newer than every
source and every harness the two lanes touched, and than the two harness fixes
this lane had to make.

| artefact | mtime |
|---|---|
| `src/lodgen.cpp` | 16:27:07 |
| `src/lodtfile.h` | 16:31:24 |
| `src/lodtfile.cpp` | 16:31:40 |
| `src/nifskope.h` | 16:55:58 |
| `src/nifskope.cpp` | 16:57:58 |
| `tests/spells/render_shot.sh` | 17:12:18 (BUILD1's fixture fix) |
| `tests/spells/lodgen_terrain.sh` | 17:15:02 (BUILD1's rung-4 region) |
| `tests/spells/lodt_write.sh` | 17:17:21 (BUILD1's fallback check) |
| `Makefile.Release` | 17:21:52 (BUILD1's dependency stopgap) |
| **`release/NifSkope.exe`** | **17:22:05**, 17,796,608 bytes |
| `release/style.qss` | 17:22:05, identical to `res/style.qss` |

`make -j2` exited **0** (its own exit code gated the chain, not a grep), on a
game-down and NifSkope-free machine checked before the build and before every
run. The FIRST link, 17:09:31, was thrown away: it carried a stale
`btdterrain.o` and crashed -- see Mistakes.

### The gate table, all on the 17:22:05 exe

| gate | result |
|---|---|
| `tests/spells/render_shot.sh` | **15 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodgen_terrain.sh` | **26 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodt_write.sh` | **PASS**, exit 0, all three sections |
| four/five-worldspace `.lodt` vs heightmap | **0 differing texels** on all five |
| `tests/spells/lodt_open.sh` (version 1 file) | **23 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodt_open.sh` (version 2 file) | **23 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/btd_terrain.sh` | **13 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodgen_identity.sh` | **RESULT PASS**, exit 0 |

Nothing in the list was skipped. Harnesses NOT run, and why: everything the
two lanes did not reach -- the impostor-card, atlas, merge, far-ring, resource
stack, texture-array, octahedral, collision, panel and workspace spells. The
one that comes closest is `lodt_btd.sh` (the FO76 `.btd` -> `.lodt` conversion,
~25 minutes): `btd_terrain.sh` covers the same reader against the same
Appalachia file in a fraction of the time, and no lane touched the conversion
path.

### What the three cases measured

| case | exit | wall | its own output | discard log |
|---|---|---|---|---|
| `WW_RENDER_SHOT`, plain cube | 0 | 5 s (cap 90, slow bar 45) | 30,356-byte PNG | 0 lines |
| `WW_IMPOSTOR_BAKE`, plain cube | 0 | 3 s | `cube_plain.txt` | 0 lines |
| `WW_IMPOSTOR_BAKE`, `_L1` cube | 0 | 3 s | `cube_lod.txt`, 1 `hidden` line | `discarded cube_lod` |

No NifSkope process survived any of the three. The pair is what carries it: the
guard fired exactly once, on the only document that was edited.

**NOT measured.** The old binary was never run to watch the dialog appear. The
17:09 link overwrote it, and the guard cannot be switched off from outside --
it keys on any `WW_*` variable and every headless route sets one. "rc 124 is
the dialog" stays a reading of the sources, not an observation.

### The harness could not run as the lane left it

`render_shot.sh` died at `FAIL  could not rename the shape`. Its dirty fixture
was built with `-no-gui set -b N -f Name -v "<name>_L1"`, which the CLI refuses:
a block's `Name` is a `tStringIndex`, and `NifValue::setFromString`
(`src/data/nifvalue.cpp:715`) parses that as a NUMBER -- the CLI can address
the header string table by index but cannot introduce a new string. The two
fixture assertions had the same root: `get -f Name` prints the index (`1`), so
they were comparing a number against a `_L1` suffix, and the "plain" one passed
vacuously while the "dirty" one could never pass.

BUILD1's fix is harness-side only, no C++ change and no rebuild
(`scratchpad/build1_20260909/fix01_render_shot_fixture.py`): the rename
rewrites the one length-prefixed entry in the header string table -- the file
is still `new --cube`'s own bytes, nothing hand-authored -- and both names are
read out of `-no-gui list`, which prints the resolved string. The fixture
checks now read `shape [1] named 'Cube'` and `shape [1] named 'Cube_L1'`.

The check count is **15**, not the 14 the resume file predicted: the two
fixture assertions are checks too.

### Finished-work skill review (BUILD1)

**Loaded and used**: `nifskope-ww-build-verify` (the gated chain, make's own
exit code, the exe renamed aside, the link-time stylesheet copy, the
exe-newer-than-sources test, and the patch-with-a-script-file rule -- every fix
here is a `fixNN.py` under `scratchpad/build1_20260909/` with an anchor-count
assertion and a CR-count assertion), `nifskope-ww-lodgen` (the CLI table, the
worldspace IDs, the byte-identity gates, the editing traps),
`nifskope-ww-render-shot` (the switch table and the new "the run hangs and
writes nothing" section lane NOPROMPT added -- it named `rc=124`-with-output as
the signature, which is what made the rc-0 result legible).

**The skill that should exist and does not**, and it cost this build an hour:
*prove a build is CONSISTENT, not merely successful*. Nothing in
`nifskope-ww-build-verify` catches a translation unit that make had no reason
to rebuild, and that is exactly what happened -- the chain's own
`test exe -nt source` passed while one object was two hours stale against a
header that had grown three members. The procedure is short and mechanical:
after any change to a header, list every `.cpp` that includes it, check each
object's mtime against the header's, and re-run qmake when an include is NEW
(qmake's dependency lists are frozen at generation time). **WRITTEN**, as a
section of `nifskope-ww-build-verify` rather than a skill of its own, since it
belongs to the same chain: "A successful build is not a consistent one
(2026-09-09)", in the LIVE tree
`E:\Projects\Claude\.claude\skills
ifskope-ww-build-verify\SKILL.md`, with the
grep-and-mtime check, the delete-the-object-and-relink fix, the qmake caveat
and the symptom to expect. The repo tree `<repo>/.claude/skills` holds only
`ww-control-calibration`, so there is no second copy to keep in step (checked,
CONSTITUTION 1a).

**A second candidate, declined with a reason**: "regenerate bungo's installed
`.lodt` set" is now a written script
(`scratchpad/build1_20260909/regen_lodt.sh`) with the backup-first,
write-to-scratch, verify, then install order. It will recur -- the `.lodl`
rename is already owed -- but it is fifteen lines of shell that read better as
the script than as prose, and the script is in the repo. If a third lane needs
it with different worldspaces, it becomes a skill.

**Declined outright**: the header-string-table rename used to build the dirty
fixture. It is four lines of Python, it exists in `render_shot.sh` now, and the
CLI limitation it works around is the thing that should be fixed instead
(`-no-gui set -f Name` could assign through `NifModel::set<QString>`), which is
a code change for a lane that owns `src/nifcli.cpp`.
