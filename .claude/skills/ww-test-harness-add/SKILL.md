---
name: ww-test-harness-add
description: Add a WW_*_TEST harness to NifSkope Wild Wasteland Edition (E:\Projects\NifskopeWildWastelandEdition) -- the in-application gate that runs inside a real window, drives real widgets, writes release/ww_<name>_test.log ending PASS/FAIL/done, and is driven by a tests/spells/<name>.sh spell. Covers the arming and timing shape, the translation-unit rule, reading WIDGETS instead of private members, the floors that cannot fire and how to spot them, the fixture-path traps that make a gate measure the machine instead of the code, and what a harness can never test. Use whenever a lane's gate has to run inside the application rather than against a file, and before believing a harness someone else just wrote.
---

# NifSkope WW: add a WW_*_TEST harness

Repo `E:\Projects\NifskopeWildWastelandEdition`. Thirty-odd of these exist. Three
consecutive lanes (HKX2, HKX3, FILESTAB) each reconstructed the shape by reading
`src/hkxplaybacktest.cpp` and `tests/spells/loaded_nifs.sh` end to end before
writing a line, and lane BUILD9 then paid for four separate defects in gates that
had never been executed. This is that procedure.

Sits beside, not on top of, `nifskope-ww-build-verify` (the build chain and the
exe under bungo's window) and `nifskope-ww-panel-style` (what a panel gate should
be asserting in the first place).

## 1. The shape, in the order it executes

**Its own translation unit**, `src/<name>test.cpp`, whenever `nifskope_ui.cpp` is
contended -- which is most of the time, it is 31,000 lines and two or three
lanes are usually inside it. The whole footprint there is one line:

```cpp
{
    extern void wwMyHarness( NifSkope * );
    wwMyHarness( skope );
}
```

placed with the other harness calls (`grep -n "wwHkxAnimHarness\|wwFilesTabHarness"`).
Add the `.cpp` to `NifSkope.pro`'s `SOURCES` and **re-run qmake**, or its object
never enters the build.

```cpp
void wwMyHarness( NifSkope * skope )
{
    if ( !skope || !qEnvironmentVariableIsSet( "WW_MY_TEST" ) )
        return;                       // armed by environment, never by a flag
    auto * st = new WwMyState;        // heap: the lambdas outlive this call
    st->shot = qEnvironmentVariable( "WW_MY_SHOT" );

    QObject::connect( skope, &NifSkope::completeLoading, skope,
                      [skope, st]( bool ok, QString & ) {
        // 1.5 s, and the reason is not superstition: the scene is BUILT on the
        // load signal, but the first paint -- and so the first transform walk,
        // the first layout pass and every geometry a gate reads -- is not.
        QTimer::singleShot( 1500, skope, [skope, st, ok]() {
            // The QFile is a STACK object and the QTextStream is destroyed
            // before it. A stream flushing into a deleted device writes its
            // last line into freed memory, and the last line is PASS/FAIL.
            QFile logf( QApplication::applicationDirPath() + "/ww_my_test.log" );
            if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) ) return;
            QTextStream log( &logf );
            st->out = &log;

            ... checks ...

            log << st->checks << " checks, " << st->fails << " failures\n";
            log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\n";
            log << "done\n";
            log.flush();
            logf.close();

            // Leave nothing for the close dialog to ask about, or the app
            // never quits and the spell times out at 90 s with no verdict.
            if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
                n->undoStack->setClean();
            skope->setWindowModified( false );
            QTimer::singleShot( 100, qApp, &QApplication::quit );
        } );
    } );
}
```

`check()` / `say()` / `skip()` are three four-line helpers in an anonymous
namespace; copy them from `src/skeloverlaytest.cpp`. `check` increments
`checks`, increments `fails` when false, and writes `  ok   ` / `  FAIL `.

## 2. The spell that drives it

`tests/spells/<name>.sh`, and its head comment is where the gate's REASONS live
-- bungo's words verbatim, what each number means, what each floor refutes.

