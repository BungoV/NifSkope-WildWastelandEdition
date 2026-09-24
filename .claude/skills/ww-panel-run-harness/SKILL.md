---
name: ww-panel-run-harness
description: Drive a NifSkope Wild Wasteland PANEL to completion from inside its own WW_*_TEST harness -- press its action button, pump the event loop until a timer-driven run really ends, read the files it wrote and the numbers it printed, and put the user's QSettings back afterwards. Use whenever a gate has to prove that a ROW WIRES SOMETHING rather than that a row exists: a generator that writes a file, a bake that prints a time, a long operation whose result line is the thing under test. Written from lane LODUI1 (2026-09-11), whose native row and four stage times could not be proved any other way.
---

# NifSkope WW: driving a panel run inside the harness

Repo `E:\Projects\NifskopeWildWastelandEdition`. `ww-test-harness-add` owns the
SHAPE of a `WW_*_TEST` harness (arming, the log, PASS/FAIL, the spell). This
owns the one thing it does not: making the panel actually RUN, inside that
harness, and believing the result.

Reference implementation: the `WW_LODGEN_RUN` leg of `WW_LODGEN_TEST` in
`src/nifskope_ui.cpp` and its spell `tests/spells/lodgen_panel_run.sh`.

## 1. Why a reading harness is not enough

A structural check reads widgets: the row exists, it is ticked, it is hidden
under the other target. Every one of those can be true while the row is wired to
nothing. Lane NATIVE1a and NATIVE1b both closed with *"the LOD panel has no
`.lodo`/`.lodi` row at all"*; the row that answers that has to be shown WRITING
THE FILES, and a status line whose whole job is to carry four numbers has to be
shown carrying them after a real run.

The test is: **does the gate distinguish a wired row from an unwired one?** If
the row's `connect` were deleted, would the check go red? If not, it is
structural and something else is owed.

## 2. Opt it in, and give it its own spell

A run writes files and takes seconds to minutes. The structural spell usually
says in its own header that it is structure only, and its check count is a
floor other lanes read.

* Gate the leg on its own variable (`WW_LODGEN_RUN=1`), so the structural spell
  is unchanged and its count stays comparable across lanes.
* Give the leg its own spell that sets both variables, with its own measured
  floor and its own greps for the SENTENCES the leg exists to print
  (`grep -qa "Commonwealth.lodo " "$LOG"`), so a pass cannot come from the count
  alone.

## 3. Pumping: the button, and what "still running" means

The panel's own loop is usually `QTimer::singleShot( 0, this, step )`, so
`processEvents()` is what advances it; a plain sleep advances nothing and a
`QEventLoop::exec()` never returns.

```cpp
auto waitForRun = [&]( int budgetMs ) {
    QElapsedTimer t; t.start();
    while ( cancelBtn && cancelBtn->isEnabled() && t.elapsed() < budgetMs ) {
        QApplication::processEvents( QEventLoop::AllEvents, 20 );
        QThread::msleep( 5 );
    }
    return t.elapsed();
};
```

* **The "still running" flag is a WIDGET, not a member.** `running` is private.
  The Cancel button is enabled for exactly as long as a run is live and nothing
  else enables it; the Generate button is NOT the answer, because it is also
  disabled whenever the summary refuses.
* **Log the elapsed time of every run.** A leg that "passed" in 30 ms did not
  run, and the budget expiring looks identical to a fast success unless the
  number is printed.
* **A budget, always**, and generous: the run may load a worldspace. A harness
  that hangs is a build slot nobody gets back.

## 4. PUT THE SETTINGS BACK

Pressing the action button usually calls the panel's `saveSettings()`, which
writes its whole `QSettings` group -- so a harness run rearranges the user's
panel. That is the opposite of "a GUI harness forces the state it measures and
never inherits QSettings".

```cpp
QMap<QString, QVariant> saved;
{ QSettings s; s.beginGroup( "LodGeneration" );
  for ( const QString & k : s.allKeys() ) saved.insert( k, s.value( k ) );
  s.endGroup(); }
// ... drive the run ...
{ QSettings s; s.beginGroup( "LodGeneration" );
  for ( const QString & k : s.allKeys() ) if ( !saved.contains( k ) ) s.remove( k );
  for ( auto it = saved.constBegin(); it != saved.constEnd(); ++it )
      s.setValue( it.key(), it.value() );
  s.endGroup(); s.sync(); }
```

