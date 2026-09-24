# Lane HKX3 -- loaded .hkx clips in the Animation Manager, and the DROP (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, `main`, **nothing committed**.

bungo's rulings this lane serves, verbatim:

* *"in animation workspace, add an option to load a hkx file with animation,
  then they get added to the animations list, and if there's rigged geometry
  with nodes / bone names that match, they play"*
* *"So either win exporer pick or drag and drop"*
* *"Also, add support of these to the timeline workspace"* / *"or 'animation'
  workspace"*

Lane HKX2 built the playback and put a Load button in the render toolbar's
Animation panel. This lane is the rest of item 6 of HKX2's report: the dock's
own list, the unload, the root-motion and speed rows, the panel-style pass, the
refused entry, and the drop.

## 1. The shape of it, in one paragraph

A `.hkx` becomes a row of the Animation Manager's own combo, **appended after
the NIF's `NiControllerSequence` rows**, carrying its name, its frame count and
its rate, with a different glyph. Selecting it drives it: the whole transport
was already there, because a loaded clip is a name in `Scene::animGroups` with
its range in `Scene::animTags` (HKX2), which is what `Scene::timeMin/timeMax`
answer out of. The three ways to ask for a clip -- the render toolbar's button,
the dock's button, an Explorer drop -- now all call **one loader**,
`WwHkxAnimHub::loadFiles`, which loads, activates, MEASURES whether the clip
actually bound, and returns the one sentence the summary line shows.

## 2. Why a hub, and what it holds that the playback cannot

`Scene::animGroups` holds the names of clips that LOADED. It cannot hold the
ones that did not, and a dropped file that vanishes without a trace is the worst
answer a drop can give. `WwHkxAnimHub` (`src/hkxanimui.{h,cpp}`) keeps, per
`Scene`, two small registers beside the playback's own clips:

* **files that never produced a clip** -- unreadable, or read and carrying no
  animation. `skeleton.hkx` is the common one: it is a skeleton, and HKX2's
  loader returns no refusal and no clip for it. That is a refusal of the
  REQUEST, and the sentence says both halves -- it carries no animation, and its
  bones are kept for clips that need names (CONSTITUTION rule 10).
* **clips that loaded and then refused to BIND**, because no bone they name is a
  node in the open NIF.

The second register is written **only after `setActive` has been called and has
come back with the clip not active**. `GLView::setSceneSequence` has no return
value, so a clip whose bones name nothing here refuses inside
`Scene::setSequence` with no sign of it in the UI; the hub asks the playback
afterwards which clip is posing the scene. That is a measurement, not a
prediction made before anything was tried.

Both kinds appear in the list marked refused, in the skin's `danger` colour,
with the reason in the pinned summary line and in the row's tooltip.

## 3. The dock, control by control

`src/ui/widgets/timeline.{cpp,h}`, all of it behind `#ifdef WW_HKXANIM_UI`
(section 6).

| control | object name | what it does |
|---|---|---|
| the list | `TimelineSeqBox` | the existing sequence combo, now with clip rows appended after the blocks. Row text is `<name> — 93 frames @ 60 fps`; the raw name is the row's user data, and `setSequenceByName` matches on that rather than on the painted text |
| Load animation (.hkx) | `TimelineLoadAnim` | file dialog, then the shared loader. The render toolbar's button calls the same one |
| Unload | `TimelineUnloadAnim` | the button drops the selected clip; its menu carries **one row per loaded clip** plus **Unload all**, and says "No animation loaded" rather than being a greyed control with nothing beside it |
| Root motion | `TimelineRootMotion` | the switch HKX2 shipped in the render toolbar, mirrored -- both now go through the hub, so the two cannot disagree |
| Speed | `TimelineSpeed` | `GLView::setAnimSpeed`; a scrub field, chrome off, like the two snap fields beside it |
| frame readout | `TimelineFrameReadout` | `frame 46 / 92 · 0.767 s`, at the CLIP's own rate |
| the summary line | `TimelineAnimNote` | a band of its own between the toolbar and the splitter: outside the splitter, so it can never scroll away from the button that produced it. `textMuted` for a summary, `danger` for a refusal |

**Frame ticks at the clip's rate.** The dock's `fps` was a display preference
with three fixed choices (24/30/60). A loaded clip KNOWS its rate, so while a
clip row is selected the clip's rate wins: 60 for the Mixamo fixture, 30 for a
vanilla Fallout 4 clip. This is the gate that cannot be faked -- at 30 fps the
same instant is frame 23, not frame 46.

