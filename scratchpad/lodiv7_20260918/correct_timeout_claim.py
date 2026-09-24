# 2026-09-18 18:2x. A claim this lane wrote into the skill and the ledger earlier
# today was MEASURED FALSE the same afternoon. Correcting it where it was written.
#
# What was claimed: wrapping the exe launch in `timeout 600` keeps a stuck window
# from wedging the tree.
# What was measured: a run started 18:04:59 with the fixed shot() path launched
# NifSkope PID 24828, wrote one 268-byte log at 18:05:04, and was STILL RESIDENT
# at 18:22:43 -- 17 minutes, well past any 600-second guard -- with NO timeout.exe
# process present and no picture produced anywhere.

p = '.claude/skills/nifskope-ww-render-shot/SKILL.md'
s = open(p, encoding='utf-8', newline='').read()

O1 = """2. **Wrap it in `timeout`.** `timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")"`.
   A stuck window is not a failed test, it is a failed TREE. And a wedged process
   may be beyond the session's own permission to kill: `Stop-Process`, `taskkill`
   and `CloseMainWindow` were all refused by the classifier in LODIV7, so the
   lane could not clean up after itself and closed on `PENDING.md`."""
assert s.count(O1) == 1

N1 = """2. **Wrap it in `timeout` -- and know that `timeout` is NOT enough on its own.**
   `timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")"`. A stuck window is not
   a failed test, it is a failed TREE.

   **Measured the same afternoon it was written, 2026-09-18 18:2x: the guard did
   not fire.** A run started at 18:04:59 through this exact `shot()` launched
   NifSkope PID 24828, wrote one 268-byte log at 18:05:04, and was still resident
   at **18:22:43** -- 17 minutes, well past 600 seconds -- with **no `timeout.exe`
   process left** and no picture written. GNU `timeout` signals its child, and a
   native Windows GUI process started from an MSYS/Git-Bash shell does not act on
   that signal; `timeout` gives up and exits, and the exe it was guarding is left
   running and now ORPHANED FROM ITS GUARD, which is worse than no guard because
   the driver believes it is protected.

   So the third part of the guard is mandatory: **record the PID at launch and
   verify it is gone afterwards.** Launch with `&`, keep `$!`, and after the wait
   check the process by the `--port` you gave it -- `tasklist | grep -i NifSkope`
   plus the port on the command line is the only thing that tells your harness's
   window from bungo's. A driver that cannot end it must **print the PID and the
   port and stop**, because the next gate in the tree will refuse anyway.

   And a wedged process may be beyond the session's own permission to kill:
   `Stop-Process`, `taskkill` and `CloseMainWindow` were all refused by the
   classifier in LODIV7, four times, so the lane could not clean up after itself
   and closed on `PENDING.md` twice."""
s = s.replace(O1, N1)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('skill corrected')

p2 = 'MISTAKES.md'
s = open(p2, encoding='utf-8', newline='').read()
O2 = """- The rule, three parts. **(1)** A harness that launches the exe passes the SCENE
  FILE, always, and the `.lodl` is not optional. **(2)** Every such launch is
  wrapped in `timeout`: a stuck window is not a failed test, it is a failed
  TREE, because the one-instance rule means every later gate now cannot run.
  `timeout 600` is in `shot()` now, and it belongs in every driver that starts
  the exe. **(3)** After the launch, assert the artefact: `[ -s "$OUT/$1.png" ]`
  or say so. A harness whose output is a file must say in its log whether or not
  the file arrived; an absent line reads exactly like a passing one."""
assert s.count(O2) == 1
N2 = """- The rule, three parts. **(1)** A harness that launches the exe passes the SCENE
  FILE, always, and the `.lodl` is not optional. **(2)** Every such launch is
  wrapped in `timeout`: a stuck window is not a failed test, it is a failed
  TREE, because the one-instance rule means every later gate now cannot run.
  `timeout 600` is in `shot()` now, and it belongs in every driver that starts
  the exe. **(3)** After the launch, assert the artefact: `[ -s "$OUT/$1.png" ]`
  or say so. A harness whose output is a file must say in its log whether or not
  the file arrived; an absent line reads exactly like a passing one.

**CORRECTION, 2026-09-18 18:2x, and it corrects part (2) of the rule above rather
than the diagnosis.** `timeout` did not save the tree either, and that was
measured the same afternoon this entry was written. A run started at 18:04:59
through the FIXED `shot()` -- scene file present, `timeout 600` present --
launched NifSkope PID 24828, wrote one 268-byte log at 18:05:04, and was still
resident at **18:22:43**, 17 minutes later, with **no `timeout.exe` process left**
and no picture written. GNU `timeout` signals its child; a native Windows GUI
process started from an MSYS/Git-Bash shell does not act on that signal, so
`timeout` gives up, exits, and leaves the exe running and orphaned from its own
guard. **A guard that exits without ending what it guards is worse than no guard,
because the driver believes it is protected.** Part (2) therefore reads: wrap it
in `timeout` AND record the PID at launch AND verify after the wait that it is
gone -- and if it is not, print the PID and the port and stop, because every
later gate will refuse anyway. The second wedge cost this lane its pictures for
the second time in one day."""
s = s.replace(O2, N2)
open(p2, 'w', encoding='utf-8', newline='').write(s)
print('MISTAKES corrected')