Keys the harness INVENTED must be removed, not just overwritten -- restoring
only what was there leaves the new ones behind.

## 5. Two runs, not one: that is where the floor lives

One run proves a number was written. It cannot prove the number MEANS anything.
Arrange the pair so every field is seen at its zero in one run and above it in
the other:

* run 1: the work the field measures, with everything else off;
* run 2: a different piece of work, with the first off.

Lane LODUI1: run 1 built one object chunk (`landscape 0.0, meshes 3.8,
textures 0.2, impostors 0.0`), run 2 baked the shadow heightmap alone
(`landscape 1.1, meshes 0.0, textures 0.0, impostors 0.0`). Add
`check( line1 != line2 )` as well, or a result line that is simply the previous
run's, reprinted, passes both halves.

**Pick the cheapest input that still exercises the path**, and MEASURE it
headlessly through the CLI first, before the harness is written: a one-chunk
region bake with the native emitter on is 4.9 s and a 4096 shadow heightmap is
2.0 s, which is what made an in-application run leg reasonable at all. If the
headless measurement is minutes, the leg belongs in a shell gate instead.

## 6. Write the same numbers on the command line

A field that only the GUI prints can only be gated by a GUI harness, which is
the slowest and most fragile gate there is. State the formatter ONCE in the
shared code (`lodgenStageTimeLine()` in `src/lodgen.cpp`) and print it from both
the panel and the CLI driver: the cheap shell gate then proves the arithmetic
and the movement, and the GUI leg only has to prove the panel reaches it.

## 7. Cleaning up, and the traps

* **Delete the output folder before the run**, or "the file exists" is answered
  by a file some earlier run left behind.
* **Turn the viewport preview OFF.** A panel that splices every finished chunk
  into the workspace and reframes is a person's feature and a harness's noise.
  It may have no object name -- find it by the start of its label text.
* Everything goes under `%TEMP%` or the repo's scratchpad, **never an installed
  `Data` folder**, and never the whole worldspace.
* The leg goes BEFORE the `SHOT=` block if the picture should show the result,
  and after it if the picture should show the panel at rest.

## 8. A PROPERTY USED AS A DOORBELL CARRIES A VALUE THAT MOVES (2026-09-12, lane PANEL1)

The harness asks the panel to save its settings through a dynamic property --
`panel->setProperty( "wwSaveSettings", true )`, answered by an `event()`
override watching `QEvent::DynamicPropertyChange`. It rang once and was deaf
afterwards, and the gate built on it reported that 56 of 57 rows did not
round-trip through QSettings: a clean, plausible, entirely false result.

`QObject::setProperty` on a dynamic property returns EARLY, without posting
`QDynamicPropertyChangeEvent`, when the stored value equals the new one. The
second `true` is not a change.

* Carry a value that moves: `panel->setProperty( "wwSaveSettings", ++tick )`,
  with `tick` a counter the harness owns.
* The same applies to any state a harness drives through properties, and to
  `QWidget::setUpdatesEnabled`-style idempotent setters: if the response is an
  event, the input has to differ.
* And the general rule: **when a gate says nearly every item fails, suspect the
  instrument before the items.** 56 of 57 is the shape of a broken reader, not
  of 56 broken rows. Prove the instrument on ONE item you can verify by hand
  before you believe it about fifty.

## 9. A PER-ROW BAKE GATE CARRIES EACH ROW'S DEPENDENCY, ITS MODE, AND WHETHER IT IS A DIAL OR A THRESHOLD (2026-09-12, lane PANEL1)

The strongest thing a panel harness can say about a new row is that moving it
moves the OUTPUT bytes. The shape is: bake once with every new row at its
default, then, per row, move that row alone, bake again, and compare the tree
digest. A digest that does not move means the row reaches nothing.

That gate is worth building, and in one lane it was wrong three times running —
every time in the instrument, never in the rows. Build it with all four of these
from the start:

