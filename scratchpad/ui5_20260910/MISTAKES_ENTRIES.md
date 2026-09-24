<!-- Lane UI5, 2026-09-11. TEXT for MISTAKES.md at the repo root; the director
     splices it (CONSTITUTION 8). Each entry: what was done, what was true
     instead, how it was found, the rule that prevents it. -->

## 2026-09-11 -- lane UI5: a probe's OUTPUT file was OLDER than the probe that wrote it

**What was done.** The lane was handed `scratchpad/ui5_20260910/probe_out.txt`
by a dead UI5 instance and read it as the probe's result. It is 16 KB, it runs
from `case 0` through `case C8`, and it ends in the middle of a sweep with no
banner saying so.

**What was true instead.** The output is stamped **20:58** and its own source
`probe.cpp` **21:00**: the source is two minutes NEWER than the result. The dead
lane wrote the D, E and \* cases into `probe.cpp` after the run that produced
the file -- and those are the padding route, which is the one that actually
works, and the no-event-loop calibration the whole design turns on. Nine cases
of the sweep had never been executed, including every case that answers the
question.

**How it was found.** `ls -la` on the lane's own directory before reading either
file, and then `tail` on the output, which stopped at C8 while the source's last
case is G. Two mtimes in one table (CONSTITUTION 4) is the whole of it.

**Cost.** None -- the probe was rebuilt and rerun (15 s to compile, instant to
run) and the missing cases are what the change is built on. Had the file been
believed, the lane would have shipped the margin route (case C6, ink offset
-0.5) whose rect lies about where the item is drawn, and its gate would have
been green on both states.

**The rule that prevents it.** An output file is evidence only for the SOURCE
IT WAS PRODUCED FROM. Before quoting a measurement another lane left behind, put
the output's mtime and its generator's mtime in one table; if the generator is
newer, the output is a draft and the thing to do is re-run it, not read it.

## 2026-09-11 -- lane UI5: a harness check was renamed and the spell that greps it by name was not

**What was done.** Group M's first floor shipped as
`"(M floor) every title in the menu bar was found as real text ink"`, and
`tests/spells/water_ui.sh` was given that exact string in its by-name gate list.
The check was then improved -- it must count PAINTED titles, not every action --
and its text became `"(M floor) every painted title..."`. The spell still
carried the old string.

**What was true instead.** The spell reads its gates back BY NAME from a verdict
line, so a renamed check does not read as "red": it reads as
`FAIL: gate '...' did not run`, which is the message for a harness that crashed
before reaching it. One word in a sentence would have looked like a dead
harness.

**How it was found.** A `grep` of the new string across the harness and the
spell in the same command, run because the check's text had just been edited.
Never ran; caught before the build.

**The rule that prevents it.** The text of a check is an INTERFACE, not prose.
Editing it is a two-file edit: the harness and every spell that names it, in one
step, with a grep that proves the two strings are byte-identical afterwards.

## 2026-09-11 -- lane UI5: a floor that would have refused a correct menu bar

**What was done.** Group M's first floor was written as "every title in the menu
bar was found as real text ink", asserting
`found == menubar->actions().size()`.

**What was true instead.** `QMenuBar::actions()` returns every action the bar
holds, and a hidden action, a separator, or one whose `actionGeometry()` is
empty paints no box and has no ink to find. The floor would then have gone red
on a menu bar that was perfectly correct, and the red would have been read as
"a title is not centred".

**How it was found.** Re-reading the lane's own check before the build and
asking what its denominator actually counts -- the same question the brief's own
floor rule asks of every new check.

**The rule that prevents it.** A floor's denominator is what the gate can SEE,
never what the widget merely holds. Count the painted things once, print the
number, and assert against that; a floor that can refuse a correct input is not
a floor, it is a second defect.

## 2026-09-11 -- lane UI5: the gate called the mnemonic underline "the text", and it cost a link

**What was done.** Group M's verdict took the whole ink band of each menu title
-- the topmost bright row to the bottommost -- and called its midpoint the
text's centre. The standalone probe had predicted the shipped rule would put
that midpoint within +0.5 px of the row's centre line.

**What was true instead.** It read **+1.5** for File and View and **+2.0** for
Spells and Options, and the code was right. The ink band is not the letters: it
also holds the **mnemonic underline**, which Qt draws two rows below the
baseline under the F of File and the V of View, and the **descender of the p**
in Spells and Options, four rows below it. Both pull the midpoint down, by
different amounts per title -- which is also why the "the five agree with each
other" check read a spread of 0.5 on five titles sitting at identical heights.
The font's own numbers (probe case H: ascent 13, descent 3, height 16,
capHeight 8) put the capital letters at y 13..21 in a content box at y 9..24 --
**centre 17.0, the row's centre line exactly.**

**How it was found.** By running the gate, and by reading its own floor: the
same run's red half, with the shipped 20:45:47 arithmetic put back, printed
`File 6..16` and `Spells 6..17` -- the identical numbers a different instrument
in a different process (`measure_before.py`, on the shipped picture) had read.
A gate whose floor reproduces the old state exactly, while its verdict says the
new state is wrong, is accusing itself.

**Cost.** One link (~1 minute; one translation unit). No application code
changed.

**The rule that prevents it.** Decide WHICH BAND of ink is the thing being
judged before writing the predicate, and say so in the check's own words. For
text, that is the CAP BAND -- the topmost ink row down by the font's own
`capHeight()` -- not the ink's bounding box, which collects underlines,
descenders and any other decoration the style draws. And a probe's prediction of
a pixel number is a prediction about the probe: it renders without the
application's antialiasing and without its mnemonics.

## 2026-09-11 -- lane UI5: the lane's own build script destroyed the rollback rung

**What was done.** `scratchpad/ui5_20260910/build.sh` ran
`cp -p release/NifSkope.exe release/NifSkope.before_ui5.exe` before every link,
unconditionally, so that a rollback rung would exist. It ran three times.

**What was true instead.** The first run copied the real pre-UI5 exe
(WATER8's 21:02:12). The second run copied **this lane's own first build**, and
the third copied its second. After the third link, a file named
`NifSkope.before_ui5.exe` held a binary that already carried every line of UI5's
change, which is worse than no rung at all -- a rung that is not what its name
says is a trap for whoever reaches for it.

**How it was found.** Listing `release/*.exe` with timestamps after the last
link, because a rung is one of the things a handoff has to state, and its
timestamp read 05:53:59 instead of 2026-09-10 21:02:12.

**Cost and damage.** The pre-UI5 exe is gone from disk. The nearest earlier rung
is `release/NifSkope.before_ui4.exe` (18:25:20), which predates UI4 and WATER8
too. It matters little here -- the exact way back for this change is the setting
`UI/CompactTopBars = false`, gated live -- but the rung itself is not
recoverable without a revert and a rebuild. Both misleading copies were deleted
rather than left on disk under a name that was no longer true.

**The rule that prevents it.** A rollback rung is written ONCE, and the write is
guarded by "does it already exist". Any build script that may be run twice in a
session -- and a lane that fixes its own gate WILL run twice -- must never
overwrite a file whose whole value is that it predates the session.
