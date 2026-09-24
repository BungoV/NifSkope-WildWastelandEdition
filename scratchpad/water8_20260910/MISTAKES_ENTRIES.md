<!-- Lane WATER8-GATE, 2026-09-11. TEXT ONLY for MISTAKES.md at the repo root;
     the director appends it (CONSTITUTION 2 + 8). Same wording as section 5 of
     scratchpad/lane_water8_report.md. -->

## 2026-09-11 -- lane WATER8: a finished build left BUILDING up and DONE unwritten

**What was done.** Lane WATER8 ran its chain to `CHAIN-RC=0`
(`release/NifSkope.exe` 21:02:12, 20,830,208 B) and then died at its account
limit. It left `scratchpad/water8_20260910/BUILDING` in place, no `DONE`, no
report section after 0.3, no `WW_CHANGES_ENTRY.md`, no `HANDOFF_BLOCK.md` and
an empty `images/`.

**What was true instead.** A built exe nobody has gated is indistinguishable,
from outside, from a lane that died mid-compile -- and `BUILDING` is the flag
two other lanes (UI5, UI6) wait on. The build was finished at 21:02:12; the
marker said "in progress" for eight hours.

**How it was found.** The continuation lane listed the directory before
believing anything: `BUILDING` present, `DONE` absent, `images/` empty, and
`find src res tests NifSkope.pro -newer release/NifSkope.exe` printing nothing.

**The rule that prevents it.** CONSTITUTION 1b/1c already say a lane past half
its window writes its report and resume FIRST. Extended by this: the build
markers are part of that write, not part of the report. **The moment a chain
returns, the marker is settled -- `DONE` if it linked, `PENDING.md` if it did
not -- before the gates are run and before anything else is written.** A lane
that cannot afford to run its gates can still afford one line of text, and a
`DONE` beside a red gate is a valid, unblocking verdict.

## 2026-09-11 -- lane WATER8-GATE: two copies of the gate chain ran at once, and the logs were rubbish

**What was done.** The chain script was launched three times in ninety seconds.
The first launch redirected its stdout INTO the directory the script itself
creates (`.../logs/chain_stdout.log`), so the shell's redirect failed before
the script ran and it looked like nothing had started. The second launch was
believed dead for the same reason -- its own redirect and `logs/SUMMARY.txt`
were both checked too early and both were empty -- so a third was started. Two
`gates.sh` processes then ran concurrently, each launching NifSkope.

**What was true instead.** Both were alive. `SUMMARY.txt` interleaved two runs
(`### water_ui start 05:23:53` next to `### files_tab rc=1 05:23:55`) and the
harnesses collided on their fixed ports: `animws.sh` reported "the harness
wrote no log (did the app exit before it ran, or is port 42317 bound?)" and
`water_mark.sh` "no dock log -- did the app exit before the harness ran?". Two
NifSkope instances were up at the same time, which CONSTITUTION 6 forbids
outright.

**How it was found.** The out-of-order timestamps in the lane's own summary
file, then `Get-CimInstance Win32_Process` showing two `gates.sh` shells
(pids 50100 and 16408) and two `-no-gui` NifSkope processes.

**Cost and damage.** Nine harness logs thrown away and the chain re-run; about
four minutes. Nothing was built, deployed or written outside the lane's
scratchpad; the void run is kept as
`scratchpad/water8_20260910/logs_void_doublechain/` and
`images_void_doublechain/` rather than deleted, so the collision stays
readable. No process of bungo's was touched (both NifSkope instances carried
`-no-gui` command lines belonging to this lane's own spells).

**The rules that prevent it.** Three, in order of how much they would have
saved:

1. **Before launching a chain, prove no copy is already running**, by command
   line, not by an empty log: the `Get-CimInstance Win32_Process` filter from
   `nifskope-ww-build-verify` ("Whose NifSkope is that?") applied to `bash.exe`
   with the script's own name. An empty `SUMMARY.txt` means "has not written
   yet", never "did not start"; a process list is the only honest answer.
2. **Never redirect a launcher's output into a directory the launched script
   creates.** `mkdir -p` the log directory in the SAME shell, before the
   redirect is parsed -- the redirect is opened before the command runs, so
   `script.sh > newdir/log` fails no matter what the script would have done.
3. **The chain script refuses a second copy itself.** A lock (`mkdir
   $OUT/.lock` or a pid file) makes the double launch impossible instead of
   merely unlikely, and it costs three lines. `gates.sh` for this lane did not
   have one; the next chain script written in this tree should.