1. **A dependency, per row.** Erosion rounds cannot move a bake whose erosion
   strength is 0. Bake the parent ON as a LOCAL baseline first, then move the
   child: two bakes per dependent row, and the answer means something.
2. **The dependency's MODE, not just "on".** Turning a selector "on" by stepping
   it one place lands on its first item, which may be a rule that never reads
   the row you are testing. Carry the VALUE the dependency must take.
3. **Dial or threshold.** One spin-box step is the right question for a strength
   and the wrong one for a radius, a count or a cutoff: 64 → 65 texels asks
   whether two things sit exactly 65 texels apart. Carry a bump VALUE for the
   rows that need one, big enough that a negative answer means something.
4. **A named skip for what this run cannot reach at all.** No road on this chunk,
   no card library armed, the module off in this module set, the row hidden under
   this render target — say so in the log, in plain language, with the spell that
   DOES read that row, and never count it as a pass.
5. **A chain can be two deep.** `slope warp` reads the guide's slope reference
   and then MULTIPLIES the land-warp amplitude, whose default is 0: with the
   right rule selected the row still bakes identically, because anything times
   zero is zero. Carry a second dependency where one exists, and print the whole
   chain the gate built: `(with landWarp on, landGuide at 4)`.
6. **FORCE the defaults; never inherit QSettings.** "Every row at its default"
   is a claim about the code, and a panel that has been used on this machine
   opens on what the machine REMEMBERS. Write each row's own default into its
   widget before the baseline bake, count the ones that had to be put back and
   NAME them:
   `forced back: waterSubdiv (this machine had 4, the default is 3)`.
   Saving the operator's settings group and restoring it at the end is a
   different duty -- it protects HIS settings and does nothing for the
   measurement. Do both. The shape this catches: a panel-versus-command-line
   comparison that disagrees on exactly one file (lane PANEL1, 2026-09-12: the
   terrain mesh, 56,916 B against 46,518 B, one remembered number).

And the verdict the gate prints is not "N rows failed": it is "every row this
bake can reach moves its bytes, and every row it cannot is NAMED". A row that
appears in neither list is the bug.

**The refuter, which costs one command**: ask the same question on the command
line, where a bump can be as large as you like and a rule can be typed by name.
If the CLI moves the bytes where the panel gate did not, the gate is the thing
that is broken. Do that BEFORE writing "this row reaches nothing" anywhere.


## 9. Run the spell from the shell that set the variables (2026-09-16, lane LAYOUT1)

A driver that prepends `/c/msys64/usr/bin` to `PATH` and then calls
`bash tests/spells/<x>.sh` runs the harness under the **MSYS2** bash, and that
boundary DROPS every variable the calling shell set. Two things follow, both
measured on the 22:48:35 exe in one session:

* `SHOT=<abs png> bash tests/spells/lod_generation.sh` writes **no picture and no
  log line**. The spell passes `WW_LODGEN_SHOT="${SHOT:-}"`, the app guards the
  grab with `!shot.isEmpty()`, and an empty value skips it silently -- there is
  not even a `NOT saved` line to notice;
* `TMP`/`TEMP` go the same way, so `QDir::tempPath()` lands where the app cannot
  write. The self-test's archives-vs-unpacked leg then fails with all three
  passes writing **0 bytes** while still printing `placed 678 objects, 10
  material buckets`. Run from Git Bash: 128 checks, **0 failures**, 1,280,239
  bytes both ways. A red produced through that shell is not evidence about the
  code.

The fix is one `PATH` entry. The exe needs the Qt DLLs, so prepend
**`/c/msys64/ucrt64/bin` alone** and never `/c/msys64/usr/bin` -- only the
second changes which `bash` runs. Check it in one line before a picture run:

    export PATH="/c/msys64/ucrt64/bin:$PATH"
    which bash                       # must still be /usr/bin/bash
    FOO=bar bash -c 'echo ${FOO:-EMPTY}'   # must print bar

`lodgen_byte_gate.sh` dies at `USER: unbound variable` for the same reason
(`TMP="${TEMP:-/c/Users/$USER/...}"` under `set -u`), which is the cheapest
smoke test that you are on the wrong side of the boundary.