**Lanes.** One `rangeOnly` lane per BOUND bone, in track order. A clip's samples
are not model keys: there is no `NiKey` row to select, no interpolator to edit
and nothing in the file to write back to. `rangeOnly` is the shape this widget
already has for exactly that (it is how a B-spline lane is drawn), `iSelect`
stays invalid, and both places that emit `indexSelected` already test it. The
lane count line reads `78 bones, 93 frames, 17 track(s) with no bone here`.

## 4. The drop

`.hkx` is deliberately **not** one of `NifSkope::fileExtensions()` -- it is not
a document this program opens -- so the application event filter's existing loop
never collected it and a dropped clip was ignored with no feedback at all. The
hook-up adds a second collection beside `nifFiles` in that same filter, using
the same `belongsHere` test, the same `QTimer::singleShot( 0, ... )` (never open
a dialog or rebuild a scene while Qt is unwinding the platform drag), and the
same loader. The loader's sentence goes to the status bar as well as to the
dock's own line.

**No model open refuses in words.** Measured as an empty node list, not as an
empty filename: NifSkope always has a document, and the starter cube is a model.
The sentence names the file: *"Nothing is open to animate. Open a rigged NIF
first, then load Running_To_Slide_And_Back_To_Running.hkx onto it."*

## 5. Gates -- PRE-REGISTERED, WRITTEN, AND UNRUN (section 8)

`src/hkxanimuitest.cpp`, driven by `tests/spells/hkxanim_ui.sh`. Fixtures:
`fixtures/human_male_vanilla.nif` (the rig), `fixtures/Running_To_Slide_And_Back_To_Running.hkx`
(Mixamo, 93 frames, 60 fps, measured by `release/hkxanim_dump.exe`), and
`scratchpad/hkx1_20260910/clips/skeleton.hkx` (the non-animation `.hkx`).

Everything is read off the WIDGETS by object name, never off the dock's private
members: a gate that reads private state reads what the code MEANT.

| gate | what it asks | the floor beside it |
|---|---|---|
| (a) | loading puts ONE new row in the dock's list, and the row SAYS 93 frames and 60 fps | a name that was never loaded has no row |
| (b) | with the row selected and the scene at frame 46, every bound node's `localTrans()` equals `HkxPlayback::sampleTrack` for frame 46: translation <= 1e-4, rotation <= 0.01 deg (the 4·asin metric) | the same comparison against frame 0 must go RED |
| (c) | the readout says `frame 46 / 92` -- only true at 60 fps; the Speed row writes `GLView::animationSpeed`; the dock's Loop row flips the transport's action; the clip is registered as a looping cycle | at t=0 the readout says frame 0; the speed reads 1 before the row is touched |
| (d) | unloading restores every `Transform` in the scene byte for byte (memcmp, all 13 floats, every node) | while posed, > 0 nodes must differ |
| (e) | a simulated Explorer drop of the same file, through the REAL application event filter, is accepted and yields the same row with the same 93/60 | dropping a file that is not an animation adds no row |
| (f) | `skeleton.hkx` lands as a row marked refused, the row text says "refused", the reason is a sentence and reaches the summary line | the clip that plays is NOT marked refused |
| (g) | panel style: 0 unstamped number fields of >= 3, 0 group boxes, 0 unstyled selectors of >= 1, all 6 new controls present with a tooltip, the summary line and the list OUTSIDE the splitter, the wheel over the unfocused Speed field leaves it | ... and steps it once focused |
| picture | the dock grabbed with the clip loaded and the timeline at frame 46 | the grab is > 400x80 and non-null |

A `SKIP` is printed by name and counted separately; the spell prints every SKIP
line after the log, so a resume that forgot a fixture sees the word.

## 6. The hook-up, and compiling without it

`scratchpad/hkx3_20260910/hookup.py`, a refusing script (skill
`ww-anchored-hookup`). Lane BUILD8 owns `NifSkope.pro`, `src/nifskope.cpp`,
`src/nifskope_ui.cpp` and `src/gltfexport.*`; lane FILESTAB is preparing edits
to the browser dock. **Nine edits, every anchor matching exactly once**, CR 0 ->
0 on both files:

1-2. `NifSkope.pro`: `src/hkxanimui.h`; `src/hkxanimui.cpp` + `src/hkxanimuitest.cpp`.
3. `NifSkope.pro`: `DEFINES += WW_HKXANIM_UI`.
4. `src/nifskope_ui.cpp`: `#include "hkxanimui.h"`.
5. `src/nifskope_ui.cpp`: `wwHkxAnimUiHarness( skope );`.
6. `src/nifskope_ui.cpp`: `timeline->setGLView( ogl );`.
7. `src/nifskope_ui.cpp`: the render toolbar's Load lambda becomes a call to the shared loader.
8. `src/nifskope_ui.cpp`: the Root motion row goes through the hub.
9. `src/nifskope_ui.cpp`: the drop branch.

Edit 3 is the compile-time switch. Every clip-shaped line in
`src/ui/widgets/timeline.{cpp,h}` is behind `#ifdef WW_HKXANIM_UI`, so with the
script unapplied the dock compiles, links and behaves exactly as it did before
and the new files are simply not in the build. Both halves are proven:

```
scratchpad/hkx3_20260910/syntax.sh          (with    -DWW_HKXANIM_UI)  ALL-RC=0
scratchpad/hkx3_20260910/syntax_nohook.sh   (without -DWW_HKXANIM_UI)  ALL-RC=0
```

over `src/hkxanimui.cpp`, `src/hkxanimuitest.cpp`, `src/ui/widgets/timeline.cpp`,
`src/ui/widgets/timelineedit.cpp` and `src/ui/widgets/timelineviews.cpp`, with
the real `Makefile.Release` flags. Zero new warnings.

## 7. Mistakes (for the director to splice into MISTAKES.md)

MISTAKES.md was deliberately NOT edited: lane BUILD8 is alive, and the
constitution's rule is that a lane delivers the TEXT and the director splices it.

1. **A patch script was written with LF anchors against a CRLF file, without
   measuring the file's line endings first.** `src/ui/widgets/timeline.cpp` is
   **fully CRLF** at HEAD (2,743 CR = 2,743 LF); the script's anchors carried
   bare newlines and matched 0 times. Caught by the script's own
   `assert count(anchor) == 1` before anything was written, so nothing landed --
   but the rule that was skipped exists precisely for this: measure
   `b.count(b'\r')` FIRST, and put the line ending IN the anchor. The assertion
   is what turned a silent no-op into a stop; a replace() without one would have
   written nothing and reported success.
2. **`.gitattributes` and CONSTITUTION rule 8 both name FIVE CRLF files
   (`src/glview.cpp`, `src/nifskope.cpp`, `src/gl/controllers.cpp`,
   `src/spells/havok.cpp`, `WW_CHANGES.md`) and say "everything else is LF".
   That is not true of this tree**: `src/ui/widgets/timeline.cpp` is fully CRLF
   at HEAD and is on neither list, while `src/ui/widgets/timeline.h` beside it is
   LF-only. A lane that trusts the list and normalises produces a 3,000-line diff
   that changes no code. The measurement, not the document, is the authority --
   which is what the rule already says, and the list is what made it look
   unnecessary.

## 8. What was NOT measured

**Everything about behaviour.** No build, no exe launch, no harness, no picture.
The likely failures and what each would mean are listed in
scratchpad/hkx3_20260910/PENDING.md. These are UNVALIDATED claims of mechanism,
each with what would refute it:

* **That the clip rows appear at all.** Refuted by gate (a) failing, or by
  `grep -c TimelineSeqBox release/NifSkope.exe` returning 0 -- which would mean
  WW_HKXANIM_UI never reached the compiler and the whole feature compiled out
  while the suite stayed green.
* **That the drop reaches the loader.** The harness's simulated drop goes
  through `QApplication::sendEvent( skope, ... )`, which does run the real
  application event filter -- but a REAL Explorer drop arrives at the native
  window container, and the existing .nif route's own comment records that
  Windows delivers it where the naive filter never saw it. Refuted by gate (e)
  passing and a real drag from Explorer doing nothing. That second half is
  bungo's to try, and it is owed.
* **That `rangeOnly` lanes with an invalid `iSelect` draw and click safely.**
  Both `indexSelected` emit sites test `isValid()` and `TimelineLanesView`
  returns early on `rangeOnly` -- read, not run. Refuted by a crash or an empty
  lane strip when a clip row is selected.
* **That the clip's rate reaches the RULER and not only the readout.** The
  readout is gate (c); the ruler's tick spacing comes from
  `formatTime`/`tlNiceStep`, which this lane did not change, so a 60 fps clip's
  ruler may still be labelled at 30-fps-shaped steps. Not measured, not claimed.