```bash
. "$(dirname "$0")/_harness.sh"      # WW_WINDOW_AT=1960,40 and winpath()
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-42311}"                # ANY UNUSED PORT BELOW 49152
LOG="$ROOT/release/ww_my_test.log"
rm -f "$LOG"
WW_MY_TEST=1 WW_MY_SHOT="$(winpath "$SHOT")" \
    "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 90); do
    [ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
    kill -0 "$pid" 2>/dev/null || break
    sleep 1
done
kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
grep -q '^PASS$' "$LOG" || exit 1
```

Rules the loop encodes: poll for `^done$` (not for the process exiting -- a
crashed app and a finished one look the same to `wait`); print every `SKIP`
line after the log; `--port` on a bound port makes the app exit SILENTLY, which
reads exactly like a harness that wrote nothing.

## 3. Read the WIDGETS, not the private members

`friend class NifSkope` and a harness written inside `nifskope_ui.cpp` can reach
anything. A separate translation unit cannot, and that is a feature: **a gate
that reads private state measures what the code MEANT.**

* Give every control the lane adds an object name, and read it back with
  `findChild<T *>( QStringLiteral( "TimelineSpeed" ) )`. One line per control,
  and it measures what the user is looking at.
* Copying a line out of a class's own file does not lift its access. Both of
  these were caught by `-fsyntax-only`, in two different lanes:
  `NifSkope::ogl` is private (use `getGLView()`), and `setLeftColumnMode()`
  **and its `LeftColumnMode` enum** are private -- switch the left column by
  clicking its `QTabBar` `LeftColumnModeSelector` instead, which is what a user
  does anyway.
* Check every member you touch is public BEFORE the build, not after.

## 4. A dock defers its work while it is hidden

`TimelineWidget::refresh()` returns early when `!isVisible()`, and it is not the
only one. A harness that never shows the dock reads an empty list and proves
nothing. Show the dock, `qApp->processEvents()`, refresh, and only THEN read.
The same applies to a stacked page: the browser page is only laid out while it
is the current page, so its geometry is meaningless until the tab is selected.

## 5. Every check gets a floor, and the floor has to be able to fire

CONSTITUTION rule 4 requires the floor. Lane BUILD9 found two floors that were
green-or-red for reasons unrelated to what they measure:

* **A floor that picks its victim blindly.** "Blank one tooltip and watch the
  count rise by one" blanked `tools.first()` -- which was already untipped, so
  the count could not rise and the floor went red while the rule it guards was
  fine. **Choose the victim by the property being tested** (the first button
  that currently HAS a tooltip), never by position.
* **A floor that needs keyboard focus.** `QWidget::hasFocus()` is false in a
  window that is not active, however many times `setFocus()` is called -- and a
  WW harness window is never activated, by design (`WW_WINDOW_AT`, no
  `raise()`). Any rule of the form "this only happens while the field has
  focus" therefore cannot be exercised. Print `hasFocus`, `QApplication::focusWidget()`
  and `window()->isActiveWindow()` beside such a check so its red says which of
  the two it is.

## 6. The fixture traps that make a gate measure the MACHINE

* **`winpath()` only rewrites drive-style `/x/...` paths.** `mktemp -d` returns
  `/tmp/tmp.XXXX`, which passes through unchanged and reaches the Windows binary
  as a literal string it cannot open -- and a resource root that fails to open
  is usually COUNTED AND IGNORED, not reported. Lane FILESTAB's tree census read
  `.nif 0 | .bto 0 | .btr 0` for exactly this reason. **Build fixture trees under
  the repo** (`scratchpad/<lane>/fixture_tree`) and assert the converted path
  matches `X:/`.
* **A harness FORCES the state it measures.** Resource roots, the left-column
  mode, the dock's visibility, the scene's time. A census over whatever is in
  bungo's Settings is a measurement of his machine. Force in memory
  (`Game::GameManager::update_folders`), never write his settings.
* **Absolute Windows paths for every fixture**, one `winpath()` each. A
  semicolon-joined LIST is not rescued by MSYS2's automatic conversion.
