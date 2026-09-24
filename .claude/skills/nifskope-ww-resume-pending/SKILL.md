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

* write that script with the WRITE TOOL, never a Bash heredoc or `python -c`:
  a quoted heredoc through the tool halves backslashes, so an anchor that holds
  a backslash-n for a C string literal arrives as a real newline and counts 0
  (paid for the fourth time 2026-09-10, lane BUILD6; the rule lives in
  `nifskope-ww-build-verify`). Take the note's code from its own fenced blocks
  by index rather than retyping them, and print `repr()` of any anchor that
  carries a backslash before counting it;
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

**qmake's scan stops at `src/`** (2026-09-10, lane BUILD4). A new include that
points into a vendored `lib/` tree is STILL absent from the regenerated
dependency block, so re-running qmake does not cure it. `src/lodtfile.cpp`
gained `#include "esmfile.hpp"`; after `qmake` its object's block still read
only `src/lodtfile.cpp src/lodtfile.h src/esmdata.h src/io/lodvfile.h`. For a
`lib/` include the object-mtime check from `nifskope-ww-build-verify` is the
only gate -- and if a `lib/` header is itself edited, delete the objects of
every `src/` file that includes it before relinking.

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

### Exactly one copy of the chain, proved by the process list

Two copies of the same chain script ran at once on 2026-09-11 (lane
WATER8-GATE) and every harness after the first collided on its FIXED port:
`animws.sh` "the harness wrote no log (did the app exit before it ran, or is
port 42317 bound?)", `water_mark.sh` "no dock log". The summary file interleaved
two runs, which is the tell -- `### water_ui start 05:23:53` printed next to
`### files_tab rc=1 05:23:55`.

* **An empty log is not "did not start".** A chain that has been alive for two
  seconds has written nothing yet. The only honest answer is the process list,
  by command line, the same filter `nifskope-ww-build-verify` uses for NifSkope:

```bash
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='bash.exe'\" | Where-Object { \$_.CommandLine -like '*gates.sh*' } | Select-Object ProcessId, CommandLine | Format-List"
```

* **Never redirect a launcher into a directory the launched script creates.**
  `bash chain.sh > $OUT/logs/stdout.log` fails before the script runs, because
  the shell opens the redirect first -- and it looks exactly like a script that
  refused to start. `mkdir -p` the log directory in the launching shell.
* **Give the chain script its own lock**, three lines, so a second copy is
  impossible rather than merely unlikely:

```bash
mkdir "$OUT/.lock" 2>/dev/null || { echo "REFUSED: a chain is already running ($OUT/.lock)"; exit 8; }
trap 'rmdir "$OUT/.lock" 2>/dev/null' EXIT
```

* When it happens anyway, KEEP the ruined run beside the good one
  (`logs_void_<reason>/`) instead of deleting it: the interleaved timestamps are
  the evidence that the numbers were rubbish, and a reader of the report will
  want them.

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

## 9. A PENDING's numbers are PREDICTIONS (2026-09-10, lane BUILD9)

A resume was written by a lane that could not run anything, so every figure in
it is a prediction. Re-derive, do not accept:

* marker counts. `scratchpad/hkx3_20260910/PENDING.md` said
  `grep -c "lane HKX3" ...` would print 1 and 7; it prints 1 and **6**, and 6 is
  correct -- six edits reach that file, one marker each.
* proof-of-flag checks. The same resume said to prove `WW_HKXANIM_UI` reached
  the compiler with `grep -c "TimelineSeqBox" release/NifSkope.exe`. That can
  never pass: `QStringLiteral` compiles to UTF-16. Use
  `grep -c <MACRO> Makefile.Release`.
* the gate's own expected values. Four gates in this round had never been
  executed, and running them for the first time found four defects IN THE GATES
  (a `/tmp` fixture path the Windows binary could not open, a floor that picked
  an already-failing victim, two counts with no names beside them, and a floor
  that needs keyboard focus in a window that is never activated). See
  `ww-test-harness-add`.

## 10. The SIBLINGS assert the old strings (2026-09-10, lane BUILD9)

When the pending lane renamed user-visible text, every other harness that quotes
those labels goes red the moment the rename lands -- `WW_LOADEDNIFS_TEST` went
from 3 failures to 9 on lane FILESTAB's renames, and `top_bar.sh` looked like it
had gained one too.

* Repair the **expected literals only**, with a refusing script, and touch no
  check name, no assertion and no widget.
* The proof is the count returning to its pre-change baseline, which is why the
  baseline is worth recording BEFORE applying a hook-up: run the neighbours once
  on the old exe and keep the number.
* Not every new red is yours. `top_bar.sh`'s five failures survived the repair
  because it expects a View menu listing six docks that were merged into one
  "Left Editor" entry long before this session -- the log's own menu dump says
  so. Read what the harness printed before repairing anything.

## 11. The BEFORE picture, and the env-var leak in the chain (2026-09-10, lane BUILD12)

Two traps in this page's own machinery, both paid for on one resume.

**A pending lane's NEW harness cannot photograph the OLD state.** WATER7 owed
bungo a before/after of the top of the window and its resume said to grab the
"before" with the gate it had just written. That gate drives `WW_WATERUI_TEST`,
which does not exist in the exe on disk: run against it, the harness writes no
log, no PNG, and the spell exits 1 -- and the moment the build lands the old
state is gone for good. **Take the before picture with a SIBLING spell that
already photographs the same region and is already in the exe** (here BUILD9's
`ui_align.sh`, `SHOT=` a path under your own scratchpad), run BEFORE the
hook-up is applied, and say in the report which spell took which half. Its
geometry dump doubles as the numeric baseline for the same rows, which is what
turns "the top did not grow" from a claim into a comparison: `tMode` 33 px at
top 36 before, 35 px at top 35 after, search row at top 70 in both.

**`VAR=x run_helper` LEAKS.** Section 5's chain helper is a shell FUNCTION, and
bash keeps a variable assignment that prefixes a function call in the
environment AFTER the function returns. A chain written as

```bash
SHOT="$OUT/a.png" run water_ui 240 bash tests/spells/water_ui.sh
...
run ui_align 300 bash tests/spells/ui_align.sh      # still sees SHOT
```

hands `ui_align.sh` the first gate's `SHOT` and quietly overwrites the picture
that was the whole point of the run. Put the assignment on the CHILD instead --
`run water_ui 240 env SHOT=... bash tests/spells/water_ui.sh` -- and give every
other picture-taking spell its own explicit `env SHOT=`, because their defaults
point at some previous lane's scratchpad folder.
