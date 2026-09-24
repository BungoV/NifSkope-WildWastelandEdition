#!/usr/bin/env python3
"""Lane UI6 -- append section 2 to the lane report (the brief requires the
report to be written incrementally; this is the writer for one section)."""

TEXT = r'''
---

## 2. The changes

Two refusing scripts (skill `ww-anchored-hookup`) for the four files other lanes
have been writing into all session; direct edits, named here, for the files with
one owner. **No lane was alive in the tree** (`ls scratchpad/*/BUILDING` empty,
no NifSkope running) -- the scripts are the audit trail, not a lock.

### 2.1 `scratchpad/ui6_20260910/hookup.py` -- the application code

`--check` (the default; writes nothing):

```
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the joined-strip rationale replaces the separated one
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the strip is one joined box at every air
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- outer air only; MARGINFIRST replaces SEAM/BRAD
src/nifskope_ui.cpp        after    anchor x1  already x0  CR 0  -- the arrow's column, with the probe's map
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the arrow's column, in the row's own button sheet
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- stamp wwHasMenu on the row's buttons
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the Animation Manager dock is not constructed
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the transport no longer routes through the retired dock
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- no toggle action to re-dock: the dock is gone
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the clock listens to the Animation dock directly
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- ...and reports back to it
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the Animation dock already has both of these connections
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- auto-key goes to the Animation dock, which is already connected
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the tooltip names the surviving dock
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- WW_DOCKS_TEST counts the surviving animation dock
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- ...and opens it on demand
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- WW_ROTKEY_TEST refuses in words instead of measuring nothing
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- WW_ANIMPLAY_TEST refuses in words instead of measuring nothing
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the workspace manager list: the new dock takes the old one's seat
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- Workspaces > Animation opens the new dock
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- the render panel's button opens the surviving dock
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- "Open in Animation" reaches the surviving dock
src/nifskope.h             replace  anchor x1  already x0  CR 0  -- the retired dock's member goes with it (it had no initialiser)
src/nifskope.h             replace  anchor x1  already x0  CR 0  -- say why the member is permanently null

24 of 24 edits match once
```

Applied: `src/nifskope_ui.cpp` 1,507,501 -> 1,509,597 B; `src/nifskope.h`
49,810 -> 50,049 B. CR 0 -> 0 on both.

### 2.2 `scratchpad/ui6_20260910/hookup_gates.py` -- the shared gate files

```
src/wateruitest.cpp        after    anchor x1  CR 0  -- the arrow instrument, beside the strip's and the menu bar's
src/wateruitest.cpp        replace  anchor x1  CR 0  -- the fifth distance is 0, and it has its own predicate
src/wateruitest.cpp        replace  anchor x1  CR 0  -- S5 reads 0, not the air
src/wateruitest.cpp        after    anchor x1  CR 0  -- the gap's OWN floor, live, in the same run
src/wateruitest.cpp        replace  anchor x1  CR 0  -- R4: the sheet has one more selector, the arrow's column
src/wateruitest.cpp        replace  anchor x1  CR 0  -- R4: one horizontal rule, the arrow's, and only on menu buttons
src/wateruitest.cpp        after    anchor x1  CR 0  -- group A: the arrows' air, with both halves of its live floor
tests/spells/water_ui.sh   replace  anchor x1  CR 0  -- the spell reads L8's missing half back by name
tests/spells/water_ui.sh   replace  anchor x1  CR 0  -- R4's renamed check (its text is an interface, UI5's own mistake)
tests/spells/water_ui.sh   replace  anchor x1  CR 0  -- the spell reads S5's own floor and the whole of group A by name
tests/spells/water_ui.sh   replace  anchor x1  CR 0  -- the spell's own header says what S5 now means
tests/spells/water_ui.sh   replace  anchor x1  CR 0  -- the count floor rises with the new checks, with the arithmetic
tests/spells/water_ui.sh   replace  anchor x1  CR 0  -- ...and the assertion that reads it

13 edits
```

Applied: `src/wateruitest.cpp` 54,229 -> 67,953 B; `tests/spells/water_ui.sh`
12,744 -> 13,951 B. CR 0 -> 0 on both.

### 2.3 Direct edits (one owner each, no lane alive)

| file | what |
|---|---|
| `src/wateruitest_lod.cpp` | L8's missing half: the LOD strip's inter-segment gap, with both halves of its own live floor |
| `src/hkxplayback.h` / `.cpp` | `HkxMapping::summaryShort()`, `HkxPlayback::summaryShort()`, `lastShort` beside `lastSummary` |
| `src/hkxanimui.h` / `.cpp` | `WwHkxAnimHub::sentenceShort()`; `say()` takes a short form |
| `src/animworkspace.h` / `.cpp` | `say()` takes a detail; the label is a few words and the tooltip is the sentence; `noteDetail()` reads it back |
| `src/animworkspacetest.cpp` | gate (j)'s missing floor, the stepper-clip gate and its floor, and gate (k), the binding label |
| `src/hkxanimuitest.cpp` | re-aimed from the retired dock at the Animation dock |
| `src/ui/widgets/timeline.cpp` / `.h` | the interim clip strip deleted (14 + 4 guarded blocks; 512 + 52 lines) |
| `tests/spells/hkxanim_ui.sh` | its header says which dock it now measures |

### 2.4 Item 1 -- the strips rejoined

**One sheet, both strips.** `wwSegmentedQss` (`src/nifskope_ui.cpp:548`) is the
only place either strip's look is stated, and gate L5 compares the two strips'
stylesheets byte for byte, so one change moves both. The air is now MARGIN on
the outer edges only:

* the base rule keeps `border-left: 0` and `border-radius: 0` at every air (it
  dropped both the moment the air went above 0), so the segments share one seam
  and their inner corners are square;
* `:first` puts its own left border and its two left corner radii back, and
  takes `margin-left: air`;
* `:last` takes `margin-right: air - separator`;
* the base takes `margin-top` / `margin-bottom` and nothing horizontal.

`min-height` is unchanged arithmetic (`row - 8 - 2*air`), so a segment is still
27 px in the 35 px row. **`UI/SegmentedStripAir = 0` still emits the 18:25:20
sheet byte for byte** -- the new `MARGINFIRST` token carries its own leading
space precisely so that it does.

### 2.5 Item 5 -- the dropdown arrows

`wwBarRowButtonQss( contentHeight )` gains ONE rule and nothing else:

```
QToolButton[wwHasMenu="true"] { padding-right: 8px; }
```

and `wwAlignBarRow` stamps `wwHasMenu` on each button in the row after LOOKING
at it (`b->menu()`, or its default action's menu). **`popupMode` is not the
test**: a QToolButton's default mode is DelayedPopup, which every button in the
row carries whether it has a menu or not, so selecting on it would have padded
the whole row. The property is a measurement, and gate A2 pins that it reached
every menu button and no other (floor: 0 buttons without a menu carry it).

Nothing vertical moved: no height, no `min-height`, no top or bottom padding.
The menu buttons are 4 px wider.

### 2.6 Item 2 -- the old Animation Manager retired

Gone from the window: the `QDockWidget` named `TimelineDock`, the
`TimelineWidget` inside it, all eleven of its connections, its
`toggleViewAction` re-dock lambda, and its seat in both dock lists.

The Animation dock takes its place **at the same index**, which is the whole of
why the Workspaces menu still works: `managers[0]` was the old dock and the
"Animation" entry is workspace 1, and stored workspace indices live in
QSettings. Putting `dAnimWs` at index 0 and dropping it from the end leaves
every other index where it was -- and it gives the new dock the Workspaces entry
it never had. (It was appended at index 10, one past the end of the eleven
workspace NAMES, so nothing in that menu could reach it at all.)

The transport's clock now listens to `AnimWorkspace::playPauseRequested`
directly; it used to be forwarded through the old dock's identical signal.
`GLView::transformCommitted`, `aAnimPlay::toggled` and `sequenceStopped` were
each connected to BOTH docks and keep their Animation-dock half.

`src/ui/widgets/timeline.cpp` and `timeline.h`: the interim clip strip -- lane
HKX3's Load / Unload / Root motion buttons, the clip rows, the Speed field, the
frame readout and the binding paragraph -- is **deleted**: 14 `#ifdef
WW_HKXANIM_UI` blocks from the `.cpp` (512 lines) and 4 from the `.h` (52
lines), none of which had an `#else`. The deletion is safe in shape because
those blocks are additive and guarded, so what is left is the translation unit
as it read before lane HKX3 -- and it compiles (section 3).

`NifSkope::timeline` stays declared and stays **null**. `NifSkope::dTimeline` is
deleted outright, because it had no initialiser and a member nothing assigns
would have been an indeterminate pointer. Two old harnesses that drove the
widget (`WW_ANIMPLAY_TEST`, `WW_ROTKEY_TEST`) now write a RETIRED line naming
the lane and the date instead of measuring nothing.

`src/hkxanimuitest.cpp` is re-aimed at the Animation dock: the same letters, the
same loader, the same playback; the widget names moved (`TimelineSeqBox` ->
`AnimWsClipList`, a QListWidget where the old one was a QComboBox,
`TimelineFrameReadout` -> `AnimWsReadout`, `TimelineAnimNote` -> `AnimWsNote`,
`TimelineSpeed` -> `AnimWsSpeed`, `TimelineUnloadAnim` -> `AnimWsUnloadAnim`).
One check changed its SUBJECT and says so in its own text: the old dock's list
was a strip control that had to live outside the splitter; the new dock's list
is a PANE inside it by design, so the same "cannot scroll away from what it
answers for" question is asked of the action bar.

### 2.7 Item 3 -- scrub fields, and no clipped steppers

**The dock already complied.** All 11 numeric fields (Frequency, Start time,
Stop time, Reduce tolerance x2, Trim from, Trim to, Retime, Float value, Speed,
Frame) go through `wwMakeScrubField`, and gate (j) already counted the unstamped
ones -- but with no floor, so a count that has only ever read zero was proving
nothing. Added:

* **(j floor)** one deliberately un-stamped `QDoubleSpinBox` is parented to the
  dock, the same question is asked, the count must rise by exactly one, and the
  box is destroyed again.
* **(j)** no number field has a stepper clipped to a sliver: a scrub field sets
  `NoButtons` and has no Qt stepper to clip, and any field that still has one
  must have its up-arrow rect inside its own rect, >= 8 px wide and >= 4 px tall.
* **(j floor)** a bare 26x9 `QDoubleSpinBox` with Qt's arrows -- what bungo
  photographed -- must be called clipped by that same predicate.

### 2.8 Item 4 -- the binding line cut to a summary

`HkxMapping::summaryShort()` is new beside `HkxMapping::summary()`: "78 of 95
bones", or "0 of 95 bones, none play" when nothing binds. `HkxPlayback` keeps it
beside `lastSummary`, `WwHkxAnimHub` beside `lastSentence`, and the dock's
`say()` takes a third argument: the SHORT form goes in the label, the FULL
sentence in the label's tooltip.

**Nothing was shortened by deleting it.** Every long sentence is still built,
still exact and still reachable; only where it is SHOWN moved. The same
treatment went to the other two things that label says: a selected clip reads
"93 frames @ 60 fps, 93 keys" with the document's full summary in the tooltip,
and a NIF sequence reads "N controlled block(s)" with its own sentence there.
`AnimWorkspace::noteDetail()` reads the tooltip back so the gate measures both
halves.
'''

with open('scratchpad/lane_ui6_report.md', 'a', encoding='utf-8', newline='\n') as f:
    f.write(TEXT)
print('appended %d chars' % len(TEXT))