* **Compare two snapshots at the same scene TIME.** A gate that snapshots the
  bind pose at t=0, scrubs to t=mid and compares after an unload is also
  measuring every controller the NIF itself carries: lane FILESTAB's restore
  gate reported one node unrestored, and it was `PipboyBone`, which the fixture
  drives with its own `NiTransformController`.

## 7. A named SKIP, never a silent pass

A missing fixture prints `SKIP` with its reason and is counted separately, and
the spell prints every SKIP line after the log:

```
48 checks, 1 failures, 0 skips
--- skips (a SKIP is never a pass) ---
```

Otherwise a resume that forgot a path reads a green suite that tested two thirds
of what it claims.

## 8. What a harness can never test, and must say so

* **A `QFileDialog`.** Gate the loader the button calls, one line further in,
  and name the dialog line as untested.
* **A real Explorer drag.** `QApplication::sendEvent` does run the real
  application event filter, but a native drop arrives at the window container by
  a different route. That half is bungo's to try, and it is owed.
* **Legibility.** No count sees a mark the colour of its row. That is a picture
  (`SHOT=<png>` dock grab, or `nifskope-ww-render-shot` for geometry).
* **Anything behind an `#ifdef` that did not reach the compiler.** Prove the
  define landed: `grep -c "WW_MY_FLAG" Makefile.Release`. Do NOT try to prove it
  by grepping the exe for a new string -- `QStringLiteral` compiles to UTF-16,
  so an ASCII grep for `TimelineSeqBox` returns 0 on an exe that contains it
  twice. Search `s.encode('utf-16-le')` in Python if you must look.

## 9. Before you believe a harness you did not run

* `N checks, M failures` next to the exe timestamp, never `PASS` alone.
* Every FAIL gets a NAME printed beside its count -- which node, which widget,
  which string. A count cannot be acted on; lane BUILD9 had to rebuild twice to
  turn "1 of 139 nodes differ" and "2 without a tooltip" into leads.
* A harness that has never been executed is a draft. Its numbers are
  pre-registrations, not results, and its own defects are found by running it.

## 10. Files to read when this page is not enough

`src/hkxplaybacktest.cpp` (the reference shape), `src/hkxanimuitest.cpp`
(widget-side reading by object name), `src/skeloverlaytest.cpp` (render
comparison, masks, floors), `src/uialigntest.cpp` (geometry in one frame of
reference), `tests/spells/_harness.sh`, and `WW_CYCLETYPE_TEST` in
`src/nifskope_ui.cpp` (~:7721) for the in-file variant.

## 5a. A check's TEXT is an interface, not prose (2026-09-11, lane UI5)

`tests/spells/*.sh` read their gates back BY NAME, matching a literal substring
of the check's own sentence against the `ok` / `FAIL` lines. So the sentence is
part of the contract:

* Editing a check's wording is a TWO-FILE edit -- the harness and every spell
  that names it -- in one step, ending with a `grep` that proves the two strings
  are byte-identical.
* A renamed check does not read as red. It reads as
  `FAIL: gate '...' did not run`, which is the message for a harness that
  crashed before reaching it. Lane UI5 changed one word ("every title" ->
  "every painted title") and caught it only because it grepped both files in the
  same command.
* Corollary for the arithmetic: put the varying numbers in `%1` arguments and
  keep the FIXED part of the sentence long enough to be unique, so the spell's
  substring never has to include a number that moves.

## 5c. A framebuffer check needs a MEASURED tolerance, and the tolerance needs its own floor (2026-09-11, lane SKEL2)

`grabFramebuffer()` is not bit-stable between repaints. Two checks in
`src/skeloverlaytest.cpp` demanded EXACT equality on it and had therefore been
red on a CORRECT overlay for two builds: BUILD11 recorded them varying 0..37 px
across four runs and called it "flaky", and the next lane inherited a suite that
nobody could read.

The procedure, and it costs four minutes:

* **run the same gate five times ON THE RUNG**, before any code, and print all
  five. Lane SKEL2 measured `(c)` at 16 / 13 / 14 / 9 / 1 and `(e)` at
  12 / 22 / 19 / 15 / 21; with BUILD11's four runs that is nine runs and a worst
  value of 37 out of ~1.2 million pixels;
