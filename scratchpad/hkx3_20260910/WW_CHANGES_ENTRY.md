## 2026-09-10 -- a loaded .hkx animation is a row of the Animation Manager, and a dropped file loads (lane HKX3)

bungo's rulings, verbatim: **"Also, add support of these to the timeline
workspace"** / *"or 'animation' workspace"*, on top of *"in animation workspace,
add an option to load a hkx file with animation, then they get added to the
animations list, and if there's rigged geometry with nodes / bone names that
match, they play"* and *"So either win exporer pick or drag and drop"*.

**STATUS: BUILD PENDING. Nothing here has been compiled or run.** Lane BUILD8
was still building and owns `NifSkope.pro` and `src/nifskope_ui.cpp`, so the
hook-up (nine anchored edits, all matching x1) is written and NOT applied.
Resume: `scratchpad/hkx3_20260910/PENDING.md`. Every statement below is a
statement of MECHANISM, and the gates that would prove it are written and unrun.

**THE LIST.** The Animation Manager's sequence combo was built from
`NiControllerSequence` BLOCKS held as `QPersistentModelIndex`, so a clip read off
disk -- which has no block and no index -- could not be a row in it. Clips are a
SECOND SOURCE appended after the blocks, with a different glyph (the posed
figure, against the sequence rows' `sequence`), the clip's name, and its frame
count and rate painted in the row: `Running_To_Slide_And_Back_To_Running — 93
frames @ 60 fps`. The raw name is the row's user data, and the dock now matches
on that rather than on the painted text.

**THE TRANSPORT IS THE ONE THAT WAS THERE.** A loaded clip is a name in
`Scene::animGroups` with its range in `Scene::animTags` (lane HKX2), which is
what `Scene::timeMin/timeMax` answer out of -- so scrub, play, pause, loop,
reverse, speed and cycle drive it with no new transport code. What this entry
adds beside it: **Load animation (.hkx)** (file dialog), **Unload** (the button
drops the selected clip; its menu carries one row per loaded clip plus *Unload
all*), **Root motion**, **Speed**, and a frame readout.

**FRAME TICKS AT THE CLIP'S OWN RATE.** The dock's fps was a display preference
with three fixed choices. A clip knows its rate, so while a clip row is selected
the clip's rate wins: 60 for the Mixamo fixture, 30 for a vanilla Fallout 4
clip. The readout reads `frame 46 / 92 · 0.767 s`. That number is the gate: at
30 fps the same instant is frame 23.

**ONE LOADER, THREE DOORS.** The render toolbar's button, the dock's button and
a dropped file all call `WwHkxAnimHub::loadFiles` (`src/hkxanimui.{h,cpp}`),
which loads, activates, MEASURES whether the clip actually bound, and returns
the one sentence the summary line shows. Lane HKX2 had spelled the load, the
activation, the summary and the signal out inline in one call site; two more
copies of that is two more places for the three to drift, and drift here is
invisible -- a drop that loads but does not activate looks exactly like a clip
that refused.

**REFUSALS GET A ROW.** `Scene::animGroups` holds what LOADED; it cannot hold
what did not, and a dropped file that vanishes without a trace is the worst
answer a drop can give. Two registers beside the playback's clips: files that
never produced a clip (`skeleton.hkx` is the common one -- it reads perfectly
and is a skeleton, so the sentence says both halves: it carries no animation,
and its bones are kept for clips that need names), and clips that loaded and
then refused to BIND because no bone they name is a node in the open NIF. Both
appear in the list marked refused, in the skin's `danger` colour, with the
reason in the pinned summary line and in the row's tooltip. The second register
is written **only after `setActive` has been called and has come back with the
clip not active** -- `GLView::setSceneSequence` has no return value, so this is
a measurement rather than a prediction.

**THE DROP.** `.hkx` is deliberately not one of `NifSkope::fileExtensions()`,
so the application event filter's existing loop never collected it and a dropped
clip was ignored with no feedback at all. A second collection sits beside
`nifFiles` in that same filter, with the same `belongsHere` test, the same
`QTimer::singleShot( 0, ... )` (never open a dialog while Qt is unwinding the
platform drag) and the same loader; the sentence goes to the status bar as well
as to the dock. **No model open refuses in words** -- measured as an empty node
list, not an empty filename, because NifSkope always has a document and the
starter cube is a model.

**LANES.** One `rangeOnly` lane per bound bone, in track order. A clip's samples
are not model keys -- no `NiKey` row to select, no interpolator to edit, nothing
to write back -- and `rangeOnly` is the shape this widget already has for that
(it is how a B-spline lane is drawn). `iSelect` stays invalid, and both places
that emit `indexSelected` already test it.

**A COMPILE-TIME SWITCH, not a behaviour switch.** Everything above is behind
`WW_HKXANIM_UI`, defined by the same `.pro` edit that adds the new files. With
the hook-up unapplied the dock compiles, links and behaves exactly as before.
Proven both ways with the real `Makefile.Release` flags over five translation
units: `scratchpad/hkx3_20260910/syntax_with.log` and `syntax_nohook.log`, both
`ALL-RC=0`, zero new warnings.

**Gates, pre-registered and UNRUN.** `src/hkxanimuitest.cpp` +
`tests/spells/hkxanim_ui.sh`: (a) one new row saying 93 frames / 60 fps, floor =
a name never loaded has no row; (b) at frame 46 every bound node's transform
equals the decoder's within 1e-4 / 0.01 deg, floor = frame 0 goes red; (c) the
readout, the Speed row and the Loop row, with floors; (d) unload restores every
`Transform` byte for byte, floor = > 0 differ while posed; (e) a simulated
Explorer drop through the real event filter yields the same row, floor = a
non-animation file adds none; (f) `skeleton.hkx` lands marked refused with a
sentence, floor = the clip that plays is not; (g) the panel-style counts, each
with a floor. Plus a dock grab at mid-clip. Every gate reads the widgets by
object name, never the dock's private members.

**Files.** New: `src/hkxanimui.{h,cpp}`, `src/hkxanimuitest.cpp`,
`tests/spells/hkxanim_ui.sh`, `scratchpad/hkx3_20260910/`. Changed:
`src/ui/widgets/timeline.h` +56 (LF, CR 0), `src/ui/widgets/timeline.cpp` +533
/ -3 (**CRLF -- this file is fully CRLF at HEAD, which `.gitattributes`'s list
of five CRLF files does not mention**; CR and LF both moved by exactly the same
count). Unapplied: nine anchored edits to `NifSkope.pro` and
`src/nifskope_ui.cpp`.