* **That the hub's per-Scene registers cannot outlive their scene.** Scene is
  owned by GLView and lives as long as the view (`Scene::clear()` empties a
  scene, it does not destroy it), so the hash key is stable per window -- read
  from the ownership, not measured. A second document window gets its own Scene
  and so its own registers; closing one leaves a stale key that is never
  dereferenced (the hash is only read through a live GLView) but is also never
  reclaimed. Refuted by a crash on closing a window with clips loaded.
* **The QFileDialog line itself.** Neither Load button's dialog can run in a
  harness; the gate calls the loader the button calls, one line further in.

## 9. Files

**New** (all LF, CR 0): `src/hkxanimui.h`, `src/hkxanimui.cpp`,
`src/hkxanimuitest.cpp`, `tests/spells/hkxanim_ui.sh`, and
`scratchpad/hkx3_20260910/` (hookup.py, syntax.sh, syntax_nohook.sh,
syntax_with.log, syntax_nohook.log, PENDING.md, WW_CHANGES_ENTRY.md).

**Changed:** `src/ui/widgets/timeline.h` +56 / -0 (LF, CR 0 -> 0),
`src/ui/widgets/timeline.cpp` +533 / -3 (CRLF; CR 2,743 -> 3,273 and LF
2,743 -> 3,273 -- the same count, so no line changed its ending).

**Written and NOT applied:** nine anchored edits to NifSkope.pro and
src/nifskope_ui.cpp (scratchpad/hkx3_20260910/hookup.py).

Nothing committed. MISTAKES.md, WW_CHANGES.md and HANDOFF.md were not touched --
three lanes are alive in the tree and those three files are the director's to
splice.

## 10. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used in earnest.**

* `nifskope-ww-panel-style` -- it is why `wwMatchFieldStyle` went on the sequence
  combo (which had never had it: the dock's one selector sat beside two scrub
  fields looking like a different widget set), why Speed is a scrub field with
  chrome off rather than a plain spin box, why the summary line is a band
  outside the splitter, why the Unload menu says "No animation loaded" instead
  of being a greyed control with nothing beside it, and why gate (g) COUNTS each
  of those with a floor instead of asserting they exist.
* `ww-anchored-hookup` -- the whole of section 6, including the two things this
  lane would otherwise have got wrong: writing hookup.py with the Write tool
  (its anchors carry the .pro's escaped quotes, which a heredoc would have
  halved), and the compile-time switch so the new code compiles with AND without
  the hook-up, proven by two syntax logs rather than asserted.
* `ww-hkx-animation` -- the settled facts it saved re-deriving: a clip is an
  animations-list entry for free via animGroups / animTags / animCycle, so the
  transport needs no new code; the corrected 4*asin angle metric (HKX1's
  published angles are halves); Node::local and its neighbours being protected.
* `nifskope-ww-resume-pending` -- the PENDING format, qmake before make, the
  per-object dependency read-back by awk rather than `grep -A3`, the
  whole-working-set staleness sweep, the four documents, and rule 6 (measure the
  cause, do not land the fix).
* `nifskope-ww-build-verify` -- the syntax pass with the real Makefile.Release
  flags, and the heredoc-backslash trap it names (which this lane then walked
  into anyway, from the other direction: see section 7).

**Declined, with the reason.** `nifskope-ww-render-shot`: this lane's picture is
a DOCK GRAB (`QWidget::grab`), not a render through the hook. The render hook
photographs the 3D view and there is no geometry claim here; CONSTITUTION rule 5
names the in-app dock grab as the right instrument for a layout change, and that
is what the harness takes.

**The skill that should exist and does not -- the same one lane HKX2 recorded as
owed, and this lane paid for a second time: "add a WW_*_TEST harness".** The
whole shape -- `qEnvironmentVariableIsSet` to arm; the completeLoading +
`QTimer::singleShot( 1500 )` pattern, and WHY 1.5 s (the scene is built on the
load signal, the first transform walk is not); the check/fails counters;
release/ww_<name>_test.log with PASS / FAIL / done; clearing the undo stack and
`setWindowModified( false )` before quitting; the _harness.sh window rules; the
sub-49152 port; the absolute-Windows-path rule for every fixture; and the "put
it in its own translation unit when nifskope_ui.cpp is contended" trick -- was
reconstructed by reading src/hkxplaybacktest.cpp and
tests/spells/hkxanim_play.sh end to end. That is a full read of two files in
every lane that ships a gate, which is most of them.