* **set the bar as a FIXED COUNT above the worst measurement**, not a
  percentage -- 64 px here. A percentage of the frame moves when the frame size
  moves, and the frame size moves with the build;
* **write the number into the check's own sentence** (`"... (%1 outside the
  mask, bar %2)"`), so a log read six weeks later says what was tolerated;
* **give the tolerance its own FLOOR, in the same run**: assert that the SAME
  bar still REFUSES a real difference -- the ON-vs-OFF comparison the feature
  itself produces, which is tens of thousands of pixels. Without it a bar that
  swallowed everything would pass silently, and the gate would be worse than the
  red one it replaced;
* put the five numbers in the harness's own comment block, not only in the lane
  report. The next lane reads the source.

The general rule: **a check that has been red for two builds on correct code is
a defect in the check.** Fix it in the lane that trips over it, with the
measurement, rather than passing it on as a known red.

## 5d. FORCE every persisted state the gate measures (2026-09-11, lane SKEL2)

Section 6 says a harness forces the state it measures. The case that is easy to
miss is a state the lane itself has just made PERSISTENT: lane SKEL2 added three
Overlays rows that save to `QSettings`, so without four explicit setter calls at
the top of the harness the whole suite would have measured whichever bone shape
bungo last ticked. The setters go in before the first grab, and they include the
ones whose default is what you want -- a default is not a guarantee once a
setting exists.

## 5b. A floor's denominator is what the gate can SEE (2026-09-11, lane UI5)

Beside BUILD9's two broken floors, a third shape: a floor whose denominator
counts more than the gate could ever measure.

`found == menubar->actions().size()` looks like "every title was found". It is
not: `actions()` returns every action the bar HOLDS, and a hidden one, a
separator, or one with an empty `actionGeometry()` paints no box and has no ink
to find -- so the floor goes red on a menu bar that is perfectly correct, and
the red is read as "a title is not centred".

Count the PAINTED things once, print the number, and assert against that. A
floor that can refuse a correct input is not a floor, it is a second defect.

## 5c. A CHECK-COUNT FLOOR IS MEASURED, NEVER PREDICTED (2026-09-11, lane LODUI1)

The count floor at the bottom of a spell (`[ "$COUNT" -ge 116 ]`) exists to
catch a suite that silently stopped running half its block. It is the one number
in the file that must not be reasoned about.

Lane LODUI1 shipped a new spell with `floor 128`, arrived at by counting the
`check(` calls in its own new block by eye. The gate ran: **125 checks, 0
failures, PASS** -- and then the spell failed on its own arithmetic. The block
adds 9 checks, not 12, because two of them live inside an `else` branch that a
healthy panel never takes and one is a loop.

* Write the floor as `0` (or leave the line out), run the gate once, read the
  count, and write THAT number back with the exe timestamp and the date beside
  it in the comment.
* When you raise an existing floor, the comment says what the previous number
  was and what the lane added: `74 -> 97 -> 116` with a clause for each step is
  what lets the next lane explain its own delta instead of guessing.
* The same rule covers a floor you raise because your lane ADDED checks: it is
  the measured green count, never "the old floor plus what I think I added".

## 5d. AFTER CHANGING A DEFAULT, SWEEP THE HARNESS FOR STALE PREMISES (2026-09-11, lane LODUI1)

`ww-retire-a-surface` section 6 covers a harness that drove a surface you
REMOVED. The commoner and quieter case is a harness whose checks still run, and
still compile, but whose ENGLISH states a premise your change made false.

LODUI1 gave the object pass a second head, ticked by default. The existing check
`"the chunk range is greyed while no chunk output is selected"` then measured a
panel that legitimately had an output selected, and went red -- and the check
beside it, `"ticking Object LOD chunks enables the chunk range"`, went GREEN for
the wrong reason, because the range was already live.

Before the build, not after the red:

```bash
grep -n "while no \|opens with \|by default\|is unticked\|nothing is selected" src/<the harness>.cpp
```

