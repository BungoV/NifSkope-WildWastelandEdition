---
name: nifskope-ww-resume-pending
description: Resume one or more NifSkope Wild Wasteland lanes that ended BUILD PENDING (the game was up, or an account-B lane could not run the exe) and turn them into one built, gated, ledgered result -- the read order for their PENDING resumes, the cross-lane change one lane could not make, qmake-before-make and the dependency read-back, the exe-newer sweep over every changed file rather than one, the sequential harness chain, and the four documents that have to stop saying "not built". Use whenever a lane's deliverable is code on disk that nobody has compiled, and whenever two such lanes share one build.
---

# NifSkope WW: build and gate a BUILD PENDING lane

Repo `E:\Projects\NifskopeWildWastelandEdition`. This is the lane that turns
*"finished and syntax-checked"* into *"built, measured, written down"*. It sits
on top of `nifskope-ww-build-verify` (the build chain itself) and does not
repeat it.

## 1. Read in this order, and nothing else first

1. `CONSTITUTION.md`.
2. Every `scratchpad/<topic>_<date>/PENDING.md` you were given. These are
   paste-able resumes and they carry the steps their own lane could not run,
   in order, with the numbers each step must produce.
3. Any `*_CHANGE_NEEDED.md` beside them: the change ANOTHER lane owns (one lane
   per file). Applying it is the resuming lane's job and it is applied
   **exactly as written**, not improved.
4. The lane reports (`scratchpad/lane_<name>_report.md`) -- for what was
   measured, what was not, and the lane's own refuters. The refuters are the
   gate you are about to run.

## 2. Apply the owed cross-lane change with a script, at equal byte length

A `*_CHANGE_NEEDED.md` is usually a table of one-line substitutions. Write the
patch as a Python script under `scratchpad/<yours>/`, and:

* measure the file's line endings FIRST (`b.count(b'\r')`). `src/nifskope.cpp`
  is mixed and mostly CRLF; `src/nifskope_ui.cpp` is LF-only. Put the line
  ending IN the anchor, so an anchor that does not match the file's real bytes
  refuses instead of writing;
* assert `count(anchor) == 1` before every replace, and assert CR, LF and total
  bytes are unchanged after -- a pure rename at equal length must move nothing;
* leave what the note did not list, and SAY SO in the report. A note that lists
  five suffix comparisons and three comments has not authorised the log strings
  two lines away.

## 3. qmake BEFORE make, then read the dependency back

qmake freezes its dependency lists when the Makefile is generated, so a NEW
`#include` added by a pending lane is invisible to `make` and the link silently
mixes a fresh object with a stale one. Any pending lane that added a cross
include, or whose resume says a `Makefile.Release` line was hand-patched, gets:

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > <scratch>/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > <scratch>/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" <scratch>/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

Then READ the regenerated dependencies back, per object, by name:

```bash
grep -n "lodtfile\.h" Makefile.Release | cut -c1-160
for n in <line numbers>; do awk -v s=$n 'NR<=s && /^GeneratedFiles\/\.obj\/[a-z_]*\.o:/ {last=$0} NR==s {print last}' Makefile.Release; done
```

The `awk` walk is the point: `grep -A3` misses a dependency that sits ten
continuation lines down, and a hand patch that qmake has just thrown away looks
exactly like one that survived unless you name the object it belongs to.

## 4. The exe is newer than EVERY changed file, not the one you edited

Two pending lanes plus your own change is a dozen files, and `test exe -nt
src/thefile.cpp` passes while another lane's source is newer. Sweep the whole
working set:

```bash
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue
  [ "$EXE" -nt "$f" ] || echo "STALE vs $f"
done
```

and, for every header a pending lane touched, the object staleness check from
`nifskope-ww-build-verify` ("A successful build is not a consistent one").

## 5. One sequential chain for the harnesses, with the numbers echoed

One NifSkope instance at a time, so the harnesses are a chain, not a fan-out.
Write it as a script that logs each harness separately and echoes its summary
line as it lands, then run it in the background and read the summary file:

```bash
run () { local label="$1"; shift; local secs="$1"; shift
  echo "### $label start $(date +%H:%M:%S)"
  timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
  echo "### $label rc=$?  $(date +%H:%M:%S)"
  grep -E "checks, [0-9]+ failures|^PASS|^FAIL" "$OUT/logs/$label.log" | tail -4; echo; }
```

* Run only the harnesses the change reaches, and NAME the skipped ones with the
  reason in the report. A skipped harness with no reason reads as a pass.
* Some harnesses print `RESULT PASS` and no count -- count their `^  ok` lines
  yourself rather than reporting a blank.
* A harness whose fixture is the user's own installed file (`lodl_open.sh`
  defaults to `.../mods/FO4CS/Terrain/Commonwealth.lodl`) runs AFTER the step
  that renames or writes that file. Order the chain around its fixtures.

## 6. When a gate fails, measure the cause and STOP

The resuming lane's product is a verdict, not a cure.

* Measure the cause and say it with the number that shows it. The instrument
  the failing lane shipped is usually the answer -- read its log rather than
  re-deriving the theory.
* **Do not land the fix.** A design failure found in a build lane goes to the
  director with its candidates named as candidates. Landing it costs a second
  build to add and a third to revert when the cure turns out to be worse than
  the disease, which is exactly what happened on 2026-09-09
  (`MISTAKES.md`, "lane BUILD2 changed behaviour its brief did not give it").
* Judge the tree you LEAVE by its failure mode, not its gate colour: a change
  that makes a pipeline produce empty output while exiting 0 is worse than the
  defect it fixes, and it does not stay in the tree.

## 7. The four places that must stop saying "not built"

None of these is optional and all four are done before the report is written:

1. `WW_CHANGES.md` -- the entry's status block becomes the measured one: the
   exe timestamp, each harness with its numbers, what is still red and what was
   not measured. The file is MIXED; the 2026-09 entries at the top are LF-only.
   Assert the CR count is unchanged (19,020 as of 2026-09-09).
2. `MISTAKES.md` at the root -- every mistake found while building, including
   the resuming lane's own, the moment it is recognised.
3. Each lane report -- a `## Build (<your lane>)` section with the gate table,
   the mtimes in ONE table, what was skipped and why, and what is still owed.
   Do not rewrite the lane's own text; append.
4. Any skill whose text the build has just disproved. A skill in the LIVE tree
   (`E:\Projects\Claude\.claude\skills`) instructing lanes to rely on behaviour
   that does not exist is the most expensive document in the repo.

## 8. Standing traps

* A "kept for a before-picture" copy of the exe (`release/NifSkope.before.exe`)
  is deleted only when the gate that replaced it PASSED. If the gate failed, the
  copy is the only picture of the old behaviour there will ever be.
* A user's installed files are renamed only with the verify in the same step:
  move, then read each one back with the new command, one at a time, and print
  name, size and the tool's own exit code. Backups keep their old names -- a
  renamed backup makes the rollback ladder unreadable.
* `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` before the build and
  before every exe launch; `rc=1` or the lane ends BUILD PENDING again.