Three things such a skill would have to say that were learned HERE:

1. **Read the WIDGETS, not the private members.** `friend class NifSkope` gets a
   harness into a dock's internals, and a gate that uses it measures what the
   code MEANT. Object names on the new controls (TimelineSeqBox,
   TimelineLoadAnim, ...) plus `findChild<T *>( name )` measures what the user is
   looking at, and costs one line per control.
2. **A dock defers its work while hidden.** `TimelineWidget::refresh` returns
   early when `!isVisible()`, so a harness that never shows the dock reads an
   empty list and proves nothing. Show the dock, processEvents, refresh, and
   only then read a list.
3. **A named SKIP, never a silent pass.** A missing fixture prints SKIP with its
   reason and is counted separately, and the spell prints every SKIP line after
   the log -- otherwise a resume that forgot a path reads a green suite that
   tested two thirds of what it claims.

It is still **not written here**, for HKX2's reason and one of its own: doing it
properly needs a survey of the thirty existing harnesses to separate the
convention from one lane's habit, and this lane ended BUILD PENDING with every
gate unrun. A harness convention proposed from two examples, neither of which
has been executed, is exactly the "typed from memory" failure rule 1a warns
about. Recorded as owed for the third time, with the files to read first:
src/nifskope_ui.cpp WW_CYCLETYPE_TEST at ~:7721, src/hkxplaybacktest.cpp,
tests/spells/_harness.sh, and now src/hkxanimuitest.cpp for the widget-side
shape.

**Amendment owed to `ww-hkx-animation`** (repo tree
.claude/skills/ww-hkx-animation/SKILL.md; the director mirrors it to the live
tree). Section 8, "Settled, do not re-derive", should gain:

* **A clip has no model index, so it cannot be a row of anything built from
  blocks.** The Animation Manager's list is QPersistentModelIndex-backed; a clip
  is a SECOND source appended after it, and the row's raw name lives in the
  item's user data because the row's TEXT carries the frame count and rate.
* **`GLView::setSceneSequence` returns nothing.** Whether a clip actually bound
  is knowable only by asking `HkxPlayback::activeName()` afterwards. Every "this
  clip refused" mark must be written from that reading, never predicted.
* **`Scene::animGroups` cannot hold a refusal**, so anything that wants a
  refused file to stay visible needs a register of its own beside the playback.
* **.hkx is not in `NifSkope::fileExtensions()`** and must not be: it is not a
  document. A dropped .hkx needs its own collection in the application event
  filter, beside the .nif one, using the same belongsHere test and the same
  `singleShot( 0 )` deferral.

## 11. Note for lane FILESTAB (its files were already on disk; none was touched)

bungo's Files-tab ruling includes *"opening an .hkx from the tree loads it as an
animation onto the loaded model (the same path as the button / the drop)"*.
That path is `WwHkxAnimHub::instance()->loadFiles( ogl, { path }, true )`, and
`WwHkxAnimHub::isAnimationFile( path )` is the one extension test -- the same
two calls the drop branch makes. Nothing else needs writing on that side, and
writing a fourth copy of the load-activate-summarise sequence is the thing the
hub exists to prevent.


## Build (BUILD9)

Built and gated 2026-09-10 by lane BUILD9, after FILESTAB and before
SKELOVERLAY.

`hookup.py --check`: nine anchors, each still matching exactly once after
FILESTAB's 71 edits to the same two files. `--apply`: `NifSkope.pro`
19,044 -> 19,350 and `src/nifskope_ui.cpp` 1,476,506 -> 1,478,429, CR 0 -> 0 on
both. `hkxanimui.h` named by four objects (`hkxanimui.o`, `hkxanimuitest.o`,
`nifskope_ui.o`, `timeline.o`), `GeneratedFiles/.moc/moc_hkxanimui.cpp` present,
`WW_HKXANIM_UI` in `Makefile.Release`.

### The gate table (final exe 15:52:46)