and for each hit ask whether your change makes the sentence false. Re-aim it by
FORCING the state the sentence describes (untick every head, then tick the one
the target offers) rather than by editing the sentence to match the new default
-- a check that asserts the default is not a check.

## 6a. A CHECK THAT GRABS PIXELS NAMES THE WIDGET, AND ASSERTS IT IS VISIBLE (2026-09-12, lane PANEL1)

`WW_LODGEN_TEST` had a check reading "an unticked box can be seen (24+ levels
against the ground)". It grabbed the pixels at `LodgenAtlasCheck`'s coordinates.
Under the FO4 Community Shaders target that box is HIDDEN -- and the same log
said so three lines above: `FO4CS: the object atlas hidden: yes`. A hidden
widget keeps its last geometry, so the grab returned whatever was painted at
those coordinates. With four rows above it that happened to be another row's
box: 32 levels, green, for months. Lane PANEL1 put fifty-seven rows above it,
the coordinates landed on empty ground, and the check read 0 and went red on a
change that touched nothing about check-box contrast.

* A pixel check asserts `isVisible()` on the widget FIRST. `isHidden()` is not
  that test: a widget inside a folded section is not hidden, and a widget whose
  whole window is hidden is not either.
* It NAMES what it measured in the log (`unticked box (LodgenCullCheck): ...`),
  so the next reader sees the subject rather than trusting a number.
* Where the point is "some widget of this class is legible", FIND one at run
  time -- walk `findChildren<T *>()` for the first visible one inside the
  container under test -- instead of hard-coding an object name that a target
  may hide.
* The failure shape to recognise: **a pixel check that goes red on a change
  that could not possibly affect it.** Look for a hidden or moved widget before
  you look at the palette.

## Added by lane HORIZON2 (2026-09-18)

**A floor between the code and a reference that CALLS IT is not a floor.**
Before pre-registering any agreement threshold, `grep -n "<producer class>::"
<the refuter source>` and name every function the two share. If the list is not
empty the number measures agreement, and the gate's own comment must say so --
it is still useful as a regression signal, it is simply not a check. The check
that CAN fail is built from the RAW INPUTS and shares no line of code with the
thing it scores; freeze it as DATA next to the script (this lane:
`tests/spells/lodgen_horizon_witness.json`, ten receivers computed from the BTD
heightmap and the placements as exact world boxes), give it a fixture-rot check
so a fixture that silently loses its content fails instead of passing, and
**show it failing on a real pre-fix artefact, not on a mutation** (`--expect-fail`
fed the sheet the previous exe baked). Lane HORIZON2, 2026-09-18.

## 11. Proving a repair that changes the WINDOW every gate is photographed in (2026-09-19, lane HARNESSWIN2)

A repair to the harness window -- the dock order, a persisted-geometry guard,
anything that changes how big the framebuffer comes out -- is not one gate's
business. It moves the picture under EVERY spell that takes a `WW_RENDER_SHOT`.
The work is almost entirely in deciding, before the build, what each of those
spells is allowed to do afterwards. The procedure that worked:

**Sort the spells by what they actually COMPARE, not by what they write.** Three
kinds, and only the first can be "re-based":

* **A stored baseline.** An image on disk that a later run is diffed against
  (`tests/baselines/<gate>/`). Find these by asking for the CONSUMER, not the
  files: `grep -rn "baseline\|golden\|expect" tests/ tools/ --include=*.sh`.
  A glob for `*.png` counts fixtures as baselines and misses every store that
  lives outside the directory you globbed.
* **A same-run pair.** The gate renders A and B in one run and compares them to
  each other, or reads the size back out of the PNG it just wrote. Its artefacts
  may all change size without a single number moving. **Pinned by its COUNTS.**
* **An independent source.** A census, a plugin walk, a known-answer table. The
  window cannot reach it at all.

**Take the census BEFORE the build**: every shot's path, width, height and sha1
into a file. Afterwards take it again and diff. Then, per line:

* size unchanged + sha1 unchanged -> the repair did not reach it. Expected for
  any path that already sized itself correctly, and for any grab site the edit
  deliberately did not touch -- which is how you prove the edit was scoped.
* **size unchanged + sha1 CHANGED -> STOP.** That is a rendering change wearing
  a window change's clothes, and re-basing it would bury the evidence.
* size changed -> read the new size against the gate's own settled log line and
  against the request, and do not assume which one it will match (below).

**The floor you removed is not the only floor.** Hiding the docks takes the
docks out of the layout minimum; the rest of the layout is still in it. On this
machine the window went from a 1822 px minimum to 857 px, so a gate asking 1024
now lands exactly and a gate asking 640 still floors -- and still says FLOORED,
which is the honest outcome, not a miss. Have the harness PRINT the measured
minimum ("measured layout minimum for this build: 857x480") and read that number
rather than predicting the new size from the request.

**Read a trace's LAST record, never a count of its lines.** The window recorder
writes on every resize and state change, so one correct run logs its whole
convergence and several of those lines legitimately carry the refusal word. A
`grep -c` over that file measures history. If the file already has a reader that
takes the last line, use it.

**A rung binary is an UNREPAIRED binary.** Every red control, and every "was it
me or was it already broken" bisect, runs a build in which the defect is live.
If the defect is that runs write the user's settings, then the bisect writes the
user's settings. The isolation has to come from the environment, never from the
exe: `EXE=<a rung>` is never written without `WW_SETTINGS_SCOPE=<something>`
beside it, and a spell that offers `EXE=` but no scope knob says in its head
comment that it is only safe on the current build.

**A gate that goes from 8 failures to 3 is a result, not a failure.** Run the
neighbour on the rung as well (in a scope) and put the two logs side by side:
rows that fail identically on both binaries, with identical values, are not
yours, and saying so with the pair of numbers is worth more than a green gate.
Bisect one exe further back before naming a lane.

## A gate's absent fixture is usually NAMED INSIDE the artefact you already have (2026-09-19, lane CELLVIEW2B)

`tests/spells/impostor_draw.sh` reads three fixtures out of the environment --
`IMPOSTOR_LODM` (the card set), `IMPOSTOR_LODM_MORE` (a second sheet count, for
row 9) and `IMPOSTOR_NIF` (the SOURCE MESH, handed to the exe as its file
argument). With them unset the spell still exits 0 and prints a smaller count:
it SKIPS the rows that need them and reports **13 steps** where the lane that
wrote it reported 24. A control that "came back 13/0" is not a control that came
back equal, and a resuming lane will report it as one unless it knows the floor.

The bake that produced the fixtures was three lanes ago and its log was gone, so
the obvious route -- re-bake the tree to get a mesh name -- costs twenty minutes
and produces DIFFERENT bytes from the ones the gate was calibrated on. It was
not needed. **The card file says what it was made from.** A `.lodm` is JSON with
a `source` field, and `scratchpad/impostorfix3_20260919/fixture/blast_n4/cards/000531b3_oct.lodm`
carries `"source":"TreeMapleblasted05.nif"`, which resolves under the data root
in one glob. 24 steps, 0 failures, the calibrated fixtures, two minutes.

The rule, before hunting for a bake log or re-baking anything:

* **Read the artefact's own metadata for the name of its input.** Ours record
  it on purpose -- `.lodm` `source`, the `<chunk>.BTO.manifest.txt` beside a
  chunk, the `.lodt` provenance block, the arrays sidecar's `layers` list.
  `python -c "import json;print(json.load(open(p))['source'])"` beats an hour of
  archaeology.
* **A lane's fixture folder is its own bake, and the lane's GREEN run names the
  count it was green at.** `scratchpad/<lane>/gate_green.txt` (or whatever the
  lane called its saved run) is the floor to restore, with a timestamp. Pin the
  fixture paths in the chain script with a comment saying which run they came
  from, so the next lane inherits the calibration instead of rediscovering it.
* **Say the floor next to the count, always.** "impostor_draw 24/0" and
  "impostor_draw 13/0 (11 rows skipped, fixtures unset)" are different reports,
  and only the second one is honest about an unarmed gate.