| gate | expected | measured | verdict |
|---|---|---|---|
| (a) | one new row, 93 frames, 60 fps, the ROW says both | one row, 93 @ 60, length 1.53333 s | pass |
| (b) | worst translation <= 1e-4, rotation <= 0.01 deg at frame 46 | 78 bound nodes, translation **0**, rotation **1.72665e-05 deg**; frame-0 floor red at 299.83 / 104.964 deg | pass |
| (c) | `frame 46 / 92`; speed 1 -> 2 -> 1; Loop flips; cycle = CycleLoop | all four, readout `frame 46 / 92 - 0.767 s` | pass |
| (d) | 0 nodes differ after unload, > 0 while posed | posed 78 of 139, after unload **0 of 139** | pass |
| (e) | drop accepted, same row, same 93/60; junk floor adds no row | enter 1 / drop 1, rows 2 -> 3; junk 0 / 0, rows 2 -> 2 | pass |
| (f) | `skeleton.hkx` is a row marked refused with a sentence | row 3, text "skeleton - refused", reason in words, reaches the summary line | pass |
| (g) | 0 unstamped number fields of >= 3; 0 group boxes; 0 unstyled selectors of >= 1; 6 of 6 with tooltips; summary line and list outside the splitter; the wheel guard both ways | 3 fields 0 unstamped, 0 boxes, 1 selector 0 unstyled, 6 of 6, both outside; wheel guard's UNFOCUSED half passes, the focused half **cannot fire** | 1 FAIL |
| picture | > 400x80 | `scratchpad/hkx3_20260910/dock_clip_midclip.png`, 1549x284 | pass |

**48 checks, 1 failure, 0 skips.** Regression gate `hkxanim_play.sh`: **27
checks, 0 failures**, 78 / 17 / 4 on `fixtures/human_male_vanilla.nif` -- HKX2's
playback underneath is untouched.

### The one red is an instrument that cannot fire, and it was measured

Gate (g)'s floor asks the wheel to step the Speed field once it has focus. The
harness now prints the focus state beside the result:
`after setFocus: hasFocus no, focusWidget <none>, window active no`. A WW
harness window is deliberately never activated (`WW_WINDOW_AT`, no `raise()`),
and `QWidget::hasFocus()` is false in an inactive window however many times
`setFocus()` is called -- so `wwGuardWheel`, which blocks the wheel exactly
while `!hasFocus()`, is behaving as specified and the floor has no way to reach
the other half. Not a defect in the guard, and not a pass either.

### Two figures in PENDING.md that were wrong

* `grep -c "lane HKX3" NifSkope.pro src/nifskope_ui.cpp` prints **1 and 6**, not
  1 and 7: six of the nine edits reach that file and each carries one marker.
* `grep -c "TimelineSeqBox" release/NifSkope.exe` can never be >= 1.
  `QStringLiteral` compiles to UTF-16; an ASCII grep finds 0 in an exe that
  contains the string twice. `b.count("TimelineSeqBox".encode("utf-16-le"))`
  finds both, and `grep -c WW_HKXANIM_UI Makefile.Release` is the check that
  actually proves the define reached the compiler.

### The build trap this hook-up walked into

Edit 3 adds `DEFINES += WW_HKXANIM_UI`. `make` compares mtimes, not flags, so
`timeline.o` (15:19:10, newer than its source) was kept -- compiled WITHOUT the
define, with `setGLView` compiled out -- while the fresh `nifskope_ui.o` called
it. The link failed on one undefined reference and DELETED
`release/NifSkope.exe` on the way out. Six objects had to be removed by hand
before the rebuild. In MISTAKES.md and in `nifskope-ww-build-verify`, both trees.

### Still owed

A REAL Explorer drag onto the window: gate (e) is a simulated drop through
`QApplication::sendEvent`, which does run the application event filter, but a
native drop arrives by another route. That half is bungo's to try.

### The clocks, in one table

| artefact | time |
|---|---|
| `release/NifSkope.exe` (final) | 2026-09-10 15:52:46 |
| `release/style.qss` | 15:52:46, `cmp` equal to `res/style.qss` |
| FILESTAB hook-up applied | 15:17 |
| FILESTAB first gated exe | 15:20:34, re-gated on 15:25:05 and 15:52:46 |
| HKX3 hook-up applied | 15:29 |
| HKX3 gated exe | 15:32:06, re-gated on 15:35:05 and 15:52:46 |
| SKELOVERLAY hook-up applied | 15:40 |
| SKELOVERLAY gated exe | 15:36:50, re-gated on 15:52:46 |
| alignment before-measurement | 15:43:14 exe |
| alignment after-measurement | 15:52:46 exe |

Exe-newer sweep over every path `git status --porcelain -- src res tools tests`
reports: 0 stale at each gate run.
